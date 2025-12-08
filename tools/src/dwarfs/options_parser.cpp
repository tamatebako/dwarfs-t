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

#include <dwarfs/tool/dwarfs/options_parser.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include <dwarfs/os_access.h>

#include <fmt/format.h>

#if FUSE_USE_VERSION >= 30
#include <fuse3/fuse_lowlevel.h>
#else
#include <fuse.h>
#include <fuse/fuse_lowlevel.h>
#endif

#include <dwarfs/binary_literals.h>
#include <dwarfs/config.h>
#include <dwarfs/conv.h>
#include <dwarfs/error.h>
#include <dwarfs/logger.h>
#include <dwarfs/sorted_array_map.h>
#include <dwarfs/string.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/util.h>

namespace dwarfs::tool::dwarfs_tool {

namespace {

using namespace std::string_view_literals;
using namespace dwarfs::binary_literals;

constexpr size_t const kDefaultBlockSize{static_cast<size_t>(512) << 10};
constexpr size_t const kDefaultSeqDetectorThreshold{4};

constexpr sorted_array_map cache_tidy_strategy_map{
    std::pair{"none"sv, reader::cache_tidy_strategy::NONE},
    std::pair{"time"sv, reader::cache_tidy_strategy::EXPIRY_TIME},
    std::pair{"swap"sv, reader::cache_tidy_strategy::BLOCK_SWAPPED_OUT},
};

constexpr sorted_array_map block_allocator_map{
    std::pair{"malloc"sv, reader::block_cache_allocation_mode::MALLOC},
    std::pair{"mmap"sv, reader::block_cache_allocation_mode::MMAP},
};

#define DWARFS_OPT(t, p, v)                                                    \
  ::fuse_opt { t, offsetof(parsed_options, p), v }

constexpr std::array dwarfs_opts{
    DWARFS_OPT("cachesize=%s", cachesize_str, 0),
    DWARFS_OPT("blocksize=%s", blocksize_str, 0),
    DWARFS_OPT("readahead=%s", readahead_str, 0),
    DWARFS_OPT("debuglevel=%s", debuglevel_str, 0),
    DWARFS_OPT("workers=%s", workers_str, 0),
#ifndef _WIN32
    DWARFS_OPT("uid=%s", uid_str, 0),
    DWARFS_OPT("gid=%s", gid_str, 0),
#endif
    DWARFS_OPT("mlock=%s", mlock_str, 0),
    DWARFS_OPT("decratio=%s", decompress_ratio_str, 0),
   DWARFS_OPT("offset=%s", image_offset_str, 0),
    DWARFS_OPT("imagesize=%s", image_size_str, 0),
    DWARFS_OPT("tidy_strategy=%s", cache_tidy_strategy_str, 0),
    DWARFS_OPT("tidy_interval=%s", cache_tidy_interval_str, 0),
    DWARFS_OPT("tidy_max_age=%s", cache_tidy_max_age_str, 0),
    DWARFS_OPT("block_allocator=%s", block_alloc_mode_str, 0),
    DWARFS_OPT("seq_detector=%s", seq_detector_thresh_str, 0),
    DWARFS_OPT("analysis_file=%s", analysis_file_str, 0),
    DWARFS_OPT("preload_category=%s", preload_category_str, 0),
    DWARFS_OPT("preload_all", preload_all, 1),
    DWARFS_OPT("enable_nlink", enable_nlink, 1),
    DWARFS_OPT("readonly", readonly, 1),
    DWARFS_OPT("case_insensitive", case_insensitive, 1),
    DWARFS_OPT("cache_image", cache_image, 1),
    DWARFS_OPT("no_cache_image", cache_image, 2),
    DWARFS_OPT("cache_files", cache_files, 1),
    DWARFS_OPT("no_cache_files", cache_files, 0),
#ifdef DWARFS_FUSE_HAS_LSEEK
    DWARFS_OPT("cache_sparse", cache_sparse, 1),
    DWARFS_OPT("no_cache_sparse", cache_sparse, 0),
#endif
#if DWARFS_PERFMON_ENABLED
    DWARFS_OPT("perfmon=%s", perfmon_enabled_str, 0),
    DWARFS_OPT("perfmon_trace=%s", perfmon_trace_file_str, 0),
#endif
    ::fuse_opt(FUSE_OPT_END),
};

// FUSE option handler callback
int option_hdl(void* data, char const* arg, int key,
               struct fuse_args* /*outargs*/) {
  auto& opts = *reinterpret_cast<parsed_options*>(data);
  std::string_view argsv{arg};

