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

Schema SchemaBuilder::build_from(domain::metadata const& meta) {
  // TODO: Task 6-7 will analyze meta to generate complete schema
  // For now, this creates a minimal stub schema
  (void)meta;  // Mark as intentionally unused for now

  // Check for overflow before allocating IDs
  if (next_layout_id_ > std::numeric_limits<int16_t>::max() - 10) {
    throw std::runtime_error("SchemaBuilder: layout ID overflow");
  }

  Schema schema;
  schema.relax_type_checks = true;
  schema.file_version = 1;

  int16_t u32_layout_id = next_layout_id_++;
  int16_t metadata_layout_id = next_layout_id_++;
  int16_t chunk_layout_id = next_layout_id_++;
  int16_t vector_layout_id = next_layout_id_++;

  // Create u32 layout (used for scalars)
  SchemaLayout u32_layout;
  u32_layout.bits = 32;
  layouts_.insert(u32_layout_id, u32_layout);

  // Create vector layout
  SchemaLayout vector_layout;
  vector_layout.bits = 64;
  layouts_.insert(vector_layout_id, vector_layout);

  // Create chunk layout
  layouts_.insert(chunk_layout_id, build_chunk_layout(u32_layout_id));

  // Create metadata root layout
  SchemaLayout metadata_layout;
  metadata_layout.size = 0;

  // Field 1: chunks (vector)
  SchemaField chunks_field;
  chunks_field.layout_id = chunk_layout_id;
  chunks_field.offset = 0;
  metadata_layout.fields.insert(1, chunks_field);

  layouts_.insert(metadata_layout_id, metadata_layout);
  schema.layouts = layouts_;
  schema.root_layout = metadata_layout_id;

  return schema;
}

} // namespace dwarfs::metadata::legacy
