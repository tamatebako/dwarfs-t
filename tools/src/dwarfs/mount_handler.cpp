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

#include <dwarfs/tool/dwarfs/mount_handler.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_set>

#include <dwarfs/config.h>

// Default to low-level API (1) unless overridden by CMake
// For FUSE-T, CMake sets DWARFS_FUSE_LOWLEVEL=0 to use high-level API
#ifndef DWARFS_FUSE_LOWLEVEL
#define DWARFS_FUSE_LOWLEVEL 0
#endif

// Check for FUSE-T first, as it has hybrid API
#ifdef DWARFS_USE_FUSE_T
#if DWARFS_FUSE_LOWLEVEL
#include <fuse/fuse_lowlevel.h>
#else
#include <fuse/fuse.h>
#endif
#elif FUSE_USE_VERSION >= 30
#if DWARFS_FUSE_LOWLEVEL
#include <fuse3/fuse_lowlevel.h>
#else
#include <fuse3/fuse.h>
#endif
#else
#include <fuse.h>
#if DWARFS_FUSE_LOWLEVEL
#if __has_include(<fuse/fuse_lowlevel.h>)
#include <fuse/fuse_lowlevel.h>
#else
#include <fuse_lowlevel.h>
#endif
#endif
#endif

#include <dwarfs/error.h>
#include <dwarfs/logger.h>
#include <dwarfs/os_access_generic.h>
#include <dwarfs/performance_monitor.h>
#include <dwarfs/reader/filesystem_loader.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/fuse_driver.h>
#include <dwarfs/reader/mlock_mode.h>
#include <dwarfs/scope_exit.h>
#include <dwarfs/string.h>
#include <dwarfs/tool/dwarfs/parsed_options.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/util.h>

namespace dwarfs::tool::dwarfs_tool {

namespace {

#ifndef _WIN32
/**
 * Auto-cleanup mountpoint helper
 */
class auto_mountpoint_guard {
 public:
  auto_mountpoint_guard() = default;

  void set(std::filesystem::path path) { path_ = std::move(path); }

  ~auto_mountpoint_guard() {
    if (path_.has_value()) {
      std::error_code ec;
      std::filesystem::remove(*path_, ec);
      if (ec) {
        std::cerr << "failed to remove temporary mountpoint '"
                  << path_to_utf8_string_sanitized(*path_)
                  << "': " << ec.message() << "\n";
      }
    }
  }

