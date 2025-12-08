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

#include <chrono>
#include <cmath>
#include <fstream>
#include <numbers>
#include <random>
#include <sstream>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <dwarfs/block_compressor.h>
#include <dwarfs/block_decompressor.h>
#include <dwarfs/compressor_registry.h>
#include <dwarfs/decompressor_registry.h>
#include <dwarfs/malloc_byte_buffer.h>
#include <dwarfs/pcm_sample_transformer.h>

using dwarfs::bad_compression_ratio_error;

using namespace dwarfs;

namespace {

// ============================================================================
// Test Data Generation Utilities
// ============================================================================

/**
 * Generate highly compressible source code data
 */
shared_byte_buffer make_source_code_data(size_t size = 1024 * 1024) {
  std::stringstream ss;
  
  // Repetitive C++ code patterns
  for (size_t i = 0; i < size / 100; ++i) {
    ss << "// Copyright (c) 2025 DwarFS Project\n"
       << "#include <iostream>\n"
       << "#include <vector>\n"
       << "namespace dwarfs {\n"
       << "void function_" << i << "() {\n"
       << "  std::vector<int> data;\n"
       << "  for (int i = 0; i < 100; ++i) {\n"
       << "    data.push_back(i * 2);\n"
       << "  }\n"
       << "}\n"
       << "} // namespace dwarfs\n\n";
  }
  
  auto str = ss.str();
  auto out = malloc_byte_buffer::create();
  out.resize(std::min(str.size(), size));
  std::memcpy(out.data(), str.data(), out.size());
  return out.share();
}

/**
 * Generate repetitive log data (very high compressibility)
 */
shared_byte_buffer make_log_data(size_t size = 1024 * 1024) {
  std::stringstream ss;
  
  std::array<std::string, 4> log_levels = {"ERROR", "WARN", "INFO", "DEBUG"};
  std::array<std::string, 6> messages = {
    "Connection failed",
    "Database query timeout",
    "Request processed successfully",
    "Configuration loaded",
    "Cache miss",
    "Authentication successful"
  };
  
  std::mt19937 rng(42);
  std::uniform_int_distribution<> level_dist(0, 3);
  std::uniform_int_distribution<> msg_dist(0, 5);
  
  while (ss.tellp() < static_cast<std::streampos>(size)) {
    ss << "2025-12-01 10:15:30 " 
       << log_levels[level_dist(rng)] << ": "
       << messages[msg_dist(rng)] << "\n";
  }
  
  auto str = ss.str();
  auto out = malloc_byte_buffer::create();
  out.resize(std::min(str.size(), size));
  std::memcpy(out.data(), str.data(), out.size());
  return out.share();
}

/**
 * Generate mixed binary data
 */
shared_byte_buffer make_binary_data(size_t size = 1024 * 1024) {
  std::mt19937 rng(42);
  std::uniform_int_distribution<uint8_t> dist(0, 255);
  
  auto out = malloc_byte_buffer::create();
  out.resize(size);
  
  // Mix of patterns: 50% repetitive, 30% semi-random, 20% random
  size_t third = size / 3;
  
  // Repetitive section
  uint8_t pattern[] = {0xDE, 0xAD, 0xBE, 0xEF};
  for (size_t i = 0; i < third; ++i) {
    out.data()[i] = pattern[i % 4];
  }
  
  // Semi-random section (biased)
  for (size_t i = third; i < 2 * third; ++i) {
    out.data()[i] = dist(rng) & 0xF0; // Only high nibbles
  }
  
  // Random section
  for (size_t i = 2 * third; i < size; ++i) {
    out.data()[i] = dist(rng);
  }
  
  return out.share();
}

/**
 * Generate incompressible random data
 */
shared_byte_buffer make_random_data(size_t size = 1024 * 1024) {
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<uint8_t> dist(0, 255);
  
  auto out = malloc_byte_buffer::create();
  out.resize(size);
  
  for (size_t i = 0; i < size; ++i) {
    out.data()[i] = dist(rng);
  }
  
  return out.share();
}

/**
 * Generate PCM audio data for FLAC testing
 */
template <typename T>
std::vector<T> make_sine_wave(int bits, size_t length, double period) {
  std::vector<T> rv(length);
  double amplitude = (1 << (bits - 1));
  for (size_t i = 0; i < length; ++i) {
    rv[i] = static_cast<T>(
        amplitude * std::sin(2 * std::numbers::pi * i / period) - 0.5);
  }
  return rv;
}

template <typename T>
std::vector<T> multiplex_channels(std::vector<std::vector<T>> const& in) {
  auto samples = in.front().size();
  auto channels = in.size();
  std::vector<T> out(channels * samples);

  for (size_t i = 0; i < samples; ++i) {
    for (size_t c = 0; c < channels; ++c) {
      out[i * channels + c] = in.at(c).at(i);
    }
  }

  return out;
}

shared_byte_buffer make_pcm_audio_data(
    int channels = 2, 
    int samples = 48000,  // 1 second at 48kHz
    int bytes_per_sample = 2,
    int bits_per_sample = 16) {
  
  std::vector<std::vector<int32_t>> data;
  for (int c = 0; c < channels; ++c) {
    data.emplace_back(
        make_sine_wave<int32_t>(bits_per_sample, samples, 
                                3.1 * ((599 * (c + 1)) % 256)));
  }
  
  auto muxed = multiplex_channels(data);
  auto out = malloc_byte_buffer::create();
  out.resize(bytes_per_sample * channels * samples);
  
  pcm_sample_transformer<int32_t> xfm(
      pcm_sample_endianness::Little,
      pcm_sample_signedness::Signed,
      pcm_sample_padding::Msb,
      bytes_per_sample,
      bits_per_sample);
  
  xfm.pack(out.span(), muxed);
  return out.share();
}

/**
 * Generate FITS image data for Rice++ testing
 */
shared_byte_buffer make_fits_image_data(
    int components = 1,
    int width = 512,
    int height = 512,
    int unused_lsb = 0) {
  
  std::mt19937_64 rng(42);
  std::uniform_int_distribution<uint16_t> noise(30000, 31000);
  
  size_t pixels = width * height;
  std::vector<uint16_t> data(components * pixels);
  
  uint16_t mask = static_cast<uint16_t>(0xFFFF << unused_lsb);
  
  for (size_t i = 0; i < data.size(); ++i) {
    uint16_t value = noise(rng) & mask;
    // Convert to big-endian (FITS standard)
    data[i] = ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
  }
  
  auto out = malloc_byte_buffer::create();
  out.resize(data.size() * sizeof(uint16_t));
  std::memcpy(out.data(), data.data(), out.size());
  
  return out.share();
}

// ============================================================================
// Benchmark Result Structure
// ============================================================================

struct BenchmarkResult {
  std::string algorithm;
  std::string level_or_option;
  std::string dataset;
  
