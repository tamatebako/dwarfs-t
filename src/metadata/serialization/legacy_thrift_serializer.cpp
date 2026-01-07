/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhx.io)
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

#include "dwarfs/metadata/serialization/legacy_thrift_serializer.h"
#include "dwarfs/metadata/serialization/serializer_registry.h"
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/legacy/frozen2_serializer.h"
#include "dwarfs/metadata/legacy/frozen2_deserializer.h"
#include "dwarfs/metadata/legacy/frozen_schema_serializer.h"

#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace dwarfs::metadata::serialization {

std::vector<uint8_t> LegacyThriftSerializer::serialize(
    const void* metadata) const {

  if (metadata == nullptr) {
    throw std::invalid_argument("Cannot serialize null metadata");
  }

  // Use Frozen2Serializer for Homebrew compatibility
  // (Task 5: Main orchestrator using SchemaBuilder + FrozenSchemaSerializer)
  legacy::Frozen2Serializer frozen2;
  return frozen2.serialize(metadata);
}

std::unique_ptr<void, void(*)(void*)> LegacyThriftSerializer::deserialize(
    const std::vector<uint8_t>& data) const {

  if (data.empty()) {
    throw std::invalid_argument("Cannot deserialize empty data");
  }

  /**
   * DwarFS v0.14.1 Legacy Thrift Format (Homebrew):
   *
   * Structure:
   *   [8 bytes]  Size prefix (little-endian uint64)
   *   [N bytes]  Schema section (Thrift Compact Protocol)
   *   [M bytes]  Metadata section (Frozen2 bit-packed)
   *
   * We need to:
   *   1. Skip size prefix (first 8 bytes)
   *   2. Deserialize schema using FrozenSchemaSerializer
   *   3. Find where frozen data starts (after schema)
   *   4. Deserialize metadata using Frozen2Deserializer
   */

  // Check minimum size (8-byte prefix + some schema/data)
  if (data.size() < 16) {
    throw std::runtime_error("Legacy Thrift data too small (< 16 bytes)");
  }

  // Skip 8-byte size prefix
  std::span<uint8_t const> metadata_bytes(data.data() + 8, data.size() - 8);

  // Step 1: Deserialize schema (Thrift Compact)
  // The schema tells us how to interpret the bit-packed frozen data
  // Track bytes consumed to find where frozen data starts
  size_t schema_size = 0;
  auto schema = legacy::FrozenSchemaSerializer::deserialize(metadata_bytes, schema_size);

  if (metadata_bytes.size() < schema_size) {
    throw std::runtime_error(
        "Incomplete schema (data size: " +
        std::to_string(metadata_bytes.size()) +
        ", schema size: " + std::to_string(schema_size) + ")");
  }

  // Step 2: Extract frozen bytes (after schema)
  // Note: Task 5 only serializes schema, no frozen data yet
  // Task 6 will add metadata encoding for chunks, scalars, etc.
  std::span<uint8_t const> frozen_bytes;
  if (metadata_bytes.size() > schema_size) {
    frozen_bytes = std::span<uint8_t const>(
        metadata_bytes.data() + schema_size,
        metadata_bytes.size() - schema_size);
  }

  // Step 3: Deserialize using Frozen2 format
  // If no frozen data, create empty metadata (Task 5 limitation)
  std::unique_ptr<domain::metadata> domain_meta;
  if (frozen_bytes.empty()) {
    // Task 5: No frozen data yet, return empty metadata
    domain_meta = std::make_unique<domain::metadata>();
  } else {
    // Task 6+: Deserialize frozen data
    domain_meta = std::make_unique<domain::metadata>(
        legacy::Frozen2Deserializer::deserialize(schema, frozen_bytes));
  }

  // Return with custom deleter
  return std::unique_ptr<void, void(*)(void*)>(
      domain_meta.release(),
      [](void* ptr) { delete static_cast<domain::metadata*>(ptr); }
  );
}

// Registration function called by init_serializers()
void register_legacy_thrift_serializer() {
  static SerializerRegistration<LegacyThriftSerializer> registration{
    "Legacy Thrift",
    {},  // No magic bytes (fallback detection)
    50,  // Medium priority (lower than FlatBuffers 120)
    SerializationFormat::LEGACY_THRIFT
  };
}

} // namespace dwarfs::metadata::serialization