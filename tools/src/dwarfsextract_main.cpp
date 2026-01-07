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

#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include <dwarfs/config.h>
#include <dwarfs/decompressor_registry.h>
#include <dwarfs/glob_matcher.h>
#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/performance_monitor.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>
#include <dwarfs/string.h>
#include <dwarfs/tool/dwarfsextract/argtable3_options_parser.h>
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/tool/tool.h>
#include <dwarfs/util.h>
#include <dwarfs/utility/filesystem_extractor.h>
#include <dwarfs_tool_main.h>
#include <dwarfs_tool_manpage.h>

namespace dwarfs::tool {

namespace {

#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
#ifdef _WIN32
constexpr std::wstring_view kDash{L"-"};
#else
constexpr std::string_view kDash{"-"};
#endif
#endif

} // namespace

int dwarfsextract_main(int argc, sys_char** argv, iolayer const& iol) {
  int rv = 0;

  try {
    dwarfsextract::argtable3_options_parser opt_parser;

#ifdef DWARFS_BUILTIN_MANPAGE
    // Wire up manpage for --man flag
    opt_parser.set_manpage_context(manpage::get_dwarfsextract_manpage(), iol);
#endif

    // Parse arguments
    int parse_result = opt_parser.parse(argc, argv);
    if (parse_result != 0) {
      return parse_result == 1 ? 0 : 1; // 1=help/version shown, 2=error
    }

    // Load environment variables (after CLI parsing, before use)
    opt_parser.load_environment_variables();

    // Get parsed options
    auto const& opts_ = opt_parser.get_parsed_options();

    // Create matcher from patterns
    std::unique_ptr<glob_matcher> matcher;
    if (!opts_.patterns.empty()) {
      matcher = std::make_unique<glob_matcher>(opts_.patterns);
    }

    // Create logger from base parser's logger options
    auto logopts = opt_parser.get_logger_options();
    stream_logger lgr(iol.term, iol.err, logopts);

    // Setup filesystem options
    reader::filesystem_options fsopts;
    fsopts.image_offset = reader::parse_image_offset(opts_.image_offset);
    fsopts.block_cache.max_bytes = parse_size_with_unit(opts_.cache_size);
    fsopts.block_cache.num_workers = opts_.num_workers;
    fsopts.block_cache.disable_block_integrity_check = opts_.disable_integrity_check;

    // Performance monitoring setup
    std::shared_ptr<performance_monitor> perfmon;

#if DWARFS_PERFMON_ENABLED
    std::unordered_set<std::string> perfmon_enabled;
    std::optional<std::filesystem::path> perfmon_trace_file;

    if (!opts_.perfmon.empty()) {
      split_to(opts_.perfmon, ',', perfmon_enabled);
    }

    if (!opts_.perfmon_trace_file.empty()) {
      perfmon_trace_file = iol.os->canonical(opts_.perfmon_trace_file);
    }

    perfmon = performance_monitor::create(perfmon_enabled, iol.file,
                                          perfmon_trace_file);
#endif

    // Create filesystem and extractor
    reader::filesystem_v2_lite fs(lgr, *iol.os, opts_.input, fsopts, perfmon);
    utility::filesystem_extractor fsx(lgr, *iol.os, iol.file);

    if (opts_.benchmark_mode) {
      fsx.enable_metrics(true);
    }

#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
    if (opts_.format_name.empty()) {
#endif
      // Create output directory first if it doesn't exist
      if (!opts_.output.empty()) {
        std::filesystem::create_directories(opts_.output);
        fsx.open_disk(iol.os->canonical(opts_.output));
      } else {
        fsx.open_disk(iol.os->canonical(std::filesystem::current_path()));
      }
#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
    } else {
      std::ostream* stream{nullptr};
      utility::filesystem_extractor_archive_format format;
      format.name = opts_.format_name;
      format.options = opts_.format_options;

      if (opts_.output.empty() || opts_.output == kDash) {
        if (opts_.stdout_progress) {
          DWARFS_THROW(runtime_error,
                       "cannot use --stdout-progress with --output=-");
        }

        if (&iol.out == &std::cout) {
          // No output specified, use stdout
        } else {
          stream = &iol.out;
        }
      }

      if (!opts_.format_filters.empty()) {
        split_to(opts_.format_filters, ',', format.filters);
      }

      if (stream) {
        fsx.open_stream(*stream, format);
      } else if (!opts_.output.empty()) {
        fsx.open_archive(iol.os->canonical(opts_.output), format);
      } else {
        fsx.open_archive({}, format); // stdout
      }
    }
#endif

    // Setup extractor options
    utility::filesystem_extractor_options fsx_opts;
    fsx_opts.max_queued_bytes = fsopts.block_cache.max_bytes;
    fsx_opts.continue_on_error = opts_.continue_on_error;
    int prog{-1};
    if (opts_.stdout_progress) {
      fsx_opts.progress = [&prog, &iol](std::string_view, uint64_t extracted,
                                        uint64_t total) {
        int p = 100 * extracted / total;
        if (p > prog) {
          prog = p;
          iol.out << "\r" << prog << "%";
          iol.out.flush();
        }
        if (extracted == total) {
          iol.out << "\n";
        }
      };
    }

    // Run extraction (possibly multiple times for benchmarking)
    std::vector<utility::extraction_metrics> all_metrics;
    all_metrics.reserve(opts_.repeat_count);

    for (size_t i = 0; i < opts_.repeat_count; ++i) {
      if (opts_.benchmark_mode && opts_.repeat_count > 1) {
        iol.out << "Run " << (i + 1) << "/" << opts_.repeat_count << "...\n";
      }

      fsx.reset_metrics();
      bool success = fsx.extract(fs, matcher.get(), fsx_opts);

      if (opts_.benchmark_mode) {
        all_metrics.push_back(fsx.get_metrics());
      }

      if (!success) {
        rv = 2;
        if (i + 1 < opts_.repeat_count) {
          iol.err << "Extraction failed on run " << (i + 1)
                  << ", aborting remaining runs\n";
          break;
        }
      }
    }

    fsx.close();

    // Output benchmark results
    if (opts_.benchmark_mode && !all_metrics.empty()) {
      auto const& m = all_metrics[0];

      iol.out << "\n=== Benchmark Results ===\n";
      iol.out << fmt::format("Metadata load time: {:.3f} ms\n",
                            m.metadata_load_time.count() / 1000.0);
      iol.out << fmt::format("Extraction time: {:.3f} ms\n",
                            m.extraction_time.count() / 1000.0);
      iol.out << fmt::format("Bytes extracted: {} ({:.2f} MiB)\n",
                            m.bytes_extracted,
                            m.bytes_extracted / (1024.0 * 1024.0));
      iol.out << fmt::format("Files extracted: {}\n", m.files_extracted);
      iol.out << fmt::format("Directories extracted: {}\n",
                            m.directories_extracted);
      iol.out << fmt::format("Symlinks extracted: {}\n", m.symlinks_extracted);
      iol.out << fmt::format("Hard errors: {}\n", m.hard_errors);
      iol.out << fmt::format("Soft errors: {}\n", m.soft_errors);

      if (opts_.repeat_count > 1) {
        // Calculate averages
        double avg_metadata_load = 0, avg_extraction = 0;
        for (auto const& met : all_metrics) {
          avg_metadata_load += met.metadata_load_time.count();
          avg_extraction += met.extraction_time.count();
        }
        avg_metadata_load /= all_metrics.size() * 1000.0;
        avg_extraction /= all_metrics.size() * 1000.0;

        iol.out << "\n=== Averages over " << opts_.repeat_count << " runs ===\n";
        iol.out << fmt::format("Avg metadata load: {:.3f} ms\n",
                              avg_metadata_load);
        iol.out << fmt::format("Avg extraction: {:.3f} ms\n", avg_extraction);
      }

      // Export to JSON if requested
      if (!opts_.output_json.empty()) {
        std::ofstream json_out(opts_.output_json);
        if (!json_out) {
          iol.err << "error: could not open " << opts_.output_json << " for writing\n";
          return 1;
        }

        json_out << "{\n";
        json_out << fmt::format("  \"image\": \"{}\",\n",
                               tool::sys_string_to_string(opts_.input));
        json_out << fmt::format("  \"repeat_count\": {},\n", opts_.repeat_count);
        json_out << "  \"runs\": [\n";

        for (size_t i = 0; i < all_metrics.size(); ++i) {
          auto const& met = all_metrics[i];
          json_out << "    {\n";
          json_out << fmt::format("      \"metadata_load_us\": {},\n",
                                 met.metadata_load_time.count());
          json_out << fmt::format("      \"extraction_time_us\": {},\n",
                                 met.extraction_time.count());
          json_out << fmt::format("      \"bytes_extracted\": {},\n",
                                 met.bytes_extracted);
          json_out << fmt::format("      \"files_extracted\": {},\n",
                                 met.files_extracted);
          json_out << fmt::format("      \"directories_extracted\": {},\n",
                                 met.directories_extracted);
          json_out << fmt::format("      \"symlinks_extracted\": {},\n",
                                 met.symlinks_extracted);
          json_out << fmt::format("      \"hard_errors\": {},\n",
                                 met.hard_errors);
          json_out << fmt::format("      \"soft_errors\": {}\n",
                                 met.soft_errors);
          json_out << "    }";
          if (i + 1 < all_metrics.size()) {
            json_out << ",";
          }
          json_out << "\n";
        }

        json_out << "  ]\n";
        json_out << "}\n";

        iol.out << "Benchmark results written to " << opts_.output_json << "\n";
      }
    }

    if (perfmon) {
      perfmon->summarize(iol.err);
    }
  } catch (std::exception const& e) {
    iol.err << exception_str(e) << "\n";
    return 1;
  }

  return rv;
}

} // namespace dwarfs::tool