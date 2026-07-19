/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file converter_test.cpp
 * \brief Unit tests for Modern Thrift converters
 *
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>

#ifdef DWARFS_HAVE_MODERN_THRIFT

#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/modern/domain_to_thrift.h"
#include "dwarfs/metadata/modern/thrift_to_domain.h"

using namespace dwarfs::metadata;

namespace {

// Helper to create a simple metadata instance
domain::metadata create_simple_metadata() {
  domain::metadata dm;

  // Basic parameters
  dm.block_size = 131072;
  dm.timestamp_base = 1234567890;
  dm.total_fs_size = 1000000;

  // Add a simple chunk
  domain::chunk c(0, 0, 1024);
  dm.chunks.push_back(c);

  // Add a simple directory
  domain::directory dir(0, 0, 0);
  dm.directories.push_back(dir);

  // Add a simple inode
  domain::inode_data inode;
  inode.mode_index = 0;
  inode.owner_index = 0;
  inode.group_index = 0;
  inode.atime_offset = 100;
  inode.mtime_offset = 200;
  inode.ctime_offset = 300;
  dm.inodes.push_back(inode);

  // Add lookup tables
  dm.uids.push_back(1000);
  dm.gids.push_back(1000);
  dm.modes.push_back(0755);
  dm.names.push_back("file.txt");

  // Add chunk table
  dm.chunk_table.push_back(0);
  dm.chunk_table.push_back(1);

  return dm;
}

// Helper to create complex metadata with all optional fields
domain::metadata create_complex_metadata() {
  auto dm = create_simple_metadata();

  // Add optional features
  dm.devices = std::vector<uint64_t>{1, 2, 3};
  dm.dwarfs_version = "0.17.0-test";
  dm.create_timestamp = static_cast<uint64_t>(time(nullptr));

  // Add fs_options
  domain::fs_options opts;
  opts.mtime_only = false;
  opts.packed_chunk_table = true;
  opts.packed_directories = true;
  opts.packed_shared_files_table = false;
  opts.has_btime = true;
  opts.inodes_have_nlink = true;
  opts.time_resolution_sec = 1;
  opts.subsecond_resolution_nsec_multiplier = 1000000;
  dm.options = opts;

  // Add directory entries
  std::vector<domain::dir_entry> entries;
  entries.push_back(domain::dir_entry(0, 0));
  dm.dir_entries = std::move(entries);

  // Add string tables
  domain::string_table names_table;
  names_table.buffer = "file.txt";
  names_table.index = {0, 8};
  names_table.packed_index = false;
  dm.compact_names = names_table;

  // Add features
  dm.features = std::set<std::string>{"feature1", "feature2"};

  // Add categories
  dm.category_names = std::vector<std::string>{"category1", "category2"};
  dm.block_categories = std::vector<uint32_t>{0, 1};

  // Add inode size cache
  domain::inode_size_cache cache;
  cache.size_lookup[0] = 1024;
  cache.min_chunk_count = 10;
  cache.allocated_size_lookup[0] = 2048;
  dm.reg_file_size_cache = cache;

  // Add category metadata
  dm.category_metadata_json = std::vector<std::string>{R"({"key":"value"})"};
  dm.block_category_metadata = std::map<uint32_t, uint32_t>{{0, 0}};

  // Add version history
  domain::history_entry history;
  history.major = 2;
  history.minor = 5;
  history.dwarfs_version = "0.17.0";
  history.block_size = 131072;
  history.options = opts;
  dm.metadata_version_history = std::vector<domain::history_entry>{history};

  // Add sparse file support
  dm.hole_block_index = 999;
  dm.large_hole_size = std::vector<uint64_t>{4096, 8192};
  dm.total_allocated_fs_size = 900000;

  // Set shared files table
  dm.shared_files_table = std::vector<uint32_t>{0, 1, 2};
  dm.total_hardlink_size = 5000;

  // Add symlinks
  dm.symlinks.push_back("/path/to/target");
  dm.symlink_table.push_back(0);
  dm.symlink_table.push_back(1);

  // Add preferred path separator
  dm.preferred_path_separator = '/';

  return dm;
}

} // anonymous namespace

TEST(ModernThriftConverter, SimpleMetadataRoundTrip) {
  // Create simple domain metadata
  auto original = create_simple_metadata();

  // Convert domain → thrift
  auto thrift_meta = modern::domain_to_thrift(original);

  // Convert thrift → domain
  auto restored = modern::thrift_to_domain(thrift_meta);

  // Verify round-trip preserves data
  EXPECT_EQ(original.block_size, restored.block_size);
  EXPECT_EQ(original.timestamp_base, restored.timestamp_base);
  EXPECT_EQ(original.total_fs_size, restored.total_fs_size);

  // Verify chunks
  ASSERT_EQ(original.chunks.size(), restored.chunks.size());
  EXPECT_EQ(original.chunks[0], restored.chunks[0]);

  // Verify directories
  ASSERT_EQ(original.directories.size(), restored.directories.size());
  EXPECT_EQ(original.directories[0], restored.directories[0]);

  // Verify inodes
  ASSERT_EQ(original.inodes.size(), restored.inodes.size());
  EXPECT_EQ(original.inodes[0], restored.inodes[0]);

  // Verify lookup tables
  EXPECT_EQ(original.uids, restored.uids);
  EXPECT_EQ(original.gids, restored.gids);
  EXPECT_EQ(original.modes, restored.modes);
  EXPECT_EQ(original.names, restored.names);

  // Verify chunk table
  EXPECT_EQ(original.chunk_table, restored.chunk_table);
}