  switch (key) {
  case FUSE_OPT_KEY_NONOPT:
    if (opts.seen_mountpoint) {
      return -1;
    }

    if (opts.fsimage) {
      opts.seen_mountpoint = 1;
      return 1;
    }

    opts.fsimage = std::make_shared<std::string>(argsv);
    return 0;

  case FUSE_OPT_KEY_OPT:
    if (argsv == "-h" || argsv == "--help") {
      opts.is_help = true;
      return -1;
    }

    if (argsv == "--auto-mountpoint") {
      opts.is_auto_mountpoint = true;
      return 0;
    }

#ifdef DWARFS_BUILTIN_MANPAGE
    if (argsv == "--man") {
      opts.is_man = true;
      return -1;
    }
#endif
    break;

  default:
    break;
  }

  return 1;
}

// Handle auto-mountpoint option
int handle_auto_mountpoint(parsed_options& opts, struct fuse_args& args,
                           iolayer const& iol) {
  if (opts.seen_mountpoint) {
    iol.err << "error: cannot combine <mountpoint> with --auto-mountpoint\n";
    return 1;
  }
  if (!opts.fsimage) {
    return 1;
  }

  auto fspath = std::filesystem::path(opts.fsimage->data());
  // assume .dwarfs extension, so user gets "fs.dwarfs" -> "fs/"
  auto mountpath = fspath.parent_path() / fspath.stem();

  if (fspath == mountpath) {
    iol.err << "error: cannot select mountpoint directory for file with no "
               "extension\n";
    return 1;
  }

  // for Windows, check the mount point name doesn't exist and let WinFSP create
  // it. other platforms, create or select an existing empty mount directory.
#ifdef _WIN32
  if (std::filesystem::exists(mountpath)) {
    iol.err << "error: mountpoint directory already exists\n";
    return 1;
  }
#else
  if (std::filesystem::exists(mountpath) &&
      (!std::filesystem::is_empty(mountpath) ||
       !std::filesystem::is_directory(mountpath))) {
    iol.err << "error: cannot find a suitable empty mountpoint directory\n";
    return 1;
  }

  if (!std::filesystem::exists(mountpath)) {
    std::error_code ec;
    bool const mp_created = std::filesystem::create_directory(mountpath, ec);
    if (!mp_created && ec) {
      iol.err << "error: unable to create mountpoint directory: "
              << ec.message() << "\n";
      return 1;
    }
    // Note: auto_mountpoint tracking happens in mount_handler
  }
#endif

  auto const mp_arg = path_to_utf8_string_sanitized(mountpath);
  fuse_opt_add_arg(&args, mp_arg.c_str());
  opts.seen_mountpoint = 1;

  return 0;
}

} // anonymous namespace

int options_parser::parse(int argc, sys_char** argv, iolayer const& iol,
                          parsed_options& opts, fuse_args& args,
                          std::filesystem::path& progname) {
  progname = std::filesystem::path(argv[0]);

  // Parse FUSE arguments
  fuse_opt_parse(&args, &opts, dwarfs_opts.data(), option_hdl);

  // Handle auto-mountpoint
  if (opts.is_auto_mountpoint) {
    if (handle_auto_mountpoint(opts, args, iol)) {
      return 1;
    }
  }

#ifndef _WIN32
  // Add fsname option if we have an image
  if (opts.fsimage) {
    auto const fsname_opt =
        "-ofsname=" + const_cast<os_access&>(*iol.os).canonical(*opts.fsimage).string();
    fuse_opt_add_arg(&args, fsname_opt.c_str());
#if defined(__linux__) || defined(__FreeBSD__)
    fuse_opt_add_arg(&args, "-osubtype=dwarfs");
#elif defined(__APPLE__)
    fuse_opt_add_arg(&args, "-ofstypename=dwarfs");
#endif
  }
#endif

  // Parse and validate string options
  try {
    parse_string_options(opts, iol);
    validate_options(opts, iol);
    apply_defaults(opts);
  } catch (std::exception const& e) {
    iol.err << "error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}

void options_parser::parse_string_options(parsed_options& opts,
                                          iolayer const& iol) {
  // Parse debug level
  if (opts.debuglevel_str) {
    opts.logopts.threshold = logger::parse_level(opts.debuglevel_str);
  }

  // Parse cache size
  opts.cachesize =
      opts.cachesize_str ? parse_size_with_unit(opts.cachesize_str) : 512_MiB;

  // Parse block size
  opts.blocksize = opts.blocksize_str
                       ? parse_size_with_unit(opts.blocksize_str)
                       : kDefaultBlockSize;

  // Parse readahead
  opts.readahead =
      opts.readahead_str ? parse_size_with_unit(opts.readahead_str) : 0;

  // Parse worker count
  opts.workers = opts.workers_str ? to<size_t>(opts.workers_str) : 2;

  // Parse mlock mode
  opts.lock_mode = opts.mlock_str ? reader::parse_mlock_mode(opts.mlock_str)
                                  : reader::mlock_mode::NONE;

  // Parse decompression ratio
  opts.decompress_ratio =
      opts.decompress_ratio_str ? to<double>(opts.decompress_ratio_str) : 0.8;

#ifndef _WIN32
  // Parse uid/gid overrides
  if (opts.uid_str) {
    opts.fs_uid = to<file_stat::uid_type>(opts.uid_str);
  }

  if (opts.gid_str) {
    opts.fs_gid = to<file_stat::gid_type>(opts.gid_str);
  }
#endif

  // Parse cache tidy strategy
  if (opts.cache_tidy_strategy_str) {
    if (auto it = cache_tidy_strategy_map.find(opts.cache_tidy_strategy_str);
        it != cache_tidy_strategy_map.end()) {
      opts.block_cache_tidy_strategy = it->second;
    } else {
      throw std::runtime_error(
          fmt::format("no such cache tidy strategy: {}",
                      opts.cache_tidy_strategy_str));
    }

    if (opts.cache_tidy_interval_str) {
      opts.block_cache_tidy_interval =
          parse_time_with_unit(opts.cache_tidy_interval_str);
    }

    if (opts.cache_tidy_max_age_str) {
      opts.block_cache_tidy_max_age =
          parse_time_with_unit(opts.cache_tidy_max_age_str);
    }
  }

  // Parse block allocator
  if (opts.block_alloc_mode_str) {
    if (auto it = block_allocator_map.find(opts.block_alloc_mode_str);
        it != block_allocator_map.end()) {
      opts.block_allocator = it->second;
    } else {
      throw std::runtime_error(
          fmt::format("no such block allocator: {}", opts.block_alloc_mode_str));
    }
  } else {
    opts.block_allocator = reader::block_cache_allocation_mode::MALLOC;
  }

  // Parse sequential detector threshold
  opts.seq_detector_threshold =
      opts.seq_detector_thresh_str
          ? to<size_t>(opts.seq_detector_thresh_str)
          : kDefaultSeqDetectorThreshold;
}

void options_parser::validate_options(parsed_options const& opts,
                                      iolayer const& iol) {
  // Validate decompression ratio
  if (opts.decompress_ratio < 0.0 || opts.decompress_ratio > 1.0) {
    throw std::runtime_error("decratio must be between 0.0 and 1.0");
  }

  // Check mountpoint was provided
  if (!opts.seen_mountpoint) {
    throw std::runtime_error("mountpoint not specified");
  }
}

void options_parser::apply_defaults(parsed_options& opts) {
  // No additional defaults needed - already applied during parsing
}

} // namespace dwarfs::tool::dwarfs_tool