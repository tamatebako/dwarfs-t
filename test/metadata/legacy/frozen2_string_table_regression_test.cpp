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
 * Frozen2 string_table Deserializer Regression Tests
 *
 * Regression test for the bug documented in
 * docs/issues/frozen2_string_table_issue.md:
 *
 *   The frozen2 deserializer read string_table.buffer (Thrift field 1,
 *   a plain non-optional `string`) as if it were `optional<string>`,
 *   i.e. it first consumed an is_some boolean and only then read the
 *   outlined string. Since a non-optional outlined string is encoded as
 *   {distance, length} with no is_some flag, the buffer always came back
 *   empty. Downstream (libtfs/tebako) this surfaced as placeholder
 *   filenames (file_1, file_2, ...) instead of the real names stored in
 *   compact_names when reading legacy (v0.14.1, Homebrew mkdwarfs)
 *   archives.
 *
 * The fix reads the buffer directly as an outlined string
 * (distance + length) without checking for an is_some boolean
 * (see read_string_table() in src/metadata/legacy/frozen2_deserializer.cpp).
 *
 * These tests hand-craft the Frozen2 schema and bit-packed data so they
 * exercise the *deserializer* in isolation. (Frozen2Serializer cannot be
 * used to produce the bytes: it currently only emits the schema section,
 * and its metadata-data encoding is a known-broken work in progress —
 * see the DISABLED_StringTableRoundTrip test in
 * frozen2_integration_test.cpp.)
 *
 * Byte-layout assumptions mirror frozen_bits (little-endian, LSB-first)
 * and the Reader implementation: SchemaField offsets >= 0 are byte
 * offsets; u32 scalars are 32 bits wide; bools are a single LSB bit.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "dwarfs/metadata/legacy/frozen2_deserializer.h"
#include "dwarfs/metadata/legacy/frozen_schema.h"
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/domain/string_table.h"

using namespace dwarfs::metadata;
using namespace dwarfs::metadata::legacy;
using namespace dwarfs::metadata::domain;

namespace {

// Layout IDs used by the hand-crafted schema. The actual values are
// arbitrary as long as they are used consistently.
constexpr int16_t kRootLayoutId = 0;
constexpr int16_t kOptionalStringTableLayoutId = 1;
constexpr int16_t kBoolLayoutId = 2;
constexpr int16_t kStringTableLayoutId = 3;
constexpr int16_t kOutlinedStringLayoutId = 4;
constexpr int16_t kU32VectorLayoutId = 5;
constexpr int16_t kU32LayoutId = 6;

// Absolute byte offsets in the synthetic frozen data.
//
// Byte 0:      compact_names (metadata field 24) optional<string_table>
//              - byte 0:      is_some (bool)
//              - byte 1..17:  string_table
// Byte 1..8:   buffer outlined string {distance (4), length (4)}
// Byte 9..16:  index vector<u32> {distance (4), length (4)}
// Byte 17:     packed_index (bool)
// Byte 18..:   outlined payloads (buffer contents, then index elements)
constexpr size_t kStringTableOffset = 1;
constexpr size_t kIndexVectorOffset = 9;
constexpr size_t kPackedIndexOffset = 17;
constexpr size_t kPayloadOffset = 18;

struct encoded_metadata {
  Schema schema;
  std::vector<uint8_t> data;
};

void append_u32_le(std::vector<uint8_t>& data, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    data.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xff));
  }
}

void set_u32_le(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    data.at(offset + i) = static_cast<uint8_t>((value >> (8 * i)) & 0xff);
  }
}

/**
 * Build a Frozen2 schema that describes a metadata struct containing
 * only field 24 (compact_names, optional<string_table>).
 */
