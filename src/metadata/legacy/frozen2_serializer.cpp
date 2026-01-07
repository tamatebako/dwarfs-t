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

/**
 * Frozen2 Serializer - Entry Point
 *
 * Ported from: dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs
 *
 * Orchestrates modular components:
 * - Layout builders (frozen2_layout_builder)
 * - Schema converter (frozen2_schema_converter)
 * - Value serializer (frozen2_value_serializer)
 */

#include "dwarfs/metadata/legacy/frozen2_serializer.h"

#include <stdexcept>

#include "dwarfs/metadata/legacy/frozen2_layout_builder.h"
#include "dwarfs/metadata/legacy/frozen2_schema_converter.h"
#include "dwarfs/metadata/legacy/frozen2_value_serializer.h"

namespace dwarfs::metadata::legacy {

std::pair<Schema, std::vector<uint8_t>>
Frozen2Serializer::serialize(domain::metadata const& meta) {
  // Build layout from domain metadata
  auto layout = build_metadata(meta);

  // Finalize layout - this converts Collections to Structs
  auto root_size = layout->finish();
  if (!root_size) {
    throw std::runtime_error("Root layout too large to serialize");
  }

  // Convert layout to schema layouts
  std::vector<SchemaLayout> schema_layouts;
  auto root_layout_id = cvt_layout(layout.get(), schema_layouts);

  if (!root_layout_id) {
    throw std::runtime_error("Root layout is None");
  }

  // Set root layout's size field
  schema_layouts[*root_layout_id].size = static_cast<int32_t>(*root_size);

  // Build schema using DenseMap
  Schema schema;
  schema.file_version = 1;
  schema.relax_type_checks = true;
  schema.root_layout = *root_layout_id;

  // Insert all schema layouts into DenseMap
  for (size_t i = 0; i < schema_layouts.size(); ++i) {
    schema.layouts.insert(static_cast<int16_t>(i), std::move(schema_layouts[i]));
  }

  // Allocate buffer for serialized data
  std::vector<uint8_t> buf(*root_size, 0);

  // Serialize metadata to buffer
  Serializer ser(layout.get(), buf, 0, 0);
  ser.serialize_metadata(meta);

  return {schema, buf};
}

} // namespace dwarfs::metadata::legacy
