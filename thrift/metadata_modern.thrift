/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * Modern Thrift Metadata Schema for DwarFS
 *
 * Uses CompactProtocol for smallest possible size
 * All fields use camelCase naming convention
 *
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 * SPDX-License-Identifier: MIT
 */

namespace cpp dwarfs.thrift.modern

/**
 * One chunk of data
 */
struct Chunk {
  1: i32 block
  2: i32 offset
  3: i32 size
}

/**
 * One directory
 */
struct Directory {
  1: i32 firstEntry
  2: i32 parentEntry
  3: i32 selfEntry
}

/**
 * Inode Data
 */
struct InodeData {
  1: i32 modeIndex
  2: i32 ownerIndex
  3: i32 groupIndex
  4: i64 atimeOffset
  5: i64 mtimeOffset
  6: i64 ctimeOffset
  7: i64 btimeOffset
  8: i64 atimeSubsec
  9: i64 mtimeSubsec
  10: i64 ctimeSubsec
  11: i64 btimeSubsec
  12: i32 nlinkMinusOne
  13: optional i32 nameIndexV2_2
  14: optional i32 inodeV2_2
}

/**
 * Directory Entry
 */
struct DirEntry {
  1: i32 nameIndex
  2: i32 inodeNum
}

/**
 * Filesystem Options
 */
struct FsOptions {
  1: bool mtimeOnly
  2: bool packedChunkTable
  3: bool packedDirectories
  4: bool packedSharedFilesTable
  5: bool hasBtime
  6: bool inodesHaveNlink
  7: optional i32 timeResolutionSec
  8: optional i32 subsecondResolutionNsecMultiplier
}

/**
 * Compact String Table Storage
 */
struct StringTable {
  1: string buffer
  2: list<i32> index
  3: bool packedIndex
  4: optional string symtab
}

/**
 * Inode Size Cache for Performance
 */
struct InodeSizeCache {
  1: map<i32, i64> sizeLookup
  2: i64 minChunkCount
  3: map<i32, i64> allocatedSizeLookup
}

/**
 * History Entry for Version Tracking
 */
struct HistoryEntry {
  1: i32 major
  2: i32 minor
  3: i64 blockSize
  4: optional string dwarfsVersion
  5: optional FsOptions options
}

/**
 * Root Metadata Structure
 */
struct Metadata {
  // Core structures
  1: list<Chunk> chunks
  2: list<Directory> directories
  3: list<InodeData> inodes

  // Tables
  4: list<i32> chunkTable
  5: list<i32> entryTableV2_2
  6: list<i32> symlinkTable

  // Lookup tables
  7: list<i32> uids
  8: list<i32> gids
  9: list<i32> modes
  10: list<string> names
  11: list<string> symlinks

  // Filesystem parameters
  12: i64 timestampBase
  13: i64 blockSize
  14: i64 totalFsSize

  // Optional features (v2.1+)
  15: optional list<i64> devices
  16: optional FsOptions options

  // Directory entries (v2.3+)
  17: optional list<DirEntry> dirEntries
  18: optional list<i32> sharedFilesTable
  19: optional i64 totalHardlinkSize

  // Metadata information
  20: optional string dwarfsVersion
  21: optional i64 createTimestamp

  // Compact string storage (v2.3+)
  22: optional StringTable compactNames
  23: optional StringTable compactSymlinks

  // Path separator (v2.5+)
  24: optional i32 preferredPathSeparator

  // Features and categories (v2.5+)
  25: optional set<string> features
  26: optional list<string> categoryNames
  27: optional list<i32> blockCategories

  // Performance caches (v2.5+)
  28: optional InodeSizeCache regFileSizeCache

  // Category metadata (v2.5+)
  29: optional list<string> categoryMetadataJson
  30: optional map<i32, i32> blockCategoryMetadata

  // Version history (v2.5+)
  31: optional list<HistoryEntry> metadataVersionHistory

  // Sparse file support (v2.5+)
  32: optional i32 holeBlockIndex
  33: optional list<i64> largeHoleSize
  34: optional i64 totalAllocatedFsSize
}