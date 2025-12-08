#include "dwarfs/metadata/converters/thrift_metadata_converter.h"

// Include generated Thrift types
#include <dwarfs/gen-cpp2/metadata_types.h>

#include <iostream>
#include <stdexcept>

namespace dwarfs::metadata::converters {

// =====================================================================
// Main conversion: Thrift → Domain
// =====================================================================

domain::metadata ThriftMetadataConverter::to_domain(
    const void* format_specific) const {

  if (!format_specific) {
    throw std::invalid_argument("format_specific cannot be null");
  }

  auto* t_meta = static_cast<const thrift::metadata::metadata*>(format_specific);

  // DEBUG: Log input Thrift metadata
  std::cerr << "=== Thrift→Domain Conversion ===" << std::endl;
  std::cerr << "Input names: " << t_meta->names()->size() << std::endl;
  std::cerr << "Input dir_entries: " << (t_meta->dir_entries() ? t_meta->dir_entries()->size() : 0) << std::endl;

  domain::metadata d_meta;

  // ===================================================================
  // Convert core collections (MECE: all non-optional fields)
  // ===================================================================

  // Convert chunks
  d_meta.chunks.reserve(t_meta->chunks()->size());
  for (const auto& t_chunk : *t_meta->chunks()) {
    d_meta.chunks.push_back(convert_chunk_to_domain(t_chunk));
  }

  // Convert directories
  d_meta.directories.reserve(t_meta->directories()->size());
  for (const auto& t_dir : *t_meta->directories()) {
    d_meta.directories.push_back(convert_directory_to_domain(t_dir));
  }

  // Convert inodes
  d_meta.inodes.reserve(t_meta->inodes()->size());
  for (const auto& t_inode : *t_meta->inodes()) {
    d_meta.inodes.push_back(convert_inode_to_domain(t_inode));
  }

  // Convert tables (simple vector copies)
  d_meta.chunk_table = *t_meta->chunk_table();
  d_meta.entry_table_v2_2 = *t_meta->entry_table_v2_2();
  d_meta.symlink_table = *t_meta->symlink_table();

  // Convert lookup tables
  d_meta.uids = *t_meta->uids();
  d_meta.gids = *t_meta->gids();
  d_meta.modes = *t_meta->modes();
  d_meta.names = *t_meta->names();
  d_meta.symlinks = *t_meta->symlinks();

  // Convert basic fields
  d_meta.timestamp_base = *t_meta->timestamp_base();
  d_meta.block_size = *t_meta->block_size();
  d_meta.total_fs_size = *t_meta->total_fs_size();

  // ===================================================================
  // Convert optional fields (v2.1+)
  // ===================================================================

  if (t_meta->devices()) {
    d_meta.devices = *t_meta->devices();
  }

  if (t_meta->options()) {
    d_meta.options = convert_fs_options_to_domain(*t_meta->options());
  }

  // ===================================================================
  // Convert optional fields (v2.3+)
  // ===================================================================

  if (t_meta->dir_entries()) {
    std::vector<domain::dir_entry> entries;
    entries.reserve(t_meta->dir_entries()->size());
    for (const auto& t_entry : *t_meta->dir_entries()) {
      entries.push_back(convert_dir_entry_to_domain(t_entry));
    }
    d_meta.dir_entries = std::move(entries);
  }

  if (t_meta->shared_files_table()) {
    d_meta.shared_files_table = *t_meta->shared_files_table();
  }

  if (t_meta->total_hardlink_size()) {
    d_meta.total_hardlink_size = *t_meta->total_hardlink_size();
  }

  if (t_meta->dwarfs_version()) {
    d_meta.dwarfs_version = *t_meta->dwarfs_version();
  }

  if (t_meta->create_timestamp()) {
    d_meta.create_timestamp = *t_meta->create_timestamp();
  }

  if (t_meta->compact_names()) {
    d_meta.compact_names = convert_string_table_to_domain(*t_meta->compact_names());
  }

  if (t_meta->compact_symlinks()) {
    d_meta.compact_symlinks = convert_string_table_to_domain(*t_meta->compact_symlinks());
  }

  // ===================================================================
  // Convert optional fields (v2.5+)
  // ===================================================================

  if (t_meta->preferred_path_separator()) {
    d_meta.preferred_path_separator = *t_meta->preferred_path_separator();
  }

  if (t_meta->features()) {
    d_meta.features = *t_meta->features();
  }

  if (t_meta->category_names()) {
    d_meta.category_names = *t_meta->category_names();
  }

  if (t_meta->block_categories()) {
    d_meta.block_categories = *t_meta->block_categories();
  }

  if (t_meta->reg_file_size_cache()) {
    d_meta.reg_file_size_cache = convert_inode_size_cache_to_domain(
        *t_meta->reg_file_size_cache());
  }

  if (t_meta->category_metadata_json()) {
    d_meta.category_metadata_json = *t_meta->category_metadata_json();
  }

  if (t_meta->block_category_metadata()) {
    d_meta.block_category_metadata = *t_meta->block_category_metadata();
  }

  if (t_meta->metadata_version_history()) {
    std::vector<domain::history_entry> history;
    history.reserve(t_meta->metadata_version_history()->size());
    for (const auto& t_entry : *t_meta->metadata_version_history()) {
      history.push_back(convert_history_entry_to_domain(t_entry));
    }
    d_meta.metadata_version_history = std::move(history);
  }

  if (t_meta->hole_block_index()) {
    d_meta.hole_block_index = *t_meta->hole_block_index();
  }

  if (t_meta->large_hole_size()) {
    d_meta.large_hole_size = *t_meta->large_hole_size();
  }

  if (t_meta->total_allocated_fs_size()) {
    d_meta.total_allocated_fs_size = *t_meta->total_allocated_fs_size();
  }

  // DEBUG: Log output Domain metadata
  std::cerr << "Output names: " << d_meta.names.size() << std::endl;
  std::cerr << "Output dir_entries: " << (d_meta.dir_entries ? d_meta.dir_entries->size() : 0) << std::endl;
  std::cerr << "=== End Conversion ===" << std::endl;

  return d_meta;
}

// =====================================================================
// Main conversion: Domain → Thrift
// =====================================================================

std::unique_ptr<void, void(*)(void*)> ThriftMetadataConverter::from_domain(
    const domain::metadata& d_meta) const {

  // DEBUG: Log input Domain metadata
  std::cerr << "=== Domain→Thrift Conversion ===" << std::endl;
  std::cerr << "Input names: " << d_meta.names.size() << std::endl;
  std::cerr << "Input dir_entries: " << (d_meta.dir_entries ? d_meta.dir_entries->size() : 0) << std::endl;

  auto t_meta = std::make_unique<thrift::metadata::metadata>();

  // ===================================================================
  // Convert core collections
  // ===================================================================

  // Convert chunks
  t_meta->chunks()->reserve(d_meta.chunks.size());
  for (const auto& d_chunk : d_meta.chunks) {
    t_meta->chunks()->push_back(convert_chunk_from_domain(d_chunk));
  }

  // Convert directories
  t_meta->directories()->reserve(d_meta.directories.size());
  for (const auto& d_dir : d_meta.directories) {
    t_meta->directories()->push_back(convert_directory_from_domain(d_dir));
  }

  // Convert inodes
  t_meta->inodes()->reserve(d_meta.inodes.size());
  for (const auto& d_inode : d_meta.inodes) {
    t_meta->inodes()->push_back(convert_inode_from_domain(d_inode));
  }

  // Convert tables
  *t_meta->chunk_table() = d_meta.chunk_table;
  *t_meta->entry_table_v2_2() = d_meta.entry_table_v2_2;
  *t_meta->symlink_table() = d_meta.symlink_table;

  // Convert lookup tables
  *t_meta->uids() = d_meta.uids;
  *t_meta->gids() = d_meta.gids;
  *t_meta->modes() = d_meta.modes;
  *t_meta->names() = d_meta.names;
  *t_meta->symlinks() = d_meta.symlinks;

  // Convert basic fields
  *t_meta->timestamp_base() = d_meta.timestamp_base;
  *t_meta->block_size() = d_meta.block_size;
  *t_meta->total_fs_size() = d_meta.total_fs_size;

  // ===================================================================
  // Convert optional fields (v2.1+)
  // ===================================================================

  if (d_meta.devices) {
    t_meta->devices() = *d_meta.devices;
  }

  if (d_meta.options) {
    t_meta->options() = convert_fs_options_from_domain(*d_meta.options);
  }

  // ===================================================================
  // Convert optional fields (v2.3+)
  // ===================================================================

  if (d_meta.dir_entries) {
    std::vector<thrift::metadata::dir_entry> entries;
    entries.reserve(d_meta.dir_entries->size());
    for (const auto& d_entry : *d_meta.dir_entries) {
      entries.push_back(convert_dir_entry_from_domain(d_entry));
    }
    t_meta->dir_entries() = std::move(entries);
  }

  if (d_meta.shared_files_table) {
    t_meta->shared_files_table() = *d_meta.shared_files_table;
  }

  if (d_meta.total_hardlink_size) {
    t_meta->total_hardlink_size() = *d_meta.total_hardlink_size;
  }

  if (d_meta.dwarfs_version) {
    t_meta->dwarfs_version() = *d_meta.dwarfs_version;
  }

  if (d_meta.create_timestamp) {
    t_meta->create_timestamp() = *d_meta.create_timestamp;
  }

  if (d_meta.compact_names) {
    t_meta->compact_names() = convert_string_table_from_domain(*d_meta.compact_names);
  }

  if (d_meta.compact_symlinks) {
    t_meta->compact_symlinks() = convert_string_table_from_domain(*d_meta.compact_symlinks);
  }

  // ===================================================================
  // Convert optional fields (v2.5+)
  // ===================================================================

  if (d_meta.preferred_path_separator) {
    t_meta->preferred_path_separator() = *d_meta.preferred_path_separator;
  }

  if (d_meta.features) {
    t_meta->features() = *d_meta.features;
  }

  if (d_meta.category_names) {
    t_meta->category_names() = *d_meta.category_names;
  }

  if (d_meta.block_categories) {
    t_meta->block_categories() = *d_meta.block_categories;
  }

  if (d_meta.reg_file_size_cache) {
    t_meta->reg_file_size_cache() = convert_inode_size_cache_from_domain(
        *d_meta.reg_file_size_cache);
  }

  if (d_meta.category_metadata_json) {
    t_meta->category_metadata_json() = *d_meta.category_metadata_json;
  }

  if (d_meta.block_category_metadata) {
    t_meta->block_category_metadata() = *d_meta.block_category_metadata;
  }

  if (d_meta.metadata_version_history) {
    std::vector<thrift::metadata::history_entry> history;
    history.reserve(d_meta.metadata_version_history->size());
    for (const auto& d_entry : *d_meta.metadata_version_history) {
      history.push_back(convert_history_entry_from_domain(d_entry));
    }
    t_meta->metadata_version_history() = std::move(history);
  }

  if (d_meta.hole_block_index) {
    t_meta->hole_block_index() = *d_meta.hole_block_index;
  }

  if (d_meta.large_hole_size) {
    t_meta->large_hole_size() = *d_meta.large_hole_size;
  }

  if (d_meta.total_allocated_fs_size) {
    t_meta->total_allocated_fs_size() = *d_meta.total_allocated_fs_size;
  }

  // DEBUG: Log output Thrift metadata
  std::cerr << "Output names: " << t_meta->names()->size() << std::endl;
  std::cerr << "Output dir_entries: " << (t_meta->dir_entries() ? t_meta->dir_entries()->size() : 0) << std::endl;
  std::cerr << "=== End Conversion ===" << std::endl;

  // Return with custom deleter
  return {
    t_meta.release(),
    [](void* p) { delete static_cast<thrift::metadata::metadata*>(p); }
  };
}

// =====================================================================
// Helper conversions: Thrift → Domain
// =====================================================================

domain::chunk ThriftMetadataConverter::convert_chunk_to_domain(
    const thrift::metadata::chunk& t_chunk) const {
  return domain::chunk(*t_chunk.block(), *t_chunk.offset(), *t_chunk.size());
}

domain::directory ThriftMetadataConverter::convert_directory_to_domain(
    const thrift::metadata::directory& t_dir) const {
  return domain::directory(
      *t_dir.parent_entry(),
      *t_dir.first_entry(),
      t_dir.self_entry().has_value() ? *t_dir.self_entry() : 0);
}

domain::inode_data ThriftMetadataConverter::convert_inode_to_domain(
    const thrift::metadata::inode_data& t_inode) const {

  domain::inode_data d_inode;

  // Required fields
  d_inode.mode_index = *t_inode.mode_index();
  d_inode.owner_index = *t_inode.owner_index();
  d_inode.group_index = *t_inode.group_index();
  d_inode.atime_offset = *t_inode.atime_offset();
  d_inode.mtime_offset = *t_inode.mtime_offset();
  d_inode.ctime_offset = *t_inode.ctime_offset();

  // Optional v2.5+ fields
  if (t_inode.btime_offset().has_value()) {
    d_inode.btime_offset = *t_inode.btime_offset();
  }

  if (t_inode.atime_subsec().has_value()) {
    d_inode.atime_subsec = *t_inode.atime_subsec();
  }

  if (t_inode.mtime_subsec().has_value()) {
    d_inode.mtime_subsec = *t_inode.mtime_subsec();
  }

  if (t_inode.ctime_subsec().has_value()) {
    d_inode.ctime_subsec = *t_inode.ctime_subsec();
  }

  if (t_inode.btime_subsec().has_value()) {
    d_inode.btime_subsec = *t_inode.btime_subsec();
  }

  if (t_inode.nlink_minus_one().has_value()) {
    d_inode.nlink_minus_one = *t_inode.nlink_minus_one();
  }

  // Deprecated v2.2 fields
  if (t_inode.name_index_v2_2().has_value()) {
    d_inode.name_index_v2_2 = *t_inode.name_index_v2_2();
  }

  if (t_inode.inode_v2_2().has_value()) {
    d_inode.inode_v2_2 = *t_inode.inode_v2_2();
  }

  return d_inode;
}

domain::dir_entry ThriftMetadataConverter::convert_dir_entry_to_domain(
    const thrift::metadata::dir_entry& t_entry) const {
  return domain::dir_entry(*t_entry.name_index(), *t_entry.inode_num());
}

domain::fs_options ThriftMetadataConverter::convert_fs_options_to_domain(
    const thrift::metadata::fs_options& t_opts) const {

  domain::fs_options d_opts;

  d_opts.mtime_only = *t_opts.mtime_only();
  d_opts.packed_chunk_table = *t_opts.packed_chunk_table();
  d_opts.packed_directories = *t_opts.packed_directories();
  d_opts.packed_shared_files_table = *t_opts.packed_shared_files_table();
  d_opts.has_btime = *t_opts.has_btime();
  d_opts.inodes_have_nlink = *t_opts.inodes_have_nlink();

  if (t_opts.time_resolution_sec()) {
    d_opts.time_resolution_sec = *t_opts.time_resolution_sec();
  }

  if (t_opts.subsecond_resolution_nsec_multiplier()) {
    d_opts.subsecond_resolution_nsec_multiplier =
        *t_opts.subsecond_resolution_nsec_multiplier();
  }

  return d_opts;
}

domain::string_table ThriftMetadataConverter::convert_string_table_to_domain(
    const thrift::metadata::string_table& t_table) const {

  domain::string_table d_table;

  d_table.buffer = *t_table.buffer();
  d_table.index = *t_table.index();
  d_table.packed_index = *t_table.packed_index();

  if (t_table.symtab()) {
    d_table.symtab = *t_table.symtab();
  }

  return d_table;
}

domain::inode_size_cache ThriftMetadataConverter::convert_inode_size_cache_to_domain(
    const thrift::metadata::inode_size_cache& t_cache) const {

  domain::inode_size_cache d_cache;

  d_cache.size_lookup = *t_cache.size_lookup();
  d_cache.min_chunk_count = *t_cache.min_chunk_count();

  if (t_cache.allocated_size_lookup().has_value()) {
    d_cache.allocated_size_lookup = *t_cache.allocated_size_lookup();
  }

  return d_cache;
}

domain::history_entry ThriftMetadataConverter::convert_history_entry_to_domain(
    const thrift::metadata::history_entry& t_entry) const {

  domain::history_entry d_entry;

  d_entry.major = *t_entry.major();
  d_entry.minor = *t_entry.minor();
  d_entry.block_size = *t_entry.block_size();

  if (t_entry.dwarfs_version()) {
    d_entry.dwarfs_version = *t_entry.dwarfs_version();
  }

  if (t_entry.options()) {
    d_entry.options = convert_fs_options_to_domain(*t_entry.options());
  }

  return d_entry;
}

// =====================================================================
// Helper conversions: Domain → Thrift
// =====================================================================

thrift::metadata::chunk ThriftMetadataConverter::convert_chunk_from_domain(
    const domain::chunk& d_chunk) const {

  thrift::metadata::chunk t_chunk;
  *t_chunk.block() = d_chunk.block();
  *t_chunk.offset() = d_chunk.offset();
  *t_chunk.size() = d_chunk.size();
  return t_chunk;
}

thrift::metadata::directory ThriftMetadataConverter::convert_directory_from_domain(
    const domain::directory& d_dir) const {

  thrift::metadata::directory t_dir;
  *t_dir.parent_entry() = d_dir.parent_entry();
  *t_dir.first_entry() = d_dir.first_entry();
  *t_dir.self_entry() = d_dir.self_entry();
  return t_dir;
}

thrift::metadata::inode_data ThriftMetadataConverter::convert_inode_from_domain(
    const domain::inode_data& d_inode) const {

  thrift::metadata::inode_data t_inode;

  // Required fields
  *t_inode.mode_index() = d_inode.mode_index;
  *t_inode.owner_index() = d_inode.owner_index;
  *t_inode.group_index() = d_inode.group_index;
  *t_inode.atime_offset() = d_inode.atime_offset;
  *t_inode.mtime_offset() = d_inode.mtime_offset;
  *t_inode.ctime_offset() = d_inode.ctime_offset;

  // Optional v2.5+ fields
  if (d_inode.btime_offset != 0) {
    t_inode.btime_offset() = d_inode.btime_offset;
  }

  if (d_inode.atime_subsec != 0) {
    t_inode.atime_subsec() = d_inode.atime_subsec;
  }

  if (d_inode.mtime_subsec != 0) {
    t_inode.mtime_subsec() = d_inode.mtime_subsec;
  }

  if (d_inode.ctime_subsec != 0) {
    t_inode.ctime_subsec() = d_inode.ctime_subsec;
  }

  if (d_inode.btime_subsec != 0) {
    t_inode.btime_subsec() = d_inode.btime_subsec;
  }

  if (d_inode.nlink_minus_one != 0) {
    t_inode.nlink_minus_one() = d_inode.nlink_minus_one;
  }

  // Deprecated v2.2 fields
  if (d_inode.name_index_v2_2) {
    t_inode.name_index_v2_2() = *d_inode.name_index_v2_2;
  }

  if (d_inode.inode_v2_2) {
    t_inode.inode_v2_2() = *d_inode.inode_v2_2;
  }

  return t_inode;
}

thrift::metadata::dir_entry ThriftMetadataConverter::convert_dir_entry_from_domain(
    const domain::dir_entry& d_entry) const {

  thrift::metadata::dir_entry t_entry;
  *t_entry.name_index() = d_entry.name_index();
  *t_entry.inode_num() = d_entry.inode_num();
  return t_entry;
}

thrift::metadata::fs_options ThriftMetadataConverter::convert_fs_options_from_domain(
    const domain::fs_options& d_opts) const {

  thrift::metadata::fs_options t_opts;

  *t_opts.mtime_only() = d_opts.mtime_only;
  *t_opts.packed_chunk_table() = d_opts.packed_chunk_table;
  *t_opts.packed_directories() = d_opts.packed_directories;
  *t_opts.packed_shared_files_table() = d_opts.packed_shared_files_table;
  *t_opts.has_btime() = d_opts.has_btime;
  *t_opts.inodes_have_nlink() = d_opts.inodes_have_nlink;

  if (d_opts.time_resolution_sec) {
    t_opts.time_resolution_sec() = *d_opts.time_resolution_sec;
  }

  if (d_opts.subsecond_resolution_nsec_multiplier) {
    t_opts.subsecond_resolution_nsec_multiplier() =
        *d_opts.subsecond_resolution_nsec_multiplier;
  }

  return t_opts;
}

thrift::metadata::string_table ThriftMetadataConverter::convert_string_table_from_domain(
    const domain::string_table& d_table) const {

  thrift::metadata::string_table t_table;

  *t_table.buffer() = d_table.buffer;
  *t_table.index() = d_table.index;
  *t_table.packed_index() = d_table.packed_index;

  if (d_table.symtab) {
    t_table.symtab() = *d_table.symtab;
  }

  return t_table;
}

thrift::metadata::inode_size_cache ThriftMetadataConverter::convert_inode_size_cache_from_domain(
    const domain::inode_size_cache& d_cache) const {

  thrift::metadata::inode_size_cache t_cache;

  *t_cache.size_lookup() = d_cache.size_lookup;
  *t_cache.min_chunk_count() = d_cache.min_chunk_count;

  if (!d_cache.allocated_size_lookup.empty()) {
    t_cache.allocated_size_lookup() = d_cache.allocated_size_lookup;
  }

  return t_cache;
}

thrift::metadata::history_entry ThriftMetadataConverter::convert_history_entry_from_domain(
    const domain::history_entry& d_entry) const {

  thrift::metadata::history_entry t_entry;

  *t_entry.major() = d_entry.major;
  *t_entry.minor() = d_entry.minor;
  *t_entry.block_size() = d_entry.block_size;

  if (d_entry.dwarfs_version) {
    t_entry.dwarfs_version() = *d_entry.dwarfs_version;
  }

  if (d_entry.options) {
    t_entry.options() = convert_fs_options_from_domain(*d_entry.options);
  }

  return t_entry;
}

} // namespace dwarfs::metadata::converters