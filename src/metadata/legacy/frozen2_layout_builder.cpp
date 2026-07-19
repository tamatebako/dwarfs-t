/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "dwarfs/metadata/legacy/frozen2_layout_builder.h"

#include <algorithm>

namespace dwarfs::metadata::legacy {

// Primitive helpers (ser_frozen.rs:135-141, 270-290)

std::unique_ptr<Layout> make_primitive(uint16_t byte_size) {
  if (byte_size == 0) {
    return std::make_unique<LayoutNone>();
  }
  return std::make_unique<LayoutPrimitive>(byte_size);
}

std::unique_ptr<Layout> build_bool(bool v) {
  if (v) {
    return std::make_unique<LayoutPrimitive>(1);
  }
  return std::make_unique<LayoutNone>();
}

std::unique_ptr<Layout> build_u32(uint32_t v) {
  if (v != 0) {
    return std::make_unique<LayoutPrimitive>(4);
  }
  return std::make_unique<LayoutNone>();
}

std::unique_ptr<Layout> build_u64(uint64_t v) {
  if (v != 0) {
    return std::make_unique<LayoutPrimitive>(8);
  }
  return std::make_unique<LayoutNone>();
}

std::unique_ptr<Layout> build_bytes(std::string const& v) {
  uint32_t len = static_cast<uint32_t>(v.size());
  bool has_elem = std::any_of(v.begin(), v.end(), [](char c) { return c != 0; });

  auto st = std::make_unique<LayoutStruct>();
  // Distance (relaxed if all zeros)
  if (has_elem) {
    st->add_field(std::make_unique<LayoutPrimitive>(4));
  } else {
    st->add_field(std::make_unique<LayoutNone>());
  }
  // Count
  if (len != 0) {
    st->add_field(std::make_unique<LayoutPrimitive>(4));
  } else {
    st->add_field(std::make_unique<LayoutNone>());
  }
  return st;
}

// Domain type builders

std::unique_ptr<Layout> build_chunk(domain::chunk const& c) {
  auto st = std::make_unique<LayoutStruct>();
  st->add_field(build_u32(c.block()));
  st->add_field(build_u32(c.offset()));
  st->add_field(build_u32(c.size()));
  return st;
}

std::unique_ptr<Layout> build_directory(domain::directory const& d) {
  auto st = std::make_unique<LayoutStruct>();
  st->add_field(build_u32(d.first_entry()));
  st->add_field(build_u32(d.parent_entry()));
  st->add_field(build_u32(d.self_entry()));
  return st;
}

std::unique_ptr<Layout> build_inode_data(domain::inode_data const& i) {
  auto st = std::make_unique<LayoutStruct>();
  st->add_field(build_u32(i.mode_index));
  st->add_field(build_u32(i.owner_index));
  st->add_field(build_u32(i.group_index));
  st->add_field(build_u64(i.atime_offset));
  st->add_field(build_u64(i.mtime_offset));
  st->add_field(build_u64(i.ctime_offset));
  st->add_field(build_u64(i.btime_offset));
  st->add_field(build_u32(i.nlink_minus_one));
  st->add_field(build_optional(i.name_index_v2_2, [](uint32_t v) { return build_u32(v); }));
  st->add_field(build_optional(i.inode_v2_2, [](uint32_t v) { return build_u32(v); }));
  return st;
}

std::unique_ptr<Layout> build_dir_entry(domain::dir_entry const& e) {
  auto st = std::make_unique<LayoutStruct>();
  st->add_field(build_u32(e.name_index()));
  st->add_field(build_u32(e.inode_num()));
  return st;
}

std::unique_ptr<Layout> build_fs_options(domain::fs_options const& opts) {
  auto st = std::make_unique<LayoutStruct>();
  st->add_field(build_bool(opts.mtime_only));
  st->add_field(build_optional(opts.time_resolution_sec, [](uint32_t v) { return build_u32(v); }));
  st->add_field(build_bool(opts.packed_chunk_table));
  st->add_field(build_bool(opts.packed_directories));
  st->add_field(build_bool(opts.packed_shared_files_table));
  st->add_field(build_optional(opts.subsecond_resolution_nsec_multiplier, [](uint32_t v) { return build_u32(v); }));
  st->add_field(build_bool(opts.has_btime));
  st->add_field(build_bool(opts.inodes_have_nlink));
  return st;
}

std::unique_ptr<Layout> build_string_table(domain::string_table const& st) {
  auto layout = std::make_unique<LayoutStruct>();
  layout->add_field(build_bytes(st.buffer));
  layout->add_field(build_optional(st.symtab, [](std::string const& v) { return build_bytes(v); }));
  layout->add_field(build_vector(st.index, [](uint32_t v) { return build_u32(v); }));
  layout->add_field(build_bool(st.packed_index));
  return layout;
}

std::unique_ptr<Layout> build_inode_size_cache(domain::inode_size_cache const& cache) {
  auto layout = std::make_unique<LayoutStruct>();
  layout->add_field(build_map(cache.size_lookup,
      [](uint32_t k) { return build_u32(k); },
      [](uint64_t v) { return build_u64(v); }));
  layout->add_field(build_u64(cache.min_chunk_count));
  layout->add_field(build_map(cache.allocated_size_lookup,
      [](uint32_t k) { return build_u32(k); },
      [](uint64_t v) { return build_u64(v); }));
  return layout;
}

std::unique_ptr<Layout> build_history_entry(domain::history_entry const& entry) {
  auto layout = std::make_unique<LayoutStruct>();
  // uint8_t stored as u32
  layout->add_field(build_u32(entry.major));
  layout->add_field(build_u32(entry.minor));
  layout->add_field(build_optional(entry.dwarfs_version, [](std::string const& v) { return build_bytes(v); }));
  layout->add_field(build_u32(entry.block_size));
  layout->add_field(build_optional(entry.options, [](domain::fs_options const& v) { return build_fs_options(v); }));
  return layout;
}

// Top-level metadata builder - all 36 fields in order
std::unique_ptr<Layout> build_metadata(domain::metadata const& meta) {
  auto st = std::make_unique<LayoutStruct>();

  // Core structures
  st->add_field(build_vector(meta.chunks, [](domain::chunk const& v) { return build_chunk(v); }));
  st->add_field(build_vector(meta.directories, [](domain::directory const& v) { return build_directory(v); }));
  st->add_field(build_vector(meta.inodes, [](domain::inode_data const& v) { return build_inode_data(v); }));

  // Tables
  st->add_field(build_vector(meta.chunk_table, [](uint32_t v) { return build_u32(v); }));
  st->add_field(build_vector(meta.entry_table_v2_2, [](uint32_t v) { return build_u32(v); }));
  st->add_field(build_vector(meta.symlink_table, [](uint32_t v) { return build_u32(v); }));

  // Lookup tables
  st->add_field(build_vector(meta.uids, [](uint32_t v) { return build_u32(v); }));
  st->add_field(build_vector(meta.gids, [](uint32_t v) { return build_u32(v); }));
  st->add_field(build_vector(meta.modes, [](uint32_t v) { return build_u32(v); }));
  st->add_field(build_vector(meta.names, [](std::string const& v) { return build_bytes(v); }));
  st->add_field(build_vector(meta.symlinks, [](std::string const& v) { return build_bytes(v); }));

  // Filesystem parameters
  st->add_field(build_u64(meta.timestamp_base));
  st->add_field(build_u32(meta.block_size));
  st->add_field(build_u64(meta.total_fs_size));

  // Optional features
  st->add_field(build_optional(meta.devices,
      [](auto const& v) { return build_vector(v, [](uint64_t val) { return build_u64(val); }); }));
  st->add_field(build_optional(meta.options,
      [](domain::fs_options const& v) { return build_fs_options(v); }));

  // Directory entries (v2.3+)
  st->add_field(build_optional(meta.dir_entries,
      [](auto const& v) { return build_vector(v, [](domain::dir_entry const& e) { return build_dir_entry(e); }); }));
  st->add_field(build_optional(meta.shared_files_table,
      [](auto const& v) { return build_vector(v, [](uint32_t val) { return build_u32(val); }); }));
  st->add_field(build_optional(meta.total_hardlink_size,
      [](uint64_t v) { return build_u64(v); }));

  // Metadata information
  st->add_field(build_optional(meta.dwarfs_version,
      [](std::string const& v) { return build_bytes(v); }));
  st->add_field(build_optional(meta.create_timestamp,
      [](uint64_t v) { return build_u64(v); }));

  // Compact string storage (v2.3+)
  st->add_field(build_optional(meta.compact_names,
      [](domain::string_table const& v) { return build_string_table(v); }));
  st->add_field(build_optional(meta.compact_symlinks,
      [](domain::string_table const& v) { return build_string_table(v); }));

  // Path separator (v2.5+)
  st->add_field(build_optional(meta.preferred_path_separator,
      [](uint32_t v) { return build_u32(v); }));

  // Features and categories (v2.5+)
  st->add_field(build_optional(meta.features,
      [](auto const& v) { return build_set(v, [](std::string const& s) { return build_bytes(s); }); }));
  st->add_field(build_optional(meta.category_names,
      [](auto const& v) { return build_vector(v, [](std::string const& s) { return build_bytes(s); }); }));
  st->add_field(build_optional(meta.block_categories,
      [](auto const& v) { return build_vector(v, [](uint32_t val) { return build_u32(val); }); }));

  // Performance caches (v2.5+)
  st->add_field(build_optional(meta.reg_file_size_cache,
      [](domain::inode_size_cache const& v) { return build_inode_size_cache(v); }));

  // Category metadata (v2.5+)
  st->add_field(build_optional(meta.category_metadata_json,
      [](auto const& v) { return build_vector(v, [](std::string const& s) { return build_bytes(s); }); }));
  st->add_field(build_optional(meta.block_category_metadata,
      [](auto const& v) { return build_map(v,
          [](uint32_t k) { return build_u32(k); },
          [](uint32_t val) { return build_u32(val); }); }));

  // Version history (v2.5+)
  st->add_field(build_optional(meta.metadata_version_history,
      [](auto const& v) { return build_vector(v, [](domain::history_entry const& e) { return build_history_entry(e); }); }));

  // Sparse file support (v2.5+)
  st->add_field(build_optional(meta.hole_block_index,
      [](uint32_t v) { return build_u32(v); }));
  st->add_field(build_optional(meta.large_hole_size,
      [](auto const& v) { return build_vector(v, [](uint64_t val) { return build_u64(val); }); }));
  st->add_field(build_optional(meta.total_allocated_fs_size,
      [](uint64_t v) { return build_u64(v); }));

  return st;
}

} // namespace dwarfs::metadata::legacy