/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Integration tests for metadata serialization framework
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 *
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>

#include <vector>

#include "dwarfs/metadata/reader.h"
#include "dwarfs/metadata/writer.h"
#include "dwarfs/metadata/serialization/format_detector.h"
#include "dwarfs/metadata/serialization/serializer_factory.h"
#include "dwarfs/metadata/domain/metadata.h"

using namespace dwarfs::metadata;
using namespace dwarfs::metadata::serialization;
using namespace dwarfs::metadata::domain;

/**
 * Test fixture for integration tests
 */
class IntegrationTest : public ::testing::Test {
protected:
  /**
   * Create a test metadata object
   */
  metadata create_test_metadata() {
    metadata meta;
    meta.block_size = 131072;
    meta.total_fs_size = 10 * 1024 * 1024;
    meta.timestamp_base = 1609459200;

    // Add some chunks
    meta.chunks.push_back(chunk{0, 0, 4096});
    meta.chunks.push_back(chunk{0, 4096, 8192});
    meta.chunks.push_back(chunk{1, 0, 2048});

    // Add names
    meta.names = {"file1.txt", "file2.txt", "dir1", "dir2"};

    // Add symlinks
    meta.symlinks = {"/usr/bin/python3", "../lib/libtest.so"};

    // Add optional fields
    meta.dwarfs_version = "0.7.0";
    meta.create_timestamp = 1609459200;

    return meta;
  }
};

/**
 * Test MetadataReader with auto-detection (Cereal format)
 */
TEST_F(IntegrationTest, ReaderAutoDetectCereal) {
  auto meta = create_test_metadata();

  // Write with Cereal
  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  auto data = writer.write(meta);

  // Read with auto-detection
  MetadataReader reader;
  auto meta2 = reader.read(data);

  ASSERT_NE(meta2, nullptr);
  EXPECT_EQ(meta2->block_size, meta.block_size);
  EXPECT_EQ(meta2->total_fs_size, meta.total_fs_size);
  EXPECT_EQ(meta2->chunks.size(), meta.chunks.size());
}

/**
 * Test MetadataReader with explicit format
 */
TEST_F(IntegrationTest, ReaderExplicitFormat) {
  auto meta = create_test_metadata();

  // Write with Cereal
  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  auto data = writer.write(meta);

  // Read with explicit Cereal format
  MetadataReader reader(SerializationFormat::CEREAL_BINARY);
  auto meta2 = reader.read(data);

  ASSERT_NE(meta2, nullptr);
  EXPECT_EQ(meta2->block_size, meta.block_size);
}

/**
 * Test MetadataReader format detection
 */
TEST_F(IntegrationTest, ReaderFormatDetection) {
  auto meta = create_test_metadata();

  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  auto data = writer.write(meta);

  MetadataReader reader;
  auto detected_format = reader.detect_format(data);

  EXPECT_EQ(detected_format, SerializationFormat::CEREAL_BINARY);
}

/**
 * Test MetadataReader format info
 */
TEST_F(IntegrationTest, ReaderFormatInfo) {
  auto meta = create_test_metadata();

  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  auto data = writer.write(data);

  MetadataReader reader;
  auto info = reader.get_format_info(data);

  EXPECT_FALSE(info.empty());
  EXPECT_TRUE(info.find("Cereal") != std::string::npos);
}

/**
 * Test MetadataWriter with Cereal format
 */
TEST_F(IntegrationTest, WriterCerealFormat) {
  auto meta = create_test_metadata();

  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  auto data = writer.write(meta);

  // Verify magic bytes
  ASSERT_GE(data.size(), 3u);
  EXPECT_EQ(data[0], 0xCE);
  EXPECT_EQ(data[1], 0xEA);
  EXPECT_EQ(data[2], 0x01);

  // Verify round-trip
  MetadataReader reader;
  auto meta2 = reader.read(data);
  ASSERT_NE(meta2, nullptr);
  EXPECT_EQ(meta2->block_size, meta.block_size);
}

/**
 * Test MetadataWriter get_format
 */
TEST_F(IntegrationTest, WriterGetFormat) {
  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  EXPECT_EQ(writer.get_format(), SerializationFormat::CEREAL_BINARY);
}

