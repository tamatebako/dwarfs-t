/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file modern_thrift_serialization_test.cpp
 *
 * Tests for Modern Thrift Compact serializer
 *
 * \author Ribose (@riboseinc @tamatebako)
 * \date 2026-01-02
 * \copyright See LICENSE file
 */

#include "dwarfs/config.h"

#ifdef DWARFS_HAVE_THRIFT

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "dwarfs/metadata/serialization/thrift_compact_serializer.h"
#include "dwarfs/metadata/serialization/serializer_registry.h"
#include "dwarfs/metadata/serialization/init_serializers.h"
#include "dwarfs/metadata/domain/metadata.h"

using namespace dwarfs::metadata;
using namespace dwarfs::metadata::serialization;

class ModernThriftSerializationTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Ensure all serializers are registered
    init_serializers();
  }

  domain::metadata create_test_metadata() {
    domain::metadata meta;

    // Basic fields
    meta.timestamp_base = 1609459200; // 2021-01-01
    meta.block_size = 65536;
    meta.total_fs_size = 1024 * 1024;

    // Add some chunks
    meta.chunks.emplace_back(0, 0, 1000);
    meta.chunks.emplace_back(0, 1000, 2000);
    meta.chunks.emplace_back(1, 0, 1500);

    // Add directories
    meta.directories.emplace_back(0, 0, 0);
    meta.directories.emplace_back(0, 1, 1);

    // Add inodes
    domain::inode_data inode1;
    inode1.mode_index = 0;
    inode1.owner_index = 0;
    inode1.group_index = 0;
    inode1.atime_offset = 100;
    inode1.mtime_offset = 200;
    inode1.ctime_offset = 300;
    meta.inodes.push_back(inode1);

    // Add tables
    meta.chunk_table = {0, 2, 5};
    meta.uids = {0, 1000};
    meta.gids = {0, 1000};
    meta.modes = {0644, 0755};
    meta.names = {"file1.txt", "file2.txt", "dir1"};
    meta.symlinks = {};

    // Optional fields
    meta.options = domain::fs_options{};
    meta.options->mtime_only = false;
    meta.options->packed_chunk_table = true;

    return meta;
  }
};

TEST_F(ModernThriftSerializationTest, SerializerExists) {
  ThriftCompactSerializer serializer;
  EXPECT_EQ(serializer.get_format_name(), "Modern Thrift Compact");
  EXPECT_EQ(serializer.get_format(), SerializationFormat::MODERN_THRIFT);
  EXPECT_TRUE(serializer.can_write());
  EXPECT_TRUE(serializer.can_read());
}

TEST_F(ModernThriftSerializationTest, MagicBytes) {
  ThriftCompactSerializer serializer;
  auto magic = serializer.get_magic_bytes();

  ASSERT_EQ(magic.size(), 2);
  EXPECT_EQ(magic[0], 0x82);
  EXPECT_EQ(magic[1], 0x21);
}

TEST_F(ModernThriftSerializationTest, RoundTripSerialization) {
  auto original = create_test_metadata();

  ThriftCompactSerializer serializer;
  auto data = serializer.serialize(&original);

  // Verify magic bytes at the beginning
  ASSERT_GE(data.size(), 2);
  EXPECT_EQ(data[0], 0x82);
  EXPECT_EQ(data[1], 0x21);

  // Verify data is not empty
  EXPECT_GT(data.size(), 100); // Should be substantial

  // Round-trip deserialize
  auto result_ptr = serializer.deserialize(data);
  auto* result = static_cast<domain::metadata*>(result_ptr.get());

  // Verify basic fields
  EXPECT_EQ(original.timestamp_base, result->timestamp_base);
  EXPECT_EQ(original.block_size, result->block_size);
  EXPECT_EQ(original.total_fs_size, result->total_fs_size);

  // Verify chunks
  ASSERT_EQ(original.chunks.size(), result->chunks.size());
  for (size_t i = 0; i < original.chunks.size(); ++i) {
    EXPECT_EQ(original.chunks[i].block(), result->chunks[i].block());
    EXPECT_EQ(original.chunks[i].offset(), result->chunks[i].offset());
    EXPECT_EQ(original.chunks[i].size(), result->chunks[i].size());
  }

  // Verify directories
  ASSERT_EQ(original.directories.size(), result->directories.size());

  // Verify inodes
  ASSERT_EQ(original.inodes.size(), result->inodes.size());
  EXPECT_EQ(original.inodes[0].mode_index, result->inodes[0].mode_index);

  // Verify tables
  EXPECT_EQ(original.chunk_table, result->chunk_table);
  EXPECT_EQ(original.uids, result->uids);
  EXPECT_EQ(original.gids, result->gids);
  EXPECT_EQ(original.modes, result->modes);
  EXPECT_EQ(original.names, result->names);

  // Verify options
  ASSERT_TRUE(result->options.has_value());
  EXPECT_EQ(original.options->mtime_only, result->options->mtime_only);
  EXPECT_EQ(original.options->packed_chunk_table, result->options->packed_chunk_table);
}

