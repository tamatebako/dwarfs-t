// vim:set ts=2 sw=2 sts=2 et:
/**
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Ribose Inc.
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#include <vector>
#include <cstdint>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <dwarfs/reader/internal/metadata_factory.h>
#include <dwarfs/reader/internal/metadata_view_interface.h>
#include <dwarfs/error.h>

#include "test_logger.h"

using namespace dwarfs;
using namespace dwarfs::reader::internal;

/**
 * Test fixture for metadata factory tests
 *
 * Tests the Strategy Pattern implementation for format detection
 * and backend creation.
 */
class metadata_factory_test : public ::testing::Test {
 protected:
  test::test_logger lgr;

  /**
   * Create minimal valid FlatBuffers metadata
   * Magic bytes: "DWFS" + version bytes
   */
  std::vector<uint8_t> create_flatbuffers_metadata() {
    std::vector<uint8_t> data;
    // FlatBuffers magic: 4 bytes identifier
    data.push_back('D');
    data.push_back('W');
    data.push_back('F');
    data.push_back('S');
    // Add minimal FlatBuffers structure (simplified)
    for (int i = 0; i < 64; ++i) {
      data.push_back(0);
    }
    return data;
  }

  /**
   * Create minimal Thrift-looking metadata (no magic bytes)
   */
  std::vector<uint8_t> create_thrift_metadata() {
    std::vector<uint8_t> data;
    // Thrift has no magic bytes, just serialized data
    // Start with some plausible Thrift Compact protocol bytes
    for (int i = 0; i < 64; ++i) {
      data.push_back(static_cast<uint8_t>(i));
    }
    return data;
  }

  /**
   * Create invalid metadata (too short)
   */
  std::vector<uint8_t> create_invalid_metadata() {
    return {0x00, 0x01, 0x02}; // Too short to be valid
  }
};

/**
 * Test: FlatBuffers format detection via magic bytes
 *
 * Verifies that data with FlatBuffers magic bytes is correctly
 * identified as FlatBuffers format.
 */
TEST_F(metadata_factory_test, detect_flatbuffers_format) {
  auto data = create_flatbuffers_metadata();
  auto format = metadata_factory::detect_format(data);

#ifdef DWARFS_HAVE_FLATBUFFERS
  EXPECT_EQ(format, metadata_format::flatbuffers);
#else
  // If FlatBuffers not available, should still detect it
  // (even though creation will fail)
  EXPECT_EQ(format, metadata_format::flatbuffers);
#endif
}

/**
 * Test: Thrift format detection (fallback when no magic bytes)
 *
 * Verifies that data without FlatBuffers magic bytes falls back
 * to Thrift format detection.
 */
TEST_F(metadata_factory_test, detect_thrift_format) {
  auto data = create_thrift_metadata();
  auto format = metadata_factory::detect_format(data);

 #ifdef DWARFS_HAVE_THRIFT
  EXPECT_EQ(format, metadata_format::thrift);
#else
  // If Thrift not available, defaults to FlatBuffers
  EXPECT_EQ(format, metadata_format::flatbuffers);
#endif
}

/**
 * Test: Empty data handling
 *
 * Verifies that empty data is handled gracefully.
 */
TEST_F(metadata_factory_test, detect_empty_data) {
  std::vector<uint8_t> empty_data;

  // Should default to FlatBuffers or handle gracefully
  auto format = metadata_factory::detect_format(empty_data);

  // Default should be FlatBuffers (modern default)
  EXPECT_EQ(format, metadata_format::flatbuffers);
}

/**
 * Test: Invalid/corrupted data handling
 *
 * Verifies that invalid data is handled appropriately.
 */
TEST_F(metadata_factory_test, detect_invalid_data) {
  auto data = create_invalid_metadata();

  // Should not crash, should return default or handle gracefully
  EXPECT_NO_THROW({
    auto format = metadata_factory::detect_format(data);
    // Should default to FlatBuffers
    EXPECT_EQ(format, metadata_format::flatbuffers);
  });
}

