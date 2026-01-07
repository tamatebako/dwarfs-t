#include <dwarfs/error.h>
#include <dwarfs/logger.h>
#include <dwarfs/sorted_array_map.h>
#include <dwarfs/writer/console_writer.h>
#include <dwarfs/writer/filter_debug.h>
#include <dwarfs/writer/scanner_options.h>

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

#include <dwarfs/tool/mkdwarfs/options_parser.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <filesystem>
#include <string>
#include <string_view>
#include <stdint.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>

#include <fmt/format.h>

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/view/map.hpp>

#include <dwarfs/checksum.h>
#include <dwarfs/compressor_registry.h>
#include <dwarfs/config.h>
#include <dwarfs/decompressor_registry.h>
#include <dwarfs/error.h>
#include <dwarfs/logger.h>
#include <dwarfs/sorted_array_map.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/tool/program_options_helpers.h>
#include <dwarfs/tool/tool.h>
#include <dwarfs/util.h>
#include <dwarfs/string.h>
#include <dwarfs/conv.h>
#include <dwarfs_tool_manpage.h>

namespace po = boost::program_options;

namespace dwarfs::tool::mkdwarfs {

namespace {

using namespace std::string_view_literals;

constexpr sorted_array_map progress_modes{
    std::pair{"none"sv, ::dwarfs::writer::console_writer::NONE},
    std::pair{"simple"sv, ::dwarfs::writer::console_writer::SIMPLE},
    std::pair{"ascii"sv, ::dwarfs::writer::console_writer::ASCII},
    std::pair{"unicode"sv, ::dwarfs::writer::console_writer::UNICODE},
};

constexpr sorted_array_map debug_filter_modes{
    std::pair{"included"sv, ::dwarfs::writer::debug_filter_mode::INCLUDED},
    std::pair{"included-files"sv, ::dwarfs::writer::debug_filter_mode::INCLUDED_FILES},
    std::pair{"excluded"sv, ::dwarfs::writer::debug_filter_mode::EXCLUDED},
    std::pair{"excluded-files"sv, ::dwarfs::writer::debug_filter_mode::EXCLUDED_FILES},
    std::pair{"files"sv, ::dwarfs::writer::debug_filter_mode::FILES},
    std::pair{"all"sv, ::dwarfs::writer::debug_filter_mode::ALL},
};

struct level_defaults {
  unsigned block_size_bits;
  std::string_view data_compression;
  std::string_view schema_history_compression;
  std::string_view metadata_compression;
  unsigned window_size;
  unsigned window_step;
  std::string_view order;
};

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
  } else if (format_str == "thrift") {
#ifdef DWARFS_HAVE_THRIFT
    return SerializationFormat::THRIFT_COMPACT;
#else
    throw std::runtime_error(
        "Thrift format not available (build without Thrift support)");
#endif
  } else if (format_str == "cereal" || format_str == "bitsery") {
    throw std::runtime_error(
        fmt::format("Metadata format '{}' is no longer supported. "
                    "Cereal and Bitsery formats were removed in v0.16.0. "
                    "Please use 'flatbuffers' (default) or 'thrift' (legacy) instead. "
                    "To convert existing images, use: mkdwarfs --recompress=metadata "
                    "--rebuild-metadata --format=flatbuffers -I old.dwarfs -O new.dwarfs",
                    format_str));
  } else {
    throw std::runtime_error(
        fmt::format("Unknown metadata format: '{}'. "
                    "Supported formats: flatbuffers (default), thrift (legacy)",
                    format_str));
  }
}

} // anonymous namespace

