/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#ifdef __linux__
#include <sys/resource.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

namespace dwarfs::benchmark {

/**
 * High-resolution timer for benchmarking
 */
class timer {
 public:
  using clock = std::chrono::high_resolution_clock;
  using duration = std::chrono::nanoseconds;
  using time_point = clock::time_point;

  timer() : start_(clock::now()) {}

  void reset() { start_ = clock::now(); }

  duration elapsed() const { return clock::now() - start_; }

  double elapsed_seconds() const {
    return std::chrono::duration<double>(elapsed()).count();
  }

  double elapsed_milliseconds() const {
    return std::chrono::duration<double, std::milli>(elapsed()).count();
  }

 private:
  time_point start_;
};

/**
 * Memory usage tracker
 */
class memory_tracker {
 public:
  struct memory_info {
    size_t rss_bytes{0};     // Resident Set Size
    size_t vsize_bytes{0};   // Virtual memory size
    size_t peak_rss_bytes{0}; // Peak RSS
  };

  static memory_info get_current() {
    memory_info info;

#ifdef __linux__
    // Parse /proc/self/status for accurate memory info
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
      if (line.substr(0, 6) == "VmRSS:") {
        info.rss_bytes = parse_kb_line(line) * 1024;
      } else if (line.substr(0, 7) == "VmSize:") {
        info.vsize_bytes = parse_kb_line(line) * 1024;
      } else if (line.substr(0, 10) == "VmHWM:") {
        info.peak_rss_bytes = parse_kb_line(line) * 1024;
      }
    }

#elif defined(__APPLE__)
    // Use mach task_info for macOS
    struct mach_task_basic_info mach_info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&mach_info, &count) == KERN_SUCCESS) {
      info.rss_bytes = mach_info.resident_size;
      info.vsize_bytes = mach_info.virtual_size;
    }

    // Get peak RSS from getrusage
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
      info.peak_rss_bytes = usage.ru_maxrss; // Already in bytes on macOS
    }

#elif defined(_WIN32)
    // Use Windows API
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
      info.rss_bytes = pmc.WorkingSetSize;
      info.vsize_bytes = pmc.PrivateUsage;
      info.peak_rss_bytes = pmc.PeakWorkingSetSize;
    }
#endif

    return info;
  }

  static std::string format_bytes(size_t bytes) {
    const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    int unit = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 4) {
      size /= 1024.0;
      ++unit;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f %s", size, units[unit]);
    return buf;
  }

 private:
#ifdef __linux__
  static size_t parse_kb_line(std::string const& line) {
    // Format: "VmRSS:    12345 kB"
    auto pos = line.find_last_of(' ');
    if (pos != std::string::npos) {
      auto num_pos = line.find_last_not_of(' ', pos - 1);
      if (num_pos != std::string::npos) {
        auto start = line.find_last_of(' ', num_pos) + 1;
        return std::stoull(line.substr(start, num_pos - start + 1));
      }
    }
    return 0;
  }
#endif
};

/**
 * Statistical analysis for benchmark results
 */
class statistics {
 public:
  void add_sample(double value) { samples_.push_back(value); }

  size_t count() const { return samples_.size(); }

  double mean() const {
    if (samples_.empty()) return 0.0;
    return std::accumulate(samples_.begin(), samples_.end(), 0.0) /
           samples_.size();
  }

  double median() const {
    if (samples_.empty()) return 0.0;
    auto sorted = samples_;
    std::sort(sorted.begin(), sorted.end());
    size_t mid = sorted.size() / 2;
    if (sorted.size() % 2 == 0) {
      return (sorted[mid - 1] + sorted[mid]) / 2.0;
    }
    return sorted[mid];
  }

  double stddev() const {
    if (samples_.size() < 2) return 0.0;
    double m = mean();
    double sq_sum = 0.0;
    for (auto v : samples_) {
      double diff = v - m;
      sq_sum += diff * diff;
    }
    return std::sqrt(sq_sum / (samples_.size() - 1));
  }

  double min() const {
    if (samples_.empty()) return 0.0;
    return *std::min_element(samples_.begin(), samples_.end());
  }

  double max() const {
    if (samples_.empty()) return 0.0;
    return *std::max_element(samples_.begin(), samples_.end());
  }

  nlohmann::json to_json() const {
    return {
        {"count", count()},
        {"mean", mean()},
        {"median", median()},
        {"stddev", stddev()},
        {"min", min()},
        {"max", max()},
    };
  }

 private:
  std::vector<double> samples_;
};

/**
 * Benchmark result for a single test
 */
