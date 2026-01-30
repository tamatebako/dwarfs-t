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
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
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

struct options {
  fs::path image_path;
  fs::path file_list;
  fs::path output_dir;
  size_t iterations{3};
  size_t cache_size{512 << 20}; // 512 MiB
  size_t num_workers{2};
  size_t num_threads{1}; // Extraction threads
  bool warmup{true};
  bool verbose{false};
  std::optional<fs::path> json_output;
};

void print_usage(char const* prog) {
  std::cerr << "Usage: " << prog
            << " IMAGE FILE_LIST [OPTIONS]\n\n"
            << "Extract multiple files from a DwarFS image and benchmark performance.\n\n"
            << "Arguments:\n"
            << "  IMAGE        Path to DwarFS image (.dff or .dft)\n"
            << "  FILE_LIST    Text file with one file path per line\n\n"
            << "Options:\n"
            << "  -o DIR       Output directory (default: /tmp/bench_multi)\n"
            << "  -n NUM       Number of iterations (default: 3)\n"
            << "  -c SIZE      Cache size in MiB (default: 512)\n"
            << "  -w NUM       Number of worker threads (default: 2)\n"
            << "  -t NUM       Number of extraction threads (default: 1)\n"
            << "  --no-warmup  Skip warmup iteration\n"
            << "  --json PATH  Write results to JSON file\n"
            << "  -v           Verbose output\n"
            << "  -h, --help   Show this help\n\n"
            << "File List Format:\n"
            << "  One file path per line, paths relative to image root:\n"
            << "    /path/to/file1.txt\n"
            << "    /path/to/file2.txt\n"
            << "    /another/file.bin\n\n"
            << "Examples:\n"
            << "  " << prog << " perl.dff files.txt\n"
            << "  " << prog << " perl.dff files.txt -t 4 -o /tmp/output\n"
            << "  " << prog << " data.dff large_list.txt -n 10 --json results.json\n";
}

