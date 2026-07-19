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

#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include <dwarfs/file_stat.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/mlock_mode.h>

namespace dwarfs {

class logger;
class os_access;
class performance_monitor;

namespace reader {

/**
 * Configuration for filesystem loading
 */
struct filesystem_load_config {
  // Image location
  std::filesystem::path image_path;

  // Optional image offset (for self-extracting archives)
  std::optional<file_off_t> image_offset;

  // Optional image size limit
  std::optional<file_off_t> image_size;

  // Cache settings
  size_t cache_size{512 << 20};  // 512 MiB default
  size_t block_size{512 << 10};   // 512 KiB default
  size_t readahead{0};
  size_t num_workers{2};

  // Memory locking
  mlock_mode lock_mode{mlock_mode::NONE};

  // Decompression settings
  double decompress_ratio{0.8};
  size_t seq_detector_threshold{4};

  // Block allocator
  block_cache_allocation_mode block_allocator{
      block_cache_allocation_mode::MALLOC};

  // Filesystem options
  bool readonly{false};
  bool case_insensitive{false};
  bool enable_sparse_files{true};

#ifndef _WIN32
  // User/group overrides
  std::optional<file_stat::uid_type> fs_uid;
  std::optional<file_stat::gid_type> fs_gid;
#endif

  // Inode offset (for FUSE compatibility)
  int inode_offset{0};

#if DWARFS_PERFMON_ENABLED
  // Performance monitoring
  std::unordered_set<std::string> perfmon_enabled;
  std::optional<std::filesystem::path> perfmon_trace_file;
#endif
};

/**
 * Filesystem loader
 *
 * Provides a reusable interface for loading DwarFS filesystem images.
 * This class encapsulates all the complexity of filesystem initialization,
 * making it easy for external projects to embed DwarFS functionality.
 *
 * Example usage:
 * ```cpp
 * filesystem_load_config config;
 * config.image_path = "/path/to/image.dwarfs";
 * config.cache_size = 1024 * 1024 * 1024;  // 1 GiB
 *
 * auto fs = filesystem_loader::load(lgr, os, config, perfmon);
 * ```
 */
class filesystem_loader {
 public:
  /**
   * Load a DwarFS filesystem image
   *
   * @param lgr Logger for diagnostic output
   * @param os OS access layer for file I/O
   * @param config Loading configuration
   * @param perfmon Optional performance monitor
   * @return Loaded filesystem instance
   * @throws std::exception on load failure
   */
  static filesystem_v2_lite load(
      logger& lgr, os_access& os, filesystem_load_config const& config,
      std::shared_ptr<performance_monitor> perfmon = nullptr);

  /**
   * Create filesystem options from load config
   *
   * Helper method to convert load config to filesystem options.
   * Useful for custom loading scenarios.
   *
   * @param config Load configuration
   * @return Filesystem options ready for filesystem_v2_lite construction
   */
  static filesystem_options
  make_options(filesystem_load_config const& config);

 private:
  filesystem_loader() = delete;  // Static class, no instances
};

} // namespace reader
} // namespace dwarfs