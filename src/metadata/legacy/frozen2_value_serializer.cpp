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

#include "dwarfs/metadata/legacy/frozen2_value_serializer.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace dwarfs::metadata::legacy {

// Serializer implementation

Serializer::Serializer(Layout const* layout,
                       std::vector<uint8_t>& buf,
                       uint32_t base,
                       uint32_t inline_pos)
    : layout_(layout)
    , buf_(buf)
    , base_(base)
    , inline_pos_(inline_pos) {
}

uint32_t Serializer::distance() const {
  return static_cast<uint32_t>(buf_.size()) - base_;
}

void Serializer::put_primitive(uint8_t const* data, size_t size) {
  if (layout_->is_none()) {
    return;
  }

  auto const* prim = dynamic_cast<LayoutPrimitive const*>(layout_);
  if (!prim) {
    throw std::logic_error("put_primitive on non-primitive layout");
  }

  if (prim->byte_size() != size) {
    throw std::logic_error("primitive size mismatch");
  }

  std::memcpy(&buf_[inline_pos_], data, size);
}

LayoutStruct const* Serializer::as_struct(size_t field_count) const {
  Layout const* actual_layout = get_actual_layout(layout_);

  if (!actual_layout || actual_layout->is_none()) {
    return nullptr;
  }

  auto const* st = dynamic_cast<LayoutStruct const*>(actual_layout);
  if (!st) {
    throw std::logic_error("expected struct layout");
  }

  if (st->fields().size() != field_count) {
    throw std::logic_error("struct field count mismatch");
  }

  return st;
}

// Primitive serializers (ser_frozen.rs:576-608)

void Serializer::serialize_bool(bool v) {
  uint8_t byte = v ? 1 : 0;
  put_primitive(&byte, 1);
}

void Serializer::serialize_u32(uint32_t v) {
  uint8_t bytes[4];
  bytes[0] = v & 0xFF;
  bytes[1] = (v >> 8) & 0xFF;
  bytes[2] = (v >> 16) & 0xFF;
  bytes[3] = (v >> 24) & 0xFF;
  put_primitive(bytes, 4);
}

void Serializer::serialize_u64(uint64_t v) {
  uint8_t bytes[8];
  bytes[0] = v & 0xFF;
  bytes[1] = (v >> 8) & 0xFF;
  bytes[2] = (v >> 16) & 0xFF;
  bytes[3] = (v >> 24) & 0xFF;
  bytes[4] = (v >> 32) & 0xFF;
  bytes[5] = (v >> 40) & 0xFF;
  bytes[6] = (v >> 48) & 0xFF;
  bytes[7] = (v >> 56) & 0xFF;
  put_primitive(bytes, 8);
}

void Serializer::serialize_bytes(std::string const& v) {
  // ser_frozen.rs:591-608

  auto st_layout = as_struct(2);
  if (!st_layout) {
    return;
  }

  uint32_t distance = this->distance();
  uint32_t len = static_cast<uint32_t>(v.size());

  StructSerializer st(*this, st_layout->fields());

  bool omit_elements = st_layout->fields()[0]->is_none();
  st.serialize_field(distance, [](auto& s, uint32_t val) { s.serialize_u32(val); });
  st.serialize_field(len, [](auto& s, uint32_t val) { s.serialize_u32(val); });

  if (!omit_elements) {
    buf_.insert(buf_.end(), v.begin(), v.end());
  }
}

// Domain type serializers