int options_parser::parse(int argc, sys_char** argv, iolayer const& iol,
                          parsed_options& opts) {
  // Collect command line for history
  opts.command_line.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    opts.command_line.emplace_back(sys_string_to_string(argv[i]));
  }

  // Temporary variables for sys_string parsing
  sys_string path_str, input_list_str, output_str, header_str;
  uint16_t uid, gid;  // temporary variables for set-owner/set-group

  // Helper lambda functions for default values
  auto lvl_def_val = [](auto opt) {
    // Placeholder for level defaults - will be set after level is parsed
    return "arg";
  };

  auto dep_def_val = [](auto dep) { return fmt::format("arg (={})", dep); };

  auto cat_def_val = [](auto def) {
    return fmt::format("[cat::]arg (={})", def);
  };

  // Set up option descriptions
  // clang-format off
  po::options_description basic_opts("Options");
  basic_opts.add_options()
    ("input,i",
        po_sys_value<sys_string>(&path_str),
        "path to root directory or source filesystem")
    ("input-list",
        po_sys_value<sys_string>(&input_list_str),
        "file containing list of file paths relative to root directory "
        "or - for stdin")
    ("output,o",
        po_sys_value<sys_string>(&output_str),
        "filesystem output name or - for stdout")
    ("force,f",
        po::value<bool>(&opts.force_overwrite)->zero_tokens(),
        "force overwrite of existing output image")
    ("compress-level,l",
        po::value<unsigned>(&opts.level)->default_value(7),
        "compression level (0=fast, 9=best, see -H and man page for details)")
    ;

  logger_options logopts;
  tool::add_common_options(basic_opts, logopts);

  basic_opts.add_options()
    ("long-help,H",
        "output full help message and exit")
    ;

  po::options_description advanced_opts("Advanced options");
  advanced_opts.add_options()
    ("block-size-bits,S",
        po::value<unsigned>(&opts.segmenter_config.block_size_bits),
        "block size bits (size = 2^arg bits)")
    ("num-workers,N",
        po::value<size_t>(&opts.num_workers)->default_value(std::max(std::thread::hardware_concurrency(), 1U)),
        "number of writer (compression) worker threads")
    ("compress-niceness",
        po::value<int>(&opts.compress_niceness)->default_value(5),
        "compression worker threads niceness")
    ("num-scanner-workers",
        po::value<size_t>(&opts.num_scanner_workers),
        "number of scanner (hasher/categorizer) worker threads")
    ("num-segmenter-workers",
        po::value<size_t>(&opts.num_segmenter_workers),
        "number of segmenter worker threads")
    ("memory-limit,L",
        po::value<std::string>(&opts.memory_limit)->default_value("auto"),
        "block manager memory limit")
    ("recompress",
        po::value<std::string>(&opts.recompress_opts)->implicit_value("all"),
        "recompress an existing filesystem (none, block, metadata, all)")
    ("rebuild-metadata",
        po::value<bool>(&opts.rebuild_metadata)->zero_tokens(),
        "fully rebuild metadata")
    ("change-block-size",
        po::value<bool>(&opts.change_block_size)->zero_tokens(),
        "change block size when recompressing")
    ("no-metadata-version-history",
        po::value<bool>(&opts.scanner_opts.metadata.no_metadata_version_history)->zero_tokens(),
        "remove metadata version history")
    ("recompress-categories",
        po::value<std::string>(&opts.recompress_categories),
        "only recompress blocks of these categories")
    ("categorize",
        po::value<std::string>(&opts.categorizer_list)->implicit_value("fits,pcmaudio,incompressible"),
        "enable categorizers in the given order")
    ("order",
        po::value<std::vector<std::string>>(&opts.order)->multitoken()->composing(),
        "inode fragments order")
    ("max-similarity-size",
        po::value<std::string>(&opts.max_similarity_size),
        "maximum file size to compute similarity")
    ("file-hash",
        po::value<std::string>(&opts.file_hash_algo)->default_value("xxh3-128"),
        "choice of file hashing function")
    ("progress",
        po::value<std::string>(&opts.progress_mode)->default_value("unicode"),
        "progress mode")
    ("no-progress",
        po::value<bool>(&opts.no_progress)->zero_tokens(),
        "don't show progress")
    ;

  po::options_description filesystem_opts("File system options");
  filesystem_opts.add_options()
    ("with-devices",
        po::value<bool>(&opts.scanner_opts.with_devices)->zero_tokens(),
        "include block and character devices")
    ("with-specials",
        po::value<bool>(&opts.scanner_opts.with_specials)->zero_tokens(),
        "include named fifo and sockets")
    ("no-sparse-files",
        po::value<bool>(&opts.no_sparse_files)->zero_tokens(),
        "don't store sparse files as sparse")
    ("header",
        po_sys_value<sys_string>(&header_str),
        "prepend output filesystem with contents of this file")
    ("remove-header",
        po::value<bool>(&opts.remove_header)->zero_tokens(),
        "remove any header present before filesystem data"
        " (use with --recompress)")
    ("no-section-index",
        po::value<bool>(&opts.no_section_index)->zero_tokens(),
        "don't add section index to file system")
    ("no-history",
        po::value<bool>(&opts.no_history)->zero_tokens(),
        "don't add history to file system")
    ("no-history-timestamps",
        po::value<bool>(&opts.no_history_timestamps)->zero_tokens(),
        "don't add timestamps to file system history")
    ("no-history-command-line",
        po::value<bool>(&opts.no_history_command_line)->zero_tokens(),
        "don't add command line to file system history")
    ;

  po::options_description segmenter_opts("Segmenter options");
  segmenter_opts.add_options()
    ("max-lookback-blocks,B",
        po::value<std::vector<std::string>>(&opts.max_lookback_blocks)->multitoken()->composing(),
        "how many blocks to scan for segments")
    ("window-size,W",
        po::value<std::vector<std::string>>(&opts.window_size)->multitoken()->composing(),
        "window sizes for block hashing")
    ("window-step,w",
        po::value<std::vector<std::string>>(&opts.window_step)->multitoken()->composing(),
        "window step (as right shift of size)")
    ("bloom-filter-size",
        po::value<std::vector<std::string>>(&opts.bloom_filter_size)->multitoken()->composing(),
        "bloom filter size (2^N*values bits)")
    ;

  po::options_description compressor_opts("Compressor options");
  compressor_opts.add_options()
    ("compression,C",
        po::value<std::vector<std::string>>(&opts.compression)->multitoken()->composing(),
        "block compression algorithm")
    ("schema-compression",
        po::value<std::string>(&opts.schema_compression),
        "metadata schema compression algorithm")
    ("metadata-compression",
        po::value<std::string>(&opts.metadata_compression),
        "metadata compression algorithm")
    ("history-compression",
        po::value<std::string>(&opts.history_compression),
        "history compression algorithm")
    ;

  po::options_description filter_opts("Filter options");
  filter_opts.add_options()
    ("filter,F",
        po_sys_value<std::vector<sys_string>>(&opts.filter_rules)->multitoken()->composing(),
        "add filter rule")
    ("debug-filter",
        po::value<std::string>(&opts.debug_filter)->implicit_value("all"),
        "show effect of filter rules without producing an image")
    ("remove-empty-dirs",
        po::value<bool>(&opts.scanner_opts.remove_empty_dirs)->zero_tokens(),
        "remove empty directories in file system")
    ;

  po::options_description metadata_opts("Metadata options");
  metadata_opts.add_options()
    ("set-owner",
        po::value<uint16_t>(&uid),
        "set owner (uid) for whole file system")
    ("set-group",
        po::value<uint16_t>(&gid),
        "set group (gid) for whole file system")
    ("chmod",
        po::value<std::string>(&opts.chmod_str),
        "recursively apply permission changes")
    ("no-create-timestamp",
        po::value<bool>(&opts.scanner_opts.metadata.no_create_timestamp)->zero_tokens(),
        "don't add create timestamp to file system")
    ("set-time",
        po::value<std::string>(&opts.timestamp),
        "set timestamp for whole file system (unixtime or 'now')")
    ("keep-all-times",
        po::value<bool>(&opts.scanner_opts.metadata.keep_all_times)->zero_tokens(),
        "save atime and ctime in addition to mtime")
    ("time-resolution",
        po::value<std::string>(&opts.time_resolution),
        "resolution of inode timestamps (default: 1s)")
    ("no-category-names",
        po::value<bool>(&opts.scanner_opts.metadata.no_category_names)->zero_tokens(),
        "don't add category names to file system")
    ("no-category-metadata",
        po::value<bool>(&opts.scanner_opts.metadata.no_category_metadata)->zero_tokens(),
        "don't add category metadata to file system")
    ("no-hardlink-table",
        po::value<bool>(&opts.scanner_opts.metadata.no_hardlink_table)->zero_tokens(),
        "don't add hardlink count table to file system")
    ("pack-metadata,P",
        po::value<std::string>(&opts.pack_metadata)->default_value("auto"),
        "pack certain metadata elements (auto, all, none, chunk_table, "
        "directories, shared_files, names, names_index, symlinks, "
        "symlinks_index, force, plain)")
    ("format",
#if defined(DWARFS_HAVE_FLATBUFFERS)
        po::value<std::string>(&opts.metadata_format)->default_value("flatbuffers"),
        "metadata serialization format (flatbuffers [default], thrift [legacy])")
