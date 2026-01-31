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

#include <dwarfs/tool/mkdwarfs/argtable3_options_parser.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <argtable3.h>
#include <fmt/format.h>

#include <dwarfs/checksum.h>
#include <dwarfs/compressor_registry.h>
#include <dwarfs/config.h>
#include <dwarfs/conv.h>
#include <dwarfs/error.h>
#include <dwarfs/logger.h>
#include <dwarfs/sorted_array_map.h>
#include <dwarfs/string.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/util.h>
#include <dwarfs/writer/console_writer.h>
#include <dwarfs/writer/filter_debug.h>

namespace dwarfs::tool::mkdwarfs {

namespace {

using namespace std::string_view_literals;

struct level_defaults {
  unsigned block_size_bits;
  std::string_view data_compression;
  std::string_view schema_history_compression;
  std::string_view metadata_compression;
  unsigned window_size;
  unsigned window_step;
  std::string_view order;
};

// Compression algorithm defaults based on available libraries
#if defined(DWARFS_HAVE_LIBLZ4)
#define ALG_DATA_1 "lz4"
#define ALG_DATA_2 "lz4hc:level=9"
#define ALG_DATA_3 "lz4hc:level=9"
#elif defined(DWARFS_HAVE_LIBZSTD)
#define ALG_DATA_1 "zstd:level=1"
#define ALG_DATA_2 "zstd:level=4"
#define ALG_DATA_3 "zstd:level=7"
#elif defined(DWARFS_HAVE_LIBLZMA)
#define ALG_DATA_1 "lzma:level=1"
#define ALG_DATA_2 "lzma:level=2"
#define ALG_DATA_3 "lzma:level=3"
#else
#define ALG_DATA_1 "null"
#define ALG_DATA_2 "null"
#define ALG_DATA_3 "null"
#endif

#if defined(DWARFS_HAVE_LIBZSTD)
#define ALG_DATA_4 "zstd:level=11"
#define ALG_DATA_5 "zstd:level=19"
#define ALG_DATA_6 "zstd:level=22"
#define ALG_DATA_7 "zstd:level=22"
#elif defined(DWARFS_HAVE_LIBLZMA)
#define ALG_DATA_4 "lzma:level=3"
#define ALG_DATA_5 "lzma:level=4"
#define ALG_DATA_6 "lzma:level=5"
#define ALG_DATA_7 "lzma:level=8"
#elif defined(DWARFS_HAVE_LIBLZ4)
#define ALG_DATA_4 "lz4hc:level=9"
#define ALG_DATA_5 "lz4hc:level=9"
#define ALG_DATA_6 "lz4hc:level=9"
#define ALG_DATA_7 "lz4hc:level=9"
#else
#define ALG_DATA_4 "null"
#define ALG_DATA_5 "null"
#define ALG_DATA_6 "null"
#define ALG_DATA_7 "null"
#endif

#if defined(DWARFS_HAVE_LIBLZMA)
#define ALG_DATA_8 "lzma:level=9"
#define ALG_DATA_9 "lzma:level=9"
#elif defined(DWARFS_HAVE_LIBZSTD)
#define ALG_DATA_8 "zstd:level=22"
#define ALG_DATA_9 "zstd:level=22"
#elif defined(DWARFS_HAVE_LIBLZ4)
#define ALG_DATA_8 "lz4hc:level=9"
#define ALG_DATA_9 "lz4hc:level=9"
#else
#define ALG_DATA_8 "null"
#define ALG_DATA_9 "null"
#endif

#if defined(DWARFS_HAVE_LIBZSTD)
#define ALG_SCHEMA "zstd:level=16"
#elif defined(DWARFS_HAVE_LIBLZMA)
#define ALG_SCHEMA "lzma:level=4"
#elif defined(DWARFS_HAVE_LIBLZ4)
#define ALG_SCHEMA "lz4hc:level=9"
#else
#define ALG_SCHEMA "null"
#endif

#if defined(DWARFS_HAVE_LIBZSTD)
#define ALG_METADATA_7 "zstd:level=22"
#elif defined(DWARFS_HAVE_LIBLZMA)
#define ALG_METADATA_7 "lzma:level=9"
#elif defined(DWARFS_HAVE_LIBLZ4)
#define ALG_METADATA_7 "lz4hc:level=9"
#else
#define ALG_METADATA_7 "null"
#endif

#if defined(DWARFS_HAVE_LIBLZMA)
#define ALG_METADATA_9 "lzma:level=9"
#elif defined(DWARFS_HAVE_LIBZSTD)
#define ALG_METADATA_9 "zstd:level=22"
#elif defined(DWARFS_HAVE_LIBLZ4)
#define ALG_METADATA_9 "lz4hc:level=9"
#else
#define ALG_METADATA_9 "null"
#endif

constexpr std::array<level_defaults, 10> levels{{
    // clang-format off
    /* 0 */ {20, "null",     "null"    , "null",          0, 0, "none"},
    /* 1 */ {20, ALG_DATA_1, ALG_SCHEMA, "null",          0, 0, "path"},
    /* 2 */ {20, ALG_DATA_2, ALG_SCHEMA, "null",          0, 0, "path"},
    /* 3 */ {21, ALG_DATA_3, ALG_SCHEMA, "null",         12, 1, "similarity"},
    /* 4 */ {22, ALG_DATA_4, ALG_SCHEMA, "null",         12, 2, "similarity"},
    /* 5 */ {23, ALG_DATA_5, ALG_SCHEMA, "null",         12, 2, "similarity"},
    /* 6 */ {24, ALG_DATA_6, ALG_SCHEMA, "null",         12, 3, "nilsimsa"},
    /* 7 */ {24, ALG_DATA_7, ALG_SCHEMA, ALG_METADATA_7, 12, 3, "nilsimsa"},
    /* 8 */ {24, ALG_DATA_8, ALG_SCHEMA, ALG_METADATA_9, 12, 4, "nilsimsa"},
    /* 9 */ {26, ALG_DATA_9, ALG_SCHEMA, ALG_METADATA_9, 12, 4, "nilsimsa"},
    // clang-format on
}};

constexpr size_t min_block_size_bits{10};
constexpr size_t max_block_size_bits{30};

metadata::serialization::SerializationFormat
get_format_from_string(std::string const& format_str) {
  using namespace metadata::serialization;

  if (format_str == "flatbuffers" || format_str == "flatbuffer") {
    return SerializationFormat::FLATBUFFERS;
  } else if (format_str == "legacy-thrift" || format_str == "legacy") {
    // Legacy Thrift (Frozen2) - hand-coded, Homebrew v0.14.1 compatible
    return SerializationFormat::LEGACY_THRIFT;
  } else if (format_str == "thrift") {
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
    // Note: "thrift" with DWARFS_HAVE_EXPERIMENTAL_THRIFT creates EXPERIMENTAL Modern Thrift format
    // This is NOT compatible with Homebrew dwarfs (which uses Legacy Thrift)
    return SerializationFormat::MODERN_THRIFT;
#else
    throw std::runtime_error(
        "Modern Thrift format not available (build without fbthrift support)");
#endif
  } else if (format_str == "cereal" || format_str == "bitsery") {
    throw std::runtime_error(fmt::format(
        "Metadata format '{}' is no longer supported. "
        "Cereal and Bitsery formats were removed in v0.16.0. "
        "Please use 'flatbuffers' (default/recommended), "
        "'legacy-thrift' (Homebrew compatible, uses .dwarfs extension), or "
        "'thrift' (experimental Modern Thrift fbthrift, uses .dftx extension) instead. "
        "To convert existing images, use: mkdwarfs --recompress=metadata "
        "--rebuild-metadata --format=flatbuffers -I old.dwarfs -O new.dwarfs",
        format_str));
  } else {
    throw std::runtime_error(fmt::format(
        "Unknown metadata format: '{}'. "
        "Supported formats:\n"
        "  flatbuffers - FlatBuffers format (.dff, stable, recommended default)\n"
        "  legacy-thrift - Legacy Thrift/Frozen2 (.dwarfs, stable, Homebrew compatible)\n"
        "  thrift - Modern Thrift CompactProtocol (.dftx, experimental, fbthrift)",
        format_str));
  }
}

} // anonymous namespace

argtable3_options_parser::argtable3_options_parser() = default;

argtable3_options_parser::~argtable3_options_parser() = default;

int argtable3_options_parser::parse(int argc, char** argv) {
  // Store command line for history
  opts_.command_line.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    opts_.command_line.emplace_back(argv[i]);
  }