  // Input/output sizes
  size_t input_size_bytes{0};
  size_t output_size_bytes{0};
  double compression_ratio_percent{0.0};
  
  // Timing
  double compression_time_ms{0.0};
  double decompression_time_ms{0.0};
  
  // Throughput
  double compression_speed_mbps{0.0};
  double decompression_speed_mbps{0.0};
  
  // Verification
  bool decompression_successful{false};
  bool data_matches{false};
  
  void print() const {
    std::cout << "Algorithm: " << algorithm << ":" << level_or_option << "\n"
              << "Dataset: " << dataset << "\n"
              << "Input: " << input_size_bytes << " bytes\n"
              << "Output: " << output_size_bytes << " bytes\n"
              << "Ratio: " << compression_ratio_percent << "%\n"
              << "Compression: " << compression_time_ms << " ms ("
              << compression_speed_mbps << " MB/s)\n"
              << "Decompression: " << decompression_time_ms << " ms ("
              << decompression_speed_mbps << " MB/s)\n"
              << "Verification: " 
              << (data_matches ? "PASSED" : "FAILED") << "\n\n";
  }
};

// ============================================================================
// Benchmark Test Fixture
// ============================================================================

class CompressionAlgorithmBenchmark : public ::testing::Test {
protected:
  void SetUp() override {
    // Registries are singletons, accessed via instance()
    // No need to initialize them here
    
    // Create test datasets
    datasets_["source_code"] = make_source_code_data(512 * 1024);
    datasets_["logs"] = make_log_data(512 * 1024);
    datasets_["binary"] = make_binary_data(512 * 1024);
    datasets_["random"] = make_random_data(256 * 1024); // Smaller for speed
    datasets_["pcm_audio"] = make_pcm_audio_data(2, 24000); // 0.5s stereo
    datasets_["fits_image"] = make_fits_image_data(1, 256, 256);
  }
  