#elif defined(DWARFS_HAVE_THRIFT)
        po::value<std::string>(&opts.metadata_format)->default_value("thrift"),
        "metadata serialization format (thrift [default], flatbuffers not available)")
#else
        #error "At least one metadata format (DWARFS_HAVE_FLATBUFFERS or DWARFS_HAVE_THRIFT) must be enabled"
#endif
    ;
  // clang-format on

  // Combine all option groups
  po::options_description all_opts;
  all_opts.add(basic_opts)
      .add(advanced_opts)
      .add(filter_opts)
      .add(segmenter_opts)
      .add(compressor_opts)
      .add(filesystem_opts)
      .add(metadata_opts);

  po::variables_map vm;

  try {
    auto parsed = po::parse_command_line(argc, argv, all_opts);
    po::store(parsed, vm);
    po::notify(vm);

    auto unrecognized =
        po::collect_unrecognized(parsed.options, po::include_positional);

    if (!unrecognized.empty()) {
      iol.err << "error: unrecognized argument(s) '"
              << sys_string_to_string(boost::join(unrecognized, " ")) << "'\n";
      return 1;
    }
  } catch (po::error const& e) {
    iol.err << "error: " << e.what() << "\n";
    return 1;
  }

#ifdef DWARFS_BUILTIN_MANPAGE
  // Check for --man BEFORE any validation or processing
  if (vm.contains("man")) {
    opts.is_man = true;
    return 0;
  }
