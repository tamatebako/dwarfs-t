/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhx.io)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "dwarfs/metadata/serialization/serializer_registry.h"
#include "dwarfs/metadata/serialization/init_serializers.h"
#include "dwarfs/metadata/domain/metadata.h"

using namespace dwarfs::metadata;

class SerializerRegistryTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize all serializers
    serialization::init_serializers();
    registry_ = &serialization::SerializerRegistry::instance();
  }

  // Helper: Create test metadata
  domain::metadata create_test_metadata() {
    domain::metadata meta;
    meta.timestamp_base = 1609459200; // 2021-01-01 00:00:00 UTC
    meta.block_size = 262144;
    meta.total_fs_size = 1099511627776ULL; // 1 TiB

    // Add some chunks
    meta.chunks.emplace_back(domain::chunk(0, 0, 1024));
    meta.chunks.emplace_back(domain::chunk(1, 0, 2048));

    // Add directories
    meta.directories.emplace_back(domain::directory(0, 0, 0));
    meta.directories.emplace_back(domain::directory(0, 1, 1));

    // Add inodes
    domain::inode_data inode1;
    inode1.mode_index = 0;
    inode1.owner_index = 0;
    inode1.group_index = 0;
    meta.inodes.push_back(inode1);

    // Add names
    meta.names.push_back("root");
    meta.names.push_back("file.txt");

    // Add modes, uids, gids
    meta.modes.push_back(0755);
    meta.uids.push_back(1000);
    meta.gids.push_back(1000);

    return meta;
  }

  serialization::SerializerRegistry* registry_;
};

// Test 1: Legacy Thrift is registered
TEST_F(SerializerRegistryTest, LegacyThriftRegistered) {
  EXPECT_TRUE(registry_->is_format_available(
      serialization::SerializationFormat::LEGACY_THRIFT));
}

// Test 2: FlatBuffers is registered (if enabled)
TEST_F(SerializerRegistryTest, FlatBuffersRegistered) {
#ifdef DWARFS_HAVE_FLATBUFFERS
  EXPECT_TRUE(registry_->is_format_available(
      serialization::SerializationFormat::FLATBUFFERS));
#else
  EXPECT_FALSE(registry_->is_format_available(
      serialization::SerializationFormat::FLATBUFFERS));
#endif
}

// Test 3: Can create Legacy Thrift serializer
TEST_F(SerializerRegistryTest, CreateLegacyThrift) {
  auto serializer = registry_->create_serializer(
      serialization::SerializationFormat::LEGACY_THRIFT);

  ASSERT_NE(serializer, nullptr);
  EXPECT_EQ(serializer->get_format(),
            serialization::SerializationFormat::LEGACY_THRIFT);
  EXPECT_EQ(serializer->get_format_name(), "Legacy Thrift");
  EXPECT_TRUE(serializer->can_read());
  EXPECT_TRUE(serializer->can_write());
}

// Test 4: Legacy Thrift round-trip via registry
TEST_F(SerializerRegistryTest, LegacyThrift_RoundTrip) {
  auto meta = create_test_metadata();

  // Serialize
  auto serializer = registry_->create_serializer(
      serialization::SerializationFormat::LEGACY_THRIFT);
  auto data = serializer->serialize(&meta);

  EXPECT_FALSE(data.empty());

  // Deserialize
  auto deserialized_ptr = serializer->deserialize(data);
  auto* deserialized = static_cast<domain::metadata*>(deserialized_ptr.get());

  // Verify
  EXPECT_EQ(deserialized->timestamp_base, meta.timestamp_base);
  EXPECT_EQ(deserialized->block_size, meta.block_size);
  EXPECT_EQ(deserialized->total_fs_size, meta.total_fs_size);
  EXPECT_EQ(deserialized->chunks.size(), meta.chunks.size());
  EXPECT_EQ(deserialized->directories.size(), meta.directories.size());
  EXPECT_EQ(deserialized->names.size(), meta.names.size());
}

// Test 5: Format detection - Legacy Thrift (no magic bytes)
TEST_F(SerializerRegistryTest, DetectFormat_LegacyThrift) {
  auto meta = create_test_metadata();

  auto serializer = registry_->create_serializer(
      serialization::SerializationFormat::LEGACY_THRIFT);
  auto data = serializer->serialize(&meta);

  // Legacy Thrift has no magic bytes, should be detected as fallback
  auto detected = registry_->detect_format(data);
  ASSERT_TRUE(detected.has_value());

  // Should detect as LEGACY_THRIFT (fallback detection)
  EXPECT_EQ(detected.value(), serialization::SerializationFormat::LEGACY_THRIFT);
}