  // Set defaults
  opts_.level = 7;
  opts_.num_workers = std::max(std::thread::hardware_concurrency(), 1U);
  opts_.compress_niceness = 5;
  opts_.memory_limit = "auto";
  opts_.file_hash_algo = "xxh3-128";
  opts_.progress_mode = "unicode";
  opts_.pack_metadata = "auto";

#if defined(DWARFS_HAVE_FLATBUFFERS)
  opts_.metadata_format = "flatbuffers";
#elif defined(DWARFS_HAVE_EXPERIMENTAL_THRIFT)
  opts_.metadata_format = "thrift";
#else
#error "At least one metadata format must be enabled"
#endif

  // Initialize argtable (estimate ~80 options)
  init_argtable(100);

  // Add common options (help, version, man)
  add_common_options();

  // Add logger options (log-level, verbose, quiet)
  add_logger_options();

  // Add tool-specific options
  define_tool_options();

  // Finalize argtable
  finalize_argtable();

  // Parse arguments
  int nerrors = parse_args(argc, argv);

  // Check for help/version/man BEFORE validation
  if (help_requested_ || version_requested_ || man_requested_) {
    return 1; // Signal that we displayed help/version/man
  }

  if (nerrors > 0) {
    return 2; // Parse error
  }

  // Populate parsed_options from argtable values
  populate_parsed_options();