#endif

  // Convert sys_string paths to filesystem::path
  if (vm.contains("input")) {
    opts.input_path = std::filesystem::path(path_str);
  }
  if (vm.contains("input-list")) {
    // Will be processed later - just note that it was provided
    // Store the string for now, processing happens in main
  }
  if (vm.contains("output")) {
    opts.output_path = std::filesystem::path(output_str);
  }
  if (vm.contains("header")) {
    opts.header_path = std::filesystem::path(header_str);
  }

  // Handle help and version
  if (vm.contains("help")) {
    // Output help - handled by caller
    return 0;
  }

  // Set recompress mode flags
  if (vm.contains("recompress") || vm.contains("rebuild-metadata") || vm.contains("change-block-size")) {
    opts.is_recompress = true;
  }

  // Handle set-owner/set-group
  if (vm.contains("set-owner")) {
    opts.set_owner = uid;
  }
  if (vm.contains("set-group")) {
    opts.set_group = gid;
  }

  // Parse metadata format and set it in scanner_opts immediately
  try {
    opts.metadata_format_enum = get_format_from_string(opts.metadata_format);
    // CRITICAL: Set scanner_opts.metadata_format right away
    opts.scanner_opts.metadata_format = opts.metadata_format_enum;
  } catch (std::exception const& e) {
    iol.err << "error: " << e.what() << "\n";
    return 2;
  }

  // Check file extension and provide recommendations
  if (vm.contains("output") && !opts.output_path.empty()) {
    auto ext = opts.output_path.extension().string();
    std::string recommended_ext;
    std::string format_name;

    using namespace metadata::serialization;
    if (opts.metadata_format_enum == SerializationFormat::FLATBUFFERS) {
      recommended_ext = ".dff";
      format_name = "FlatBuffers";
    } else if (opts.metadata_format_enum == SerializationFormat::THRIFT_COMPACT) {
      recommended_ext = ".dft";
      format_name = "Thrift";
    }

    if (!recommended_ext.empty()) {
      if (ext == ".dwarfs") {
        iol.out << "info: Using generic .dwarfs extension with " << format_name
                << " format. For clarity, consider using " << recommended_ext
                << " extension.\n";
      } else if (!ext.empty() && ext != recommended_ext) {
        iol.out << "warning: Output extension " << ext << " does not match "
                << format_name << " format. Recommended extension: "
                << recommended_ext << "\n";
      }
      // If extension matches or is empty, proceed silently
    }
  }

  // Validate and process options
  try {
    validate_level(opts);
    validate_block_size(opts);
    validate_paths(opts, iol);
    validate_recompress_requirements(opts);
  } catch (std::exception const& e) {
    iol.err << "error: " << e.what() << "\n";
    return 2;
  }

  // Apply defaults based on compression level
  auto const& defaults = levels[opts.level];

  if (!vm.contains("block-size-bits")) {
    opts.segmenter_config.block_size_bits = defaults.block_size_bits;
  }

  if (!vm.contains("schema-compression")) {
    opts.schema_compression = defaults.schema_history_compression;
  }

  if (!vm.contains("history-compression")) {
    opts.history_compression = defaults.schema_history_compression;
  }

  if (!vm.contains("metadata-compression")) {
    opts.metadata_compression = defaults.metadata_compression;
  }

  // CRITICAL: Apply data compression default if not explicitly set
  if (!vm.contains("compression") && opts.compression.empty()) {
    opts.compression.push_back(std::string(defaults.data_compression));
  }

  // CRITICAL: Apply segmenter defaults based on level
  if (!vm.contains("window-size")) {
    opts.segmenter_config.blockhash_window_size.set_default(defaults.window_size);
  }
  if (!vm.contains("window-step")) {
    opts.segmenter_config.window_increment_shift.set_default(defaults.window_step);
  }
  if (!vm.contains("max-lookback-blocks")) {
    opts.segmenter_config.max_active_blocks.set_default(1);
  }
  if (!vm.contains("bloom-filter-size")) {
    opts.segmenter_config.bloom_filter_size.set_default(4);
  }

  // Process input-list option
  if (vm.contains("input-list")) {
    if (vm.contains("filter")) {
      iol.err << "error: cannot combine --input-list and --filter\n";
      return 1;
    }

    // Implicitly turn on device and special file support
    opts.scanner_opts.with_devices = true;
    opts.scanner_opts.with_specials = true;

    // If no explicit input path, use current directory
    if (!vm.contains("input")) {
      // Store empty path to signal we should use current directory
      opts.input_path.clear();
    }

    // Note: Actual file reading happens in main, we just store the path
    opts.input_list.emplace();  // Initialize the vector
  }

  // Process file hash algorithm
  if (opts.file_hash_algo == "none") {
    opts.scanner_opts.file_hash_algorithm.reset();
  } else if (!checksum::is_available(opts.file_hash_algo)) {
    iol.err << "error: unknown file hash function '" << opts.file_hash_algo << "'\n";
    return 1;
  } else {
    opts.scanner_opts.file_hash_algorithm = opts.file_hash_algo;
  }

  // Process max similarity size
  if (vm.contains("max-similarity-size")) {
    try {
      auto size = parse_size_with_unit(opts.max_similarity_size);
      if (size > 0) {
        opts.scanner_opts.inode.max_similarity_scan_size = size;
      }
    } catch (std::exception const& e) {
      iol.err << "error: invalid max-similarity-size: " << e.what() << "\n";
      return 1;
    }
  }

  // Set worker thread defaults
  if (!vm.contains("num-scanner-workers")) {
    opts.num_scanner_workers = opts.num_workers;
  }

  if (!vm.contains("num-segmenter-workers")) {
    opts.num_segmenter_workers = opts.num_workers;
  }

  opts.scanner_opts.num_segmenter_workers = opts.num_segmenter_workers;

  // Process chmod option
  if (vm.contains("chmod")) {
    std::string chmod_str = opts.chmod_str;
    if (chmod_str == "norm") {
      chmod_str = "ug-st,=Xr";
    }
    opts.scanner_opts.metadata.chmod_specifiers = chmod_str;
    opts.scanner_opts.metadata.umask = get_current_umask();
  }

  // Process set-time option
  if (vm.contains("set-time")) {
    if (opts.timestamp == "now") {
      opts.scanner_opts.metadata.timestamp = std::time(nullptr);
    } else if (auto val = try_to<uint64_t>(opts.timestamp)) {
      opts.scanner_opts.metadata.timestamp = val;
    } else {
      try {
        auto tp = parse_time_point(opts.timestamp);
        opts.scanner_opts.metadata.timestamp =
            std::chrono::duration_cast<std::chrono::seconds>(
                tp.time_since_epoch())
                .count();
      } catch (std::exception const& e) {
        iol.err << "error: invalid timestamp: " << e.what() << "\n";
        return 1;
      }
    }
  }

  // Process time-resolution option
  if (vm.contains("time-resolution")) {
    try {
      auto const res = parse_time_with_unit(opts.time_resolution);
      if (res.count() == 0) {
        iol.err << "error: the argument to '--time-resolution' must be nonzero\n";
        return 1;
      }
      opts.scanner_opts.metadata.time_resolution = res;
    } catch (std::exception const& e) {
      iol.err << "error: the argument ('" << opts.time_resolution
              << "') to '--time-resolution' is invalid (" << e.what() << ")\n";
      return 1;
    }
  }

  // Process pack-metadata option
  if (!opts.pack_metadata.empty() && opts.pack_metadata != "none") {
    if (opts.pack_metadata == "auto") {
      opts.scanner_opts.metadata.force_pack_string_tables = false;
      opts.scanner_opts.metadata.pack_chunk_table = false;
      opts.scanner_opts.metadata.pack_directories = false;
      opts.scanner_opts.metadata.pack_shared_files_table = false;
      opts.scanner_opts.metadata.pack_names = true;
      opts.scanner_opts.metadata.pack_names_index = false;
      opts.scanner_opts.metadata.pack_symlinks = true;
      opts.scanner_opts.metadata.pack_symlinks_index = false;
    } else {
      auto pack_opts =
          split_to<std::vector<std::string_view>>(opts.pack_metadata, ',');
      for (auto const& opt : pack_opts) {
        if (opt == "chunk_table") {
          opts.scanner_opts.metadata.pack_chunk_table = true;
        } else if (opt == "directories") {
          opts.scanner_opts.metadata.pack_directories = true;
        } else if (opt == "shared_files") {
          opts.scanner_opts.metadata.pack_shared_files_table = true;
        } else if (opt == "names") {
          opts.scanner_opts.metadata.pack_names = true;
        } else if (opt == "names_index") {
          opts.scanner_opts.metadata.pack_names_index = true;
        } else if (opt == "symlinks") {
          opts.scanner_opts.metadata.pack_symlinks = true;
        } else if (opt == "symlinks_index") {
          opts.scanner_opts.metadata.pack_symlinks_index = true;
        } else if (opt == "force") {
          opts.scanner_opts.metadata.force_pack_string_tables = true;
        } else if (opt == "plain") {
          opts.scanner_opts.metadata.plain_names_table = true;
          opts.scanner_opts.metadata.plain_symlinks_table = true;
        } else if (opt == "all") {
          opts.scanner_opts.metadata.pack_chunk_table = true;
          opts.scanner_opts.metadata.pack_directories = true;
          opts.scanner_opts.metadata.pack_shared_files_table = true;
          opts.scanner_opts.metadata.pack_names = true;
          opts.scanner_opts.metadata.pack_names_index = true;
          opts.scanner_opts.metadata.pack_symlinks = true;
          opts.scanner_opts.metadata.pack_symlinks_index = true;
        } else {
          iol.err << "error: the argument ('" << opt
                  << "') to '--pack-metadata' is invalid\n";
          return 1;
        }
      }
    }
  }

  return 0;
}

