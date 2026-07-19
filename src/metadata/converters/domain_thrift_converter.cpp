//( Create new converter implementation
/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file domain_thrift_converter.cpp
 *
 * Implementation of bidirectional conversion between Thrift and domain model.
 *
 * \author Ribose Inc.
 * \date 2025-11-17
 * \copyright See LICENSE file
 */

#include "dwarfs/metadata/converters/domain_thrift_converter.h"

#include <dwarfs/gen-cpp2/metadata_types.h>

namespace dwarfs::metadata::converters {

// ============================================================================
// Thrift → Domain Conversions
// ============================================================================

// Helper: Convert Thrift chunk to domain
domain::chunk from_thrift(dwarfs::thrift::metadata::chunk const& t) {
  return domain::chunk(
    t.block().value(),
    t.offset().value(),
    t.size().value()
  );
}

// Helper: Convert Thrift directory to domain
domain::directory from_thrift(dwarfs::thrift::metadata::directory const& t) {
  return domain::directory(
    t.parent_entry().value(),
    t.first_entry().value(),
    t.self_entry().value()
  );
}

// Helper: Convert Thrift inode_data to domain
domain::inode_data from_thrift(dwarfs::thrift::metadata::inode_data const& t) {
  domain::inode_data i;
  i.mode_index = t.mode_index().value();
  i.owner_index = t.owner_index().value();
  i.group_index = t.group_index().value();
  i.atime_offset = t.atime_offset().value();
  i.mtime_offset = t.mtime_offset().value();
  i.ctime_offset = t.ctime_offset().value();

  // Optional fields
  if (t.btime_offset().has_value()) i.btime_offset = t.btime_offset().value();
  if (t.atime_subsec().has_value()) i.atime_subsec = t.atime_subsec().value();
  if (t.mtime_subsec().has_value()) i.mtime_subsec = t.mtime_subsec().value();
  if (t.ctime_subsec().has_value()) i.ctime_subsec = t.ctime_subsec().value();
  if (t.btime_subsec().has_value()) i.btime_subsec = t.btime_subsec().value();
  if (t.nlink_minus_one().has_value()) i.nlink_minus_one = t.nlink_minus_one().value();
  if (t.name_index_v2_2().has_value()) i.name_index_v2_2 = t.name_index_v2_2().value();
  if (t.inode_v2_2().has_value()) i.inode_v2_2 = t.inode_v2_2().value();

  return i;
}

domain::dir_entry from_thrift(const dwarfs::thrift::metadata::dir_entry& t) {
  return domain::dir_entry(
    t.name_index().value(),
    t.inode_num().value()
  );
}

domain::fs_options from_thrift(const dwarfs::thrift::metadata::fs_options& t) {
  domain::fs_options opts;

  opts.mtime_only = t.mtime_only().has_value() ? t.mtime_only().value() : false;
  opts.packed_chunk_table = t.packed_chunk_table().has_value() ? t.packed_chunk_table().value() : false;
  opts.packed_directories = t.packed_directories().has_value() ? t.packed_directories().value() : false;
  opts.packed_shared_files_table = t.packed_shared_files_table().has_value() ? t.packed_shared_files_table().value() : false;
  opts.has_btime = t.has_btime().has_value() ? t.has_btime().value() : false;
  opts.inodes_have_nlink = t.inodes_have_nlink().has_value() ? t.inodes_have_nlink().value() : false;

  if (t.time_resolution_sec().has_value()) {
    opts.time_resolution_sec = t.time_resolution_sec().value();
  }

  if (t.subsecond_resolution_nsec_multiplier().has_value()) {
    opts.subsecond_resolution_nsec_multiplier =
      t.subsecond_resolution_nsec_multiplier().value();
  }

  return opts;
}

domain::string_table from_thrift(const dwarfs::thrift::metadata::string_table& t) {
  domain::string_table st;

  st.buffer = t.buffer().has_value() ? t.buffer().value() : std::string();
  st.packed_index = t.packed_index().has_value() ? t.packed_index().value() : false;

  if (t.index().has_value()) {
    const auto& thrift_index = t.index().value();
    st.index.assign(thrift_index.begin(), thrift_index.end());
  }

  if (t.symtab().has_value()) {
    st.symtab = t.symtab().value();
  }

  return st;
}

domain::inode_size_cache from_thrift(const dwarfs::thrift::metadata::inode_size_cache& t) {
  domain::inode_size_cache cache;

  if (t.size_lookup().has_value()) {
    const auto& thrift_map = t.size_lookup().value();
    cache.size_lookup.insert(thrift_map.begin(), thrift_map.end());
  }
  cache.min_chunk_count = t.min_chunk_count().has_value() ? t.min_chunk_count().value() : 0;

  if (t.allocated_size_lookup().has_value()) {
    const auto& thrift_map = t.allocated_size_lookup().value();
    cache.allocated_size_lookup.insert(thrift_map.begin(), thrift_map.end());
  }

  return cache;
}

domain::history_entry from_thrift(const dwarfs::thrift::metadata::history_entry& t) {
  domain::history_entry e;

  e.major = t.major().has_value() ? t.major().value() : 0;
  e.minor = t.minor().has_value() ? t.minor().value() : 0;
  e.block_size = t.block_size().has_value() ? t.block_size().value() : 0;

  if (t.dwarfs_version().has_value()) {
    e.dwarfs_version = t.dwarfs_version().value();
  }

  if (t.options().has_value()) {
    e.options = from_thrift(t.options().value());
  }

  return e;
}

domain::metadata from_thrift(const dwarfs::thrift::metadata::metadata& t) {
  domain::metadata m;

  // Convert core collections
  if (t.chunks().has_value()) {
    const auto& thrift_chunks = t.chunks().value();
    m.chunks.reserve(thrift_chunks.size());
    for (const auto& tc : thrift_chunks) {
      m.chunks.push_back(from_thrift(tc));
    }
  }

  if (t.directories().has_value()) {
    const auto& thrift_dirs = t.directories().value();
    m.directories.reserve(thrift_dirs.size());
    for (const auto& td : thrift_dirs) {
      m.directories.push_back(from_thrift(td));
    }
  }

  if (t.inodes().has_value()) {
    const auto& thrift_inodes = t.inodes().value();
    m.inodes.reserve(thrift_inodes.size());
    for (const auto& ti : thrift_inodes) {
      m.inodes.push_back(from_thrift(ti));
    }
  }

  // Convert tables
  if (t.chunk_table().has_value()) {
    const auto& tt = t.chunk_table().value();
    m.chunk_table.assign(tt.begin(), tt.end());
  }
  if (t.entry_table_v2_2().has_value()) {
    const auto& tt = t.entry_table_v2_2().value();
    m.entry_table_v2_2.assign(tt.begin(), tt.end());
  }
  if (t.symlink_table().has_value()) {
    const auto& tt = t.symlink_table().value();
    m.symlink_table.assign(tt.begin(), tt.end());
  }
  if (t.uids().has_value()) {
    const auto& tt = t.uids().value();
    m.uids.assign(tt.begin(), tt.end());
  }
  if (t.gids().has_value()) {
    const auto& tt = t.gids().value();
    m.gids.assign(tt.begin(), tt.end());
  }
  if (t.modes().has_value()) {
    const auto& tt = t.modes().value();
    m.modes.assign(tt.begin(), tt.end());
  }
  if (t.names().has_value()) {
    const auto& tt = t.names().value();
    m.names.assign(tt.begin(), tt.end());
  }
  if (t.symlinks().has_value()) {
    const auto& tt = t.symlinks().value();
    m.symlinks.assign(tt.begin(), tt.end());
  }

  // Basic fields
  m.timestamp_base = t.timestamp_base().has_value() ? t.timestamp_base().value() : 0;
  m.block_size = t.block_size().has_value() ? t.block_size().value() : 0;
  m.total_fs_size = t.total_fs_size().has_value() ? t.total_fs_size().value() : 0;

  // Optional fields (v2.1+)
  if (t.devices().has_value()) {
    const auto& thrift_devices = t.devices().value();
    m.devices = std::vector<uint64_t>(thrift_devices.begin(), thrift_devices.end());
  }

  if (t.options().has_value()) {
    m.options = from_thrift(t.options().value());
  }

  // Optional fields (v2.3+)
  if (t.dir_entries().has_value()) {
    const auto& thrift_entries = t.dir_entries().value();
    std::vector<domain::dir_entry> entries;
    entries.reserve(thrift_entries.size());
    for (const auto& te : thrift_entries) {
      entries.push_back(from_thrift(te));
    }
    m.dir_entries = std::move(entries);
  }

  if (t.shared_files_table().has_value()) {
    const auto& tt = t.shared_files_table().value();
    m.shared_files_table = std::vector<uint32_t>(tt.begin(), tt.end());
  }

  if (t.total_hardlink_size().has_value()) {
    m.total_hardlink_size = t.total_hardlink_size().value();
  }

  if (t.dwarfs_version().has_value()) {
    m.dwarfs_version = t.dwarfs_version().value();
  }

  if (t.create_timestamp().has_value()) {
    m.create_timestamp = t.create_timestamp().value();
  }

  if (t.compact_names().has_value()) {
    m.compact_names = from_thrift(t.compact_names().value());
  }

  if (t.compact_symlinks().has_value()) {
    m.compact_symlinks = from_thrift(t.compact_symlinks().value());
  }

  // Optional fields (v2.5+)
  if (t.preferred_path_separator().has_value()) {
    m.preferred_path_separator = t.preferred_path_separator().value();
  }

  if (t.features().has_value()) {
    const auto& thrift_features = t.features().value();
    m.features = std::set<std::string>(thrift_features.begin(), thrift_features.end());
  }

  if (t.category_names().has_value()) {
    const auto& tt = t.category_names().value();
    m.category_names = std::vector<std::string>(tt.begin(), tt.end());
  }

  if (t.block_categories().has_value()) {
    const auto& tt = t.block_categories().value();
    m.block_categories = std::vector<uint32_t>(tt.begin(), tt.end());
  }

  if (t.reg_file_size_cache().has_value()) {
    m.reg_file_size_cache = from_thrift(t.reg_file_size_cache().value());
  }

  if (t.category_metadata_json().has_value()) {
    const auto& tt = t.category_metadata_json().value();
    m.category_metadata_json = std::vector<std::string>(tt.begin(), tt.end());
  }

  if (t.block_category_metadata().has_value()) {
    const auto& thrift_map = t.block_category_metadata().value();
    m.block_category_metadata = std::map<uint32_t, uint32_t>(
      thrift_map.begin(), thrift_map.end());
  }

  if (t.metadata_version_history().has_value()) {
    const auto& thrift_history = t.metadata_version_history().value();
    std::vector<domain::history_entry> history;
    history.reserve(thrift_history.size());
    for (const auto& te : thrift_history) {
      history.push_back(from_thrift(te));
    }
    m.metadata_version_history = std::move(history);
  }

  if (t.hole_block_index().has_value()) {
    m.hole_block_index = t.hole_block_index().value();
  }

  if (t.large_hole_size().has_value()) {
    const auto& tt = t.large_hole_size().value();
    m.large_hole_size = std::vector<uint64_t>(tt.begin(), tt.end());
  }

  if (t.total_allocated_fs_size().has_value()) {
    m.total_allocated_fs_size = t.total_allocated_fs_size().value();
  }

  return m;
}

// ============================================================================
// Domain → Thrift Conversions
// ============================================================================

dwarfs::thrift::metadata::chunk to_thrift(const domain::chunk& c) {
  dwarfs::thrift::metadata::chunk t;
  t.block() = c.block();
  t.offset() = c.offset();
  t.size() = c.size();
  return t;
}

dwarfs::thrift::metadata::directory to_thrift(const domain::directory& d) {
  dwarfs::thrift::metadata::directory t;
  t.parent_entry() = d.parent_entry();
  t.first_entry() = d.first_entry();
  t.self_entry() = d.self_entry();
  return t;
}

dwarfs::thrift::metadata::inode_data to_thrift(const domain::inode_data& i) {
  dwarfs::thrift::metadata::inode_data t;

  // Core fields
  t.mode_index() = i.mode_index;
  t.owner_index() = i.owner_index;
  t.group_index() = i.group_index;
  t.atime_offset() = i.atime_offset;
  t.mtime_offset() = i.mtime_offset;
  t.ctime_offset() = i.ctime_offset;

  // Extended fields - only set if non-zero
  if (i.btime_offset != 0) {
    t.btime_offset() = i.btime_offset;
  }
  if (i.atime_subsec != 0) {
    t.atime_subsec() = i.atime_subsec;
  }
  if (i.mtime_subsec != 0) {
    t.mtime_subsec() = i.mtime_subsec;
  }
  if (i.ctime_subsec != 0) {
    t.ctime_subsec() = i.ctime_subsec;
  }
  if (i.btime_subsec != 0) {
    t.btime_subsec() = i.btime_subsec;
  }
  if (i.nlink_minus_one != 0) {
    t.nlink_minus_one() = i.nlink_minus_one;
  }

  // Deprecated fields
  if (i.name_index_v2_2.has_value()) {
    t.name_index_v2_2() = i.name_index_v2_2.value();
  }
  if (i.inode_v2_2.has_value()) {
    t.inode_v2_2() = i.inode_v2_2.value();
  }

  return t;
}

dwarfs::thrift::metadata::dir_entry to_thrift(const domain::dir_entry& e) {
  dwarfs::thrift::metadata::dir_entry t;
  t.name_index() = e.name_index();
  t.inode_num() = e.inode_num();
  return t;
}

dwarfs::thrift::metadata::fs_options to_thrift(const domain::fs_options& opts) {
  dwarfs::thrift::metadata::fs_options t;

  t.mtime_only() = opts.mtime_only;
  t.packed_chunk_table() = opts.packed_chunk_table;
  t.packed_directories() = opts.packed_directories;
  t.packed_shared_files_table() = opts.packed_shared_files_table;
  t.has_btime() = opts.has_btime;
  t.inodes_have_nlink() = opts.inodes_have_nlink;

  if (opts.time_resolution_sec.has_value()) {
    t.time_resolution_sec() = opts.time_resolution_sec.value();
  }

  if (opts.subsecond_resolution_nsec_multiplier.has_value()) {
    t.subsecond_resolution_nsec_multiplier() =
      opts.subsecond_resolution_nsec_multiplier.value();
  }

  return t;
}

dwarfs::thrift::metadata::string_table to_thrift(const domain::string_table& st) {
  dwarfs::thrift::metadata::string_table t;

  t.buffer() = st.buffer;
  t.index() = st.index;
  t.packed_index() = st.packed_index;

  if (st.symtab.has_value()) {
    t.symtab() = st.symtab.value();
  }

  return t;
}

dwarfs::thrift::metadata::inode_size_cache to_thrift(const domain::inode_size_cache& cache) {
  dwarfs::thrift::metadata::inode_size_cache t;

  t.size_lookup() = cache.size_lookup;
  t.min_chunk_count() = cache.min_chunk_count;

  if (!cache.allocated_size_lookup.empty()) {
    t.allocated_size_lookup() = cache.allocated_size_lookup;
  }

  return t;
}

dwarfs::thrift::metadata::history_entry to_thrift(const domain::history_entry& e) {
  dwarfs::thrift::metadata::history_entry t;

  t.major() = e.major;
  t.minor() = e.minor;
  t.block_size() = e.block_size;

  if (e.dwarfs_version.has_value()) {
    t.dwarfs_version() = e.dwarfs_version.value();
  }

  if (e.options.has_value()) {
    t.options() = to_thrift(e.options.value());
  }

  return t;
}

dwarfs::thrift::metadata::metadata to_thrift(const domain::metadata& m) {
  dwarfs::thrift::metadata::metadata t;

  // Convert core collections
  std::vector<dwarfs::thrift::metadata::chunk> thrift_chunks;
  thrift_chunks.reserve(m.chunks.size());
  for (const auto& c : m.chunks) {
    thrift_chunks.push_back(to_thrift(c));
  }
  t.chunks() = std::move(thrift_chunks);

  std::vector<dwarfs::thrift::metadata::directory> thrift_dirs;
  thrift_dirs.reserve(m.directories.size());
  for (const auto& d : m.directories) {
    thrift_dirs.push_back(to_thrift(d));
  }
  t.directories() = std::move(thrift_dirs);

  std::vector<dwarfs::thrift::metadata::inode_data> thrift_inodes;
  thrift_inodes.reserve(m.inodes.size());
  for (const auto& i : m.inodes) {
    thrift_inodes.push_back(to_thrift(i));
  }
  t.inodes() = std::move(thrift_inodes);

  // Convert tables
  t.chunk_table() = m.chunk_table;
  t.entry_table_v2_2() = m.entry_table_v2_2;
  t.symlink_table() = m.symlink_table;
  t.uids() = m.uids;
  t.gids() = m.gids;
  t.modes() = m.modes;
  t.names() = m.names;
  t.symlinks() = m.symlinks;

  // Basic fields
  t.timestamp_base() = m.timestamp_base;
  t.block_size() = m.block_size;
  t.total_fs_size() = m.total_fs_size;

  // Optional fields (v2.1+)
  if (m.devices.has_value()) {
    t.devices() = m.devices.value();
  }

  if (m.options.has_value()) {
    t.options() = to_thrift(m.options.value());
  }

  // Optional fields (v2.3+)
  if (m.dir_entries.has_value()) {
    std::vector<dwarfs::thrift::metadata::dir_entry> thrift_entries;
    thrift_entries.reserve(m.dir_entries->size());
    for (const auto& e : m.dir_entries.value()) {
      thrift_entries.push_back(to_thrift(e));
    }
    t.dir_entries() = std::move(thrift_entries);
  }

  if (m.shared_files_table.has_value()) {
    t.shared_files_table() = m.shared_files_table.value();
  }

  if (m.total_hardlink_size.has_value()) {
    t.total_hardlink_size() = m.total_hardlink_size.value();
  }

  if (m.dwarfs_version.has_value()) {
    t.dwarfs_version() = m.dwarfs_version.value();
  }

  if (m.create_timestamp.has_value()) {
    t.create_timestamp() = m.create_timestamp.value();
  }

  if (m.compact_names.has_value()) {
    t.compact_names() = to_thrift(m.compact_names.value());
  }

  if (m.compact_symlinks.has_value()) {
    t.compact_symlinks() = to_thrift(m.compact_symlinks.value());
  }

  // Optional fields (v2.5+)
  if (m.preferred_path_separator.has_value()) {
    t.preferred_path_separator() = m.preferred_path_separator.value();
  }

  if (m.features.has_value()) {
    t.features() = m.features.value();
  }

  if (m.category_names.has_value()) {
    t.category_names() = m.category_names.value();
  }

  if (m.block_categories.has_value()) {
    t.block_categories() = m.block_categories.value();
  }

  if (m.reg_file_size_cache.has_value()) {
    t.reg_file_size_cache() = to_thrift(m.reg_file_size_cache.value());
  }

  if (m.category_metadata_json.has_value()) {
    t.category_metadata_json() = m.category_metadata_json.value();
  }

  if (m.block_category_metadata.has_value()) {
    t.block_category_metadata() = m.block_category_metadata.value();
  }

  if (m.metadata_version_history.has_value()) {
    std::vector<dwarfs::thrift::metadata::history_entry> thrift_history;
    thrift_history.reserve(m.metadata_version_history->size());
    for (const auto& e : m.metadata_version_history.value()) {
      thrift_history.push_back(to_thrift(e));
    }
    t.metadata_version_history() = std::move(thrift_history);
  }

  if (m.hole_block_index.has_value()) {
    t.hole_block_index() = m.hole_block_index.value();
  }

  if (m.large_hole_size.has_value()) {
    t.large_hole_size() = m.large_hole_size.value();
  }

  if (m.total_allocated_fs_size.has_value()) {
    t.total_allocated_fs_size() = m.total_allocated_fs_size.value();
  }

  return t;
}

} // namespace dwarfs::metadata::converters