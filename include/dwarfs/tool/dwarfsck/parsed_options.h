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

#include <filesystem>
#include <string>

#include <dwarfs/tool/sys_char.h>

namespace dwarfs::tool::dwarfsck {

/**
 * @brief Parsed command-line options for dwarfsck
 */
struct parsed_options {
  // Input/Output
  sys_string input;                  // Input filesystem image
  sys_string export_metadata;        // Export metadata to JSON file
  
  // Operation modes
  bool quiet{false};                 // Don't print unless error
  bool verbose{false};               // Verbose file listing
  bool output_json{false};           // Output in JSON format
  bool print_header{false};          // Print filesystem header only
  bool list_files{false};            // List all files
  
  // Filesystem options
  std::string image_offset{"auto"};  // Filesystem image offset
  std::string cache_size{"512m"};    // Block cache size
  size_t num_workers{0};             // Number of worker threads (0=auto)
  
  // Check options
  bool check_integrity{false};       // Full integrity check
  bool no_check{false};              // Skip checksum verification
  
  // Detail/Info options
  std::string detail;                // Detail level or feature list
  
  // Checksum options
  std::string checksum_algo;         // Checksum algorithm for file hashing
};

} // namespace dwarfs::tool::dwarfsck