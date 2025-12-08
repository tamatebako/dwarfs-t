/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file metadata_structures.h
 *
 * DwarFS Plain C++ Metadata Structures
 *
 * This file contains plain C++ equivalents of all Thrift metadata structures
 * used in DwarFS filesystem images. These structures are serialization-agnostic
 * and can be used with Thrift frozen layouts, Cereal, Bitsery, or other
 * serialization backends.
 *
 * Design Principles:
 * - MECE (Mutually Exclusive, Collectively Exhaustive) architecture
 * - Field-by-field correspondence with Thrift definitions
 * - Zero external dependencies (STL only, C++17)
 * - Version-aware field organization
 * - Memory-efficient container types
 *
 * Thrift Sources:
 * - thrift/metadata.thrift (core metadata structures)
 * - thrift/features.thrift (feature enum)
 * - thrift/history.thrift (version history structures)
 * - thrift/compression.thrift (compression block headers)
 *
 * \author Ribose
 * \date 2025-11-12
 * \copyright See LICENSE file
 */

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dwarfs::internal {

/**
 * Type Mappings from Thrift to C++:
 *
 * Thrift Type        → C++ Type
 * ────────────────────────────────────────────
 * UInt8              → uint8_t
 * UInt16             → uint16_t
 * UInt32             → uint32_t
 * UInt64             → uint64_t
 * string             → std::string
 * binary             → std::vector<uint8_t>
 * list<T>            → std::vector<T>
 * set<T>             → std::unordered_set<T>
 * map<K,V>           → std::unordered_map<K,V>
 * optional<T>        → std::optional<T>
 */

// ============================================================================
// Core Metadata Structures
// ============================================================================

/**
 * chunk - Represents a data chunk in the filesystem
 *
 * A chunk is a contiguous piece of file data stored in a compressed block.
 * Multiple chunks can reference the same block (data deduplication).
 *
 * Thrift Source: metadata.thrift, struct chunk
 * Version: 2.0+
 */
struct chunk {
  uint32_t block;   ///< File system block number
  uint32_t offset;  ///< Offset from start of block, in bytes
  uint32_t size;    ///< Size of chunk, in bytes
};

/**
 * directory - Represents a directory in the filesystem
 *
 * Directories are organized hierarchically. Each directory contains
 * references to its parent and entries (files/subdirectories).
 *
 * Thrift Source: metadata.thrift, struct directory
 * Version: 2.0+
 */
struct directory {
  uint32_t parent_entry;  ///< Index into dir_entries for parent directory
  uint32_t first_entry;   ///< Index into dir_entries for first child entry
  uint32_t self_entry;    ///< Index into dir_entries for self (v2.5+)
};

/**
 * inode_data - Represents inode metadata for a file system object
 *
 * Contains all metadata associated with an inode: permissions, ownership,
 * timestamps, and hard link information.
 *
 * Thrift Source: metadata.thrift, struct inode_data
 * Version: 2.0+ (core fields), 2.5+ (extended fields), 2.2 (deprecated fields)
 */
struct inode_data {
  // Core fields (v2.0+)
  uint32_t mode_index;    ///< Index into modes[] array
  uint32_t owner_index;   ///< Index into uids[] array (user ID)
  uint32_t group_index;   ///< Index into gids[] array (group ID)
  uint64_t atime_offset;  ///< Access time offset relative to timestamp_base
  uint64_t mtime_offset;  ///< Modification time offset relative to timestamp_base
  uint64_t ctime_offset;  ///< Status change time offset relative to timestamp_base

  // Extended fields (v2.5+)
  uint64_t btime_offset;    ///< Birth time offset (creation time)
  uint64_t atime_subsec;    ///< Subsecond part of access time
  uint64_t mtime_subsec;    ///< Subsecond part of modification time
  uint64_t ctime_subsec;    ///< Subsecond part of status change time
  uint64_t btime_subsec;    ///< Subsecond part of birth time
  uint32_t nlink_minus_one; ///< Number of hard links minus one

  // Deprecated fields (v2.2 only, kept for backward compatibility)
  uint32_t name_index_v2_2; ///< DEPRECATED: v2.2 name index (unused in v2.3+)
  uint32_t inode_v2_2;      ///< DEPRECATED: v2.2 inode number (unused in v2.3+)
};

/**
 * dir_entry - Represents a directory entry (name → inode mapping)
 *
 * Links a filename to its corresponding inode. All directory entries
 * are stored in a flat array for efficient traversal.
 *
 * Thrift Source: metadata.thrift, struct dir_entry
 * Version: 2.3+
 */
struct dir_entry {
  uint32_t name_index; ///< Index into names[] or compact_names
  uint32_t inode_num;  ///< Index into inodes[] array
};

/**
 * fs_options - Filesystem-wide configuration options
 *
 * Controls various filesystem features and compression strategies.
 * Determines how timestamps, tables, and other data are stored.
 *
 * Thrift Source: metadata.thrift, struct fs_options
 * Version: 2.1+ (core), 2.5+ (subsecond fields)
 */
struct fs_options {
  bool mtime_only;                       ///< Only mtime stored (no atime/ctime)
  std::optional<uint32_t> time_resolution_sec; ///< Time resolution in seconds
  bool packed_chunk_table;               ///< Chunk table is delta-compressed
  bool packed_directories;               ///< Directories are delta-compressed
  bool packed_shared_files_table;        ///< Shared files table is delta-compressed

  // v2.5+ subsecond and birth time support
  std::optional<uint32_t> subsecond_resolution_nsec_multiplier; ///< Subsecond multiplier
  bool has_btime;                        ///< Birth time (creation time) included
  bool inodes_have_nlink;                ///< Hard link counts stored in inode_data
};

/**
 * string_table - Compressed string storage with optional FSST compression
 *
 * Stores a collection of strings (filenames or symlinks) in a compressed format.
 * Uses FSST (Fast Static Symbol Table) compression for improved space efficiency.
 *
 * Thrift Source: metadata.thrift, struct string_table
 * Version: 2.3+
 */
struct string_table {
  std::string buffer;                    ///< Concatenated strings (may be FSST compressed)
  std::optional<std::string> symtab;     ///< FSST symbol table (present if compressed)
  std::vector<uint32_t> index;           ///< Offsets into buffer (may be delta-compressed)
  bool packed_index;                     ///< Index is delta-compressed
};

/**
 * inode_size_cache - Cache for highly fragmented file sizes
 *
 * Provides O(1) size lookup for files with many chunks. Only stores
 * entries for files exceeding the min_chunk_count threshold.
 *
 * Thrift Source: metadata.thrift, struct inode_size_cache
 * Version: 2.5+
 */
struct inode_size_cache {
  std::unordered_map<uint32_t, uint64_t> size_lookup; ///< inode → file size mapping
  uint64_t min_chunk_count;                            ///< Cache threshold (chunk count)
  std::unordered_map<uint32_t, uint64_t> allocated_size_lookup; ///< inode → allocated size (sparse)
};

/**
 * history_entry - Legacy filesystem version history entry
 *
 * Simplified version history format used in older metadata versions.
 *
 * Thrift Source: metadata.thrift, struct history_entry
 * Version: 2.0+
 */
struct history_entry {
  uint8_t major;                          ///< Major version number
  uint8_t minor;                          ///< Minor version number
  std::optional<std::string> dwarfs_version; ///< DwarFS version string
  uint32_t block_size;                    ///< Block size in bytes
  std::optional<fs_options> options;      ///< Filesystem options at creation
};

// ============================================================================
// Feature Enumeration
// ============================================================================

/**
 * feature - Filesystem feature flags
 *
 * Enum of optional filesystem features that can be enabled.
 * Features are stored as strings in metadata for extensibility.
 *
 * Thrift Source: features.thrift, enum feature
 * Version: 2.5+
 */
enum class feature : uint8_t {
  sparsefiles = 0, ///< Support for sparse files with holes (v0.14.0)
};

// ============================================================================
// Root Metadata Structure
// ============================================================================

/**
 * metadata - Root filesystem metadata structure
 *
 * Contains all filesystem metadata organized by version. This is the
 * top-level structure serialized to disk.
 *
 * Field Organization:
 * - Fields 1-16:  Core metadata (v2.0+)
 * - Fields 17-18: Version 2.1 additions
 * - Fields 19-25: Version 2.3 additions
 * - Fields 26-36: Version 2.5 additions
 * - Field 37:     Reserved (never released but may exist in some images)
 *
 * Thrift Source: metadata.thrift, struct metadata
 * Version: 2.0+ (evolving structure)
 */
struct metadata {
  // ── Core Metadata (v2.0+) ────────────────────────────────────────────────

  std::vector<chunk> chunks;               ///< [1] File data chunks
  std::vector<directory> directories;      ///< [2] Directory metadata
  std::vector<inode_data> inodes;          ///< [3] Inode metadata
  std::vector<uint32_t> chunk_table;       ///< [4] Chunk lookup table (may be delta-compressed)
  std::vector<uint32_t> entry_table_v2_2;  ///< [5] DEPRECATED: v2.2 entry table (unused in v2.3+)
  std::vector<uint32_t> symlink_table;     ///< [6] Symlink target lookup table
  std::vector<uint32_t> uids;              ///< [7] User IDs (unique values)
  std::vector<uint32_t> gids;              ///< [8] Group IDs (unique values)
  std::vector<uint32_t> modes;             ///< [9] File modes (unique values)
  std::vector<std::string> names;          ///< [10] Entry names (may be replaced by compact_names)
  std::vector<std::string> symlinks;       ///< [11] Symlink targets (may be replaced by compact_symlinks)
  uint64_t timestamp_base;                 ///< [12] Base timestamp for all time offsets
  uint32_t block_size;                     ///< [15] Filesystem block size in bytes
  uint64_t total_fs_size;                  ///< [16] Total filesystem size in bytes

  // ── Version 2.1+ Fields ───────────────────────────────────────────────────

  std::optional<std::vector<uint64_t>> devices; ///< [17] Device IDs (for device files)
  std::optional<fs_options> options;            ///< [18] Filesystem configuration options

  // ── Version 2.3+ Fields ───────────────────────────────────────────────────

  std::optional<std::vector<dir_entry>> dir_entries;      ///< [19] All directory entries (flat array)
  std::optional<std::vector<uint32_t>> shared_files_table; ///< [20] Hard link mappings (may be delta-compressed)
  std::optional<uint64_t> total_hardlink_size;            ///< [21] DEPRECATED: total hard link size (kept for compat)
  std::optional<std::string> dwarfs_version;              ///< [22] DwarFS version string
  std::optional<uint64_t> create_timestamp;               ///< [23] Filesystem creation timestamp (Unix epoch)
  std::optional<string_table> compact_names;              ///< [24] Compressed entry names (FSST)
  std::optional<string_table> compact_symlinks;           ///< [25] Compressed symlink targets (FSST)

  // ── Version 2.5+ Fields ───────────────────────────────────────────────────

  std::optional<uint32_t> preferred_path_separator;       ///< [26] Path separator character (ASCII)
  std::optional<std::unordered_set<std::string>> features; ///< [27] Enabled feature flags
  std::optional<std::vector<std::string>> category_names; ///< [28] Block category names
  std::optional<std::vector<uint32_t>> block_categories;  ///< [29] Block → category index mapping
  std::optional<inode_size_cache> reg_file_size_cache;    ///< [30] Size cache for fragmented files
  std::optional<std::vector<std::string>> category_metadata_json; ///< [31] Category metadata (JSON)
  std::optional<std::unordered_map<uint32_t, uint32_t>> block_category_metadata; ///< [32] Block → metadata index
  std::optional<std::vector<history_entry>> metadata_version_history; ///< [33] Version history (legacy format)
  std::optional<uint32_t> hole_block_index;               ///< [34] Block index for sparse file holes
  std::optional<std::vector<uint64_t>> large_hole_size;   ///< [35] Large sparse hole sizes
  std::optional<uint64_t> total_allocated_fs_size;        ///< [36] Total allocated size (including holes)

  // Field [37] reserved (never released but may exist in some images)
};

// ============================================================================
// History Structures (Full Format)
// ============================================================================

/**
 * dwarfs_version - Detailed DwarFS version information
 *
 * Captures the exact DwarFS version used to create a filesystem,
 * including Git metadata for development builds.
 *
 * Thrift Source: history.thrift, struct dwarfs_version
 * Version: 2.5+
 */
struct dwarfs_version {
  uint16_t major;                          ///< Major version number
  uint16_t minor;                          ///< Minor version number
  uint16_t patch;                          ///< Patch version number
  bool is_release;                         ///< True for release builds, false for dev builds
  std::optional<std::string> git_rev;      ///< Git commit hash (dev builds)
  std::optional<std::string> git_branch;   ///< Git branch name (dev builds)
  std::optional<std::string> git_desc;     ///< Git describe output (dev builds)
};

/**
 * history_entry_full - Complete filesystem creation history entry
 *
 * Records full details about how a filesystem was created, including
 * build environment, compiler, and command-line arguments.
 *
 * Thrift Source: history.thrift, struct history_entry
 * Version: 2.5+
 */
struct history_entry_full {
  dwarfs_version version;                                  ///< DwarFS version
  std::string system_id;                                   ///< System identifier (OS, architecture)
  std::string compiler_id;                                 ///< Compiler identifier (name, version)
  std::optional<std::vector<std::string>> arguments;       ///< Command-line arguments
  std::optional<uint64_t> timestamp;                       ///< Creation timestamp (Unix epoch)
  std::optional<std::unordered_set<std::string>> library_versions; ///< Library versions (name=version)
};

/**
 * history - Complete filesystem creation history
 *
 * Contains a sequence of history entries documenting all filesystem
 * operations (create, rewrite, etc.).
 *
 * Thrift Source: history.thrift, struct history
 * Version: 2.5+
 */
struct history {
  std::vector<history_entry_full> entries; ///< Ordered history entries
};

// ============================================================================
// Compression Block Headers
// ============================================================================

/**
 * flac_block_header - FLAC compression block header
 *
 * Contains metadata for FLAC-compressed audio blocks. Used when
 * compressing PCM audio data with FLAC codec.
 *
 * Thrift Source: compression.thrift, struct flac_block_header
 * Version: 2.0+
 */
struct flac_block_header {
  uint16_t num_channels;    ///< Number of audio channels
  uint8_t bits_per_sample;  ///< Bits per sample (8, 16, 24, 32)
  uint8_t flags;            ///< FLAC encoder flags
};

/**
 * ricepp_block_header - Rice++ compression block header
 *
 * Contains metadata for Rice++-compressed blocks. Rice++ is an
 * efficient codec for compressing numerical data with good locality.
 *
 * Thrift Source: compression.thrift, struct ricepp_block_header
 * Version: 2.0+
 */
struct ricepp_block_header {
  uint32_t block_size;      ///< Size of compressed block in bytes
  uint16_t component_count; ///< Number of data components
  uint8_t bytes_per_sample; ///< Bytes per sample value
  uint8_t unused_lsb_count; ///< Number of unused LSB bits
  bool big_endian;          ///< True if data is big-endian
  uint16_t ricepp_version;  ///< Rice++ version number
};

} // namespace dwarfs::internal