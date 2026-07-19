/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 */

#include <gtest/gtest.h>

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
// This test benchmarks Modern Thrift serialization

#include <chrono>
#include <iostream>
#include "dwarfs/metadata/serialization/serialization_facade.h"
#include "dwarfs/metadata/serialization/facade_factory.h"
#include "dwarfs/metadata/serialization/serialization_format.h"
#include "dwarfs/metadata/serialization/init_serializers.h"
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/modern/domain_to_thrift.h"
#include "dwarfs/metadata/modern/thrift_to_domain.h"
#include "metadata_modern_types.h"

using namespace dwarfs::metadata;
using namespace std::chrono;

namespace {

// Create realistic test metadata (large dataset)
domain::metadata create_large_test_metadata() {
  domain::metadata meta;

  // Basic required fields
  meta.block_size = 4096;
  meta.total_fs_size = 1024 * 1024 * 1024;  // 1GB
  meta.timestamp_base = 1609459200;

  // Add 10,000 chunks
  for (int i = 0; i < 10000; ++i) {
    domain::chunk chunk{static_cast<uint32_t>(i / 100),
                        static_cast<uint32_t>((i % 100) * 4096),
                        4096};
    meta.chunks.push_back(chunk);
  }

  // Add 1,000 directories
  for (int i = 0; i < 1000; ++i) {
    domain::directory dir{static_cast<uint32_t>(i * 10),
                          static_cast<uint32_t>(i > 0 ? (i - 1) : 0),
                          0};
    meta.directories.push_back(dir);
  }

  // Add 5,000 inodes
  for (int i = 0; i < 5000; ++i) {
    domain::inode_data inode;
    inode.mode_index = i % 10;
    inode.owner_index = i % 100;
    inode.group_index = i % 50;
    inode.atime_offset = i;
    inode.mtime_offset = i + 1000;
    inode.ctime_offset = i + 2000;
    meta.inodes.push_back(inode);
  }

  // Initialize tables before use (required)
  meta.chunk_table.push_back(0);
  meta.symlink_table.push_back(0);

  // Add 1,000 names
  for (int i = 0; i < 1000; ++i) {
    meta.names.push_back("filename_" + std::to_string(i) + ".txt");
  }

  // Add UIDs, GIDs, modes
  for (int i = 0; i < 100; ++i) {
    meta.uids.push_back(1000 + i);
    meta.gids.push_back(1000 + i);
    meta.modes.push_back(0644 + (i % 3));
  }

  meta.symlinks.push_back("/tmp/link");

  // Add 10,000 dir entries
  meta.dir_entries = std::vector<domain::dir_entry>();
  for (int i = 0; i < 10000; ++i) {
    meta.dir_entries->push_back(domain::dir_entry{
        static_cast<uint32_t>(i % 1000),
        static_cast<uint32_t>(i % 5000)});
  }

  return meta;
}

// Benchmark helper
template<typename Func>
auto benchmark(Func&& func, int iterations = 10) {
  auto start = high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    func();
  }
  auto end = high_resolution_clock::now();
  return duration_cast<microseconds>(end - start).count() / iterations;
}

} // anonymous namespace

// NOTE: Benchmarks are limited with current architecture:
// - FlatBuffers: Modern default (writing)
// - Thrift: Legacy read-only format
//
// Comprehensive benchmark comparisons between FlatBuffers and Thrift
// should be performed using real-world data via the benchmarks/ directory,
// not unit tests.

// Benchmark: Serialization Speed
TEST(SerializationBenchmark, SerializationSpeed) {
  // Initialize serializers before use
  serialization::init_serializers();

  std::cout << "\n=== Serialization Speed Benchmark ===" << std::endl;

  auto test_meta = create_large_test_metadata();

  // Modern Thrift format
  #ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  {
    auto facade = serialization::FacadeFactory::create(
        serialization::SerializationFormat::MODERN_THRIFT);

    auto thrift_time = benchmark([&]() {
      auto bytes = facade->serialize(test_meta);
    });

    std::cout << "Modern Thrift:  " << thrift_time << " μs/op" << std::endl;
  }
  #endif
}

// Benchmark: Deserialization Speed
TEST(SerializationBenchmark, DeserializationSpeed) {
  // Initialize serializers before use
  serialization::init_serializers();

  std::cout << "\n=== Deserialization Speed Benchmark ===" << std::endl;

  auto test_meta = create_large_test_metadata();

  #ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  {
    auto facade = serialization::FacadeFactory::create(
        serialization::SerializationFormat::MODERN_THRIFT);
    auto bytes = facade->serialize(test_meta);

    auto thrift_time = benchmark([&]() {
      auto result = facade->deserialize(bytes);
    });

    std::cout << "Modern Thrift:  " << thrift_time << " μs/op" << std::endl;
  }
  #endif
}

// Benchmark: Serialized Size
TEST(SerializationBenchmark, SerializedSize) {
  // Initialize serializers before use
  serialization::init_serializers();

  std::cout << "\n=== Serialized Data Size ===" << std::endl;

  auto test_meta = create_large_test_metadata();

  #ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  {
    auto facade = serialization::FacadeFactory::create(
        serialization::SerializationFormat::MODERN_THRIFT);
    auto bytes = facade->serialize(test_meta);
    std::cout << "Modern Thrift:  " << bytes.size() << " bytes" << std::endl;
  }
  #endif
}

// Benchmark: Round-trip Performance
TEST(SerializationBenchmark, RoundTripPerformance) {
  // Initialize serializers before use
  serialization::init_serializers();

  std::cout << "\n=== Round-Trip Performance ===" << std::endl;

  auto test_meta = create_large_test_metadata();

  #ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
  {
    auto facade = serialization::FacadeFactory::create(
        serialization::SerializationFormat::MODERN_THRIFT);

    auto thrift_time = benchmark([&]() {
      auto bytes = facade->serialize(test_meta);
      auto result = facade->deserialize(bytes);
    });

    std::cout << "Modern Thrift:  " << thrift_time << " μs/op (full round-trip)" << std::endl;
  }
  #endif
}
#else

// Thrift not available - skip benchmark
TEST(SerializationBenchmark, thrift_unavailable) {
  GTEST_SKIP() << "Thrift serialization not enabled - skipping serialization benchmark tests";
}

#endif // DWARFS_HAVE_EXPERIMENTAL_THRIFT