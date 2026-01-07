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

#include <dwarfs/tool/dwarfsextract/argtable3_options_parser.h>

#include <stdexcept>
#include <string>

#include <dwarfs/config.h>

#include <argtable3.h>
#include <fmt/format.h>

#include <dwarfs/conv.h>
#include <dwarfs/error.h>
#include <dwarfs/util.h>

namespace dwarfs::tool::dwarfsextract {

namespace {

using namespace std::string_view_literals;

} // anonymous namespace

argtable3_options_parser::argtable3_options_parser() = default;

argtable3_options_parser::~argtable3_options_parser() = default;

int argtable3_options_parser::parse(int argc, char** argv) {
  // Set defaults
  opts_.num_workers = 4;
  opts_.cache_size = "512m";
  opts_.image_offset = "auto";
  opts_.repeat_count = 1;

  // Initialize argtable (estimate ~25 options)
  init_argtable(35);

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

  // Validate options
  if (!validate_options()) {
    return 2; // Validation error
  }

  return 0; // Success
}

void argtable3_options_parser::define_tool_options() {
  // Input/Output options
  input_opt_ = arg_file0("i", "input", "<file>", "input filesystem file");
  output_opt_ = arg_file0("o", "output", "<path>", "output file or directory");
  pattern_opt_ = arg_strn(nullptr, "pattern", "<pattern>", 0, 100,
                          "only extract files matching these patterns");

  // Filesystem options
  image_offset_opt_ =
      arg_str0("O", "image-offset", "<offset>",
               "filesystem image offset in bytes");
  cache_size_opt_ =
      arg_str0("s", "cache-size", "<size>", "block cache size");
  num_workers_opt_ =
      arg_int0("n", "num-workers", "<count>", "number of worker threads");

  // Archive format options
#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
  format_opt_ = arg_str0("f", "format", "<format>", "output format");
  format_options_opt_ =
      arg_str0(nullptr, "format-options", "<options>",
               "options for the specific libarchive format/filters");
  format_filters_opt_ =
      arg_str0(nullptr, "format-filters", "<filters>",
               "comma-separated libarchive format filters");
#endif

  // Extraction options
  continue_on_error_opt_ =
      arg_lit0(nullptr, "continue-on-error",
               "continue if errors are encountered");
  disable_integrity_check_opt_ =
      arg_lit0(nullptr, "disable-integrity-check",
               "disable file system image block integrity check (dangerous)");
  stdout_progress_opt_ =
      arg_lit0(nullptr, "stdout-progress", "write percentage progress to stdout");

  // Benchmark options
  benchmark_mode_opt_ =
      arg_lit0(nullptr, "benchmark-mode",
               "enable benchmark mode with detailed metrics");
  output_json_opt_ =
      arg_str0(nullptr, "output-json", "<file>",
               "output benchmark metrics to JSON file");
  repeat_opt_ =
      arg_int0(nullptr, "repeat", "<count>",
               "number of times to repeat extraction for averaging");

  // Performance monitoring
#if DWARFS_PERFMON_ENABLED
  perfmon_opt_ =
      arg_str0(nullptr, "perfmon", "<options>", "enable performance monitor");
  perfmon_trace_opt_ =
      arg_file0(nullptr, "perfmon-trace", "<file>",
                "write performance monitor trace file");
#endif

  // Add all options to argtable
  argtable_.push_back(input_opt_);
  argtable_.push_back(output_opt_);
  argtable_.push_back(pattern_opt_);
  argtable_.push_back(image_offset_opt_);
  argtable_.push_back(cache_size_opt_);
  argtable_.push_back(num_workers_opt_);
#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
  argtable_.push_back(format_opt_);
  argtable_.push_back(format_options_opt_);
  argtable_.push_back(format_filters_opt_);
#endif
  argtable_.push_back(continue_on_error_opt_);
  argtable_.push_back(disable_integrity_check_opt_);
  argtable_.push_back(stdout_progress_opt_);
  argtable_.push_back(benchmark_mode_opt_);
  argtable_.push_back(output_json_opt_);
  argtable_.push_back(repeat_opt_);
#if DWARFS_PERFMON_ENABLED
  argtable_.push_back(perfmon_opt_);
  argtable_.push_back(perfmon_trace_opt_);
#endif
}

void argtable3_options_parser::populate_parsed_options() {
  // Input/Output paths
  if (input_opt_->count > 0) {
    opts_.input = input_opt_->filename[0];
  }
  if (output_opt_->count > 0) {
    opts_.output = output_opt_->filename[0];
  }
  if (pattern_opt_->count > 0) {
    for (int i = 0; i < pattern_opt_->count; ++i) {
      opts_.patterns.emplace_back(pattern_opt_->sval[i]);
    }
  }

  // Filesystem options
  if (image_offset_opt_->count > 0) {
    opts_.image_offset = image_offset_opt_->sval[0];
  }
  if (cache_size_opt_->count > 0) {
    opts_.cache_size = cache_size_opt_->sval[0];
  }
  if (num_workers_opt_->count > 0) {
    opts_.num_workers = static_cast<size_t>(num_workers_opt_->ival[0]);
  }

  // Archive format options
#ifndef DWARFS_FILESYSTEM_EXTRACTOR_NO_OPEN_FORMAT
  if (format_opt_->count > 0) {
    opts_.format_name = format_opt_->sval[0];
  }
  if (format_options_opt_->count > 0) {
    opts_.format_options = format_options_opt_->sval[0];
  }
  if (format_filters_opt_->count > 0) {
    opts_.format_filters = format_filters_opt_->sval[0];
  }
#endif

  // Extraction options
  opts_.continue_on_error = continue_on_error_opt_->count > 0;
  opts_.disable_integrity_check = disable_integrity_check_opt_->count > 0;
  opts_.stdout_progress = stdout_progress_opt_->count > 0;

  // Benchmark options
  opts_.benchmark_mode = benchmark_mode_opt_->count > 0;
  if (output_json_opt_->count > 0) {
    opts_.output_json = output_json_opt_->sval[0];
  }
  if (repeat_opt_->count > 0) {
    opts_.repeat_count = static_cast<size_t>(repeat_opt_->ival[0]);
  }

  // Performance monitoring
#if DWARFS_PERFMON_ENABLED
  if (perfmon_opt_->count > 0) {
    opts_.perfmon = perfmon_opt_->sval[0];
  }
  if (perfmon_trace_opt_->count > 0) {
    opts_.perfmon_trace_file = perfmon_trace_opt_->filename[0];
  }
#endif
}

bool argtable3_options_parser::validate_options() {
  try {
    // Validate input is provided
    if (opts_.input.empty()) {
      throw std::runtime_error("input filesystem is required (use -i or --input)");
    }

    // Validate benchmark options
    if (!opts_.output_json.empty() && !opts_.benchmark_mode) {
      throw std::runtime_error("--output-json requires --benchmark-mode");
    }

    if (opts_.repeat_count > 1 && !opts_.benchmark_mode) {
      throw std::runtime_error("--repeat requires --benchmark-mode");
    }

    if (opts_.benchmark_mode && opts_.stdout_progress) {
      throw std::runtime_error("cannot use --stdout-progress with --benchmark-mode");
    }

  } catch (std::exception const& e) {
    fmt::print(stderr, "error: {}\n", e.what());
    return false;
  }
  return true;
}

void argtable3_options_parser::load_environment_variables() {
  // Load base environment variables (DWARFS_LOG_LEVEL, etc.)
  argtable3_base_parser::load_environment_variables("DWARFSEXTRACT");

  // Load tool-specific environment variables
  // Format: DWARFS_DWARFSEXTRACT_<OPTION>
  std::string prefix = "DWARFS_DWARFSEXTRACT_";

  // Example: DWARFS_DWARFSEXTRACT_CACHE_SIZE
  if (auto env_val = get_env_var(prefix + "CACHE_SIZE"); !env_val.empty()) {
    if (cache_size_opt_->count == 0) { // CLI takes priority
      opts_.cache_size = env_val;
    }
  }

  // Example: DWARFS_DWARFSEXTRACT_NUM_WORKERS
  if (auto env_val = get_env_var(prefix + "NUM_WORKERS"); !env_val.empty()) {
    if (num_workers_opt_->count == 0) {
      if (auto val = try_to<size_t>(env_val)) {
        opts_.num_workers = *val;
      }
    }
  }

  // Example: DWARFS_DWARFSEXTRACT_IMAGE_OFFSET
  if (auto env_val = get_env_var(prefix + "IMAGE_OFFSET"); !env_val.empty()) {
    if (image_offset_opt_->count == 0) {
      opts_.image_offset = env_val;
    }
  }

  // Example: DWARFS_DWARFSEXTRACT_OUTPUT
  if (auto env_val = get_env_var(prefix + "OUTPUT"); !env_val.empty()) {
    if (output_opt_->count == 0) {
      opts_.output = env_val;
    }
  }

  // Boolean flags
  if (auto env_val = get_env_var(prefix + "CONTINUE_ON_ERROR"); !env_val.empty()) {
    if (continue_on_error_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.continue_on_error = true;
    }
  }

  if (auto env_val = get_env_var(prefix + "DISABLE_INTEGRITY_CHECK"); !env_val.empty()) {
    if (disable_integrity_check_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.disable_integrity_check = true;
    }
  }

  if (auto env_val = get_env_var(prefix + "STDOUT_PROGRESS"); !env_val.empty()) {
    if (stdout_progress_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.stdout_progress = true;
    }
  }

  if (auto env_val = get_env_var(prefix + "BENCHMARK_MODE"); !env_val.empty()) {
    if (benchmark_mode_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.benchmark_mode = true;
    }
  }

  // Following MECE principle: CLI > ENV > defaults
}

} // namespace dwarfs::tool::dwarfsextract