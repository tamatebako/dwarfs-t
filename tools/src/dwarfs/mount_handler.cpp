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

#ifndef DWARFS_FUSE_LOWLEVEL
#define DWARFS_FUSE_LOWLEVEL 1
#endif

#if FUSE_USE_VERSION >= 30
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

// FUSE-T compatibility: undefine versioned macros
#ifdef DWARFS_USE_FUSE_T
#undef fuse_session_new
#undef fuse_session_loop_mt
#undef fuse_parse_cmdline
// Declare the actual functions FUSE-T provides
extern "C" {
struct fuse_session* fuse_session_new(struct fuse_args *args,
                                      const struct fuse_lowlevel_ops *op,
                                      size_t op_size, void *userdata);
int fuse_session_loop_mt(struct fuse_session *se, int clone_fd);
int fuse_parse_cmdline(struct fuse_args *args, struct fuse_cmdline_opts *opts);
}
#endif

#include <dwarfs/error.h>
#include <dwarfs/logger.h>
#include <dwarfs/os_access_generic.h>
#include <dwarfs/performance_monitor.h>
#include <dwarfs/reader/filesystem_loader.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/reader/fuse_driver.h>
#include <dwarfs/scope_exit.h>
#include <dwarfs/string.h>
#include <dwarfs/tool/dwarfs/options_parser.h>
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
  fuse_lowlevel_ops fsops{};

  driver.setup_operations(fsops);

  int err = 1;

  // Traditional FUSE 2.x only
  if (auto ch = fuse_mount(mountpoint.c_str(), &args)) {
    if (auto se = fuse_lowlevel_new(&args, &fsops, sizeof(fsops),
                                    driver.get_userdata())) {
      if (fuse_daemonize(fg) != -1) {
        if (fuse_set_signal_handlers(se) != -1) {
          fuse_session_add_chan(se, ch);
          err = mt ? fuse_session_loop_mt(se) : fuse_session_loop(se);
          fuse_remove_signal_handlers(se);
          fuse_session_remove_chan(ch);
        }
      }
      fuse_session_destroy(se);
    }
    fuse_unmount(mountpoint.c_str(), ch);
  } else {
    check_fusermount(lgr);
  }

  fuse_opt_free_args(&args);

  return err;
}

#endif

} // namespace

struct mount_handler::impl {
  parsed_options& opts;
  fuse_args& args;
  iolayer const& iol;
  std::filesystem::path const& progname;
  stream_logger lgr;
#ifndef _WIN32
  auto_mountpoint_guard auto_mp;
#endif

  impl(parsed_options& opts_, fuse_args& args_, iolayer const& iol_,
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

      if (opts.cache_image == 1) {
        LOG_WARN << "`cache_image` is obsolete and has no effect";
      }

      if (opts.cache_image == 2) {
        LOG_WARN << "`no_cache_image` is obsolete and has no effect";
      }

      // 1. Create performance monitor if enabled
      std::shared_ptr<performance_monitor> perfmon;
#if DWARFS_PERFMON_ENABLED
      if (opts.perfmon_enabled_str) {
        std::unordered_set<std::string> perfmon_enabled;
        split_to(opts.perfmon_enabled_str, '+', perfmon_enabled);

        std::optional<std::filesystem::path> perfmon_trace_file;
        if (opts.perfmon_trace_file_str) {
          perfmon_trace_file = iol.os->canonical(std::filesystem::path(
              reinterpret_cast<char8_t const*>(opts.perfmon_trace_file_str)));
        }

        perfmon = performance_monitor::create(perfmon_enabled, iol.file,
                                               perfmon_trace_file);
      }
#endif

      // 2. Load filesystem using library
      reader::filesystem_load_config load_config;
      load_config.image_path = iol.os->canonical(std::filesystem::path(
          reinterpret_cast<char8_t const*>(opts.fsimage->data())));
      load_config.cache_size = opts.cachesize;
      load_config.block_size = opts.blocksize;
      load_config.readahead = opts.readahead;
      load_config.num_workers = opts.workers;
      load_config.lock_mode = opts.lock_mode;
      load_config.decompress_ratio = opts.decompress_ratio;
      load_config.seq_detector_threshold = opts.seq_detector_threshold;
      load_config.block_allocator = opts.block_allocator;
      load_config.readonly = bool(opts.readonly);
      load_config.case_insensitive = bool(opts.case_insensitive);
#ifdef DWARFS_FUSE_HAS_LSEEK
      load_config.enable_sparse_files = true;
#else
      load_config.enable_sparse_files = false;
#endif

#ifndef _WIN32
      load_config.fs_uid = opts.fs_uid;
      load_config.fs_gid = opts.fs_gid;
#endif

#ifdef FUSE_ROOT_ID
      load_config.inode_offset = FUSE_ROOT_ID;
#else
      load_config.inode_offset = 0;
#endif

      if (opts.image_offset_str) {
        load_config.image_offset =
            reader::parse_image_offset(opts.image_offset_str);
      }

      if (opts.image_size_str) {
        load_config.image_size = to<file_off_t>(opts.image_size_str);
      }

#if DWARFS_PERFMON_ENABLED
      if (opts.perfmon_enabled_str) {
        split_to(opts.perfmon_enabled_str, '+', load_config.perfmon_enabled);
      }
      if (opts.perfmon_trace_file_str) {
        load_config.perfmon_trace_file = iol.os->canonical(
            std::filesystem::path(reinterpret_cast<char8_t const*>(
                opts.perfmon_trace_file_str)));
      }
#endif

      LOG_DEBUG << "attempting to load filesystem from "
                << load_config.image_path;

      auto fs = reader::filesystem_loader::load(lgr, const_cast<os_access&>(*iol.os), load_config,
                                                 perfmon);

      LOG_INFO << "file system initialized";

      // 3. Setup FUSE driver using library
      reader::fuse_driver_config fuse_config;
      fuse_config.num_workers = opts.workers;
      fuse_config.cache_files = bool(opts.cache_files);
#ifdef DWARFS_FUSE_HAS_LSEEK
      fuse_config.cache_sparse = bool(opts.cache_sparse);
#endif
      fuse_config.log_threshold = opts.logopts.threshold;

      // Cache tidying
      fuse_config.tidy.strategy = opts.block_cache_tidy_strategy;
      fuse_config.tidy.interval = opts.block_cache_tidy_interval;
      fuse_config.tidy.expiry_time = opts.block_cache_tidy_max_age;

      // Preloading
      if (opts.preload_category_str) {
        fuse_config.preload_category = opts.preload_category_str;
      }
      fuse_config.preload_all = bool(opts.preload_all);

      // Analysis
      if (opts.analysis_file_str) {
        fuse_config.analysis_file = iol.os->canonical(std::filesystem::path(
            reinterpret_cast<char8_t const*>(opts.analysis_file_str)));
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

mount_handler::mount_handler(parsed_options& opts, fuse_args& args,
                             iolayer const& iol,
                             std::filesystem::path const& progname)
    : impl_{std::make_unique<impl>(opts, args, iol, progname)} {}

mount_handler::~mount_handler() = default;

int mount_handler::run() { return impl_->run(); }

} // namespace dwarfs::tool::dwarfs_tool