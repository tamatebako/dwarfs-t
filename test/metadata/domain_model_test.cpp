/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Domain model unit tests
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 *
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>

#include <sstream>
#include <cereal/archives/binary.hpp>

#include "dwarfs/metadata/domain/chunk.h"
#include "dwarfs/metadata/domain/directory.h"
#include "dwarfs/metadata/domain/dir_entry.h"
#include "dwarfs/metadata/domain/inode_data.h"
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/domain/fs_options.h"
#include "dwarfs/metadata/domain/string_table.h"

using namespace dwarfs::metadata::domain;

/**
 * Test fixture for domain model tests
 */
class DomainModelTest : public ::testing::Test {
protected:
  /**
   * Helper to test round-trip serialization for any domain object
   */
  template <typename T>
  void test_roundtrip(const T& obj) {
    std::stringstream ss;

    // Serialize
    {
      cereal::BinaryOutputArchive archive(ss);
      archive(obj);
    }

    // Deserialize
    T obj2;
    {
      cereal::BinaryInputArchive archive(ss);
      archive(obj2);
    }

    // Compare (implementation-specific checks in individual tests)
  }
};

/**
 * Test chunk serialization
 */
TEST_F(DomainModelTest, ChunkBasicSerialization) {
  chunk c;
  c.block = 42;
  c.offset = 1024;
  c.size = 4096;

  std::stringstream ss;

  // Serialize
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(c);
  }

  // Deserialize
  chunk c2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(c2);
  }

  EXPECT_EQ(c.block, c2.block);
  EXPECT_EQ(c.offset, c2.offset);
  EXPECT_EQ(c.size, c2.size);
}

/**
 * Test chunk with edge values
 */
TEST_F(DomainModelTest, ChunkEdgeValues) {
  chunk c;
  c.block = 0;
  c.offset = 0;
  c.size = 0;

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(c);
  }

  chunk c2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(c2);
  }

  EXPECT_EQ(c.block, c2.block);
  EXPECT_EQ(c.offset, c2.offset);
  EXPECT_EQ(c.size, c2.size);
}

/**
 * Test chunk with maximum values
 */
TEST_F(DomainModelTest, ChunkMaxValues) {
  chunk c;
  c.block = UINT32_MAX;
  c.offset = UINT32_MAX;
  c.size = UINT32_MAX;

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(c);
  }

  chunk c2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(c2);
  }

  EXPECT_EQ(c.block, c2.block);
  EXPECT_EQ(c.offset, c2.offset);
  EXPECT_EQ(c.size, c2.size);
}

/**
 * Test directory entry serialization
 */
TEST_F(DomainModelTest, DirEntrySerialization) {
  dir_entry entry;
  entry.inode_num = 123;
  entry.name_index = 456;

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(entry);
  }

  dir_entry entry2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(entry2);
  }

  EXPECT_EQ(entry.inode_num, entry2.inode_num);
  EXPECT_EQ(entry.name_index, entry2.name_index);
}

/**
 * Test directory serialization
 */
TEST_F(DomainModelTest, DirectorySerialization) {
  directory dir;
  dir.first_entry = 10;
  dir.parent_inode = 5;

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(dir);
  }

  directory dir2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(dir2);
  }

  EXPECT_EQ(dir.first_entry, dir2.first_entry);
  EXPECT_EQ(dir.parent_inode, dir2.parent_inode);
}

/**
 * Test inode_data serialization
 */
TEST_F(DomainModelTest, InodeDataSerialization) {
  inode_data inode;
  inode.mode_index = 1;
  inode.owner_index = 2;
  inode.group_index = 3;
  inode.atime_offset = 1000;
  inode.mtime_offset = 2000;
  inode.ctime_offset = 3000;

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(inode);
  }

  inode_data inode2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(inode2);
  }

  EXPECT_EQ(inode.mode_index, inode2.mode_index);
  EXPECT_EQ(inode.owner_index, inode2.owner_index);
  EXPECT_EQ(inode.group_index, inode2.group_index);
  EXPECT_EQ(inode.atime_offset, inode2.atime_offset);
  EXPECT_EQ(inode.mtime_offset, inode2.mtime_offset);
  EXPECT_EQ(inode.ctime_offset, inode2.ctime_offset);
}

/**
 * Test metadata basic fields serialization
 */
TEST_F(DomainModelTest, MetadataBasicFields) {
  metadata meta;
  meta.block_size = 131072;
  meta.total_fs_size = 1024 * 1024 * 1024;
  meta.timestamp_base = 1609459200; // 2021-01-01

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(meta);
  }

  metadata meta2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(meta2);
  }

  EXPECT_EQ(meta.block_size, meta2.block_size);
  EXPECT_EQ(meta.total_fs_size, meta2.total_fs_size);
  EXPECT_EQ(meta.timestamp_base, meta2.timestamp_base);
}

/**
 * Test metadata with chunks
 */