void Serializer::serialize_chunk(domain::chunk const& c) {
  auto st_layout = as_struct(3);
  if (!st_layout) return;

  StructSerializer st(*this, st_layout->fields());
  st.serialize_field(c.block(), [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(c.offset(), [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(c.size(), [](auto& s, uint32_t v) { s.serialize_u32(v); });
}

void Serializer::serialize_directory(domain::directory const& d) {
  auto st_layout = as_struct(3);
  if (!st_layout) return;

  StructSerializer st(*this, st_layout->fields());
  st.serialize_field(d.first_entry(), [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(d.parent_entry(), [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(d.self_entry(), [](auto& s, uint32_t v) { s.serialize_u32(v); });
}

void Serializer::serialize_inode_data(domain::inode_data const& i) {
  auto st_layout = as_struct(14);
  if (!st_layout) return;

  StructSerializer st(*this, st_layout->fields());
  st.serialize_field(i.mode_index, [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(i.owner_index, [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(i.group_index, [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(i.atime_offset, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(i.mtime_offset, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(i.ctime_offset, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(i.btime_offset, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(i.atime_subsec, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(i.mtime_subsec, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(i.ctime_subsec, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(i.btime_subsec, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(i.nlink_minus_one, [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(i.name_index_v2_2,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(i.inode_v2_2,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
}

void Serializer::serialize_dir_entry(domain::dir_entry const& e) {
  auto st_layout = as_struct(2);
  if (!st_layout) return;

  StructSerializer st(*this, st_layout->fields());
  st.serialize_field(e.name_index(), [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(e.inode_num(), [](auto& s, uint32_t v) { s.serialize_u32(v); });
}

void Serializer::serialize_fs_options(domain::fs_options const& opts) {
  auto st_layout = as_struct(8);
  if (!st_layout) return;

  StructSerializer st(*this, st_layout->fields());
  st.serialize_field(opts.mtime_only, [](auto& s, bool v) { s.serialize_bool(v); });
  st.serialize_field(opts.time_resolution_sec,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(opts.packed_chunk_table, [](auto& s, bool v) { s.serialize_bool(v); });
  st.serialize_field(opts.packed_directories, [](auto& s, bool v) { s.serialize_bool(v); });
  st.serialize_field(opts.packed_shared_files_table, [](auto& s, bool v) { s.serialize_bool(v); });
  st.serialize_field(opts.subsecond_resolution_nsec_multiplier,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(opts.has_btime, [](auto& s, bool v) { s.serialize_bool(v); });
  st.serialize_field(opts.inodes_have_nlink, [](auto& s, bool v) { s.serialize_bool(v); });
}

void Serializer::serialize_string_table(domain::string_table const& table) {
  auto st_layout = as_struct(4);
  if (!st_layout) return;

  StructSerializer st(*this, st_layout->fields());
  st.serialize_field(table.buffer, [](auto& s, auto const& v) { s.serialize_bytes(v); });
  st.serialize_field(table.symtab,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& v) { s2.serialize_bytes(v); });
      });
  st.serialize_field(table.index,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(table.packed_index, [](auto& s, bool v) { s.serialize_bool(v); });
}

void Serializer::serialize_inode_size_cache(domain::inode_size_cache const& cache) {
  auto st_layout = as_struct(3);
  if (!st_layout) return;

  StructSerializer st(*this, st_layout->fields());
  st.serialize_field(cache.size_lookup,
      [](auto& s, auto const& map) {
        s.serialize_map(map,
            [](auto& s2, uint32_t k) { s2.serialize_u32(k); },
            [](auto& s2, uint64_t v) { s2.serialize_u64(v); });
      });
  st.serialize_field(cache.min_chunk_count, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(cache.allocated_size_lookup,
      [](auto& s, auto const& map) {
        s.serialize_map(map,
            [](auto& s2, uint32_t k) { s2.serialize_u32(k); },
            [](auto& s2, uint64_t v) { s2.serialize_u64(v); });
      });
}

void Serializer::serialize_history_entry(domain::history_entry const& entry) {
  auto st_layout = as_struct(5);
  if (!st_layout) return;

  StructSerializer st(*this, st_layout->fields());
  // uint8_t stored as u32
  st.serialize_field(static_cast<uint32_t>(entry.major),
      [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(static_cast<uint32_t>(entry.minor),
      [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(entry.dwarfs_version,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& v) { s2.serialize_bytes(v); });
      });
  st.serialize_field(entry.block_size, [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(entry.options,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& v) { s2.serialize_fs_options(v); });
      });
}

void Serializer::serialize_metadata(domain::metadata const& meta) {
  auto st_layout = as_struct(34);  // 11 vectors + 3 primitives + 20 optionals
  if (!st_layout) return;

  StructSerializer st(*this, st_layout->fields());

  // Core structures
  st.serialize_field(meta.chunks,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, auto const& v) { s2.serialize_chunk(v); });
      });
  st.serialize_field(meta.directories,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, auto const& v) { s2.serialize_directory(v); });
      });
  st.serialize_field(meta.inodes,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, auto const& v) { s2.serialize_inode_data(v); });
      });

  // Tables
  st.serialize_field(meta.chunk_table,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(meta.entry_table_v2_2,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(meta.symlink_table,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });

  // Lookup tables
  st.serialize_field(meta.uids,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(meta.gids,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(meta.modes,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(meta.names,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, auto const& v) { s2.serialize_bytes(v); });
      });
  st.serialize_field(meta.symlinks,
      [](auto& s, auto const& vec) {
        s.serialize_vector(vec, [](auto& s2, auto const& v) { s2.serialize_bytes(v); });
      });

  // Filesystem parameters
  st.serialize_field(meta.timestamp_base, [](auto& s, uint64_t v) { s.serialize_u64(v); });
  st.serialize_field(meta.block_size, [](auto& s, uint32_t v) { s.serialize_u32(v); });
  st.serialize_field(meta.total_fs_size, [](auto& s, uint64_t v) { s.serialize_u64(v); });

  // Optional features
  st.serialize_field(meta.devices,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& vec) {
          s2.serialize_vector(vec, [](auto& s3, uint64_t v) { s3.serialize_u64(v); });
        });
      });
  st.serialize_field(meta.options,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& v) { s2.serialize_fs_options(v); });
      });

  // Directory entries (v2.3+)
  st.serialize_field(meta.dir_entries,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& vec) {
          s2.serialize_vector(vec, [](auto& s3, auto const& v) { s3.serialize_dir_entry(v); });
        });
      });
  st.serialize_field(meta.shared_files_table,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& vec) {
          s2.serialize_vector(vec, [](auto& s3, uint32_t v) { s3.serialize_u32(v); });
        });
      });
  st.serialize_field(meta.total_hardlink_size,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, uint64_t v) { s2.serialize_u64(v); });
      });

  // Metadata information
  st.serialize_field(meta.dwarfs_version,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& v) { s2.serialize_bytes(v); });
      });
  st.serialize_field(meta.create_timestamp,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, uint64_t v) { s2.serialize_u64(v); });
      });

  // Compact string storage (v2.3+)
  st.serialize_field(meta.compact_names,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& v) { s2.serialize_string_table(v); });
      });
  st.serialize_field(meta.compact_symlinks,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& v) { s2.serialize_string_table(v); });
      });

  // Path separator (v2.5+)
  st.serialize_field(meta.preferred_path_separator,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });

  // Features and categories (v2.5+)
  st.serialize_field(meta.features,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& set) {
          s2.serialize_set(set, [](auto& s3, auto const& v) { s3.serialize_bytes(v); });
        });
      });
  st.serialize_field(meta.category_names,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& vec) {
          s2.serialize_vector(vec, [](auto& s3, auto const& v) { s3.serialize_bytes(v); });
        });
      });
  st.serialize_field(meta.block_categories,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& vec) {
          s2.serialize_vector(vec, [](auto& s3, uint32_t v) { s3.serialize_u32(v); });
        });
      });

  // Performance caches (v2.5+)
  st.serialize_field(meta.reg_file_size_cache,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& v) { s2.serialize_inode_size_cache(v); });
      });

  // Category metadata (v2.5+)
  st.serialize_field(meta.category_metadata_json,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& vec) {
          s2.serialize_vector(vec, [](auto& s3, auto const& v) { s3.serialize_bytes(v); });
        });
      });
  st.serialize_field(meta.block_category_metadata,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& map) {
          s2.serialize_map(map,
              [](auto& s3, uint32_t k) { s3.serialize_u32(k); },
              [](auto& s3, uint32_t v) { s3.serialize_u32(v); });
        });
      });

  // Version history (v2.5+)
  st.serialize_field(meta.metadata_version_history,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& vec) {
          s2.serialize_vector(vec, [](auto& s3, auto const& v) { s3.serialize_history_entry(v); });
        });
      });

  // Sparse file support (v2.5+)
  st.serialize_field(meta.hole_block_index,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, uint32_t v) { s2.serialize_u32(v); });
      });
  st.serialize_field(meta.large_hole_size,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, auto const& vec) {
          s2.serialize_vector(vec, [](auto& s3, uint64_t v) { s3.serialize_u64(v); });
        });
      });
  st.serialize_field(meta.total_allocated_fs_size,
      [](auto& s, auto const& opt) {
        s.serialize_optional(opt, [](auto& s2, uint64_t v) { s2.serialize_u64(v); });
      });
}

// StructSerializer implementation

StructSerializer::StructSerializer(Serializer& ser,
                                   std::vector<std::unique_ptr<Layout>> const& fields)
    : ser_(ser)
    , fields_(fields)
    , field_idx_(0) {
}

void StructSerializer::skip_field() {
  if (field_idx_ >= fields_.size()) return;

  Layout const* field = fields_[field_idx_].get();
  field_idx_++;

  ser_.inline_pos_ += field->byte_size();
}

} // namespace dwarfs::metadata::legacy