/**
 * Test FormatDetector with Cereal data
 */
TEST_F(IntegrationTest, FormatDetectorCereal) {
  std::vector<uint8_t> data = {0xCE, 0xEA, 0x01, 0x00, 0x00};

  auto format = FormatDetector::detect_format(data);
  EXPECT_EQ(format, SerializationFormat::CEREAL_BINARY);
}

/**
 * Test FormatDetector with Thrift data
 */
TEST_F(IntegrationTest, FormatDetectorThrift) {
  std::vector<uint8_t> data = {0x82, 0x21, 0x00, 0x00, 0x00};

  auto format = FormatDetector::detect_format(data);
  EXPECT_EQ(format, SerializationFormat::THRIFT_COMPACT);
}

/**
 * Test FormatDetector with unknown data
 */
TEST_F(IntegrationTest, FormatDetectorUnknown) {
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00, 0x00};

  EXPECT_THROW(FormatDetector::detect_format(data), std::runtime_error);
}

/**
 * Test FormatDetector with insufficient data
 */
TEST_F(IntegrationTest, FormatDetectorInsufficientData) {
  std::vector<uint8_t> data = {0xCE};

  EXPECT_THROW(FormatDetector::detect_format(data), std::invalid_argument);
}

/**
 * Test FormatDetector get_format_info
 */
TEST_F(IntegrationTest, FormatDetectorInfo) {
  std::vector<uint8_t> cereal_data = {0xCE, 0xEA, 0x01, 0x00, 0x00};
  auto info = FormatDetector::get_format_info(cereal_data);

  EXPECT_FALSE(info.empty());
  EXPECT_TRUE(info.find("Cereal") != std::string::npos);
}

/**
 * Test SerializerFactory create with Cereal
 */
TEST_F(IntegrationTest, FactoryCreateCereal) {
  auto serializer = SerializerFactory::create(SerializationFormat::CEREAL_BINARY);

  ASSERT_NE(serializer, nullptr);
  EXPECT_EQ(serializer->get_format(), SerializationFormat::CEREAL_BINARY);
}

/**
 * Test SerializerFactory create with Thrift
 */
TEST_F(IntegrationTest, FactoryCreateThrift) {
  auto serializer = SerializerFactory::create(SerializationFormat::THRIFT_COMPACT);

  ASSERT_NE(serializer, nullptr);
  EXPECT_EQ(serializer->get_format(), SerializationFormat::THRIFT_COMPACT);
}

/**
 * Test SerializerFactory with invalid format
 */
TEST_F(IntegrationTest, FactoryInvalidFormat) {
  EXPECT_THROW(
    SerializerFactory::create(SerializationFormat::AUTO_DETECT),
    std::invalid_argument
  );
}

/**
 * Test SerializerFactory is_supported
 */
TEST_F(IntegrationTest, FactoryIsSupported) {
  EXPECT_TRUE(SerializerFactory::is_supported(SerializationFormat::CEREAL_BINARY));
  EXPECT_TRUE(SerializerFactory::is_supported(SerializationFormat::THRIFT_COMPACT));
  EXPECT_FALSE(SerializerFactory::is_supported(SerializationFormat::AUTO_DETECT));
}

/**
 * Test full round-trip: write -> detect -> read
 */
TEST_F(IntegrationTest, FullRoundTrip) {
  auto meta = create_test_metadata();

  // Write
  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  auto data = writer.write(meta);

  // Detect format
  auto detected_format = FormatDetector::detect_format(data);
  EXPECT_EQ(detected_format, SerializationFormat::CEREAL_BINARY);

  // Read
  MetadataReader reader;
  auto meta2 = reader.read(data);

  // Verify
  ASSERT_NE(meta2, nullptr);
  EXPECT_EQ(meta2->block_size, meta.block_size);
  EXPECT_EQ(meta2->total_fs_size, meta.total_fs_size);
  EXPECT_EQ(meta2->timestamp_base, meta.timestamp_base);

  // Verify collections
  ASSERT_EQ(meta2->chunks.size(), meta.chunks.size());
  ASSERT_EQ(meta2->names.size(), meta.names.size());
  ASSERT_EQ(meta2->symlinks.size(), meta.symlinks.size());
}

/**
 * Test error handling in MetadataReader
 */
