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

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#ifdef _WIN32
#include <io.h>
#endif

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>

#include <fmt/format.h>
#if FMT_VERSION >= 110000
#include <fmt/ranges.h>
#endif

#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/map.hpp>

#include <dwarfs/binary_literals.h>
#include <dwarfs/block_compressor.h>
#include <dwarfs/block_compressor_parser.h>
#include <dwarfs/checksum.h>
#include <dwarfs/compressor_registry.h>
#include <dwarfs/config.h>
#include <dwarfs/conv.h>
#include <dwarfs/metadata/serialization/serialization_format.h>
#include <dwarfs/decompressor_registry.h>
#include <dwarfs/error.h>
#include <dwarfs/file_access.h>
#include <dwarfs/integral_value_parser.h>
#include <dwarfs/logger.h>
#include <dwarfs/match.h>
#include <dwarfs/os_access.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/sorted_array_map.h>
#include <dwarfs/string.h>
#include <dwarfs/terminal.h>
#include <dwarfs/thread_pool.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/tool/program_options_helpers.h>
#include <dwarfs/tool/sysinfo.h>
#include <dwarfs/tool/tool.h>
#include <dwarfs/util.h>
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
#include <dwarfs/utility/rewrite_filesystem.h>
#include <dwarfs/utility/rewrite_options.h>
#endif
#include <dwarfs/writer/categorizer.h>
#include <dwarfs/writer/category_parser.h>
#include <dwarfs/writer/console_writer.h>
#include <dwarfs/writer/entry_factory.h>
#include <dwarfs/writer/filesystem_block_category_resolver.h>
#include <dwarfs/writer/filesystem_writer.h>
#include <dwarfs/writer/filesystem_writer_options.h>
#include <dwarfs/writer/filter_debug.h>
#include <dwarfs/writer/fragment_order_parser.h>
#include <dwarfs/writer/rule_based_entry_filter.h>
#include <dwarfs/writer/scanner.h>
#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>
#include <dwarfs/writer/writer_progress.h>
#include <dwarfs/tool/mkdwarfs/argtable3_options_parser.h>
#include <dwarfs/tool/mkdwarfs/create_handler.h>
#include <dwarfs/tool/mkdwarfs/handler_factory.h>
#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
#include <dwarfs/tool/mkdwarfs/recompress_handler.h>
#endif
#include <dwarfs_tool_main.h>
#include <dwarfs_tool_manpage.h>

namespace po = boost::program_options;

