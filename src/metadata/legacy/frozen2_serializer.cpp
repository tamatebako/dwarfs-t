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
 * Frozen2 Serializer - Main Orchestrator (Task 7)
 *
 * Simplified API for serializing domain::metadata to Frozen2 format.
 * Uses:
 * - SchemaBuilder (Task 4) to generate schema
 * - FrozenSchemaSerializer to encode schema to Thrift
 * - ValueEncoders (Task 6-7) to encode metadata values
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
  if (field1 && !domain_meta->chunks.empty()) {
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

  // Encode directories (field 2)
  auto* field2 = root_layout->fields.get(2);
  if (field2 && !domain_meta->directories.empty()) {
    auto* vector_layout = schema.layouts.get(field2->layout_id);
    if (!vector_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: vector layout not found");
    }

    auto* dir_layout = schema.layouts.get(builder.directory_layout_id());
    if (!dir_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: directory layout not found");
    }

    VectorEncoder vec_encoder;
    vec_encoder.encode_directories(
      writer, *vector_layout, *dir_layout, &domain_meta->directories);
  }

  // Encode inodes (field 3)
  auto* field3 = root_layout->fields.get(3);
  if (field3 && !domain_meta->inodes.empty()) {
    auto* vector_layout = schema.layouts.get(field3->layout_id);
    if (!vector_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: vector layout not found");
    }

    auto* inode_layout = schema.layouts.get(builder.inode_layout_id());
    if (!inode_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: inode layout not found");
    }

    VectorEncoder vec_encoder;
    vec_encoder.encode_inodes(
      writer, *vector_layout, *inode_layout, &domain_meta->inodes);
  }

  // Encode chunk_table (field 4)
  auto* field4 = root_layout->fields.get(4);
  if (field4 && !domain_meta->chunk_table.empty()) {
    auto* vector_layout = schema.layouts.get(field4->layout_id);
    if (!vector_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: vector layout not found");
    }

    VectorEncoder vec_encoder;
    vec_encoder.encode(writer, *vector_layout, &domain_meta->chunk_table);
  }

  // Encode uids (field 7)
  auto* field7 = root_layout->fields.get(7);
  if (field7 && !domain_meta->uids.empty()) {
    auto* vector_layout = schema.layouts.get(field7->layout_id);
    if (!vector_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: vector layout not found");
    }

    VectorEncoder vec_encoder;
    vec_encoder.encode(writer, *vector_layout, &domain_meta->uids);
  }

  // Encode gids (field 8)
  auto* field8 = root_layout->fields.get(8);
  if (field8 && !domain_meta->gids.empty()) {
    auto* vector_layout = schema.layouts.get(field8->layout_id);
    if (!vector_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: vector layout not found");
    }

    VectorEncoder vec_encoder;
    vec_encoder.encode(writer, *vector_layout, &domain_meta->gids);
  }

  // Encode modes (field 9)
  auto* field9 = root_layout->fields.get(9);
  if (field9 && !domain_meta->modes.empty()) {
    auto* vector_layout = schema.layouts.get(field9->layout_id);
    if (!vector_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: vector layout not found");
    }

    VectorEncoder vec_encoder;
    vec_encoder.encode(writer, *vector_layout, &domain_meta->modes);
  }

  // Encode timestamp_base (field 12)
  auto* field12 = root_layout->fields.get(12);
  if (field12 && domain_meta->timestamp_base != 0) {
    auto* u64_layout = schema.layouts.get(field12->layout_id);
    if (!u64_layout) {
      throw std::runtime_error("Frozen2Serializer::serialize: u64 layout not found");
    }

    ScalarEncoder scalar_encoder;
    uint64_t ts = domain_meta->timestamp_base;
    scalar_encoder.encode(writer, *u64_layout, &ts);
  }

  // Encode block_size (field 15)
  auto* field15 = root_layout->fields.get(15);
  if (field15 && domain_meta->block_size != 0) {
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
  if (field16 && domain_meta->total_fs_size != 0) {
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
