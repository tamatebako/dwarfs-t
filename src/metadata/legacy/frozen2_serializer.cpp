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
 * Frozen2 Serializer - Main Orchestrator (Task 6)
 *
 * Simplified API for serializing domain::metadata to Frozen2 format.
 * Uses:
 * - SchemaBuilder (Task 4) to generate schema
 * - FrozenSchemaSerializer to encode schema to Thrift
 * - ValueEncoders (Task 6) to encode metadata values
 */

#include "dwarfs/metadata/legacy/frozen2_serializer.h"
#include "dwarfs/metadata/legacy/frozen2_schema_builder.h"
#include "dwarfs/metadata/legacy/frozen_schema_serializer.h"
#include "dwarfs/metadata/legacy/frozen_writer.h"
#include "dwarfs/metadata/legacy/value_encoders.h"
#include "dwarfs/metadata/domain/metadata.h"

#include <cstring>
#include <fmt/core.h>
#include <stdexcept>
#include <vector>

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

  // Step 3: Encode metadata to frozen bytes
  std::vector<uint8_t> frozen_buffer(65536); // 64KB initial buffer
  FrozenWriter writer{std::span<uint8_t>(frozen_buffer)};

  // Get root layout and field layouts
  auto* root_layout = schema.layouts.get(schema.root_layout);
  if (!root_layout) {
    throw std::runtime_error("Frozen2Serializer::serialize: root layout not found");
  }

  // Encode chunks (field 1)
  auto* field1 = root_layout->fields.get(1);
  if (field1) {
    auto* vector_layout = schema.layouts.get(field1->layout_id);
    if (!vector_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: vector layout not found");
    }

    auto* chunk_layout = schema.layouts.get(builder.chunk_layout_id());
    if (!chunk_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: chunk layout not found");
    }

    VectorEncoder vec_encoder;
    vec_encoder.encode_with_element_layout(
      writer, *vector_layout, *chunk_layout, &domain_meta->chunks);
  }

  // Encode block_size (field 15)
  auto* field15 = root_layout->fields.get(15);
  if (field15) {
    auto* u32_layout = schema.layouts.get(field15->layout_id);
    if (!u32_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: u32 layout not found");
    }

    ScalarEncoder scalar_encoder;
    uint32_t block_size = domain_meta->block_size;
    scalar_encoder.encode(writer, *u32_layout, &block_size);
  }

  // Encode total_fs_size (field 16)
  auto* field16 = root_layout->fields.get(16);
  if (field16) {
    auto* u64_layout = schema.layouts.get(field16->layout_id);
    if (!u64_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: u64 layout not found");
    }

    ScalarEncoder scalar_encoder;
    uint64_t total_fs_size = domain_meta->total_fs_size;
    scalar_encoder.encode(writer, *u64_layout, &total_fs_size);
  }

  writer.finalize();

  // Calculate actual frozen data size
  size_t frozen_size = (writer.current_bit_offset() + 7) / 8 + writer.storage_size();

  // Buffer overflow check
  if (frozen_size > frozen_buffer.size()) {
    throw std::runtime_error(
        fmt::format("Frozen2Serializer::serialize: buffer overflow - "
                    "frozen_size {} exceeds buffer_size {}",
                    frozen_size, frozen_buffer.size()));
  }

  // Validate frozen_size before using
  if (frozen_size == 0 && (writer.current_bit_offset() > 0 || writer.storage_size() > 0)) {
    throw std::runtime_error("Frozen2Serializer::serialize: invalid frozen_size calculation");
  }

  // Step 4: Combine with size prefix
  std::vector<uint8_t> output;
  output.reserve(8 + schema_bytes.size() + frozen_size);

  uint64_t total_size = schema_bytes.size() + frozen_size;
  uint8_t size_bytes[8];
  std::memcpy(size_bytes, &total_size, 8);
  output.insert(output.end(), size_bytes, size_bytes + 8);

  output.insert(output.end(), schema_bytes.begin(), schema_bytes.end());
  output.insert(output.end(), frozen_buffer.begin(),
                frozen_buffer.begin() + frozen_size);

  return output;
}

} // namespace dwarfs::metadata::legacy
