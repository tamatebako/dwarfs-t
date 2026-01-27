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
// FUSE2 uses the standard fuse.h
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
#include <dwarfs/tool/dwarfs/argtable3_options_parser.h>
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
     << "    --cachesize=SIZE       set size of block cache (512M)\n"
     << "    --blocksize=SIZE       set file I/O block size (512K)\n"
     << "    --readahead=SIZE       set readahead size (0)\n"
     << "    --workers=NUM          number of worker threads (2)\n"
#ifndef _WIN32
     << "    --uid=NUM              override user ID for file system\n"
     << "    --gid=NUM              override group ID for file system\n"
#endif
     << "    --mlock=NAME           mlock mode: (none), try, must\n"
     << "    --decratio=NUM         ratio for full decompression (0.8)\n"
     << "    -O, --offset=NUM|auto  filesystem image offset in bytes (0)\n"
     << "    --imagesize=NUM        filesystem image size in bytes\n"
     << "    --readonly             show read-only file system\n"
     << "    --case-insensitive     perform case-insensitive lookups\n"
     << "    --preload-category=NAME  preload blocks from this category\n"
     << "    --preload-all          preload all file system blocks\n"
     << "    --cache-files          keep files in kernel cache (default)\n"
     << "    --no-cache-files       don't keep files in kernel cache\n"
#ifdef DWARFS_FUSE_HAS_LSEEK
     << "    --cache-sparse         keep sparse files in kernel cache\n"
     << "    --no-cache-sparse      don't keep sparse files in kernel cache\n"
#endif
     << "    --tidy-strategy=NAME   (none)|time|swap\n"
     << "    --tidy-interval=TIME   interval for cache tidying (5m)\n"
     << "    --tidy-max-age=TIME    tidy blocks after this time (10m)\n"
     << "    --block-allocator=NAME (malloc)|mmap\n"
     << "    --seq-detector=NUM     sequential access detector threshold (4)\n"
#if DWARFS_PERFMON_ENABLED
     << "    --perfmon=name[+...]   enable performance monitor\n"
     << "    --perfmon-trace=FILE   write performance monitor trace file\n"
#endif
     << "    --auto-mountpoint      auto-select mountpoint\n"
#ifdef DWARFS_BUILTIN_MANPAGE
     << "    --man                  show manual page and exit\n"
#endif
     << "    --version              show version and exit\n"
     << "    -h, --help             show this help and exit\n"
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

