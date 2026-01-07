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

#include "test_common.h"

#include <filesystem>
#include <sstream>

#include <dwarfs/block_compressor.h>
#include <dwarfs/config.h>
#include <dwarfs/logger.h>
#include <dwarfs/thread_pool.h>
#include <dwarfs/writer/entry_factory.h>
#include <dwarfs/writer/entry_filter.h>
#include <dwarfs/writer/filesystem_writer.h>
#include <dwarfs/writer/filesystem_writer_options.h>
#include <dwarfs/writer/scanner.h>
#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>
#include <dwarfs/writer/writer_progress.h>

#include "test_helpers.h"

namespace dwarfs::test {

std::string const default_file_hash_algo{"xxh3-128"};

std::vector<std::string> const compressions{
    "null",
#ifdef DWARFS_HAVE_LIBLZ4
    "lz4",
    "lz4hc:level=4",
#endif
#ifdef DWARFS_HAVE_LIBZSTD
    "zstd:level=1",
#endif
#ifdef DWARFS_HAVE_LIBLZMA
    "lzma:level=1",
    "lzma:level=1:binary=x86",
    "lzma:level=1:dict_size=15:mode=fast:mf=bt2:nice=42:depth=4711:extreme",
#endif
#ifdef DWARFS_HAVE_LIBBROTLI
    "brotli:quality=2",
#endif
};

// TODO: jeeeez, this is ugly :/
std::string
build_dwarfs(logger& lgr, std::shared_ptr<os_access_mock> input,
             std::string const& compression,
             writer::segmenter::config const& cfg,
             writer::scanner_options const& options,
             writer::filesystem_writer_options const& writer_opts,
             writer::writer_progress* prog,
             std::shared_ptr<filter_transformer_data> ftd,
             std::optional<std::span<std::filesystem::path const>> input_list,
             std::unique_ptr<writer::entry_filter> filter) {
  // force multithreading
  thread_pool pool(lgr, *input, "worker", 4);

  std::unique_ptr<writer::writer_progress> local_prog;
  if (!prog) {
    local_prog = std::make_unique<writer::writer_progress>();
    prog = local_prog.get();
  }

  // TODO: ugly hack :-)
  writer::segmenter_factory::config sf_cfg;
  sf_cfg.block_size_bits = cfg.block_size_bits;
  sf_cfg.blockhash_window_size.set_default(cfg.blockhash_window_size);
  sf_cfg.window_increment_shift.set_default(cfg.window_increment_shift);
  sf_cfg.max_active_blocks.set_default(cfg.max_active_blocks);
  sf_cfg.bloom_filter_size.set_default(cfg.bloom_filter_size);

  writer::segmenter_factory sf(lgr, *prog, sf_cfg);
  writer::entry_factory ef;

  writer::scanner s(lgr, pool, sf, ef, *input, options);

  if (ftd) {
    s.add_filter(std::make_unique<mock_filter>(ftd));
  }

  if (filter) {
    s.add_filter(std::move(filter));
  }

  std::ostringstream oss;

  block_compressor bc(compression);
  writer::filesystem_writer fsw(oss, lgr, pool, *prog, writer_opts);
  fsw.add_default_compressor(bc);

  s.scan(fsw, std::filesystem::path("/"), *prog, input_list);

  return oss.str();
}

} // namespace dwarfs::test