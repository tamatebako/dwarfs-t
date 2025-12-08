/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */

#include <gtest/gtest.h>

#include "dwarfs/metadata/serialization/serializer_registry.h"
#include "dwarfs/metadata/serialization/serialization_format.h"
#include "dwarfs/metadata/serialization/init_serializers.h"
#include "dwarfs/metadata/domain/metadata.h"

#ifdef DWARFS_HAVE_THRIFT
#include "dwarfs/metadata/serialization/thrift_compact_serializer.h"
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
#include "dwarfs/metadata/serialization/flatbuffers_serializer.h"
#endif

using namespace dwarfs::metadata::serialization;
using namespace dwarfs::metadata::domain;

namespace {

// Helper to create test metadata
metadata create_test_metadata() {
  metadata meta;

  // Add some test data
  meta.block_size = 4096;
  meta.total_fs_size = 1024 * 1024;
  meta.timestamp_base = 1234567890;

  // Add chunks
  chunk c1{0, 0, 100};
  chunk c2{0, 100, 200};
  meta.chunks.push_back(c1);
  meta.chunks.push_back(c2);

  // Add directories
  directory d1{0, 0, 0};
  directory d2{1, 0, 1};
  meta.directories.push_back(d1);
  meta.directories.push_back(d2);

  // Add inodes
  inode_data i1;
  i1.mode_index = 0;
  i1.owner_index = 0;
  i1.group_index = 0;

  inode_data i2;
  i2.mode_index = 1;
  i2.owner_index = 1;
  i2.group_index = 1;
  i2.atime_offset = 100;
  i2.mtime_offset = 200;
  i2.ctime_offset = 300;

  meta.inodes.push_back(i1);
  meta.inodes.push_back(i2);

  // Add names
  meta.names.push_back("root");
  meta.names.push_back("file.txt");
  meta.names.push_back("dir");

  // Add symlinks
  meta.symlinks.push_back("/usr/bin/test");

  // Add dir entries
  dir_entry e1{0, 0};
  dir_entry e2{1, 1};
  meta.dir_entries = std::vector<dir_entry>();
  meta.dir_entries->push_back(e1);
  meta.dir_entries->push_back(e2);

  // Add tables
  meta.chunk_table.push_back(0);
  meta.chunk_table.push_back(1);
  meta.symlink_table.push_back(0);

  meta.uids.push_back(1000);
  meta.gids.push_back(1000);
  meta.modes.push_back(0755);
  meta.modes.push_back(0644);

  return meta;
}

// Helper to verify metadata equality
void verify_metadata_equal(const metadata& expected, const metadata& actual) {
  EXPECT_EQ(expected.block_size, actual.block_size);
  EXPECT_EQ(expected.total_fs_size, actual.total_fs_size);
  EXPECT_EQ(expected.timestamp_base, actual.timestamp_base);

  EXPECT_EQ(expected.chunks.size(), actual.chunks.size());
  for (size_t i = 0; i < expected.chunks.size(); ++i) {
    EXPECT_EQ(expected.chunks[i].block(), actual.chunks[i].block());
    EXPECT_EQ(expected.chunks[i].offset(), actual.chunks[i].offset());
    EXPECT_EQ(expected.chunks[i].size(), actual.chunks[i].size());
  }

  EXPECT_EQ(expected.directories.size(), actual.directories.size());
  EXPECT_EQ(expected.inodes.size(), actual.inodes.size());
  EXPECT_EQ(expected.names.size(), actual.names.size());
  EXPECT_EQ(expected.symlinks.size(), actual.symlinks.size());
  EXPECT_EQ(expected.dir_entries.has_value(), actual.dir_entries.has_value());
  if (expected.dir_entries && actual.dir_entries) {
    EXPECT_EQ(expected.dir_entries->size(), actual.dir_entries->size());
  }
  EXPECT_EQ(expected.chunk_table.size(), actual.chunk_table.size());
  EXPECT_EQ(expected.symlink_table.size(), actual.symlink_table.size());
  EXPECT_EQ(expected.uids.size(), actual.uids.size());
  EXPECT_EQ(expected.gids.size(), actual.gids.size());
  EXPECT_EQ(expected.modes.size(), actual.modes.size());
}

} // anonymous namespace

// Test registry singleton
TEST(SerializationTest, RegistrySingleton) {
  auto& registry1 = SerializerRegistry::instance();
  auto& registry2 = SerializerRegistry::instance();
  EXPECT_EQ(&registry1, &registry2);
}

