/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     DwarFS Implementation
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

#include <limits>
#include <stdexcept>
#include <iostream>

#include "dwarfs/metadata/legacy/frozen2_schema_builder.h"
#include "dwarfs/metadata/domain/inode_data.h"

namespace dwarfs::metadata::legacy {

SchemaLayout SchemaBuilder::build_chunk_layout(int16_t u32_layout_id) {
  SchemaLayout layout;
  layout.type_name = "chunk";
  layout.bits = 96; // 3 u32 fields @ 32 bits each = 96 bits total

  // Field 1: block (u32)
  SchemaField field1;
  field1.layout_id = u32_layout_id;
  field1.offset = 0;  // 0 bytes
  layout.fields.insert(1, field1);

  // Field 2: offset (u32)
  SchemaField field2;
  field2.layout_id = u32_layout_id;
  field2.offset = 4;  // 4 bytes
  layout.fields.insert(2, field2);

  // Field 3: size (u32)
  SchemaField field3;
  field3.layout_id = u32_layout_id;
  field3.offset = 8;  // 8 bytes
  layout.fields.insert(3, field3);

  return layout;
}

SchemaLayout SchemaBuilder::build_directory_layout(int16_t u32_layout_id) {
  SchemaLayout layout;
  layout.type_name = "directory";
  layout.bits = 96; // 3 u32 fields @ 32 bits each = 96 bits total

  // Field 1: parent_entry (u32)
  SchemaField field1;
  field1.layout_id = u32_layout_id;
  field1.offset = 0;  // 0 bytes
  layout.fields.insert(1, field1);

  // Field 2: first_entry (u32)
  SchemaField field2;
  field2.layout_id = u32_layout_id;
  field2.offset = 4;  // 4 bytes
  layout.fields.insert(2, field2);

  // Field 3: self_entry (u32)
  SchemaField field3;
  field3.layout_id = u32_layout_id;
  field3.offset = 8;  // 8 bytes
  layout.fields.insert(3, field3);

  return layout;
}

SchemaLayout SchemaBuilder::build_inode_layout(int16_t u32_layout_id, int16_t u64_layout_id) {
  SchemaLayout layout;
  layout.type_name = "inode_data";
  // inode_data has:
  // - 3 u32 fields (mode_index, owner_index, group_index) = 12 bytes
  // - 8 u64 fields (atime_offset, mtime_offset, ctime_offset, btime_offset,
  //                 atime_subsec, mtime_subsec, ctime_subsec, btime_subsec) = 64 bytes
  // - 1 u32 field (nlink_minus_one) = 4 bytes
  // Total: 80 bytes = 640 bits
  layout.bits = 640;

  int offset_bytes = 0;

  // Field 1: mode_index (u32)
  SchemaField field1;
  field1.layout_id = u32_layout_id;
  field1.offset = offset_bytes;
  layout.fields.insert(1, field1);
  offset_bytes += 4;  // 4 bytes for u32

  // Field 2: owner_index (u32)
  SchemaField field2;
  field2.layout_id = u32_layout_id;
  field2.offset = offset_bytes;
  layout.fields.insert(2, field2);
  offset_bytes += 4;  // 4 bytes for u32

  // Field 3: group_index (u32)
  SchemaField field3;
  field3.layout_id = u32_layout_id;
  field3.offset = offset_bytes;
  layout.fields.insert(3, field3);
  offset_bytes += 4;  // 4 bytes for u32

  // Field 4: atime_offset (u64)
  SchemaField field4;
  field4.layout_id = u64_layout_id;
  field4.offset = offset_bytes;
  layout.fields.insert(4, field4);
  offset_bytes += 8;  // 8 bytes for u64

  // Field 5: mtime_offset (u64)
  SchemaField field5;
  field5.layout_id = u64_layout_id;
  field5.offset = offset_bytes;
  layout.fields.insert(5, field5);
  offset_bytes += 8;  // 8 bytes for u64

  // Field 6: ctime_offset (u64)
  SchemaField field6;
  field6.layout_id = u64_layout_id;
  field6.offset = offset_bytes;
  layout.fields.insert(6, field6);
  offset_bytes += 8;  // 8 bytes for u64

  // Field 7: btime_offset (u64)
  SchemaField field7;
  field7.layout_id = u64_layout_id;
  field7.offset = offset_bytes;
  layout.fields.insert(7, field7);
  offset_bytes += 8;  // 8 bytes for u64

  // Field 8: atime_subsec (u64)
  SchemaField field8;
  field8.layout_id = u64_layout_id;
  field8.offset = offset_bytes;
  layout.fields.insert(8, field8);
  offset_bytes += 8;  // 8 bytes for u64

  // Field 9: mtime_subsec (u64)
  SchemaField field9;
  field9.layout_id = u64_layout_id;
  field9.offset = offset_bytes;
  layout.fields.insert(9, field9);
  offset_bytes += 8;  // 8 bytes for u64

  // Field 10: ctime_subsec (u64)
  SchemaField field10;
  field10.layout_id = u64_layout_id;
  field10.offset = offset_bytes;
  layout.fields.insert(10, field10);
  offset_bytes += 8;  // 8 bytes for u64

  // Field 11: btime_subsec (u64)
  SchemaField field11;
  field11.layout_id = u64_layout_id;
  field11.offset = offset_bytes;
  layout.fields.insert(11, field11);
  offset_bytes += 8;  // 8 bytes for u64

  // Field 12: nlink_minus_one (u32)
  SchemaField field12;
  field12.layout_id = u32_layout_id;
  field12.offset = offset_bytes;
  layout.fields.insert(12, field12);

  return layout;
}

Schema SchemaBuilder::build_from(domain::metadata const& meta) {
  // Use meta to determine which fields to include in schema
  (void)meta;  // Check for each field whether it's non-empty

  // Check for overflow before allocating IDs
  if (next_layout_id_ > std::numeric_limits<int16_t>::max() - 20) {
    throw std::runtime_error("SchemaBuilder: layout ID overflow");
  }

  Schema schema;
  schema.relax_type_checks = true;
  schema.file_version = 1;

  // Allocate layout IDs
  int16_t u32_layout_id = next_layout_id_++;
  int16_t u64_layout_id = next_layout_id_++;

  // Create primitive layouts
  SchemaLayout u32_layout;
  u32_layout.bits = 32;
  layouts_.insert(u32_layout_id, u32_layout);

  SchemaLayout u64_layout;
  u64_layout.bits = 64;
  layouts_.insert(u64_layout_id, u64_layout);

  // Helper to create a vector layout for a specific element type
  auto create_vector_layout = [&](int16_t element_layout_id, const char* name) -> int16_t {
    int16_t vector_layout_id = next_layout_id_++;
    SchemaLayout vector_layout;
    vector_layout.bits = 64;
    vector_layout.type_name = name;

    // Field 1: distance (u32)
    SchemaField vec_field1;
    vec_field1.layout_id = u32_layout_id;
    vec_field1.offset = 0;  // 0 bytes
    vector_layout.fields.insert(1, vec_field1);

    // Field 2: length (u32)
    SchemaField vec_field2;
    vec_field2.layout_id = u32_layout_id;
    vec_field2.offset = 4;  // 4 bytes (32 bits)
    vector_layout.fields.insert(2, vec_field2);

    // Field 3: element layout (this is crucial for deserialization!)
    SchemaField vec_field3;
    vec_field3.layout_id = element_layout_id;
    vec_field3.offset = 8;  // 8 bytes (just a placeholder, not actually used)
    vector_layout.fields.insert(3, vec_field3);

    layouts_.insert(vector_layout_id, vector_layout);
    return vector_layout_id;
  };

  // Create element layouts
  int16_t chunk_layout_id = next_layout_id_++;
  layouts_.insert(chunk_layout_id, build_chunk_layout(u32_layout_id));
  chunk_layout_id_ = chunk_layout_id;

  int16_t directory_layout_id = next_layout_id_++;
  layouts_.insert(directory_layout_id, build_directory_layout(u32_layout_id));
  directory_layout_id_ = directory_layout_id;

  int16_t inode_layout_id = next_layout_id_++;
  layouts_.insert(inode_layout_id, build_inode_layout(u32_layout_id, u64_layout_id));
  inode_layout_id_ = inode_layout_id;

  // Create vector layouts for each element type
  int16_t chunk_vector_layout_id = create_vector_layout(chunk_layout_id, "vector<chunk>");
  int16_t directory_vector_layout_id = create_vector_layout(directory_layout_id, "vector<directory>");
  int16_t inode_vector_layout_id = create_vector_layout(inode_layout_id, "vector<inode_data>");
  int16_t u32_vector_layout_id = create_vector_layout(u32_layout_id, "vector<u32>");

  // Allocate metadata root layout ID
  int16_t metadata_layout_id = next_layout_id_++;

  // Create metadata root layout with all fields
  SchemaLayout metadata_layout;

  // Track running offset (fields are packed, no gaps for empty fields)
  int offset_bytes = 0;
  int field_id = 1;

  // Field 1: chunks (vector of chunk)
  SchemaField field1;
  field1.layout_id = chunk_vector_layout_id;
  field1.offset = offset_bytes;
  metadata_layout.fields.insert(field_id++, field1);
  offset_bytes += 8;  // Each vector field is 8 bytes (distance + length)

  // Field 2: directories (vector of directory)
  if (!meta.directories.empty()) {
    SchemaField field2;
    field2.layout_id = directory_vector_layout_id;
    field2.offset = offset_bytes;
    metadata_layout.fields.insert(field_id++, field2);
    offset_bytes += 8;
  }

  // Field 3: inodes (vector of inode_data)
  if (!meta.inodes.empty()) {
    SchemaField field3;
    field3.layout_id = inode_vector_layout_id;
    field3.offset = offset_bytes;
    metadata_layout.fields.insert(field_id++, field3);
    offset_bytes += 8;
  }

  // Field 4: chunk_table (vector of u32)
  if (!meta.chunk_table.empty()) {
    SchemaField field4;
    field4.layout_id = u32_vector_layout_id;
    field4.offset = offset_bytes;
    metadata_layout.fields.insert(field_id++, field4);
    offset_bytes += 8;
  }

  // Field 5: entry_table_v2_2 (not in this implementation)

  // Field 6: symlink_table (not in this implementation)

  // Field 7: uids (vector of u32)
  if (!meta.uids.empty()) {
    SchemaField field7;
    field7.layout_id = u32_vector_layout_id;
    field7.offset = offset_bytes;
    metadata_layout.fields.insert(7, field7);  // Always use field ID 7 for uids
    offset_bytes += 8;
  }

  // Field 8: gids (vector of u32)
  if (!meta.gids.empty()) {
    SchemaField field8;
    field8.layout_id = u32_vector_layout_id;
    field8.offset = offset_bytes;
    metadata_layout.fields.insert(8, field8);  // Always use field ID 8 for gids
    offset_bytes += 8;
  }

  // Field 9: modes (vector of u32)
  if (!meta.modes.empty()) {
    SchemaField field9;
    field9.layout_id = u32_vector_layout_id;
    field9.offset = offset_bytes;
    metadata_layout.fields.insert(9, field9);  // Always use field ID 9 for modes
    offset_bytes += 8;
  }

  // Field 10: names (vector of string) - use u32_vector_layout as placeholder
  if (!meta.names.empty()) {
    SchemaField field10;
    field10.layout_id = u32_vector_layout_id;
    field10.offset = offset_bytes;
    metadata_layout.fields.insert(10, field10);  // Always use field ID 10 for names
    offset_bytes += 8;
  }

  // Field 11: symlinks (not in this implementation)

  // Field 12: timestamp_base (u64)
  if (meta.timestamp_base != 0) {
    SchemaField field12;
    field12.layout_id = u64_layout_id;
    field12.offset = offset_bytes;
    metadata_layout.fields.insert(12, field12);
    offset_bytes += 8;  // u64 is 8 bytes
  }

  // Field 13: devices (not in this implementation)
  // Field 14: options (not in this implementation)

  // Field 15: block_size (u32)
  if (meta.block_size != 0) {
    SchemaField field15;
    field15.layout_id = u32_layout_id;
    field15.offset = offset_bytes;
    metadata_layout.fields.insert(15, field15);
    offset_bytes += 4;  // u32 is 4 bytes
  }

  // Field 16: total_fs_size (u64)
  if (meta.total_fs_size != 0) {
    SchemaField field16;
    field16.layout_id = u64_layout_id;
    field16.offset = offset_bytes;
    metadata_layout.fields.insert(16, field16);
    offset_bytes += 8;  // u64 is 8 bytes
  }

  layouts_.insert(metadata_layout_id, metadata_layout);
  schema.layouts = layouts_;
  schema.root_layout = metadata_layout_id;

  return schema;
}

} // namespace dwarfs::metadata::legacy
