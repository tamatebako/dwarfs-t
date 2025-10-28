/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Cereal binary serializer unit tests
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 *
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "dwarfs/metadata/serialization/cereal_binary_serializer.h"
#include "dwarfs/metadata/domain/metadata.h"

using namespace dwarfs::metadata::serialization;
using namespace dwarfs::metadata::domain;

/**
 * Test fixture for Cereal serializer tests
 */
class CerealSerializerTest : public ::testing::Test {
protected:
  CerealBinarySerializer serializer;

  /**
   * Create a minimal valid metadata object
   */
  metadata create_minimal_metadata() {
    metadata meta;
    meta.block_size = 131072;
    meta.total_fs_size = 1024 * 1024;
    meta.timestamp_base = 1609459200;
    return meta;
  }

  /**
   * Create a comprehensive metadata object with all fields
   */
  metadata create_comprehensive_metadata() {
    metadata meta = create_minimal_metadata();

    // Add chunks
    meta.chunks.push_back(chunk{0, 0, 4096});
    meta.chunks.push_back(chunk{0, 4096, 8192});
    meta.chunks.push_back(chunk{1, 0, 2048});

    // Add directories
    meta.directories.push_back(directory{0, 0});
    meta.directories.push_back(directory{3, 0});

    // Add inodes
    meta.inodes.push_back(inode_data{});
    meta.inodes.push_back(inode_data{});

    // Add chunk table
    meta.chunk_table = {0, 1, 3};

    // Add symlink table
    meta.symlink_table = {0, 1};

    // Add UIDs, GIDs, modes
    meta.uids = {1000, 0};
    meta.gids = {1000, 0};
    meta.modes = {0644, 0755};

    // Add names and symlinks
    meta.names = {"file1.txt", "file2.txt", "directory"};
    meta.symlinks = {"/usr/bin/python", "../lib/lib.so"};

    // Add optional fields
    meta.dwarfs_version = "0.7.0";
    meta.create_timestamp = 1609459200;
    meta.preferred_path_separator = '/';

    // Add features
    meta.features = std::set<std::string>{"symlinks", "devices"};

    // Add dir_entries
    std::vector<dir_entry> entries;
    entries.push_back(dir_entry{1, 0});
    entries.push_back(dir_entry{2, 1});
    meta.dir_entries = entries;

    return meta;
  }
};

/**
 * Test basic serialization/deserialization round-trip
 */
TEST_F(CerealSerializerTest, BasicRoundTrip) {
  metadata meta = create_minimal_metadata();

  // Serialize
  auto data = serializer.serialize(meta);

  // Data should not be empty
  EXPECT_GT(data.size(), 0u);

  // Deserialize
  auto meta2 = serializer.deserialize(data);

  ASSERT_NE(meta2, nullptr);
  EXPECT_EQ(meta2->block_size, meta.block_size);
  EXPECT_EQ(meta2->total_fs_size, meta.total_fs_size);
  EXPECT_EQ(meta2->timestamp_base, meta.timestamp_base);
}

/**
 * Test magic bytes are correctly written
 */
TEST_F(CerealSerializerTest, MagicBytesPresent) {
  metadata meta = create_minimal_metadata();

  auto data = serializer.serialize(meta);

  // Check minimum size
  ASSERT_GE(data.size(), 3u);

  // Check magic bytes
  EXPECT_EQ(data[0], 0xCE);
  EXPECT_EQ(data[1], 0xEA);
  EXPECT_EQ(data[2], 0x01);
}

/**
 * Test comprehensive metadata round-trip
 */
