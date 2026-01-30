/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

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
  fs::path output_dir;
  size_t iterations{3};
  size_t cache_size{512 << 20}; // 512 MiB
  size_t num_workers{2};
  size_t num_threads{1}; // Extraction threads
  bool warmup{true};
  bool verbose{false};
  std::optional<fs::path> json_output;
};

struct extraction_stats {
  std::atomic<size_t> files_extracted{0};
  std::atomic<size_t> dirs_created{0};
  std::atomic<size_t> symlinks_created{0};
  std::atomic<size_t> bytes_written{0};
  std::atomic<size_t> errors{0};
  std::mutex error_mutex;
  std::vector<std::string> error_messages;
};

void print_usage(char const* prog) {
  std::cerr << "Usage: " << prog
            << " IMAGE OUTPUT_DIR [OPTIONS]\n\n"
            << "Extract entire DwarFS filesystem and benchmark performance.\n\n"
            << "Arguments:\n"
            << "  IMAGE        Path to DwarFS image (.dff or .dft)\n"
            << "  OUTPUT_DIR   Directory to extract files to\n\n"
            << "Options:\n"
            << "  -n NUM       Number of iterations (default: 3)\n"
            << "  -c SIZE      Cache size in MiB (default: 512)\n"
            << "  -w NUM       Number of worker threads (default: 2)\n"
            << "  -t NUM       Number of extraction threads (default: 1)\n"
            << "  --no-warmup  Skip warmup iteration\n"
            << "  --json PATH  Write results to JSON file\n"
            << "  -v           Verbose output\n"
            << "  -h, --help   Show this help\n\n"
            << "Examples:\n"
            << "  " << prog << " perl.dff /tmp/perl_extracted\n"
            << "  " << prog << " data.dff /tmp/output -t 4 -n 5\n"
            << "  " << prog << " image.dff /tmp/test --json results.json -v\n";
}

