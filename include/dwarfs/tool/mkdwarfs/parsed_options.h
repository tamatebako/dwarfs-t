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

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <dwarfs/metadata/serialization/serialization_format.h>
#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>

namespace dwarfs::tool::mkdwarfs {

/**
 * @brief Parsed command-line options for mkdwarfs
 */
struct parsed_options {
  // Input/Output
  std::filesystem::path input_path;        // Input directory or filesystem
  std::optional<std::vector<std::string>> input_list; // List of input files
  std::filesystem::path output_path;       // Output filesystem file
  std::filesystem::path header_path;       // Header file to prepend
  bool remove_header{false};               // Remove header before recompress

  // Basic options
  bool force_overwrite{false};             // Force overwrite existing output
  unsigned level{7};                       // Compression level (0-9)

  // Compression options
  std::vector<std::string> compression;    // Block compression algorithms
  std::string schema_compression;          // Schema compression algorithm
  std::string metadata_compression;        // Metadata compression algorithm
  std::string history_compression;         // History compression algorithm

  // Segmenter configuration
  writer::segmenter_factory::config segmenter_config;
  std::vector<std::string> max_lookback_blocks;
  std::vector<std::string> window_size;
  std::vector<std::string> window_step;
  std::vector<std::string> bloom_filter_size;

  // Threading options
  size_t num_workers{0};                   // Number of writer worker threads
  size_t num_scanner_workers{0};           // Number of scanner worker threads
  size_t num_segmenter_workers{0};         // Number of segmenter worker threads
  int compress_niceness{5};                // Compression worker niceness
  std::string memory_limit{"auto"};        // Memory limit for block manager

  // Categorization options
  std::string categorizer_list;            // List of categorizers
  bool categorizer_explicit{false};        // Categorizer explicitly set
  std::string max_similarity_size;         // Max file size for similarity
  std::vector<std::string> order;          // Fragment ordering algorithm

  // File hash option
  std::string file_hash_algo{"xxh3-128"};  // File hashing algorithm

  // Progress options
  std::string progress_mode{"unicode"};    // Progress display mode
  bool no_progress{false};                 // Disable progress display
  std::string debug_filter;                // Debug filter mode

  // Recompress options
  std::string recompress_opts;             // Recompress mode
  bool is_recompress{false};               // Is this a recompress operation
  bool rebuild_metadata{false};            // Rebuild metadata
  bool change_block_size{false};           // Change block size
  std::string recompress_categories;       // Categories to recompress

  // Filesystem options
  bool no_sparse_files{false};             // Don't store sparse files as sparse
  bool no_section_index{false};            // Don't add section index
  bool no_history{false};                  // Don't add history
  bool no_history_timestamps{false};       // Don't add timestamps to history
  bool no_history_command_line{false};     // Don't add command line to history

  // Metadata options
  std::optional<uint16_t> set_owner;       // Override owner (uid)
  std::optional<uint16_t> set_group;       // Override group (gid)
  std::string chmod_str;                   // Permission changes
  std::string timestamp;                   // Timestamp for whole filesystem
  std::string time_resolution;             // Timestamp resolution
  std::string pack_metadata{"auto"};       // Metadata packing options
  std::string metadata_format;             // Metadata serialization format
  metadata::serialization::SerializationFormat metadata_format_enum;

  // Filter options
  std::vector<std::string> filter_rules;   // Filter rules

  // Scanner options (embedded structure)
  writer::scanner_options scanner_opts;

  // Command line (for history)
  std::vector<std::string> command_line;
};

} // namespace dwarfs::tool::mkdwarfs