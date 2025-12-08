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

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <cstdlib>

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
// FUSE-T uses the same header paths as regular FUSE (it's API-compatible)
#include <fuse.h>
#if DWARFS_FUSE_LOWLEVEL
#if __has_include(<fuse/fuse_lowlevel.h>)
#include <fuse/fuse_lowlevel.h>
#else
#include <fuse_lowlevel.h>
#endif
#endif
#endif

#ifdef _WIN32
#include <windows.h>
// --- windows.h must be included before delayimp.h ---
#include <delayimp.h>

#include <fuse3/winfsp_fuse.h>
#endif

// FUSE-T compatibility: undefine versioned macros right after FUSE headers
#ifdef DWARFS_USE_FUSE_T
#undef fuse_parse_cmdline
// Declare the actual function FUSE-T provides
extern "C" {
int fuse_parse_cmdline(struct fuse_args *args, struct fuse_cmdline_opts *opts);
}
#endif

#include <fmt/format.h>

#include <dwarfs/error.h>
#include <dwarfs/library_dependencies.h>
#include <dwarfs/decompressor_registry.h>
#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/string.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/tool/tool.h>
#include <dwarfs/tool/dwarfs/mount_handler.h>
#include <dwarfs/tool/dwarfs/options_parser.h>
#include <dwarfs/util.h>
#include <dwarfs/version.h>
#include <dwarfs_tool_main.h>
#include <dwarfs_tool_manpage.h>

#ifdef DWARFS_STACKTRACE_ENABLED
#include <dwarfs/stacktrace.h>
#endif

namespace {

#ifdef _WIN32
FARPROC WINAPI delay_hook(unsigned dliNotify, PDelayLoadInfo pdli) {
  switch (dliNotify) {
  case dliFailLoadLib:
    std::cerr << "failed to load " << pdli->szDll << "\n";
    break;

  case dliFailGetProc:
    std::cerr << "failed to load symbol from " << pdli->szDll << "\n";
    break;

  default:
    return NULL;
  }

  ::exit(1);
}
#endif

} // namespace

#ifdef _WIN32
extern "C" const PfnDliHook __pfnDliFailureHook2 = delay_hook;
#endif

