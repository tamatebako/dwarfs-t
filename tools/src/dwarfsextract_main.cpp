/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
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

#include <boost/program_options.hpp>
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
#include <dwarfs/tool/iolayer.h>
#include <dwarfs/tool/program_options_helpers.h>
#include <dwarfs/tool/tool.h>
#include <dwarfs/util.h>
#include <dwarfs/utility/filesystem_extractor.h>
#include <dwarfs_tool_main.h>
#include <dwarfs_tool_manpage.h>

namespace po = boost::program_options;

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
  sys_string fs_image, output, trace_file;
  std::string cache_size_str, image_offset, output_json;
  logger_options logopts;
#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
  utility::filesystem_extractor_archive_format format;
  std::string format_filters;
#endif
#if DWARFS_PERFMON_ENABLED
  std::string perfmon_str;
#endif
  size_t num_workers, repeat_count{1};
  bool continue_on_error{false}, disable_integrity_check{false},
      stdout_progress{false}, benchmark_mode{false};

  // clang-format off
  po::options_description opts("Command line options");
  opts.add_options()
    ("input,i",
        po_sys_value<sys_string>(&fs_image),
        "input filesystem file")
    ("output,o",
        po_sys_value<sys_string>(&output),
        "output file or directory")
    ("pattern",
        po::value<std::vector<std::string>>(),
        "only extract files matching these patterns")
    ("image-offset,O",
        po::value<std::string>(&image_offset)->default_value("auto"),
        "filesystem image offset in bytes")
#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
    ("format,f",
        po::value<std::string>(&format.name),
        "output format")
    ("format-filters",
        po::value<std::string>(&format_filters),
        "comma-separated libarchive format filters")
    ("format-options",
        po::value<std::string>(&format.options),
        "options for the specific libarchive format/filters")
#endif
    ("continue-on-error",
        po::value<bool>(&continue_on_error)->zero_tokens(),
        "continue if errors are encountered")
    ("disable-integrity-check",
        po::value<bool>(&disable_integrity_check)->zero_tokens(),
        "disable file system image block integrity check (dangerous)")
    ("stdout-progress",
        po::value<bool>(&stdout_progress)->zero_tokens(),
        "write percentage progress to stdout")
    ("num-workers,n",
        po::value<size_t>(&num_workers)->default_value(4),
        "number of worker threads")
    ("cache-size,s",
        po::value<std::string>(&cache_size_str)->default_value("512m"),
        "block cache size")
    ("benchmark-mode",
        po::value<bool>(&benchmark_mode)->zero_tokens(),
        "enable benchmark mode with detailed metrics")
    ("output-json",
        po::value<std::string>(&output_json),
        "output benchmark metrics to JSON file")
    ("repeat",
        po::value<size_t>(&repeat_count)->default_value(1),
        "number of times to repeat extraction for averaging")
#if DWARFS_PERFMON_ENABLED
    ("perfmon",
        po::value<std::string>(&perfmon_str),
        "enable performance monitor")
    ("perfmon-trace",
        po_sys_value<sys_string>(&trace_file),
        "write performance monitor trace file")
#endif
    ;
  // clang-format on

  tool::add_common_options(opts, logopts);

  po::positional_options_description pos;
  pos.add("pattern", -1);

  po::variables_map vm;

  try {
    po::store(po::basic_command_line_parser<sys_char>(argc, argv)
                  .options(opts)
                  .positional(pos)
                  .run(),
              vm);
    po::notify(vm);
  } catch (po::error const& e) {
    iol.err << "error: " << e.what() << "\n";
    return 1;
  }

#ifdef DWARFS_BUILTIN_MANPAGE
  if (vm.contains("man")) {
    tool::show_manpage(tool::manpage::get_dwarfsextract_manpage(), iol);
    return 0;
  }