TEST_F(CerealSerializerTest, ComprehensiveRoundTrip) {
  metadata meta = create_comprehensive_metadata();

  // Serialize
  auto data = serializer.serialize(meta);

  // Deserialize
  auto meta2 = serializer.deserialize(data);

  ASSERT_NE(meta2, nullptr);

  // Check basic fields
  EXPECT_EQ(meta2->block_size, meta.block_size);
  EXPECT_EQ(meta2->total_fs_size, meta.total_fs_size);
  EXPECT_EQ(meta2->timestamp_base, meta.timestamp_base);

  // Check chunks
  ASSERT_EQ(meta2->chunks.size(), meta.chunks.size());
  for (size_t i = 0; i < meta.chunks.size(); ++i) {
    EXPECT_EQ(meta2->chunks[i].block, meta.chunks[i].block);
    EXPECT_EQ(meta2->chunks[i].offset, meta.chunks[i].offset);
    EXPECT_EQ(meta2->chunks[i].size, meta.chunks[i].size);
  }

  // Check names
  ASSERT_EQ(meta2->names.size(), meta.names.size());
  for (size_t i = 0; i < meta.names.size(); ++i) {
    EXPECT_EQ(meta2->names[i], meta.names[i]);
  }

  // Check symlinks
  ASSERT_EQ(meta2->symlinks.size(), meta.symlinks.size());
  for (size_t i = 0; i < meta.symlinks.size(); ++i) {
    EXPECT_EQ(meta2->symlinks[i], meta.symlinks[i]);
  }

  // Check optional fields
  ASSERT_TRUE(meta2->dwarfs_version.has_value());
  EXPECT_EQ(*meta2->dwarfs_version, *meta.dwarfs_version);

  ASSERT_TRUE(meta2->create_timestamp.has_value());
  EXPECT_EQ(*meta2->create_timestamp, *meta.create_timestamp);

  ASSERT_TRUE(meta2->preferred_path_separator.has_value());
  EXPECT_EQ(*meta2->preferred_path_separator, *meta.preferred_path_separator);

  // Check features
  ASSERT_TRUE(meta2->features.has_value());
  EXPECT_EQ(*meta2->features, *meta.features);
}

/**
 * Test empty metadata serialization
 */
TEST_F(CerealSerializerTest, EmptyMetadata) {
  metadata meta;

  auto data = serializer.serialize(meta);
  EXPECT_GT(data.size(), 3u); // At least magic bytes + some data

  auto meta2 = serializer.deserialize(data);
  ASSERT_NE(meta2, nullptr);

  EXPECT_EQ(meta2->chunks.size(), 0u);
  EXPECT_EQ(meta2->directories.size(), 0u);
  EXPECT_EQ(meta2->inodes.size(), 0u);
}

/**
 * Test deserialization with invalid data size
 */
TEST_F(CerealSerializerTest, DeserializeInvalidSize) {
  std::vector<uint8_t> data = {0xCE, 0xEA}; // Too small

  EXPECT_THROW(serializer.deserialize(data), std::invalid_argument);
}

/**
 * Test deserialization with wrong magic bytes
 */
TEST_F(CerealSerializerTest, DeserializeWrongMagicBytes) {
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x00, 0x00};

  EXPECT_THROW(serializer.deserialize(data), std::invalid_argument);
}

/**
 * Test deserialization with partially correct magic bytes
 */
TEST_F(CerealSerializerTest, DeserializePartialMagicBytes) {
  std::vector<uint8_t> data = {0xCE, 0xEA, 0x00, 0x00, 0x00};

  EXPECT_THROW(serializer.deserialize(data), std::invalid_argument);
}

/**
 * Test deserialization with corrupted data
 */
TEST_F(CerealSerializerTest, DeserializeCorruptedData) {
  metadata meta = create_minimal_metadata();
  auto data = serializer.serialize(meta);

  // Corrupt the data (but keep magic bytes)
  if (data.size() > 10) {
    data[5] = ~data[5];
    data[6] = ~data[6];
    data[7] = ~data[7];
  }

  // Deserialization should throw
  EXPECT_THROW(serializer.deserialize(data), std::runtime_error);
}

/**
 * Test get_format_name
 */
TEST_F(CerealSerializerTest, GetFormatName) {
  auto name = serializer.get_format_name();
  EXPECT_EQ(name, "Cereal Binary");
}

/**
 * Test get_format
 */
TEST_F(CerealSerializerTest, GetFormat) {
  auto format = serializer.get_format();
  EXPECT_EQ(format, SerializationFormat::CEREAL_BINARY);
}

/**
 * Test serialization determinism
 */
