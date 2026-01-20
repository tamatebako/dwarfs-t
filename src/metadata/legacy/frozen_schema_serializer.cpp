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

#include "dwarfs/metadata/legacy/frozen_schema_serializer.h"

#include <iostream>
#include <stdexcept>

#include "dwarfs/metadata/legacy/thrift_compact_reader.h"
#include "dwarfs/metadata/legacy/thrift_compact_writer.h"

namespace dwarfs::metadata::legacy {

namespace {

// Helper to skip default values during serialization
// Ported from: metadata.rs:171-173
template <typename T>
bool is_default(T const& v) {
  return v == T{};
}

} // anonymous namespace

// ============================================================================
// Writer implementation
// ============================================================================

class FrozenSchemaSerializer::Writer {
 public:
  explicit Writer(std::vector<uint8_t>& buf)
      : w_(buf) {}

  void write_schema(Schema const& schema) {
    // Ported from: ser_thrift.rs (serialize_struct implementation)
    // Schema fields in order: relax_type_checks, layouts, root_layout,
    // file_version

    w_.begin_struct();

    uint16_t field_id = 1;

    // Field 1: relax_type_checks (bool) - skip if default (false)
    if (!is_default(schema.relax_type_checks)) {
      w_.write_field_header(field_id, Tag::BOOL_TRUE);
      w_.write_bool(schema.relax_type_checks, true);
    }
    ++field_id;

    // Field 2: layouts (DenseMap<SchemaLayout>) - always present
    w_.write_field_header(field_id, Tag::MAP);
    write_layout_map(schema.layouts);
    ++field_id;

    // Field 3: root_layout (i16) - skip if default (0)
    if (!is_default(schema.root_layout)) {
      w_.write_field_header(field_id, Tag::I16);
      w_.write_i16(schema.root_layout);
    }
    ++field_id;

    // Field 4: file_version (i32) - skip if default (0)
    if (!is_default(schema.file_version)) {
      w_.write_field_header(field_id, Tag::I32);
      w_.write_i32(schema.file_version);
    }

    w_.end_struct();
  }

 private:
  void write_layout_map(DenseMap<SchemaLayout> const& layouts) {
    // Count non-empty entries
    uint32_t count = 0;
    for (auto [_, __] : layouts) {
      (void)_;
      (void)__;
      ++count;
    }

    w_.begin_map(count);
    if (count > 0) {
      // Write type byte (i16 -> SchemaLayout)
      w_.write_map_type_byte(Tag::I16, Tag::STRUCT);

      // Write each entry
      for (auto [key, layout_ptr] : layouts) {
        w_.write_i16(key);
        write_layout(*layout_ptr);
      }
    }
    w_.end_map();
  }

  void write_layout(SchemaLayout const& layout) {
    // SchemaLayout fields: size, bits, fields, type_name
    w_.begin_struct();

    uint16_t field_id = 1;

    // Field 1: size (i32) - skip if default (0)
    if (!is_default(layout.size)) {
      w_.write_field_header(field_id, Tag::I32);
      w_.write_i32(layout.size);
    }
    ++field_id;

    // Field 2: bits (i16) - skip if default (0)
    if (!is_default(layout.bits)) {
      w_.write_field_header(field_id, Tag::I16);
      w_.write_i16(layout.bits);
    }
    ++field_id;

    // Field 3: fields (DenseMap<SchemaField>) - always present
    w_.write_field_header(field_id, Tag::MAP);
    write_field_map(layout.fields);
    ++field_id;

    // Field 4: type_name (string) - always present
    w_.write_field_header(field_id, Tag::BINARY);
    w_.write_string(layout.type_name);

    w_.end_struct();
  }

  void write_field_map(DenseMap<SchemaField> const& fields) {
    // Count non-empty entries
    uint32_t count = 0;
    for (auto [_, __] : fields) {
      (void)_;
      (void)__;
      ++count;
    }

    w_.begin_map(count);
    if (count > 0) {
      // Write type byte (i16 -> SchemaField)
      w_.write_map_type_byte(Tag::I16, Tag::STRUCT);

      // Write each entry
      for (auto [key, field_ptr] : fields) {
        w_.write_i16(key);
        write_field(*field_ptr);
      }
    }
    w_.end_map();
  }

  void write_field(SchemaField const& field) {
    // SchemaField fields: layout_id, offset
    w_.begin_struct();

    uint16_t field_id = 1;

    // Field 1: layout_id (i16) - always present
    w_.write_field_header(field_id, Tag::I16);
    w_.write_i16(field.layout_id);
    ++field_id;

    // Field 2: offset (i16) - skip if default (0)
    if (!is_default(field.offset)) {
      w_.write_field_header(field_id, Tag::I16);
      w_.write_i16(field.offset);
    }

    w_.end_struct();
  }

  ThriftCompactWriter w_;
};

// ============================================================================
// Reader implementation
// ============================================================================

class FrozenSchemaSerializer::Reader {
 public:
  explicit Reader(std::span<uint8_t const> data)
      : r_(data) {}

  /**
   * Get current read position
   */
  size_t position() const { return r_.position(); }

