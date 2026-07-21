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
