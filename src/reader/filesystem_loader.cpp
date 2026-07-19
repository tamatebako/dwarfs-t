/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <dwarfs/reader/filesystem_loader.h>

#include <filesystem>
#include <iostream>
#include <memory>

#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/performance_monitor.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/util.h>

namespace dwarfs::reader {

filesystem_options
filesystem_loader::make_options(filesystem_load_config const& config) {
  filesystem_options opts;

  // Lock mode
  opts.lock_mode = config.lock_mode;

  // Block cache settings
  opts.block_cache.max_bytes = config.cache_size;
  opts.block_cache.num_workers = config.num_workers;
  opts.block_cache.decompress_ratio = config.decompress_ratio;
  opts.block_cache.sequential_access_detector_threshold =
      config.seq_detector_threshold;
  opts.block_cache.allocation_mode = config.block_allocator;

  // Inode reader settings
  opts.inode_reader.readahead = config.readahead;

  // Metadata settings
  opts.metadata.enable_sparse_files = config.enable_sparse_files;
  opts.metadata.readonly = config.readonly;
  opts.metadata.case_insensitive_lookup = config.case_insensitive;
  opts.metadata.block_size = config.block_size;

#ifndef _WIN32
  opts.metadata.fs_uid = config.fs_uid;
  opts.metadata.fs_gid = config.fs_gid;
#endif

  // Inode offset
  opts.inode_offset = config.inode_offset;

  // Image offset and size
  if (config.image_offset) {
    opts.image_offset = *config.image_offset;
  }

  if (config.image_size) {
    opts.image_size = *config.image_size;
  }

  return opts;
}

filesystem_v2_lite
filesystem_loader::load(logger& lgr, os_access& os,
                        filesystem_load_config const& config,
                        std::shared_ptr<performance_monitor> perfmon) {
  // Temporarily disable LOG_PROXY to debug crash
  // LOG_PROXY(prod_logger_policy, lgr);

  // auto ti = LOG_TIMED_INFO;
  [[maybe_unused]] auto& opts = config;

  // Canonicalize the image path
  auto fsimage = os.canonical(config.image_path);

  // LOG_DEBUG << "attempting to load filesystem from " << fsimage;

  // Create filesystem options
  auto fsopts = make_options(config);

  // Create the filesystem instance
  filesystem_v2_lite fs(lgr, os, fsimage, fsopts, perfmon);

  // ti << "file system initialized";

  return fs;
}

} // namespace dwarfs::reader