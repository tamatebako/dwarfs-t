/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file cpp_thrift_converter.cpp
 *
 * Implementation of bidirectional conversion between Thrift and plain C++
 * metadata structures.
 *
 * This file implements the adapter pattern for seamless conversion between
 * Apache Thrift metadata structures and plain C++ equivalents, enabling
 * backward compatibility with existing frozen format images while supporting
 * a modern serialization backend (FlatBuffers).
 *
 * \author Ribose
 * \date 2025-11-12
 * \copyright See LICENSE file
 */

#include "dwarfs/internal/cpp_thrift_converter.h"

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT

namespace dwarfs::internal {

// ============================================================================
// Thrift → C++ Conversions (from_thrift)
// ============================================================================

chunk from_thrift(const thrift::metadata::chunk& t) {
  chunk c;
  c.block = t.block().has_value() ? t.block().value() : 0;
  c.offset = t.offset().has_value() ? t.offset().value() : 0;
  c.size = t.size().has_value() ? t.size().value() : 0;
  return c;
}

directory from_thrift(const thrift::metadata::directory& t) {
  directory d;
  d.parent_entry = t.parent_entry().has_value() ? t.parent_entry().value() : 0;
  d.first_entry = t.first_entry().has_value() ? t.first_entry().value() : 0;
  d.self_entry = t.self_entry().has_value() ? t.self_entry().value() : 0;
  return d;
}

inode_data from_thrift(const thrift::metadata::inode_data& t) {
  inode_data i;

  // Required core fields (v2.0+)
  i.mode_index = t.mode_index().has_value() ? t.mode_index().value() : 0;
  i.owner_index = t.owner_index().has_value() ? t.owner_index().value() : 0;
  i.group_index = t.group_index().has_value() ? t.group_index().value() : 0;
  i.atime_offset = t.atime_offset().has_value() ? t.atime_offset().value() : 0;
  i.mtime_offset = t.mtime_offset().has_value() ? t.mtime_offset().value() : 0;
  i.ctime_offset = t.ctime_offset().has_value() ? t.ctime_offset().value() : 0;

  // Extended fields (v2.5+) - initialize to 0 if not present
  i.btime_offset = t.btime_offset().has_value() ? t.btime_offset().value() : 0;
  i.atime_subsec = t.atime_subsec().has_value() ? t.atime_subsec().value() : 0;
  i.mtime_subsec = t.mtime_subsec().has_value() ? t.mtime_subsec().value() : 0;
  i.ctime_subsec = t.ctime_subsec().has_value() ? t.ctime_subsec().value() : 0;
  i.btime_subsec = t.btime_subsec().has_value() ? t.btime_subsec().value() : 0;
  i.nlink_minus_one = t.nlink_minus_one().has_value() ? t.nlink_minus_one().value() : 0;

  // Deprecated fields (v2.2) - initialize to 0 if not present
  i.name_index_v2_2 = t.name_index_v2_2().has_value() ? t.name_index_v2_2().value() : 0;
  i.inode_v2_2 = t.inode_v2_2().has_value() ? t.inode_v2_2().value() : 0;

  return i;
}

dir_entry from_thrift(const thrift::metadata::dir_entry& t) {
  dir_entry e;
  e.name_index = t.name_index().has_value() ? t.name_index().value() : 0;
  e.inode_num = t.inode_num().has_value() ? t.inode_num().value() : 0;
  return e;
}

fs_options from_thrift(const thrift::metadata::fs_options& t) {
  fs_options opts;

  // Required boolean fields
  opts.mtime_only = t.mtime_only().has_value() ? t.mtime_only().value() : false;
  opts.packed_chunk_table = t.packed_chunk_table().has_value() ? t.packed_chunk_table().value() : false;
  opts.packed_directories = t.packed_directories().has_value() ? t.packed_directories().value() : false;
  opts.packed_shared_files_table = t.packed_shared_files_table().has_value() ? t.packed_shared_files_table().value() : false;
  opts.has_btime = t.has_btime().has_value() ? t.has_btime().value() : false;
  opts.inodes_have_nlink = t.inodes_have_nlink().has_value() ? t.inodes_have_nlink().value() : false;

  // Optional fields
  if (t.time_resolution_sec().has_value()) {
    opts.time_resolution_sec = t.time_resolution_sec().value();
  }

  if (t.subsecond_resolution_nsec_multiplier().has_value()) {
    opts.subsecond_resolution_nsec_multiplier =
      t.subsecond_resolution_nsec_multiplier().value();
  }

  return opts;
}

string_table from_thrift(const thrift::metadata::string_table& t) {
  string_table st;

  // Required fields
  st.buffer = t.buffer().has_value() ? t.buffer().value() : std::string();
  st.packed_index = t.packed_index().has_value() ? t.packed_index().value() : false;

  // Convert index vector
  if (t.index().has_value()) {
    const auto& thrift_index = t.index().value();
    st.index.assign(thrift_index.begin(), thrift_index.end());
  }

  // Optional FSST symbol table
  if (t.symtab().has_value()) {
    st.symtab = t.symtab().value();
  }

  return st;
}

inode_size_cache from_thrift(const thrift::metadata::inode_size_cache& t) {
  inode_size_cache cache;

  // Required fields
  if (t.size_lookup().has_value()) {
    const auto& thrift_map = t.size_lookup().value();
    cache.size_lookup.insert(thrift_map.begin(), thrift_map.end());
  }
  cache.min_chunk_count = t.min_chunk_count().has_value() ? t.min_chunk_count().value() : 0;

  // Optional allocated size lookup (v2.5+ for sparse files)
  if (t.allocated_size_lookup().has_value()) {
    const auto& thrift_map = t.allocated_size_lookup().value();
    cache.allocated_size_lookup.insert(thrift_map.begin(), thrift_map.end());
  }

  return cache;
}

history_entry from_thrift(const thrift::metadata::history_entry& t) {
  history_entry e;

  e.major = t.major().has_value() ? t.major().value() : 0;
  e.minor = t.minor().has_value() ? t.minor().value() : 0;
  e.block_size = t.block_size().has_value() ? t.block_size().value() : 0;

  // Optional fields
  if (t.dwarfs_version().has_value()) {
    e.dwarfs_version = t.dwarfs_version().value();
  }

  if (t.options().has_value()) {
    e.options = from_thrift(t.options().value());
  }

  return e;
}

dwarfs_version from_thrift(const thrift::history::dwarfs_version& t) {
  dwarfs_version v;

  v.major = t.major().has_value() ? t.major().value() : 0;
  v.minor = t.minor().has_value() ? t.minor().value() : 0;
  v.patch = t.patch().has_value() ? t.patch().value() : 0;
  v.is_release = t.is_release().has_value() ? t.is_release().value() : false;

  // Optional Git metadata
  if (t.git_rev().has_value()) {
    v.git_rev = t.git_rev().value();
  }
  if (t.git_branch().has_value()) {
    v.git_branch = t.git_branch().value();
  }
  if (t.git_desc().has_value()) {
    v.git_desc = t.git_desc().value();
  }

  return v;
}

history_entry_full from_thrift(const thrift::history::history_entry& t) {
  history_entry_full e;

  // Required fields
  if (t.version().has_value()) {
    e.version = from_thrift(t.version().value());
  }
  e.system_id = t.system_id().has_value() ? t.system_id().value() : std::string();
  e.compiler_id = t.compiler_id().has_value() ? t.compiler_id().value() : std::string();

  // Optional fields
  if (t.arguments().has_value()) {
    const auto& thrift_args = t.arguments().value();
    e.arguments = std::vector<std::string>(thrift_args.begin(), thrift_args.end());
  }

  if (t.timestamp().has_value()) {
    e.timestamp = t.timestamp().value();
  }

  if (t.library_versions().has_value()) {
    const auto& thrift_libs = t.library_versions().value();
    e.library_versions = std::unordered_set<std::string>(
      thrift_libs.begin(), thrift_libs.end());
  }

  return e;
}

history from_thrift(const thrift::history::history& t) {
  history h;

  if (t.entries().has_value()) {
    const auto& thrift_entries = t.entries().value();
    h.entries.reserve(thrift_entries.size());
    for (const auto& te : thrift_entries) {
      h.entries.push_back(from_thrift(te));
    }
  }

  return h;
}

flac_block_header from_thrift(const thrift::compression::flac_block_header& t) {
  flac_block_header h;
  h.num_channels = t.num_channels().has_value() ? t.num_channels().value() : 0;
  h.bits_per_sample = t.bits_per_sample().has_value() ? t.bits_per_sample().value() : 0;
  h.flags = t.flags().has_value() ? t.flags().value() : 0;
  return h;
}

ricepp_block_header from_thrift(const thrift::compression::ricepp_block_header& t) {
  ricepp_block_header h;
  h.block_size = t.block_size().has_value() ? t.block_size().value() : 0;
  h.component_count = t.component_count().has_value() ? t.component_count().value() : 0;
  h.bytes_per_sample = t.bytes_per_sample().has_value() ? t.bytes_per_sample().value() : 0;
  h.unused_lsb_count = t.unused_lsb_count().has_value() ? t.unused_lsb_count().value() : 0;
  h.big_endian = t.big_endian().has_value() ? t.big_endian().value() : false;
  h.ricepp_version = t.ricepp_version().has_value() ? t.ricepp_version().value() : 0;
  return h;
}

metadata from_thrift(const thrift::metadata::metadata& t) {
  metadata m;

  // ── Core collections (v2.0+) ──────────────────────────────────────────

  // Convert chunks
  if (t.chunks().has_value()) {
    const auto& thrift_chunks = t.chunks().value();
    m.chunks.reserve(thrift_chunks.size());
    for (const auto& tc : thrift_chunks) {
      m.chunks.push_back(from_thrift(tc));
    }
  }

  // Convert directories
  if (t.directories().has_value()) {
    const auto& thrift_dirs = t.directories().value();
    m.directories.reserve(thrift_dirs.size());
    for (const auto& td : thrift_dirs) {
      m.directories.push_back(from_thrift(td));
    }
  }

  // Convert inodes
  if (t.inodes().has_value()) {
    const auto& thrift_inodes = t.inodes().value();
    m.inodes.reserve(thrift_inodes.size());
    for (const auto& ti : thrift_inodes) {
      m.inodes.push_back(from_thrift(ti));
    }
  }

  // Convert lookup tables
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

  // Basic scalar fields
  m.timestamp_base = t.timestamp_base().has_value() ? t.timestamp_base().value() : 0;
  m.block_size = t.block_size().has_value() ? t.block_size().value() : 0;
  m.total_fs_size = t.total_fs_size().has_value() ? t.total_fs_size().value() : 0;

  // ── Version 2.1+ fields ───────────────────────────────────────────────

  if (t.devices().has_value()) {
    const auto& thrift_devices = t.devices().value();
    m.devices = std::vector<uint64_t>(thrift_devices.begin(), thrift_devices.end());
  }

  if (t.options().has_value()) {
    m.options = from_thrift(t.options().value());
  }

  // ── Version 2.3+ fields ───────────────────────────────────────────────

  if (t.dir_entries().has_value()) {
    const auto& thrift_entries = t.dir_entries().value();
    std::vector<dir_entry> entries;
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

  // ── Version 2.5+ fields ───────────────────────────────────────────────

  if (t.preferred_path_separator().has_value()) {
    m.preferred_path_separator = t.preferred_path_separator().value();
  }

  if (t.features().has_value()) {
    const auto& thrift_features = t.features().value();
    m.features = std::unordered_set<std::string>(
      thrift_features.begin(), thrift_features.end());
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
    m.block_category_metadata = std::unordered_map<uint32_t, uint32_t>(
      thrift_map.begin(), thrift_map.end());
  }

  if (t.metadata_version_history().has_value()) {
    const auto& thrift_history = t.metadata_version_history().value();
    std::vector<history_entry> history;
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
// C++ → Thrift Conversions (to_thrift)
// ============================================================================

thrift::metadata::chunk to_thrift(const chunk& c) {
  thrift::metadata::chunk t;
  t.block() = c.block;
  t.offset() = c.offset;
  t.size() = c.size;
  return t;
}

thrift::metadata::directory to_thrift(const directory& d) {
  thrift::metadata::directory t;
  t.parent_entry() = d.parent_entry;
  t.first_entry() = d.first_entry;
  if (d.self_entry != 0) {
    t.self_entry() = d.self_entry;
  }
  return t;
}

thrift::metadata::inode_data to_thrift(const inode_data& i) {
  thrift::metadata::inode_data t;

  // Required core fields
  t.mode_index() = i.mode_index;
  t.owner_index() = i.owner_index;
  t.group_index() = i.group_index;
  t.atime_offset() = i.atime_offset;
  t.mtime_offset() = i.mtime_offset;
  t.ctime_offset() = i.ctime_offset;

  // Extended fields (v2.5+) - only set if non-zero
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

  // Deprecated fields (v2.2) - only set if non-zero
  if (i.name_index_v2_2 != 0) {
    t.name_index_v2_2() = i.name_index_v2_2;
  }
  if (i.inode_v2_2 != 0) {
    t.inode_v2_2() = i.inode_v2_2;
  }

  return t;
}

thrift::metadata::dir_entry to_thrift(const dir_entry& e) {
  thrift::metadata::dir_entry t;
  t.name_index() = e.name_index;
  t.inode_num() = e.inode_num;
  return t;
}

thrift::metadata::fs_options to_thrift(const fs_options& opts) {
  thrift::metadata::fs_options t;

  // Required boolean fields
  t.mtime_only() = opts.mtime_only;
  t.packed_chunk_table() = opts.packed_chunk_table;
  t.packed_directories() = opts.packed_directories;
  t.packed_shared_files_table() = opts.packed_shared_files_table;
  t.has_btime() = opts.has_btime;
  t.inodes_have_nlink() = opts.inodes_have_nlink;

  // Optional fields
  if (opts.time_resolution_sec.has_value()) {
    t.time_resolution_sec() = opts.time_resolution_sec.value();
  }

  if (opts.subsecond_resolution_nsec_multiplier.has_value()) {
    t.subsecond_resolution_nsec_multiplier() =
      opts.subsecond_resolution_nsec_multiplier.value();
  }

  return t;
}

thrift::metadata::string_table to_thrift(const string_table& st) {
  thrift::metadata::string_table t;

  // Required fields
  t.buffer() = st.buffer;
  t.packed_index() = st.packed_index;

  // Index vector: only set if non-empty to preserve optional semantics
  if (!st.index.empty()) {
    t.index() = st.index;
  }

  // Optional FSST symbol table
  if (st.symtab.has_value()) {
    t.symtab() = st.symtab.value();
  }

  return t;
}

thrift::metadata::inode_size_cache to_thrift(const inode_size_cache& cache) {
  thrift::metadata::inode_size_cache t;

  // Required fields - convert unordered_map to map for Thrift
  std::map<uint32_t, uint64_t> size_lookup_map(
    cache.size_lookup.begin(), cache.size_lookup.end());
  t.size_lookup() = std::move(size_lookup_map);
  t.min_chunk_count() = cache.min_chunk_count;

  // Optional allocated size lookup (v2.5+ for sparse files)
  if (!cache.allocated_size_lookup.empty()) {
    std::map<uint32_t, uint64_t> allocated_map(
      cache.allocated_size_lookup.begin(), cache.allocated_size_lookup.end());
    t.allocated_size_lookup() = std::move(allocated_map);
  }

  return t;
}

thrift::metadata::history_entry to_thrift(const history_entry& e) {
  thrift::metadata::history_entry t;

  t.major() = e.major;
  t.minor() = e.minor;
  t.block_size() = e.block_size;

  // Optional fields
  if (e.dwarfs_version.has_value()) {
    t.dwarfs_version() = e.dwarfs_version.value();
  }

  if (e.options.has_value()) {
    t.options() = to_thrift(e.options.value());
  }

  return t;
}

thrift::history::dwarfs_version to_thrift(const dwarfs_version& v) {
  thrift::history::dwarfs_version t;

  t.major() = v.major;
  t.minor() = v.minor;
  t.patch() = v.patch;
  t.is_release() = v.is_release;

  // Optional Git metadata
  if (v.git_rev.has_value()) {
    t.git_rev() = v.git_rev.value();
  }
  if (v.git_branch.has_value()) {
    t.git_branch() = v.git_branch.value();
  }
  if (v.git_desc.has_value()) {
    t.git_desc() = v.git_desc.value();
  }

  return t;
}

thrift::history::history_entry to_thrift(const history_entry_full& e) {
  thrift::history::history_entry t;

  // Required fields
  t.version() = to_thrift(e.version);
  t.system_id() = e.system_id;
  t.compiler_id() = e.compiler_id;

  // Optional fields
  if (e.arguments.has_value()) {
    t.arguments() = e.arguments.value();
  }

  if (e.timestamp.has_value()) {
    t.timestamp() = e.timestamp.value();
  }

  if (e.library_versions.has_value()) {
    std::set<std::string> lib_set(
      e.library_versions->begin(), e.library_versions->end());
    t.library_versions() = std::move(lib_set);
  }

  return t;
}

thrift::history::history to_thrift(const history& h) {
  thrift::history::history t;

  std::vector<thrift::history::history_entry> thrift_entries;
  thrift_entries.reserve(h.entries.size());
  for (const auto& e : h.entries) {
    thrift_entries.push_back(to_thrift(e));
  }
  t.entries() = std::move(thrift_entries);

  return t;
}

thrift::compression::flac_block_header to_thrift(const flac_block_header& h) {
  thrift::compression::flac_block_header t;
  t.num_channels() = h.num_channels;
  t.bits_per_sample() = h.bits_per_sample;
  t.flags() = h.flags;
  return t;
}

thrift::compression::ricepp_block_header to_thrift(const ricepp_block_header& h) {
  thrift::compression::ricepp_block_header t;
  t.block_size() = h.block_size;
  t.component_count() = h.component_count;
  t.bytes_per_sample() = h.bytes_per_sample;
  t.unused_lsb_count() = h.unused_lsb_count;
  t.big_endian() = h.big_endian;
  t.ricepp_version() = h.ricepp_version;
  return t;
}

thrift::metadata::metadata to_thrift(const metadata& m) {
  thrift::metadata::metadata t;

  // ── Core collections (v2.0+) ──────────────────────────────────────────

  // Convert chunks
  std::vector<thrift::metadata::chunk> thrift_chunks;
  thrift_chunks.reserve(m.chunks.size());
  for (const auto& c : m.chunks) {
    thrift_chunks.push_back(to_thrift(c));
  }
  t.chunks() = std::move(thrift_chunks);

  // Convert directories
  std::vector<thrift::metadata::directory> thrift_dirs;
  thrift_dirs.reserve(m.directories.size());
  for (const auto& d : m.directories) {
    thrift_dirs.push_back(to_thrift(d));
  }
  t.directories() = std::move(thrift_dirs);

  // Convert inodes
  std::vector<thrift::metadata::inode_data> thrift_inodes;
  thrift_inodes.reserve(m.inodes.size());
  for (const auto& i : m.inodes) {
    thrift_inodes.push_back(to_thrift(i));
  }
  t.inodes() = std::move(thrift_inodes);

  // Convert lookup tables
  t.chunk_table() = m.chunk_table;
  t.entry_table_v2_2() = m.entry_table_v2_2;
  t.symlink_table() = m.symlink_table;
  t.uids() = m.uids;
  t.gids() = m.gids;
  t.modes() = m.modes;
  t.names() = m.names;
  t.symlinks() = m.symlinks;

  // Basic scalar fields
  t.timestamp_base() = m.timestamp_base;
  t.block_size() = m.block_size;
  t.total_fs_size() = m.total_fs_size;

  // ── Version 2.1+ fields ───────────────────────────────────────────────

  if (m.devices.has_value()) {
    t.devices() = m.devices.value();
  }

  if (m.options.has_value()) {
    t.options() = to_thrift(m.options.value());
  }

  // ── Version 2.3+ fields ───────────────────────────────────────────────

  if (m.dir_entries.has_value()) {
    std::vector<thrift::metadata::dir_entry> thrift_entries;
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

  // ── Version 2.5+ fields ───────────────────────────────────────────────

  if (m.preferred_path_separator.has_value()) {
    t.preferred_path_separator() = m.preferred_path_separator.value();
  }

  if (m.features.has_value()) {
    std::set<std::string> features_set(
      m.features->begin(), m.features->end());
    t.features() = std::move(features_set);
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
    std::map<uint32_t, uint32_t> category_map(
      m.block_category_metadata->begin(), m.block_category_metadata->end());
    t.block_category_metadata() = std::move(category_map);
  }

  if (m.metadata_version_history.has_value()) {
    std::vector<thrift::metadata::history_entry> thrift_history;
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

} // namespace dwarfs::internal

#endif // DWARFS_HAVE_EXPERIMENTAL_THRIFT