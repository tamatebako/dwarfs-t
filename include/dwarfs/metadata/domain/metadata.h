#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "dwarfs/metadata/domain/chunk.h"
#include "dwarfs/metadata/domain/dir_entry.h"
#include "dwarfs/metadata/domain/directory.h"
#include "dwarfs/metadata/domain/fs_options.h"
#include "dwarfs/metadata/domain/history_entry.h"
#include "dwarfs/metadata/domain/inode_data.h"
#include "dwarfs/metadata/domain/inode_size_cache.h"
#include "dwarfs/metadata/domain/string_table.h"

namespace dwarfs::metadata::domain {

/**
 * Top-level metadata container for DwarFS filesystem.
 *
 * Single Responsibility: Hold all filesystem metadata
 * Dependencies: Other domain types (all from domain layer)
 *
 * This is a pure value object with no serialization knowledge.
 * Contains all metadata for an entire DwarFS filesystem including
 * chunks, directories, inodes, and various lookup tables.
 */
class metadata {
public:
  /**
   * Default constructor creates empty metadata
   */
  metadata() = default;

  // Core filesystem structures
  std::vector<chunk> chunks;
  std::vector<directory> directories;
  std::vector<inode_data> inodes;

  // Chunk and entry tables
  std::vector<uint32_t> chunk_table;
  std::vector<uint32_t> entry_table_v2_2;  // Deprecated (v2.2 compatibility)
  std::vector<uint32_t> symlink_table;

  // Lookup tables for deduplication
  std::vector<uint32_t> uids;
  std::vector<uint32_t> gids;
  std::vector<uint32_t> modes;
  std::vector<std::string> names;
  std::vector<std::string> symlinks;

  // Filesystem parameters
  uint64_t timestamp_base = 0;
  uint32_t block_size = 0;
  uint64_t total_fs_size = 0;

  // Optional features (v2.1+)
  std::optional<std::vector<uint64_t>> devices;
  std::optional<fs_options> options;

  // Directory entries (v2.3+)
  std::optional<std::vector<dir_entry>> dir_entries;
  std::optional<std::vector<uint32_t>> shared_files_table;
  std::optional<uint64_t> total_hardlink_size;

  // Metadata information
  std::optional<std::string> dwarfs_version;
  std::optional<uint64_t> create_timestamp;

  // Compact string storage (v2.3+)
  std::optional<string_table> compact_names;
  std::optional<string_table> compact_symlinks;

  // Path separator (v2.5+)
  std::optional<uint32_t> preferred_path_separator;

  // Features and categories (v2.5+)
  std::optional<std::set<std::string>> features;
  std::optional<std::vector<std::string>> category_names;
  std::optional<std::vector<uint32_t>> block_categories;

  // Performance caches (v2.5+)
  std::optional<inode_size_cache> reg_file_size_cache;

  // Uncompressed file sizes (direct lookup for readers)
  // Maps inode index to uncompressed file size, populated during write
  std::optional<std::vector<uint64_t>> uncompressed_file_sizes;

  // Category metadata (v2.5+)
  std::optional<std::vector<std::string>> category_metadata_json;
  std::optional<std::map<uint32_t, uint32_t>> block_category_metadata;

  // Version history (v2.5+)
  std::optional<std::vector<history_entry>> metadata_version_history;

  // Sparse file support (v2.5+)
  std::optional<uint32_t> hole_block_index;
  std::optional<std::vector<uint64_t>> large_hole_size;
  std::optional<uint64_t> total_allocated_fs_size;

  /**
   * Equality comparison for testing and validation
   *
   * @param other Metadata to compare with
   * @return true if all fields are equal
   */
  bool operator==(const metadata& other) const {
    return chunks == other.chunks
        && directories == other.directories
        && inodes == other.inodes
        && chunk_table == other.chunk_table
        && entry_table_v2_2 == other.entry_table_v2_2
        && symlink_table == other.symlink_table
        && uids == other.uids
        && gids == other.gids
        && modes == other.modes
        && names == other.names
        && symlinks == other.symlinks
        && timestamp_base == other.timestamp_base
        && block_size == other.block_size
        && total_fs_size == other.total_fs_size
        && devices == other.devices
        && options == other.options
        && dir_entries == other.dir_entries
        && shared_files_table == other.shared_files_table
        && total_hardlink_size == other.total_hardlink_size
        && dwarfs_version == other.dwarfs_version
        && create_timestamp == other.create_timestamp
        && compact_names == other.compact_names
        && compact_symlinks == other.compact_symlinks
        && preferred_path_separator == other.preferred_path_separator
        && features == other.features
        && category_names == other.category_names
        && block_categories == other.block_categories
        && reg_file_size_cache == other.reg_file_size_cache
        && uncompressed_file_sizes == other.uncompressed_file_sizes
        && category_metadata_json == other.category_metadata_json
        && block_category_metadata == other.block_category_metadata
        && metadata_version_history == other.metadata_version_history
        && hole_block_index == other.hole_block_index
        && large_hole_size == other.large_hole_size
        && total_allocated_fs_size == other.total_allocated_fs_size;
  }

  bool operator!=(const metadata& other) const {
    return !(*this == other);
  }
};

} // namespace dwarfs::metadata::domain