  // Apply level defaults
  apply_level_defaults();

  // Validate options
  if (!validate_options()) {
    return 2; // Validation error
  }

  return 0; // Success
}

void argtable3_options_parser::define_tool_options() {
  // Input/Output options
  input_opt_ = arg_file0("i", "input", "<path>",
                         "path to root directory or source filesystem");
  input_list_opt_ =
      arg_file0(nullptr, "input-list", "<file>",
                "file containing list of file paths relative to root directory "
                "or - for stdin");
  output_opt_ =
      arg_file0("o", "output", "<file>", "filesystem output name or - for stdout");
  header_opt_ = arg_file0(nullptr, "header", "<file>",
                          "prepend output filesystem with contents of this file");
  remove_header_opt_ = arg_lit0(
      nullptr, "remove-header",
      "remove any header present before filesystem data (use with --recompress)");

  // Basic options
  force_opt_ = arg_lit0("f", "force", "force overwrite of existing output image");
  level_opt_ =
      arg_int0("l", "compress-level", "<0-9>",
               "compression level (0=fast, 9=best, default: 7)");
  long_help_opt_ = arg_lit0("H", "long-help", "output full help message and exit");

  // Compression options
  compression_opt_ =
      arg_strn("C", "compression", "<alg>", 0, 10, "block compression algorithm");
  schema_compression_opt_ = arg_str0(nullptr, "schema-compression", "<alg>",
                                     "metadata schema compression algorithm");
  metadata_compression_opt_ =
      arg_str0(nullptr, "metadata-compression", "<alg>",
               "metadata compression algorithm");
  history_compression_opt_ = arg_str0(nullptr, "history-compression", "<alg>",
                                      "history compression algorithm");
  block_size_bits_opt_ =
      arg_int0("S", "block-size-bits", "<N>", "block size bits (size = 2^N bits)");

  // Segmenter options
  max_lookback_blocks_opt_ =
      arg_strn("B", "max-lookback-blocks", "<N>", 0, 10,
              "how many blocks to scan for segments");
  window_size_opt_ =
      arg_strn("W", "window-size", "<N>", 0, 10, "window sizes for block hashing");
  window_step_opt_ = arg_strn("w", "window-step", "<N>", 0, 10,
                               "window step (as right shift of size)");
  bloom_filter_size_opt_ = arg_strn(nullptr, "bloom-filter-size", "<N>", 0, 10,
                                     "bloom filter size (2^N*values bits)");

  // Threading options
  num_workers_opt_ =
      arg_int0("N", "num-workers", "<count>",
               "number of writer (compression) worker threads");
  num_scanner_workers_opt_ =
      arg_int0(nullptr, "num-scanner-workers", "<count>",
               "number of scanner (hasher/categorizer) worker threads");
  num_segmenter_workers_opt_ =
      arg_int0(nullptr, "num-segmenter-workers", "<count>",
               "number of segmenter worker threads");
  compress_niceness_opt_ =
      arg_int0(nullptr, "compress-niceness", "<N>",
               "compression worker threads niceness");
  memory_limit_opt_ =
      arg_str0("L", "memory-limit", "<size>", "block manager memory limit");

  // Categorization options
  categorize_opt_ = arg_str0(nullptr, "categorize", "<list>",
                             "enable categorizers in the given order");
  max_similarity_size_opt_ =
      arg_str0(nullptr, "max-similarity-size", "<size>",
               "maximum file size to compute similarity");
  order_opt_ = arg_strn(nullptr, "order", "<algo>", 0, 10, "inode fragments order");

  // File hash option
  file_hash_opt_ =
      arg_str0(nullptr, "file-hash", "<algo>", "choice of file hashing function");

  // Progress options
  progress_opt_ = arg_str0(nullptr, "progress", "<mode>", "progress mode");
  no_progress_opt_ = arg_lit0(nullptr, "no-progress", "don't show progress");
  debug_filter_opt_ =
      arg_str0(nullptr, "debug-filter", "<mode>",
               "show effect of filter rules without producing an image");

  // Recompress options
  recompress_opt_ =
      arg_str0(nullptr, "recompress", "<opts>",
               "recompress an existing filesystem (none, block, metadata, all)");
  rebuild_metadata_opt_ =
      arg_lit0(nullptr, "rebuild-metadata", "fully rebuild metadata");
  change_block_size_opt_ =
      arg_lit0(nullptr, "change-block-size", "change block size when recompressing");
  recompress_categories_opt_ =
      arg_str0(nullptr, "recompress-categories", "<list>",
               "only recompress blocks of these categories");

  // Filesystem options
  with_devices_opt_ =
      arg_lit0(nullptr, "with-devices", "include block and character devices");
  with_specials_opt_ =
      arg_lit0(nullptr, "with-specials", "include named fifo and sockets");
  no_sparse_files_opt_ =
      arg_lit0(nullptr, "no-sparse-files", "don't store sparse files as sparse");
  no_section_index_opt_ =
      arg_lit0(nullptr, "no-section-index", "don't add section index to file system");
  no_history_opt_ = arg_lit0(nullptr, "no-history", "don't add history to file system");
  no_history_timestamps_opt_ = arg_lit0(nullptr, "no-history-timestamps",
                                        "don't add timestamps to file system history");
  no_history_command_line_opt_ =
      arg_lit0(nullptr, "no-history-command-line",
               "don't add command line to file system history");

  // Metadata options
  set_owner_opt_ =
      arg_int0(nullptr, "set-owner", "<uid>", "set owner (uid) for whole file system");
  set_group_opt_ =
      arg_int0(nullptr, "set-group", "<gid>", "set group (gid) for whole file system");
  chmod_opt_ =
      arg_str0(nullptr, "chmod", "<mode>", "recursively apply permission changes");
  no_create_timestamp_opt_ = arg_lit0(nullptr, "no-create-timestamp",
                                      "don't add create timestamp to file system");
  set_time_opt_ = arg_str0(nullptr, "set-time", "<time>",
                           "set timestamp for whole file system (unixtime or 'now')");
  keep_all_times_opt_ =
      arg_lit0(nullptr, "keep-all-times", "save atime and ctime in addition to mtime");
  time_resolution_opt_ = arg_str0(nullptr, "time-resolution", "<res>",
                                  "resolution of inode timestamps (default: 1s)");
  no_category_names_opt_ =
      arg_lit0(nullptr, "no-category-names", "don't add category names to file system");
  no_category_metadata_opt_ = arg_lit0(nullptr, "no-category-metadata",
                                       "don't add category metadata to file system");
  no_hardlink_table_opt_ = arg_lit0(nullptr, "no-hardlink-table",
                                    "don't add hardlink count table to file system");
  no_metadata_version_history_opt_ =
      arg_lit0(nullptr, "no-metadata-version-history",
               "remove metadata version history");
  pack_metadata_opt_ =
      arg_str0("P", "pack-metadata", "<opts>",
               "pack certain metadata elements (auto, all, none, etc.)");
  format_opt_ = arg_str0(
      nullptr, "format", "<format>",
      "metadata serialization format (flatbuffers [.dff, stable, default], legacy-thrift [.dwarfs, stable], thrift [.dftx, experimental])");

  // Filter options
  filter_opt_ = arg_strn("F", "filter", "<rule>", 0, 100, "add filter rule");
  remove_empty_dirs_opt_ =
      arg_lit0(nullptr, "remove-empty-dirs", "remove empty directories in file system");

  // Add all options to argtable
  argtable_.push_back(input_opt_);
  argtable_.push_back(input_list_opt_);
  argtable_.push_back(output_opt_);
  argtable_.push_back(header_opt_);
  argtable_.push_back(remove_header_opt_);
  argtable_.push_back(force_opt_);
  argtable_.push_back(level_opt_);
  argtable_.push_back(long_help_opt_);
  argtable_.push_back(compression_opt_);
  argtable_.push_back(schema_compression_opt_);
  argtable_.push_back(metadata_compression_opt_);
  argtable_.push_back(history_compression_opt_);
  argtable_.push_back(block_size_bits_opt_);
  argtable_.push_back(max_lookback_blocks_opt_);
  argtable_.push_back(window_size_opt_);
  argtable_.push_back(window_step_opt_);
  argtable_.push_back(bloom_filter_size_opt_);
  argtable_.push_back(num_workers_opt_);
  argtable_.push_back(num_scanner_workers_opt_);
  argtable_.push_back(num_segmenter_workers_opt_);
  argtable_.push_back(compress_niceness_opt_);
  argtable_.push_back(memory_limit_opt_);
  argtable_.push_back(categorize_opt_);
  argtable_.push_back(max_similarity_size_opt_);
  argtable_.push_back(order_opt_);
  argtable_.push_back(file_hash_opt_);
  argtable_.push_back(progress_opt_);
  argtable_.push_back(no_progress_opt_);
  argtable_.push_back(debug_filter_opt_);
  argtable_.push_back(recompress_opt_);
  argtable_.push_back(rebuild_metadata_opt_);
  argtable_.push_back(change_block_size_opt_);
  argtable_.push_back(recompress_categories_opt_);
  argtable_.push_back(with_devices_opt_);
  argtable_.push_back(with_specials_opt_);
  argtable_.push_back(no_sparse_files_opt_);
  argtable_.push_back(no_section_index_opt_);
  argtable_.push_back(no_history_opt_);
  argtable_.push_back(no_history_timestamps_opt_);
  argtable_.push_back(no_history_command_line_opt_);
  argtable_.push_back(set_owner_opt_);
  argtable_.push_back(set_group_opt_);
  argtable_.push_back(chmod_opt_);
  argtable_.push_back(no_create_timestamp_opt_);
  argtable_.push_back(set_time_opt_);
  argtable_.push_back(keep_all_times_opt_);
  argtable_.push_back(time_resolution_opt_);
  argtable_.push_back(no_category_names_opt_);
  argtable_.push_back(no_category_metadata_opt_);
  argtable_.push_back(no_hardlink_table_opt_);
  argtable_.push_back(no_metadata_version_history_opt_);
  argtable_.push_back(pack_metadata_opt_);
  argtable_.push_back(format_opt_);
  argtable_.push_back(filter_opt_);
  argtable_.push_back(remove_empty_dirs_opt_);
}