#ifdef DWARFS_HAVE_FLATBUFFERS
// Test 6: Format detection - FlatBuffers (with magic bytes)
TEST_F(SerializerRegistryTest, DetectFormat_FlatBuffers) {
  auto meta = create_test_metadata();

  auto serializer = registry_->create_serializer(
      serialization::SerializationFormat::FLATBUFFERS);
  auto data = serializer->serialize(&meta);

  // FlatBuffers has "DFBF" magic bytes
  auto detected = registry_->detect_format(data);
  ASSERT_TRUE(detected.has_value());
  EXPECT_EQ(detected.value(), serialization::SerializationFormat::FLATBUFFERS);
}

// Test 7: Cross-format conversion (Legacy → FlatBuffers)
TEST_F(SerializerRegistryTest, CrossFormat_LegacyToFlatBuffers) {
  auto meta = create_test_metadata();

  // Serialize with Legacy Thrift
  auto legacy_serializer = registry_->create_serializer(
      serialization::SerializationFormat::LEGACY_THRIFT);
  auto legacy_data = legacy_serializer->serialize(&meta);

  // Deserialize with Legacy Thrift
  auto legacy_meta_ptr = legacy_serializer->deserialize(legacy_data);
  auto* legacy_meta = static_cast<domain::metadata*>(legacy_meta_ptr.get());

  // Serialize with FlatBuffers
  auto fb_serializer = registry_->create_serializer(
      serialization::SerializationFormat::FLATBUFFERS);
  auto fb_data = fb_serializer->serialize(legacy_meta);

  // Deserialize with FlatBuffers
  auto fb_meta_ptr = fb_serializer->deserialize(fb_data);
  auto* fb_meta = static_cast<domain::metadata*>(fb_meta_ptr.get());

  // Verify content matches
  EXPECT_EQ(fb_meta->timestamp_base, meta.timestamp_base);
  EXPECT_EQ(fb_meta->block_size, meta.block_size);
  EXPECT_EQ(fb_meta->total_fs_size, meta.total_fs_size);
  EXPECT_EQ(fb_meta->chunks.size(), meta.chunks.size());
}

// Test 8: Cross-format conversion (FlatBuffers → Legacy)
TEST_F(SerializerRegistryTest, CrossFormat_FlatBuffersToLegacy) {
  auto meta = create_test_metadata();

  // Serialize with FlatBuffers
  auto fb_serializer = registry_->create_serializer(
      serialization::SerializationFormat::FLATBUFFERS);
  auto fb_data = fb_serializer->serialize(&meta);

  // Deserialize with FlatBuffers
  auto fb_meta_ptr = fb_serializer->deserialize(fb_data);
  auto* fb_meta = static_cast<domain::metadata*>(fb_meta_ptr.get());

  // Serialize with Legacy Thrift
  auto legacy_serializer = registry_->create_serializer(
      serialization::SerializationFormat::LEGACY_THRIFT);
  auto legacy_data = legacy_serializer->serialize(fb_meta);

  // Deserialize with Legacy Thrift
  auto legacy_meta_ptr = legacy_serializer->deserialize(legacy_data);
  auto* legacy_meta = static_cast<domain::metadata*>(legacy_meta_ptr.get());

  // Verify content matches
  EXPECT_EQ(legacy_meta->timestamp_base, meta.timestamp_base);
  EXPECT_EQ(legacy_meta->block_size, meta.block_size);
  EXPECT_EQ(legacy_meta->total_fs_size, meta.total_fs_size);
  EXPECT_EQ(legacy_meta->chunks.size(), meta.chunks.size());
}
#endif

// Test 9: U64 values preserved in Legacy Thrift
TEST_F(SerializerRegistryTest, LegacyThrift_U64_NoTruncation) {
  domain::metadata meta;
  meta.timestamp_base = UINT64_MAX; // Maximum u64
  meta.block_size = 262144;
  meta.total_fs_size = 0xFFFFFFFFFFFFFFFFULL; // Max u64

  auto serializer = registry_->create_serializer(
      serialization::SerializationFormat::LEGACY_THRIFT);

  auto data = serializer->serialize(&meta);
  auto deserialized_ptr = serializer->deserialize(data);
  auto* deserialized = static_cast<domain::metadata*>(deserialized_ptr.get());

  // NO truncation
  EXPECT_EQ(deserialized->timestamp_base, UINT64_MAX);
  EXPECT_EQ(deserialized->total_fs_size, UINT64_MAX);
}

// Test 10: Get available formats
TEST_F(SerializerRegistryTest, GetAvailableFormats) {
  auto formats = registry_->get_available_formats();

  EXPECT_FALSE(formats.empty());
  EXPECT_THAT(formats, ::testing::Contains(
      serialization::SerializationFormat::LEGACY_THRIFT));

#ifdef DWARFS_HAVE_FLATBUFFERS
  EXPECT_THAT(formats, ::testing::Contains(
      serialization::SerializationFormat::FLATBUFFERS));
#endif
}