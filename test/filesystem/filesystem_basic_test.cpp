/*
 * Copyright (C) Marcus Holland-Moritz - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Marcus Holland-Moritz <github@mhxnet.de>
 */

#include <gtest/gtest.h>

#include <vector>
#include <iostream>

#include <dwarfs/reader/filesystem_v2.h>

#include "../fixtures/filesystem_test_fixture.h"

using namespace dwarfs;

namespace dwarfs::test {

// Test class for basic filesystem operations
class FilesystemBasicTest : public FilesystemTestFixture {
  // No additional setup needed - uses base fixture
};

TEST_F(FilesystemBasicTest, find_by_path) {
  // This test needs pre-populated test data
  input_ = create_test_instance();

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  std::vector<std::string> paths;
  filesystem_->walk([&](auto e) { paths.emplace_back(e.unix_path()); });

  EXPECT_GT(paths.size(), 10);

  for (auto const& p : paths) {
    auto dev = filesystem_->find(p);
    ASSERT_TRUE(dev) << p;
    EXPECT_FALSE(filesystem_->find(dev->inode().inode_num(), "desktop.ini")) << p;
    EXPECT_FALSE(filesystem_->find(p + "/desktop.ini")) << p;
  }
}

TEST_F(FilesystemBasicTest, root_access_github204) {
  // Setup custom test data
  input_->add("", {1, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("other", {2, 040755, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("group", {3, 040750, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("user", {4, 040700, 1, 1000, 100, 10, 42, 0, 0, 0});
  input_->add("other/file", {5, 0100644, 1, 1000, 100, 5, 42, 0, 0, 0}, "other");
  input_->add("group/file", {6, 0100640, 1, 1000, 100, 5, 42, 0, 0, 0}, "group");
  input_->add("user/file", {7, 0100600, 1, 1000, 100, 4, 42, 0, 0, 0}, "user");

  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  // Debug: Print modes of all directories
  auto other = filesystem_->find("/other");
  auto group = filesystem_->find("/group");
  auto user = filesystem_->find("/user");

  ASSERT_TRUE(other);
  ASSERT_TRUE(group);
  ASSERT_TRUE(user);

  auto iv_other = other->inode();
  auto iv_group = group->inode();
  auto iv_user = user->inode();

#ifdef _WIN32
  static constexpr int const x_ok{1};
  static constexpr int const r_ok{4};
  static constexpr int const w_ok{2};
#else
  static constexpr int const x_ok{X_OK};
  static constexpr int const r_ok{R_OK};
  static constexpr int const w_ok{W_OK};
#endif

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_group, r_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_user, r_ok, 1000, 100));

  EXPECT_TRUE(filesystem_->access(iv_other, w_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_group, w_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_user, w_ok, 1000, 100));

  EXPECT_TRUE(filesystem_->access(iv_other, x_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_group, x_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_user, x_ok, 1000, 100));

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, r_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, r_ok, 1000, 0));

  EXPECT_TRUE(filesystem_->access(iv_other, w_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, w_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, w_ok, 1000, 0));

  EXPECT_TRUE(filesystem_->access(iv_other, x_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, x_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, x_ok, 1000, 0));

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 2000, 100));
  EXPECT_TRUE(filesystem_->access(iv_group, r_ok, 2000, 100));
  EXPECT_FALSE(filesystem_->access(iv_user, r_ok, 2000, 100));

  EXPECT_FALSE(filesystem_->access(iv_other, w_ok, 2000, 100));
  EXPECT_FALSE(filesystem_->access(iv_group, w_ok, 2000, 100));
  EXPECT_FALSE(filesystem_->access(iv_user, w_ok, 2000, 100));

  EXPECT_TRUE(filesystem_->access(iv_other, x_ok, 2000, 100));
  EXPECT_TRUE(filesystem_->access(iv_group, x_ok, 2000, 100));
  EXPECT_FALSE(filesystem_->access(iv_user, x_ok, 2000, 100));

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_group, r_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_user, r_ok, 2000, 200));

  EXPECT_FALSE(filesystem_->access(iv_other, w_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_group, w_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_user, w_ok, 2000, 200));

  EXPECT_TRUE(filesystem_->access(iv_other, x_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_group, x_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_user, x_ok, 2000, 200));

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, r_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, r_ok, 0, 0));

  EXPECT_TRUE(filesystem_->access(iv_other, w_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, w_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, w_ok, 0, 0));

  EXPECT_TRUE(filesystem_->access(iv_other, x_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, x_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, x_ok, 0, 0));

  // Test file access
  other = filesystem_->find("/other/file");
  group = filesystem_->find("/group/file");
  user = filesystem_->find("/user/file");

  ASSERT_TRUE(other);
  ASSERT_TRUE(group);
  ASSERT_TRUE(user);

  iv_other = other->inode();
  iv_group = group->inode();
  iv_user = user->inode();

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_group, r_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_user, r_ok, 1000, 100));

  EXPECT_TRUE(filesystem_->access(iv_other, w_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_group, w_ok, 1000, 100));
  EXPECT_TRUE(filesystem_->access(iv_user, w_ok, 1000, 100));

  EXPECT_FALSE(filesystem_->access(iv_other, x_ok, 1000, 100));
  EXPECT_FALSE(filesystem_->access(iv_group, x_ok, 1000, 100));
  EXPECT_FALSE(filesystem_->access(iv_user, x_ok, 1000, 100));

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, r_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, r_ok, 1000, 0));

  EXPECT_TRUE(filesystem_->access(iv_other, w_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, w_ok, 1000, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, w_ok, 1000, 0));

  EXPECT_FALSE(filesystem_->access(iv_other, x_ok, 1000, 0));
  EXPECT_FALSE(filesystem_->access(iv_group, x_ok, 1000, 0));
  EXPECT_FALSE(filesystem_->access(iv_user, x_ok, 1000, 0));

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 2000, 100));
  EXPECT_TRUE(filesystem_->access(iv_group, r_ok, 2000, 100));
  EXPECT_FALSE(filesystem_->access(iv_user, r_ok, 2000, 100));

  EXPECT_FALSE(filesystem_->access(iv_other, w_ok, 2000, 100));
  EXPECT_FALSE(filesystem_->access(iv_group, w_ok, 2000, 100));
  EXPECT_FALSE(filesystem_->access(iv_user, w_ok, 2000, 100));

  EXPECT_FALSE(filesystem_->access(iv_other, x_ok, 2000, 100));
  EXPECT_FALSE(filesystem_->access(iv_group, x_ok, 2000, 100));
  EXPECT_FALSE(filesystem_->access(iv_user, x_ok, 2000, 100));

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_group, r_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_user, r_ok, 2000, 200));

  EXPECT_FALSE(filesystem_->access(iv_other, w_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_group, w_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_user, w_ok, 2000, 200));

  EXPECT_FALSE(filesystem_->access(iv_other, x_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_group, x_ok, 2000, 200));
  EXPECT_FALSE(filesystem_->access(iv_user, x_ok, 2000, 200));

  EXPECT_TRUE(filesystem_->access(iv_other, r_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, r_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, r_ok, 0, 0));

  EXPECT_TRUE(filesystem_->access(iv_other, w_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_group, w_ok, 0, 0));
  EXPECT_TRUE(filesystem_->access(iv_user, w_ok, 0, 0));

  EXPECT_FALSE(filesystem_->access(iv_other, x_ok, 0, 0));
  EXPECT_FALSE(filesystem_->access(iv_group, x_ok, 0, 0));
  EXPECT_FALSE(filesystem_->access(iv_user, x_ok, 0, 0));
}

}  // namespace dwarfs::test