void argtable3_options_parser::populate_parsed_options() {
  // Input/Output paths
  if (input_opt_->count > 0) {
    opts_.input_path = std::filesystem::path(string_to_sys_string(input_opt_->filename[0]));
  }
  if (input_list_opt_->count > 0) {
    opts_.input_list.emplace(); // Initialize vector
    // Actual file reading happens in main
  }
  if (output_opt_->count > 0) {
    opts_.output_path = std::filesystem::path(string_to_sys_string(output_opt_->filename[0]));
  }
  if (header_opt_->count > 0) {
    opts_.header_path = std::filesystem::path(string_to_sys_string(header_opt_->filename[0]));
  }

  // Basic options
  opts_.force_overwrite = force_opt_->count > 0;
  opts_.remove_header = remove_header_opt_->count > 0;
  if (level_opt_->count > 0) {
    opts_.level = static_cast<unsigned>(level_opt_->ival[0]);
  }

  // Compression options
  if (compression_opt_->count > 0) {
    for (int i = 0; i < compression_opt_->count; ++i) {
      opts_.compression.emplace_back(compression_opt_->sval[i]);
    }
  }
  if (schema_compression_opt_->count > 0) {
    opts_.schema_compression = schema_compression_opt_->sval[0];
  }
  if (metadata_compression_opt_->count > 0) {
    opts_.metadata_compression = metadata_compression_opt_->sval[0];
  }
  if (history_compression_opt_->count > 0) {
    opts_.history_compression = history_compression_opt_->sval[0];
  }
  if (block_size_bits_opt_->count > 0) {
    opts_.segmenter_config.block_size_bits =
        static_cast<unsigned>(block_size_bits_opt_->ival[0]);
  }

  // Segmenter options
  if (max_lookback_blocks_opt_->count > 0) {
    for (int i = 0; i < max_lookback_blocks_opt_->count; ++i) {
      opts_.max_lookback_blocks.emplace_back(max_lookback_blocks_opt_->sval[i]);
    }
  }
  if (window_size_opt_->count > 0) {
    for (int i = 0; i < window_size_opt_->count; ++i) {
      opts_.window_size.emplace_back(window_size_opt_->sval[i]);
    }
  }
  if (window_step_opt_->count > 0) {
    for (int i = 0; i < window_step_opt_->count; ++i) {
      opts_.window_step.emplace_back(window_step_opt_->sval[i]);
    }
  }
  if (bloom_filter_size_opt_->count > 0) {
    for (int i = 0; i < bloom_filter_size_opt_->count; ++i) {
      opts_.bloom_filter_size.emplace_back(bloom_filter_size_opt_->sval[i]);
    }
  }

  // Threading options
  if (num_workers_opt_->count > 0) {
    opts_.num_workers = static_cast<size_t>(num_workers_opt_->ival[0]);
  }
  if (num_scanner_workers_opt_->count > 0) {
    opts_.num_scanner_workers = static_cast<size_t>(num_scanner_workers_opt_->ival[0]);
  }
  if (num_segmenter_workers_opt_->count > 0) {
    opts_.num_segmenter_workers =
        static_cast<size_t>(num_segmenter_workers_opt_->ival[0]);
  }
  if (compress_niceness_opt_->count > 0) {
    opts_.compress_niceness = compress_niceness_opt_->ival[0];
  }
  if (memory_limit_opt_->count > 0) {
    opts_.memory_limit = memory_limit_opt_->sval[0];
  }

  // Categorization options
  if (categorize_opt_->count > 0) {
    opts_.categorizer_list = categorize_opt_->sval[0];
    opts_.categorizer_explicit = true;
  }
  if (max_similarity_size_opt_->count > 0) {
    opts_.max_similarity_size = max_similarity_size_opt_->sval[0];
  }
  if (order_opt_->count > 0) {
    for (int i = 0; i < order_opt_->count; ++i) {
      opts_.order.emplace_back(order_opt_->sval[i]);
    }
  }

  // File hash option
  if (file_hash_opt_->count > 0) {
    opts_.file_hash_algo = file_hash_opt_->sval[0];
  }

  // Progress options
  if (progress_opt_->count > 0) {
    opts_.progress_mode = progress_opt_->sval[0];
  }
  opts_.no_progress = no_progress_opt_->count > 0;
  if (debug_filter_opt_->count > 0) {
    opts_.debug_filter = debug_filter_opt_->sval[0];
  }

  // Recompress options
  if (recompress_opt_->count > 0) {
    opts_.recompress_opts = recompress_opt_->sval[0];
    opts_.is_recompress = true;
  }
  if (rebuild_metadata_opt_->count > 0) {
    opts_.rebuild_metadata = true;
    opts_.is_recompress = true;
  }
  if (change_block_size_opt_->count > 0) {
    opts_.change_block_size = true;
    opts_.is_recompress = true;
  }
  if (recompress_categories_opt_->count > 0) {
    opts_.recompress_categories = recompress_categories_opt_->sval[0];
  }

  // Filesystem options
  opts_.scanner_opts.with_devices = with_devices_opt_->count > 0;
  opts_.scanner_opts.with_specials = with_specials_opt_->count > 0;
  opts_.no_sparse_files = no_sparse_files_opt_->count > 0;
  opts_.no_section_index = no_section_index_opt_->count > 0;
  opts_.no_history = no_history_opt_->count > 0;
  opts_.no_history_timestamps = no_history_timestamps_opt_->count > 0;
  opts_.no_history_command_line = no_history_command_line_opt_->count > 0;

  // Metadata options
  if (set_owner_opt_->count > 0) {
    opts_.set_owner = static_cast<uint16_t>(set_owner_opt_->ival[0]);
  }
  if (set_group_opt_->count > 0) {
    opts_.set_group = static_cast<uint16_t>(set_group_opt_->ival[0]);
  }
  if (chmod_opt_->count > 0) {
    opts_.chmod_str = chmod_opt_->sval[0];
  }
  opts_.scanner_opts.metadata.no_create_timestamp = no_create_timestamp_opt_->count > 0;
  if (set_time_opt_->count > 0) {
    opts_.timestamp = set_time_opt_->sval[0];
  }
  opts_.scanner_opts.metadata.keep_all_times = keep_all_times_opt_->count > 0;
  if (time_resolution_opt_->count > 0) {
    opts_.time_resolution = time_resolution_opt_->sval[0];
  }
  opts_.scanner_opts.metadata.no_category_names = no_category_names_opt_->count > 0;
  opts_.scanner_opts.metadata.no_category_metadata =
      no_category_metadata_opt_->count > 0;
  opts_.scanner_opts.metadata.no_hardlink_table = no_hardlink_table_opt_->count > 0;
  opts_.scanner_opts.metadata.no_metadata_version_history =
      no_metadata_version_history_opt_->count > 0;
  if (pack_metadata_opt_->count > 0) {
    opts_.pack_metadata = pack_metadata_opt_->sval[0];
  }
  if (format_opt_->count > 0) {
    opts_.metadata_format = format_opt_->sval[0];
  }

  // Filter options
  if (filter_opt_->count > 0) {
    for (int i = 0; i < filter_opt_->count; ++i) {
      opts_.filter_rules.emplace_back(filter_opt_->sval[i]);
    }
  }
  opts_.scanner_opts.remove_empty_dirs = remove_empty_dirs_opt_->count > 0;

  // Parse metadata format
  try {
    opts_.metadata_format_enum = get_format_from_string(opts_.metadata_format);
    opts_.scanner_opts.metadata_format = opts_.metadata_format_enum;
  } catch (std::exception const& e) {
    throw std::runtime_error(fmt::format("Invalid metadata format: {}", e.what()));
  }
}