options parse_args(int argc, char** argv) {
  options opts;

  if (argc < 3) {
    print_usage(argv[0]);
    std::exit(1);
  }

  opts.image_path = argv[1];
  opts.file_list = argv[2];
  opts.output_dir = "/tmp/bench_multi";

  for (int i = 3; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      std::exit(0);
    } else if (arg == "-o" && i + 1 < argc) {
      opts.output_dir = argv[++i];
    } else if (arg == "-n" && i + 1 < argc) {
      opts.iterations = std::stoull(argv[++i]);
    } else if (arg == "-c" && i + 1 < argc) {
      opts.cache_size = std::stoull(argv[++i]) << 20;
    } else if (arg == "-w" && i + 1 < argc) {
      opts.num_workers = std::stoull(argv[++i]);
    } else if (arg == "-t" && i + 1 < argc) {
      opts.num_threads = std::stoull(argv[++i]);
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

std::vector<std::string> load_file_list(fs::path const& path) {
  std::vector<std::string> files;
  std::ifstream in(path);

  if (!in) {
    throw std::runtime_error("Failed to open file list: " + path.string());
  }

  std::string line;
  while (std::getline(in, line)) {
    // Trim whitespace
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    files.push_back(std::move(line));
  }

  return files;
}

void extract_file(reader::filesystem_v2_lite const& fs, std::string const& path,
                  fs::path const& output_dir) {
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
    // Skip directories, symlinks, etc.
    return;
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

  // Create output path (flatten directory structure)
  auto output_path = output_dir / fs::path(path).filename();

  // Write to disk
  std::ofstream out(output_path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Failed to open output file: " +
                             output_path.string());
  }

  out.write(content.data(), content.size());
  if (!out) {
    throw std::runtime_error("Failed to write output file");
  }
}

void extract_files_single_threaded(reader::filesystem_v2_lite const& fs,
                                    std::vector<std::string> const& files,
                                    fs::path const& output_dir) {
  for (auto const& file : files) {
    extract_file(fs, file, output_dir);
  }
}

void extract_files_multi_threaded(reader::filesystem_v2_lite const& fs,
                                   std::vector<std::string> const& files,
                                   fs::path const& output_dir,
                                   size_t num_threads) {
  std::atomic<size_t> next_index{0};
  std::mutex error_mutex;
  std::vector<std::string> errors;

  auto worker = [&]() {
    while (true) {
      size_t index = next_index.fetch_add(1);
      if (index >= files.size()) {
        break;
      }

      try {
        extract_file(fs, files[index], output_dir);
      } catch (std::exception const& e) {
        std::lock_guard<std::mutex> lock(error_mutex);
        errors.push_back("File " + files[index] + ": " + e.what());
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  for (size_t i = 0; i < num_threads; ++i) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }

  if (!errors.empty()) {
    std::string msg = "Extraction errors:\n";
    for (auto const& err : errors) {
      msg += "  " + err + "\n";
    }
    throw std::runtime_error(msg);
  }
}

} // anonymous namespace

int main(int argc, char** argv) {
  try {
    auto opts = parse_args(argc, argv);

    // Setup
    logger_options log_opts;
    log_opts.threshold = opts.verbose ? LOGGER_LEVEL_VERBOSE : LOGGER_LEVEL_WARN;
    auto lgr = stream_logger(std::cerr, log_opts);
    LOG_PROXY(prod_logger_policy, lgr);

    auto os = os_access_generic();

    // Load file list
    auto files = load_file_list(opts.file_list);

    if (files.empty()) {
      std::cerr << "ERROR: File list is empty\n";
      return 1;
    }

    if (opts.verbose) {
      std::cout << "Loading DwarFS image: " << opts.image_path << "\n";
      std::cout << "Files to extract: " << files.size() << "\n";
      std::cout << "Output directory: " << opts.output_dir << "\n";
      std::cout << "Iterations: " << opts.iterations << "\n";
      std::cout << "Cache size: "
                << memory_tracker::format_bytes(opts.cache_size) << "\n";
      std::cout << "Worker threads: " << opts.num_workers << "\n";
      std::cout << "Extraction threads: " << opts.num_threads << "\n\n";
    }

    // Create output directory
    fs::create_directories(opts.output_dir);

    // Load filesystem
    reader::filesystem_load_config config;
    config.image_path = opts.image_path;
    config.cache_size = opts.cache_size;
    config.num_workers = opts.num_workers;

    auto fs_obj = reader::filesystem_loader::load(lgr, os, config);

    // Calculate total size
    size_t total_size = 0;
    for (auto const& file : files) {
      auto entry_opt = fs_obj.find(file);
      if (entry_opt) {
        auto stat = fs_obj.getattr(entry_opt->inode());
        if (stat.is_regular_file()) {
          total_size += stat.size();
        }
      }
    }

    if (opts.verbose) {
      std::cout << "Total data size: "
                << memory_tracker::format_bytes(total_size) << "\n\n";
    }

    // Run benchmark
    runner bench;

    std::string bench_name =
        opts.num_threads == 1 ? "multiple_files_single_thread"
                              : "multiple_files_multi_thread";

    bench.run_with_size(
        bench_name,
        [&fs_obj, &files, &opts]() {
          if (opts.num_threads == 1) {
            extract_files_single_threaded(fs_obj, files, opts.output_dir);
          } else {
            extract_files_multi_threaded(fs_obj, files, opts.output_dir,
                                         opts.num_threads);
          }
        },
        total_size, opts.iterations, opts.warmup);

    // Add metadata
    bench.add_metadata("image_path", opts.image_path.string());
    bench.add_metadata("file_count", std::to_string(files.size()));
    bench.add_metadata("total_size", std::to_string(total_size));
    bench.add_metadata("cache_size", std::to_string(opts.cache_size));
    bench.add_metadata("num_workers", std::to_string(opts.num_workers));
    bench.add_metadata("extraction_threads", std::to_string(opts.num_threads));

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

    // Count extracted files
    size_t extracted = 0;
    for (auto const& entry : fs::directory_iterator(opts.output_dir)) {
      if (entry.is_regular_file()) {
        ++extracted;
      }
    }

    if (opts.verbose) {
      std::cout << "\nExtracted " << extracted << " files to "
                << opts.output_dir << "\n";
    }

    return 0;

  } catch (std::exception const& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}