TEST(ModernThriftConverter, ComplexMetadataWithOptionals) {
  // Create complex domain metadata
  auto original = create_complex_metadata();

  // Convert domain → thrift
  auto thrift_meta = modern::domain_to_thrift(original);

  // Convert thrift → domain
  auto restored = modern::thrift_to_domain(thrift_meta);

  // Verify optional fields are preserved
  ASSERT_TRUE(restored.devices.has_value());
  EXPECT_EQ(*original.devices, *restored.devices);

  ASSERT_TRUE(restored.dwarfs_version.has_value());
  EXPECT_EQ(*original.dwarfs_version, *restored.dwarfs_version);

  ASSERT_TRUE(restored.create_timestamp.has_value());
  EXPECT_EQ(*original.create_timestamp, *restored.create_timestamp);

  // Verify fs_options
  ASSERT_TRUE(restored.options.has_value());
  EXPECT_EQ(*original.options, *restored.options);

  // Verify directory entries
  ASSERT_TRUE(restored.dir_entries.has_value());
  ASSERT_EQ(original.dir_entries->size(), restored.dir_entries->size());
  EXPECT_EQ((*original.dir_entries)[0], (*restored.dir_entries)[0]);

  // Verify string tables
  ASSERT_TRUE(restored.compact_names.has_value());
  EXPECT_EQ(*original.compact_names, *restored.compact_names);

  // Verify features
  ASSERT_TRUE(restored.features.has_value());
  EXPECT_EQ(*original.features, *restored.features);

  // Verify categories
  ASSERT_TRUE(restored.category_names.has_value());
  EXPECT_EQ(*original.category_names, *restored.category_names);

  ASSERT_TRUE(restored.block_categories.has_value());
  EXPECT_EQ(*original.block_categories, *restored.block_categories);

  // Verify inode size cache
  ASSERT_TRUE(restored.reg_file_size_cache.has_value());
  EXPECT_EQ(*original.reg_file_size_cache, *restored.reg_file_size_cache);

  // Verify category metadata
  ASSERT_TRUE(restored.category_metadata_json.has_value());
  EXPECT_EQ(*original.category_metadata_json, *restored.category_metadata_json);

  ASSERT_TRUE(restored.block_category_metadata.has_value());
  EXPECT_EQ(*original.block_category_metadata, *restored.block_category_metadata);

  // Verify version history
  ASSERT_TRUE(restored.metadata_version_history.has_value());
  ASSERT_EQ(original.metadata_version_history->size(), restored.metadata_version_history->size());
  EXPECT_EQ((*original.metadata_version_history)[0], (*restored.metadata_version_history)[0]);

  // Verify sparse file support
  ASSERT_TRUE(restored.hole_block_index.has_value());
  EXPECT_EQ(*original.hole_block_index, *restored.hole_block_index);

  ASSERT_TRUE(restored.large_hole_size.has_value());
  EXPECT_EQ(*original.large_hole_size, *restored.large_hole_size);

  ASSERT_TRUE(restored.total_allocated_fs_size.has_value());
  EXPECT_EQ(*original.total_allocated_fs_size, *restored.total_allocated_fs_size);
}

TEST(ModernThriftConverter, EmptyMetadata) {
  // Create empty metadata
  domain::metadata original;

  // Convert domain → thrift
  auto thrift_meta = modern::domain_to_thrift(original);

  // Convert thrift → domain
  auto restored = modern::thrift_to_domain(thrift_meta);

  // Verify empty state is preserved
  EXPECT_EQ(original.block_size, restored.block_size);
  EXPECT_EQ(original.timestamp_base, restored.timestamp_base);
  EXPECT_TRUE(restored.chunks.empty());
  EXPECT_TRUE(restored.directories.empty());
  EXPECT_TRUE(restored.inodes.empty());
}

TEST(ModernThriftConverter, OptionalFieldsNotSet) {
  // Create metadata with no optional fields
  auto original = create_simple_metadata();

  // Ensure no optional fields are set
  ASSERT_FALSE(original.devices.has_value());
  ASSERT_FALSE(original.options.has_value());
  ASSERT_FALSE(original.dir_entries.has_value());

  // Convert domain → thrift
  auto thrift_meta = modern::domain_to_thrift(original);

  // Convert thrift → domain
  auto restored = modern::thrift_to_domain(thrift_meta);

  // Verify optional fields remain unset
  EXPECT_FALSE(restored.devices.has_value());
  EXPECT_FALSE(restored.options.has_value());
  EXPECT_FALSE(restored.dir_entries.has_value());
  EXPECT_FALSE(restored.compact_names.has_value());
  EXPECT_FALSE(restored.features.has_value());
}

TEST(ModernThriftConverter, FullMetadataEquality) {
  // Create complex metadata
  auto original = create_complex_metadata();

  // Convert domain → thrift → domain
  auto thrift_meta = modern::domain_to_thrift(original);
  auto restored = modern::thrift_to_domain(thrift_meta);

  // Verify complete equality
  EXPECT_EQ(original, restored);
}

#else

// Modern Thrift not available - skip these tests
TEST(ModernThriftConverter, ModernThriftNotAvailable) {
  GTEST_SKIP() << "Modern Thrift support not enabled - skipping converter tests";
}

#endif // DWARFS_HAVE_MODERN_THRIFT