#endif

  auto constexpr usage = "Usage: dwarfsextract [OPTIONS...]\n";

  if (vm.contains("help") or !vm.contains("input")) {
    auto extra_deps = [](library_dependencies& deps) {
      utility::filesystem_extractor::add_library_dependencies(deps);
      decompressor_registry::instance().add_library_dependencies(deps);
    };

    iol.out << tool::tool_header("dwarfsextract", extra_deps) << usage << "\n"
            << opts << "\n";
    return 0;
  }

  std::unique_ptr<glob_matcher> matcher;

  if (vm.contains("pattern")) {
    matcher = std::make_unique<glob_matcher>(
        vm["pattern"].as<std::vector<std::string>>());
  }

  int rv = 0;

  // Validate benchmark options
  if (!output_json.empty() && !benchmark_mode) {
    iol.err << "error: --output-json requires --benchmark-mode\n";
    return 1;
  }

  if (repeat_count > 1 && !benchmark_mode) {
    iol.err << "error: --repeat requires --benchmark-mode\n";
    return 1;
  }

  if (benchmark_mode && stdout_progress) {
    iol.err << "error: cannot use --stdout-progress with --benchmark-mode\n";
    return 1;
  }

  try {
    stream_logger lgr(iol.term, iol.err, logopts);
    reader::filesystem_options fsopts;

    fsopts.image_offset = reader::parse_image_offset(image_offset);
    fsopts.block_cache.max_bytes = parse_size_with_unit(cache_size_str);
    fsopts.block_cache.num_workers = num_workers;
    fsopts.block_cache.disable_block_integrity_check = disable_integrity_check;

    std::shared_ptr<performance_monitor> perfmon;

#if DWARFS_PERFMON_ENABLED
    std::unordered_set<std::string> perfmon_enabled;
    std::optional<std::filesystem::path> perfmon_trace_file;

    if (!perfmon_str.empty()) {
      split_to(perfmon_str, ',', perfmon_enabled);
    }

    if (!trace_file.empty()) {
      perfmon_trace_file = iol.os->canonical(trace_file);
    }

    perfmon = performance_monitor::create(perfmon_enabled, iol.file,
                                          perfmon_trace_file);
#endif

    reader::filesystem_v2_lite fs(lgr, *iol.os, fs_image, fsopts, perfmon);
    utility::filesystem_extractor fsx(lgr, *iol.os, iol.file);

    if (benchmark_mode) {
      fsx.enable_metrics(true);
    }

#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
    if (format.name.empty()) {
#endif
      // Create output directory first if it doesn't exist
      if (!output.empty()) {
        std::filesystem::create_directories(output);
        fsx.open_disk(iol.os->canonical(output));
      } else {
        fsx.open_disk(iol.os->canonical(std::filesystem::current_path()));
      }
#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
    } else {
      std::ostream* stream{nullptr};

      if (output.empty() or output == kDash) {
        if (stdout_progress) {
          DWARFS_THROW(runtime_error,
                       "cannot use --stdout-progress with --output=-");
        }

        if (&iol.out == &std::cout) {
          output.clear();
        } else {
          stream = &iol.out;
        }
      }

      split_to(format_filters, ',', format.filters);

      if (stream) {
        fsx.open_stream(*stream, format);
      } else {
        fsx.open_archive(iol.os->canonical(output), format);
      }
    }
#endif

    utility::filesystem_extractor_options fsx_opts;

    fsx_opts.max_queued_bytes = fsopts.block_cache.max_bytes;
    fsx_opts.continue_on_error = continue_on_error;
    int prog{-1};
    if (stdout_progress) {
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
    all_metrics.reserve(repeat_count);

    for (size_t i = 0; i < repeat_count; ++i) {
      if (benchmark_mode && repeat_count > 1) {
        iol.out << "Run " << (i + 1) << "/" << repeat_count << "...\n";
      }

      fsx.reset_metrics();
      bool success = fsx.extract(fs, matcher.get(), fsx_opts);

      if (benchmark_mode) {
        all_metrics.push_back(fsx.get_metrics());
      }

      if (!success) {
        rv = 2;
        if (i + 1 < repeat_count) {
          iol.err << "Extraction failed on run " << (i + 1)
                  << ", aborting remaining runs\n";
          break;
        }
      }
    }

    fsx.close();

    // Output benchmark results
    if (benchmark_mode && !all_metrics.empty()) {
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

      if (repeat_count > 1) {
        // Calculate averages
        double avg_metadata_load = 0, avg_extraction = 0;
        for (auto const& met : all_metrics) {
          avg_metadata_load += met.metadata_load_time.count();
          avg_extraction += met.extraction_time.count();
        }
        avg_metadata_load /= all_metrics.size() * 1000.0;
        avg_extraction /= all_metrics.size() * 1000.0;

        iol.out << "\n=== Averages over " << repeat_count << " runs ===\n";
        iol.out << fmt::format("Avg metadata load: {:.3f} ms\n",
                              avg_metadata_load);
        iol.out << fmt::format("Avg extraction: {:.3f} ms\n", avg_extraction);
      }

      // Export to JSON if requested
      if (!output_json.empty()) {
        std::ofstream json_out(output_json);
        if (!json_out) {
          iol.err << "error: could not open " << output_json << " for writing\n";
          return 1;
        }

        json_out << "{\n";
#ifdef _WIN32
        json_out << fmt::format("  \"image\": \"{}\",\n",
                               wstring_to_utf8(fs_image));
#else
        json_out << fmt::format("  \"image\": \"{}\",\n", fs_image);
#endif
        json_out << fmt::format("  \"repeat_count\": {},\n", repeat_count);
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

        iol.out << "Benchmark results written to " << output_json << "\n";
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