void argtable3_options_parser::apply_level_defaults() {
  auto const& defaults = levels[opts_.level];

  // Apply block size default if not set
  if (block_size_bits_opt_->count == 0) {
    opts_.segmenter_config.block_size_bits = defaults.block_size_bits;
  }

  // Apply compression defaults if not set
  if (schema_compression_opt_->count == 0) {
    opts_.schema_compression = defaults.schema_history_compression;
  }
  if (history_compression_opt_->count == 0) {
    opts_.history_compression = defaults.schema_history_compression;
  }
  if (metadata_compression_opt_->count == 0) {
    opts_.metadata_compression = defaults.metadata_compression;
  }
  if (compression_opt_->count == 0 && opts_.compression.empty()) {
    opts_.compression.push_back(std::string(defaults.data_compression));
  }

  // Apply segmenter defaults if not set
  if (window_size_opt_->count == 0) {
    opts_.segmenter_config.blockhash_window_size.set_default(defaults.window_size);
  }
  if (window_step_opt_->count == 0) {
    opts_.segmenter_config.window_increment_shift.set_default(defaults.window_step);
  }
  if (max_lookback_blocks_opt_->count == 0) {
    opts_.segmenter_config.max_active_blocks.set_default(1);
  }
  if (bloom_filter_size_opt_->count == 0) {
    opts_.segmenter_config.bloom_filter_size.set_default(4);
  }

  // Set worker thread defaults
  if (num_scanner_workers_opt_->count == 0) {
    opts_.num_scanner_workers = opts_.num_workers;
  }
  if (num_segmenter_workers_opt_->count == 0) {
    opts_.num_segmenter_workers = opts_.num_workers;
  }
  opts_.scanner_opts.num_segmenter_workers = opts_.num_segmenter_workers;

  // Process additional options that need defaults
  process_additional_options();
}