  BenchmarkResult benchmark_algorithm(
      std::string const& algorithm_spec,
      std::string const& dataset_name,
      std::string const& metadata_json = "") {
    
    BenchmarkResult result;
    result.algorithm = algorithm_spec.substr(0, algorithm_spec.find(':'));
    result.level_or_option = algorithm_spec;
    result.dataset = dataset_name;
    
    auto const& data = datasets_.at(dataset_name);
    result.input_size_bytes = data.size();
    
    try {
      // Compression benchmark
      block_compressor comp(algorithm_spec);
      
      auto start = std::chrono::high_resolution_clock::now();
      auto compressed = comp.compress(data, metadata_json);
      auto end = std::chrono::high_resolution_clock::now();
      
      result.compression_time_ms =
          std::chrono::duration<double, std::milli>(end - start).count();
      result.output_size_bytes = compressed.size();
      result.compression_ratio_percent =
          (1.0 - static_cast<double>(compressed.size()) / data.size()) * 100.0;
      result.compression_speed_mbps =
          (data.size() / (1024.0 * 1024.0)) / (result.compression_time_ms / 1000.0);
      
      // Decompression benchmark
      auto comp_type = comp.type();
      
      start = std::chrono::high_resolution_clock::now();
      auto decompressed = block_decompressor::decompress(comp_type, compressed.span());
      end = std::chrono::high_resolution_clock::now();
      
      result.decompression_time_ms =
          std::chrono::duration<double, std::milli>(end - start).count();
      result.decompression_speed_mbps =
          (decompressed.size() / (1024.0 * 1024.0)) /
          (result.decompression_time_ms / 1000.0);
      
      result.decompression_successful = true;
      result.data_matches = (data == decompressed);
      
    } catch (bad_compression_ratio_error const& e) {
      // This is expected for incompressible data
      result.decompression_successful = true;
      result.data_matches = true;  // Compressor correctly detected expansion
      result.compression_ratio_percent = -100.0;  // Indicate rejection
    } catch (std::exception const& e) {
      std::cerr << "Benchmark failed for " << algorithm_spec
                << " on " << dataset_name << ": " << e.what() << "\n";
      result.decompression_successful = false;
      result.data_matches = false;
    }
    
    return result;
  }
  