options parse_args(int argc, char** argv) {
  options opts;

  if (argc < 3) {
    print_usage(argv[0]);
    std::exit(1);
  }

  opts.image_path = argv[1];
  opts.output_dir = argv[2];

  for (int i = 3; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      std::exit(0);
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

void extract_entry(reader::filesystem_v2_lite const& fs_obj,
                   reader::dir_entry_view entry, fs::path const& output_base,
                   extraction_stats& stats) {
  std::error_code ec;

  // Get entry inode
  auto inode = entry.inode();

  // Get attributes
  auto stat = fs_obj.getattr(inode, ec);
  if (ec) {
    std::lock_guard<std::mutex> lock(stats.error_mutex);
    stats.error_messages.push_back("getattr failed: " + ec.message());
    stats.errors.fetch_add(1);
    return;
  }

  // Get path
  auto path_str = entry.path();
  auto output_path = output_base / fs::path(path_str).relative_path();

  if (stat.is_directory()) {
    // Create directory
    fs::create_directories(output_path, ec);
    if (!ec) {
      stats.dirs_created.fetch_add(1);
    } else {
      std::lock_guard<std::mutex> lock(stats.error_mutex);
      stats.error_messages.push_back("mkdir failed for " + path_str + ": " +
                                      ec.message());
      stats.errors.fetch_add(1);
    }
  } else if (stat.is_regular_file()) {
    // Extract file
    auto inode_num = fs_obj.open(inode, ec);
    if (ec) {
      std::lock_guard<std::mutex> lock(stats.error_mutex);
      stats.error_messages.push_back("open failed for " + path_str + ": " +
                                      ec.message());
      stats.errors.fetch_add(1);
      return;
    }

    auto content = fs_obj.read_string(inode_num, ec);
    if (ec) {
      std::lock_guard<std::mutex> lock(stats.error_mutex);
      stats.error_messages.push_back("read failed for " + path_str + ": " +
                                      ec.message());
      stats.errors.fetch_add(1);
      return;
    }

    // Ensure parent directory exists
    fs::create_directories(output_path.parent_path(), ec);

    // Write file
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
      std::lock_guard<std::mutex> lock(stats.error_mutex);
      stats.error_messages.push_back("Failed to open output file: " +
                                      output_path.string());
      stats.errors.fetch_add(1);
      return;
    }

    out.write(content.data(), content.size());
    if (!out) {
      std::lock_guard<std::mutex> lock(stats.error_mutex);
      stats.error_messages.push_back("Failed to write file: " +
                                      output_path.string());
      stats.errors.fetch_add(1);
      return;
    }

    stats.files_extracted.fetch_add(1);
    stats.bytes_written.fetch_add(content.size());

  } else if (stat.is_symlink()) {
    // Read symlink target
    auto target = fs_obj.readlink(inode, ec);
    if (ec) {
      std::lock_guard<std::mutex> lock(stats.error_mutex);
      stats.error_messages.push_back("readlink failed for " + path_str + ": " +
                                      ec.message());
      stats.errors.fetch_add(1);
      return;
    }

    // Ensure parent directory exists
    fs::create_directories(output_path.parent_path(), ec);

    // Create symlink
    fs::create_symlink(target, output_path, ec);
    if (!ec) {
      stats.symlinks_created.fetch_add(1);
    } else {
      std::lock_guard<std::mutex> lock(stats.error_mutex);
      stats.error_messages.push_back("symlink failed for " + path_str + ": " +
                                      ec.message());
      stats.errors.fetch_add(1);
    }
  }
}

void extract_filesystem_single_threaded(reader::filesystem_v2_lite const& fs_obj,
                                         fs::path const& output_dir,
                                         extraction_stats& stats) {
  fs_obj.walk([&](reader::dir_entry_view entry) {
    extract_entry(fs_obj, entry, output_dir, stats);
  });
}

void extract_filesystem_multi_threaded(reader::filesystem_v2_lite const& fs_obj,
                                        fs::path const& output_dir,
                                        size_t num_threads,
                                        extraction_stats& stats) {
  // Collect all entries first
  std::vector<reader::dir_entry_view> entries;
  fs_obj.walk([&entries](reader::dir_entry_view entry) {
    entries.push_back(entry);
  });

  // Extract in parallel
  std::atomic<size_t> next_index{0};

  auto worker = [&]() {
    while (true) {
      size_t index = next_index.fetch_add(1);
      if (index >= entries.size()) {
        break;
      }
      extract_entry(fs_obj, entries[index], output_dir, stats);
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

    if (opts.verbose) {
      std::cout << "Loading DwarFS image: " << opts.image_path << "\n";
      std::cout << "Output directory: " << opts.output_dir << "\n";
      std::cout << "Iterations: " << opts.iterations << "\n";
      std::cout << "Cache size: "
                << memory_tracker::format_bytes(opts.cache_size) << "\n";
      std::cout << "Worker threads: " << opts.num_workers << "\n";
      std::cout << "Extraction threads: " << opts.num_threads << "\n\n";
    }

    // Load filesystem
    reader::filesystem_load_config config;
    config.image_path = opts.image_path;
    config.cache_size = opts.cache_size;
    config.num_workers = opts.num_workers;

    auto fs_obj = reader::filesystem_loader::load(lgr, os, config);

    // Calculate total size
    size_t total_files = 0;
    size_t total_size = 0;

    fs_obj.walk([&](reader::dir_entry_view entry) {
      auto stat = fs_obj.getattr(entry.inode());
      if (stat.is_regular_file()) {
        ++total_files;
        total_size += stat.size();
      }
    });

    if (opts.verbose) {
      std::cout << "Files in image: " << total_files << "\n";
      std::cout << "Total data size: "
                << memory_tracker::format_bytes(total_size) << "\n\n";
    }

    // Run benchmark
    runner bench;

    std::string bench_name = opts.num_threads == 1
                                 ? "full_extraction_single_thread"
                                 : "full_extraction_multi_thread";

    bench.run_with_size(
        bench_name,
        [&fs_obj, &opts]() {
          // Clean output directory
          std::error_code ec;
          fs::remove_all(opts.output_dir, ec);
          fs::create_directories(opts.output_dir);

          extraction_stats stats;

          if (opts.num_threads == 1) {
            extract_filesystem_single_threaded(fs_obj, opts.output_dir, stats);
          } else {
            extract_filesystem_multi_threaded(fs_obj, opts.output_dir,
                                               opts.num_threads, stats);
          }

          // Check for errors
          if (stats.errors.load() > 0) {
            std::string msg = "Extraction errors (" +
                              std::to_string(stats.errors.load()) + "):\n";
            std::lock_guard<std::mutex> lock(stats.error_mutex);
            for (size_t i = 0; i < std::min(stats.error_messages.size(),
                                             static_cast<size_t>(10));
                 ++i) {
              msg += "  " + stats.error_messages[i] + "\n";
            }
            throw std::runtime_error(msg);
          }
        },
        total_size, opts.iterations, opts.warmup);

    // Add metadata
    bench.add_metadata("image_path", opts.image_path.string());
    bench.add_metadata("file_count", std::to_string(total_files));
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

    // Report extraction statistics
    if (opts.verbose && fs::exists(opts.output_dir)) {
      size_t final_files = 0;
      for (auto const& entry :
           fs::recursive_directory_iterator(opts.output_dir)) {
        if (entry.is_regular_file()) {
          ++final_files;
        }
      }

      std::cout << "\nFinal: " << final_files << " files in "
                << opts.output_dir << "\n";
    }

    return 0;

  } catch (std::exception const& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}