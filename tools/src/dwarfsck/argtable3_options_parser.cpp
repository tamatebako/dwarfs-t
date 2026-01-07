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

#include <dwarfs/tool/dwarfsck/argtable3_options_parser.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <thread>

#include <argtable3.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <dwarfs/checksum.h>
#include <dwarfs/conv.h>
#include <dwarfs/error.h>
#include <dwarfs/reader/fsinfo_options.h>
#include <dwarfs/tool/sys_char.h>
#include <dwarfs/util.h>

namespace dwarfs::tool::dwarfsck {

namespace {

using namespace std::string_view_literals;

} // anonymous namespace

argtable3_options_parser::argtable3_options_parser() = default;

argtable3_options_parser::~argtable3_options_parser() = default;

int argtable3_options_parser::parse(int argc, char** argv) {
  // Set defaults
  opts_.num_workers = std::max(std::thread::hardware_concurrency(), 1U);
  opts_.cache_size = "512m";
  opts_.image_offset = "auto";
  opts_.detail = reader::fsinfo_features::for_level(2).to_string();

  // Initialize argtable (estimate ~20 options)
  init_argtable(30);

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
  input_opt_ = arg_file0("i", "input", "<path>", "input filesystem");
  export_metadata_opt_ =
      arg_file0(nullptr, "export-metadata", "<file>",
                "export raw metadata as JSON to file");

  // Operation mode options
  quiet_opt_ = arg_lit0("q", "quiet",
                        "don't print anything unless an error occurs");
  verbose_opt_ = arg_lit0("v", "verbose", "produce verbose output");
  output_json_opt_ =
      arg_lit0("j", "json", "print information in JSON format");
  print_header_opt_ =
      arg_lit0("H", "print-header", "print filesystem header to stdout and exit");
  list_files_opt_ = arg_lit0("l", "list", "list all files and exit");

  // Filesystem options
  auto algo_list = checksum::available_algorithms();
  auto checksum_desc = fmt::format("print checksums for all files ({})",
                                   fmt::join(algo_list, ", "));
  auto detail_desc = fmt::format(
      "detail level (0-{}, or feature list: {})",
      reader::fsinfo_features::max_level(),
      fmt::join(reader::fsinfo_features::all().to_string_views(), ", "));

  image_offset_opt_ =
      arg_str0("O", "image-offset", "<offset>",
               "filesystem image offset in bytes");
  cache_size_opt_ =
      arg_str0("s", "cache-size", "<size>", "block cache size");
  num_workers_opt_ =
      arg_int0("n", "num-workers", "<count>", "number of reader worker threads");

  // Check options
  check_integrity_opt_ =
      arg_lit0(nullptr, "check-integrity", "check integrity of each block");
  no_check_opt_ =
      arg_lit0(nullptr, "no-check", "don't even verify block checksums");

  // Detail/Info options
  detail_opt_ = arg_str0("d", "detail", "<level>", detail_desc.c_str());

  // Checksum options
  checksum_opt_ = arg_str0(nullptr, "checksum", "<algo>", checksum_desc.c_str());

  // Add all options to argtable
  argtable_.push_back(input_opt_);
  argtable_.push_back(export_metadata_opt_);
  argtable_.push_back(quiet_opt_);
  argtable_.push_back(verbose_opt_);
  argtable_.push_back(output_json_opt_);
  argtable_.push_back(print_header_opt_);
  argtable_.push_back(list_files_opt_);
  argtable_.push_back(image_offset_opt_);
  argtable_.push_back(cache_size_opt_);
  argtable_.push_back(num_workers_opt_);
  argtable_.push_back(check_integrity_opt_);
  argtable_.push_back(no_check_opt_);
  argtable_.push_back(detail_opt_);
  argtable_.push_back(checksum_opt_);
}

void argtable3_options_parser::populate_parsed_options() {
  // Input/Output paths
  if (input_opt_->count > 0) {
    opts_.input = string_to_sys_string(input_opt_->filename[0]);
  }
  if (export_metadata_opt_->count > 0) {
    opts_.export_metadata = string_to_sys_string(export_metadata_opt_->filename[0]);
  }

  // Operation mode options
  opts_.quiet = quiet_opt_->count > 0;
  opts_.verbose = verbose_opt_->count > 0;
  opts_.output_json = output_json_opt_->count > 0;
  opts_.print_header = print_header_opt_->count > 0;
  opts_.list_files = list_files_opt_->count > 0;

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

  // Check options
  opts_.check_integrity = check_integrity_opt_->count > 0;
  opts_.no_check = no_check_opt_->count > 0;

  // Detail/Info options
  if (detail_opt_->count > 0) {
    opts_.detail = detail_opt_->sval[0];
  }

  // Checksum options
  if (checksum_opt_->count > 0) {
    opts_.checksum_algo = checksum_opt_->sval[0];
  }
}

bool argtable3_options_parser::validate_options() {
  try {
    // Validate input is provided
    if (opts_.input.empty()) {
      throw std::runtime_error("input filesystem is required (use -i or --input)");
    }

    // Validate mutual exclusions
    if (opts_.no_check && opts_.check_integrity) {
      throw std::runtime_error("--no-check and --check-integrity are mutually exclusive");
    }

    // Validate checksum algorithm
    if (!opts_.checksum_algo.empty() &&
        !checksum::is_available(opts_.checksum_algo)) {
      throw std::runtime_error(
          fmt::format("checksum algorithm not available: {}", opts_.checksum_algo));
    }

    // Validate print-header mutual exclusions
    if (opts_.print_header &&
        (opts_.output_json || !opts_.export_metadata.empty() ||
         opts_.check_integrity || opts_.list_files ||
         !opts_.checksum_algo.empty())) {
      throw std::runtime_error(
          "--print-header is mutually exclusive with --json, "
          "--export-metadata, --check-integrity, --list and --checksum");
    }

    // Validate detail level
    auto numeric_detail = try_to<int>(opts_.detail);
    if (numeric_detail.has_value()) {
      if (*numeric_detail < 0 ||
          *numeric_detail > static_cast<int>(reader::fsinfo_features::max_level())) {
        throw std::runtime_error(
            fmt::format("detail level must be between 0 and {}",
                        reader::fsinfo_features::max_level()));
      }
    } else {
      // Try to parse as feature list
      try {
        (void)reader::fsinfo_features::parse(opts_.detail);
      } catch (std::exception const& e) {
        throw std::runtime_error(
            fmt::format("invalid detail specification: {}", e.what()));
      }
    }

  } catch (std::exception const& e) {
    fmt::print(stderr, "error: {}\n", e.what());
    return false;
  }
  return true;
}

void argtable3_options_parser::load_environment_variables() {
  // Load base environment variables (DWARFS_LOG_LEVEL, etc.)
  argtable3_base_parser::load_environment_variables("DWARFSCK");

  // Load tool-specific environment variables
  // Format: DWARFS_DWARFSCK_<OPTION>
  std::string prefix = "DWARFS_DWARFSCK_";

  // Example: DWARFS_DWARFSCK_CACHE_SIZE
  if (auto env_val = get_env_var(prefix + "CACHE_SIZE"); !env_val.empty()) {
    if (cache_size_opt_->count == 0) { // CLI takes priority
      opts_.cache_size = env_val;
    }
  }

  // Example: DWARFS_DWARFSCK_NUM_WORKERS
  if (auto env_val = get_env_var(prefix + "NUM_WORKERS"); !env_val.empty()) {
    if (num_workers_opt_->count == 0) {
      if (auto val = try_to<size_t>(env_val)) {
        opts_.num_workers = *val;
      }
    }
  }

  // Example: DWARFS_DWARFSCK_IMAGE_OFFSET
  if (auto env_val = get_env_var(prefix + "IMAGE_OFFSET"); !env_val.empty()) {
    if (image_offset_opt_->count == 0) {
      opts_.image_offset = env_val;
    }
  }

  // Example: DWARFS_DWARFSCK_DETAIL
  if (auto env_val = get_env_var(prefix + "DETAIL"); !env_val.empty()) {
    if (detail_opt_->count == 0) {
      opts_.detail = env_val;
    }
  }

  // Boolean flags
  if (auto env_val = get_env_var(prefix + "QUIET"); !env_val.empty()) {
    if (quiet_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.quiet = true;
    }
  }

  if (auto env_val = get_env_var(prefix + "VERBOSE"); !env_val.empty()) {
    if (verbose_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.verbose = true;
    }
  }

  if (auto env_val = get_env_var(prefix + "CHECK_INTEGRITY"); !env_val.empty()) {
    if (check_integrity_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.check_integrity = true;
    }
  }

  if (auto env_val = get_env_var(prefix + "NO_CHECK"); !env_val.empty()) {
    if (no_check_opt_->count == 0 && (env_val == "1" || env_val == "true")) {
      opts_.no_check = true;
    }
  }

  // Following MECE principle: CLI > ENV > defaults
}

} // namespace dwarfs::tool::dwarfsck