void argtable3_options_parser::process_additional_options() {
  // Process input-list option
  if (input_list_opt_->count > 0) {
    opts_.scanner_opts.with_devices = true;
    opts_.scanner_opts.with_specials = true;
    if (input_opt_->count == 0) {
      opts_.input_path.clear();
    }
  }

  // Process file hash algorithm
  if (opts_.file_hash_algo == "none") {
    opts_.scanner_opts.file_hash_algorithm.reset();
  } else if (!checksum::is_available(opts_.file_hash_algo)) {
    throw std::runtime_error(
        fmt::format("Unknown file hash function '{}'", opts_.file_hash_algo));
  } else {
    opts_.scanner_opts.file_hash_algorithm = opts_.file_hash_algo;
  }

  // Process max similarity size
  if (max_similarity_size_opt_->count > 0) {
    auto size = parse_size_with_unit(opts_.max_similarity_size);
    if (size > 0) {
      opts_.scanner_opts.inode.max_similarity_scan_size = size;
    }
  }

  // Process chmod option
  if (chmod_opt_->count > 0) {
    std::string chmod_str = opts_.chmod_str;
    if (chmod_str == "norm") {
      chmod_str = "ug-st,=Xr";
    }
    opts_.scanner_opts.metadata.chmod_specifiers = chmod_str;
    opts_.scanner_opts.metadata.umask = get_current_umask();
  }

  // Process set-time option
  if (set_time_opt_->count > 0) {
    if (opts_.timestamp == "now") {
      opts_.scanner_opts.metadata.timestamp = std::time(nullptr);
    } else if (auto val = try_to<uint64_t>(opts_.timestamp)) {
      opts_.scanner_opts.metadata.timestamp = val;
    } else {
      try {
        auto tp = parse_time_point(opts_.timestamp);
        opts_.scanner_opts.metadata.timestamp =
            std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch())
                .count();
      } catch (std::exception const& e) {
        throw std::runtime_error(
            fmt::format("Invalid timestamp: {}", opts_.timestamp));
      }
    }
  }

  // Process time-resolution option
  if (time_resolution_opt_->count > 0) {
    auto const res = parse_time_with_unit(opts_.time_resolution);
    if (res.count() == 0) {
      throw std::runtime_error("time-resolution must be nonzero");
    }
    opts_.scanner_opts.metadata.time_resolution = res;
  }

  // Process pack-metadata option
  if (!opts_.pack_metadata.empty() && opts_.pack_metadata != "none") {
    if (opts_.pack_metadata == "auto") {
      opts_.scanner_opts.metadata.force_pack_string_tables = false;
      opts_.scanner_opts.metadata.pack_chunk_table = false;
      opts_.scanner_opts.metadata.pack_directories = false;
      opts_.scanner_opts.metadata.pack_shared_files_table = false;
      opts_.scanner_opts.metadata.pack_names = true;
      opts_.scanner_opts.metadata.pack_names_index = false;
      opts_.scanner_opts.metadata.pack_symlinks = true;
      opts_.scanner_opts.metadata.pack_symlinks_index = false;
    } else {
      auto pack_opts =
          split_to<std::vector<std::string_view>>(opts_.pack_metadata, ',');
      for (auto const& opt : pack_opts) {
        if (opt == "chunk_table") {
          opts_.scanner_opts.metadata.pack_chunk_table = true;
        } else if (opt == "directories") {
          opts_.scanner_opts.metadata.pack_directories = true;
        } else if (opt == "symlinks") {
          opts_.scanner_opts.metadata.pack_symlinks = true;
        } else if (opt == "symlinks_index") {
          opts_.scanner_opts.metadata.pack_symlinks_index = true;
        } else if (opt == "force") {
          opts_.scanner_opts.metadata.force_pack_string_tables = true;
        } else if (opt == "plain") {
          opts_.scanner_opts.metadata.plain_names_table = true;
          opts_.scanner_opts.metadata.plain_symlinks_table = true;
        } else if (opt == "all") {
          opts_.scanner_opts.metadata.pack_chunk_table = true;
          opts_.scanner_opts.metadata.pack_directories = true;
          opts_.scanner_opts.metadata.pack_shared_files_table = true;
          opts_.scanner_opts.metadata.pack_names = true;
          opts_.scanner_opts.metadata.pack_names_index = true;
          opts_.scanner_opts.metadata.pack_symlinks = true;
          opts_.scanner_opts.metadata.pack_symlinks_index = true;
        } else {
          throw std::runtime_error(
              fmt::format("Invalid pack-metadata option: {}", opt));
        }
      }
    }
  }
}