TEST_F(CerealSerializerTest, SerializationDeterminism) {
  metadata meta = create_comprehensive_metadata();

  // Serialize twice
  auto data1 = serializer.serialize(meta);
  auto data2 = serializer.serialize(meta);

  // Results should be identical
  EXPECT_EQ(data1, data2);
}

/**
 * Test large metadata object
 */
TEST_F(CerealSerializerTest, LargeMetadata) {
  metadata meta = create_minimal_metadata();

  // Add many chunks
  for (uint32_t i = 0; i < 10000; ++i) {
    meta.chunks.push_back(chunk{i / 1000, (i % 1000) * 128, 128});
  }

  // Add many names
  for (int i = 0; i < 1000; ++i) {
    meta.names.push_back("very_long_filename_with_lots_of_characters_" +
                         std::to_string(i) + ".txt");
  }

  // Serialize
  auto data = serializer.serialize(meta);

  // Should produce significant data
  EXPECT_GT(data.size(), 10000u);

  // Deserialize
  auto meta2 = serializer.deserialize(data);

  ASSERT_NE(meta2, nullptr);
  EXPECT_EQ(meta2->chunks.size(), 10000u);
  EXPECT_EQ(meta2->names.size(), 1000u);
}

/**
 * Test metadata with all optional fields set
 */
TEST_F(CerealSerializerTest, AllOptionalFields) {
  metadata meta = create_minimal_metadata();

  // Set all optional fields
  meta.devices = std::vector<uint64_t>{0x0801, 0x0802};
  meta.options = fs_options{};
  meta.dir_entries = std::vector<dir_entry>{{1, 0}, {2, 1}};
  meta.shared_files_table = std::vector<uint32_t>{0, 1, 2};
  meta.total_hardlink_size = 1024 * 1024;
  meta.dwarfs_version = "0.7.0";
  meta.create_timestamp = 1609459200;
  meta.compact_names = string_table{};
  meta.compact_symlinks = string_table{};
  meta.preferred_path_separator = '/';
  meta.features = std::set<std::string>{"feature1", "feature2"};
  meta.category_names = std::vector<std::string>{"cat1", "cat2"};
  meta.block_categories = std::vector<uint32_t>{0, 1, 0, 1};

  // Serialize and deserialize
  auto data = serializer.serialize(meta);
  auto meta2 = serializer.deserialize(data);

  ASSERT_NE(meta2, nullptr);

  // Verify optional fields
  ASSERT_TRUE(meta2->devices.has_value());
  EXPECT_EQ(meta2->devices->size(), 2u);

  ASSERT_TRUE(meta2->dwarfs_version.has_value());
  EXPECT_EQ(*meta2->dwarfs_version, "0.7.0");

  ASSERT_TRUE(meta2->features.has_value());
  EXPECT_EQ(meta2->features->size(), 2u);

  ASSERT_TRUE(meta2->category_names.has_value());
  EXPECT_EQ(meta2->category_names->size(), 2u);
}

/**
 * Test metadata with special characters in strings
 */
TEST_F(CerealSerializerTest, SpecialCharactersInStrings) {
  metadata meta = create_minimal_metadata();

  // Add names with special characters
  meta.names.push_back("file with spaces.txt");
  meta.names.push_back("file_with_unicode_🔥.txt");
  meta.names.push_back("file\twith\ttabs.txt");
  meta.names.push_back("file\nwith\nnewlines.txt");

  // Add symlinks with special paths
  meta.symlinks.push_back("../../../etc/passwd");
  meta.symlinks.push_back("/path/with spaces/file.txt");

  auto data = serializer.serialize(meta);
  auto meta2 = serializer.deserialize(data);

  ASSERT_NE(meta2, nullptr);
  ASSERT_EQ(meta2->names.size(), 4u);
  EXPECT_EQ(meta2->names[0], "file with spaces.txt");
  EXPECT_EQ(meta2->names[1], "file_with_unicode_🔥.txt");

  ASSERT_EQ(meta2->symlinks.size(), 2u);
  EXPECT_EQ(meta2->symlinks[0], "../../../etc/passwd");
}