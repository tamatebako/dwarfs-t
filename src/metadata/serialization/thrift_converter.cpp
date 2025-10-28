/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <dwarfs/metadata/serialization/thrift_converter.h>

#include <algorithm>
#include <map>
#include <set>

namespace dwarfs::metadata::serialization {

namespace {

// Helper to convert Thrift optional fields to std::optional
template <typename T>
std::optional<T> convert_optional(const apache::thrift::optional_field_ref<const T&> opt) {
  if (opt.has_value()) {
    return opt.value();
  }
  return std::nullopt;
}

// Helper to convert vector fields that might be unset
template <typename T>
std::vector<T> convert_vector(const std::vector<T>& vec) {
  return vec; // Direct copy for vectors
}

// Helper to convert optional vectors
template <typename T>
std::optional<std::vector<T>> convert_optional_vector(
    const apache::thrift::optional_field_ref<const std::vector<T>&> opt) {
  if (opt.has_value()) {
    return opt.value();
  }
  return std::nullopt;
}

// Helper to convert maps
template <typename K, typename V>
std::map<K, V> convert_map(const std::map<K, V>& m) {
  return m; // Direct copy for maps
}

// Helper to convert optional maps
template <typename K, typename V>
std::optional<std::map<K, V>> convert_optional_map(
    const apache::thrift::optional_field_ref<const std::map<K, V>&> opt) {
  if (opt.has_value()) {
    return opt.value();
  }
  return std::nullopt;
}

// Helper to convert sets
template <typename T>
std::set<T> convert_set(const std::set<T>& s) {
  return s; // Direct copy for sets
}

// Helper to convert optional sets
template <typename T>
std::optional<std::set<T>> convert_optional_set(
    const apache::thrift::optional_field_ref<const std::set<T>&> opt) {
  if (opt.has_value()) {
    return opt.value();
  }
  return std::nullopt;
}

} // anonymous namespace

domain::chunk ThriftConverter::to_domain(
    const thrift::metadata::chunk& thrift_chunk) {
  domain::chunk result;
  result.block = thrift_chunk.block().value();
  result.offset = thrift_chunk.offset().value();
  result.size = thrift_chunk.size().value();
  return result;
}

domain::directory ThriftConverter::to_domain(
    const thrift::metadata::directory& thrift_dir) {
  domain::directory result;
  result.parent_entry = thrift_dir.parent_entry().value();
  result.first_entry = thrift_dir.first_entry().value();
  result.self_entry = convert_optional(thrift_dir.self_entry());
  return result;
}

domain::inode_data ThriftConverter::to_domain(
    const thrift::metadata::inode_data& thrift_inode) {
  domain::inode_data result;

  result.mode_index = thrift_inode.mode_index().value();
  result.owner_index = thrift_inode.owner_index().value();
  result.group_index = thrift_inode.group_index().value();
  result.atime_offset = thrift_inode.atime_offset().value();
  result.mtime_offset = thrift_inode.mtime_offset().value();
  result.ctime_offset = thrift_inode.ctime_offset().value();

  // Fields added in v2.5
  result.btime_offset = convert_optional(thrift_inode.btime_offset());
  result.atime_subsec = convert_optional(thrift_inode.atime_subsec());
  result.mtime_subsec = convert_optional(thrift_inode.mtime_subsec());
  result.ctime_subsec = convert_optional(thrift_inode.ctime_subsec());
  result.btime_subsec = convert_optional(thrift_inode.btime_subsec());
  result.nlink_minus_one = convert_optional(thrift_inode.nlink_minus_one());

  // Deprecated v2.2 fields
  result.name_index_v2_2 = convert_optional(thrift_inode.name_index_v2_2());
  result.inode_v2_2 = convert_optional(thrift_inode.inode_v2_2());

  return result;
}

domain::dir_entry ThriftConverter::to_domain(
    const thrift::metadata::dir_entry& thrift_entry) {
  domain::dir_entry result;
  result.name_index = thrift_entry.name_index().value();
  result.inode_num = thrift_entry.inode_num().value();
  return result;
}

domain::fs_options ThriftConverter::to_domain(
    const thrift::metadata::fs_options& thrift_opts) {
  domain::fs_options result;

  result.mtime_only = thrift_opts.mtime_only().value();
  result.time_resolution_sec = convert_optional(thrift_opts.time_resolution_sec());
  result.packed_chunk_table = thrift_opts.packed_chunk_table().value();
  result.packed_directories = thrift_opts.packed_directories().value();
  result.packed_shared_files_table = thrift_opts.packed_shared_files_table().value();

  // Fields added in v2.5
  result.subsecond_resolution_nsec_multiplier =
      convert_optional(thrift_opts.subsecond_resolution_nsec_multiplier());
  result.has_btime = thrift_opts.has_btime().value();
  result.inodes_have_nlink = thrift_opts.inodes_have_nlink().value();

  return result;
}

domain::string_table ThriftConverter::to_domain(
    const thrift::metadata::string_table& thrift_table) {
  domain::string_table result;

  result.buffer = thrift_table.buffer().value();
  result.symtab = convert_optional(thrift_table.symtab());
  result.index = convert_vector(thrift_table.index().value());
  result.packed_index = thrift_table.packed_index().value();

  return result;
}

domain::inode_size_cache ThriftConverter::to_domain(
    const thrift::metadata::inode_size_cache& thrift_cache) {
  domain::inode_size_cache result;

  result.size_lookup = convert_map(thrift_cache.size_lookup().value());
  result.min_chunk_count = thrift_cache.min_chunk_count().value();
  result.allocated_size_lookup =
      convert_optional_map(thrift_cache.allocated_size_lookup());

  return result;
}

domain::history_entry ThriftConverter::to_domain(
    const thrift::metadata::history_entry& thrift_entry) {
  domain::history_entry result;

  result.major = thrift_entry.major().value();
  result.minor = thrift_entry.minor().value();
  result.dwarfs_version = convert_optional(thrift_entry.dwarfs_version());
  result.block_size = thrift_entry.block_size().value();

  if (thrift_entry.options().has_value()) {
    result.options = to_domain(thrift_entry.options().value());
  }

  return result;
}

domain::metadata ThriftConverter::to_domain(
    const thrift::metadata::metadata& thrift_meta) {
  domain::metadata result;

  // Base fields (version 1)
  result.chunks = to_domain_vector<domain::chunk>(thrift_meta.chunks().value());
  result.directories = to_domain_vector<domain::directory>(thrift_meta.directories().value());
  result.inodes = to_domain_vector<domain::inode_data>(thrift_meta.inodes().value());
  result.chunk_table = convert_vector(thrift_meta.chunk_table().value());
  result.symlink_table = convert_vector(thrift_meta.symlink_table().value());
  result.uids = convert_vector(thrift_meta.uids().value());
  result.gids = convert_vector(thrift_meta.gids().value());
  result.modes = convert_vector(thrift_meta.modes().value());
  result.names = convert_vector(thrift_meta.names().value());
  result.symlinks = convert_vector(thrift_meta.symlinks().value());
  result.timestamp_base = thrift_meta.timestamp_base().value();
  result.block_size = thrift_meta.block_size().value();
  result.total_fs_size = thrift_meta.total_fs_size().value();

  // Deprecated v2.2 field
  result.entry_table_v2_2 = convert_vector(thrift_meta.entry_table_v2_2().value());

  // Fields added in dwarfs-0.3.0, file system version 2.1
  if (thrift_meta.devices().has_value()) {
    result.devices = thrift_meta.devices().value();
  }

  if (thrift_meta.options().has_value()) {
    result.options = to_domain(thrift_meta.options().value());
  }

  // Fields added in dwarfs-0.5.0, file system version 2.3
  if (thrift_meta.dir_entries().has_value()) {
    result.dir_entries = to_domain_vector<domain::dir_entry>(
        thrift_meta.dir_entries().value());
  }

  result.shared_files_table = convert_optional_vector(thrift_meta.shared_files_table());
  result.total_hardlink_size = convert_optional(thrift_meta.total_hardlink_size());
  result.dwarfs_version = convert_optional(thrift_meta.dwarfs_version());
  result.create_timestamp = convert_optional(thrift_meta.create_timestamp());

  if (thrift_meta.compact_names().has_value()) {
    result.compact_names = to_domain(thrift_meta.compact_names().value());
  }

  if (thrift_meta.compact_symlinks().has_value()) {
    result.compact_symlinks = to_domain(thrift_meta.compact_symlinks().value());
  }

  // Fields added in dwarfs-0.7.0, file system version 2.5
  result.preferred_path_separator = convert_optional(thrift_meta.preferred_path_separator());

  // Fields added in dwarfs-0.7.3, file system version 2.5
  result.features = convert_optional_set(thrift_meta.features());

  // Fields added in dwarfs-0.8.0, file system version 2.5
  result.category_names = convert_optional_vector(thrift_meta.category_names());
  result.block_categories = convert_optional_vector(thrift_meta.block_categories());

  // Fields added in dwarfs-0.11.0, file system version 2.5
  if (thrift_meta.reg_file_size_cache().has_value()) {
    result.reg_file_size_cache = to_domain(thrift_meta.reg_file_size_cache().value());
  }

  // Fields added in dwarfs-0.13.0, file system version 2.5
  result.category_metadata_json = convert_optional_vector(thrift_meta.category_metadata_json());
  result.block_category_metadata = convert_optional_map(thrift_meta.block_category_metadata());

  if (thrift_meta.metadata_version_history().has_value()) {
    result.metadata_version_history = to_domain_vector<domain::history_entry>(
        thrift_meta.metadata_version_history().value());
  }

  // Fields added in dwarfs-0.14.0, file system version 2.5
  result.hole_block_index = convert_optional(thrift_meta.hole_block_index());
  result.large_hole_size = convert_optional_vector(thrift_meta.large_hole_size());
  result.total_allocated_fs_size = convert_optional(thrift_meta.total_allocated_fs_size());

  return result;
}

} // namespace dwarfs::metadata::serialization