bool argtable3_options_parser::validate_options() {
  try {
    validate_level();
    validate_block_size();
    validate_recompress_requirements();
    // Path validation requires iolayer, will be done in main
  } catch (std::exception const& e) {
    fmt::print(stderr, "error: {}\n", e.what());
    return false;
  }
  return true;
}

void argtable3_options_parser::validate_level() {
  constexpr unsigned max_level = 9;
  if (opts_.level > max_level) {
    throw std::runtime_error("invalid compression level (must be 0-9)");
  }
}

void argtable3_options_parser::validate_block_size() {
  if (opts_.segmenter_config.block_size_bits < min_block_size_bits ||
      opts_.segmenter_config.block_size_bits > max_block_size_bits) {
    throw std::runtime_error(
        fmt::format("block size must be between {} and {}", min_block_size_bits,
                    max_block_size_bits));
  }
}

void argtable3_options_parser::validate_paths(iolayer const& iol) {
  // Validate input path exists
  if (!opts_.is_recompress) {
    std::error_code ec;
    if (!std::filesystem::exists(opts_.input_path, ec)) {
      throw std::runtime_error(
          fmt::format("input path does not exist: {}", opts_.input_path.string()));
    }
  }
}

void argtable3_options_parser::validate_recompress_requirements() {
#ifndef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  if (opts_.is_recompress || opts_.rebuild_metadata || opts_.change_block_size) {
    throw std::runtime_error(
        "recompress functionality requires Modern Thrift support\n"
        "This build was compiled without Modern Thrift (DWARFS_WITH_EXPERIMENTAL_THRIFT=OFF)\n"
        "Recompressing existing images requires Modern Thrift because the rewrite\n"
        "implementation depends on Thrift-specific metadata APIs.\n"
        "\n"
        "To use recompress features, rebuild with DWARFS_WITH_EXPERIMENTAL_THRIFT=ON");
  }
#endif
}

void argtable3_options_parser::load_environment_variables() {
  // Load base environment variables (DWARFS_LOG_LEVEL, etc.)
  argtable3_base_parser::load_environment_variables("MKDWARFS");

  // Load tool-specific environment variables
  // Format: DWARFS_MKDWARFS_<OPTION>
  std::string prefix = "DWARFS_MKDWARFS_";

  // Example: DWARFS_MKDWARFS_COMPRESSION_LEVEL
  if (auto env_val = get_env_var(prefix + "COMPRESSION_LEVEL"); !env_val.empty()) {
    if (level_opt_->count == 0) { // CLI takes priority
      if (auto val = try_to<int>(env_val)) {
        opts_.level = static_cast<unsigned>(*val);
      }
    }
  }

  // Example: DWARFS_MKDWARFS_NUM_WORKERS
  if (auto env_val = get_env_var(prefix + "NUM_WORKERS"); !env_val.empty()) {
    if (num_workers_opt_->count == 0) {
      if (auto val = try_to<size_t>(env_val)) {
        opts_.num_workers = *val;
      }
    }
  }

  // Add more environment variable handling as needed
  // Following MECE principle: CLI > ENV > defaults
}

} // namespace dwarfs::tool::mkdwarfs