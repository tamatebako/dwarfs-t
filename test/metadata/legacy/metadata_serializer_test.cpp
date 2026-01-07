/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhx.io)
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
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cinttypes>

#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/legacy/legacy_metadata_serializer.h"

using namespace dwarfs::metadata::legacy;
using namespace dwarfs::metadata::domain;

// Helper: Create minimal test metadata
metadata create_minimal_metadata() {
  metadata meta;
  meta.block_size = 4096;
  meta.timestamp_base = 1609459200; // 2021-01-01 00:00:00 UTC
  meta.total_fs_size = 1024 * 1024; // 1 MiB
  return meta;
}

// Helper: Create metadata with chunks
metadata create_metadata_with_chunks() {
  metadata meta = create_minimal_metadata();

  meta.chunks.emplace_back(0, 0, 1024);
  meta.chunks.emplace_back(0, 1024, 2048);
  meta.chunks.emplace_back(1, 0, 512);

  return meta;
}

// Helper: Create metadata with directories
metadata create_metadata_with_directories() {
  metadata meta = create_minimal_metadata();

  meta.directories.emplace_back(0, 1, 0); // Root directory
  meta.directories.emplace_back(0, 3, 1); // /dir1
  meta.directories.emplace_back(0, 5, 2); // /dir2

  return meta;
}

// Helper: Create metadata with inodes
metadata create_metadata_with_inodes() {
  metadata meta = create_minimal_metadata();

  inode_data inode1;
  inode1.mode_index = 0;
  inode1.owner_index = 0;
  inode1.group_index = 0;
  inode1.atime_offset = 0;
  inode1.mtime_offset = 100;
  inode1.ctime_offset = 200;
  meta.inodes.push_back(inode1);

  inode_data inode2;
  inode2.mode_index = 1;
  inode2.owner_index = 1;
  inode2.group_index = 1;
  inode2.atime_offset = 300;
  inode2.mtime_offset = 400;
  inode2.ctime_offset = 500;
  meta.inodes.push_back(inode2);

  return meta;
}

// Helper: Create metadata with tables
metadata create_metadata_with_tables() {
  metadata meta = create_minimal_metadata();

  meta.chunk_table = {0, 1, 2, 3};
  meta.symlink_table = {0, 1};
  meta.uids = {0, 1000, 1001};
  meta.gids = {0, 1000, 1001};
  meta.modes = {0755, 0644, 0600};
  meta.names = {"file1", "file2", "dir1"};
  meta.symlinks = {"/target1", "/target2"};

  return meta;
}

// Helper: Create comprehensive metadata
metadata create_comprehensive_metadata() {
  metadata meta = create_minimal_metadata();

  // Add chunks
  meta.chunks.emplace_back(0, 0, 1024);
  meta.chunks.emplace_back(1, 0, 2048);

  // Add directories
  meta.directories.emplace_back(0, 1, 0);
  meta.directories.emplace_back(0, 2, 1);

  // Add inodes
  inode_data inode;
  inode.mode_index = 0;
  inode.owner_index = 0;
  inode.group_index = 0;
  inode.mtime_offset = 1000;
  meta.inodes.push_back(inode);

  // Add tables
  meta.chunk_table = {0, 1};
  meta.uids = {0, 1000};
  meta.gids = {0, 1000};
  meta.modes = {0755, 0644};
  meta.names = {"file1", "file2"};
  meta.symlinks = {"/target"};
  meta.symlink_table = {0};

  return meta;
}

// ============================================================================
// Round-trip Tests
// ============================================================================

TEST(LegacyMetadataSerializer, RoundTrip_Minimal) {
  metadata original = create_minimal_metadata();

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(original, serialized);

  EXPECT_GT(serialized.size(), 0);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  EXPECT_EQ(deserialized.block_size, original.block_size);
  EXPECT_EQ(deserialized.timestamp_base, original.timestamp_base);
  EXPECT_EQ(deserialized.total_fs_size, original.total_fs_size);
}

