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

#include <dwarfs/tool/argtable3_base_parser.h>
#include <dwarfs/tool/dwarfs/parsed_options.h>

// Forward declare argtable3 types
struct arg_lit;
struct arg_str;
struct arg_int;
struct arg_file;

namespace dwarfs::tool::dwarfs {

/**
 * @brief argtable3-based option parser for dwarfs FUSE driver
 *
 * Migrated from FUSE option parsing to argtable3 for:
 * - Unified option handling across all tools
 * - Environment variable support
 * - Cleaner OOP architecture
 * - Better --version and --help support
 *
 * Note: This is the most complex tool with ~60 options including
 * platform-specific FUSE options.
 */
class argtable3_options_parser : public argtable3_base_parser {
public:
  argtable3_options_parser();
  ~argtable3_options_parser() override;

  /**
   * @brief Parse command-line arguments
   * @param argc Argument count
   * @param argv Argument vector
   * @return 0 on success, 1 if help/version shown, 2 on parse error
   */
  int parse(int argc, char** argv) override;

  /**
   * @brief Get parsed options
   * @return Reference to parsed options (valid after successful parse)
   */
  parsed_options const& get_parsed_options() const { return opts_; }

  /**
   * @brief Get mutable parsed options
   * @return Mutable reference to parsed options (valid after successful parse)
   */
  parsed_options& get_parsed_options() { return opts_; }

  /**
   * @brief Load environment variables
   * Loads DWARFS_DWARFS_* environment variables
   */
  void load_environment_variables();

protected:
  /**
   * @brief Define tool-specific options
   * Adds all dwarfs FUSE driver command-line options to argtable
   */
  void define_tool_options() override;

  /**
   * @brief Validate parsed options
   * @return true if validation passed, false otherwise
   */
  bool validate_options() override;

  /**
   * @brief Get tool name for version display
   */
  std::string_view get_tool_name() const override { return "dwarfs"; }

private:
  /**
   * @brief Populate parsed_options from argtable values
   */
  void populate_parsed_options();

  // Parsed options storage
  parsed_options opts_;

  // Positional arguments
  arg_file* image_opt_{nullptr};       // Input filesystem image
  arg_file* mountpoint_opt_{nullptr};  // Mount point directory

  // Cache options
  arg_str* cache_size_opt_{nullptr};
  arg_str* block_size_opt_{nullptr};
  arg_str* readahead_opt_{nullptr};
  arg_int* num_workers_opt_{nullptr};

  // Memory locking
  arg_str* mlock_mode_opt_{nullptr};

  // Decompression
  arg_str* decompress_ratio_opt_{nullptr};

  // Filesystem image options
  arg_str* image_offset_opt_{nullptr};
  arg_str* image_size_opt_{nullptr};

  // Filesystem behavior
  arg_lit* readonly_opt_{nullptr};
  arg_lit* case_insensitive_opt_{nullptr};

  // Cache behavior
  arg_lit* cache_files_opt_{nullptr};
  arg_lit* no_cache_files_opt_{nullptr};
#ifdef DWARFS_FUSE_HAS_LSEEK
  arg_lit* cache_sparse_opt_{nullptr};
  arg_lit* no_cache_sparse_opt_{nullptr};
#endif

  // Preloading
  arg_str* preload_category_opt_{nullptr};
  arg_lit* preload_all_opt_{nullptr};

  // Cache tidying
  arg_str* tidy_strategy_opt_{nullptr};
  arg_str* tidy_interval_opt_{nullptr};
  arg_str* tidy_max_age_opt_{nullptr};

  // Block allocator
  arg_str* block_allocator_opt_{nullptr};

  // Sequential access
  arg_str* seq_detector_opt_{nullptr};

  // Analysis
  arg_file* analysis_file_opt_{nullptr};

#ifndef _WIN32
  // User/group override
  arg_str* uid_opt_{nullptr};
  arg_str* gid_opt_{nullptr};
#endif

#if DWARFS_PERFMON_ENABLED
  // Performance monitoring
  arg_str* perfmon_opt_{nullptr};
  arg_file* perfmon_trace_opt_{nullptr};
#endif

  // Special modes
  arg_lit* auto_mountpoint_opt_{nullptr};

  // Obsolete options (for compatibility warnings)
  arg_lit* enable_nlink_opt_{nullptr};
  arg_lit* cache_image_opt_{nullptr};
  arg_lit* no_cache_image_opt_{nullptr};
};

} // namespace dwarfs::tool::dwarfs