namespace dwarfs::tool {

namespace {

using namespace dwarfs_tool;

void usage(std::ostream& os, std::filesystem::path const& progname) {
  auto extra_deps = [](library_dependencies& deps) {
    decompressor_registry::instance().add_library_dependencies(deps);
#if FUSE_USE_VERSION >= 30 && !defined(DWARFS_USE_FUSE_T)
    deps.add_library("libfuse", ::fuse_pkgversion());
#elif defined(DWARFS_USE_FUSE_T)
    deps.add_library("fuse-t", "1.0.49");  // FUSE-T version
#endif
  };

  os << tool::tool_header("dwarfs",
                          fmt::format(", fuse version {}", FUSE_USE_VERSION),
                          extra_deps)
#if !DWARFS_FUSE_LOWLEVEL
     << "USING HIGH-LEVEL FUSE API\n\n"
#endif
     << "Usage: " << progname.filename().string()
     << " <image> <mountpoint> [options]\n"
     << "       " << progname.filename().string()
     << " <image> --auto-mountpoint [options]\n\n"
     << "DWARFS options:\n"
     << "    -o cachesize=SIZE      set size of block cache (512M)\n"
     << "    -o blocksize=SIZE      set file I/O block size (512K)\n"
     << "    -o readahead=SIZE      set readahead size (0)\n"
     << "    -o workers=NUM         number of worker threads (2)\n"
#ifndef _WIN32
     << "    -o uid=NUM             override user ID for file system\n"
     << "    -o gid=NUM             override group ID for file system\n"
#endif
     << "    -o mlock=NAME          mlock mode: (none), try, must\n"
     << "    -o decratio=NUM        ratio for full decompression (0.8)\n"
     << "    -o offset=NUM|auto     filesystem image offset in bytes (0)\n"
     << "    -o imagesize=NUM       filesystem image size in bytes\n"
     << "    -o readonly            show read-only file system\n"
     << "    -o case_insensitive    perform case-insensitive lookups\n"
     << "    -o preload_category=NAME  preload blocks from this category\n"
     << "    -o preload_all         preload all file system blocks\n"
     << "    -o (no_)cache_files    (don't) keep files in kernel cache\n"
#ifdef DWARFS_FUSE_HAS_LSEEK
     << "    -o (no_)cache_sparse   (don't) keep sparse files in kernel cache\n"
#endif
     << "    -o debuglevel=NAME     " << logger::all_level_names() << "\n"
     << "    -o analysis_file=FILE  write accessed files to this file\n"
     << "    -o tidy_strategy=NAME  (none)|time|swap\n"
     << "    -o tidy_interval=TIME  interval for cache tidying (5m)\n"
     << "    -o tidy_max_age=TIME   tidy blocks after this time (10m)\n"
     << "    -o block_allocator=NAME  (malloc)|mmap\n"
     << "    -o seq_detector=NUM    sequential access detector threshold (4)\n"
#if DWARFS_PERFMON_ENABLED
     << "    -o perfmon=name[+...]  enable performance monitor\n"
     << "    -o perfmon_trace=FILE  write performance monitor trace file\n"
#endif
#ifdef DWARFS_BUILTIN_MANPAGE
     << "    --man                  show manual page and exit\n"
#endif
     << "\n";

#if DWARFS_FUSE_LOWLEVEL && FUSE_USE_VERSION >= 30 && !defined(DWARFS_USE_FUSE_T)
  os << "FUSE options:\n";
  fuse_cmdline_help();
#elif defined(DWARFS_USE_FUSE_T)
  os << "FUSE-T options: (see FUSE-T documentation)\n";
#else
  struct fuse_args args = FUSE_ARGS_INIT(0, nullptr);
  fuse_opt_add_arg(&args, "");
  fuse_opt_add_arg(&args, "-ho");
  struct fuse_operations fsops{};
  fuse_main(args.argc, args.argv, &fsops, nullptr);
  fuse_opt_free_args(&args);
#endif
}

int handle_auto_mountpoint(parsed_options& opts, struct fuse_args& args,
                           iolayer const& iol,
                           std::filesystem::path const& progname) {
  if (opts.seen_mountpoint) {
    iol.err << "error: cannot combine <mountpoint> with --auto-mountpoint\n";
    usage(iol.out, progname);
    return 1;
  }
  if (!opts.fsimage) {
    usage(iol.out, progname);
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

  // for Windows, check the mount point name doesn't exist and let WinFSP
  // create it. other platforms, create or select an existing empty mount
  // directory.
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
  }
#endif

  auto const mp_arg = path_to_utf8_string_sanitized(mountpath);
  fuse_opt_add_arg(&args, mp_arg.c_str());
  opts.seen_mountpoint = 1;

  return 0;
}

} // namespace

int dwarfs_main(int argc, sys_char** argv, iolayer const& iol) {
#ifdef _WIN32
  std::vector<std::string> argv_strings;
  std::vector<char*> argv_copy;
  argv_strings.reserve(argc);
  argv_copy.reserve(argc);

  for (int i = 0; i < argc; ++i) {
    argv_strings.push_back(sys_string_to_string(argv[i]));
    argv_copy.push_back(argv_strings.back().data());
  }

  struct fuse_args args = FUSE_ARGS_INIT(argc, argv_copy.data());
#else
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
#endif

  std::filesystem::path progname(argv[0]);
  parsed_options opts;

  // Parse options using options_parser
  options_parser parser;
  if (parser.parse(argc, argv, iol, opts, args, progname)) {
    return opts.is_help ? 0 : 1;
  }

#ifdef DWARFS_BUILTIN_MANPAGE
  if (opts.is_man) {
    tool::show_manpage(tool::manpage::get_dwarfs_manpage(), iol);
    return 0;
  }
#endif

  // Handle auto-mountpoint if requested
  if (opts.is_auto_mountpoint) {
    if (handle_auto_mountpoint(opts, args, iol, progname)) {
      return 1;
    }
  }

  // Ensure mountpoint was provided
  if (!opts.seen_mountpoint) {
    usage(iol.out, progname);
    return 1;
  }

  // Add platform-specific FUSE options
#ifndef _WIN32
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

  // Parse FUSE command-line options
#if DWARFS_FUSE_LOWLEVEL
#if FUSE_USE_VERSION >= 30
  struct fuse_cmdline_opts fuse_opts;

  if (fuse_parse_cmdline(&args, &fuse_opts) == -1 || !fuse_opts.mountpoint) {
    usage(iol.out, progname);
    return 1;
  }

#ifdef DWARFS_STACKTRACE_ENABLED
  if (fuse_opts.foreground) {
    install_signal_handlers();
  }
#endif

  // Free mountpoint as mount_handler will handle it
  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
  ::free(fuse_opts.mountpoint);
#else
  char* mp_unsafe = nullptr;
  int mt, fg;

  if (fuse_parse_cmdline(&args, &mp_unsafe, &mt, &fg) == -1 || !mp_unsafe) {
    usage(iol.out, progname);
    return 1;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
  ::free(mp_unsafe);

#ifdef DWARFS_STACKTRACE_ENABLED
  if (fg) {
    install_signal_handlers();
  }
#endif
#endif
#endif

  // Create and run mount handler
  try {
    mount_handler handler(opts, args, iol, progname);
    return handler.run();
  } catch (std::exception const& e) {
    iol.err << "error: " << exception_str(e) << "\n";
    return 1;
  }
}

} // namespace dwarfs::tool