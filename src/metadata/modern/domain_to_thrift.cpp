/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file domain_to_thrift.cpp
 * \brief Convert domain model to Modern Thrift types
 *
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#include "dwarfs/metadata/modern/domain_to_thrift.h"

#include "metadata_modern_types.h"  // Generated Thrift types

// Map fbthrift's generated cpp2 namespace to clean public API namespace
namespace dwarfs::thrift::modern {
// Bring all generated types into our clean namespace
using namespace dwarfs::thrift::modern::cpp2;
} // namespace dwarfs::thrift::modern

namespace dwarfs::metadata::modern {

namespace {

// Helper: Convert unsigned vector to signed (Thrift doesn't have unsigned types)
template<typename T>
std::vector<int32_t> to_signed_i32_vec(std::vector<T> const& uv) {
  std::vector<int32_t> sv;
  sv.reserve(uv.size());
  for (auto val : uv) {
    sv.push_back(static_cast<int32_t>(val));
  }
  return sv;
}

template<typename T>
std::vector<int64_t> to_signed_i64_vec(std::vector<T> const& uv) {
  std::vector<int64_t> sv;
  sv.reserve(uv.size());
  for (auto val : uv) {
    sv.push_back(static_cast<int64_t>(val));
  }
  return sv;
}

std::map<int32_t, int32_t> to_signed_i32_map(std::map<uint32_t, uint32_t> const& um) {
  std::map<int32_t, int32_t> sm;
  for (auto const& [key, val] : um) {
    sm[static_cast<int32_t>(key)] = static_cast<int32_t>(val);
  }
  return sm;
}

// Helper: Convert chunk
thrift::modern::Chunk convert_chunk(domain::chunk const& dc) {
  thrift::modern::Chunk tc;
  tc.block_ref() = dc.block();
  tc.offset_ref() = dc.offset();
  tc.size_ref() = dc.size();
  return tc;
}

// Helper: Convert directory
thrift::modern::Directory convert_directory(domain::directory const& dd) {
  thrift::modern::Directory td;
  td.firstEntry_ref() = dd.first_entry();
  td.parentEntry_ref() = dd.parent_entry();
  td.selfEntry_ref() = dd.self_entry();
  return td;
}

// Helper: Convert inode_data
thrift::modern::InodeData convert_inode_data(domain::inode_data const& di) {
  thrift::modern::InodeData ti;
  ti.modeIndex_ref() = di.mode_index;
  ti.ownerIndex_ref() = di.owner_index;
  ti.groupIndex_ref() = di.group_index;
  ti.atimeOffset_ref() = di.atime_offset;
  ti.mtimeOffset_ref() = di.mtime_offset;
  ti.ctimeOffset_ref() = di.ctime_offset;
  ti.btimeOffset_ref() = di.btime_offset;
  ti.atimeSubsec_ref() = di.atime_subsec;
  ti.mtimeSubsec_ref() = di.mtime_subsec;
  ti.ctimeSubsec_ref() = di.ctime_subsec;
  ti.btimeSubsec_ref() = di.btime_subsec;
  ti.nlinkMinusOne_ref() = di.nlink_minus_one;

  // Optional v2.2 compatibility fields
  if (di.name_index_v2_2) {
    ti.nameIndexV2_2_ref() = *di.name_index_v2_2;
  }
  if (di.inode_v2_2) {
    ti.inodeV2_2_ref() = *di.inode_v2_2;
  }

  return ti;
}

// Helper: Convert dir_entry
thrift::modern::DirEntry convert_dir_entry(domain::dir_entry const& de) {
  thrift::modern::DirEntry te;
  te.nameIndex_ref() = de.name_index();
  te.inodeNum_ref() = de.inode_num();
  return te;
}

// Helper: Convert fs_options
thrift::modern::FsOptions convert_fs_options(domain::fs_options const& dfo) {
  thrift::modern::FsOptions tfo;
  tfo.mtimeOnly_ref() = dfo.mtime_only;
  tfo.packedChunkTable_ref() = dfo.packed_chunk_table;
  tfo.packedDirectories_ref() = dfo.packed_directories;
  tfo.packedSharedFilesTable_ref() = dfo.packed_shared_files_table;
  tfo.hasBtime_ref() = dfo.has_btime;
  tfo.inodesHaveNlink_ref() = dfo.inodes_have_nlink;

  if (dfo.time_resolution_sec) {
    tfo.timeResolutionSec_ref() = *dfo.time_resolution_sec;
  }
  if (dfo.subsecond_resolution_nsec_multiplier) {
    tfo.subsecondResolutionNsecMultiplier_ref() = *dfo.subsecond_resolution_nsec_multiplier;
  }

  return tfo;
}

// Helper: Convert string_table
thrift::modern::StringTable convert_string_table(domain::string_table const& dst) {
  thrift::modern::StringTable tst;
  tst.buffer_ref() = dst.buffer;
  tst.index_ref() = to_signed_i32_vec(dst.index);
  tst.packedIndex_ref() = dst.packed_index;

  if (dst.symtab) {
    tst.symtab_ref() = *dst.symtab;
  }

  return tst;
}

// Helper: Convert inode_size_cache
thrift::modern::InodeSizeCache convert_inode_size_cache(domain::inode_size_cache const& disc) {
  thrift::modern::InodeSizeCache tisc;

  // Convert size_lookup map
  for (auto const& [key, val] : disc.size_lookup) {
    tisc.sizeLookup_ref()[static_cast<int32_t>(key)] = static_cast<int64_t>(val);
  }

  tisc.minChunkCount_ref() = static_cast<int64_t>(disc.min_chunk_count);

  // Convert allocated_size_lookup map
  for (auto const& [key, val] : disc.allocated_size_lookup) {
    tisc.allocatedSizeLookup_ref()[static_cast<int32_t>(key)] = static_cast<int64_t>(val);
  }

  return tisc;
}

// Helper: Convert history_entry
thrift::modern::HistoryEntry convert_history_entry(domain::history_entry const& dhe) {
  thrift::modern::HistoryEntry the;
  the.major_ref() = dhe.major;
  the.minor_ref() = dhe.minor;
  the.blockSize_ref() = dhe.block_size;

  if (dhe.dwarfs_version) {
    the.dwarfsVersion_ref() = *dhe.dwarfs_version;
  }
  if (dhe.options) {
    the.options_ref() = convert_fs_options(*dhe.options);
  }

  return the;
}

} // anonymous namespace

thrift::modern::Metadata domain_to_thrift(domain::metadata const& dm) {
  thrift::modern::Metadata tm;

  // Convert core structures
  for (const auto& chunk : dm.chunks) {
    tm.chunks_ref()->push_back(convert_chunk(chunk));
  }

  for (const auto& dir : dm.directories) {
    tm.directories_ref()->push_back(convert_directory(dir));
  }

  for (const auto& inode : dm.inodes) {
    tm.inodes_ref()->push_back(convert_inode_data(inode));
  }

  // Convert tables
  tm.chunkTable_ref() = to_signed_i32_vec(dm.chunk_table);
  tm.entryTableV2_2_ref() = to_signed_i32_vec(dm.entry_table_v2_2);
  tm.symlinkTable_ref() = to_signed_i32_vec(dm.symlink_table);

  // Convert lookup tables
  tm.uids_ref() = to_signed_i32_vec(dm.uids);
  tm.gids_ref() = to_signed_i32_vec(dm.gids);
  tm.modes_ref() = to_signed_i32_vec(dm.modes);
  tm.names_ref() = dm.names;
  tm.symlinks_ref() = dm.symlinks;

  // Convert filesystem parameters
  tm.timestampBase_ref() = dm.timestamp_base;
  tm.blockSize_ref() = dm.block_size;
  tm.totalFsSize_ref() = dm.total_fs_size;

  // Convert optional features (v2.1+)
  if (dm.devices) {
    tm.devices_ref() = to_signed_i64_vec(*dm.devices);
  }
  if (dm.options) {
    tm.options_ref() = convert_fs_options(*dm.options);
  }

  // Convert directory entries (v2.3+)
  if (dm.dir_entries) {
    std::vector<thrift::modern::DirEntry> entries;
    for (const auto& entry : *dm.dir_entries) {
      entries.push_back(convert_dir_entry(entry));
    }
    tm.dirEntries_ref() = std::move(entries);
  }

  if (dm.shared_files_table) {
    tm.sharedFilesTable_ref() = to_signed_i32_vec(*dm.shared_files_table);
  }
  if (dm.total_hardlink_size) {
    tm.totalHardlinkSize_ref() = *dm.total_hardlink_size;
  }

  // Convert metadata information
  if (dm.dwarfs_version) {
    tm.dwarfsVersion_ref() = *dm.dwarfs_version;
  }
  if (dm.create_timestamp) {
    tm.createTimestamp_ref() = *dm.create_timestamp;
  }

  // Convert compact string storage (v2.3+)
  if (dm.compact_names) {
    tm.compactNames_ref() = convert_string_table(*dm.compact_names);
  }
  if (dm.compact_symlinks) {
    tm.compactSymlinks_ref() = convert_string_table(*dm.compact_symlinks);
  }

  // Convert path separator (v2.5+)
  if (dm.preferred_path_separator) {
    tm.preferredPathSeparator_ref() = *dm.preferred_path_separator;
  }

  // Convert features and categories (v2.5+)
  if (dm.features) {
    tm.features_ref() = *dm.features;
  }
  if (dm.category_names) {
    tm.categoryNames_ref() = *dm.category_names;
  }
  if (dm.block_categories) {
    tm.blockCategories_ref() = to_signed_i32_vec(*dm.block_categories);
  }

  // Convert performance caches (v2.5+)
  if (dm.reg_file_size_cache) {
    tm.regFileSizeCache_ref() = convert_inode_size_cache(*dm.reg_file_size_cache);
  }

  // Convert category metadata (v2.5+)
  if (dm.category_metadata_json) {
    tm.categoryMetadataJson_ref() = *dm.category_metadata_json;
  }
  if (dm.block_category_metadata) {
    tm.blockCategoryMetadata_ref() = to_signed_i32_map(*dm.block_category_metadata);
  }

  // Convert version history (v2.5+)
  if (dm.metadata_version_history) {
    std::vector<thrift::modern::HistoryEntry> history;
    for (const auto& entry : *dm.metadata_version_history) {
      history.push_back(convert_history_entry(entry));
    }
    tm.metadataVersionHistory_ref() = std::move(history);
  }

  // Convert sparse file support (v2.5+)
  if (dm.hole_block_index) {
    tm.holeBlockIndex_ref() = *dm.hole_block_index;
  }
  if (dm.large_hole_size) {
    tm.largeHoleSize_ref() = to_signed_i64_vec(*dm.large_hole_size);
  }
  if (dm.total_allocated_fs_size) {
    tm.totalAllocatedFsSize_ref() = *dm.total_allocated_fs_size;
  }

  return tm;
}

} // namespace dwarfs::metadata::modern
