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
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <dwarfs/logger.h>
#include <dwarfs/reader/cache_tidy_config.h>

// Forward declarations for FUSE types
#ifndef DWARFS_FUSE_LOWLEVEL
#define DWARFS_FUSE_LOWLEVEL 1
#endif

#if DWARFS_FUSE_LOWLEVEL
struct fuse_lowlevel_ops;
#else
struct fuse_operations;
#endif

namespace dwarfs {

class os_access;
class performance_monitor;

namespace reader {

class filesystem_v2_lite;

/**
 * Configuration for FUSE driver
 */
struct fuse_driver_config {
  /** Number of worker threads for filesystem operations */
  size_t num_workers{2};

  /** Cache tidying configuration */
  cache_tidy_config tidy;

  /** Category to preload (optional) */
  std::optional<std::string> preload_category;

  /** Preload all blocks */
  bool preload_all{false};

  /** Enable file caching in kernel */
  bool cache_files{true};

  /** Enable sparse file caching in kernel */
  bool cache_sparse{false};

  /** Analysis output file (optional) */
  std::optional<std::filesystem::path> analysis_file;

  /** Performance monitor (optional, shared ownership) */
  std::shared_ptr<performance_monitor> perfmon;

  /** Logging threshold for operation selection */
  logger::level_type log_threshold{LOGGER_LEVEL_WARN};
};

/**
 * FUSE driver for mounting DwarFS filesystems
 *
 * This class encapsulates all FUSE operations and provides a reusable
 * interface for mounting DwarFS images. It supports both lowlevel and
 * high-level FUSE APIs, and handles platform-specific differences.
 *
 * Example usage:
 * @code
 *   reader::filesystem_v2_lite fs = ...;  // Load filesystem
 *   reader::fuse_driver_config config;
 *   config.num_workers = 4;
 *   config.cache_files = true;
 *
 *   reader::fuse_driver driver(fs, config, lgr, os);
 *
 *   // Setup FUSE operations
 *   fuse_lowlevel_ops ops{};
 *   driver.setup_operations(ops);
 *
 *   // Use ops with FUSE session...
 * @endcode
 */
class fuse_driver {
 public:
  /**
   * Construct a FUSE driver
   *
   * @param fs The filesystem to mount
   * @param config Configuration options
   * @param lgr Logger for operations
   * @param os OS access interface
   */
  fuse_driver(filesystem_v2_lite& fs, fuse_driver_config const& config,
              logger& lgr, os_access const& os);

  /**
   * Destructor
   */
  ~fuse_driver();

  // Non-copyable, non-movable
  fuse_driver(fuse_driver const&) = delete;
  fuse_driver& operator=(fuse_driver const&) = delete;
  fuse_driver(fuse_driver&&) = delete;
  fuse_driver& operator=(fuse_driver&&) = delete;

#if DWARFS_FUSE_LOWLEVEL
  /**
   * Setup FUSE lowlevel operations
   *
   * Populates the fuse_lowlevel_ops structure with function pointers
   * to this driver's FUSE operations. The operations will use the
   * debug logger policy if log_threshold >= DEBUG, otherwise the
   * production logger policy.
   *
   * @param ops The FUSE operations structure to populate
   */
  void setup_operations(fuse_lowlevel_ops& ops);
#else
  /**
   * Setup FUSE high-level operations
   *
   * Populates the fuse_operations structure with function pointers
   * to this driver's FUSE operations. The operations will use the
   * debug logger policy if log_threshold >= DEBUG, otherwise the
   * production logger policy.
   *
   * @param ops The FUSE operations structure to populate
   */
  void setup_operations(fuse_operations& ops);
#endif

  /**
   * Get userdata pointer for FUSE
   *
   * Returns a pointer to the internal userdata structure that FUSE
   * will pass to operation callbacks.
   *
   * @return Opaque pointer to userdata
   */
  void* get_userdata();

 private:
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace reader
} // namespace dwarfs