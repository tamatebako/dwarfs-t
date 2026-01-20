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

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "dwarfs/metadata/legacy/thrift_types.h"

namespace dwarfs::metadata::legacy {

/**
 * Thrift Compact protocol reader
 *
 * Port of dwarfs-rs's de_thrift.rs deserializer.
 * Implements minimal Thrift Compact protocol decoding needed for DwarFS metadata.
 *
 * Reference: dwarfs-rs/dwarfs/src/metadata/de_thrift.rs
 */
class ThriftCompactReader {
public:
  /**
   * Field header information
   */
  struct FieldHeader {
    uint16_t field_id;
    Tag type;
  };

  /**
   * Map header information
   */
  struct MapHeader {
    uint32_t size;
    Tag ktype;
    Tag vtype;
  };

  /**
   * Construct reader with input data
   *
   * @param data Input byte span (must remain valid during reader lifetime)
   */
  explicit ThriftCompactReader(std::span<uint8_t const> data);

  // Primitives
  /**
   * Read boolean value
   *
   * @param hint Tag hint (BOOL_TRUE/BOOL_FALSE for inline, UNKNOWN_BOOL for separate byte)
   */
  bool read_bool(Tag hint = Tag::UNKNOWN_BOOL);

  /**
   * Read 16-bit signed integer (zigzag encoded)
   */
  int16_t read_i16();

  /**
   * Read 32-bit signed integer (zigzag encoded)
   */
  int32_t read_i32();

  /**
   * Read 64-bit signed integer (zigzag encoded)
   */
  int64_t read_i64();

  /**
   * Read string (varint length + bytes)
   */
  std::string_view read_string();

  // Low-level decodings
  /**
   * Read varint-encoded unsigned 32-bit integer
   *
   * Decoding: 7-bit chunks with continuation bit in bit 7
   * Reference: de_thrift.rs:103-113
   */
  uint32_t read_varint();

  /**
   * Read varint-encoded unsigned 64-bit integer
   *
   * Decoding: 7-bit chunks with continuation bit in bit 7
   */
  uint64_t read_varint64();

  /**
   * Read zigzag-encoded signed 32-bit integer
   *
   * Decoding: (x >> 1) ^ -(x & 1)
   * Reference: de_thrift.rs:115-118
   */
  int32_t read_zigzag();

  /**
   * Read zigzag-encoded signed 64-bit integer
   *
   * Decoding: (x >> 1) ^ -(x & 1)
   */
  int64_t read_zigzag64();

  /**
   * Read single byte
   */
  uint8_t read_byte();

  /**
   * Peek at next byte without consuming it
   */
  uint8_t peek_byte() const {
    if (pos_ >= data_.size()) {
      throw std::runtime_error("ThriftCompactReader: unexpected end of data");
    }
    return data_[pos_];
  }

  /**
   * Unget last byte (move position back by 1)
   * Used when we read a byte that belongs to the next structure
   */
  void unget_byte() {
    if (pos_ > 0) {
      --pos_;
    }
  }

  // Struct support
  /**
   * Begin struct decoding
   */
  void begin_struct();

  /**
   * Read field header (field ID delta + type tag)
   *
   * @return Field header if valid field, nullopt if stop field
   *
   * Reference: de_thrift.rs:164-197
   */
  std::optional<FieldHeader> read_field_header();

  /**
   * End struct decoding (no-op)
   */
  void end_struct();

  // Map support
  /**
   * Begin map decoding
   *
   * @return Map header
   */
  MapHeader begin_map();

  /**
   * End map decoding (no-op)
   */
  void end_map();

  /**
   * Skip a value of the given type
   *
   * Used for skipping unknown fields during deserialization
   *
   * @param type Tag type of the value to skip
   */
  void skip_value(Tag type);

  /**
   * Check if at end of data
   */
  bool at_end() const { return pos_ >= data_.size(); }

  /**
   * Get current read position
   */
  size_t position() const { return pos_; }

private:
  std::span<uint8_t const> data_;
  size_t pos_{0};
  int16_t last_field_id_{0}; // Last read field ID
  std::vector<int16_t> last_field_id_stack_; // Stack for nested structs
};

} // namespace dwarfs::metadata::legacy