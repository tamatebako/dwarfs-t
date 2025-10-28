/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Test fixtures and helper utilities
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/domain/chunk.h"
#include "dwarfs/metadata/domain/directory.h"
#include "dwarfs/metadata/domain/dir_entry.h"
#include "dwarfs/metadata/domain/inode_data.h"

namespace dwarfs::metadata::test {

/**
 * Create a minimal metadata object for testing
 */
inline domain::metadata create_minimal_metadata() {
  domain::metadata meta;
  meta.block_size = 131072;
  meta.total_fs_size = 1024 * 1024;
  meta.timestamp_base = 1609459200;
  return meta;
}

/**
 * Create a small realistic metadata object
 */
inline domain::metadata create_small_metadata() {
  auto meta = create_minimal_metadata();

  // Add 10 chunks
  for (uint32_t i = 0; i < 10; ++i) {
    meta.chunks.push_back(domain::chunk{i / 5, (i % 5) * 1024, 1024});
  }

  // Add 5 directories
  for (uint32_t i = 0; i < 5; ++i) {
    meta.directories.push_back(domain::directory{i * 2, i > 0 ? i - 1 : 0});
  }

  // Add 10 inodes
  for (uint32_t i = 0; i < 10; ++i) {
    domain::inode_data inode;
    inode.mode_index = i % 3;
    inode.owner_index = i % 2;
    inode.group_index = i % 2;
    meta.inodes.push_back(inode);
  }

  // Add names
  meta.names = {"file1.txt", "file2.txt", "dir1", "dir2", "link.txt"};

  // Add symlinks
  meta.symlinks = {"/usr/bin/python3"};

  return meta;
}

/**
 * Create a medium-sized metadata object
 */
inline domain::metadata create_medium_metadata() {
  auto meta = create_minimal_metadata();

  // Add 1000 chunks
  for (uint32_t i = 0; i < 1000; ++i) {
    meta.chunks.push_back(domain::chunk{i / 100, (i % 100) * 128, 128});
  }

  // Add 100 directories
  for (uint32_t i = 0; i < 100; ++i) {
    meta.directories.push_back(domain::directory{i * 10, i > 0 ? i - 1 : 0});
  }

  // Add 500 inodes
  for (uint32_t i = 0; i < 500; ++i) {
    domain::inode_data inode;
    inode.mode_index = i % 10;
    inode.owner_index = i % 5;
    inode.group_index = i % 5;
    meta.inodes.push_back(inode);
  }

  // Add names
  for (int i = 0; i < 100; ++i) {
    meta.names.push_back("file_" + std::to_string(i) + ".txt");
  }

  // Add symlinks
  for (int i = 0; i < 10; ++i) {
    meta.symlinks.push_back("/path/to/target_" + std::to_string(i));
  }

  // Add optional fields
  meta.dwarfs_version = "0.7.0";
  meta.create_timestamp = 1609459200;
  meta.features = std::set<std::string>{"symlinks", "devices"};

  return meta;
}

/**
 * Create a large metadata object for stress testing
 */
inline domain::metadata create_large_metadata() {
  auto meta = create_minimal_metadata();

  // Add 10000 chunks
  for (uint32_t i = 0; i < 10000; ++i) {
    meta.chunks.push_back(domain::chunk{i / 1000, (i % 1000) * 64, 64});
  }

  // Add 1000 directories
  for (uint32_t i = 0; i < 1000; ++i) {
    meta.directories.push_back(domain::directory{i * 10, i > 0 ? (i - 1) : 0});
  }

  // Add 5000 inodes
  for (uint32_t i = 0; i < 5000; ++i) {
    domain::inode_data inode;
    inode.mode_index = i % 20;
    inode.owner_index = i % 10;
    inode.group_index = i % 10;
    meta.inodes.push_back(inode);
  }

  // Add many names
  for (int i = 0; i < 1000; ++i) {
    meta.names.push_back("very_long_filename_with_many_characters_" +
                         std::to_string(i) + ".txt");
  }

  // Add symlinks
  for (int i = 0; i < 100; ++i) {
    meta.symlinks.push_back("/very/long/path/to/target/file_" +
                            std::to_string(i) + ".so");
  }

  // Add all optional fields
  meta.dwarfs_version = "0.7.0";
  meta.create_timestamp = 1609459200;
  meta.preferred_path_separator = '/';
  meta.features = std::set<std::string>{
    "symlinks", "devices", "packed_directories", "packed_chunk_table"
  };
  meta.category_names = std::vector<std::string>{"text", "binary", "image", "video"};

  return meta;
}

} // namespace dwarfs::metadata::test