TEST_F(IntegrationTest, ReaderErrorHandling) {
  MetadataReader reader;

  // Invalid data
  std::vector<uint8_t> invalid_data = {0x00, 0x00, 0x00};
  EXPECT_THROW(reader.read(invalid_data), std::exception);

  // Empty data
  std::vector<uint8_t> empty_data;
  EXPECT_THROW(reader.read(empty_data), std::exception);
}

/**
 * Test Reader/Writer consistency
 */
TEST_F(IntegrationTest, ReaderWriterConsistency) {
  auto meta = create_test_metadata();

  // Write with writer
  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  auto data = writer.write(meta);

  // Read with reader
  MetadataReader reader(SerializationFormat::CEREAL_BINARY);
  auto meta2 = reader.read(data);

  // Should get same format
  EXPECT_EQ(writer.get_format(), SerializationFormat::CEREAL_BINARY);
  EXPECT_EQ(reader.get_format(), SerializationFormat::CEREAL_BINARY);
}

/**
 * Test multiple reads don't affect data
 */
TEST_F(IntegrationTest, MultipleReads) {
  auto meta = create_test_metadata();

  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  auto data = writer.write(meta);

  MetadataReader reader;

  // Read multiple times
  auto meta1 = reader.read(data);
  auto meta2 = reader.read(data);
  auto meta3 = reader.read(data);

  // All should succeed
  ASSERT_NE(meta1, nullptr);
  ASSERT_NE(meta2, nullptr);
  ASSERT_NE(meta3, nullptr);

  // All should have same data
  EXPECT_EQ(meta1->block_size, meta2->block_size);
  EXPECT_EQ(meta2->block_size, meta3->block_size);
}

/**
 * Test format detection accuracy
 */
TEST_F(IntegrationTest, FormatDetectionAccuracy) {
  auto meta = create_test_metadata();

  // Write with Cereal
  MetadataWriter cereal_writer(SerializationFormat::CEREAL_BINARY);
  auto cereal_data = cereal_writer.write(meta);

  // Detect should identify Cereal
  EXPECT_EQ(
    FormatDetector::detect_format(cereal_data),
    SerializationFormat::CEREAL_BINARY
  );

  // Create Thrift-like data
  std::vector<uint8_t> thrift_data = {0x82, 0x21, 0x00, 0x00, 0x00};

  // Detect should identify Thrift
  EXPECT_EQ(
    FormatDetector::detect_format(thrift_data),
    SerializationFormat::THRIFT_COMPACT
  );

  // They should be different
  EXPECT_NE(
    FormatDetector::detect_format(cereal_data),
    FormatDetector::detect_format(thrift_data)
  );
}

/**
 * Test complex metadata object
 */
TEST_F(IntegrationTest, ComplexMetadataRoundTrip) {
  metadata meta = create_test_metadata();

  // Add more complex data
  meta.features = std::set<std::string>{"symlinks", "devices", "packed_dirs"};
  meta.category_names = std::vector<std::string>{"text", "binary", "image"};
  meta.block_categories = std::vector<uint32_t>{0, 1, 2, 0, 1};

  // Add devices
  meta.devices = std::vector<uint64_t>{0x0801, 0x0802, 0x0803};

  // Write and read
  MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
  auto data = writer.write(meta);

  MetadataReader reader;
  auto meta2 = reader.read(data);

  // Verify complex fields
  ASSERT_TRUE(meta2->features.has_value());
  EXPECT_EQ(meta2->features->size(), 3u);
  EXPECT_TRUE(meta2->features->count("symlinks"));

  ASSERT_TRUE(meta2->category_names.has_value());
  EXPECT_EQ(meta2->category_names->size(), 3u);

  ASSERT_TRUE(meta2->devices.has_value());
  EXPECT_EQ(meta2->devices->size(), 3u);
}

/**
 * Test Reader is_auto_detect
 */
TEST_F(IntegrationTest, ReaderIsAutoDetect) {
  MetadataReader auto_reader;
  EXPECT_TRUE(auto_reader.is_auto_detect());

  MetadataReader cereal_reader(SerializationFormat::CEREAL_BINARY);
  EXPECT_FALSE(cereal_reader.is_auto_detect());
}