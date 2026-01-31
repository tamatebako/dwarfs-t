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
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include <gtest/gtest.h>

#include <dwarfs/file_stat.h>
#include <dwarfs/file_type.h>
#include <dwarfs/logger.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/fsinfo_options.h>
#include <dwarfs/reader/getattr_options.h>
#include <dwarfs/reader/iovec_read_buf.h>
#include <dwarfs/vfs_stat.h>
#include <dwarfs/writer/filesystem_writer_options.h>
#include <dwarfs/writer/fragment_order_options.h>
#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>
#include <dwarfs/writer/writer_progress.h>

#include <dwarfs/writer/internal/progress.h>

#include "../loremipsum.h"
#include "../mmap_mock.h"

#ifdef _WIN32
// Undefine any existing macros that might conflict
#undef R_OK
#undef W_OK
#undef X_OK
static constexpr int const R_OK{4};
static constexpr int const W_OK{2};
static constexpr int const X_OK{1};
#endif
#include "../test_common.h"
#include "../test_helpers.h"
#include "../test_logger.h"

using namespace dwarfs;

namespace {

void basic_end_to_end_test(
    std::string const& compressor, unsigned block_size_bits,
    writer::fragment_order_mode file_order, bool with_devices,
    bool with_specials, bool set_uid, bool set_gid, bool set_time,
    bool keep_all_times, bool pack_chunk_table, bool pack_directories,
    bool pack_shared_files_table, bool pack_names, bool pack_names_index,
    bool pack_symlinks, bool pack_symlinks_index, bool plain_names_table,
    bool plain_symlinks_table, bool access_fail, size_t readahead,
    std::optional<std::string> file_hash_algo) {
  writer::segmenter::config cfg;
  writer::scanner_options options;

  cfg.blockhash_window_size = 10;
  cfg.block_size_bits = block_size_bits;

  writer::fragment_order_options order_opts;
  order_opts.mode = file_order;

  options.file_hash_algorithm = file_hash_algo;
  options.with_devices = with_devices;
  options.with_specials = with_specials;
  options.inode.fragment_order.set_default(order_opts);
  options.metadata.keep_all_times = keep_all_times;
  options.metadata.pack_chunk_table = pack_chunk_table;
  options.metadata.pack_directories = pack_directories;
  options.metadata.pack_shared_files_table = pack_shared_files_table;
  options.metadata.pack_names = pack_names;
  options.metadata.pack_names_index = pack_names_index;
  options.metadata.pack_symlinks = pack_symlinks;
  options.metadata.pack_symlinks_index = pack_symlinks_index;
  options.metadata.force_pack_string_tables = true;
  options.metadata.plain_names_table = plain_names_table;
  options.metadata.plain_symlinks_table = plain_symlinks_table;

  if (set_uid) {
    options.metadata.uid = 0;
  }

  if (set_gid) {
    options.metadata.gid = 0;
  }

  if (set_time) {
    options.metadata.timestamp = 4711;
  }

  test::test_logger lgr;

  auto input = test::os_access_mock::create_test_instance();

  if (access_fail) {
    input->set_access_fail("/somedir/ipsum.py");
  }

  writer::writer_progress wprog;

  auto ftd = std::make_shared<test::filter_transformer_data>();

  auto fsimage =
      test::build_dwarfs(lgr, input, compressor, cfg, options, {}, &wprog, ftd);

  EXPECT_EQ(14, ftd->filter_calls.size());

  auto image_size = fsimage.size();
  auto mm = test::make_mock_file_view(std::move(fsimage));

  bool similarity = file_order == writer::fragment_order_mode::SIMILARITY ||
                    file_order == writer::fragment_order_mode::NILSIMSA;

  size_t const num_fail_empty = access_fail ? 1 : 0;

  auto& prog = wprog.get_internal();

  EXPECT_EQ(8, prog.files_found);
  EXPECT_EQ(8, prog.files_scanned);
  EXPECT_EQ(2, prog.dirs_found);
  EXPECT_EQ(2, prog.dirs_scanned);
  EXPECT_EQ(2, prog.symlinks_found);
  EXPECT_EQ(2, prog.symlinks_scanned);
  EXPECT_EQ(2 * with_devices + with_specials, prog.specials_found);
  EXPECT_EQ(file_hash_algo ? 3 + num_fail_empty : 0, prog.duplicate_files);
  EXPECT_EQ(1, prog.hardlinks);
  EXPECT_GE(prog.block_count, 1);
  EXPECT_GE(prog.chunk_count, 100);
  EXPECT_EQ(7 - prog.duplicate_files, prog.inodes_scanned);
  EXPECT_EQ(file_hash_algo ? 4 - num_fail_empty : 7, prog.inodes_written);
  EXPECT_EQ(prog.files_found - prog.duplicate_files - prog.hardlinks,
            prog.inodes_written);
  EXPECT_EQ(prog.block_count, prog.blocks_written);
  EXPECT_EQ(num_fail_empty, prog.errors);
  EXPECT_EQ(access_fail ? 2046934 : 2056934, prog.original_size);
  EXPECT_EQ(23456, prog.hardlink_size);
  EXPECT_EQ(file_hash_algo ? 23456 : 0, prog.saved_by_deduplication);
  EXPECT_GE(prog.saved_by_segmentation, block_size_bits == 12 ? 0 : 1000000);
  EXPECT_EQ(prog.original_size -
                (prog.saved_by_deduplication + prog.saved_by_segmentation +
                 prog.symlink_size),
            prog.filesystem_size);
  EXPECT_EQ(prog.similarity.bytes,
            similarity ? prog.original_size -
                             (prog.saved_by_deduplication + prog.symlink_size)
                       : 0);
  EXPECT_EQ(prog.hash.scans, file_hash_algo ? 5 + num_fail_empty : 0);
  EXPECT_EQ(prog.hash.bytes, file_hash_algo ? 46912 : 0);
  EXPECT_EQ(image_size, prog.compressed_size);

  reader::filesystem_options opts;
  opts.block_cache.max_bytes = 1 << 20;
  opts.metadata.check_consistency = true;
  opts.inode_reader.readahead = readahead;

  reader::filesystem_v2 fs(lgr, *input, mm, opts);

  vfs_stat vfsbuf;
  fs.statvfs(&vfsbuf);

  EXPECT_EQ(1, vfsbuf.bsize);
  EXPECT_EQ(1, vfsbuf.frsize);
  EXPECT_EQ(access_fail ? 2046934 : 2056934, vfsbuf.blocks);
  EXPECT_EQ(11 + 2 * with_devices + with_specials, vfsbuf.files);
  EXPECT_TRUE(vfsbuf.readonly);
  EXPECT_GT(vfsbuf.namemax, 0);

  std::ostringstream dumpss;

  fs.dump(dumpss, {.features = reader::fsinfo_features::all()});

  EXPECT_GT(dumpss.str().size(), 1000) << dumpss.str();

  auto dev = fs.find("/foo.pl");
  ASSERT_TRUE(dev);
  auto iv = dev->inode();

  auto st = fs.getattr(iv);
  EXPECT_EQ(st.size(), 23456);
  EXPECT_EQ(st.uid(), set_uid ? 0 : 1337);
  EXPECT_EQ(st.gid(), 0);
  EXPECT_EQ(st.atime(), set_time ? 4711 : keep_all_times ? 4001 : 4002);
  EXPECT_EQ(st.mtime(), set_time ? 4711 : 4002);
  EXPECT_EQ(st.ctime(), set_time ? 4711 : keep_all_times ? 4003 : 4002);

  int inode = fs.open(iv);
  EXPECT_GE(inode, 0);

  std::error_code ec;
  std::vector<char> buf(st.size());
  auto rv = fs.read(inode, &buf[0], st.size(), ec);
  EXPECT_FALSE(ec);
  EXPECT_EQ(rv, st.size());
  EXPECT_EQ(std::string(buf.begin(), buf.end()), test::loremipsum(st.size()));

  for (auto mp : {&reader::filesystem_v2::walk,
                  &reader::filesystem_v2::walk_data_order}) {
    std::map<std::string, file_stat> entries;

    (fs.*mp)([&](reader::dir_entry_view e) {
      auto stbuf = fs.getattr(e.inode());
      auto path = e.path();
      if (!path.empty()) {
        path = "/" + path;
      }
      EXPECT_TRUE(entries.emplace(path, stbuf).second);
    });

    EXPECT_EQ(entries.size(),
              input->size() + 2 * with_devices + with_specials - 3);

    for (auto const& [p, st] : entries) {
      auto ref = input->symlink_info(p);
      auto access_result = input->access(p, R_OK);
      std::cerr << "[TEST_DEBUG] path='" << p << "', ref.size()=" << ref.size()
                << ", st.size()=" << st.size() << ", access=" << access_result
                << ", is_dir=" << st.is_directory() << std::endl;
      EXPECT_EQ(ref.mode(), st.mode()) << p;
      EXPECT_EQ(set_uid ? 0 : ref.uid(), st.uid()) << p;
      EXPECT_EQ(set_gid ? 0 : ref.gid(), st.gid()) << p;
      if (!st.is_directory()) {
        if (access_result == 0) {
          EXPECT_EQ(ref.size(), st.size()) << p;
        } else {
          EXPECT_EQ(0, st.size()) << p;
        }
      }
    }
  }

  auto dyn = fs.metadata_as_json();
  EXPECT_TRUE(dyn.is_object());

  auto json = fs.serialize_metadata_as_json(true);
  // TODO: JSON serialization not yet fully implemented for domain model
  // EXPECT_GT(json.size(), 1000) << json;
}

} // namespace