// Test format availability
TEST(SerializationTest, FormatAvailability) {
  // Initialize serializers first
  init_serializers();

  auto& registry = SerializerRegistry::instance();
  auto formats = registry.get_available_formats();

  // Should have at least one format
  EXPECT_FALSE(formats.empty());

#ifdef DWARFS_HAVE_THRIFT
  EXPECT_TRUE(registry.is_format_available(SerializationFormat::THRIFT_COMPACT));
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
  EXPECT_TRUE(registry.is_format_available(SerializationFormat::FLATBUFFERS));
#endif
}

#ifdef DWARFS_HAVE_THRIFT
TEST(SerializationTest, ThriftCapabilities) {
  ThriftCompactSerializer serializer;

  EXPECT_TRUE(serializer.can_read());
  EXPECT_FALSE(serializer.can_write());  // Read-only
  EXPECT_EQ(serializer.get_format(), SerializationFormat::THRIFT_COMPACT);
  EXPECT_EQ(serializer.get_format_name(), "Thrift Compact");
}

TEST(SerializationTest, ThriftWriteThrows) {
  ThriftCompactSerializer serializer;
  auto meta = create_test_metadata();

  // Should throw because Thrift is read-only
  EXPECT_THROW(serializer.serialize(&meta), std::runtime_error);
}
#endif // DWARFS_HAVE_THRIFT

#ifdef DWARFS_HAVE_FLATBUFFERS
// FlatBuffers tests (modern default format)

TEST(SerializationTest, FlatBuffersCapabilities) {
  FlatBuffersSerializer serializer;

  EXPECT_TRUE(serializer.can_read());
  EXPECT_TRUE(serializer.can_write());  // Read-write
  EXPECT_EQ(serializer.get_format(), SerializationFormat::FLATBUFFERS);
  EXPECT_EQ(serializer.get_format_name(), "FlatBuffers");
}

TEST(SerializationTest, FlatBuffersRoundTrip) {
  FlatBuffersSerializer serializer;
  auto original_meta = create_test_metadata();

  // Serialize
  auto bytes = serializer.serialize(&original_meta);
  EXPECT_FALSE(bytes.empty());
  EXPECT_GT(bytes.size(), 100u); // Should have substantial size

  // Deserialize
  auto deserialized_ptr = serializer.deserialize(bytes);
  ASSERT_NE(deserialized_ptr.get(), nullptr);

  auto* deserialized_meta = static_cast<metadata*>(deserialized_ptr.get());

  // Verify equality
  verify_metadata_equal(original_meta, *deserialized_meta);
}

TEST(SerializationTest, FlatBuffersSerializeNullThrows) {
  FlatBuffersSerializer serializer;
  EXPECT_THROW(serializer.serialize(nullptr), std::invalid_argument);
}

TEST(SerializationTest, FlatBuffersDeserializeInvalidThrows) {
  FlatBuffersSerializer serializer;
  std::vector<uint8_t> invalid = {0x00, 0x00, 0x00, 0x00};
  // Throws invalid_argument for data too short, runtime_error for invalid format
  EXPECT_THROW(serializer.deserialize(invalid), std::invalid_argument);
}

TEST(SerializationTest, FlatBuffersFormatDetection) {
  init_serializers();

  FlatBuffersSerializer serializer;
  auto meta = create_test_metadata();
  auto bytes = serializer.serialize(&meta);

  // Check magic bytes exist
  ASSERT_GE(bytes.size(), 8u);

  auto& registry = SerializerRegistry::instance();

  // Format detection needs enough bytes
  std::vector<uint8_t> sample(bytes.begin(), bytes.begin() + std::min(bytes.size(), size_t(16)));
  auto detected = registry.detect_format(sample);

  ASSERT_TRUE(detected.has_value());
  EXPECT_EQ(*detected, SerializationFormat::FLATBUFFERS);
}

#endif // DWARFS_HAVE_FLATBUFFERS

// Test error cases
TEST(SerializationTest, DetectFormatEmptyData) {
  auto& registry = SerializerRegistry::instance();
  std::vector<uint8_t> empty;
  auto detected = registry.detect_format(empty);
  EXPECT_FALSE(detected.has_value());
}

TEST(SerializationTest, DetectFormatInvalidMagic) {
  auto& registry = SerializerRegistry::instance();
  std::vector<uint8_t> invalid = {0xFF, 0xFF, 0xFF};
  auto detected = registry.detect_format(invalid);
  EXPECT_FALSE(detected.has_value());
}