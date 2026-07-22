/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <map>
#include <string>
#include <vector>

#include <gmock/gmock.h>

#include <nlohmann/json.hpp>

#include "test_tool_main_tester.h"

using namespace dwarfs::test;
using namespace dwarfs;

TEST(mkdwarfs_test, duplicate_content_files_read_own_data) {
  // Regression test for a bug where files with duplicate content would read
  // back another file's data: the reader resolved chunk table indices using
  // a file-size heuristic instead of the shared files table, conflating
  // files that merely share a size.
  std::map<std::string, std::string> const expected{
      // a.txt and a-copy.txt have identical content; b.txt and d.txt have
      // the same size but different content, c.txt has a distinct size.
      {"a.txt", "alpha content for a\n"},
      {"a-copy.txt", "alpha content for a\n"},
      {"b.txt", "bravo content for b\n"},
      {"c.txt", "charlie content for cc\n"},
      {"d.txt", "delta content for d\n"},
  };

  auto t = mkdwarfs_tester::create_empty();
  t.add_root_dir();

  for (auto const& [name, content] : expected) {
    t.os->add_file(name, content);
  }

  ASSERT_EQ(0, t.run("-i / -o - --no-progress")) << t.err();

  auto fs = t.fs_from_stdout();

  std::map<std::string, std::string> actual;

  fs.walk([&](auto const& dev) {
    auto iv = dev.inode();
    if (iv.is_regular_file()) {
      std::string data;
      auto stat = fs.getattr(iv);
      data.resize(stat.size());
      ASSERT_EQ(data.size(),
                fs.read(iv.inode_num(), data.data(), data.size()));
      ASSERT_TRUE(actual.emplace(dev.unix_path(), std::move(data)).second);
    }
  });

  EXPECT_THAT(actual, ::testing::ContainerEq(expected));

  // The exported metadata listing must contain all files, including those
  // with duplicate content.
  auto const listing = nlohmann::json::parse(fs.serialize_metadata_as_json(false));
  std::vector<std::string> names;
  for (auto const& e : listing["root"]["inodes"]) {
    names.push_back(e["name"].template get<std::string>());
  }
  EXPECT_THAT(names, ::testing::UnorderedElementsAre(
                         "a-copy.txt", "a.txt", "b.txt", "c.txt", "d.txt"));
}

TEST(mkdwarfs_test, empty_shared_files_read_no_data) {
  // Regression test for a bug where zero-size files in the shared files table
  // (the empty-content duplicate group) read back the first file's data: the
  // reader redirected an empty chunk range to chunk table index 0 instead of
  // returning an empty range.
  std::map<std::string, std::string> const expected{
      // Several zero-size files so the empty-content group is deduplicated
      // into the shared files table.
      {"empty1.txt", ""},
      {"empty2.txt", ""},
      {"empty3.txt", ""},
      // A group of duplicate non-empty files.
      {"dup1.txt", "duplicate content for dup group\n"},
      {"dup2.txt", "duplicate content for dup group\n"},
      {"dup3.txt", "duplicate content for dup group\n"},
      // Unique files.
      {"uniq1.txt", "unique content one\n"},
      {"uniq2.txt", "unique content twenty-two\n"},
  };

  auto t = mkdwarfs_tester::create_empty();
  t.add_root_dir();

  for (auto const& [name, content] : expected) {
    t.os->add_file(name, content);
  }

  ASSERT_EQ(0, t.run("-i / -o - --no-progress")) << t.err();

  auto fs = t.fs_from_stdout();

  std::map<std::string, std::string> actual;

  fs.walk([&](auto const& dev) {
    auto iv = dev.inode();
    if (iv.is_regular_file()) {
      // Raw read with no size clamp: the buffer is deliberately larger than
      // any file so the reader must report the correct number of bytes.
      std::string data;
      data.resize(8192, '\xff');
      auto const n = fs.read(iv.inode_num(), data.data(), data.size());
      data.resize(n);
      ASSERT_TRUE(actual.emplace(dev.unix_path(), std::move(data)).second);
    }
  });

  EXPECT_THAT(actual, ::testing::ContainerEq(expected));
}

TEST(mkdwarfs_test, directory_streams_synthesize_dot_entries) {
  // Regression test for missing "." and ".." synthesis in the domain
  // metadata readdir path: directory streams must lead with "." (self)
  // and ".." (parent), then the real entries shifted by two, and dirsize
  // must count them. ruby's Dir.read (via tebako's memfs) relies on this.
  auto t = mkdwarfs_tester::create_empty();
  t.add_root_dir();
  t.os->add_dir("somedir");
  t.os->add_file("somedir/file-1.txt", "one\n");
  t.os->add_file("somedir/file-2.txt", "two\n");
  t.os->add_dir("emptydir");
  ASSERT_EQ(0, t.run("-i / -o - --no-progress")) << t.err();
  auto fs = t.fs_from_stdout();

  auto dev = fs.find("/somedir");
  ASSERT_TRUE(dev);
  auto iv = dev->inode();
  auto dir = fs.opendir(iv);
  ASSERT_TRUE(dir);

  ASSERT_EQ(4u, fs.dirsize(*dir));

  auto r0 = fs.readdir(*dir, 0);
  ASSERT_TRUE(r0);
  EXPECT_EQ(".", r0->name());
  EXPECT_EQ(iv.inode_num(), r0->inode().inode_num());

  auto r1 = fs.readdir(*dir, 1);
  ASSERT_TRUE(r1);
  EXPECT_EQ("..", r1->name());
  auto parent = fs.find("/");
  ASSERT_TRUE(parent);
  EXPECT_EQ(parent->inode().inode_num(), r1->inode().inode_num());

  std::vector<std::string> names;
  for (size_t off = 2; off < fs.dirsize(*dir); ++off) {
    auto r = fs.readdir(*dir, off);
    ASSERT_TRUE(r);
    names.push_back(r->name());
  }
  EXPECT_THAT(names, ::testing::UnorderedElementsAre("file-1.txt", "file-2.txt"));

  // Empty directory: just the two synthesized entries.
  auto edev = fs.find("/emptydir");
  ASSERT_TRUE(edev);
  auto edir = fs.opendir(edev->inode());
  ASSERT_TRUE(edir);
  ASSERT_EQ(2u, fs.dirsize(*edir));
  EXPECT_TRUE(fs.readdir(*edir, 0));
  EXPECT_TRUE(fs.readdir(*edir, 1));
  EXPECT_FALSE(fs.readdir(*edir, 2));
}
