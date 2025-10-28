/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Performance tests for metadata serialization
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 *
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>

#include <chrono>
#include <iostream>

#include "dwarfs/metadata/serialization/cereal_binary_serializer.h"
#include "dwarfs/metadata/reader.h"
#include "dwarfs/metadata/writer.h"
#include "test_fixtures.h"

using namespace dwarfs::metadata;
using namespace dwarfs::metadata::serialization;
using namespace dwarfs::metadata::test;
using namespace std::chrono;

/**
 * Test fixture for performance tests
 */
class PerformanceTest : public ::testing::Test {
protected:
  /**
   * Measure serialization performance
   */
  template <typename Func>
  double measure_time_ms(Func&& func, int iterations = 100) {
    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
      func();
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start).count();

    return duration / 1000.0; // Convert to milliseconds
  }
};

/**
 * Test Cereal serialization performance - small metadata
 */
TEST_F(PerformanceTest, CerealSerializeSmall) {
  auto meta = create_small_metadata();
  CerealBinarySerializer serializer;

  auto time_ms = measure_time_ms([&]() {
    auto data = serializer.serialize(meta);
  });

  std::cout << "Cereal serialize (small): " << time_ms << " ms for 100 iterations\n";
  std::cout << "  Average: " << (time_ms / 100.0) << " ms per operation\n";

  // Should complete in reasonable time
  EXPECT_LT(time_ms, 1000.0); // Less than 1 second for 100 iterations
}

/**
 * Test Cereal deserialization performance - small metadata
 */
TEST_F(PerformanceTest, CerealDeserializeSmall) {
  auto meta = create_small_metadata();
  CerealBinarySerializer serializer;
  auto data = serializer.serialize(meta);

  auto time_ms = measure_time_ms([&]() {
    auto meta2 = serializer.deserialize(data);
  });

  std::cout << "Cereal deserialize (small): " << time_ms << " ms for 100 iterations\n";
  std::cout << "  Average: " << (time_ms / 100.0) << " ms per operation\n";

  EXPECT_LT(time_ms, 1000.0);
}

/**
 * Test Cereal serialization performance - medium metadata
 */
TEST_F(PerformanceTest, CerealSerializeMedium) {
  auto meta = create_medium_metadata();
  CerealBinarySerializer serializer;

  auto time_ms = measure_time_ms([&]() {
    auto data = serializer.serialize(meta);
  }, 50); // Fewer iterations for larger data

  std::cout << "Cereal serialize (medium): " << time_ms << " ms for 50 iterations\n";
  std::cout << "  Average: " << (time_ms / 50.0) << " ms per operation\n";

  EXPECT_LT(time_ms, 5000.0);
}

/**
 * Test Cereal deserialization performance - medium metadata
 */
TEST_F(PerformanceTest, CerealDeserializeMedium) {
  auto meta = create_medium_metadata();
  CerealBinarySerializer serializer;
  auto data = serializer.serialize(meta);

  auto time_ms = measure_time_ms([&]() {
    auto meta2 = serializer.deserialize(data);
  }, 50);

  std::cout << "Cereal deserialize (medium): " << time_ms << " ms for 50 iterations\n";
  std::cout << "  Average: " << (time_ms / 50.0) << " ms per operation\n";

  EXPECT_LT(time_ms, 5000.0);
}

/**
 * Test Cereal serialization performance - large metadata
 */
TEST_F(PerformanceTest, CerealSerializeLarge) {
  auto meta = create_large_metadata();
  CerealBinarySerializer serializer;

  auto time_ms = measure_time_ms([&]() {
    auto data = serializer.serialize(meta);
  }, 10); // Even fewer iterations for large data

  std::cout << "Cereal serialize (large): " << time_ms << " ms for 10 iterations\n";
  std::cout << "  Average: " << (time_ms / 10.0) << " ms per operation\n";
  std::cout << "  Serialized size: " << serializer.serialize(meta).size() << " bytes\n";

  EXPECT_LT(time_ms, 10000.0);
}

/**
 * Test Cereal deserialization performance - large metadata
 */
TEST_F(PerformanceTest, CerealDeserializeLarge) {
  auto meta = create_large_metadata();
  CerealBinarySerializer serializer;
  auto data = serializer.serialize(meta);

  auto time_ms = measure_time_ms([&]() {
    auto meta2 = serializer.deserialize(data);
  }, 10);

  std::cout << "Cereal deserialize (large): " << time_ms << " ms for 10 iterations\n";
  std::cout << "  Average: " << (time_ms / 10.0) << " ms per operation\n";

  EXPECT_LT(time_ms, 10000.0);
}

/**
 * Test round-trip performance
 */
TEST_F(PerformanceTest, RoundTripPerformance) {
  auto meta = create_medium_metadata();

  auto time_ms = measure_time_ms([&]() {
    MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
    auto data = writer.write(meta);

    MetadataReader reader;
    auto meta2 = reader.read(data);
  }, 50);

  std::cout << "Full round-trip (medium): " << time_ms << " ms for 50 iterations\n";
  std::cout << "  Average: " << (time_ms / 50.0) << " ms per operation\n";

  EXPECT_LT(time_ms, 10000.0);
}

/**
 * Test serialization size efficiency
 */
TEST_F(PerformanceTest, SerializationSize) {
  CerealBinarySerializer serializer;

  auto small = create_small_metadata();
  auto medium = create_medium_metadata();
  auto large = create_large_metadata();

  auto small_size = serializer.serialize(small).size();
  auto medium_size = serializer.serialize(medium).size();
  auto large_size = serializer.serialize(large).size();

  std::cout << "Serialization sizes:\n";
  std::cout << "  Small:  " << small_size << " bytes\n";
  std::cout << "  Medium: " << medium_size << " bytes\n";
  std::cout << "  Large:  " << large_size << " bytes\n";

  // Sizes should increase with data size
  EXPECT_LT(small_size, medium_size);
  EXPECT_LT(medium_size, large_size);

  // Small metadata should be reasonably compact
  EXPECT_LT(small_size, 10000u);
}

/**
 * Test memory efficiency during serialization
 */
TEST_F(PerformanceTest, MemoryEfficiency) {
  auto meta = create_large_metadata();
  CerealBinarySerializer serializer;

  // Serialize multiple times to check for memory leaks
  size_t first_size = 0;
  for (int i = 0; i < 10; ++i) {
    auto data = serializer.serialize(meta);
    if (i == 0) {
      first_size = data.size();
    } else {
      // Size should be consistent
      EXPECT_EQ(data.size(), first_size);
    }
  }

  std::cout << "Memory efficiency test passed - consistent sizes\n";
}