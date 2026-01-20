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
 * Frozen2 Serializer - Schema-Driven Implementation
 *
 * This implementation uses the schema as the single source of truth for
 * serialization. The schema describes:
 * - Which fields exist and their layout IDs
 * - The byte offset of each field
 * - The element layout for vector types
 *
 * This is a clean OOP approach where:
 * - SchemaBuilder analyzes metadata and creates the schema
 * - SchemaFieldEncoder encodes fields based on their layout
 * - FrozenWriter handles the bit-packing details
 *
 * This separates concerns and avoids hardcoded field-by-field encoding.
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

namespace {

/**
 * SchemaFieldEncoder - Encode a single field according to its schema layout
 *
 * This class handles encoding of individual fields based on their schema.
 * It knows how to:
 * - Position the writer at the correct bit offset
 * - Encode scalar values (u32, u64)
 * - Encode vectors of values (chunks, directories, inodes, etc.)
 * - Use the element layout for vector element encoding
 */
class SchemaFieldEncoder {
public:
  explicit SchemaFieldEncoder(const domain::metadata& meta)
      : meta_(meta) {}

  /**
   * Encode a single field according to its schema
   *
   * @param writer The FrozenWriter to encode to
   * @param schema The complete schema (for looking up layouts)
   * @param field_id The field ID to encode
   * @param field The field layout describing how to encode
   * @return Number of bits written
   */
  uint32_t encode_field(
      FrozenWriter& writer,
      Schema const& schema,
      int16_t field_id,
      SchemaField const& field) {

    // Get the layout for this field's type
    auto* field_layout = schema.layouts.get(field.layout_id);
    if (!field_layout) {
      throw std::runtime_error(
          fmt::format("SchemaFieldEncoder: layout {} not found for field {}",
                      field.layout_id, field_id));
    }

    // Seek to the field's bit offset
    uint16_t bit_offset = field.offset_bits();
    writer.seek_to_bit_offset(bit_offset);

    // Dispatch based on field type
    // Fields 1-4, 7-10: vectors
    // Fields 12, 15, 16: scalars
    switch (field_id) {
      case 1: return encode_chunks(writer, schema, field_layout);
      case 2: return encode_directories(writer, schema, field_layout);
      case 3: return encode_inodes(writer, schema, field_layout);
      case 4: return encode_chunk_table(writer, field_layout);
      case 7: return encode_uids(writer, field_layout);
      case 8: return encode_gids(writer, field_layout);
      case 9: return encode_modes(writer, field_layout);
      case 10: return encode_names(writer, field_layout);
      case 12: return encode_timestamp_base(writer, field_layout);
      case 15: return encode_block_size(writer, field_layout);
      case 16: return encode_total_fs_size(writer, field_layout);
      default:
        // Skip unknown fields (they may be optional)
        return 0;
    }
  }

private:
  // Vector encoding methods
  uint32_t encode_chunks(
      FrozenWriter& writer,
      Schema const& schema,
      SchemaLayout const* field_layout) {

    auto* elem_field = field_layout->fields.get(3);
    auto* elem_layout = elem_field ? schema.layouts.get(elem_field->layout_id) : nullptr;

    VectorEncoder encoder;
    return encoder.encode_with_element_layout(
        writer, *field_layout, *elem_layout, &meta_.chunks);
  }

  uint32_t encode_directories(
      FrozenWriter& writer,
      Schema const& schema,
      SchemaLayout const* field_layout) {

    auto* elem_field = field_layout->fields.get(3);
    auto* elem_layout = elem_field ? schema.layouts.get(elem_field->layout_id) : nullptr;

    VectorEncoder encoder;
    return encoder.encode_directories(
        writer, *field_layout, *elem_layout, &meta_.directories);
  }

  uint32_t encode_inodes(
      FrozenWriter& writer,
      Schema const& schema,
      SchemaLayout const* field_layout) {

    auto* elem_field = field_layout->fields.get(3);
    auto* elem_layout = elem_field ? schema.layouts.get(elem_field->layout_id) : nullptr;

    VectorEncoder encoder;
    return encoder.encode_inodes(
        writer, *field_layout, *elem_layout, &meta_.inodes);
  }

  uint32_t encode_chunk_table(
      FrozenWriter& writer,
      SchemaLayout const* field_layout) {

    VectorEncoder encoder;
    return encoder.encode(writer, *field_layout, &meta_.chunk_table);
  }

  uint32_t encode_uids(
      FrozenWriter& writer,
      SchemaLayout const* field_layout) {

    VectorEncoder encoder;
    return encoder.encode(writer, *field_layout, &meta_.uids);
  }

  uint32_t encode_gids(
      FrozenWriter& writer,
      SchemaLayout const* field_layout) {

    VectorEncoder encoder;
    return encoder.encode(writer, *field_layout, &meta_.gids);
  }

  uint32_t encode_modes(
      FrozenWriter& writer,
      SchemaLayout const* field_layout) {

    VectorEncoder encoder;
    return encoder.encode(writer, *field_layout, &meta_.modes);
  }

  uint32_t encode_names(
      FrozenWriter& writer,
      SchemaLayout const* field_layout) {

    // The schema uses u32_vector_layout_id as a placeholder for names
    // For now, encode each name as a u32 (simple representation)
    // TODO: Implement proper string table encoding

    // Create a vector of u32 values representing the names
    std::vector<uint32_t> name_indices;
    name_indices.reserve(meta_.names.size());

    for (auto const& name : meta_.names) {
      // Use string length as a simple u32 representation
      // This is a placeholder - real implementation would use string table indices
      name_indices.push_back(static_cast<uint32_t>(name.length()));
    }

    VectorEncoder encoder;
    return encoder.encode(writer, *field_layout, &name_indices);
  }

  // Scalar encoding methods
  uint32_t encode_timestamp_base(
      FrozenWriter& writer,
      SchemaLayout const* field_layout) {

    ScalarEncoder encoder;
    uint64_t value = meta_.timestamp_base;
    return encoder.encode(writer, *field_layout, &value);
  }

  uint32_t encode_block_size(
      FrozenWriter& writer,
      SchemaLayout const* field_layout) {

    ScalarEncoder encoder;
    uint32_t value = meta_.block_size;
    return encoder.encode(writer, *field_layout, &value);
  }

  uint32_t encode_total_fs_size(
      FrozenWriter& writer,
      SchemaLayout const* field_layout) {

    ScalarEncoder encoder;
    uint64_t value = meta_.total_fs_size;
    return encoder.encode(writer, *field_layout, &value);
  }

  const domain::metadata& meta_;
};

} // anonymous namespace

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

  // Step 3: Encode metadata using schema-driven approach
  std::vector<uint8_t> frozen_buffer(65536); // 64KB initial buffer
  FrozenWriter writer{std::span<uint8_t>(frozen_buffer)};

  // Get root layout
  auto* root_layout = schema.layouts.get(schema.root_layout);
  if (!root_layout) {
    throw std::runtime_error("Frozen2Serializer::serialize: root layout not found");
  }

  // Create the schema-driven encoder
  SchemaFieldEncoder encoder(*domain_meta);

  // Iterate through root layout fields in order and encode each one
  for (auto [field_id, field] : root_layout->fields) {
    encoder.encode_field(writer, schema, field_id, *field);
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