 private:
  std::optional<std::filesystem::path> path_;
};
#endif

void check_fusermount(logger& lgr [[maybe_unused]]) {
#ifdef __linux__
  // fusermount is Linux-specific
#if FUSE_USE_VERSION >= 30
  static constexpr std::string_view const fusermount_name = "fusermount3";
  static constexpr std::string_view const fuse_pkg = "fuse3";
#else
  static constexpr std::string_view const fusermount_name = "fusermount";
  static constexpr std::string_view const fuse_pkg = "fuse/fuse2";
#endif

  auto fusermount = os_access_generic().find_executable(fusermount_name);

  if (fusermount.empty() || !std::filesystem::exists(fusermount)) {
    LOG_PROXY(prod_logger_policy, lgr);
    LOG_ERROR << "Could not find `" << fusermount_name << "' in PATH";
    LOG_WARN << "Do you need to install the `" << fuse_pkg << "' package?";
  }
#elif defined(__APPLE__)
  // macOS FUSE checks
#ifdef DWARFS_USE_FUSE_T
  // FUSE-T doesn't require a separate mount helper
  LOG_PROXY(prod_logger_policy, lgr);
  LOG_DEBUG << "Using FUSE-T (no separate mount helper required)";
#else
  // macFUSE/osxfuse mount helper check
  LOG_PROXY(prod_logger_policy, lgr);
  auto mount_helper = os_access_generic().find_executable("mount_macfuse");
  if (mount_helper.empty() &&
      !std::filesystem::exists("/Library/Filesystems/macfuse.fs")) {
    LOG_ERROR << "macFUSE not properly installed";
    LOG_WARN << "Please install macFUSE from https://osxfuse.github.io/";
  }
#endif
#endif
}

#if FUSE_USE_VERSION > 30

int run_fuse_session(fuse_args& args,
#if DWARFS_FUSE_LOWLEVEL
                     fuse_cmdline_opts const& fuse_opts,
#endif
                     reader::fuse_driver& driver, logger& lgr) {
#if DWARFS_FUSE_LOWLEVEL
  fuse_lowlevel_ops fsops{};
#else
  fuse_operations fsops{};
#endif

  driver.setup_operations(fsops);

  int err = 1;

#if DWARFS_FUSE_LOWLEVEL
  if (auto session =
          fuse_session_new(&args, &fsops, sizeof(fsops), driver.get_userdata())) {
    if (fuse_set_signal_handlers(session) == 0) {
#ifdef DWARFS_USE_FUSE_T
      // FUSE-T: mountpoint handled via args, no explicit mount API needed
      if (fuse_daemonize(fuse_opts.foreground) == 0) {
        if (fuse_opts.singlethread) {
          err = fuse_session_loop(session);
        } else {
          // FUSE-T provides fuse_session_loop_mt directly (FUSE 3.0 API)
          // Use the macro which will call the correct underlying function
          err = fuse_session_loop_mt(session, fuse_opts.clone_fd);
        }
      }
#else
      // Standard FUSE 3.x
      if (fuse_session_mount(session, fuse_opts.mountpoint) == 0) {
        if (fuse_daemonize(fuse_opts.foreground) == 0) {
          if (fuse_opts.singlethread) {
            err = fuse_session_loop(session);
          } else {
#if FUSE_USE_VERSION < 32
            // FUSE 3.1: use simplified API
            err = fuse_session_loop_mt_31(session, fuse_opts.clone_fd);
#else
            fuse_loop_config config;
            config.clone_fd = fuse_opts.clone_fd;
            config.max_idle_threads = fuse_opts.max_idle_threads;
            err = fuse_session_loop_mt(session, &config);
#endif
          }
        }
        fuse_session_unmount(session);
      } else {
        check_fusermount(lgr);
      }
#endif
      fuse_remove_signal_handlers(session);
    }
    fuse_session_destroy(session);
  }

  // NOLINTNEXTLINE
  ::free(fuse_opts.mountpoint);
#else
  err = fuse_main(args.argc, args.argv, &fsops, driver.get_userdata());

  if (err != 0) {
    check_fusermount(lgr);
  }
#endif

  fuse_opt_free_args(&args);

  return err;
}

#else // FUSE_USE_VERSION <= 30 (but NOT FUSE-T)

int run_fuse_session(fuse_args& args, std::string const& mountpoint, int mt,
                     int fg, reader::fuse_driver& driver, logger& lgr) {
  int err;

#if DWARFS_FUSE_LOWLEVEL
  fuse_lowlevel_ops fsops{};

  driver.setup_operations(fsops);

  err = 1;

  // Traditional FUSE 2.x only
  if (auto ch = fuse_mount(mountpoint.c_str(), &args)) {
    if (auto se = fuse_lowlevel_new(&args, &fsops, sizeof(fsops),
                                    driver.get_userdata())) {
      if (fuse_daemonize(fg) != -1) {
        if (fuse_set_signal_handlers(se) != -1) {
          fuse_session_add_chan(se, ch);
          err = mt ? fuse_session_loop_mt(se) : fuse_session_loop(se);
          fuse_remove_signal_handlers(se);
        }
      }
      fuse_session_destroy(se);
    }
    fuse_unmount(mountpoint.c_str(), ch);
  } else {
    check_fusermount(lgr);
  }
#else
  // High-level API for FUSE 2.x
  fuse_operations fsops{};

  driver.setup_operations(fsops);

  err = fuse_main(args.argc, args.argv, &fsops, driver.get_userdata());

  if (err != 0) {
    check_fusermount(lgr);
  }
#endif

  fuse_opt_free_args(&args);

  return err;
}

#endif

} // namespace

struct mount_handler::impl {
  dwarfs::parsed_options& opts;
  fuse_args& args;
  iolayer const& iol;
  std::filesystem::path const& progname;
  stream_logger lgr;
#ifndef _WIN32
  auto_mountpoint_guard auto_mp;
#endif

  impl(dwarfs::parsed_options& opts_, fuse_args& args_, iolayer const& iol_,
       std::filesystem::path const& progname_)
      : opts{opts_}
      , args{args_}
      , iol{iol_}
      , progname{progname_}
      , lgr{iol_.term, iol_.err} {}