Schema make_compact_names_schema() {
  Schema schema;
  schema.relax_type_checks = true;
  schema.file_version = 1;
  schema.root_layout = kRootLayoutId;

  // Scalar layouts (no fields).
  SchemaLayout u32_layout;
  u32_layout.bits = 32;
  u32_layout.type_name = "u32";
  schema.layouts.insert(kU32LayoutId, u32_layout);

  SchemaLayout bool_layout;
  bool_layout.bits = 1;
  bool_layout.type_name = "bool";
  schema.layouts.insert(kBoolLayoutId, bool_layout);

  // Outlined (non-optional) string: {distance, length}.
  SchemaLayout outlined;
  outlined.bits = 64;
  outlined.type_name = "string";
  outlined.fields.insert(1, SchemaField{kU32LayoutId, 0});
  outlined.fields.insert(2, SchemaField{kU32LayoutId, 4});
  schema.layouts.insert(kOutlinedStringLayoutId, outlined);

  // vector<u32>: {distance, length, element layout}.
  SchemaLayout vec;
  vec.bits = 64;
  vec.type_name = "vector<u32>";
  vec.fields.insert(1, SchemaField{kU32LayoutId, 0});
  vec.fields.insert(2, SchemaField{kU32LayoutId, 4});
  vec.fields.insert(3, SchemaField{kU32LayoutId, 8});
  schema.layouts.insert(kU32VectorLayoutId, vec);

  // string_table: field 1 buffer, field 3 index, field 4 packed_index.
  // (field 2, the optional symtab, is intentionally absent.)
  SchemaLayout table;
  table.bits = 8 * (kPackedIndexOffset + 1 - kStringTableOffset);
  table.type_name = "string_table";
  // buffer starts at relative offset 0 within the string_table.
  table.fields.insert(1, SchemaField{kOutlinedStringLayoutId, 0});
  table.fields.insert(
      3, SchemaField{kU32VectorLayoutId,
                     static_cast<int16_t>(kIndexVectorOffset -
                                          kStringTableOffset)});
  table.fields.insert(
      4, SchemaField{kBoolLayoutId,
                     static_cast<int16_t>(kPackedIndexOffset -
                                          kStringTableOffset)});
  schema.layouts.insert(kStringTableLayoutId, table);

  // optional<string_table>: field 1 is_some, field 2 value.
  SchemaLayout opt;
  opt.bits = 8 * kPayloadOffset;
  opt.type_name = "optional<string_table>";
  opt.fields.insert(1, SchemaField{kBoolLayoutId, 0});
  opt.fields.insert(
      2, SchemaField{kStringTableLayoutId,
                     static_cast<int16_t>(kStringTableOffset)});
  schema.layouts.insert(kOptionalStringTableLayoutId, opt);

  // Root metadata layout: only field 24 (compact_names) at byte 0.
  SchemaLayout root;
  root.bits = 8 * kPayloadOffset;
  root.type_name = "metadata";
  root.fields.insert(24, SchemaField{kOptionalStringTableLayoutId, 0});
  schema.layouts.insert(kRootLayoutId, root);

  return schema;
}

/**
 * Encode frozen data holding a compact_names string table with the
 * given buffer/index, laid out exactly as the schema above describes.
 */
encoded_metadata encode_compact_names(std::string const& buffer,
                                      std::vector<uint32_t> const& index,
                                      bool packed_index) {
  encoded_metadata out;
  out.schema = make_compact_names_schema();

  out.data.assign(kPayloadOffset, 0);

  // is_some = true for the optional<string_table> wrapper.
  out.data[0] = 1;

  // buffer outlined string: payload directly after the bit-packed region.
  uint32_t buffer_distance = static_cast<uint32_t>(kPayloadOffset);
  set_u32_le(out.data, kStringTableOffset, buffer_distance);
  set_u32_le(out.data, kStringTableOffset + 4,
             static_cast<uint32_t>(buffer.size()));

  // index vector: payload directly after the buffer payload.
  uint32_t index_distance =
      buffer_distance + static_cast<uint32_t>(buffer.size());
  set_u32_le(out.data, kIndexVectorOffset, index_distance);
  set_u32_le(out.data, kIndexVectorOffset + 4,
             static_cast<uint32_t>(index.size()));

  // packed_index flag.
  out.data[kPackedIndexOffset] = packed_index ? 1 : 0;

  // Outlined payloads.
  out.data.insert(out.data.end(), buffer.begin(), buffer.end());
  for (uint32_t v : index) {
    append_u32_le(out.data, v);
  }

  return out;
}