namespace dwarfs::tool {

namespace {

using namespace std::string_view_literals;
using namespace dwarfs::binary_literals;

constexpr sorted_array_map progress_modes{
    std::pair{"none"sv, writer::console_writer::NONE},
    std::pair{"simple"sv, writer::console_writer::SIMPLE},
    std::pair{"ascii"sv, writer::console_writer::ASCII},
    std::pair{"unicode"sv, writer::console_writer::UNICODE},
};

constexpr auto default_progress_mode = "unicode";

constexpr sorted_array_map debug_filter_modes{
    std::pair{"included"sv, writer::debug_filter_mode::INCLUDED},
    std::pair{"included-files"sv, writer::debug_filter_mode::INCLUDED_FILES},
    std::pair{"excluded"sv, writer::debug_filter_mode::EXCLUDED},
    std::pair{"excluded-files"sv, writer::debug_filter_mode::EXCLUDED_FILES},
    std::pair{"files"sv, writer::debug_filter_mode::FILES},
    std::pair{"all"sv, writer::debug_filter_mode::ALL},
};

constexpr size_t min_block_size_bits{10};
constexpr size_t max_block_size_bits{30};

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

using categorize_defaults_type =
    std::unordered_map<std::string, std::vector<std::string>>;

categorize_defaults_type const& categorize_defaults_common() {
  static categorize_defaults_type const defaults{
      // clang-format off
      {"--compression", {"incompressible::null"}},
      // clang-format on
  };

  return defaults;
}

categorize_defaults_type const& categorize_defaults_level(unsigned level) {
  static categorize_defaults_type const defaults_off;

  static categorize_defaults_type const defaults_fast{
      // clang-format off
      {"--order",       {"pcmaudio/waveform::revpath", "fits/image::revpath"}},
      {"--window-size", {"pcmaudio/waveform::0", "fits/image::0"}},
      {"--compression", {
#ifdef DWARFS_HAVE_FLAC
                         "pcmaudio/waveform::flac:level=3",
#else
                         "pcmaudio/waveform::zstd:level=3",
#endif
#ifdef DWARFS_HAVE_RICEPP
                         "fits/image::ricepp",
#else
                         "fits/image::zstd:level=3",
#endif
                        }},
      // clang-format on
  };

  static categorize_defaults_type const defaults_medium{
      // clang-format off
      {"--order",       {"pcmaudio/waveform::revpath", "fits/image::revpath"}},
      {"--window-size", {"pcmaudio/waveform::20", "fits/image::0"}},
      {"--compression", {
#ifdef DWARFS_HAVE_FLAC
                         "pcmaudio/waveform::flac:level=5",
#else
                         "pcmaudio/waveform::zstd:level=5",
#endif
#ifdef DWARFS_HAVE_RICEPP
                         "fits/image::ricepp",
#else
                         "fits/image::zstd:level=5",
#endif
                        }},
      // clang-format on
  };

  static categorize_defaults_type const defaults_slow{
      // clang-format off
      {"--order",       {"fits/image::revpath"}},
      {"--window-size", {"pcmaudio/waveform::16", "fits/image::0"}},
      {"--compression", {
#ifdef DWARFS_HAVE_FLAC
                         "pcmaudio/waveform::flac:level=8",
#else
                         "pcmaudio/waveform::zstd:level=8",
#endif
#ifdef DWARFS_HAVE_RICEPP
                         "fits/image::ricepp",
#else
                         "fits/image::zstd:level=8",
#endif
                        }},
      // clang-format on
  };

  static constexpr std::array<categorize_defaults_type const*, 10>
      defaults_level{{
          // clang-format off
          /* 0 */ &defaults_off,
          /* 1 */ &defaults_fast,
          /* 2 */ &defaults_fast,
          /* 3 */ &defaults_fast,
          /* 4 */ &defaults_fast,
          /* 5 */ &defaults_medium,
          /* 6 */ &defaults_medium,
          /* 7 */ &defaults_medium,
          /* 8 */ &defaults_slow,
          /* 9 */ &defaults_slow,
          // clang-format on
      }};

  return *defaults_level.at(level);
}

constexpr unsigned default_level = 7;

class categorize_optval {
 public:
  categorize_optval() = default;
  explicit categorize_optval(std::string const& val, bool expl = false)
      : value_{val}
      , is_explicit_{expl} {}

  bool empty() const { return value_.empty(); }
  std::string const& value() const { return value_; }

  bool is_explicit() const { return is_explicit_; }

  template <typename T>
  void add_implicit_defaults(T& cop) const {
    if (cop.has_category_resolver()) {
      if (auto it = defaults_.find(cop.name()); it != defaults_.end()) {
        for (auto const& v : it->second) {
          cop.parse_fallback(v);
        }
      }
    }
  }

  void add_defaults(categorize_defaults_type const& defaults) {
    for (auto const& [key, values] : defaults) {
      auto& vs = defaults_[key];
      vs.insert(vs.end(), values.begin(), values.end());
    }
  }