struct benchmark_result {
  std::string name;
  size_t iterations{0};
  statistics time_stats;      // Time in seconds
  statistics memory_stats;    // Memory in bytes
  std::optional<double> throughput; // MB/s if applicable
  std::map<std::string, std::string> metadata;

  nlohmann::json to_json() const {
    nlohmann::json j = {
        {"name", name},
        {"iterations", iterations},
        {"time", time_stats.to_json()},
        {"memory", memory_stats.to_json()},
    };
    if (throughput) {
      j["throughput_mb_per_sec"] = *throughput;
    }
    if (!metadata.empty()) {
      j["metadata"] = metadata;
    }
    return j;
  }
};

/**
 * Benchmark runner
 */
class runner {
 public:
  /**
   * Run a benchmark test
   *
   * @param name Test name
   * @param test Test function to run
   * @param iterations Number of iterations (default: 1)
   * @param warmup Run one warmup iteration first (default: true)
   */
  void run(std::string name, std::function<void()> test,
           size_t iterations = 1, bool warmup = true) {
    benchmark_result result;
    result.name = std::move(name);
    result.iterations = iterations;

    // Warmup run
    if (warmup && iterations > 1) {
      test();
    }

    // Actual iterations
    for (size_t i = 0; i < iterations; ++i) {
      auto mem_before = memory_tracker::get_current();

      timer t;
      test();
      double elapsed = t.elapsed_seconds();

      auto mem_after = memory_tracker::get_current();
      size_t mem_used = mem_after.peak_rss_bytes > mem_before.peak_rss_bytes
                            ? mem_after.peak_rss_bytes - mem_before.peak_rss_bytes
                            : 0;

      result.time_stats.add_sample(elapsed);
      result.memory_stats.add_sample(static_cast<double>(mem_used));
    }

    results_[result.name] = std::move(result);
  }

  /**
   * Run a benchmark with size information for throughput calculation
   */
  void run_with_size(std::string name, std::function<void()> test,
                     size_t data_size_bytes, size_t iterations = 1,
                     bool warmup = true) {
    // Save name before moving it
    std::string result_name = name;
    run(std::move(name), std::move(test), iterations, warmup);

    // Calculate throughput
    auto& result = results_.at(result_name);
    double mean_time = result.time_stats.mean();
    if (mean_time > 0.0) {
      double mb = data_size_bytes / (1024.0 * 1024.0);
      result.throughput = mb / mean_time;
    }
    result.metadata["data_size"] = std::to_string(data_size_bytes);
  }

  /**
   * Add metadata to last result
   */
  void add_metadata(std::string const& key, std::string value) {
    if (!results_.empty()) {
      results_.rbegin()->second.metadata[key] = std::move(value);
    }
  }

  /**
   * Get result by name
   */
  std::optional<benchmark_result> get_result(std::string const& name) const {
    auto it = results_.find(name);
    if (it != results_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  /**
   * Print results to output stream
   */
  void print(std::ostream& out) const {
    out << "\n=== Benchmark Results ===\n\n";

    for (auto const& [name, result] : results_) {
      out << name << ":\n";
      out << "  Iterations: " << result.iterations << "\n";
      out << "  Time (mean): " << result.time_stats.mean() << " s\n";
      out << "  Time (median): " << result.time_stats.median() << " s\n";
      out << "  Time (stddev): " << result.time_stats.stddev() << " s\n";
      out << "  Time (min): " << result.time_stats.min() << " s\n";
      out << "  Time (max): " << result.time_stats.max() << " s\n";

      if (result.memory_stats.count() > 0) {
        out << "  Memory (peak): "
            << memory_tracker::format_bytes(
                   static_cast<size_t>(result.memory_stats.max()))
            << "\n";
      }

      if (result.throughput) {
        out << "  Throughput: " << *result.throughput << " MB/s\n";
      }

      if (!result.metadata.empty()) {
        out << "  Metadata:\n";
        for (auto const& [k, v] : result.metadata) {
          out << "    " << k << ": " << v << "\n";
        }
      }

      out << "\n";
    }
  }

  /**
   * Export results as JSON
   */
  nlohmann::json to_json() const {
    nlohmann::json j;
    for (auto const& [name, result] : results_) {
      j[name] = result.to_json();
    }
    return j;
  }

  /**
   * Write results to JSON file
   */
  void write_json(std::filesystem::path const& path) const {
    std::ofstream out(path);
    if (!out) {
      throw std::runtime_error("Failed to open output file: " + path.string());
    }
    out << to_json().dump(2) << "\n";
  }

 private:
  std::map<std::string, benchmark_result> results_;
};

} // namespace dwarfs::benchmark