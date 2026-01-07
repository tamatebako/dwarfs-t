/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

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

enum class access_pattern { SEQUENTIAL, RANDOM, STRIDE };

struct options {
  fs::path image_path;
  std::string file_path;
  access_pattern pattern{access_pattern::SEQUENTIAL};
  size_t read_size{4096}; // 4 KiB per read
  size_t num_reads{100};
  size_t stride{0}; // For stride pattern
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
            << "Benchmark random access patterns on a file in a DwarFS image.\n\n"
            << "Arguments:\n"
            << "  IMAGE        Path to DwarFS image (.dff or .dft)\n"
            << "  FILE_PATH    Path to file within image\n\n"
            << "Options:\n"
            << "  -p PATTERN   Access pattern: sequential, random, stride (default: sequential)\n"
            << "  -s SIZE      Read size in bytes (default: 4096)\n"
            << "  -r NUM       Number of reads (default: 100)\n"
            << "  --stride N   Stride for stride pattern (default: read_size*2)\n"
            << "  -n NUM       Number of iterations (default: 3)\n"
            << "  -c SIZE      Cache size in MiB (default: 512)\n"
            << "  -w NUM       Number of worker threads (default: 2)\n"
            << "  --no-warmup  Skip warmup iteration\n"
            << "  --json PATH  Write results to JSON file\n"
            << "  -v           Verbose output\n"
            << "  -h, --help   Show this help\n\n"
            << "Access Patterns:\n"
            << "  sequential   Read sequentially from file start\n"
            << "  random       Read from random offsets\n"
            << "  stride       Read with fixed stride between offsets\n\n"
            << "Examples:\n"
            << "  " << prog << " perl.dff /bin/perl -p random -r 1000\n"
            << "  " << prog << " data.dff /large.bin -p stride --stride 65536\n"
            << "  " << prog << " image.dff /file.txt -s 8192 --json results.json\n";
}

options parse_args(int argc, char** argv) {
  options opts;

  if (argc < 3) {
    print_usage(argv[0]);
    std::exit(1);
  }

  opts.image_path = argv[1];
  opts.file_path = argv[2];

  for (int i = 3; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      std::exit(0);
    } else if (arg == "-p" && i + 1 < argc) {
      std::string pattern = argv[++i];
      if (pattern == "sequential") {
        opts.pattern = access_pattern::SEQUENTIAL;
      } else if (pattern == "random") {
        opts.pattern = access_pattern::RANDOM;
      } else if (pattern == "stride") {
        opts.pattern = access_pattern::STRIDE;
      } else {
        std::cerr << "Unknown pattern: " << pattern << "\n";
        std::exit(1);
      }
    } else if (arg == "-s" && i + 1 < argc) {
      opts.read_size = std::stoull(argv[++i]);
    } else if (arg == "-r" && i + 1 < argc) {
      opts.num_reads = std::stoull(argv[++i]);
    } else if (arg == "--stride" && i + 1 < argc) {
      opts.stride = std::stoull(argv[++i]);
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

  // Default stride: 2x read size
  if (opts.stride == 0) {
    opts.stride = opts.read_size * 2;
  }

  return opts;
}

std::vector<file_off_t> generate_offsets(access_pattern pattern,
                                          size_t file_size, size_t read_size,
                                          size_t num_reads, size_t stride) {
  std::vector<file_off_t> offsets;
  offsets.reserve(num_reads);

  if (file_size < read_size) {
    throw std::runtime_error("File too small for read size");
  }

  file_off_t max_offset = file_size - read_size;

  switch (pattern) {
  case access_pattern::SEQUENTIAL:
    for (size_t i = 0; i < num_reads; ++i) {
      file_off_t offset = (i * read_size) % max_offset;
      offsets.push_back(offset);
    }
    break;

  case access_pattern::RANDOM: {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<file_off_t> dist(0, max_offset);

    for (size_t i = 0; i < num_reads; ++i) {
      offsets.push_back(dist(gen));
    }
    break;
  }

  case access_pattern::STRIDE:
    for (size_t i = 0; i < num_reads; ++i) {
      file_off_t offset = (i * stride) % max_offset;
      offsets.push_back(offset);
    }
    break;
  }

  return offsets;
}

void perform_reads(reader::filesystem_v2_lite const& fs, uint32_t inode,
                   std::vector<file_off_t> const& offsets, size_t read_size) {
  std::error_code ec;
  std::vector<char> buffer(read_size);

  for (auto offset : offsets) {
    size_t bytes_read = fs.read(inode, buffer.data(), read_size, offset, ec);
    if (ec) {
      throw std::runtime_error("Read failed at offset " +
                               std::to_string(offset) + ": " + ec.message());
    }
    if (bytes_read == 0) {
      throw std::runtime_error("Unexpected EOF at offset " +
                               std::to_string(offset));
    }
  }
}

const char* pattern_name(access_pattern pattern) {
  switch (pattern) {
  case access_pattern::SEQUENTIAL:
    return "sequential";
  case access_pattern::RANDOM:
    return "random";
  case access_pattern::STRIDE:
    return "stride";
  }
  return "unknown";
}

} // anonymous namespace

