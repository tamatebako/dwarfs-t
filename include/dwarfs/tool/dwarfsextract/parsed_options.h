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

#include <string>
#include <vector>

#include <dwarfs/config.h>
#include <dwarfs/tool/sys_char.h>

namespace dwarfs::tool::dwarfsextract {

/**
 * @brief Parsed command-line options for dwarfsextract
 */
struct parsed_options {
  // Input/Output
  sys_string input;                  // Input filesystem image
  sys_string output;                 // Output file or directory
  std::vector<std::string> patterns; // File patterns to extract

  // Filesystem options
  std::string image_offset{"auto"};  // Filesystem image offset
  std::string cache_size{"512m"};    // Block cache size
  size_t num_workers{4};             // Number of worker threads

  // Archive format options (libarchive)
#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
  std::string format_name;           // Output format name
  std::string format_options;        // Format-specific options
  std::string format_filters;        // Comma-separated filters
#endif

  // Extraction options
  bool continue_on_error{false};     // Continue on errors
  bool disable_integrity_check{false}; // Skip block integrity checks
  bool stdout_progress{false};       // Show progress on stdout

  // Benchmark options
  bool benchmark_mode{false};        // Enable benchmark metrics
  std::string output_json;           // JSON output file for metrics
  size_t repeat_count{1};            // Number of extraction repeats

  // Performance monitoring
#if DWARFS_PERFMON_ENABLED
  std::string perfmon;               // Performance monitor settings
  sys_string perfmon_trace_file;     // Trace output file
#endif
};

} // namespace dwarfs::tool::dwarfsextract