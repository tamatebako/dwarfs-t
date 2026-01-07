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
#include <string_view>
#include <vector>

#include "dwarfs/metadata/legacy/thrift_types.h"

namespace dwarfs::metadata::legacy {

/**
 * Thrift Compact protocol writer
 *
 * Port of dwarfs-rs's ser_thrift.rs serializer.
 * Implements minimal Thrift Compact protocol encoding needed for DwarFS metadata.
 *
 * Reference: dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs
 */
class ThriftCompactWriter {
public:
  /**
   * Construct writer with output buffer
   *
   * @param buffer Output buffer (will be appended to)
   */
  explicit ThriftCompactWriter(std::vector<uint8_t>& buffer);

  // Primitives - return Tag for serializer use
  /**
   * Write boolean value
   *
   * @param v Value to write
   * @param inline_bool If true, encode in field header; if false, use separate byte
   * @return Tag (BOOL_TRUE, BOOL_FALSE, or UNKNOWN_BOOL)
   */
  Tag write_bool(bool v, bool inline_bool = false);

  /**
   * Write 16-bit signed integer (zigzag encoded)
   */
  Tag write_i16(int16_t v);

  /**
   * Write 32-bit signed integer (zigzag encoded)
   */
  Tag write_i32(int32_t v);

  /**
   * Write 64-bit signed integer (zigzag encoded)
   */
  Tag write_i64(int64_t v);

  /**
   * Write string (varint length + bytes)
   */
  Tag write_string(std::string_view s);

  // Low-level encodings
  /**
   * Write varint-encoded unsigned 32-bit integer
   *
   * Encoding: 7-bit chunks with continuation bit in bit 7
   * Reference: ser_thrift.rs:28-38
   */
  void write_varint(uint32_t v);

  /**
   * Write varint-encoded unsigned 64-bit integer
   *
   * Encoding: 7-bit chunks with continuation bit in bit 7
   */
  void write_varint64(uint64_t v);

  /**
   * Write zigzag-encoded signed 32-bit integer
   *
   * Encoding: (v << 1) ^ (v >> 31)
   * Reference: ser_thrift.rs:40-42
   */
  void write_zigzag(int32_t v);

  /**
   * Write zigzag-encoded signed 64-bit integer
   *
   * Encoding: (v << 1) ^ (v >> 63)
   */
  void write_zigzag64(int64_t v);

  // Struct support
  /**
   * Begin struct encoding
   */
  void begin_struct();

  /**
   * Write field header (field ID delta + type tag)
   *
   * @param field_id Field ID (absolute)
   * @param type Field type tag
   *
   * Reference: ser_thrift.rs:254-264
   */
  void write_field_header(uint16_t field_id, Tag type);

  /**
   * End struct encoding (writes stop field)
   */
  void end_struct();

  // Map support
  /**
   * Begin map encoding
   *
   * @param size Number of key-value pairs
   */
  void begin_map(uint32_t size);

  /**
   * Write map type byte (ktype << 4 | vtype)
   *
   * @param ktype Key type tag
   * @param vtype Value type tag
   */
  void write_map_type_byte(Tag ktype, Tag vtype);

  /**
   * End map encoding (no-op)
   */
  void end_map();

  /**
   * Get current write position
   */
  size_t position() const { return buf_.size(); }

  /**
   * Write single raw byte
   */
  void write_byte(uint8_t b) { buf_.push_back(b); }

private:
  std::vector<uint8_t>& buf_;
  uint8_t field_id_diff_tag_{0x10}; // Field ID delta tag counter
  uint16_t last_field_id_{0};       // Last written field ID
};

} // namespace dwarfs::metadata::legacy