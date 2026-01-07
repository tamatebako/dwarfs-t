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

#include <gtest/gtest.h>

#include "dwarfs/metadata/legacy/thrift_compact_reader.h"
#include "dwarfs/metadata/legacy/thrift_compact_writer.h"

using namespace dwarfs::metadata::legacy;

// ============================================================================
// Varint Tests
// ============================================================================

TEST(ThriftCompactWriter, Varint_Zero) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(0);
  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0x00);
}

TEST(ThriftCompactWriter, Varint_127) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(127);
  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0x7F);
}

TEST(ThriftCompactWriter, Varint_128) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(128);
  ASSERT_EQ(buf.size(), 2);
  EXPECT_EQ(buf[0], 0x80); // 0 | 0x80 (continuation bit)
  EXPECT_EQ(buf[1], 0x01); // 1
}

TEST(ThriftCompactWriter, Varint_16383) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(16383);
  ASSERT_EQ(buf.size(), 2);
  EXPECT_EQ(buf[0], 0xFF); // 127 | 0x80
  EXPECT_EQ(buf[1], 0x7F); // 127
}

TEST(ThriftCompactWriter, Varint_MaxU32) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(UINT32_MAX);
  ASSERT_EQ(buf.size(), 5);
  EXPECT_EQ(buf[0], 0xFF);
  EXPECT_EQ(buf[1], 0xFF);
  EXPECT_EQ(buf[2], 0xFF);
  EXPECT_EQ(buf[3], 0xFF);
  EXPECT_EQ(buf[4], 0x0F);
}

// ============================================================================
// Zigzag Tests
// ============================================================================

TEST(ThriftCompactWriter, Zigzag_Zero) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_zigzag(0);
  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0x00);
}

TEST(ThriftCompactWriter, Zigzag_NegativeOne) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_zigzag(-1);
  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0x01); // (-1 << 1) ^ (-1 >> 31) = -2 ^ -1 = 1
}

TEST(ThriftCompactWriter, Zigzag_One) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_zigzag(1);
  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0x02); // (1 << 1) ^ (1 >> 31) = 2 ^ 0 = 2
}

TEST(ThriftCompactWriter, Zigzag_MinusTwo) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_zigzag(-2);
  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0x03); // (-2 << 1) ^ (-2 >> 31) = -4 ^ -1 = 3
}

TEST(ThriftCompactWriter, Zigzag_MinI32) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_zigzag(INT32_MIN);
  ASSERT_EQ(buf.size(), 5);
}

TEST(ThriftCompactWriter, Zigzag_MaxI32) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_zigzag(INT32_MAX);
  ASSERT_EQ(buf.size(), 5);
}

// ============================================================================
// Bool Tests
// ============================================================================

TEST(ThriftCompactWriter, Bool_Inline_True) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  Tag tag = w.write_bool(true, true);
  EXPECT_EQ(tag, Tag::BOOL_TRUE);
  EXPECT_EQ(buf.size(), 0); // Inline bools don't write bytes
}

TEST(ThriftCompactWriter, Bool_Inline_False) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  Tag tag = w.write_bool(false, true);
  EXPECT_EQ(tag, Tag::BOOL_FALSE);
  EXPECT_EQ(buf.size(), 0);
}

TEST(ThriftCompactWriter, Bool_Separate_True) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  Tag tag = w.write_bool(true, false);
  EXPECT_EQ(tag, Tag::UNKNOWN_BOOL);
  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 1);
}

TEST(ThriftCompactWriter, Bool_Separate_False) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  Tag tag = w.write_bool(false, false);
  EXPECT_EQ(tag, Tag::UNKNOWN_BOOL);
  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0);
}

// ============================================================================
// String Tests
// ============================================================================

TEST(ThriftCompactWriter, String_Empty) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  Tag tag = w.write_string("");
  EXPECT_EQ(tag, Tag::BINARY);
  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0x00); // Length = 0
}