void options_parser::validate_level(parsed_options const& opts) {
  constexpr unsigned max_level = 9;
  if (opts.level > max_level) {
    throw std::runtime_error("invalid compression level");
  }
}

void options_parser::validate_block_size(parsed_options const& opts) {
  if (opts.segmenter_config.block_size_bits < min_block_size_bits ||
      opts.segmenter_config.block_size_bits > max_block_size_bits) {
    throw std::runtime_error(
        fmt::format("block size must be between {} and {}",
                    min_block_size_bits, max_block_size_bits));
  }
}

void options_parser::validate_paths(parsed_options const& opts,
                                    iolayer const& iol) {
  // Validate input path exists
  if (!opts.is_recompress) {
    std::error_code ec;
    if (!std::filesystem::exists(opts.input_path, ec)) {
      throw std::runtime_error(
          fmt::format("input path does not exist: {}", opts.input_path.string()));
    }
  }
}

void options_parser::validate_recompress_requirements(parsed_options const& opts) {
#ifndef DWARFS_HAVE_THRIFT
  if (opts.is_recompress || opts.rebuild_metadata || opts.change_block_size) {
    throw std::runtime_error(
        "recompress functionality requires Thrift support\n"
        "This build was compiled without Thrift (DWARFS_WITH_THRIFT=OFF)\n"
        "Recompressing existing images requires Thrift because the rewrite\n"
        "implementation depends on Thrift-specific metadata APIs.\n"
        "\n"
        "To use recompress features, rebuild with DWARFS_WITH_THRIFT=ON");
  }
#endif
}

} // namespace dwarfs::tool::mkdwarfs