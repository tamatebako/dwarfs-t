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

#include <dwarfs/tool/dwarfs/argtable3_options_parser.h>

#include <stdexcept>
#include <string>

#include <argtable3.h>
#include <fmt/format.h>

#include <dwarfs/conv.h>
#include <dwarfs/error.h>
#include <dwarfs/util.h>

namespace dwarfs::tool::dwarfs {

namespace {

using namespace std::string_view_literals;

} // anonymous namespace

argtable3_options_parser::argtable3_options_parser() = default;

argtable3_options_parser::~argtable3_options_parser() = default;

int argtable3_options_parser::parse(int argc, char** argv) {
  // Set defaults
  opts_.cache_size = "512m";
  opts_.block_size = "512k";
  opts_.readahead = "0";
  opts_.num_workers = 2;
  opts_.mlock_mode = "none";
  opts_.decompress_ratio = "0.8";
  opts_.image_offset = "auto";
  opts_.cache_files = true;
#ifdef DWARFS_FUSE_HAS_LSEEK
  opts_.cache_sparse = false;
#endif
  opts_.tidy_strategy = "none";
  opts_.tidy_interval = "5m";
  opts_.tidy_max_age = "10m";
  opts_.block_allocator = "malloc";
  opts_.seq_detector = "4";

  // Initialize argtable (estimate ~70 options for dwarfs)
  init_argtable(75);

  // Add common options (help, version, man)
  add_common_options();

  // Add logger options (log-level, verbose, quiet)
  add_logger_options();

  // Add tool-specific options
  define_tool_options();

  // Finalize argtable
  finalize_argtable();

  // Parse arguments
  int nerrors = parse_args(argc, argv);

  // Check for help/version/man BEFORE validation
  if (help_requested_ || version_requested_ || man_requested_) {
    return 1; // Signal that we displayed help/version/man
  }

  if (nerrors > 0) {
    return 2; // Parse error
  }

  // Populate parsed_options from argtable values
  populate_parsed_options();

  // Validate options
  if (!validate_options()) {
    return 2; // Validation error
  }

  return 0; // Success
}

void argtable3_options_parser::define_tool_options() {
  // Positional arguments (optional to allow --version/--help without image)
  image_opt_ = arg_file0(nullptr, nullptr, "<image>", "input filesystem image");
  mountpoint_opt_ = arg_file0(nullptr, nullptr, "<mountpoint>", "mount point directory");

  // Cache options
  cache_size_opt_ = arg_str0(nullptr, "cachesize", "<size>", "block cache size (default: 512m)");
  block_size_opt_ = arg_str0(nullptr, "blocksize", "<size>", "file I/O block size (default: 512k)");
  readahead_opt_ = arg_str0(nullptr, "readahead", "<size>", "readahead size (default: 0)");
  num_workers_opt_ = arg_int0(nullptr, "workers", "<count>", "number of worker threads (default: 2)");

  // Memory locking
  mlock_mode_opt_ = arg_str0(nullptr, "mlock", "<mode>", "mlock mode: none, try, must (default: none)");

  // Decompression
  decompress_ratio_opt_ = arg_str0(nullptr, "decratio", "<ratio>", "ratio for full decompression (default: 0.8)");

  // Filesystem image options
  image_offset_opt_ = arg_str0("O", "offset", "<offset>", "filesystem image offset in bytes (default: auto)");
  image_size_opt_ = arg_str0(nullptr, "imagesize", "<size>", "filesystem image size in bytes");

  // Filesystem behavior
  readonly_opt_ = arg_lit0(nullptr, "readonly", "show read-only file system");
  case_insensitive_opt_ = arg_lit0(nullptr, "case-insensitive", "perform case-insensitive lookups");

  // Cache behavior
  cache_files_opt_ = arg_lit0(nullptr, "cache-files", "keep files in kernel cache (default)");
  no_cache_files_opt_ = arg_lit0(nullptr, "no-cache-files", "don't keep files in kernel cache");
#ifdef DWARFS_FUSE_HAS_LSEEK
  cache_sparse_opt_ = arg_lit0(nullptr, "cache-sparse", "keep sparse files in kernel cache");
  no_cache_sparse_opt_ = arg_lit0(nullptr, "no-cache-sparse", "don't keep sparse files in kernel cache (default)");
#endif

  // Preloading
  preload_category_opt_ = arg_str0(nullptr, "preload-category", "<name>", "preload blocks from this category");
  preload_all_opt_ = arg_lit0(nullptr, "preload-all", "preload all file system blocks");

  // Cache tidying
  tidy_strategy_opt_ = arg_str0(nullptr, "tidy-strategy", "<strategy>", "cache tidy strategy: none, time, swap (default: none)");
  tidy_interval_opt_ = arg_str0(nullptr, "tidy-interval", "<time>", "interval for cache tidying (default: 5m)");
  tidy_max_age_opt_ = arg_str0(nullptr, "tidy-max-age", "<time>", "tidy blocks after this time (default: 10m)");

  // Block allocator
  block_allocator_opt_ = arg_str0(nullptr, "block-allocator", "<allocator>", "block allocator: malloc, mmap (default: malloc)");

  // Sequential access
  seq_detector_opt_ = arg_str0(nullptr, "seq-detector", "<threshold>", "sequential access detector threshold (default: 4)");

  // Analysis
  analysis_file_opt_ = arg_file0(nullptr, "analysis-file", "<file>", "write accessed files to this file");

#ifndef _WIN32
  // User/group override
  uid_opt_ = arg_str0(nullptr, "uid", "<uid>", "override user ID for file system");
  gid_opt_ = arg_str0(nullptr, "gid", "<gid>", "override group ID for file system");
#endif

#if DWARFS_PERFMON_ENABLED
  // Performance monitoring
  perfmon_opt_ = arg_str0(nullptr, "perfmon", "<options>", "enable performance monitor");
  perfmon_trace_opt_ = arg_file0(nullptr, "perfmon-trace", "<file>", "write performance monitor trace file");
#endif

  // Special modes
  auto_mountpoint_opt_ = arg_lit0(nullptr, "auto-mountpoint", "auto-select mountpoint based on image name");

  // Obsolete options (for compatibility warnings)
  enable_nlink_opt_ = arg_lit0(nullptr, "enable-nlink", "obsolete: enable nlink (no effect)");
  cache_image_opt_ = arg_lit0(nullptr, "cache-image", "obsolete: cache image (no effect)");
  no_cache_image_opt_ = arg_lit0(nullptr, "no-cache-image", "obsolete: don't cache image (no effect)");

  // Add all options to argtable
  argtable_.push_back(image_opt_);
  argtable_.push_back(mountpoint_opt_);
  argtable_.push_back(cache_size_opt_);
  argtable_.push_back(block_size_opt_);
  argtable_.push_back(readahead_opt_);
  argtable_.push_back(num_workers_opt_);
  argtable_.push_back(mlock_mode_opt_);
  argtable_.push_back(decompress_ratio_opt_);
  argtable_.push_back(image_offset_opt_);
  argtable_.push_back(image_size_opt_);
  argtable_.push_back(readonly_opt_);
  argtable_.push_back(case_insensitive_opt_);
  argtable_.push_back(cache_files_opt_);
  argtable_.push_back(no_cache_files_opt_);
#ifdef DWARFS_FUSE_HAS_LSEEK
  argtable_.push_back(cache_sparse_opt_);
  argtable_.push_back(no_cache_sparse_opt_);
#endif
  argtable_.push_back(preload_category_opt_);
  argtable_.push_back(preload_all_opt_);
  argtable_.push_back(tidy_strategy_opt_);
  argtable_.push_back(tidy_interval_opt_);
  argtable_.push_back(tidy_max_age_opt_);
  argtable_.push_back(block_allocator_opt_);
  argtable_.push_back(seq_detector_opt_);
  argtable_.push_back(analysis_file_opt_);
#ifndef _WIN32
  argtable_.push_back(uid_opt_);
  argtable_.push_back(gid_opt_);
#endif
#if DWARFS_PERFMON_ENABLED
  argtable_.push_back(perfmon_opt_);
  argtable_.push_back(perfmon_trace_opt_);
#endif
  argtable_.push_back(auto_mountpoint_opt_);
  argtable_.push_back(enable_nlink_opt_);
  argtable_.push_back(cache_image_opt_);
  argtable_.push_back(no_cache_image_opt_);
}

void argtable3_options_parser::populate_parsed_options() {
  // Positional arguments
  if (image_opt_->count > 0) {
    opts_.image = string_to_sys_string(image_opt_->filename[0]);
  }
  if (mountpoint_opt_->count > 0) {
    opts_.mountpoint = string_to_sys_string(mountpoint_opt_->filename[0]);
  }

  // Cache options
  if (cache_size_opt_->count > 0) {
    opts_.cache_size = cache_size_opt_->sval[0];
  }
  if (block_size_opt_->count > 0) {
    opts_.block_size = block_size_opt_->sval[0];
  }
  if (readahead_opt_->count > 0) {
    opts_.readahead = readahead_opt_->sval[0];
  }
  if (num_workers_opt_->count > 0) {
    opts_.num_workers = static_cast<size_t>(num_workers_opt_->ival[0]);
  }

  // Memory locking
  if (mlock_mode_opt_->count > 0) {
    opts_.mlock_mode = mlock_mode_opt_->sval[0];
  }

  // Decompression
  if (decompress_ratio_opt_->count > 0) {
    opts_.decompress_ratio = decompress_ratio_opt_->sval[0];
  }

  // Filesystem image options
  if (image_offset_opt_->count > 0) {
    opts_.image_offset = image_offset_opt_->sval[0];
  }
  if (image_size_opt_->count > 0) {
    opts_.image_size = image_size_opt_->sval[0];
  }

  // Filesystem behavior
  opts_.readonly = readonly_opt_->count > 0;
  opts_.case_insensitive = case_insensitive_opt_->count > 0;

  // Cache behavior (mutually exclusive flags)
  if (no_cache_files_opt_->count > 0) {
    opts_.cache_files = false;
  } else if (cache_files_opt_->count > 0) {
    opts_.cache_files = true;
  }

#ifdef DWARFS_FUSE_HAS_LSEEK
  if (cache_sparse_opt_->count > 0) {
    opts_.cache_sparse = true;
  } else if (no_cache_sparse_opt_->count > 0) {
    opts_.cache_sparse = false;
  }
#endif

  // Preloading
  if (preload_category_opt_->count > 0) {
    opts_.preload_category = preload_category_opt_->sval[0];
  }
  opts_.preload_all = preload_all_opt_->count > 0;

  // Cache tidying
  if (tidy_strategy_opt_->count > 0) {
    opts_.tidy_strategy = tidy_strategy_opt_->sval[0];
  }
  if (tidy_interval_opt_->count > 0) {
    opts_.tidy_interval = tidy_interval_opt_->sval[0];
  }
  if (tidy_max_age_opt_->count > 0) {
    opts_.tidy_max_age = tidy_max_age_opt_->sval[0];
  }

  // Block allocator
  if (block_allocator_opt_->count > 0) {
    opts_.block_allocator = block_allocator_opt_->sval[0];
  }

  // Sequential access
  if (seq_detector_opt_->count > 0) {
    opts_.seq_detector = seq_detector_opt_->sval[0];
  }

  // Analysis
  if (analysis_file_opt_->count > 0) {
    opts_.analysis_file = string_to_sys_string(analysis_file_opt_->filename[0]);
  }

#ifndef _WIN32
  // User/group override
  if (uid_opt_->count > 0) {
    opts_.uid = uid_opt_->sval[0];
  }
  if (gid_opt_->count > 0) {
    opts_.gid = gid_opt_->sval[0];
  }
#endif

#if DWARFS_PERFMON_ENABLED
  // Performance monitoring
  if (perfmon_opt_->count > 0) {
    opts_.perfmon = perfmon_opt_->sval[0];
  }
  if (perfmon_trace_opt_->count > 0) {
    opts_.perfmon_trace_file = string_to_sys_string(perfmon_trace_opt_->filename[0]);
  }
#endif

  // Special modes
  opts_.auto_mountpoint = auto_mountpoint_opt_->count > 0;

  // Obsolete options
  opts_.enable_nlink = enable_nlink_opt_->count > 0;
  if (cache_image_opt_->count > 0) {
    opts_.cache_image = "1";
  } else if (no_cache_image_opt_->count > 0) {
    opts_.cache_image = "0";
  }
}

bool argtable3_options_parser::validate_options() {
  try {
    // Validate image is provided
    if (opts_.image.empty()) {
      throw std::runtime_error("filesystem image is required");
    }

    // Validate mountpoint XOR auto-mountpoint
    if (opts_.auto_mountpoint && !opts_.mountpoint.empty()) {
      throw std::runtime_error("cannot specify both <mountpoint> and --auto-mountpoint");
    }

    if (!opts_.auto_mountpoint && opts_.mountpoint.empty()) {
      throw std::runtime_error("mountpoint is required (or use --auto-mountpoint)");
    }

    // Validate numeric ranges
    if (auto ratio = try_to<double>(opts_.decompress_ratio)) {
      if (*ratio < 0.0 || *ratio > 1.0) {
        throw std::runtime_error("decratio must be between 0.0 and 1.0");
      }
    } else {
      throw std::runtime_error(fmt::format("invalid decratio: {}", opts_.decompress_ratio));
    }

    // Validate mlock mode
    if (opts_.mlock_mode != "none" && opts_.mlock_mode != "try" && opts_.mlock_mode != "must") {
      throw std::runtime_error(fmt::format("invalid mlock mode: {} (must be none, try, or must)", opts_.mlock_mode));
    }

    // Validate tidy strategy
    if (opts_.tidy_strategy != "none" && opts_.tidy_strategy != "time" && opts_.tidy_strategy != "swap") {
      throw std::runtime_error(fmt::format("invalid tidy strategy: {} (must be none, time, or swap)", opts_.tidy_strategy));
    }

    // Validate block allocator
    if (opts_.block_allocator != "malloc" && opts_.block_allocator != "mmap") {
      throw std::runtime_error(fmt::format("invalid block allocator: {} (must be malloc or mmap)", opts_.block_allocator));
    }

    // Validate cache_files / no_cache_files mutex
    if (cache_files_opt_->count > 0 && no_cache_files_opt_->count > 0) {
      throw std::runtime_error("cannot specify both --cache-files and --no-cache-files");
    }

#ifdef DWARFS_FUSE_HAS_LSEEK
    // Validate cache_sparse / no_cache_sparse mutex
    if (cache_sparse_opt_->count > 0 && no_cache_sparse_opt_->count > 0) {
      throw std::runtime_error("cannot specify both --cache-sparse and --no-cache-sparse");
    }
#endif

  } catch (std::exception const& e) {
    fmt::print(stderr, "error: {}\n", e.what());
    return false;
  }
  return true;
}

void argtable3_options_parser::load_environment_variables() {
  // Load base environment variables (DWARFS_LOG_LEVEL, etc.)
  argtable3_base_parser::load_environment_variables("DWARFS");

  // Load tool-specific environment variables
  // Format: DWARFS_DWARFS_<OPTION>
  std::string prefix = "DWARFS_DWARFS_";

  // Cache options
  if (auto env_val = get_env_var(prefix + "CACHE_SIZE"); !env_val.empty()) {
    if (cache_size_opt_->count == 0) {
      opts_.cache_size = env_val;
    }
  }

  if (auto env_val = get_env_var(prefix + "BLOCK_SIZE"); !env_val.empty()) {
    if (block_size_opt_->count == 0) {
      opts_.block_size = env_val;
    }
  }

  if (auto env_val = get_env_var(prefix + "READAHEAD"); !env_val.empty()) {
    if (readahead_opt_->count == 0) {
      opts_.readahead = env_val;
    }
  }

  if (auto env_val = get_env_var(prefix + "NUM_WORKERS"); !env_val.empty()) {
    if (num_workers_opt_->count == 0) {
      if (auto val = try_to<size_t>(env_val)) {
        opts_.num_workers = *val;
      }
    }
  }

  // Memory locking
  if (auto env_val = get_env_var(prefix + "MLOCK_MODE"); !env_val.empty()) {
    if (mlock_mode_opt_->count == 0) {
      opts_.mlock_mode = env_val;
    }
  }

  // Decompression
  if (auto env_val = get_env_var(prefix + "DECOMPRESS_RATIO"); !env_val.empty()) {
    if (decompress_ratio_opt_->count == 0) {
      opts_.decompress_ratio = env_val;
    }
  }

  // Filesystem image options
  if (auto env_val = get_env_var(prefix + "IMAGE_OFFSET"); !env_val.empty()) {
    if (image_offset_opt_->count == 0) {
      opts_.image_offset = env_val;
    }
  }

  if (auto env_val = get_env_var(prefix + "IMAGE_SIZE"); !env_val.empty()) {
    if (image_size_opt_->count == 0) {
      opts_.image_size = env_val;
    }
  }

  // Filesystem behavior flags
  if (auto env_val = get_env_var(prefix + "READONLY"); !env_val.empty()) {
    if (readonly_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.readonly = true;
    }
  }

  if (auto env_val = get_env_var(prefix + "CASE_INSENSITIVE"); !env_val.empty()) {
    if (case_insensitive_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.case_insensitive = true;
    }
  }

  // Cache behavior
  if (auto env_val = get_env_var(prefix + "CACHE_FILES"); !env_val.empty()) {
    if (cache_files_opt_->count == 0 && no_cache_files_opt_->count == 0) {
      opts_.cache_files = (env_val == "1" || env_val == "true");
    }
  }

#ifdef DWARFS_FUSE_HAS_LSEEK
  if (auto env_val = get_env_var(prefix + "CACHE_SPARSE"); !env_val.empty()) {
    if (cache_sparse_opt_->count == 0 && no_cache_sparse_opt_->count == 0) {
      opts_.cache_sparse = (env_val == "1" || env_val == "true");
    }
  }
#endif

  // Preloading
  if (auto env_val = get_env_var(prefix + "PRELOAD_CATEGORY"); !env_val.empty()) {
    if (preload_category_opt_->count == 0) {
      opts_.preload_category = env_val;
    }
  }

  if (auto env_val = get_env_var(prefix + "PRELOAD_ALL"); !env_val.empty()) {
    if (preload_all_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.preload_all = true;
    }
  }

  // Cache tidying
  if (auto env_val = get_env_var(prefix + "TIDY_STRATEGY"); !env_val.empty()) {
    if (tidy_strategy_opt_->count == 0) {
      opts_.tidy_strategy = env_val;
    }
  }

  if (auto env_val = get_env_var(prefix + "TIDY_INTERVAL"); !env_val.empty()) {
    if (tidy_interval_opt_->count == 0) {
      opts_.tidy_interval = env_val;
    }
  }

  if (auto env_val = get_env_var(prefix + "TIDY_MAX_AGE"); !env_val.empty()) {
    if (tidy_max_age_opt_->count == 0) {
      opts_.tidy_max_age = env_val;
    }
  }

  // Block allocator
  if (auto env_val = get_env_var(prefix + "BLOCK_ALLOCATOR"); !env_val.empty()) {
    if (block_allocator_opt_->count == 0) {
      opts_.block_allocator = env_val;
    }
  }

  // Sequential access
  if (auto env_val = get_env_var(prefix + "SEQ_DETECTOR"); !env_val.empty()) {
    if (seq_detector_opt_->count == 0) {
      opts_.seq_detector = env_val;
    }
  }

  // Analysis
  if (auto env_val = get_env_var(prefix + "ANALYSIS_FILE"); !env_val.empty()) {
    if (analysis_file_opt_->count == 0) {
      opts_.analysis_file = env_val;
    }
  }

#ifndef _WIN32
  // User/group override
  if (auto env_val = get_env_var(prefix + "UID"); !env_val.empty()) {
    if (uid_opt_->count == 0) {
      opts_.uid = env_val;
    }
  }

  if (auto env_val = get_env_var(prefix + "GID"); !env_val.empty()) {
    if (gid_opt_->count == 0) {
      opts_.gid = env_val;
    }
  }
#endif

#if DWARFS_PERFMON_ENABLED
  // Performance monitoring
  if (auto env_val = get_env_var(prefix + "PERFMON"); !env_val.empty()) {
    if (perfmon_opt_->count == 0) {
      opts_.perfmon = env_val;
    }
  }

  if (auto env_val = get_env_var(prefix + "PERFMON_TRACE_FILE"); !env_val.empty()) {
    if (perfmon_trace_opt_->count == 0) {
      opts_.perfmon_trace_file = env_val;
    }
  }
#endif

  // Following MECE principle: CLI > ENV > defaults
}

} // namespace dwarfs::tool::dwarfs