int main(int argc, char** argv) {
  try {
    auto opts = parse_args(argc, argv);

    // Setup
    logger_options log_opts;
    log_opts.threshold = opts.verbose ? logger::VERBOSE : logger::WARN;
    auto lgr = stream_logger(std::cerr, log_opts);
    LOG_PROXY(prod_logger_policy, lgr);

    auto os = os_access_generic();

    if (opts.verbose) {
      std::cout << "Loading DwarFS image: " << opts.image_path << "\n";
      std::cout << "Target file: " << opts.file_path << "\n";
      std::cout << "Access pattern: " << pattern_name(opts.pattern) << "\n";
      std::cout << "Read size: " << memory_tracker::format_bytes(opts.read_size)
                << "\n";
      std::cout << "Number of reads: " << opts.num_reads << "\n";
      if (opts.pattern == access_pattern::STRIDE) {
        std::cout << "Stride: " << memory_tracker::format_bytes(opts.stride)
                  << "\n";
      }
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

    // Find and open file
    auto entry_opt = fs_obj.find(opts.file_path);
    if (!entry_opt) {
      std::cerr << "ERROR: File not found: " << opts.file_path << "\n";
      return 1;
    }

    auto entry = entry_opt->inode();
    auto stat = fs_obj.getattr(entry);

    if (!stat.is_regular_file()) {
      std::cerr << "ERROR: Not a regular file: " << opts.file_path << "\n";
      return 1;
    }

    size_t file_size = stat.size();

    if (opts.verbose) {
      std::cout << "File size: " << memory_tracker::format_bytes(file_size)
                << "\n\n";
    }

    std::error_code ec;
    auto inode = fs_obj.open(entry, ec);
    if (ec) {
      std::cerr << "ERROR: Failed to open file: " << ec.message() << "\n";
      return 1;
    }

    // Generate read offsets
    auto offsets =
        generate_offsets(opts.pattern, file_size, opts.read_size,
                         opts.num_reads, opts.stride);

    if (opts.verbose) {
      std::cout << "Generated " << offsets.size() << " read offsets\n";
      if (opts.pattern == access_pattern::RANDOM) {
        std::cout << "(random offsets change each iteration)\n";
      }
      std::cout << "\n";
    }

    // Run benchmark
    runner bench;

    size_t total_bytes = opts.read_size * opts.num_reads;

    std::string bench_name =
        std::string("random_access_") + pattern_name(opts.pattern);

    bench.run_with_size(
        bench_name,
        [&fs_obj, inode, &opts, &offsets]() {
          // For random pattern, regenerate offsets each iteration
          auto current_offsets = offsets;
          if (opts.pattern == access_pattern::RANDOM) {
            std::random_device rd;
            std::shuffle(current_offsets.begin(), current_offsets.end(),
                         std::mt19937_64(rd()));
          }
          perform_reads(fs_obj, inode, current_offsets, opts.read_size);
        },
        total_bytes, opts.iterations, opts.warmup);

    // Add metadata
    bench.add_metadata("image_path", opts.image_path.string());
    bench.add_metadata("file_path", opts.file_path);
    bench.add_metadata("file_size", std::to_string(file_size));
    bench.add_metadata("pattern", pattern_name(opts.pattern));
    bench.add_metadata("read_size", std::to_string(opts.read_size));
    bench.add_metadata("num_reads", std::to_string(opts.num_reads));
    bench.add_metadata("total_bytes", std::to_string(total_bytes));
    bench.add_metadata("cache_size", std::to_string(opts.cache_size));
    bench.add_metadata("num_workers", std::to_string(opts.num_workers));

    if (opts.pattern == access_pattern::STRIDE) {
      bench.add_metadata("stride", std::to_string(opts.stride));
    }

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

    // Calculate and display additional metrics
    auto result = bench.get_result(bench_name);
    if (result && opts.verbose) {
      double mean_time = result->time_stats.mean();
      if (mean_time > 0.0) {
        double reads_per_sec = opts.num_reads / mean_time;
        double latency_ms = (mean_time * 1000.0) / opts.num_reads;

        std::cout << "\nAdditional metrics:\n";
        std::cout << "  Reads per second: " << reads_per_sec << "\n";
        std::cout << "  Average latency: " << latency_ms << " ms per read\n";
      }
    }

    return 0;

  } catch (std::exception const& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}