class compression_test
    : public testing::TestWithParam<
          std::tuple<std::string, unsigned, writer::fragment_order_mode,
                     std::optional<std::string>>> {
  DWARFS_SLOW_FIXTURE
};

TEST_P(compression_test, end_to_end) {
  auto [compressor, block_size_bits, file_order, file_hash_algo] = GetParam();

  if (compressor.find("lzma") == 0 && block_size_bits < 16) {
    // these are notoriously slow, so just skip them
    return;
  }

  size_t readahead = 0;

  if (block_size_bits < 20) {
    readahead = static_cast<size_t>(4) << block_size_bits;
  }

  basic_end_to_end_test(compressor, block_size_bits, file_order, true, true,
                        false, false, false, false, true, true, true, true,
                        true, true, true, false, false, false, readahead,
                        file_hash_algo);
}

INSTANTIATE_TEST_SUITE_P(
    dwarfs, compression_test,
    ::testing::Combine(
        ::testing::ValuesIn(test::compressions), ::testing::Values(12, 15, 20, 28),
        ::testing::Values(writer::fragment_order_mode::NONE,
                          writer::fragment_order_mode::PATH,
                          writer::fragment_order_mode::REVPATH,
                          writer::fragment_order_mode::NILSIMSA,
                          writer::fragment_order_mode::SIMILARITY),
        ::testing::Values(std::nullopt, "xxh3-128")));