/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file domain_flatbuffers_converter.cpp
 *
 * Implementation of bidirectional conversion between FlatBuffers and domain model.
 *
 * FlatBuffers support is always enabled as the default metadata format.
 *
 * \author Ribose Inc.
 * \date 2025-12-22
 * \copyright See LICENSE file
 */

#include <flatbuffers/flatbuffers.h>
#include <iostream>
#include "dwarfs/metadata/converters/domain_flatbuffers_converter.h"
#include <dwarfs/gen-flatbuffers/metadata.h>

namespace dwarfs::metadata::converters {

// ============================================================================
// FlatBuffers → Domain Conversions
// ============================================================================

domain::chunk from_flatbuffers(const dwarfs::flatbuffers::Chunk& fb) {
  return domain::chunk(
    fb.block(),
    fb.offset(),
    fb.size()
  );
}

domain::directory from_flatbuffers(const dwarfs::flatbuffers::Directory& fb) {
  return domain::directory(
    fb.parent_entry(),
    fb.first_entry(),
    fb.self_entry()
  );
}

domain::inode_data from_flatbuffers(const dwarfs::flatbuffers::InodeData& fb) {
  domain::inode_data i;

  // Core fields
  i.mode_index = fb.mode_index();
  i.owner_index = fb.owner_index();
  i.group_index = fb.group_index();
  i.atime_offset = fb.atime_offset();
  i.mtime_offset = fb.mtime_offset();
  i.ctime_offset = fb.ctime_offset();

  // Optional v2.5+ fields (FlatBuffers uses 0 as default)
  if (fb.btime_offset() != 0) {
    i.btime_offset = fb.btime_offset();
  }
  if (fb.atime_subsec() != 0) {
    i.atime_subsec = fb.atime_subsec();
  }
  if (fb.mtime_subsec() != 0) {
    i.mtime_subsec = fb.mtime_subsec();
  }
  if (fb.ctime_subsec() != 0) {
    i.ctime_subsec = fb.ctime_subsec();
  }
  if (fb.btime_subsec() != 0) {
    i.btime_subsec = fb.btime_subsec();
  }
  if (fb.nlink_minus_one() != 0) {
    i.nlink_minus_one = fb.nlink_minus_one();
  }

  // Deprecated v2.2 fields
  if (fb.name_index_v2_2() != 0) {
    i.name_index_v2_2 = fb.name_index_v2_2();
  }
  if (fb.inode_v2_2() != 0) {
    i.inode_v2_2 = fb.inode_v2_2();
  }

  return i;
}

domain::dir_entry from_flatbuffers(const dwarfs::flatbuffers::DirEntry& fb) {
  return domain::dir_entry(
    fb.name_index(),
    fb.inode_num()
  );
}

domain::fs_options from_flatbuffers(const dwarfs::flatbuffers::FsOptions& fb) {
  domain::fs_options opts;

  opts.mtime_only = fb.mtime_only();
  opts.packed_chunk_table = fb.packed_chunk_table();
  opts.packed_directories = fb.packed_directories();
  opts.packed_shared_files_table = fb.packed_shared_files_table();
  opts.has_btime = fb.has_btime();
  opts.inodes_have_nlink = fb.inodes_have_nlink();

  if (fb.time_resolution_sec() != 0) {
    opts.time_resolution_sec = fb.time_resolution_sec();
  }

  if (fb.subsecond_resolution_nsec_multiplier() != 0) {
    opts.subsecond_resolution_nsec_multiplier =
      fb.subsecond_resolution_nsec_multiplier();
  }

  return opts;
}

domain::string_table from_flatbuffers(const dwarfs::flatbuffers::StringTable& fb) {
  domain::string_table st;

  // Buffer (required) - Convert Vector<uint8_t> to std::string
  if (fb.buffer()) {
    st.buffer = std::string(
        reinterpret_cast<const char*>(fb.buffer()->data()),
        fb.buffer()->size());
  }

  // Packed index flag
  st.packed_index = fb.packed_index();

  // Index vector (optional)
  if (fb.index()) {
    st.index.reserve(fb.index()->size());
    for (uint32_t i = 0; i < fb.index()->size(); ++i) {
      st.index.push_back(fb.index()->Get(i));
    }
  }

  // FSST symbol table (optional) - Convert Vector<uint8_t> to std::string
  if (fb.symtab()) {
    st.symtab = std::string(
        reinterpret_cast<const char*>(fb.symtab()->data()),
        fb.symtab()->size());
  }

  return st;
}

domain::inode_size_cache from_flatbuffers(const dwarfs::flatbuffers::InodeSizeCache& fb) {
  domain::inode_size_cache cache;

  // Size lookup map
  if (fb.size_lookup_keys() && fb.size_lookup_values()) {
    auto keys = fb.size_lookup_keys();
    auto values = fb.size_lookup_values();
    for (uint32_t i = 0; i < keys->size(); ++i) {
      cache.size_lookup[keys->Get(i)] = values->Get(i);
    }
  }

  cache.min_chunk_count = fb.min_chunk_count();

  // Allocated size lookup map (v2.5+)
  if (fb.allocated_size_lookup_keys() && fb.allocated_size_lookup_values()) {
    auto keys = fb.allocated_size_lookup_keys();
    auto values = fb.allocated_size_lookup_values();
    for (uint32_t i = 0; i < keys->size(); ++i) {
      cache.allocated_size_lookup[keys->Get(i)] = values->Get(i);
    }
  }

  return cache;
}

domain::history_entry from_flatbuffers(const dwarfs::flatbuffers::HistoryEntry& fb) {
  domain::history_entry e;

  e.major = fb.major();
  e.minor = fb.minor();
  e.block_size = fb.block_size();

  if (fb.dwarfs_version()) {
    e.dwarfs_version = fb.dwarfs_version()->str();
  }

  if (fb.options()) {
    e.options = from_flatbuffers(*fb.options());
  }

  return e;
}

domain::metadata from_flatbuffers(const dwarfs::flatbuffers::Metadata& fb) {
  domain::metadata m;

  // Convert core collections
  if (fb.chunks()) {
    m.chunks.reserve(fb.chunks()->size());
    for (uint32_t i = 0; i < fb.chunks()->size(); ++i) {
      m.chunks.push_back(from_flatbuffers(*fb.chunks()->Get(i)));
    }
  }

  if (fb.directories()) {
    m.directories.reserve(fb.directories()->size());
    for (uint32_t i = 0; i < fb.directories()->size(); ++i) {
      m.directories.push_back(from_flatbuffers(*fb.directories()->Get(i)));
    }
  }

  if (fb.inodes()) {
    m.inodes.reserve(fb.inodes()->size());
    for (uint32_t i = 0; i < fb.inodes()->size(); ++i) {
      m.inodes.push_back(from_flatbuffers(*fb.inodes()->Get(i)));
    }
  }

  // Convert tables
  if (fb.chunk_table()) {
    m.chunk_table.assign(fb.chunk_table()->begin(), fb.chunk_table()->end());
  }
  if (fb.entry_table_v2_2()) {
    m.entry_table_v2_2.assign(fb.entry_table_v2_2()->begin(), fb.entry_table_v2_2()->end());
  }
  if (fb.symlink_table()) {
    m.symlink_table.assign(fb.symlink_table()->begin(), fb.symlink_table()->end());
  }
  if (fb.uids()) {
    m.uids.assign(fb.uids()->begin(), fb.uids()->end());
  }
  if (fb.gids()) {
    m.gids.assign(fb.gids()->begin(), fb.gids()->end());
  }
  if (fb.modes()) {
    m.modes.assign(fb.modes()->begin(), fb.modes()->end());
  }

  // String vectors
  if (fb.names()) {
    m.names.reserve(fb.names()->size());
    for (uint32_t i = 0; i < fb.names()->size(); ++i) {
      m.names.push_back(fb.names()->Get(i)->str());
    }
  }
  if (fb.symlinks()) {
    m.symlinks.reserve(fb.symlinks()->size());
    for (uint32_t i = 0; i < fb.symlinks()->size(); ++i) {
      m.symlinks.push_back(fb.symlinks()->Get(i)->str());
    }
  }

  // Basic fields
  m.timestamp_base = fb.timestamp_base();
  m.block_size = fb.block_size();
  m.total_fs_size = fb.total_fs_size();

  // Optional fields (v2.1+)
  if (fb.devices()) {
    m.devices = std::vector<uint64_t>(fb.devices()->begin(), fb.devices()->end());
  }

  if (fb.options()) {
    m.options = from_flatbuffers(*fb.options());
  }

  // Optional fields (v2.3+)
  if (fb.dir_entries()) {
    std::vector<domain::dir_entry> entries;
    entries.reserve(fb.dir_entries()->size());
    for (uint32_t i = 0; i < fb.dir_entries()->size(); ++i) {
      entries.push_back(from_flatbuffers(*fb.dir_entries()->Get(i)));
    }
    m.dir_entries = std::move(entries);
  }

  if (fb.shared_files_table()) {
    m.shared_files_table = std::vector<uint32_t>(
      fb.shared_files_table()->begin(),
      fb.shared_files_table()->end());
  }

  if (fb.total_hardlink_size() != 0) {
    m.total_hardlink_size = fb.total_hardlink_size();
  }

  if (fb.dwarfs_version()) {
    m.dwarfs_version = fb.dwarfs_version()->str();
  }

  if (fb.create_timestamp() != 0) {
    m.create_timestamp = fb.create_timestamp();
  }

  if (fb.compact_names()) {
    m.compact_names = from_flatbuffers(*fb.compact_names());
  }

  if (fb.compact_symlinks()) {
    m.compact_symlinks = from_flatbuffers(*fb.compact_symlinks());
  }

  // Optional fields (v2.5+)
  if (fb.preferred_path_separator() != 0) {
    m.preferred_path_separator = fb.preferred_path_separator();
  }

  if (fb.features()) {
    std::set<std::string> features;
    for (uint32_t i = 0; i < fb.features()->size(); ++i) {
      features.insert(fb.features()->Get(i)->str());
    }
    m.features = std::move(features);
  }

  if (fb.category_names()) {
    std::vector<std::string> names;
    names.reserve(fb.category_names()->size());
    for (uint32_t i = 0; i < fb.category_names()->size(); ++i) {
      names.push_back(fb.category_names()->Get(i)->str());
    }
    m.category_names = std::move(names);
  }

  if (fb.block_categories()) {
    m.block_categories = std::vector<uint32_t>(
      fb.block_categories()->begin(),
      fb.block_categories()->end());
  }

  if (fb.reg_file_size_cache()) {
    m.reg_file_size_cache = from_flatbuffers(*fb.reg_file_size_cache());
  }

  if (fb.category_metadata_json()) {
    std::vector<std::string> json;
    json.reserve(fb.category_metadata_json()->size());
    for (uint32_t i = 0; i < fb.category_metadata_json()->size(); ++i) {
      json.push_back(fb.category_metadata_json()->Get(i)->str());
    }
    m.category_metadata_json = std::move(json);
  }

  if (fb.block_category_metadata_keys() && fb.block_category_metadata_values()) {
    std::map<uint32_t, uint32_t> metadata;
    auto keys = fb.block_category_metadata_keys();
    auto values = fb.block_category_metadata_values();
    for (uint32_t i = 0; i < keys->size(); ++i) {
      metadata[keys->Get(i)] = values->Get(i);
    }
    m.block_category_metadata = std::move(metadata);
  }

  if (fb.metadata_version_history()) {
    std::vector<domain::history_entry> history;
    history.reserve(fb.metadata_version_history()->size());
    for (uint32_t i = 0; i < fb.metadata_version_history()->size(); ++i) {
      history.push_back(from_flatbuffers(*fb.metadata_version_history()->Get(i)));
    }
    m.metadata_version_history = std::move(history);
  }

  if (fb.hole_block_index() != 0) {
    m.hole_block_index = fb.hole_block_index();
  }

  if (fb.large_hole_size()) {
    m.large_hole_size = std::vector<uint64_t>(
      fb.large_hole_size()->begin(),
      fb.large_hole_size()->end());
  }

  if (fb.total_allocated_fs_size() != 0) {
    m.total_allocated_fs_size = fb.total_allocated_fs_size();
  }

  return m;
}

// ============================================================================
// Domain → FlatBuffers Conversions
// ============================================================================

::flatbuffers::Offset<dwarfs::flatbuffers::Chunk> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::chunk& c) {
  return dwarfs::flatbuffers::CreateChunk(
    builder,
    c.block(),
    c.offset(),
    c.size()
  );
}

