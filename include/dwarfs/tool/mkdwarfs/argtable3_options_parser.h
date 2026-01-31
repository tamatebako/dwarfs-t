/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <dwarfs/tool/argtable3_base_parser.h>
#include <dwarfs/tool/mkdwarfs/parsed_options.h>

// Forward declare argtable3 types
struct arg_lit;
struct arg_str;
struct arg_int;
struct arg_file;
struct arg_rex;

namespace dwarfs::tool {

class iolayer;

namespace mkdwarfs {

/**
 * @brief argtable3-based option parser for mkdwarfs
 *
 * Migrated from boost::program_options to argtable3 for:
 * - Unified option handling across all tools
 * - Environment variable support
 * - Cleaner OOP architecture
 */
class argtable3_options_parser : public argtable3_base_parser {
public:
  using argtable3_base_parser::parse;  // Bring base class parse() overloads into scope

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
   * Loads DWARFS_MKDWARFS_* environment variables
   */
  void load_environment_variables();

protected:
  /**
   * @brief Define tool-specific options
   * Adds all mkdwarfs command-line options to argtable
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
  std::string_view get_tool_name() const override { return "mkdwarfs"; }

private:
  /**
   * @brief Populate parsed_options from argtable values
   */
  void populate_parsed_options();

  /**
   * @brief Apply compression level defaults
   */
  void apply_level_defaults();

  /**
   * @brief Process additional options that need defaults
   */
  void process_additional_options();

  /**
   * @brief Validate compression level (0-9)
   */
  void validate_level();

  /**
   * @brief Validate block size bits
   */
  void validate_block_size();

  /**
   * @brief Validate input/output paths
   */
  void validate_paths(iolayer const& iol);

  /**
   * @brief Validate recompress requirements
   */
  void validate_recompress_requirements();

  // Parsed options storage
  parsed_options opts_;

  // Input/Output options (5)
  arg_file* input_opt_{nullptr};
  arg_file* input_list_opt_{nullptr};
  arg_file* output_opt_{nullptr};
  arg_file* header_opt_{nullptr};
  arg_lit* remove_header_opt_{nullptr};

  // Basic options (3)
  arg_lit* force_opt_{nullptr};
  arg_int* level_opt_{nullptr};
  arg_lit* long_help_opt_{nullptr};

  // Compression options (5)
  arg_str* compression_opt_{nullptr};
  arg_str* schema_compression_opt_{nullptr};
  arg_str* metadata_compression_opt_{nullptr};
  arg_str* history_compression_opt_{nullptr};
  arg_int* block_size_bits_opt_{nullptr};

  // Segmenter options (4)
  arg_str* max_lookback_blocks_opt_{nullptr};
  arg_str* window_size_opt_{nullptr};
  arg_str* window_step_opt_{nullptr};
  arg_str* bloom_filter_size_opt_{nullptr};

  // Threading options (5)
  arg_int* num_workers_opt_{nullptr};
  arg_int* num_scanner_workers_opt_{nullptr};
  arg_int* num_segmenter_workers_opt_{nullptr};
  arg_int* compress_niceness_opt_{nullptr};
  arg_str* memory_limit_opt_{nullptr};

  // Categorization options (3)
  arg_str* categorize_opt_{nullptr};
  arg_str* max_similarity_size_opt_{nullptr};
  arg_str* order_opt_{nullptr};

  // File hash option (1)
  arg_str* file_hash_opt_{nullptr};

  // Progress options (3)
  arg_str* progress_opt_{nullptr};
  arg_lit* no_progress_opt_{nullptr};
  arg_str* debug_filter_opt_{nullptr};

  // Recompress options (4)
  arg_str* recompress_opt_{nullptr};
  arg_lit* rebuild_metadata_opt_{nullptr};
  arg_lit* change_block_size_opt_{nullptr};
  arg_str* recompress_categories_opt_{nullptr};

  // Filesystem options (7)
  arg_lit* with_devices_opt_{nullptr};
  arg_lit* with_specials_opt_{nullptr};
  arg_lit* no_sparse_files_opt_{nullptr};
  arg_lit* no_section_index_opt_{nullptr};
  arg_lit* no_history_opt_{nullptr};
  arg_lit* no_history_timestamps_opt_{nullptr};
  arg_lit* no_history_command_line_opt_{nullptr};

  // Metadata options (13)
  arg_int* set_owner_opt_{nullptr};
  arg_int* set_group_opt_{nullptr};
  arg_str* chmod_opt_{nullptr};
  arg_lit* no_create_timestamp_opt_{nullptr};
  arg_str* set_time_opt_{nullptr};
  arg_lit* keep_all_times_opt_{nullptr};
  arg_str* time_resolution_opt_{nullptr};
  arg_lit* no_category_names_opt_{nullptr};
  arg_lit* no_category_metadata_opt_{nullptr};
  arg_lit* no_hardlink_table_opt_{nullptr};
  arg_lit* no_metadata_version_history_opt_{nullptr};
  arg_str* pack_metadata_opt_{nullptr};
  arg_str* format_opt_{nullptr};

  // Filter options (2)
  arg_str* filter_opt_{nullptr};
  arg_lit* remove_empty_dirs_opt_{nullptr};
};

} // namespace mkdwarfs
} // namespace dwarfs::tool