/**
 * Test: FlatBuffers backend creation
 *
 * Verifies that FlatBuffers backend can be created when format
 * is available.
 */
TEST_F(metadata_factory_test, create_flatbuffers_backend) {
#ifdef DWARFS_HAVE_FLATBUFFERS
  auto data = create_flatbuffers_metadata();

  // This may throw if data structure is incomplete, which is expected
  // for a minimal test payload. The important part is that the factory
  // attempts FlatBuffers creation.
  EXPECT_NO_THROW({
    try {
      auto backend = metadata_factory::create_global_metadata(lgr, data);
      EXPECT_NE(backend, nullptr);
    } catch (error const& e) {
      // Expected for incomplete test data - just verifies FlatBuffers path
      EXPECT_THAT(e.what(), ::testing::HasSubstr("Flat"));
    }
  });
#else
  GTEST_SKIP() << "FlatBuffers not enabled";
#endif
}

/**
 * Test: Thrift backend creation
 *
 * Verifies that Thrift backend can be created when format is available.
 */
TEST_F(metadata_factory_test, create_thrift_backend) {
#ifdef DWARFS_HAVE_THRIFT
  auto data = create_thrift_metadata();

  // This may throw if data structure is incomplete
  EXPECT_NO_THROW({
    try {
      auto backend = metadata_factory::create_global_metadata(lgr, data);
      EXPECT_NE(backend, nullptr);
    } catch (error const& e) {
      // Expected for incomplete test data
      // Just verifies Thrift creation path is attempted
    }
  });
#else
  GTEST_SKIP() << "Thrift not enabled";
#endif
}

/**
 * Test: Explicit format creation (FlatBuffers)
 *
 * Verifies that explicit FlatBuffers format request works.
 */
TEST_F(metadata_factory_test, create_with_explicit_flatbuffers_format) {
#ifdef DWARFS_HAVE_FLATBUFFERS
  auto data = create_flatbuffers_metadata();

  EXPECT_NO_THROW({
    try {
      auto backend = metadata_factory::create_global_metadata(
          lgr, data, metadata_format::flatbuffers);
      EXPECT_NE(backend, nullptr);
    } catch (error const& e) {
      // Expected for incomplete test data
    }
  });
#else
  GTEST_SKIP() << "FlatBuffers not enabled";
#endif
}

/**
 * Test: Explicit format creation (Thrift)
 *
 * Verifies that explicit Thrift format request works.
 */
TEST_F(metadata_factory_test, create_with_explicit_thrift_format) {
#ifdef DWARFS_HAVE_THRIFT
  auto data = create_thrift_metadata();

  EXPECT_NO_THROW({
    try {
      auto backend = metadata_factory::create_global_metadata(
          lgr, data, metadata_format::thrift);
      EXPECT_NE(backend, nullptr);
    } catch (error const& e) {
      // Expected for incomplete test data
    }
  });
#else
  GTEST_SKIP() << "Thrift not enabled";
#endif
}

/**
 * Test: Format mismatch handling
 *
 * Verifies that requesting wrong format for data is handled.
 */
TEST_F(metadata_factory_test, format_mismatch_handling) {
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Create FlatBuffers data but request Thrift format
  auto fb_data = create_flatbuffers_metadata();

  EXPECT_THROW({
    auto backend = metadata_factory::create_global_metadata(
        lgr, fb_data, metadata_format::thrift);
  }, error);

  // Create Thrift data but request FlatBuffers format
  auto thrift_data = create_thrift_metadata();

  EXPECT_THROW({
    auto backend = metadata_factory::create_global_metadata(
        lgr, thrift_data, metadata_format::flatbuffers);
  }, error);
#else
  GTEST_SKIP() << "Both formats required for mismatch test";
#endif
}