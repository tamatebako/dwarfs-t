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

#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/vfs_stat.h>
#include <dwarfs/writer/segmenter_factory.h>

#include <dwarfs/internal/fs_section.h>

#include "../mmap_mock.h"
#include "../test_common.h"
#include "../test_helpers.h"
#include "../test_logger.h"

using namespace dwarfs;

TEST(segmenter, regression_block_boundary) {
  writer::segmenter::config cfg;

  // make sure we don't actually segment anything
  cfg.blockhash_window_size = 12;
  cfg.block_size_bits = 10;

  reader::filesystem_options opts;
  opts.block_cache.max_bytes = 1 << 20;
  opts.metadata.check_consistency = true;

  test::test_logger lgr;

  std::vector<size_t> fs_blocks;

  for (auto size : {1023, 1024, 1025}) {
    auto input = std::make_shared<test::os_access_mock>();

    input->add_dir("");
    input->add_file("test", size);

    auto fsdata = test::build_dwarfs(lgr, input, "null", cfg);

    auto mm = test::make_mock_file_view(fsdata);

    reader::filesystem_v2 fs(lgr, *input, mm, opts);

    vfs_stat vfsbuf;
    fs.statvfs(&vfsbuf);

    EXPECT_EQ(2, vfsbuf.files);
    EXPECT_EQ(size, vfsbuf.blocks);

    fs_blocks.push_back(fs.num_blocks());
  }

  std::vector<size_t> const fs_blocks_expected{1, 1, 2};

  EXPECT_EQ(fs_blocks_expected, fs_blocks);
}

TEST(section_index_regression, github183) {
  static constexpr uint64_t section_offset_mask{(UINT64_C(1) << 48) - 1};

  test::test_logger lgr;
  writer::segmenter::config cfg{
      .block_size_bits = 10,
  };
  auto input = test::os_access_mock::create_test_instance();

  auto fsimage = test::build_dwarfs(lgr, input, "null", cfg);

  std::vector<uint64_t> index;

  {
    uint64le_t index_pos_le;

    ::memcpy(&index_pos_le,
             fsimage.data() + (fsimage.size() - sizeof(uint64_t)),
             sizeof(uint64_t));

    uint64_t index_pos = index_pos_le.load();

    ASSERT_EQ((index_pos >> 48),
              static_cast<uint16_t>(section_type::SECTION_INDEX));
    index_pos &= section_offset_mask;

    ASSERT_LT(index_pos, fsimage.size());

    auto mm = test::make_mock_file_view(fsimage);
    auto section = internal::fs_section(mm, index_pos, 2);

    EXPECT_TRUE(section.check_fast_mm(mm));

    auto const num_entries{section.length() / sizeof(uint64_t)};
    std::vector<uint64le_t> tmp(num_entries);
    auto const segment = section.segment(mm);
    auto const data = section.data(segment);
    ::memcpy(tmp.data(), data.data(), data.size());
    index.resize(num_entries);
    std::transform(begin(tmp), end(tmp), begin(index),
                   [](auto const& v) { return v.load(); });
  }

  ASSERT_GT(index.size(), 10);

  auto const schema_ix{index.size() - 4};
  auto const metadata_ix{index.size() - 3};
  auto const history_ix{index.size() - 2};

  ASSERT_EQ(index[schema_ix] >> 48,
            static_cast<uint16_t>(section_type::METADATA_V2_SCHEMA));
  ASSERT_EQ(index[metadata_ix] >> 48,
            static_cast<uint16_t>(section_type::METADATA_V2));
  ASSERT_EQ(index[history_ix] >> 48,
            static_cast<uint16_t>(section_type::HISTORY));

  auto const schema_offset{index[schema_ix] & section_offset_mask};

  auto fsimage2 = fsimage;

  ::memset(fsimage2.data() + 8, 0xff, schema_offset - 8);

  auto mm = test::make_mock_file_view(fsimage2);

  reader::filesystem_v2 fs;

  ASSERT_NO_THROW(fs = reader::filesystem_v2(lgr, *input, mm));
  EXPECT_NO_THROW(fs.walk([](auto) {}));

  auto dev = fs.find("/foo.pl");
  ASSERT_TRUE(dev);
  auto iv = dev->inode();

  auto st = fs.getattr(iv);

  int inode{-1};

  EXPECT_NO_THROW(inode = fs.open(iv));

  std::error_code ec;
  std::vector<char> buf(st.size());
  auto rv = fs.read(inode, &buf[0], st.size(), ec);

  EXPECT_TRUE(ec);
  EXPECT_EQ(rv, 0);
  EXPECT_EQ(ec.value(), EIO);

  std::stringstream idss;
  EXPECT_THROW(reader::filesystem_v2::identify(lgr, *input, mm, idss, 3),
               dwarfs::runtime_error);
}