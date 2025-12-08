// ... existing code ...
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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <dwarfs/metadata/serialization/serialization_format.h>
#include <dwarfs/tool/sys_char.h>
#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>

namespace dwarfs::tool {

class iolayer;

namespace mkdwarfs {

/**
 * Parsed command-line options for mkdwarfs
 */
struct parsed_options {
  // Input/output paths
  std::filesystem::path input_path;
  std::optional<std::vector<std::filesystem::path>> input_list;
  std::filesystem::path output_path;
  std::optional<std::filesystem::path> header_path;

  // Operation mode
  bool is_recompress{false};
  bool rebuild_metadata{false};
  bool change_block_size{false};
  bool remove_header{false};
  std::string recompress_opts;
  std::string recompress_categories;

  // Writer options
  writer::scanner_options scanner_opts;
  writer::segmenter_factory::config segmenter_config;

  // Compression settings
  unsigned level{7};
  std::string schema_compression;
  std::string metadata_compression;
  std::string history_compression;
  std::vector<std::string> compression;
  std::vector<std::string> order;

  // Segmenter settings
  std::vector<std::string> max_lookback_blocks;
  std::vector<std::string> window_size;
  std::vector<std::string> window_step;
  std::vector<std::string> bloom_filter_size;

  // Categorization
  std::string categorizer_list;
  bool categorizer_explicit{false};
  std::string max_similarity_size;

  // Threading and resources
  size_t num_workers{0};
  size_t num_scanner_workers{0};
  size_t num_segmenter_workers{0};
  int compress_niceness{5};
  std::string memory_limit{"auto"};

  // File hashing
  std::string file_hash_algo;

  // Progress display
  std::string progress_mode{"unicode"};
  bool no_progress{false};
  std::string debug_filter;

  // Filters
  std::vector<sys_string> filter_rules;

  // Filesystem options
  bool force_overwrite{false};
  bool no_section_index{false};
  bool no_history{false};
  bool no_history_timestamps{false};
  bool no_history_command_line{false};
  bool no_sparse_files{false};

  // Metadata options
  std::optional<uint16_t> set_owner;
  std::optional<uint16_t> set_group;
  std::string chmod_str;
  std::string timestamp;
  std::string time_resolution;
  std::string pack_metadata{"auto"};
  std::string metadata_format{"flatbuffers"};

  // Parsed metadata format
  metadata::serialization::SerializationFormat metadata_format_enum;

  // Command line for history
  std::vector<std::string> command_line;
};

/**
 * Options parser for mkdwarfs
 *
 * Handles parsing and validation of all command-line options.
 * This class is stateless and can be reused for multiple parses.
 */
class options_parser {
 public:
  options_parser() = default;

  /**
   * Parse command-line arguments
   *
   * @param argc Argument count
   * @param argv Argument vector
   * @param iol I/O layer for file access and output
   * @return Parsed options or error code (0 = success, 1 = help shown/error, 2 = validation error)
   */
  int parse(int argc, sys_char** argv, iolayer const& iol, parsed_options& opts);

 private:
  // Helper methods for validation
  static void validate_level(parsed_options const& opts);
  static void validate_block_size(parsed_options const& opts);
  static void validate_paths(parsed_options const& opts, iolayer const& iol);
  static void validate_recompress_requirements(parsed_options const& opts);
};

} // namespace mkdwarfs
} // namespace dwarfs::tool
// ... existing code ...