 private:
  categorize_defaults_type defaults_;
  std::string value_;
  bool is_explicit_{false};
};

std::ostream& operator<<(std::ostream& os, categorize_optval const& optval) {
  return os << optval.value() << (optval.is_explicit() ? " (explicit)" : "");
}

void validate(boost::any& v, std::vector<std::string> const& values,
              categorize_optval*, int) {
  po::validators::check_first_occurrence(v);
  v = categorize_optval{po::validators::get_single_string(values), true};
}

uint64_t
compute_memory_limit(uint64_t const block_size, uint64_t const num_cpu) {
  auto const sys_mem = std::max(tool::sysinfo::get_total_memory(), 256_MiB);
  auto wanted_mem = num_cpu * block_size;
  if (wanted_mem < sys_mem / 64) {
    wanted_mem = sys_mem / 64;
  } else {
    wanted_mem += std::min(num_cpu, UINT64_C(8)) * block_size;
  }
  return std::min(wanted_mem, sys_mem / 8);
}

metadata::serialization::SerializationFormat
get_format_from_string(std::string const& format_str) {
  using namespace metadata::serialization;

  if (format_str == "flatbuffers" || format_str == "flatbuffer") {
    // FlatBuffers is always available (REQUIRED)
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
    throw std::runtime_error(
        fmt::format("Metadata format '{}' is no longer supported. "
                    "Cereal and Bitsery formats were removed in v0.16.0. "
                    "Please use 'flatbuffers' (default/recommended), "
                    "'legacy-thrift' (Homebrew compatible, uses .dwarfs extension), or "
                    "'thrift' (experimental Modern Thrift fbthrift, uses .dftx extension) instead. "
                    "To convert existing images, use: mkdwarfs --recompress=metadata "
                    "--rebuild-metadata --format=flatbuffers -I old.dwarfs -O new.dwarfs",
                    format_str));
  } else {
    throw std::runtime_error(
        fmt::format("Unknown metadata format: '{}'. "
                    "Supported formats:\n"
                    "  flatbuffers - FlatBuffers format (.dff, stable, recommended default)\n"
                    "  legacy-thrift - Legacy Thrift/Frozen2 (.dwarfs, stable, Homebrew compatible)\n"
                    "  thrift - Modern Thrift CompactProtocol (.dftx, experimental, fbthrift)",
                    format_str));
  }
}

} // namespace

int mkdwarfs_main(int argc, sys_char** argv, iolayer const& iol) {
  using namespace std::chrono_literals;
  using namespace std::string_view_literals;
  using namespace dwarfs::binary_literals;

  [[maybe_unused]] size_t const num_cpu = std::max(hardware_concurrency(), 1U);
  static constexpr size_t const kDefaultMaxActiveBlocks{4};
  static constexpr size_t const kDefaultBloomFilterSize{4};

  writer::segmenter_factory::config sf_config;
  sys_string path_str, input_list_str, output_str, header_str;
  std::string memory_limit, schema_compression, metadata_compression, timestamp,
      time_resolution, progress_mode, recompress_opts, pack_metadata,
      file_hash_algo, debug_filter, max_similarity_size, chmod_str,
      history_compression, recompress_categories, metadata_format;
  std::vector<sys_string> filter;
  std::vector<std::string> order, max_lookback_blocks, window_size, window_step,
      bloom_filter_size, compression;
  size_t num_workers, num_scanner_workers, num_segmenter_workers;
  bool no_progress = false, remove_header = false, no_section_index = false,
       force_overwrite = false, no_history = false, no_sparse_files = false,
       no_history_timestamps = false, no_history_command_line = false,
       rebuild_metadata = false, change_block_size = false;
  unsigned level;
  int compress_niceness;
  uint16_t uid = 0, gid = 0;  // Initialize to safe defaults
  categorize_optval categorizer_list;

  // Parse command-line options using argtable3_options_parser
  mkdwarfs::argtable3_options_parser opt_parser;

#ifdef DWARFS_BUILTIN_MANPAGE
  // Wire up manpage for --man flag
  opt_parser.set_manpage_context(manpage::get_mkdwarfs_manpage(), iol);
#endif

  // Load environment variables before parsing
  opt_parser.load_environment_variables();

  if (auto rc = opt_parser.parse(argc, argv)) {
    // Parser handles help, version, man, errors, etc.
    return rc;
  }

  // Get parsed options (use reference to avoid copy but allow modification)
  auto& opts = opt_parser.get_parsed_options();

#ifdef DWARFS_BUILTIN_MANPAGE
  // Manpage handling is now in the parser
  // No need for separate check here since parser handles it
#endif

  // Map parsed options to local variables for compatibility
  path_str = opts.input_path.native();
  output_str = opts.output_path.native();
  if (!opts.header_path.empty()) {
    header_str = opts.header_path.native();
  }
  level = opts.level;
  sf_config = opts.segmenter_config;
  schema_compression = opts.schema_compression;
  metadata_compression = opts.metadata_compression;
  history_compression = opts.history_compression;
  compression = opts.compression;
  order = opts.order;
  max_lookback_blocks = opts.max_lookback_blocks;
  window_size = opts.window_size;
  window_step = opts.window_step;
  bloom_filter_size = opts.bloom_filter_size;
  categorizer_list = categorize_optval(opts.categorizer_list, opts.categorizer_explicit);
  max_similarity_size = opts.max_similarity_size;
  num_workers = opts.num_workers;
  num_scanner_workers = opts.num_scanner_workers;
  num_segmenter_workers = opts.num_segmenter_workers;
  compress_niceness = opts.compress_niceness;
  memory_limit = opts.memory_limit;
  file_hash_algo = opts.file_hash_algo;
  progress_mode = opts.progress_mode;
  no_progress = opts.no_progress;
  debug_filter = opts.debug_filter;
  filter = tool::vector_to_sys_strings(opts.filter_rules);
  force_overwrite = opts.force_overwrite;
  no_section_index = opts.no_section_index;
  no_history = opts.no_history;
  no_history_timestamps = opts.no_history_timestamps;
  no_history_command_line = opts.no_history_command_line;
  no_sparse_files = opts.no_sparse_files;
  if (opts.set_owner) {
    uid = *opts.set_owner;
  }
  if (opts.set_group) {
    gid = *opts.set_group;
  }
  chmod_str = opts.chmod_str;
  timestamp = opts.timestamp;
  time_resolution = opts.time_resolution;
  pack_metadata = opts.pack_metadata;
  metadata_format = opts.metadata_format;
  recompress_opts = opts.recompress_opts;
  recompress_categories = opts.recompress_categories;
  rebuild_metadata = opts.rebuild_metadata;
  change_block_size = opts.change_block_size;
  remove_header = opts.remove_header;

  // Runtime object creation - console writer, progress, filesystem writer
  // Use terminal from iolayer (already a shared_ptr)
  auto const& term = iol.term;

  // Setup console writer with progress mode
  writer::console_writer::options cw_opts;
  cw_opts.enable_sparse_files = !no_sparse_files;

  if (no_progress) {
    cw_opts.progress = writer::console_writer::NONE;
  } else {
    auto it = progress_modes.find(progress_mode);
    if (it == progress_modes.end()) {
      iol.err << "error: invalid progress mode: " << progress_mode << "\n";
      return 1;
    }
    cw_opts.progress = it->second;
  }

  logger_options logopts;
  writer::console_writer console(term, iol.err, cw_opts, logopts);

  // Setup memory limit
  auto memlim = compute_memory_limit(
      UINT64_C(1) << sf_config.block_size_bits, num_workers);

  if (memory_limit != "auto") {
    try {
      memlim = parse_size_with_unit(memory_limit);
    } catch (std::exception const& e) {
      iol.err << "error: invalid memory limit: " << memory_limit
            << " (" << e.what() << ")\n";
      return 1;
    }
  }

  console.set_memory_usage_function([&] {
    // Memory usage tracking callback
    return memlim;
  });

  // Create compression thread pool
  thread_pool pool(console, *iol.os, "compress", num_workers, compress_niceness);

  // Create writer progress
  writer::writer_progress prog([&](writer::writer_progress const& p, bool last) {
    console.update(const_cast<writer::writer_progress&>(p), last);
  }, 1000ms);

  // Setup filesystem writer options
  writer::filesystem_writer_options fsopts;
  fsopts.remove_header = remove_header;
  fsopts.no_section_index = no_section_index;

  // Open output stream
  std::unique_ptr<output_stream> output;
  std::ostream* os = nullptr;

  if (output_str == SYS_STR("-")) {
    os = &iol.out;
  } else {
    std::filesystem::path output_path(output_str);
    std::error_code ec;

    // Check if file exists before trying to write
    if (!force_overwrite && std::filesystem::exists(output_path, ec)) {
      iol.err << "error: output file exists: " << output_path << "\n";
      return 1;
    }

    output = iol.file->open_output(output_path, ec);
    if (ec) {
      iol.err << "error: cannot open output file: " << ec.message() << "\n";
      return 1;
    }
    os = &output->os();
  }

  // Open header stream if provided
  std::unique_ptr<input_stream> header_is;
  std::istream* header = nullptr;

  if (!header_str.empty()) {
    std::filesystem::path header_path(header_str);
    std::error_code ec;
    header_is = iol.file->open_input(header_path, ec);
    if (ec) {
      iol.err << "error: cannot open header file: " << ec.message() << "\n";
      return 1;
    }
    header = &header_is->is();
  }

  // Create filesystem writer
  writer::filesystem_writer fsw(*os, console, pool, prog, fsopts, header);

  // Add compressors
  block_compressor_parser compressor_parser;

  for (auto const& spec : compression) {
    auto bc = compressor_parser.parse(spec);
    fsw.add_default_compressor(std::move(bc));
  }

  // Add section compressors
  if (!schema_compression.empty()) {
    auto bc = compressor_parser.parse(schema_compression);
    fsw.add_section_compressor(section_type::METADATA_V2_SCHEMA, std::move(bc));
  }

  if (!metadata_compression.empty()) {
    auto bc = compressor_parser.parse(metadata_compression);
    fsw.add_section_compressor(section_type::METADATA_V2, std::move(bc));
  }

  if (!history_compression.empty()) {
    auto bc = compressor_parser.parse(history_compression);
    fsw.add_section_compressor(section_type::HISTORY, std::move(bc));
  }

  // Setup categorizer manager if needed
  if (!categorizer_list.empty()) {
    auto catmgr = std::make_shared<writer::categorizer_manager>(console, opts.input_path);
    opts.scanner_opts.inode.categorizer_mgr = catmgr;

    // Parse categorizer list and create categorizers
    writer::categorizer_registry catreg;
    auto cats = split_to<std::vector<std::string>>(categorizer_list.value(), ',');

    for (auto const& cat_name : cats) {
      auto cat = catreg.create(console, cat_name, {}, iol.file);
      if (cat) {
        catmgr->add(std::move(cat));
      }
    }
  }

  // Setup filter if rules provided
  std::unique_ptr<writer::rule_based_entry_filter> entry_filter;

  if (!filter.empty()) {
    entry_filter = std::make_unique<writer::rule_based_entry_filter>(console, iol.file);
    entry_filter->set_root_path(opts.input_path);

    for (auto const& rule : filter) {
      auto srule = sys_string_to_string(rule);
      try {
        entry_filter->add_rule(srule);
      } catch (std::exception const& e) {
        iol.err << "error: could not parse filter rule '" << srule
                << "': " << e.what() << "\n";
        return 1;
      }
    }
  }

  auto extra_deps = [&](library_dependencies& deps) {
    // Add extra dependencies if needed
  };

  // Create appropriate handler and execute
  auto handler = mkdwarfs::handler_factory::create(opts);
  return handler->run(opts, iol, console, prog, fsw, entry_filter.get(), extra_deps);
}

} // namespace dwarfs::tool
