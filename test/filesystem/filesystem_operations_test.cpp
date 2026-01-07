/*
 * Copyright (C) Marcus Holland-Moritz - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Marcus Holland-Moritz <github@mhxnet.de>
 */

#include <gtest/gtest.h>

#include <vector>
#include <string>
#include <iostream>

#include <dwarfs/reader/filesystem_v2.h>

#include "../fixtures/filesystem_test_fixture.h"

using namespace dwarfs;

namespace dwarfs::test {

// Test class for comprehensive filesystem operations
class FilesystemOperationsTest : public FilesystemTestFixture {
  // No additional setup needed - uses base fixture
};

// ============================================================================
// Directory Operations Tests
// ============================================================================

TEST_F(FilesystemOperationsTest, empty_directory) {
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("emptydir", {2, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto dir = filesystem_->find("/emptydir");
  ASSERT_TRUE(dir);
  EXPECT_TRUE(dir->inode().is_directory());

  // Verify directory is empty - count entries INSIDE emptydir, not emptydir itself
  size_t entry_count = 0;
  filesystem_->walk([&](auto e) {
    auto path = e.unix_path();
    // Only count entries that start with "/emptydir/" (not "/emptydir" itself)
    if (path.size() > 9 && path.substr(0, 10) == "/emptydir/") {
      entry_count++;
    }
  });
  EXPECT_EQ(entry_count, 0);
}

TEST_F(FilesystemOperationsTest, large_directory) {
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("largedir", {2, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});

  // Create 150 files in directory
  for (int i = 0; i < 150; ++i) {
    std::string filename = "largedir/file_" + std::to_string(i) + ".txt";
    std::string content = "Content of file " + std::to_string(i);
    input_->add(filename, {static_cast<uint32_t>(3 + i), 0100644, 1, 1000, 100,
                           static_cast<uint32_t>(content.size()), 42, 0, 0, 0},
                content);
  }

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto dir = filesystem_->find("/largedir");
  ASSERT_TRUE(dir);
  EXPECT_TRUE(dir->inode().is_directory());

  // Verify we can find some of the files
  auto file0 = filesystem_->find("/largedir/file_0.txt");
  auto file50 = filesystem_->find("/largedir/file_50.txt");
  auto file149 = filesystem_->find("/largedir/file_149.txt");

  ASSERT_TRUE(file0);
  ASSERT_TRUE(file50);
  ASSERT_TRUE(file149);
}

TEST_F(FilesystemOperationsTest, deeply_nested_directories) {
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});

  // Create 10-level nested directory structure
  std::string path;
  for (int i = 0; i < 10; ++i) {
    path += (i == 0 ? "" : "/") + std::string("level") + std::to_string(i);
    input_->add(path, {static_cast<uint32_t>(2 + i), 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  }

  // Add file at deepest level
  std::string deepfile = path + "/deep.txt";
  input_->add(deepfile, {12, 0100644, 1, 1000, 100, 4, 42, 0, 0, 0}, "deep");

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto file = filesystem_->find("/" + deepfile);
  ASSERT_TRUE(file);
  EXPECT_FALSE(file->inode().is_directory());
}

TEST_F(FilesystemOperationsTest, readdir_iteration) {
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("testdir", {2, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("testdir/alpha.txt", {3, 0100644, 1, 1000, 100, 5, 42, 0, 0, 0}, "alpha");
  input_->add("testdir/beta.txt", {4, 0100644, 1, 1000, 100, 4, 42, 0, 0, 0}, "beta");
  input_->add("testdir/gamma.txt", {5, 0100644, 1, 1000, 100, 5, 42, 0, 0, 0}, "gamma");

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  // Verify we can find all three files
  auto alpha = filesystem_->find("/testdir/alpha.txt");
  auto beta = filesystem_->find("/testdir/beta.txt");
  auto gamma = filesystem_->find("/testdir/gamma.txt");

  ASSERT_TRUE(alpha);
  ASSERT_TRUE(beta);
  ASSERT_TRUE(gamma);

  // Verify they're files, not directories
  EXPECT_FALSE(alpha->inode().is_directory());
  EXPECT_FALSE(beta->inode().is_directory());
  EXPECT_FALSE(gamma->inode().is_directory());
}

// ============================================================================
// File Operations Tests
// ============================================================================

TEST_F(FilesystemOperationsTest, zero_byte_file) {
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("empty.txt", {2, 0100644, 1, 1000, 100, 0, 42, 0, 0, 0}, "");

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto file = filesystem_->find("/empty.txt");
  ASSERT_TRUE(file);
  EXPECT_FALSE(file->inode().is_directory());
  auto st = filesystem_->getattr(file->inode());
  EXPECT_EQ(st.size(), 0);
}

TEST_F(FilesystemOperationsTest, small_file) {
  std::string content = "Small file content under 1KB";
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("small.txt", {2, 0100644, 1, 1000, 100,
                            static_cast<uint32_t>(content.size()), 42, 0, 0, 0},
              content);

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto file = filesystem_->find("/small.txt");
  ASSERT_TRUE(file);
  EXPECT_FALSE(file->inode().is_directory());
  auto st = filesystem_->getattr(file->inode());
  EXPECT_EQ(st.size(), content.size());
}

TEST_F(FilesystemOperationsTest, large_file) {
  // Create ~1MB file
  std::string content(1024 * 1024, 'X');
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("large.bin", {2, 0100644, 1, 1000, 100,
                            static_cast<uint32_t>(content.size()), 42, 0, 0, 0},
              content);

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto file = filesystem_->find("/large.bin");
  ASSERT_TRUE(file);
  EXPECT_FALSE(file->inode().is_directory());
  auto st = filesystem_->getattr(file->inode());
  EXPECT_EQ(st.size(), content.size());
}

TEST_F(FilesystemOperationsTest, fragmented_file) {
  // Create file with known content that might fragment
  std::string content;
  for (int i = 0; i < 100; ++i) {
    content += "Block " + std::to_string(i) + " content with padding\n";
  }

  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("fragmented.txt", {2, 0100644, 1, 1000, 100,
                                 static_cast<uint32_t>(content.size()), 42, 0, 0, 0},
              content);

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto file = filesystem_->find("/fragmented.txt");
  ASSERT_TRUE(file);
  auto st = filesystem_->getattr(file->inode());
  EXPECT_EQ(st.size(), content.size());
}

// ============================================================================
// Symlink Operations Tests
// ============================================================================

TEST_F(FilesystemOperationsTest, valid_symlink) {
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("target.txt", {2, 0100644, 1, 1000, 100, 6, 42, 0, 0, 0}, "target");
  // Symlinks: add with mode 0120777, content is target path
  input_->add("link.txt", {3, 0120777, 1, 1000, 100, 10, 42, 0, 0, 0}, "target.txt");

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto link = filesystem_->find("/link.txt");
  ASSERT_TRUE(link);
  EXPECT_TRUE(link->inode().is_symlink());

  auto target = filesystem_->find("/target.txt");
  ASSERT_TRUE(target);
  EXPECT_FALSE(target->inode().is_symlink());
}

TEST_F(FilesystemOperationsTest, broken_symlink) {
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  // Symlinks: add with mode 0120777, content is target path
  input_->add("broken.txt", {2, 0120777, 1, 1000, 100, 15, 42, 0, 0, 0}, "nonexistent.txt");

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto link = filesystem_->find("/broken.txt");
  ASSERT_TRUE(link);
  EXPECT_TRUE(link->inode().is_symlink());

  // Target should not exist
  auto target = filesystem_->find("/nonexistent.txt");
  EXPECT_FALSE(target);
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(FilesystemOperationsTest, long_filename) {
  // Create 255-character filename (max for most filesystems)
  std::string longname(255, 'a');
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add(longname, {2, 0100644, 1, 1000, 100, 4, 42, 0, 0, 0}, "long");

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto file = filesystem_->find("/" + longname);
  ASSERT_TRUE(file);
  EXPECT_FALSE(file->inode().is_directory());
}

TEST_F(FilesystemOperationsTest, utf8_special_chars) {
  // Test UTF-8 filenames with emoji and special characters
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("emoji_😀_test.txt", {2, 0100644, 1, 1000, 100, 5, 42, 0, 0, 0}, "emoji");
  input_->add("中文_文件.txt", {3, 0100644, 1, 1000, 100, 7, 42, 0, 0, 0}, "chinese");
  input_->add("Ñoño.txt", {4, 0100644, 1, 1000, 100, 4, 42, 0, 0, 0}, "tilde");

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto emoji = filesystem_->find("/emoji_😀_test.txt");
  auto chinese = filesystem_->find("/中文_文件.txt");
  auto tilde = filesystem_->find("/Ñoño.txt");

  ASSERT_TRUE(emoji);
  ASSERT_TRUE(chinese);
  ASSERT_TRUE(tilde);
}

TEST_F(FilesystemOperationsTest, path_limits) {
  // Test deep path (4096 chars typical limit)
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});

  // Create very deep nesting
  std::string path;
  for (int i = 0; i < 100; ++i) {
    path += (i == 0 ? "" : "/") + std::string("d") + std::to_string(i);
    input_->add(path, {static_cast<uint32_t>(2 + i), 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  }

  // Add file at max depth
  std::string deepfile = path + "/file.txt";
  input_->add(deepfile, {102, 0100644, 1, 1000, 100, 4, 42, 0, 0, 0}, "deep");

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  auto file = filesystem_->find("/" + deepfile);
  ASSERT_TRUE(file);
  EXPECT_FALSE(file->inode().is_directory());
}

}  // namespace dwarfs::test