TEST_F(DomainModelTest, MetadataWithChunks) {
  metadata meta;
  meta.block_size = 131072;

  // Add some chunks
  chunk c1{1, 0, 4096};
  chunk c2{1, 4096, 8192};
  chunk c3{2, 0, 1024};

  meta.chunks.push_back(c1);
  meta.chunks.push_back(c2);
  meta.chunks.push_back(c3);

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(meta);
  }

  metadata meta2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(meta2);
  }

  ASSERT_EQ(meta.chunks.size(), meta2.chunks.size());
  EXPECT_EQ(meta.chunks[0].block, meta2.chunks[0].block);
  EXPECT_EQ(meta.chunks[1].offset, meta2.chunks[1].offset);
  EXPECT_EQ(meta.chunks[2].size, meta2.chunks[2].size);
}

/**
 * Test metadata with optional fields
 */
TEST_F(DomainModelTest, MetadataOptionalFields) {
  metadata meta;
  meta.block_size = 131072;

  // Set optional fields
  meta.dwarfs_version = "0.7.0";
  meta.create_timestamp = 1609459200;
  meta.preferred_path_separator = '/';

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(meta);
  }

  metadata meta2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(meta2);
  }

  ASSERT_TRUE(meta2.dwarfs_version.has_value());
  EXPECT_EQ(*meta2.dwarfs_version, "0.7.0");

  ASSERT_TRUE(meta2.create_timestamp.has_value());
  EXPECT_EQ(*meta2.create_timestamp, 1609459200u);

  ASSERT_TRUE(meta2.preferred_path_separator.has_value());
  EXPECT_EQ(*meta2.preferred_path_separator, '/');
}

/**
 * Test metadata with features
 */
TEST_F(DomainModelTest, MetadataWithFeatures) {
  metadata meta;
  meta.block_size = 131072;

  std::set<std::string> features{"symlinks", "devices", "packed_directories"};
  meta.features = features;

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(meta);
  }

  metadata meta2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(meta2);
  }

  ASSERT_TRUE(meta2.features.has_value());
  EXPECT_EQ(meta2.features->size(), 3u);
  EXPECT_TRUE(meta2.features->count("symlinks"));
  EXPECT_TRUE(meta2.features->count("devices"));
  EXPECT_TRUE(meta2.features->count("packed_directories"));
}

/**
 * Test metadata with names and symlinks
 */
TEST_F(DomainModelTest, MetadataWithNamesAndSymlinks) {
  metadata meta;
  meta.block_size = 131072;

  meta.names.push_back("file1.txt");
  meta.names.push_back("file2.txt");
  meta.names.push_back("directory");

  meta.symlinks.push_back("/usr/bin/python");
  meta.symlinks.push_back("../lib/library.so");

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(meta);
  }

  metadata meta2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(meta2);
  }

  ASSERT_EQ(meta2.names.size(), 3u);
  EXPECT_EQ(meta2.names[0], "file1.txt");
  EXPECT_EQ(meta2.names[1], "file2.txt");
  EXPECT_EQ(meta2.names[2], "directory");

  ASSERT_EQ(meta2.symlinks.size(), 2u);
  EXPECT_EQ(meta2.symlinks[0], "/usr/bin/python");
  EXPECT_EQ(meta2.symlinks[1], "../lib/library.so");
}

/**
 * Test metadata version handling
 */
TEST_F(DomainModelTest, MetadataVersioning) {
  metadata meta;
  meta.block_size = 131072;
  meta.total_fs_size = 1024 * 1024;

  // Version 3+ fields
  meta.dwarfs_version = "0.5.0";
  meta.create_timestamp = 1609459200;

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(meta);
  }

  metadata meta2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(meta2);
  }

  EXPECT_TRUE(meta2.dwarfs_version.has_value());
  EXPECT_TRUE(meta2.create_timestamp.has_value());
}

/**
 * Test empty metadata serialization
 */
TEST_F(DomainModelTest, EmptyMetadata) {
  metadata meta;

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(meta);
  }

  metadata meta2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(meta2);
  }

  EXPECT_EQ(meta2.chunks.size(), 0u);
  EXPECT_EQ(meta2.directories.size(), 0u);
  EXPECT_EQ(meta2.inodes.size(), 0u);
  EXPECT_EQ(meta2.block_size, 0u);
  EXPECT_EQ(meta2.total_fs_size, 0u);
}

/**
 * Test large metadata object
 */
TEST_F(DomainModelTest, LargeMetadata) {
  metadata meta;
  meta.block_size = 131072;

  // Add many chunks
  for (uint32_t i = 0; i < 1000; ++i) {
    chunk c{i / 100, (i % 100) * 1024, 1024};
    meta.chunks.push_back(c);
  }

  // Add many names
  for (int i = 0; i < 100; ++i) {
    meta.names.push_back("file_" + std::to_string(i) + ".txt");
  }

  std::stringstream ss;
  {
    cereal::BinaryOutputArchive archive(ss);
    archive(meta);
  }

  metadata meta2;
  {
    cereal::BinaryInputArchive archive(ss);
    archive(meta2);
  }

  EXPECT_EQ(meta2.chunks.size(), 1000u);
  EXPECT_EQ(meta2.names.size(), 100u);
  EXPECT_EQ(meta2.names[50], "file_50.txt");
}