::flatbuffers::Offset<dwarfs::flatbuffers::Directory> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::directory& d) {
  return dwarfs::flatbuffers::CreateDirectory(
    builder,
    d.parent_entry(),
    d.first_entry(),
    d.self_entry()
  );
}

::flatbuffers::Offset<dwarfs::flatbuffers::InodeData> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::inode_data& i) {
  return dwarfs::flatbuffers::CreateInodeData(
    builder,
    i.mode_index,
    i.owner_index,
    i.group_index,
    i.atime_offset,
    i.mtime_offset,
    i.ctime_offset,
    i.btime_offset,
    i.atime_subsec,
    i.mtime_subsec,
    i.ctime_subsec,
    i.btime_subsec,
    i.nlink_minus_one,
    i.name_index_v2_2.value_or(0),
    i.inode_v2_2.value_or(0)
  );
}

::flatbuffers::Offset<dwarfs::flatbuffers::DirEntry> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::dir_entry& e) {
  return dwarfs::flatbuffers::CreateDirEntry(
    builder,
    e.name_index(),
    e.inode_num()
  );
}

::flatbuffers::Offset<dwarfs::flatbuffers::FsOptions> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::fs_options& opts) {
  return dwarfs::flatbuffers::CreateFsOptions(
    builder,
    opts.mtime_only,
    opts.time_resolution_sec.value_or(0),
    opts.subsecond_resolution_nsec_multiplier.value_or(0),
    opts.packed_chunk_table,
    opts.packed_directories,
    opts.packed_shared_files_table,
    opts.has_btime,
    opts.inodes_have_nlink
  );
}

