/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/os_access_generic.h>
#include <dwarfs/reader/filesystem_loader.h>
#include <dwarfs/reader/filesystem_v2.h>

#include "benchmark_framework.h"

namespace fs = std::filesystem;
using namespace dwarfs;
using namespace dwarfs::benchmark;

namespace {

struct options {
  fs::path image_path;
  std::string file_path;
  fs::path output_path;
  size_t iterations{3};
  size_t cache_size{512 << 20}; // 512 MiB
  size_t num_workers{2};
  bool warmup{true};
  bool verbose{false};
  std::optional<fs::path> json_output;
};

void print_usage(char const* prog) {
  std::cerr << "Usage: " << prog
            << " IMAGE FILE_PATH [OPTIONS]\n\n"
            << "Extract a single file from a DwarFS image and benchmark performance.\n\n"
            << "Arguments:\n"
            << "  IMAGE        Path to DwarFS image (.dff or .dft)\n"
            << "  FILE_PATH    Path to file within image (e.g., /path/to/file.txt)\n\n"
            << "Options:\n"
            << "  -o PATH      Output path for extracted file (default: /tmp/bench.out)\n"
            << "  -n NUM       Number of iterations (default: 3)\n"
            << "  -c SIZE      Cache size in MiB (default: 512)\n"
            << "  -w NUM       Number of worker threads (default: 2)\n"
            << "  --no-warmup  Skip warmup iteration\n"
            << "  --json PATH  Write results to JSON file\n"
            << "  -v           Verbose output\n"
            << "  -h, --help   Show this help\n\n"
            << "Examples:\n"
            << "  " << prog << " perl.dff /lib/perl5/5.38.0/Config.pm\n"
            << "  " << prog << " perl.dff /bin/perl -n 10 -o /tmp/perl\n"
            << "  " << prog << " data.dff /large_file.bin --json results.json\n";
}

options parse_args(int argc, char** argv) {
  options opts;

  if (argc < 3) {
    print_usage(argv[0]);
    std::exit(1);
  }

  opts.image_path = argv[1];
  opts.file_path = argv[2];
  opts.output_path = "/tmp/bench.out";

  for (int i = 3; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      std::exit(0);
    } else if (arg == "-o" && i + 1 < argc) {
      opts.output_path = argv[++i];
    } else if (arg == "-n" && i + 1 < argc) {
      opts.iterations = std::stoull(argv[++i]);
    } else if (arg == "-c" && i + 1 < argc) {
      opts.cache_size = std::stoull(argv[++i]) << 20;
    } else if (arg == "-w" && i + 1 < argc) {
      opts.num_workers = std::stoull(argv[++i]);
    } else if (arg == "--no-warmup") {
      opts.warmup = false;
    } else if (arg == "--json" && i + 1 < argc) {
      opts.json_output = argv[++i];
    } else if (arg == "-v") {
      opts.verbose = true;
    } else {
      std::cerr << "Unknown option: " << arg << "\n";
      print_usage(argv[0]);
      std::exit(1);
    }
  }

  return opts;
}

void extract_file(reader::filesystem_v2_lite const& fs, std::string const& path,
                  fs::path const& output) {
  std::error_code ec;

  // Find the file
  auto entry_opt = fs.find(path);
  if (!entry_opt) {
    throw std::runtime_error("File not found: " + path);
  }

  auto entry = entry_opt->inode();

  // Get file attributes
  auto stat = fs.getattr(entry, ec);
  if (ec) {
    throw std::runtime_error("Failed to get file attributes: " + ec.message());
  }

  if (!stat.is_regular_file()) {
    throw std::runtime_error("Not a regular file: " + path);
  }

  // Open the file
  auto inode = fs.open(entry, ec);
  if (ec) {
    throw std::runtime_error("Failed to open file: " + ec.message());
  }

  // Read the entire file
  auto content = fs.read_string(inode, ec);
  if (ec) {
    throw std::runtime_error("Failed to read file: " + ec.message());
  }

  // Write to disk
  std::ofstream out(output, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Failed to open output file: " + output.string());
  }

  out.write(content.data(), content.size());
  if (!out) {
    throw std::runtime_error("Failed to write output file");
  }
}

} // anonymous namespace

int main(int argc, char** argv) {
  try {
    auto opts = parse_args(argc, argv);

    // Setup logger
    logger_options log_opts;
    log_opts.threshold = opts.verbose ? LOGGER_LEVEL_VERBOSE : LOGGER_LEVEL_WARN;
    auto lgr = stream_logger(std::cerr, log_opts);
    LOG_PROXY(prod_logger_policy, lgr);

    // Setup OS access
    auto os = os_access_generic();

    if (opts.verbose) {
      std::cout << "Loading DwarFS image: " << opts.image_path << "\n";
      std::cout << "Target file: " << opts.file_path << "\n";
      std::cout << "Output path: " << opts.output_path << "\n";
      std::cout << "Iterations: " << opts.iterations << "\n";
      std::cout << "Cache size: "
                << memory_tracker::format_bytes(opts.cache_size) << "\n";
      std::cout << "Worker threads: " << opts.num_workers << "\n\n";
    }

    // Load filesystem
    reader::filesystem_load_config config;
    config.image_path = opts.image_path;
    config.cache_size = opts.cache_size;
    config.num_workers = opts.num_workers;

    auto fs_obj = reader::filesystem_loader::load(lgr, os, config);

    // Get file size for throughput calculation
    auto entry_opt = fs_obj.find(opts.file_path);
    if (!entry_opt) {
      std::cerr << "ERROR: File not found: " << opts.file_path << "\n";
      return 1;
    }

    auto stat = fs_obj.getattr(entry_opt->inode());
    size_t file_size = stat.size();

    if (opts.verbose) {
      std::cout << "File size: " << memory_tracker::format_bytes(file_size)
                << "\n\n";
    }

    // Run benchmark
    runner bench;

    bench.run_with_size(
        "single_file_extraction",
        [&fs_obj, &opts]() { extract_file(fs_obj, opts.file_path, opts.output_path); },
        file_size, opts.iterations, opts.warmup);

    // Add metadata
    bench.add_metadata("image_path", opts.image_path.string());
    bench.add_metadata("file_path", opts.file_path);
    bench.add_metadata("file_size", std::to_string(file_size));
    bench.add_metadata("cache_size", std::to_string(opts.cache_size));
    bench.add_metadata("num_workers", std::to_string(opts.num_workers));

    // Print results
    bench.print(std::cout);

    // Write JSON if requested
    if (opts.json_output) {
      bench.write_json(*opts.json_output);
      if (opts.verbose) {
        std::cout << "Results written to: " << opts.json_output->string()
                  << "\n";
      }
    }

    // Verify output file exists
    if (!fs::exists(opts.output_path)) {
      std::cerr << "WARNING: Output file not created\n";
      return 1;
    }

    if (opts.verbose) {
      std::cout << "\nExtracted file: " << opts.output_path << " ("
                << memory_tracker::format_bytes(
                       fs::file_size(opts.output_path))
                << ")\n";
    }

    return 0;

  } catch (std::exception const& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}