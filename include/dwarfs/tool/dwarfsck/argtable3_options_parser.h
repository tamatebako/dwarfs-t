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
#include <dwarfs/tool/dwarfsck/parsed_options.h>

// Forward declare argtable3 types
struct arg_lit;
struct arg_str;
struct arg_int;
struct arg_file;

namespace dwarfs::tool::dwarfsck {

/**
 * @brief argtable3-based option parser for dwarfsck
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
   * Loads DWARFS_DWARFSCK_* environment variables
   */
  void load_environment_variables();

protected:
  /**
   * @brief Define tool-specific options
   * Adds all dwarfsck command-line options to argtable
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
  std::string_view get_tool_name() const override { return "dwarfsck"; }

private:
  /**
   * @brief Populate parsed_options from argtable values
   */
  void populate_parsed_options();

  // Parsed options storage
  parsed_options opts_;

  // Input/Output options
  arg_file* input_opt_{nullptr};
  arg_file* export_metadata_opt_{nullptr};

  // Operation mode options
  arg_lit* quiet_opt_{nullptr};
  arg_lit* verbose_opt_{nullptr};
  arg_lit* output_json_opt_{nullptr};
  arg_lit* print_header_opt_{nullptr};
  arg_lit* list_files_opt_{nullptr};

  // Filesystem options
  arg_str* image_offset_opt_{nullptr};
  arg_str* cache_size_opt_{nullptr};
  arg_int* num_workers_opt_{nullptr};

  // Check options
  arg_lit* check_integrity_opt_{nullptr};
  arg_lit* no_check_opt_{nullptr};

  // Detail/Info options
  arg_str* detail_opt_{nullptr};

  // Checksum options
  arg_str* checksum_opt_{nullptr};
};

} // namespace dwarfs::tool::dwarfsck