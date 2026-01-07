/*
 * Copyright (C) Marcus Holland-Moritz - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Marcus Holland-Moritz <github@mhxnet.de>
 */

#include <gtest/gtest.h>

#include <fmt/format.h>

#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/filesystem_options.h>

#include "../fixtures/filesystem_test_fixture.h"

using namespace dwarfs;

namespace dwarfs::test {

// Test class for UID/GID handling - inherits from FilesystemTestFixture
class FilesystemUidGidTest : public FilesystemTestFixture {
  // No additional setup needed - uses base fixture
};

TEST_F(FilesystemUidGidTest, handles_32_bit_uid_gid) {
  // Setup test data using fixture factory
  input_->add("", {1, 040755, 1, 0, 0, 10, 42, 0, 0, 0});
  input_->add("foo16.txt", {2, 0100755, 1, 60000, 65535, 5, 42, 0, 0, 0},
              "hello");
  input_->add("foo32.txt", {3, 0100755, 1, 65536, 4294967295, 5, 42, 0, 0, 0},
              "world");

  // Build filesystem and create reader using fixture helper
  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  // Verify using fixture helpers
  verify_uid_gid("/foo16.txt", 60000, 65535);
  verify_uid_gid("/foo32.txt", 65536, 4294967295);
}

TEST_F(FilesystemUidGidTest, handles_large_uid_gid_count) {
  // Setup test data
  input_->add("", {1, 040755, 1, 0, 0, 10, 42, 0, 0, 0});

  for (uint32_t i = 0; i < 100000; ++i) {
    input_->add(fmt::format("foo{:05d}.txt", i),
                {2 + i, 0100644, 1, 50000 + i, 250000 + i, 10, 42, 0, 0, 0},
                fmt::format("hello{:05d}", i));
  }

  // Build filesystem and create reader
  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  // Verify specific entries
  verify_uid_gid("/foo00000.txt", 50000, 250000);
  verify_uid_gid("/foo50000.txt", 100000, 300000);
  verify_uid_gid("/foo99999.txt", 149999, 349999);
}

TEST_F(FilesystemUidGidTest, supports_uid_gid_override) {
  // Setup test data
  input_->add("", {1, 040755, 1, 0, 0, 10, 42, 0, 0, 0});
  input_->add("foo16.txt", {2, 0100755, 1, 60000, 65535, 5, 42, 0, 0, 0},
              "hello");
  input_->add("foo32.txt", {3, 0100755, 1, 65536, 4294967295, 5, 42, 0, 0, 0},
              "world");

  auto fsimage = build_filesystem();
  file_view_ = create_file_view(fsimage);

  // Test with override
  {
    reader::filesystem_options opts{.metadata = {
                                        .fs_uid = 99999,
                                        .fs_gid = 80000,
                                    }};

    reader::filesystem_v2 fs(logger_, *input_, file_view_, opts);

    auto dev16 = fs.find("/foo16.txt");
    auto dev32 = fs.find("/foo32.txt");

    ASSERT_TRUE(dev16);
    ASSERT_TRUE(dev32);

    auto st16 = fs.getattr(dev16->inode());
    auto st32 = fs.getattr(dev32->inode());

    EXPECT_EQ(99999, st16.uid());
    EXPECT_EQ(80000, st16.gid());
    EXPECT_EQ(99999, st32.uid());
    EXPECT_EQ(80000, st32.gid());
  }

  // Test without override (normal values)
  {
    reader::filesystem_v2 fs(logger_, *input_, file_view_);

    auto dev16 = fs.find("/foo16.txt");
    auto dev32 = fs.find("/foo32.txt");

    ASSERT_TRUE(dev16);
    ASSERT_TRUE(dev32);

    auto st16 = fs.getattr(dev16->inode());
    auto st32 = fs.getattr(dev32->inode());

    EXPECT_EQ(60000, st16.uid());
    EXPECT_EQ(65535, st16.gid());
    EXPECT_EQ(65536, st32.uid());
    EXPECT_EQ(4294967295, st32.gid());
  }
}

}  // namespace dwarfs::test