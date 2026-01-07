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
#include <memory>

// Forward declarations for FUSE types
struct fuse_args;

namespace dwarfs::tool {

class iolayer;

namespace dwarfs {
struct parsed_options;
}

namespace dwarfs_tool {

/**
 * Handler for mounting DwarFS filesystems via FUSE
 *
 * This handler ties together the filesystem loader and FUSE driver
 * to provide a complete mounting workflow. It handles all platform-specific
 * FUSE session management and signal handling.
 *
 * Example usage:
 * @code
 *   dwarfs::parsed_options opts;
 *   fuse_args args = FUSE_ARGS_INIT(argc, argv);
 *   argtable3_options_parser parser;
 *   parser.parse(argc, argv);
 *
 *   mount_handler handler(opts, args, iol, progname);
 *   return handler.run();
 * @endcode
 */
class mount_handler {
 public:
  /**
   * Construct mount handler
   *
   * @param opts Parsed command-line options
   * @param args FUSE arguments (after option parsing)
   * @param iol I/O layer for logging and file access
   * @param progname Program name for error messages
   */
  mount_handler(dwarfs::parsed_options& opts, fuse_args& args, iolayer const& iol,
                std::filesystem::path const& progname);

  /**
   * Destructor
   */
  ~mount_handler();

  // Non-copyable, non-movable
  mount_handler(mount_handler const&) = delete;
  mount_handler& operator=(mount_handler const&) = delete;
  mount_handler(mount_handler&&) = delete;
  mount_handler& operator=(mount_handler&&) = delete;

  /**
   * Execute the mount operation
   *
   * This method:
   * 1. Loads the filesystem using filesystem_loader
   * 2. Sets up the FUSE driver with appropriate configuration
   * 3. Initializes FUSE operations
   * 4. Runs the FUSE session loop
   * 5. Handles cleanup on unmount
   *
   * @return Exit code (0 = success, non-zero = error)
   */
  int run();

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace dwarfs_tool
} // namespace dwarfs::tool