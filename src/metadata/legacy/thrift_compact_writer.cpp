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

#include "dwarfs/metadata/legacy/thrift_compact_writer.h"

#include <stdexcept>

namespace dwarfs::metadata::legacy {

ThriftCompactWriter::ThriftCompactWriter(std::vector<uint8_t>& buffer)
    : buf_(buffer) {}

void ThriftCompactWriter::write_varint(uint32_t v) {
  // Port of ser_thrift.rs:28-38
  // Varint encoding: 7-bit chunks with continuation bit
  do {
    uint32_t more = v >> 7;
    bool has_more = more > 0;
    buf_.push_back(static_cast<uint8_t>(v & 0x7F) | (has_more << 7));
    v = more;
  } while (v > 0);
}

void ThriftCompactWriter::write_zigzag(int32_t v) {
  // Port of ser_thrift.rs:40-42
  // Zigzag encoding: (v << 1) ^ (v >> 31)
  write_varint(static_cast<uint32_t>((v << 1) ^ (v >> 31)));
}

void ThriftCompactWriter::write_varint64(uint64_t v) {
  // Varint encoding for 64-bit: 7-bit chunks with continuation bit
  do {
    uint64_t more = v >> 7;
    bool has_more = more > 0;
    buf_.push_back(static_cast<uint8_t>(v & 0x7F) | (has_more << 7));
    v = more;
  } while (v > 0);
}

void ThriftCompactWriter::write_zigzag64(int64_t v) {
  // Zigzag encoding for 64-bit: (v << 1) ^ (v >> 63)
  write_varint64(static_cast<uint64_t>((v << 1) ^ (v >> 63)));
}

Tag ThriftCompactWriter::write_bool(bool v, bool inline_bool) {
  if (inline_bool) {
    // Inline in field header
    return v ? Tag::BOOL_TRUE : Tag::BOOL_FALSE;
  } else {
    // Separate byte
    buf_.push_back(v ? 1 : 0);
    return Tag::UNKNOWN_BOOL;
  }
}

Tag ThriftCompactWriter::write_i16(int16_t v) {
  write_zigzag(v);
  return Tag::I16;
}

Tag ThriftCompactWriter::write_i32(int32_t v) {
  write_zigzag(v);
  return Tag::I32;
}

Tag ThriftCompactWriter::write_i64(int64_t v) {
  write_zigzag64(v);
  return Tag::I64;
}

Tag ThriftCompactWriter::write_string(std::string_view s) {
  // Write length as varint
  write_varint(static_cast<uint32_t>(s.size()));
  // Write string bytes
  buf_.insert(buf_.end(), s.begin(), s.end());
  return Tag::BINARY;
}

void ThriftCompactWriter::begin_struct() {
  // Reset field tracking
  last_field_id_ = 0;
  field_id_diff_tag_ = 0x10;
}

void ThriftCompactWriter::write_field_header(uint16_t field_id, Tag type) {
  // Port of ser_thrift.rs:254-264
  // Field header encoding: delta in upper 4 bits, type in lower 4 bits

  uint16_t delta = field_id - last_field_id_;
  last_field_id_ = field_id;

  if (delta <= 15 && delta > 0) {
    // Small delta: encode in single byte (delta << 4 | type)
    buf_.push_back(static_cast<uint8_t>((delta << 4) | static_cast<uint8_t>(type)));
  } else {
    // Large delta: type byte + varint field ID
    buf_.push_back(static_cast<uint8_t>(type));
    write_zigzag(static_cast<int32_t>(field_id));
  }
}

void ThriftCompactWriter::end_struct() {
  // Write stop field (type 0)
  buf_.push_back(0);
}

void ThriftCompactWriter::begin_map(uint32_t size) {
  // Write size as varint
  write_varint(size);
}

void ThriftCompactWriter::write_map_type_byte(Tag ktype, Tag vtype) {
  // Map type byte: ktype in upper 4 bits, vtype in lower 4 bits
  buf_.push_back(static_cast<uint8_t>((static_cast<uint8_t>(ktype) << 4) |
                                       static_cast<uint8_t>(vtype)));
}

void ThriftCompactWriter::end_map() {
  // No-op for Thrift Compact
}

} // namespace dwarfs::metadata::legacy