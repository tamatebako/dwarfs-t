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
#include <dwarfs/tool/dwarfsextract/parsed_options.h>

// Forward declare argtable3 types
struct arg_lit;
struct arg_str;
struct arg_int;
struct arg_file;

namespace dwarfs::tool::dwarfsextract {

/**
 * @brief argtable3-based option parser for dwarfsextract
 *
 * Migrated from boost::program_options to argtable3 for:
 * - Unified option handling across all tools
 * - Environment variable support
 * - Cleaner OOP architecture
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
   * Loads DWARFS_DWARFSEXTRACT_* environment variables
   */
  void load_environment_variables();

protected:
  /**
   * @brief Define tool-specific options
   * Adds all dwarfsextract command-line options to argtable
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
  std::string_view get_tool_name() const override { return "dwarfsextract"; }

private:
  /**
   * @brief Populate parsed_options from argtable values
   */
  void populate_parsed_options();

  // Parsed options storage
  parsed_options opts_;

  // Input/Output options
  arg_file* input_opt_{nullptr};
  arg_file* output_opt_{nullptr};
  arg_str* pattern_opt_{nullptr};  // Multiple patterns allowed

  // Filesystem options
  arg_str* image_offset_opt_{nullptr};
  arg_str* cache_size_opt_{nullptr};
  arg_int* num_workers_opt_{nullptr};

  // Archive format options
#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
  arg_str* format_opt_{nullptr};
  arg_str* format_options_opt_{nullptr};
  arg_str* format_filters_opt_{nullptr};
#endif

  // Extraction options
  arg_lit* continue_on_error_opt_{nullptr};
  arg_lit* disable_integrity_check_opt_{nullptr};
  arg_lit* stdout_progress_opt_{nullptr};

  // Benchmark options
  arg_lit* benchmark_mode_opt_{nullptr};
  arg_str* output_json_opt_{nullptr};
  arg_int* repeat_opt_{nullptr};

  // Performance monitoring
#if DWARFS_PERFMON_ENABLED
  arg_str* perfmon_opt_{nullptr};
  arg_file* perfmon_trace_opt_{nullptr};
#endif
};

} // namespace dwarfs::tool::dwarfsextract