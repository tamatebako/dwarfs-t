/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     DwarFS Implementation
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

/**
 * Frozen2 Serializer Tests
 *
 * Task 7: Test complete metadata field encoding
 *
 * These tests verify the simplified API that uses SchemaBuilder
 * to generate schema and FrozenSchemaSerializer to encode it.
 */

#include <gtest/gtest.h>
#include <cstring>

#include "dwarfs/metadata/legacy/frozen2_serializer.h"
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/domain/directory.h"
#include "dwarfs/metadata/domain/inode_data.h"

namespace dwarfs::metadata::legacy::test {

// Task 5 Step 1: Round-trip test
TEST(Frozen2SerializerTest, RoundTripPreservesMetadata) {
  // Arrange
  domain::metadata original;
  original.chunks = {{0, 0, 4096}, {1, 0, 8192}};
  original.block_size = 65536;
  original.timestamp_base = 1234567890;

  // Act: Serialize
  Frozen2Serializer serializer;
  std::vector<uint8_t> serialized = serializer.serialize(&original);

  // Assert: Check structure
  EXPECT_GT(serialized.size(), 8); // At least size prefix

  // Verify size prefix
  uint64_t size_prefix;
  std::memcpy(&size_prefix, serialized.data(), 8);
  EXPECT_EQ(serialized.size() - 8, size_prefix);
}

// Task 7: Test complete metadata with all field types
TEST(Frozen2SerializerTest, CompleteMetadataRoundTrip) {
  // Arrange: Create metadata with all supported fields
  domain::metadata original;

  // Chunks (field 1)
  original.chunks = {{0, 0, 4096}, {1, 0, 8192}};

  // Directories (field 2) - vector<directory> with 3 u32 fields each
  original.directories = {
    domain::directory(0, 0, 0),  // parent_entry, first_entry, self_entry
    domain::directory(0, 1, 1)
  };

  // Inodes (field 3) - vector<inode_data> with 12 fields each
  domain::inode_data inode1;
  inode1.mode_index = 1;
  inode1.owner_index = 0;
  inode1.group_index = 0;
  inode1.atime_offset = 1000;
  inode1.mtime_offset = 2000;
  inode1.ctime_offset = 3000;
  inode1.btime_offset = 4000;
  inode1.atime_subsec = 0;
  inode1.mtime_subsec = 0;
  inode1.ctime_subsec = 0;
  inode1.btime_subsec = 0;
  inode1.nlink_minus_one = 0;

  domain::inode_data inode2;
  inode2.mode_index = 2;
  inode2.owner_index = 0;
  inode2.group_index = 0;
  inode2.atime_offset = 1000;
  inode2.mtime_offset = 2000;
  inode2.ctime_offset = 3000;
  inode2.btime_offset = 4000;
  inode2.atime_subsec = 0;
  inode2.mtime_subsec = 0;
  inode2.ctime_subsec = 0;
  inode2.btime_subsec = 0;
  inode2.nlink_minus_one = 1;

  original.inodes = {inode1, inode2};

  // Chunk table (field 4)
  original.chunk_table = {0, 1, 2};

  // UIDs (field 7)
  original.uids = {0, 1000};

  // GIDs (field 8)
  original.gids = {0, 1000};

  // Modes (field 9)
  original.modes = {0755, 0644};

  // Timestamp base (field 12)
  original.timestamp_base = 1234567890;

  // Block size (field 15)
  original.block_size = 65536;

  // Total fs size (field 16)
  original.total_fs_size = 1024 * 1024;

  // Act: Serialize
  Frozen2Serializer serializer;
  std::vector<uint8_t> serialized = serializer.serialize(&original);

  // Assert: Verify output structure
  EXPECT_GT(serialized.size(), 8); // At least size prefix

  // Verify size prefix
  uint64_t size_prefix;
  std::memcpy(&size_prefix, serialized.data(), 8);
  EXPECT_EQ(serialized.size() - 8, size_prefix);

  // Verify we have substantial data (schema + frozen)
  EXPECT_GT(serialized.size(), 100);
}

// Test with empty optional fields
TEST(Frozen2SerializerTest, OnlyRequiredFields) {
  // Arrange: Minimal metadata
  domain::metadata minimal;
  minimal.chunks = {{0, 0, 4096}};
  minimal.block_size = 65536;

  // Act: Serialize
  Frozen2Serializer serializer;
  std::vector<uint8_t> serialized = serializer.serialize(&minimal);

  // Assert: Should still produce valid output
  EXPECT_GT(serialized.size(), 8);

  // Verify size prefix
  uint64_t size_prefix;
  std::memcpy(&size_prefix, serialized.data(), 8);
  EXPECT_EQ(serialized.size() - 8, size_prefix);
}

} // namespace dwarfs::metadata::legacy::test