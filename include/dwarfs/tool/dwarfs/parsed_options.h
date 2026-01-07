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
#include <optional>
#include <string>

#include <dwarfs/file_stat.h>
#include <dwarfs/reader/block_cache_options.h>
#include <dwarfs/reader/cache_tidy_config.h>
#include <dwarfs/reader/mlock_mode.h>
#include <dwarfs/tool/sys_char.h>

namespace dwarfs::tool::dwarfs {

/**
 * @brief Parsed command-line options for dwarfs FUSE driver
 */
struct parsed_options {
  // Input/Output
  sys_string image;        // Input filesystem image
  sys_string mountpoint;   // Mount point directory

  // Cache options
  std::string cache_size{"512m"};           // Block cache size
  std::string block_size{"512k"};           // File I/O block size
  std::string readahead{"0"};               // Readahead size
  size_t num_workers{2};                    // Number of worker threads
  
  // Memory locking
  std::string mlock_mode{"none"};           // mlock mode (none/try/must)
  
  // Decompression
  std::string decompress_ratio{"0.8"};      // Ratio for full decompression
  
  // Filesystem image options
  std::string image_offset{"auto"};         // Filesystem image offset
  std::string image_size;                   // Filesystem image size
  
  // Filesystem behavior
  bool readonly{false};                     // Show read-only file system
  bool case_insensitive{false};             // Perform case-insensitive lookups
  
  // Cache behavior
  bool cache_files{true};                   // Keep files in kernel cache
#ifdef DWARFS_FUSE_HAS_LSEEK
  bool cache_sparse{false};                 // Keep sparse files in kernel cache
#endif
  
  // Preloading
  std::string preload_category;             // Preload blocks from this category
  bool preload_all{false};                  // Preload all file system blocks
  
  // Cache tidying
  std::string tidy_strategy{"none"};        // Cache tidy strategy (none/time/swap)
  std::string tidy_interval{"5m"};          // Interval for cache tidying
  std::string tidy_max_age{"10m"};          // Tidy blocks after this time
  
  // Block allocator
  std::string block_allocator{"malloc"};    // Block allocator (malloc/mmap)
  
  // Sequential access
  std::string seq_detector{"4"};            // Sequential access detector threshold
  
  // Analysis
  std::string analysis_file;                // Write accessed files to this file
  
#ifndef _WIN32
  // User/group override
  std::string uid;                          // Override user ID for file system
  std::string gid;                          // Override group ID for file system
#endif
  
#if DWARFS_PERFMON_ENABLED
  // Performance monitoring
  std::string perfmon;                      // Enable performance monitor
  std::string perfmon_trace_file;           // Write performance monitor trace file
#endif
  
  // Special modes
  bool auto_mountpoint{false};              // Auto-select mountpoint
  
  // Obsolete options (kept for compatibility warnings)
  bool enable_nlink{false};                 // Obsolete: enable nlink (no effect)
  std::string cache_image;                  // Obsolete: cache_image (no effect)
};

} // namespace dwarfs::tool::dwarfs