::flatbuffers::Offset<dwarfs::flatbuffers::StringTable> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::string_table& st) {
  // Use CreateVector for binary data (not CreateString)
  auto buffer_offset = builder.CreateVector(
      reinterpret_cast<const uint8_t*>(st.buffer.data()),
      st.buffer.size());

  ::flatbuffers::Offset<::flatbuffers::Vector<uint8_t>> symtab_offset = 0;
  if (st.symtab.has_value()) {
    symtab_offset = builder.CreateVector(
        reinterpret_cast<const uint8_t*>(st.symtab.value().data()),
        st.symtab.value().size());
  }

  auto index_offset = builder.CreateVector(st.index);

  return dwarfs::flatbuffers::CreateStringTable(
    builder,
    buffer_offset,
    symtab_offset,
    index_offset,
    st.packed_index
  );
}

::flatbuffers::Offset<dwarfs::flatbuffers::InodeSizeCache> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::inode_size_cache& cache) {
  // Convert map to parallel vectors
  std::vector<uint32_t> size_keys;
  std::vector<uint64_t> size_values;
  for (const auto& [k, v] : cache.size_lookup) {
    size_keys.push_back(k);
    size_values.push_back(v);
  }

  std::vector<uint32_t> alloc_keys;
  std::vector<uint64_t> alloc_values;
  for (const auto& [k, v] : cache.allocated_size_lookup) {
    alloc_keys.push_back(k);
    alloc_values.push_back(v);
  }

  auto size_keys_offset = builder.CreateVector(size_keys);
  auto size_values_offset = builder.CreateVector(size_values);
  auto alloc_keys_offset = builder.CreateVector(alloc_keys);
  auto alloc_values_offset = builder.CreateVector(alloc_values);

  return dwarfs::flatbuffers::CreateInodeSizeCache(
    builder,
    size_keys_offset,
    size_values_offset,
    alloc_keys_offset,
    alloc_values_offset,
    cache.min_chunk_count
  );
}