TEST(ThriftCompactWriter, String_Hello) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  Tag tag = w.write_string("hello");
  EXPECT_EQ(tag, Tag::BINARY);
  ASSERT_EQ(buf.size(), 6);
  EXPECT_EQ(buf[0], 0x05); // Length = 5
  EXPECT_EQ(buf[1], 'h');
  EXPECT_EQ(buf[2], 'e');
  EXPECT_EQ(buf[3], 'l');
  EXPECT_EQ(buf[4], 'l');
  EXPECT_EQ(buf[5], 'o');
}

// ============================================================================
// Round-trip Tests
// ============================================================================

TEST(ThriftCompact, RoundTrip_Varint) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_varint(0);
  w.write_varint(127);
  w.write_varint(128);
  w.write_varint(16383);
  w.write_varint(UINT32_MAX);

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_varint(), 0);
  EXPECT_EQ(r.read_varint(), 127);
  EXPECT_EQ(r.read_varint(), 128);
  EXPECT_EQ(r.read_varint(), 16383);
  EXPECT_EQ(r.read_varint(), UINT32_MAX);
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, RoundTrip_Zigzag) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_zigzag(0);
  w.write_zigzag(-1);
  w.write_zigzag(1);
  w.write_zigzag(-2);
  w.write_zigzag(2);
  w.write_zigzag(INT32_MIN);
  w.write_zigzag(INT32_MAX);

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_zigzag(), 0);
  EXPECT_EQ(r.read_zigzag(), -1);
  EXPECT_EQ(r.read_zigzag(), 1);
  EXPECT_EQ(r.read_zigzag(), -2);
  EXPECT_EQ(r.read_zigzag(), 2);
  EXPECT_EQ(r.read_zigzag(), INT32_MIN);
  EXPECT_EQ(r.read_zigzag(), INT32_MAX);
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, RoundTrip_Bool) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_bool(true, false);
  w.write_bool(false, false);

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_bool(), true);
  EXPECT_EQ(r.read_bool(), false);
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, RoundTrip_Bool_Inline) {
  ThriftCompactReader r_true({});
  EXPECT_EQ(r_true.read_bool(Tag::BOOL_TRUE), true);

  ThriftCompactReader r_false({});
  EXPECT_EQ(r_false.read_bool(Tag::BOOL_FALSE), false);
}

TEST(ThriftCompact, RoundTrip_I16) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_i16(0);
  w.write_i16(-123);
  w.write_i16(456);
  w.write_i16(INT16_MIN);
  w.write_i16(INT16_MAX);

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_i16(), 0);
  EXPECT_EQ(r.read_i16(), -123);
  EXPECT_EQ(r.read_i16(), 456);
  EXPECT_EQ(r.read_i16(), INT16_MIN);
  EXPECT_EQ(r.read_i16(), INT16_MAX);
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, RoundTrip_I32) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_i32(0);
  w.write_i32(42);
  w.write_i32(-42);
  w.write_i32(INT32_MIN);
  w.write_i32(INT32_MAX);

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_i32(), 0);
  EXPECT_EQ(r.read_i32(), 42);
  EXPECT_EQ(r.read_i32(), -42);
  EXPECT_EQ(r.read_i32(), INT32_MIN);
  EXPECT_EQ(r.read_i32(), INT32_MAX);
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, RoundTrip_String) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_string("");
  w.write_string("hello");
  w.write_string("world");

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_string(), "");
  EXPECT_EQ(r.read_string(), "hello");
  EXPECT_EQ(r.read_string(), "world");
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, RoundTrip_AllTypes) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_i32(42);
  w.write_i16(-123);
  w.write_string("hello");
  w.write_bool(true, false);
  w.write_bool(false, false);

  ThriftCompactReader r(buf);
  EXPECT_EQ(r.read_i32(), 42);
  EXPECT_EQ(r.read_i16(), -123);
  EXPECT_EQ(r.read_string(), "hello");
  EXPECT_EQ(r.read_bool(), true);
  EXPECT_EQ(r.read_bool(), false);
  EXPECT_TRUE(r.at_end());
}

// ============================================================================
// Struct Tests
// ============================================================================

