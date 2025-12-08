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
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <dwarfs/file_stat.h>
#include <dwarfs/logger.h>
#include <dwarfs/reader/cache_tidy_config.h>
#include <dwarfs/reader/block_cache_options.h>
#include <dwarfs/reader/mlock_mode.h>
#include <dwarfs/tool/sys_char.h>

// Forward declarations for FUSE
struct fuse_args;

namespace dwarfs::tool {

class iolayer;

namespace dwarfs_tool {

/**
 * Parsed command-line options for dwarfs FUSE driver
 */
struct parsed_options {
  // Image and mountpoint
  std::shared_ptr<std::string> fsimage;
  int seen_mountpoint{0};

  // String options (used by FUSE option parsing - must be char const*)
  char const* cachesize_str{nullptr};
  char const* blocksize_str{nullptr};
  char const* readahead_str{nullptr};
  char const* debuglevel_str{nullptr};
  char const* workers_str{nullptr};
  char const* mlock_str{nullptr};
  char const* decompress_ratio_str{nullptr};
  char const* image_offset_str{nullptr};
  char const* image_size_str{nullptr};
  char const* cache_tidy_strategy_str{nullptr};
  char const* cache_tidy_interval_str{nullptr};
  char const* cache_tidy_max_age_str{nullptr};
  char const* block_alloc_mode_str{nullptr};
  char const* seq_detector_thresh_str{nullptr};
  char const* analysis_file_str{nullptr};
#ifndef _WIN32
  char const* uid_str{nullptr};
  char const* gid_str{nullptr};
#endif
#if DWARFS_PERFMON_ENABLED
  char const* perfmon_enabled_str{nullptr};
  char const* perfmon_trace_file_str{nullptr};
#endif

  // Parsed values (populated from string options)
  size_t cachesize{0};
  size_t blocksize{0};
  size_t readahead{0};
  size_t workers{0};

  // Memory locking
  reader::mlock_mode lock_mode{reader::mlock_mode::NONE};

  // Decompression ratio
  double decompress_ratio{0.0};

  // Preloading
  char const* preload_category_str{nullptr};
  int preload_all{0};

  // File system options
  int readonly{0};
  int case_insensitive{0};
  int cache_files{1};
#ifdef DWARFS_FUSE_HAS_LSEEK
  int cache_sparse{0};
#endif

  // Cache tidying
  reader::cache_tidy_strategy block_cache_tidy_strategy{
      reader::cache_tidy_strategy::NONE};
  std::chrono::nanoseconds block_cache_tidy_interval{std::chrono::minutes(5)};
  std::chrono::nanoseconds block_cache_tidy_max_age{std::chrono::minutes{10}};

  // Block allocator
  reader::block_cache_allocation_mode block_allocator{
      reader::block_cache_allocation_mode::MALLOC};

  // Sequential detector
  size_t seq_detector_threshold{4};

#ifndef _WIN32
  // User/group override
  std::optional<file_stat::uid_type> fs_uid;
  std::optional<file_stat::gid_type> fs_gid;
#endif

  // Obsolete options (kept for compatibility warnings)
  int enable_nlink{0};
  int cache_image{0};

  // Logging
  logger_options logopts{};

  // Help/man flags
  bool is_help{false};
#ifdef DWARFS_BUILTIN_MANPAGE
  bool is_man{false};
#endif
  bool is_auto_mountpoint{false};
};

/**
 * Options parser for dwarfs FUSE driver
 *
 * Handles parsing and validation of all command-line options using FUSE's
 * option parsing system.
 */
class options_parser {
 public:
  options_parser() = default;

  /**
   * Parse command-line arguments
   *
   * @param argc Argument count
   * @param argv Argument vector
   * @param iol I/O layer for file access and output
   * @param opts Output structure for parsed options
   * @param args FUSE args structure to populate
   * @param progname Program name extracted from argv[0]
   * @return 0 on success, 1 on error/help shown
   */
  int parse(int argc, sys_char** argv, iolayer const& iol,
            parsed_options& opts, fuse_args& args,
            std::filesystem::path& progname);

 private:
  // Helper methods
  void parse_string_options(parsed_options& opts, iolayer const& iol);
  void validate_options(parsed_options const& opts, iolayer const& iol);
  void apply_defaults(parsed_options& opts);
};

} // namespace dwarfs_tool
} // namespace dwarfs::tool