  Schema read_schema() {
    // Ported from: de_thrift.rs (deserialize_struct implementation)
    Schema schema;

    r_.begin_struct();

    int field_count = 0;
    while (auto field = r_.read_field_header()) {
      field_count++;
      // Field IDs are 1-indexed in serde, but reader returns 0-indexed
      switch (field->field_id) {
      case 1: // relax_type_checks
        schema.relax_type_checks = r_.read_bool(field->type);
        break;
      case 2: // layouts
        // For Homebrew mkdwarfs, the field type is 1 (which normally means BOOL_FALSE)
        // but the actual value is a map. We need to read the map regardless of the field type.
        schema.layouts = read_layout_map();
        break;
      case 3: // root_layout
        schema.root_layout = r_.read_i16();
        break;
      case 4: // file_version
        schema.file_version = r_.read_i32();
        break;
      default:
        // Skip unknown fields for forward compatibility
        // Newer versions of mkdwarfs might add additional fields
        r_.skip_value(field->type);
        break;
      }
    }

    r_.end_struct();

    return schema;
  }

 private:
  DenseMap<SchemaLayout> read_layout_map() {
    auto header = r_.begin_map();

    DenseMap<SchemaLayout> layouts;
    for (uint32_t i = 0; i < header.size; ++i) {
      auto key = r_.read_i16();
      auto layout = read_layout();
      layouts.insert(key, std::move(layout));
    }

    r_.end_map();
    return layouts;
  }

  SchemaLayout read_layout() {
    SchemaLayout layout;

    r_.begin_struct();

    int field_count = 0;
    std::optional<ThriftCompactReader::FieldHeader> field;

    // First, peek at what the next field header would be
    // Peek at the next byte without consuming it
    {
      uint8_t next_byte = r_.peek_byte();

      // Check if this is a stop byte (empty struct)
      if (next_byte == 0) {
        r_.read_byte();  // consume the stop byte
        r_.end_struct();
        return layout;  // return empty layout
      }

      // Check if this byte could be a field header
      uint8_t delta = next_byte >> 4;
      uint8_t type = next_byte & 0x0F;
      uint16_t potential_field_id = delta;  // simplified check

      // If delta is 0, we'd need to read the field ID, which is complex
      // For now, just check if it's a reasonable field header
      if (delta == 0) {
        // Read the actual field header
        field = r_.read_field_header();
        if (!field.has_value()) {
          r_.end_struct();
          return layout;
        }
        potential_field_id = field->field_id;
      }

      // Check if this field ID is valid (should be 1-4)
      if (potential_field_id < 1 || potential_field_id > 4) {
        // Put the byte back and return empty layout
        if (delta != 0 && !field.has_value()) {
          r_.unget_byte();
        }
        r_.end_struct();
        return layout;
      }

      // If we read a field header above, we need to process it
      if (field.has_value()) {
        goto process_field;
      }
    }

    while ((field = r_.read_field_header()).has_value()) {
    process_field:
      field_count++;

      // Check if this field ID is valid (should be 1-4)
      if (field->field_id < 1 || field->field_id > 4) {
        // We've read past the end of the struct - this byte belongs to the next map entry
        r_.unget_byte();
        break;
      }

      switch (field->field_id) {
      case 1: // size
        layout.size = r_.read_i32();
        break;
      case 2: // bits
        layout.bits = r_.read_i16();
        break;
      case 3: // fields
        layout.fields = read_field_map();
        break;
      case 4: // type_name
        layout.type_name = std::string(r_.read_string());
        break;
      default:
        // Should not reach here due to check above
        r_.skip_value(field->type);
        break;
      }
    }

    r_.end_struct();

    return layout;
  }

  DenseMap<SchemaField> read_field_map() {
    auto header = r_.begin_map();

    DenseMap<SchemaField> fields;
    for (uint32_t i = 0; i < header.size; ++i) {
      auto key = r_.read_i16();
      auto field = read_field();
      fields.insert(key, std::move(field));
    }

    r_.end_map();
    return fields;
  }

  SchemaField read_field() {
    SchemaField field;

    r_.begin_struct();
    int16_t field_id_for_debug = 0;

    while (auto f = r_.read_field_header()) {
      field_id_for_debug = f->field_id;
      switch (f->field_id) {
      case 1: // layout_id
        field.layout_id = r_.read_i16();
        break;
      case 2: // offset
        field.offset = r_.read_i16();
        break;
      default:
        // Skip unknown fields for forward compatibility
        r_.skip_value(f->type);
        break;
      }
    }

    r_.end_struct();
    return field;
  }

  ThriftCompactReader r_;
};

// ============================================================================
// Public API
// ============================================================================

std::vector<uint8_t> FrozenSchemaSerializer::serialize(Schema const& schema) {
  std::vector<uint8_t> buf;
  Writer writer(buf);
  writer.write_schema(schema);
  return buf;
}

Schema FrozenSchemaSerializer::deserialize(std::span<uint8_t const> data) {
  Reader reader(data);
  auto schema = reader.read_schema();

  // Validate the parsed schema
  try {
    schema.validate();
  } catch (std::exception const& e) {
    throw std::runtime_error(
        std::string("failed to parse schema: ") + e.what());
  }

  return schema;
}

Schema FrozenSchemaSerializer::deserialize(std::span<uint8_t const> data,
                                            size_t& bytes_consumed) {
  Reader reader(data);
  auto schema = reader.read_schema();

  // Get bytes consumed from reader position
  bytes_consumed = reader.position();

  // Validate the parsed schema
  try {
    schema.validate();
  } catch (std::exception const& e) {
    throw std::runtime_error(
        std::string("failed to parse schema: ") + e.what());
  }

  return schema;
}

} // namespace dwarfs::metadata::legacy