TEST(ThriftCompact, Struct_Empty) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.begin_struct();
  w.end_struct();

  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0x00); // Stop field

  ThriftCompactReader r(buf);
  r.begin_struct();
  auto hdr = r.read_field_header();
  EXPECT_FALSE(hdr.has_value());
  r.end_struct();
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, Struct_SingleField_SmallDelta) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.begin_struct();
  w.write_field_header(1, Tag::I32);
  w.write_i32(42);
  w.end_struct();

  ThriftCompactReader r(buf);
  r.begin_struct();
  auto hdr = r.read_field_header();
  ASSERT_TRUE(hdr.has_value());
  EXPECT_EQ(hdr->field_id, 1);
  EXPECT_EQ(hdr->type, Tag::I32);
  EXPECT_EQ(r.read_i32(), 42);
  hdr = r.read_field_header();
  EXPECT_FALSE(hdr.has_value()); // Stop field
  r.end_struct();
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, Struct_MultipleFields_Sequential) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.begin_struct();
  w.write_field_header(1, Tag::I32);
  w.write_i32(42);
  w.write_field_header(2, Tag::BINARY);
  w.write_string("hello");
  w.write_field_header(3, Tag::I16);
  w.write_i16(-123);
  w.end_struct();

  ThriftCompactReader r(buf);
  r.begin_struct();

  auto hdr1 = r.read_field_header();
  ASSERT_TRUE(hdr1.has_value());
  EXPECT_EQ(hdr1->field_id, 1);
  EXPECT_EQ(hdr1->type, Tag::I32);
  EXPECT_EQ(r.read_i32(), 42);

  auto hdr2 = r.read_field_header();
  ASSERT_TRUE(hdr2.has_value());
  EXPECT_EQ(hdr2->field_id, 2);
  EXPECT_EQ(hdr2->type, Tag::BINARY);
  EXPECT_EQ(r.read_string(), "hello");

  auto hdr3 = r.read_field_header();
  ASSERT_TRUE(hdr3.has_value());
  EXPECT_EQ(hdr3->field_id, 3);
  EXPECT_EQ(hdr3->type, Tag::I16);
  EXPECT_EQ(r.read_i16(), -123);

  auto stop = r.read_field_header();
  EXPECT_FALSE(stop.has_value());
  r.end_struct();
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, Struct_LargeDelta) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.begin_struct();
  w.write_field_header(1, Tag::I32);
  w.write_i32(10);
  w.write_field_header(100, Tag::I32); // Large jump
  w.write_i32(20);
  w.end_struct();

  ThriftCompactReader r(buf);
  r.begin_struct();

  auto hdr1 = r.read_field_header();
  ASSERT_TRUE(hdr1.has_value());
  EXPECT_EQ(hdr1->field_id, 1);
  EXPECT_EQ(r.read_i32(), 10);

  auto hdr2 = r.read_field_header();
  ASSERT_TRUE(hdr2.has_value());
  EXPECT_EQ(hdr2->field_id, 100);
  EXPECT_EQ(r.read_i32(), 20);

  auto stop = r.read_field_header();
  EXPECT_FALSE(stop.has_value());
  r.end_struct();
  EXPECT_TRUE(r.at_end());
}

// ============================================================================
// Map Tests
// ============================================================================

TEST(ThriftCompact, Map_Empty) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.begin_map(0);
  w.end_map();

  ASSERT_EQ(buf.size(), 1);
  EXPECT_EQ(buf[0], 0x00); // Size = 0

  ThriftCompactReader r(buf);
  auto hdr = r.begin_map();
  EXPECT_EQ(hdr.size, 0);
  r.end_map();
  EXPECT_TRUE(r.at_end());
}

TEST(ThriftCompact, Map_SingleEntry) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.begin_map(1);
  w.write_map_type_byte(Tag::I32, Tag::BINARY);
  w.write_i32(42);
  w.write_string("value");
  w.end_map();

  ThriftCompactReader r(buf);
  auto hdr = r.begin_map();
  EXPECT_EQ(hdr.size, 1);
  EXPECT_EQ(hdr.ktype, Tag::I32);
  EXPECT_EQ(hdr.vtype, Tag::BINARY);
  
  EXPECT_EQ(r.read_i32(), 42);
  EXPECT_EQ(r.read_string(), "value");
  
  r.end_map();
  EXPECT_TRUE(r.at_end());
}