/**
 * Encode frozen data where compact_names (field 24) is absent, i.e. the
 * optional wrapper's is_some flag is false.
 */
encoded_metadata encode_without_compact_names() {
  encoded_metadata out;
  out.schema = make_compact_names_schema();
  out.data.assign(kPayloadOffset, 0);  // is_some = false, rest zero
  return out;
}

} // namespace

/**
 * Regression test: string_table.buffer must be read as a non-optional
 * outlined string (distance + length), not as optional<string>.
 *
 * With the bug present, the buffer comes back empty and the synthesized
 * names fall back to placeholders ("file_1", "file_2", ...).
 */
TEST(Frozen2StringTableRegressionTest, CompactNamesBufferIsReadAsOutlinedString) {
  // The exact filenames from the original issue report (33-byte buffer
  // as reported by dwarfsck for the affected archive).
  std::string const buffer{"hello.txt\0subdir\0nested.txt\0", 28};
  std::vector<uint32_t> const index{0, 10, 17, 28};

  auto encoded = encode_compact_names(buffer, index, /*packed_index=*/false);

  auto meta = Frozen2Deserializer::deserialize(encoded.schema, encoded.data);

  ASSERT_TRUE(meta.compact_names.has_value())
      << "compact_names must be present (optional is_some was set)";

  // The core regression assertion: the buffer must not be empty and must
  // contain the original filename data.
  EXPECT_EQ(meta.compact_names->buffer, buffer)
      << "string_table.buffer must be read as a non-optional outlined "
         "string (see docs/issues/frozen2_string_table_issue.md)";
  EXPECT_EQ(meta.compact_names->index, index);
  EXPECT_FALSE(meta.compact_names->packed_index);
  EXPECT_FALSE(meta.compact_names->symtab.has_value())
      << "symtab (field 2) was not encoded and must stay absent";

  // The names vector derived from compact_names must contain the real
  // filenames, not the file_N placeholders produced when the buffer is
  // empty.
  ASSERT_EQ(meta.names.size(), 3);
  EXPECT_EQ(meta.names[0], std::string("hello.txt\0", 10));
  EXPECT_EQ(meta.names[1], std::string("subdir\0", 7));
  EXPECT_EQ(meta.names[2], std::string("nested.txt\0", 11));
  for (auto const& name : meta.names) {
    EXPECT_EQ(name.find("file_"), std::string::npos)
        << "placeholder name detected: " << name;
  }
}

/**
 * Same code path with a string-table-heavy table (128 names), closer to
 * real-world compact_names tables.
 */
TEST(Frozen2StringTableRegressionTest, CompactNamesLargeTable) {
  constexpr size_t kNumNames = 128;

  std::string buffer;
  std::vector<uint32_t> index;
  std::vector<std::string> expected_names;

  for (size_t i = 0; i < kNumNames; ++i) {
    index.push_back(static_cast<uint32_t>(buffer.size()));
    std::string name = "name_" + std::to_string(i) + ".dat";
    buffer.append(name);
    buffer.push_back('\0');
    expected_names.push_back(name + '\0');
  }
  index.push_back(static_cast<uint32_t>(buffer.size()));

  auto encoded = encode_compact_names(buffer, index, /*packed_index=*/false);

  auto meta = Frozen2Deserializer::deserialize(encoded.schema, encoded.data);

  ASSERT_TRUE(meta.compact_names.has_value());
  EXPECT_EQ(meta.compact_names->buffer, buffer);
  EXPECT_EQ(meta.compact_names->index, index);

  ASSERT_EQ(meta.names.size(), kNumNames);
  for (size_t i = 0; i < kNumNames; ++i) {
    EXPECT_EQ(meta.names[i], expected_names[i]) << "name " << i;
  }
}

/**
 * The optional wrapper around compact_names must still be honored: when
 * is_some is false, compact_names must be absent.
 */
TEST(Frozen2StringTableRegressionTest, CompactNamesAbsentWhenNotPresent) {
  auto encoded = encode_without_compact_names();

  auto meta = Frozen2Deserializer::deserialize(encoded.schema, encoded.data);

  EXPECT_FALSE(meta.compact_names.has_value());
  EXPECT_TRUE(meta.names.empty());
}