::flatbuffers::Offset<dwarfs::flatbuffers::HistoryEntry> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::history_entry& e) {
  ::flatbuffers::Offset<::flatbuffers::String> version_offset = 0;
  if (e.dwarfs_version.has_value()) {
    version_offset = builder.CreateString(e.dwarfs_version.value());
  }

  ::flatbuffers::Offset<dwarfs::flatbuffers::FsOptions> options_offset = 0;
  if (e.options.has_value()) {
    options_offset = to_flatbuffers(builder, e.options.value());
  }

  return dwarfs::flatbuffers::CreateHistoryEntry(
    builder,
    e.major,
    e.minor,
    version_offset,
    e.block_size,
    options_offset
  );
}

::flatbuffers::Offset<dwarfs::flatbuffers::Metadata> to_flatbuffers(
    ::flatbuffers::FlatBufferBuilder& builder,
    const domain::metadata& m) {
  // Convert chunks
  std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::Chunk>> chunk_offsets;
  chunk_offsets.reserve(m.chunks.size());
  for (const auto& c : m.chunks) {
    chunk_offsets.push_back(to_flatbuffers(builder, c));
  }
  auto chunks_offset = builder.CreateVector(chunk_offsets);

  // Convert directories
  std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::Directory>> dir_offsets;
  dir_offsets.reserve(m.directories.size());
  std::cerr << "[TO_FB_CONVERTER] Converting " << m.directories.size() << " directories" << std::endl;
  for (const auto& d : m.directories) {
    std::cerr << "[TO_FB_CONVERTER] directory: parent_entry=" << d.parent_entry()
              << ", first_entry=" << d.first_entry()
              << ", self_entry=" << d.self_entry() << std::endl;
    dir_offsets.push_back(to_flatbuffers(builder, d));
  }
  auto directories_offset = builder.CreateVector(dir_offsets);

  // Convert inodes
  std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::InodeData>> inode_offsets;
  inode_offsets.reserve(m.inodes.size());
  for (const auto& i : m.inodes) {
    inode_offsets.push_back(to_flatbuffers(builder, i));
  }
  auto inodes_offset = builder.CreateVector(inode_offsets);

  // Convert tables
  auto chunk_table_offset = builder.CreateVector(m.chunk_table);
  auto entry_table_v2_2_offset = builder.CreateVector(m.entry_table_v2_2);
  auto symlink_table_offset = builder.CreateVector(m.symlink_table);
  auto uids_offset = builder.CreateVector(m.uids);
  auto gids_offset = builder.CreateVector(m.gids);
  auto modes_offset = builder.CreateVector(m.modes);

  // Convert string vectors
  std::vector<::flatbuffers::Offset<::flatbuffers::String>> name_offsets;
  for (const auto& name : m.names) {
    name_offsets.push_back(builder.CreateString(name));
  }
  auto names_offset = builder.CreateVector(name_offsets);

  std::vector<::flatbuffers::Offset<::flatbuffers::String>> symlink_offsets;
  for (const auto& symlink : m.symlinks) {
    symlink_offsets.push_back(builder.CreateString(symlink));
  }
  auto symlinks_offset = builder.CreateVector(symlink_offsets);

  // Optional fields (v2.1+)
  ::flatbuffers::Offset<::flatbuffers::Vector<uint64_t>> devices_offset = 0;
  if (m.devices.has_value()) {
    devices_offset = builder.CreateVector(m.devices.value());
  }

  ::flatbuffers::Offset<dwarfs::flatbuffers::FsOptions> options_offset = 0;
  if (m.options.has_value()) {
    options_offset = to_flatbuffers(builder, m.options.value());
  }

  // Optional fields (v2.3+)
  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<dwarfs::flatbuffers::DirEntry>>> dir_entries_offset = 0;
  if (m.dir_entries.has_value()) {
    std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::DirEntry>> entry_offsets;
    entry_offsets.reserve(m.dir_entries->size());
    for (const auto& e : m.dir_entries.value()) {
      entry_offsets.push_back(to_flatbuffers(builder, e));
    }
    dir_entries_offset = builder.CreateVector(entry_offsets);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<uint32_t>> shared_files_offset = 0;
  if (m.shared_files_table.has_value()) {
    shared_files_offset = builder.CreateVector(m.shared_files_table.value());
  }

  ::flatbuffers::Offset<::flatbuffers::String> version_offset = 0;
  if (m.dwarfs_version.has_value()) {
    version_offset = builder.CreateString(m.dwarfs_version.value());
  }

  ::flatbuffers::Offset<dwarfs::flatbuffers::StringTable> compact_names_offset = 0;
  if (m.compact_names.has_value()) {
    compact_names_offset = to_flatbuffers(builder, m.compact_names.value());
  }

  ::flatbuffers::Offset<dwarfs::flatbuffers::StringTable> compact_symlinks_offset = 0;
  if (m.compact_symlinks.has_value()) {
    compact_symlinks_offset = to_flatbuffers(builder, m.compact_symlinks.value());
  }

  // Optional fields (v2.5+)
  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>>> features_offset = 0;
  if (m.features.has_value()) {
    std::vector<::flatbuffers::Offset<::flatbuffers::String>> feature_offsets;
    for (const auto& feature : m.features.value()) {
      feature_offsets.push_back(builder.CreateString(feature));
    }
    features_offset = builder.CreateVector(feature_offsets);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>>> category_names_offset = 0;
  if (m.category_names.has_value()) {
    std::vector<::flatbuffers::Offset<::flatbuffers::String>> name_offsets;
    for (const auto& name : m.category_names.value()) {
      name_offsets.push_back(builder.CreateString(name));
    }
    category_names_offset = builder.CreateVector(name_offsets);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<uint32_t>> block_categories_offset = 0;
  if (m.block_categories.has_value()) {
    block_categories_offset = builder.CreateVector(m.block_categories.value());
  }

  ::flatbuffers::Offset<dwarfs::flatbuffers::InodeSizeCache> reg_file_size_cache_offset = 0;
  if (m.reg_file_size_cache.has_value()) {
    reg_file_size_cache_offset = to_flatbuffers(builder, m.reg_file_size_cache.value());
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<::flatbuffers::String>>> category_metadata_json_offset = 0;
  if (m.category_metadata_json.has_value()) {
    std::vector<::flatbuffers::Offset<::flatbuffers::String>> json_offsets;
    for (const auto& json : m.category_metadata_json.value()) {
      json_offsets.push_back(builder.CreateString(json));
    }
    category_metadata_json_offset = builder.CreateVector(json_offsets);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<uint32_t>> block_category_metadata_keys_offset = 0;
  ::flatbuffers::Offset<::flatbuffers::Vector<uint32_t>> block_category_metadata_values_offset = 0;
  if (m.block_category_metadata.has_value()) {
    std::vector<uint32_t> keys;
    std::vector<uint32_t> values;
    for (const auto& [k, v] : m.block_category_metadata.value()) {
      keys.push_back(k);
      values.push_back(v);
    }
    block_category_metadata_keys_offset = builder.CreateVector(keys);
    block_category_metadata_values_offset = builder.CreateVector(values);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<::flatbuffers::Offset<dwarfs::flatbuffers::HistoryEntry>>> history_offset = 0;
  if (m.metadata_version_history.has_value()) {
    std::vector<::flatbuffers::Offset<dwarfs::flatbuffers::HistoryEntry>> history_offsets;
    history_offsets.reserve(m.metadata_version_history->size());
    for (const auto& e : m.metadata_version_history.value()) {
      history_offsets.push_back(to_flatbuffers(builder, e));
    }
    history_offset = builder.CreateVector(history_offsets);
  }

  ::flatbuffers::Offset<::flatbuffers::Vector<uint64_t>> large_hole_size_offset = 0;
  if (m.large_hole_size.has_value()) {
    large_hole_size_offset = builder.CreateVector(m.large_hole_size.value());
  }

  return dwarfs::flatbuffers::CreateMetadata(
    builder,
    chunks_offset,
    directories_offset,
    inodes_offset,
    chunk_table_offset,
    entry_table_v2_2_offset,
    symlink_table_offset,
    uids_offset,
    gids_offset,
    modes_offset,
    names_offset,
    symlinks_offset,
    m.timestamp_base,
    m.block_size,
    m.total_fs_size,
    devices_offset,
    options_offset,
    dir_entries_offset,
    shared_files_offset,
    m.total_hardlink_size.value_or(0),
    version_offset,
    m.create_timestamp.value_or(0),
    compact_names_offset,
    compact_symlinks_offset,
    m.preferred_path_separator.value_or(0),
    features_offset,
    category_names_offset,
    block_categories_offset,
    reg_file_size_cache_offset,
    category_metadata_json_offset,
    block_category_metadata_keys_offset,
    block_category_metadata_values_offset,
    history_offset,
    m.hole_block_index.value_or(0),
    large_hole_size_offset,
    m.total_allocated_fs_size.value_or(0)
  );
}

} // namespace dwarfs::metadata::converters