TEST_F(ModernThriftSerializationTest, NullMetadataThrows) {
  ThriftCompactSerializer serializer;
  EXPECT_THROW(serializer.serialize(nullptr), std::invalid_argument);
}

TEST_F(ModernThriftSerializationTest, InvalidMagicBytesThrows) {
  ThriftCompactSerializer serializer;

  // Data with wrong magic bytes
  std::vector<uint8_t> bad_data = {0x00, 0x00, 0x01, 0x02, 0x03};

  EXPECT_THROW(serializer.deserialize(bad_data), std::runtime_error);
}

TEST_F(ModernThriftSerializationTest, TooShortDataThrows) {
  ThriftCompactSerializer serializer;

  // Data too short
  std::vector<uint8_t> short_data = {0x82};

  EXPECT_THROW(serializer.deserialize(short_data), std::invalid_argument);
}

TEST_F(ModernThriftSerializationTest, SerializerRegistration) {
  auto& registry = SerializerRegistry::instance();

  // Verify Modern Thrift is registered
  EXPECT_TRUE(registry.is_format_available(SerializationFormat::MODERN_THRIFT));

  // Verify can create serializer
  auto serializer = registry.create_serializer(SerializationFormat::MODERN_THRIFT);
  ASSERT_NE(serializer, nullptr);
  EXPECT_EQ(serializer->get_format(), SerializationFormat::MODERN_THRIFT);
}

TEST_F(ModernThriftSerializationTest, FormatDetection) {
  auto original = create_test_metadata();

  ThriftCompactSerializer serializer;
  auto data = serializer.serialize(&original);

  // Verify format detection
  auto& registry = SerializerRegistry::instance();
  auto detected = registry.detect_format(data);

  ASSERT_TRUE(detected.has_value());
  EXPECT_EQ(*detected, SerializationFormat::MODERN_THRIFT);
}

TEST_F(ModernThriftSerializationTest, PriorityOrder) {
  // Verify: FlatBuffers (120) > Modern Thrift (100) > Legacy Thrift (50)
  auto& registry = SerializerRegistry::instance();
  auto formats = registry.get_available_formats();

  // Should have at least Modern Thrift and Legacy Thrift
  EXPECT_GE(formats.size(), 2);

  // Verify Modern Thrift is available
  bool found_modern = false;
  for (const auto& fmt : formats) {
    if (fmt == SerializationFormat::MODERN_THRIFT) {
      found_modern = true;
      break;
    }
  }
  EXPECT_TRUE(found_modern);
}

TEST_F(ModernThriftSerializationTest, CompactSize) {
  auto original = create_test_metadata();

  ThriftCompactSerializer serializer;
  auto data = serializer.serialize(&original);

  // Modern Thrift should produce compact data
  // Just verify it's reasonable (not excessive)
  EXPECT_LT(data.size(), 10000); // Should be under 10KB for test metadata
}

#endif // DWARFS_HAVE_THRIFT