int handle_auto_mountpoint(tool::dwarfs::parsed_options& opts,
                            struct fuse_args& args,
                            iolayer const& iol,
                            std::filesystem::path const& progname) {
  if (!opts.mountpoint.empty()) {
    iol.err << "error: cannot combine <mountpoint> with --auto-mountpoint\n";
    usage(iol.out, progname);
    return 1;
  }
  if (opts.image.empty()) {
    usage(iol.out, progname);
    return 1;
  }
  auto fspath = std::filesystem::path(opts.image);
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
  opts.mountpoint = mountpath.string();

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

  // Parse options using argtable3
  tool::dwarfs::argtable3_options_parser parser;

#ifdef DWARFS_BUILTIN_MANPAGE
  // Wire up manpage for --man flag
  parser.set_manpage_context(manpage::get_dwarfs_manpage(), iol);
#endif

#ifdef _WIN32
  int parse_result = parser.parse(argc, argv_copy.data());
#else
  int parse_result = parser.parse(argc, argv);
#endif

  if (parse_result == 1) {
    // Help/version/man shown
    return 0;
  }

  if (parse_result != 0) {
    // Parse error
    return 1;
  }

  // Load environment variables
  parser.load_environment_variables();

  auto& opts = parser.get_parsed_options();

  // Handle auto-mountpoint if requested
  if (opts.auto_mountpoint) {
    if (handle_auto_mountpoint(opts, args, iol, progname)) {
      return 1;
    }
  }

  // Add platform-specific FUSE options
#ifndef _WIN32
  if (!opts.image.empty()) {
    auto const fsname_opt =
        "-ofsname=" + const_cast<os_access&>(*iol.os).canonical(std::filesystem::path(opts.image)).string();
    fuse_opt_add_arg(&args, fsname_opt.c_str());
#if defined(__linux__) || defined(__FreeBSD__)
    fuse_opt_add_arg(&args, "-osubtype=dwarfs");
#elif defined(__APPLE__)
    fuse_opt_add_arg(&args, "-ofstypename=dwarfs");
#endif
  }
#endif

  // Add DWARFS options to FUSE args
  if (!opts.cache_size.empty() && opts.cache_size != "512m") {
    fuse_opt_add_arg(&args, ("-ocachesize=" + opts.cache_size).c_str());
  }
  if (!opts.block_size.empty() && opts.block_size != "512k") {
    fuse_opt_add_arg(&args, ("-oblocksize=" + opts.block_size).c_str());
  }
  if (!opts.readahead.empty() && opts.readahead != "0") {
    fuse_opt_add_arg(&args, ("-oreadahead=" + opts.readahead).c_str());
  }
  if (opts.num_workers != 2) {
    fuse_opt_add_arg(&args, ("-oworkers=" + std::to_string(opts.num_workers)).c_str());
  }
  if (!opts.mlock_mode.empty() && opts.mlock_mode != "none") {
    fuse_opt_add_arg(&args, ("-omlock=" + opts.mlock_mode).c_str());
  }
  if (!opts.decompress_ratio.empty() && opts.decompress_ratio != "0.8") {
    fuse_opt_add_arg(&args, ("-odecratio=" + opts.decompress_ratio).c_str());
  }
  if (!opts.image_offset.empty() && opts.image_offset != "auto") {
    fuse_opt_add_arg(&args, ("-ooffset=" + opts.image_offset).c_str());
  }
  if (!opts.image_size.empty()) {
    fuse_opt_add_arg(&args, ("-oimagesize=" + opts.image_size).c_str());
  }
  if (opts.readonly) {
    fuse_opt_add_arg(&args, "-oreadonly");
  }
  if (opts.case_insensitive) {
    fuse_opt_add_arg(&args, "-ocase_insensitive");
  }
  if (!opts.cache_files) {
    fuse_opt_add_arg(&args, "-ono_cache_files");
  }
#ifdef DWARFS_FUSE_HAS_LSEEK
  if (opts.cache_sparse) {
    fuse_opt_add_arg(&args, "-ocache_sparse");
  }
#endif
  if (!opts.preload_category.empty()) {
    fuse_opt_add_arg(&args, ("-opreload_category=" + opts.preload_category).c_str());
  }
  if (opts.preload_all) {
    fuse_opt_add_arg(&args, "-opreload_all");
  }
  if (!opts.tidy_strategy.empty() && opts.tidy_strategy != "none") {
    fuse_opt_add_arg(&args, ("-otidy_strategy=" + opts.tidy_strategy).c_str());
  }
  if (!opts.tidy_interval.empty() && opts.tidy_interval != "5m") {
    fuse_opt_add_arg(&args, ("-otidy_interval=" + opts.tidy_interval).c_str());
  }
  if (!opts.tidy_max_age.empty() && opts.tidy_max_age != "10m") {
    fuse_opt_add_arg(&args, ("-otidy_max_age=" + opts.tidy_max_age).c_str());
  }
  if (!opts.block_allocator.empty() && opts.block_allocator != "malloc") {
    fuse_opt_add_arg(&args, ("-oblock_allocator=" + opts.block_allocator).c_str());
  }
  if (!opts.seq_detector.empty() && opts.seq_detector != "4") {
    fuse_opt_add_arg(&args, ("-oseq_detector=" + opts.seq_detector).c_str());
  }
  if (!opts.analysis_file.empty()) {
    fuse_opt_add_arg(&args, ("-oanalysis_file=" + opts.analysis_file).c_str());
  }
#ifndef _WIN32
  if (!opts.uid.empty()) {
    fuse_opt_add_arg(&args, ("-ouid=" + opts.uid).c_str());
  }
  if (!opts.gid.empty()) {
    fuse_opt_add_arg(&args, ("-ogid=" + opts.gid).c_str());
  }
#endif
#if DWARFS_PERFMON_ENABLED
  if (!opts.perfmon.empty()) {
    fuse_opt_add_arg(&args, ("-operfmon=" + opts.perfmon).c_str());
  }
  if (!opts.perfmon_trace_file.empty()) {
    fuse_opt_add_arg(&args, ("-operfmon_trace=" + opts.perfmon_trace_file).c_str());
  }
#endif

  // Add mountpoint as final positional arg
  if (!opts.mountpoint.empty()) {
    fuse_opt_add_arg(&args, opts.mountpoint.c_str());
  }

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
    dwarfs_tool::mount_handler handler(opts, args, iol, progname);
    return handler.run();
  } catch (std::exception const& e) {
    iol.err << "error: " << exception_str(e) << "\n";
    return 1;
  }
}

} // namespace dwarfs::tool