  std::map<std::string, shared_byte_buffer> datasets_;
};

// ============================================================================
// Test Cases
// ============================================================================

TEST_F(CompressionAlgorithmBenchmark, ZSTD_Levels) {
  std::vector<int> levels = {1, 3, 5, 9, 19, 22};
  
  for (auto level : levels) {
    auto spec = "zstd:level=" + std::to_string(level);
    auto result = benchmark_algorithm(spec, "source_code");
    
    EXPECT_GT(result.compression_ratio_percent, 0.0);
    EXPECT_TRUE(result.data_matches);
    EXPECT_GT(result.compression_speed_mbps, 0.0);
    
    result.print();
  }
}

TEST_F(CompressionAlgorithmBenchmark, LZMA_Levels) {
  std::vector<int> levels = {0, 3, 6, 9};
  
  for (auto level : levels) {
    auto spec = "lzma:level=" + std::to_string(level);
    auto result = benchmark_algorithm(spec, "binary");
    
    EXPECT_GT(result.compression_ratio_percent, 0.0);
    EXPECT_TRUE(result.data_matches);
    
    result.print();
  }
}

TEST_F(CompressionAlgorithmBenchmark, LZ4_Levels) {
  // LZ4 doesn't have levels, just use default
  auto result = benchmark_algorithm("lz4", "logs");
  
  EXPECT_GT(result.compression_ratio_percent, 0.0);
  EXPECT_TRUE(result.data_matches);
  EXPECT_GT(result.compression_speed_mbps, 100.0); // LZ4 should be fast
  
  result.print();
}

TEST_F(CompressionAlgorithmBenchmark, LZ4HC_Levels) {
  std::vector<int> levels = {1, 9};
  
  for (auto level : levels) {
    auto spec = "lz4hc:level=" + std::to_string(level);
    auto result = benchmark_algorithm(spec, "logs");
    
    EXPECT_GT(result.compression_ratio_percent, 0.0);
    EXPECT_TRUE(result.data_matches);
    
    result.print();
  }
}

TEST_F(CompressionAlgorithmBenchmark, Brotli_Levels) {
  std::vector<int> levels = {1, 5, 9, 11};
  
  for (auto level : levels) {
    auto spec = "brotli:quality=" + std::to_string(level);
    auto result = benchmark_algorithm(spec, "source_code");
    
    EXPECT_GT(result.compression_ratio_percent, 0.0);
    EXPECT_TRUE(result.data_matches);
    
    result.print();
  }
}

TEST_F(CompressionAlgorithmBenchmark, FLAC_Audio) {
  std::vector<int> levels = {0, 3, 5, 8};
  
  nlohmann::json meta{
    {"endianness", "little"},
    {"signedness", "signed"},
    {"padding", "msb"},
    {"bytes_per_sample", 2},
    {"bits_per_sample", 16},
    {"number_of_channels", 2}
  };
  
  for (auto level : levels) {
    auto spec = "flac:level=" + std::to_string(level);
    auto result = benchmark_algorithm(spec, "pcm_audio", meta.dump());
    
    EXPECT_GT(result.compression_ratio_percent, 20.0); // FLAC should compress well
    EXPECT_TRUE(result.data_matches);
    
    result.print();
  }
}

TEST_F(CompressionAlgorithmBenchmark, RicePP_FITS) {
  std::vector<int> block_sizes = {128, 256, 512};
  
  nlohmann::json meta{
    {"endianness", "big"},
    {"bytes_per_sample", 2},
    {"unused_lsb_count", 0},
    {"component_count", 1}
  };
  
  // Test if Rice++ is available by trying to create a compressor
  bool ricepp_available = false;
  try {
    block_compressor test_comp("ricepp:block_size=128");
    ricepp_available = true;
  } catch (std::exception const&) {
    std::cout << "Rice++ compressor not available, skipping test\n";
    GTEST_SKIP() << "Rice++ compressor not available in this build";
  }
  
  for (auto block_size : block_sizes) {
    auto spec = "ricepp:block_size=" + std::to_string(block_size);
    auto result = benchmark_algorithm(spec, "fits_image", meta.dump());
    
    EXPECT_GT(result.compression_ratio_percent, 10.0);
    EXPECT_TRUE(result.data_matches);
    
    result.print();
  }
}

TEST_F(CompressionAlgorithmBenchmark, IncompressibleData) {
  // Test that all algorithms handle incompressible data gracefully
  std::vector<std::string> algorithms = {
    "zstd:level=1",
    "lzma:level=0",
    "lz4",
    "brotli:quality=1"
  };
  
  for (auto const& alg : algorithms) {
    auto result = benchmark_algorithm(alg, "random");
    
    // Should not crash, ratio may be negative (expansion)
    EXPECT_TRUE(result.decompression_successful);
    EXPECT_TRUE(result.data_matches);
    
    result.print();
  }
}

TEST_F(CompressionAlgorithmBenchmark, CompressionRatioComparison) {
  // Compare all general-purpose algorithms on same dataset
  std::vector<std::string> algorithms = {
    "zstd:level=5",
    "lzma:level=6",
    "lz4hc:level=9",
    "brotli:quality=9"
  };
  
  std::cout << "\n=== Compression Ratio Comparison (source_code dataset) ===\n";
  
  std::vector<BenchmarkResult> results;
  for (auto const& alg : algorithms) {
    auto result = benchmark_algorithm(alg, "source_code");
    results.push_back(result);
    EXPECT_TRUE(result.data_matches);
  }
  
  // Sort by ratio
  std::sort(results.begin(), results.end(), 
            [](auto const& a, auto const& b) {
              return a.compression_ratio_percent > b.compression_ratio_percent;
            });
  
  for (auto const& r : results) {
    std::cout << r.algorithm << ": " << r.compression_ratio_percent 
              << "% (" << r.compression_speed_mbps << " MB/s)\n";
  }
}

TEST_F(CompressionAlgorithmBenchmark, SpeedComparison) {
  // Compare speed across algorithms
  std::vector<std::string> algorithms = {
    "lz4",              // Fastest
    "lz4hc:level=1",    // Fast
    "zstd:level=1",     // Balanced
    "brotli:quality=1"  // Good ratio
  };
  
  std::cout << "\n=== Compression Speed Comparison (logs dataset) ===\n";
  
  std::vector<BenchmarkResult> results;
  for (auto const& alg : algorithms) {
    auto result = benchmark_algorithm(alg, "logs");
    results.push_back(result);
    EXPECT_TRUE(result.data_matches);
  }
  
  // Sort by speed
  std::sort(results.begin(), results.end(), 
            [](auto const& a, auto const& b) {
              return a.compression_speed_mbps > b.compression_speed_mbps;
            });
  
  for (auto const& r : results) {
    std::cout << r.algorithm << ": " << r.compression_speed_mbps 
              << " MB/s (ratio: " << r.compression_ratio_percent << "%)\n";
  }
}

} // anonymous namespace