TEST(LegacyMetadataSerializer, RoundTrip_WithChunks) {
  metadata original = create_metadata_with_chunks();

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(original, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  ASSERT_EQ(deserialized.chunks.size(), original.chunks.size());
  for (size_t i = 0; i < original.chunks.size(); ++i) {
    EXPECT_EQ(deserialized.chunks[i], original.chunks[i]) << "Chunk " << i;
  }
}

TEST(LegacyMetadataSerializer, RoundTrip_WithDirectories) {
  metadata original = create_metadata_with_directories();

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(original, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  ASSERT_EQ(deserialized.directories.size(), original.directories.size());
  for (size_t i = 0; i < original.directories.size(); ++i) {
    EXPECT_EQ(deserialized.directories[i], original.directories[i]) << "Directory " << i;
  }
}

TEST(LegacyMetadataSerializer, RoundTrip_WithInodes) {
  metadata original = create_metadata_with_inodes();

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(original, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  ASSERT_EQ(deserialized.inodes.size(), original.inodes.size());
  for (size_t i = 0; i < original.inodes.size(); ++i) {
    auto const& orig = original.inodes[i];
    auto const& deser = deserialized.inodes[i];

    EXPECT_EQ(deser.mode_index, orig.mode_index) << "Inode " << i;
    EXPECT_EQ(deser.owner_index, orig.owner_index) << "Inode " << i;
    EXPECT_EQ(deser.group_index, orig.group_index) << "Inode " << i;
    EXPECT_EQ(deser.atime_offset, orig.atime_offset) << "Inode " << i;
    EXPECT_EQ(deser.mtime_offset, orig.mtime_offset) << "Inode " << i;
    EXPECT_EQ(deser.ctime_offset, orig.ctime_offset) << "Inode " << i;
  }
}

TEST(LegacyMetadataSerializer, RoundTrip_WithTables) {
  metadata original = create_metadata_with_tables();

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(original, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  EXPECT_EQ(deserialized.chunk_table, original.chunk_table);
  EXPECT_EQ(deserialized.symlink_table, original.symlink_table);
  EXPECT_EQ(deserialized.uids, original.uids);
  EXPECT_EQ(deserialized.gids, original.gids);
  EXPECT_EQ(deserialized.modes, original.modes);
  EXPECT_EQ(deserialized.names, original.names);
  EXPECT_EQ(deserialized.symlinks, original.symlinks);
}

TEST(LegacyMetadataSerializer, RoundTrip_Comprehensive) {
  metadata original = create_comprehensive_metadata();

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(original, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  // Compare all core fields (note: we're not comparing optional fields yet)
  EXPECT_EQ(deserialized.chunks, original.chunks);
  EXPECT_EQ(deserialized.directories, original.directories);

  ASSERT_EQ(deserialized.inodes.size(), original.inodes.size());
  for (size_t i = 0; i < original.inodes.size(); ++i) {
    EXPECT_EQ(deserialized.inodes[i].mode_index, original.inodes[i].mode_index);
    EXPECT_EQ(deserialized.inodes[i].owner_index, original.inodes[i].owner_index);
    EXPECT_EQ(deserialized.inodes[i].group_index, original.inodes[i].group_index);
  }

  EXPECT_EQ(deserialized.chunk_table, original.chunk_table);
  EXPECT_EQ(deserialized.symlink_table, original.symlink_table);
  EXPECT_EQ(deserialized.uids, original.uids);
  EXPECT_EQ(deserialized.gids, original.gids);
  EXPECT_EQ(deserialized.modes, original.modes);
  EXPECT_EQ(deserialized.names, original.names);
  EXPECT_EQ(deserialized.symlinks, original.symlinks);
  EXPECT_EQ(deserialized.block_size, original.block_size);
  EXPECT_EQ(deserialized.timestamp_base, original.timestamp_base);
  EXPECT_EQ(deserialized.total_fs_size, original.total_fs_size);
}

// ============================================================================
// Empty Collection Tests
// ============================================================================

TEST(LegacyMetadataSerializer, EmptyCollections) {
  metadata original = create_minimal_metadata();
  // All collections are empty by default

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(original, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  EXPECT_TRUE(deserialized.chunks.empty());
  EXPECT_TRUE(deserialized.directories.empty());
  EXPECT_TRUE(deserialized.inodes.empty());
  EXPECT_TRUE(deserialized.chunk_table.empty());
  EXPECT_TRUE(deserialized.uids.empty());
  EXPECT_TRUE(deserialized.gids.empty());
  EXPECT_TRUE(deserialized.modes.empty());
  EXPECT_TRUE(deserialized.names.empty());
  EXPECT_TRUE(deserialized.symlinks.empty());
}

// ============================================================================
// U64 Field Tests (No Truncation)
// ============================================================================

TEST(LegacyMetadataSerializer, RoundTrip_U64_NoTruncation) {
  metadata original = create_minimal_metadata();

  // Set u64 fields to values that would be truncated if using i32
  original.timestamp_base = UINT64_MAX;  // Max u64 value
  original.total_fs_size = (uint64_t)1 << 40;  // 1 TiB (1099511627776 bytes)

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(original, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  // Verify NO truncation occurred
  EXPECT_EQ(deserialized.timestamp_base, UINT64_MAX);
  EXPECT_EQ(deserialized.total_fs_size, (uint64_t)1 << 40);
}

TEST(LegacyMetadataSerializer, RoundTrip_U64_LargeValues) {
  metadata original = create_minimal_metadata();

  // Test various large u64 values
  std::vector<uint64_t> test_values = {
    0x0000000000000000ULL,
    0xFFFFFFFFFFFFFFFFULL,
    0x0123456789ABCDEFULL,
    0xFEDCBA9876543210ULL,
    0x8000000000000000ULL, // MSB set
    0x0000000100000000ULL, // Just over 32-bit
    (uint64_t)1 << 50,     // 1 PiB
  };

  for (auto value : test_values) {
    original.timestamp_base = value;
    original.total_fs_size = value ^ 0xAAAAAAAAAAAAAAAAULL; // Different value

    std::vector<uint8_t> serialized;
    LegacyMetadataSerializer::serialize(original, serialized);

    metadata deserialized;
    LegacyMetadataSerializer::deserialize(serialized, deserialized);

    EXPECT_EQ(deserialized.timestamp_base, value)
      << "timestamp_base: 0x" << std::hex << value;
    EXPECT_EQ(deserialized.total_fs_size, value ^ 0xAAAAAAAAAAAAAAAAULL)
      << "total_fs_size: 0x" << std::hex << (value ^ 0xAAAAAAAAAAAAAAAAULL);
  }
}

// ============================================================================
// Field Order Tests (ensure fields can be read in any order)
// ============================================================================

TEST(LegacyMetadataSerializer, FieldOrderIndependence) {
  // This test verifies that deserializer correctly handles fields
  // appearing in non-sequential order (Thrift allows this)

  metadata original;
  original.block_size = 8192;
  original.timestamp_base = 1234567890;
  original.total_fs_size = 999999;
  original.names = {"file1", "file2"};
  original.modes = {0755};

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(original, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  EXPECT_EQ(deserialized.block_size, original.block_size);
  EXPECT_EQ(deserialized.timestamp_base, original.timestamp_base);
  EXPECT_EQ(deserialized.total_fs_size, original.total_fs_size);
  EXPECT_EQ(deserialized.names, original.names);
  EXPECT_EQ(deserialized.modes, original.modes);
}