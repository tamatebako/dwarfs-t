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

#include "dwarfs/metadata/legacy/frozen2_schema_builder.h"
#include "dwarfs/metadata/domain/inode_data.h"

namespace dwarfs::metadata::legacy {

SchemaLayout SchemaBuilder::build_chunk_layout(int16_t u32_layout_id) {
  SchemaLayout layout;
  layout.type_name = "chunk";
  layout.bits = 96; // 3 u32 fields @ 32 bits each

  // Field 1: block (u32)
  SchemaField field1;
  field1.layout_id = u32_layout_id;
  field1.offset = 0;
  layout.fields.insert(1, field1);

  // Field 2: offset (u32)
  SchemaField field2;
  field2.layout_id = u32_layout_id;
  field2.offset = 32;
  layout.fields.insert(2, field2);

  // Field 3: size (u32)
  SchemaField field3;
  field3.layout_id = u32_layout_id;
  field3.offset = 64;
  layout.fields.insert(3, field3);

  return layout;
}

SchemaLayout SchemaBuilder::build_directory_layout(int16_t u32_layout_id) {
  SchemaLayout layout;
  layout.type_name = "directory";
  layout.bits = 96; // 3 u32 fields @ 32 bits each

  // Field 1: parent_entry (u32)
  SchemaField field1;
  field1.layout_id = u32_layout_id;
  field1.offset = 0;
  layout.fields.insert(1, field1);

  // Field 2: first_entry (u32)
  SchemaField field2;
  field2.layout_id = u32_layout_id;
  field2.offset = 32;
  layout.fields.insert(2, field2);

  // Field 3: self_entry (u32)
  SchemaField field3;
  field3.layout_id = u32_layout_id;
  field3.offset = 64;
  layout.fields.insert(3, field3);

  return layout;
}

SchemaLayout SchemaBuilder::build_inode_layout(int16_t u32_layout_id, int16_t u64_layout_id) {
  SchemaLayout layout;
  layout.type_name = "inode_data";
  // inode_data has:
  // - 3 u32 fields (mode_index, owner_index, group_index)
  // - 8 u64 fields (atime_offset, mtime_offset, ctime_offset, btime_offset,
  //                 atime_subsec, mtime_subsec, ctime_subsec, btime_subsec)
  // - 1 u32 field (nlink_minus_one)
  layout.bits = (3 * 32) + (8 * 64) + 32; // 32 + 512 + 32 = 576 bits

  int16_t offset = 0;

  // Field 1: mode_index (u32)
  SchemaField field1;
  field1.layout_id = u32_layout_id;
  field1.offset = offset;
  layout.fields.insert(1, field1);
  offset += 32;

  // Field 2: owner_index (u32)
  SchemaField field2;
  field2.layout_id = u32_layout_id;
  field2.offset = offset;
  layout.fields.insert(2, field2);
  offset += 32;

  // Field 3: group_index (u32)
  SchemaField field3;
  field3.layout_id = u32_layout_id;
  field3.offset = offset;
  layout.fields.insert(3, field3);
  offset += 32;

  // Field 4: atime_offset (u64)
  SchemaField field4;
  field4.layout_id = u64_layout_id;
  field4.offset = offset;
  layout.fields.insert(4, field4);
  offset += 64;

  // Field 5: mtime_offset (u64)
  SchemaField field5;
  field5.layout_id = u64_layout_id;
  field5.offset = offset;
  layout.fields.insert(5, field5);
  offset += 64;

  // Field 6: ctime_offset (u64)
  SchemaField field6;
  field6.layout_id = u64_layout_id;
  field6.offset = offset;
  layout.fields.insert(6, field6);
  offset += 64;

  // Field 7: btime_offset (u64)
  SchemaField field7;
  field7.layout_id = u64_layout_id;
  field7.offset = offset;
  layout.fields.insert(7, field7);
  offset += 64;

  // Field 8: atime_subsec (u64)
  SchemaField field8;
  field8.layout_id = u64_layout_id;
  field8.offset = offset;
  layout.fields.insert(8, field8);
  offset += 64;

  // Field 9: mtime_subsec (u64)
  SchemaField field9;
  field9.layout_id = u64_layout_id;
  field9.offset = offset;
  layout.fields.insert(9, field9);
  offset += 64;

  // Field 10: ctime_subsec (u64)
  SchemaField field10;
  field10.layout_id = u64_layout_id;
  field10.offset = offset;
  layout.fields.insert(10, field10);
  offset += 64;

  // Field 11: btime_subsec (u64)
  SchemaField field11;
  field11.layout_id = u64_layout_id;
  field11.offset = offset;
  layout.fields.insert(11, field11);
  offset += 64;

  // Field 12: nlink_minus_one (u32)
  SchemaField field12;
  field12.layout_id = u32_layout_id;
  field12.offset = offset;
  layout.fields.insert(12, field12);

  return layout;
}

Schema SchemaBuilder::build_from(domain::metadata const& meta) {
  // Use meta to determine which fields to include in schema
  (void)meta;  // Check for each field whether it's non-empty

  // Check for overflow before allocating IDs
  if (next_layout_id_ > std::numeric_limits<int16_t>::max() - 10) {
    throw std::runtime_error("SchemaBuilder: layout ID overflow");
  }

  Schema schema;
  schema.relax_type_checks = true;
  schema.file_version = 1;

  // Allocate layout IDs
  int16_t u32_layout_id = next_layout_id_++;
  int16_t u64_layout_id = next_layout_id_++;
  int16_t vector_layout_id = next_layout_id_++;
  int16_t chunk_layout_id = next_layout_id_++;
  int16_t directory_layout_id = next_layout_id_++;
  int16_t inode_layout_id = next_layout_id_++;
  int16_t metadata_layout_id = next_layout_id_++;

  // Create primitive layouts
  SchemaLayout u32_layout;
  u32_layout.bits = 32;
  layouts_.insert(u32_layout_id, u32_layout);

  SchemaLayout u64_layout;
  u64_layout.bits = 64;
  layouts_.insert(u64_layout_id, u64_layout);

  // Create vector layout (distance + length fields)
  SchemaLayout vector_layout;
  vector_layout.bits = 64;
  SchemaField vec_field1;
  vec_field1.layout_id = u32_layout_id;
  vec_field1.offset = 0;
  vector_layout.fields.insert(1, vec_field1);
  SchemaField vec_field2;
  vec_field2.layout_id = u32_layout_id;
  vec_field2.offset = 32;
  vector_layout.fields.insert(2, vec_field2);
  layouts_.insert(vector_layout_id, vector_layout);

  // Create chunk layout (block + offset + size)
  layouts_.insert(chunk_layout_id, build_chunk_layout(u32_layout_id));
  chunk_layout_id_ = chunk_layout_id;  // Store for serializer access

  // Create directory layout (parent_entry + first_entry + self_entry)
  layouts_.insert(directory_layout_id, build_directory_layout(u32_layout_id));
  directory_layout_id_ = directory_layout_id;  // Store for serializer access

  // Create inode layout (mode_index + owner_index + group_index + timestamps + nlink)
  layouts_.insert(inode_layout_id, build_inode_layout(u32_layout_id, u64_layout_id));
  inode_layout_id_ = inode_layout_id;  // Store for serializer access

  // Create metadata root layout with all fields
  SchemaLayout metadata_layout;

  // Field 1: chunks (vector of chunk)
  SchemaField field1;
  field1.layout_id = vector_layout_id;
  field1.offset = 0;
  metadata_layout.fields.insert(1, field1);

  // Field 2: directories (vector of directory)
  if (!meta.directories.empty()) {
    SchemaField field2;
    field2.layout_id = vector_layout_id;
    field2.offset = 64;
    metadata_layout.fields.insert(2, field2);
  }

  // Field 3: inodes (vector of inode_data)
  if (!meta.inodes.empty()) {
    SchemaField field3;
    field3.layout_id = vector_layout_id;
    field3.offset = 2 * 64;
    metadata_layout.fields.insert(3, field3);
  }

  // Field 4: chunk_table (vector of u32)
  if (!meta.chunk_table.empty()) {
    SchemaField field4;
    field4.layout_id = vector_layout_id;
    field4.offset = 3 * 64;
    metadata_layout.fields.insert(4, field4);
  }

  // Field 7: uids (vector of u32)
  if (!meta.uids.empty()) {
    SchemaField field7;
    field7.layout_id = vector_layout_id;
    field7.offset = 6 * 64;
    metadata_layout.fields.insert(7, field7);
  }

  // Field 8: gids (vector of u32)
  if (!meta.gids.empty()) {
    SchemaField field8;
    field8.layout_id = vector_layout_id;
    field8.offset = 7 * 64;
    metadata_layout.fields.insert(8, field8);
  }

  // Field 9: modes (vector of u32)
  if (!meta.modes.empty()) {
    SchemaField field9;
    field9.layout_id = vector_layout_id;
    field9.offset = 8 * 64;
    metadata_layout.fields.insert(9, field9);
  }

  // Field 10: names (vector of string)
  if (!meta.names.empty()) {
    SchemaField field10;
    field10.layout_id = vector_layout_id;
    field10.offset = 9 * 64;
    metadata_layout.fields.insert(10, field10);
  }

  // Field 12: timestamp_base (u64)
  if (meta.timestamp_base != 0) {
    SchemaField field12;
    field12.layout_id = u64_layout_id;
    field12.offset = 11 * 64;
    metadata_layout.fields.insert(12, field12);
  }

  // Field 15: block_size (u32)
  if (meta.block_size != 0) {
    SchemaField field15;
    field15.layout_id = u32_layout_id;
    field15.offset = 14 * 64;
    metadata_layout.fields.insert(15, field15);
  }

  // Field 16: total_fs_size (u64)
  if (meta.total_fs_size != 0) {
    SchemaField field16;
    field16.layout_id = u64_layout_id;
    field16.offset = 15 * 64;
    metadata_layout.fields.insert(16, field16);
  }

  layouts_.insert(metadata_layout_id, metadata_layout);
  schema.layouts = layouts_;
  schema.root_layout = metadata_layout_id;

  return schema;
}

} // namespace dwarfs::metadata::legacy
