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

/**
 * Frozen2 Serializer - Main Orchestrator (Task 5)
 *
 * Simplified API for serializing domain::metadata to Frozen2 format.
 * Uses:
 * - SchemaBuilder (Task 4) to generate schema
 * - FrozenSchemaSerializer to encode schema to Thrift
 * - (Task 6) Metadata encoding to encode values
 */

#include "dwarfs/metadata/legacy/frozen2_serializer.h"
#include "dwarfs/metadata/legacy/frozen2_schema_builder.h"
#include "dwarfs/metadata/legacy/frozen_schema_serializer.h"
#include "dwarfs/metadata/domain/metadata.h"

#include <cstring>
#include <stdexcept>

namespace dwarfs::metadata::legacy {

std::vector<uint8_t> Frozen2Serializer::serialize(
    const void* metadata) const {

  if (metadata == nullptr) {
    throw std::invalid_argument("Frozen2Serializer::serialize: metadata is null");
  }

  auto* domain_meta = static_cast<const domain::metadata*>(metadata);

  // Step 1: Build Schema using SchemaBuilder
  SchemaBuilder builder;
  Schema schema = builder.build_from(*domain_meta);

  // Step 2: Serialize Schema to Thrift Compact Protocol
  std::vector<uint8_t> schema_bytes =
    FrozenSchemaSerializer::serialize(schema);

  // Step 3: Create frozen metadata data
  // For now (Task 5), just combine schema with empty frozen data
  // Task 6 will add metadata encoding for chunks, scalars, etc.
  std::vector<uint8_t> output;
  output.reserve(8 + schema_bytes.size());

  // Size prefix (little-endian uint64, matches x86/x86_64)
  uint64_t total_size = schema_bytes.size();
  uint8_t size_bytes[8];
  std::memcpy(size_bytes, &total_size, 8);
  output.insert(output.end(), size_bytes, size_bytes + 8);

  // Schema section (Thrift Compact Protocol)
  output.insert(output.end(), schema_bytes.begin(), schema_bytes.end());

  // Frozen metadata data will be added in Task 6

  return output;
}

} // namespace dwarfs::metadata::legacy
