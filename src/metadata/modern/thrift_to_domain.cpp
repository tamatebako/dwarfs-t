/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file thrift_to_domain.cpp
 * \brief Convert Modern Thrift types to domain model
 *
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#include "dwarfs/metadata/modern/thrift_to_domain.h"

#include "metadata_modern_types.h"  // Generated Thrift types

// Map fbthrift's generated cpp2 namespace to clean public API namespace
namespace dwarfs::thrift::modern {
// Bring all generated types into our clean namespace
using namespace dwarfs::thrift::modern::cpp2;
} // namespace dwarfs::thrift::modern

namespace dwarfs::metadata::modern {

namespace {

// Helper: Convert signed vector to unsigned (Thrift doesn't have unsigned types)
template<typename T>
std::vector<uint32_t> to_unsigned_u32_vec(std::vector<T> const& sv) {
  std::vector<uint32_t> uv;
  uv.reserve(sv.size());
  for (auto val : sv) {
    uv.push_back(static_cast<uint32_t>(val));
  }
  return uv;
}

template<typename T>
std::vector<uint64_t> to_unsigned_u64_vec(std::vector<T> const& sv) {
  std::vector<uint64_t> uv;
  uv.reserve(sv.size());
  for (auto val : sv) {
    uv.push_back(static_cast<uint64_t>(val));
  }
  return uv;
}

std::map<uint32_t, uint32_t> to_unsigned_u32_map(std::map<int32_t, int32_t> const& sm) {
  std::map<uint32_t, uint32_t> um;
  for (auto const& [key, val] : sm) {
    um[static_cast<uint32_t>(key)] = static_cast<uint32_t>(val);
  }
  return um;
}

// Helper: Convert chunk
domain::chunk convert_chunk(thrift::modern::Chunk const& tc) {
  domain::chunk dc;
  dc.set_block(static_cast<uint32_t>(*tc.block_ref()));
  dc.set_offset(static_cast<uint32_t>(*tc.offset_ref()));
  dc.set_size(static_cast<uint32_t>(*tc.size_ref()));
  return dc;
}

// Helper: Convert directory
domain::directory convert_directory(thrift::modern::Directory const& td) {
  domain::directory dd;
  dd.set_first_entry(static_cast<uint32_t>(*td.firstEntry_ref()));
  dd.set_parent_entry(static_cast<uint32_t>(*td.parentEntry_ref()));
  dd.set_self_entry(static_cast<uint32_t>(*td.selfEntry_ref()));
  return dd;
}

// Helper: Convert inode_data
domain::inode_data convert_inode_data(thrift::modern::InodeData const& ti) {
  domain::inode_data di;
  di.mode_index = static_cast<uint32_t>(*ti.modeIndex_ref());
  di.owner_index = static_cast<uint32_t>(*ti.ownerIndex_ref());
  di.group_index = static_cast<uint32_t>(*ti.groupIndex_ref());
  di.atime_offset = static_cast<uint64_t>(*ti.atimeOffset_ref());
  di.mtime_offset = static_cast<uint64_t>(*ti.mtimeOffset_ref());
  di.ctime_offset = static_cast<uint64_t>(*ti.ctimeOffset_ref());
  di.btime_offset = static_cast<uint64_t>(*ti.btimeOffset_ref());
  di.atime_subsec = static_cast<uint64_t>(*ti.atimeSubsec_ref());
  di.mtime_subsec = static_cast<uint64_t>(*ti.mtimeSubsec_ref());
  di.ctime_subsec = static_cast<uint64_t>(*ti.ctimeSubsec_ref());
  di.btime_subsec = static_cast<uint64_t>(*ti.btimeSubsec_ref());
  di.nlink_minus_one = static_cast<uint32_t>(*ti.nlinkMinusOne_ref());

  // Optional v2.2 compatibility fields
  if (ti.nameIndexV2_2_ref().has_value()) {
    di.name_index_v2_2 = static_cast<uint32_t>(*ti.nameIndexV2_2_ref());
  }
  if (ti.inodeV2_2_ref().has_value()) {
    di.inode_v2_2 = static_cast<uint32_t>(*ti.inodeV2_2_ref());
  }

  return di;
}

// Helper: Convert dir_entry
domain::dir_entry convert_dir_entry(thrift::modern::DirEntry const& te) {
  domain::dir_entry de;
  de.set_name_index(static_cast<uint32_t>(*te.nameIndex_ref()));
  de.set_inode_num(static_cast<uint32_t>(*te.inodeNum_ref()));
  return de;
}

// Helper: Convert fs_options
domain::fs_options convert_fs_options(thrift::modern::FsOptions const& tfo) {
  domain::fs_options dfo;
  dfo.mtime_only = *tfo.mtimeOnly_ref();
  dfo.packed_chunk_table = *tfo.packedChunkTable_ref();
  dfo.packed_directories = *tfo.packedDirectories_ref();
  dfo.packed_shared_files_table = *tfo.packedSharedFilesTable_ref();
  dfo.has_btime = *tfo.hasBtime_ref();
  dfo.inodes_have_nlink = *tfo.inodesHaveNlink_ref();

  if (tfo.timeResolutionSec_ref().has_value()) {
    dfo.time_resolution_sec = static_cast<uint32_t>(*tfo.timeResolutionSec_ref());
  }
  if (tfo.subsecondResolutionNsecMultiplier_ref().has_value()) {
    dfo.subsecond_resolution_nsec_multiplier = static_cast<uint32_t>(*tfo.subsecondResolutionNsecMultiplier_ref());
  }

  return dfo;
}

// Helper: Convert string_table
domain::string_table convert_string_table(thrift::modern::StringTable const& tst) {
  domain::string_table dst;
  dst.buffer = *tst.buffer_ref();
  dst.index = to_unsigned_u32_vec(*tst.index_ref());
  dst.packed_index = *tst.packedIndex_ref();

  if (tst.symtab_ref().has_value()) {
    dst.symtab = *tst.symtab_ref();
  }

  return dst;
}

// Helper: Convert inode_size_cache
domain::inode_size_cache convert_inode_size_cache(thrift::modern::InodeSizeCache const& tisc) {
  domain::inode_size_cache disc;

  // Convert size_lookup map
  for (auto const& [key, val] : *tisc.sizeLookup_ref()) {
    disc.size_lookup[static_cast<uint32_t>(key)] = static_cast<uint64_t>(val);
  }

  disc.min_chunk_count = static_cast<uint64_t>(*tisc.minChunkCount_ref());

  // Convert allocated_size_lookup map
  for (auto const& [key, val] : *tisc.allocatedSizeLookup_ref()) {
    disc.allocated_size_lookup[static_cast<uint32_t>(key)] = static_cast<uint64_t>(val);
  }

  return disc;
}

// Helper: Convert history_entry
domain::history_entry convert_history_entry(thrift::modern::HistoryEntry const& the) {
  domain::history_entry dhe;
  dhe.major = static_cast<uint8_t>(*the.major_ref());
  dhe.minor = static_cast<uint8_t>(*the.minor_ref());
  dhe.block_size = static_cast<uint64_t>(*the.blockSize_ref());

  if (the.dwarfsVersion_ref().has_value()) {
    dhe.dwarfs_version = *the.dwarfsVersion_ref();
  }
  if (the.options_ref().has_value()) {
    dhe.options = convert_fs_options(*the.options_ref());
  }

  return dhe;
}

} // anonymous namespace

domain::metadata thrift_to_domain(thrift::modern::Metadata const& tm) {
  domain::metadata dm;

  // Convert core structures
  for (const auto& chunk : *tm.chunks_ref()) {
    dm.chunks.push_back(convert_chunk(chunk));
  }

  for (const auto& dir : *tm.directories_ref()) {
    dm.directories.push_back(convert_directory(dir));
  }

  for (const auto& inode : *tm.inodes_ref()) {
    dm.inodes.push_back(convert_inode_data(inode));
  }

  // Convert tables
  dm.chunk_table = to_unsigned_u32_vec(*tm.chunkTable_ref());
  dm.entry_table_v2_2 = to_unsigned_u32_vec(*tm.entryTableV2_2_ref());
  dm.symlink_table = to_unsigned_u32_vec(*tm.symlinkTable_ref());

  // Convert lookup tables
  dm.uids = to_unsigned_u32_vec(*tm.uids_ref());
  dm.gids = to_unsigned_u32_vec(*tm.gids_ref());
  dm.modes = to_unsigned_u32_vec(*tm.modes_ref());
  dm.names = *tm.names_ref();
  dm.symlinks = *tm.symlinks_ref();

  // Convert filesystem parameters
  dm.timestamp_base = static_cast<uint64_t>(*tm.timestampBase_ref());
  dm.block_size = static_cast<uint32_t>(*tm.blockSize_ref());
  dm.total_fs_size = static_cast<uint64_t>(*tm.totalFsSize_ref());

  // Convert optional features (v2.1+)
  if (tm.devices_ref().has_value()) {
    dm.devices = to_unsigned_u64_vec(*tm.devices_ref());
  }
  if (tm.options_ref().has_value()) {
    dm.options = convert_fs_options(*tm.options_ref());
  }

  // Convert directory entries (v2.3+)
  if (tm.dirEntries_ref().has_value()) {
    std::vector<domain::dir_entry> entries;
    for (const auto& entry : *tm.dirEntries_ref()) {
      entries.push_back(convert_dir_entry(entry));
    }
    dm.dir_entries = std::move(entries);
  }

  if (tm.sharedFilesTable_ref().has_value()) {
    dm.shared_files_table = to_unsigned_u32_vec(*tm.sharedFilesTable_ref());
  }
  if (tm.totalHardlinkSize_ref().has_value()) {
    dm.total_hardlink_size = static_cast<uint64_t>(*tm.totalHardlinkSize_ref());
  }

  // Convert metadata information
  if (tm.dwarfsVersion_ref().has_value()) {
    dm.dwarfs_version = *tm.dwarfsVersion_ref();
  }
  if (tm.createTimestamp_ref().has_value()) {
    dm.create_timestamp = static_cast<uint64_t>(*tm.createTimestamp_ref());
  }

  // Convert compact string storage (v2.3+)
  if (tm.compactNames_ref().has_value()) {
    dm.compact_names = convert_string_table(*tm.compactNames_ref());
  }
  if (tm.compactSymlinks_ref().has_value()) {
    dm.compact_symlinks = convert_string_table(*tm.compactSymlinks_ref());
  }

  // Convert path separator (v2.5+)
  if (tm.preferredPathSeparator_ref().has_value()) {
    dm.preferred_path_separator = static_cast<uint32_t>(*tm.preferredPathSeparator_ref());
  }

  // Convert features and categories (v2.5+)
  if (tm.features_ref().has_value()) {
    dm.features = *tm.features_ref();
  }
  if (tm.categoryNames_ref().has_value()) {
    dm.category_names = *tm.categoryNames_ref();
  }
  if (tm.blockCategories_ref().has_value()) {
    dm.block_categories = to_unsigned_u32_vec(*tm.blockCategories_ref());
  }

  // Convert performance caches (v2.5+)
  if (tm.regFileSizeCache_ref().has_value()) {
    dm.reg_file_size_cache = convert_inode_size_cache(*tm.regFileSizeCache_ref());
  }

  // Convert category metadata (v2.5+)
  if (tm.categoryMetadataJson_ref().has_value()) {
    dm.category_metadata_json = *tm.categoryMetadataJson_ref();
  }
  if (tm.blockCategoryMetadata_ref().has_value()) {
    dm.block_category_metadata = to_unsigned_u32_map(*tm.blockCategoryMetadata_ref());
  }

  // Convert version history (v2.5+)
  if (tm.metadataVersionHistory_ref().has_value()) {
    std::vector<domain::history_entry> history;
    for (const auto& entry : *tm.metadataVersionHistory_ref()) {
      history.push_back(convert_history_entry(entry));
    }
    dm.metadata_version_history = std::move(history);
  }

  // Convert sparse file support (v2.5+)
  if (tm.holeBlockIndex_ref().has_value()) {
    dm.hole_block_index = static_cast<uint32_t>(*tm.holeBlockIndex_ref());
  }
  if (tm.largeHoleSize_ref().has_value()) {
    dm.large_hole_size = to_unsigned_u64_vec(*tm.largeHoleSize_ref());
  }
  if (tm.totalAllocatedFsSize_ref().has_value()) {
    dm.total_allocated_fs_size = static_cast<uint64_t>(*tm.totalAllocatedFsSize_ref());
  }

  return dm;
}

} // namespace dwarfs::metadata::modern