  int run() {
    try {
      LOG_PROXY(debug_logger_policy, lgr);

      // Validate obsolete options
      if (opts.enable_nlink) {
        LOG_WARN << "`enable_nlink` is obsolete and has no effect";
      }

      if (!opts.cache_image.empty()) {
        LOG_WARN << "`cache_image`/`no_cache_image` is obsolete and has no effect";
      }

      // 1. Create performance monitor if enabled
      std::shared_ptr<performance_monitor> perfmon;
#if DWARFS_PERFMON_ENABLED
      if (!opts.perfmon.empty()) {
        std::unordered_set<std::string> perfmon_enabled;
        split_to(opts.perfmon, '+', perfmon_enabled);

        std::optional<std::filesystem::path> perfmon_trace_file;
        if (!opts.perfmon_trace_file.empty()) {
          perfmon_trace_file = iol.os->canonical(std::filesystem::path(opts.perfmon_trace_file));
        }

        perfmon = performance_monitor::create(perfmon_enabled, iol.file,
                                               perfmon_trace_file);
      }
#endif

      // 2. Load filesystem using library
      reader::filesystem_load_config load_config;
      load_config.image_path = iol.os->canonical(std::filesystem::path(opts.image));
      load_config.cache_size = parse_size_with_unit(opts.cache_size);
      load_config.block_size = parse_size_with_unit(opts.block_size);
      load_config.readahead = parse_size_with_unit(opts.readahead);
      load_config.num_workers = opts.num_workers;
      load_config.lock_mode = reader::parse_mlock_mode(opts.mlock_mode);
      load_config.decompress_ratio = to<double>(opts.decompress_ratio);
      load_config.seq_detector_threshold = to<size_t>(opts.seq_detector);
      load_config.block_allocator = (opts.block_allocator == "mmap")
          ? reader::block_cache_allocation_mode::MMAP
          : reader::block_cache_allocation_mode::MALLOC;
      load_config.readonly = opts.readonly;
      load_config.case_insensitive = opts.case_insensitive;
#ifdef DWARFS_FUSE_HAS_LSEEK
      load_config.enable_sparse_files = true;
#else
      load_config.enable_sparse_files = false;
#endif

#ifndef _WIN32
      if (!opts.uid.empty()) {
        load_config.fs_uid = to<file_stat::uid_type>(opts.uid);
      }
      if (!opts.gid.empty()) {
        load_config.fs_gid = to<file_stat::gid_type>(opts.gid);
      }
#endif

#ifdef FUSE_ROOT_ID
      load_config.inode_offset = FUSE_ROOT_ID;
#else
      load_config.inode_offset = 0;
#endif

      if (!opts.image_offset.empty()) {
        load_config.image_offset = reader::parse_image_offset(opts.image_offset);
      }

      if (!opts.image_size.empty()) {
        load_config.image_size = to<file_off_t>(opts.image_size);
      }

#if DWARFS_PERFMON_ENABLED
      if (!opts.perfmon.empty()) {
        split_to(opts.perfmon, '+', load_config.perfmon_enabled);
      }
      if (!opts.perfmon_trace_file.empty()) {
        load_config.perfmon_trace_file = iol.os->canonical(
            std::filesystem::path(opts.perfmon_trace_file));
      }
#endif

      LOG_DEBUG << "attempting to load filesystem from "
                << load_config.image_path;

      auto fs = reader::filesystem_loader::load(lgr, const_cast<os_access&>(*iol.os), load_config,
                                                 perfmon);

      LOG_INFO << "file system initialized";

      // 3. Setup FUSE driver using library
      reader::fuse_driver_config fuse_config;
      fuse_config.num_workers = opts.num_workers;
      fuse_config.cache_files = opts.cache_files;
#ifdef DWARFS_FUSE_HAS_LSEEK
      fuse_config.cache_sparse = opts.cache_sparse;
#endif
      fuse_config.log_threshold = logger::parse_level("info"); // Default level

      // Cache tidying
      if (opts.tidy_strategy == "time") {
        fuse_config.tidy.strategy = reader::cache_tidy_strategy::EXPIRY_TIME;
      } else if (opts.tidy_strategy == "swap") {
        fuse_config.tidy.strategy = reader::cache_tidy_strategy::BLOCK_SWAPPED_OUT;
      } else {
        fuse_config.tidy.strategy = reader::cache_tidy_strategy::NONE;
      }
      fuse_config.tidy.interval = parse_time_with_unit(opts.tidy_interval);
      fuse_config.tidy.expiry_time = parse_time_with_unit(opts.tidy_max_age);

      // Preloading
      if (!opts.preload_category.empty()) {
        fuse_config.preload_category = opts.preload_category;
      }
      fuse_config.preload_all = opts.preload_all;

      // Analysis
      if (!opts.analysis_file.empty()) {
        fuse_config.analysis_file = iol.os->canonical(std::filesystem::path(opts.analysis_file));
      }

      fuse_config.perfmon = perfmon;

      reader::fuse_driver driver(fs, fuse_config, lgr, const_cast<os_access&>(*iol.os));

      // 4. Setup performance monitor summary on exit
      scope_exit perfmon_summary{[&] {
        if (perfmon) {
          perfmon->summarize(iol.err);
        }
      }};

      // 5. Run FUSE session
#if FUSE_USE_VERSION >= 30
#if DWARFS_FUSE_LOWLEVEL
      fuse_cmdline_opts fuse_opts;
      if (fuse_parse_cmdline(&args, &fuse_opts) == -1 ||
          !fuse_opts.mountpoint) {
        return 1;
      }
      return run_fuse_session(args, fuse_opts, driver, lgr);
#else
      return run_fuse_session(args, driver, lgr);
#endif
#else
      // FUSE 2.x only (not FUSE-T)
      char* mp_unsafe = nullptr;
      int mt, fg;
      if (fuse_parse_cmdline(&args, &mp_unsafe, &mt, &fg) == -1 ||
          !mp_unsafe) {
        return 1;
      }
      std::string mountpoint{mp_unsafe};
      // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
      ::free(mp_unsafe);
      return run_fuse_session(args, mountpoint, mt, fg, driver, lgr);
#endif

    } catch (std::exception const& e) {
      LOG_PROXY(prod_logger_policy, lgr);
      LOG_ERROR << "error: " << exception_str(e);
      return 1;
    }
  }
};

mount_handler::mount_handler(dwarfs::parsed_options& opts, fuse_args& args,
                             iolayer const& iol,
                             std::filesystem::path const& progname)
    : impl_{std::make_unique<impl>(opts, args, iol, progname)} {}

mount_handler::~mount_handler() = default;

int mount_handler::run() { return impl_->run(); }

} // namespace dwarfs::tool::dwarfs_tool