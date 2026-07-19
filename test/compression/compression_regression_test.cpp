/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
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

#include <algorithm>
#include <limits>
#include <random>
#include <regex>
#include <set>
#include <sstream>

#include <gtest/gtest.h>

#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/vfs_stat.h>
#include <dwarfs/writer/segmenter_factory.h>

#include "../loremipsum.h"
#include "../mmap_mock.h"
#include "../test_common.h"
#include "../test_helpers.h"
#include "../test_logger.h"

using namespace dwarfs;

class compression_regression : public testing::TestWithParam<std::string> {};

TEST_P(compression_regression, github45) {
  auto compressor = GetParam();

  writer::segmenter::config cfg;

  constexpr size_t block_size_bits = 18;
  constexpr size_t file_size = 1 << block_size_bits;

  cfg.blockhash_window_size = 0;
  cfg.block_size_bits = block_size_bits;

  reader::filesystem_options opts;
  opts.block_cache.max_bytes = 1 << 20;
  opts.metadata.check_consistency = true;

  test::test_logger lgr;

  std::independent_bits_engine<std::mt19937_64,
                               std::numeric_limits<uint8_t>::digits, uint16_t>
      rng;

  std::string random;
  random.resize(file_size);
  std::generate(begin(random), end(random), std::ref(rng));

  auto input = std::make_shared<test::os_access_mock>();

  input->add_dir("");
  input->add_file("random", random);
  input->add_file("test", file_size);

  auto fsdata = test::build_dwarfs(lgr, input, compressor, cfg);

  auto mm = test::make_mock_file_view(fsdata);

  std::stringstream idss;
  reader::filesystem_v2::identify(lgr, *input, mm, idss, 3);

  std::string line;
  std::regex const re(
      "^SECTION \\[[^\\]]+\\] num=\\d+, type=BLOCK, compression=(\\w+).*");
  std::set<std::string> compressions;
  while (std::getline(idss, line)) {
    std::smatch m;
    if (std::regex_match(line, m, re)) {
      compressions.emplace(m[1]);
    }
  }

  if (compressor == "null") {
    EXPECT_EQ(1, compressions.size());
  } else {
    EXPECT_EQ(2, compressions.size());
  }
  EXPECT_EQ(1, compressions.count("NONE"));

  reader::filesystem_v2 fs(lgr, *input, mm, opts);

  vfs_stat vfsbuf;
  fs.statvfs(&vfsbuf);

  EXPECT_EQ(3, vfsbuf.files);
  EXPECT_EQ(2 * file_size, vfsbuf.blocks);

  auto check_file = [&](char const* name, std::string const& contents) {
    auto dev = fs.find(name);
    ASSERT_TRUE(dev);
    auto iv = dev->inode();

    auto st = fs.getattr(iv);
    EXPECT_EQ(st.size(), file_size);

    int inode = fs.open(iv);
    EXPECT_GE(inode, 0);

    auto buf = fs.read_string(inode);
    EXPECT_EQ(buf, contents);
  };

  check_file("random", random);
  check_file("test", test::loremipsum(file_size));
}

INSTANTIATE_TEST_SUITE_P(dwarfs, compression_regression,
                         ::testing::ValuesIn(test::compressions));