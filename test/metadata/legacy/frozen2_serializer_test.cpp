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

/**
 * Frozen2 Serializer Tests
 *
 * Ported from: dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs:859-961
 *
 * These tests verify byte-for-byte compatibility with the Rust implementation
 * to ensure Homebrew v0.14.1 compatibility.
 */

#include <stdio.h>
#include <gtest/gtest.h>

#include "dwarfs/metadata/legacy/frozen2_serializer.h"
#include "dwarfs/metadata/domain/metadata.h"

namespace dwarfs::metadata::legacy::test {

// NEW: Minimal test - just verify basic struct with primitives
TEST(Frozen2SerializerTest, SimpleStruct) {
  domain::metadata meta;
  // Set only simple u64 fields
  meta.timestamp_base = 42;
  meta.block_size = 100;
  meta.total_fs_size = 200;

  auto [schema, out] = Frozen2Serializer::serialize(meta);

  // This should work if basic struct serialization works
  EXPECT_GT(out.size(), 0);
}

// Port of ser_frozen.rs:864-886
TEST(Frozen2SerializerTest, SmokeTest) {
  domain::metadata meta;
  auto opts = domain::fs_options{};
  opts.mtime_only = true;
  opts.time_resolution_sec = 42;
  meta.options = opts;

  auto [schema, out] = Frozen2Serializer::serialize(meta);

  // Verify schema
  EXPECT_EQ(schema.file_version, 1);
  EXPECT_TRUE(schema.relax_type_checks);

  // Verify exact byte output
  std::vector<uint8_t> expected = {
    1,              // options.is_some = true
    1,              // options.inner.mtime_only = true
    1,              // options.inner.time_resolution.is_some = true
    42, 0, 0, 0,    // options.inner.time_resolution.inner = 42 (little-endian u32)
  };
  EXPECT_EQ(out, expected);
}

// Port of ser_frozen.rs:888-915
TEST(Frozen2SerializerTest, BytesTest) {
  domain::metadata meta;
  meta.dwarfs_version = "abc";

  auto [schema, out] = Frozen2Serializer::serialize(meta);

  // Verify exact byte output
  std::vector<uint8_t> expected = {
    1,              // dwarfs_version.is_some = true
    9, 0, 0, 0,     // dwarfs_version.inner.distance = 9 (little-endian u32)
    3, 0, 0, 0,     // dwarfs_version.inner.count = 3 (little-endian u32)
    // Outlined data:
    'a', 'b', 'c',  // actual string bytes
  };
  EXPECT_EQ(out, expected);

  // Test all-zeros case (ser_frozen.rs:905-914)
  meta.dwarfs_version = std::string("\0\0", 2);  // Two null bytes
  auto [schema2, out2] = Frozen2Serializer::serialize(meta);

  // With all-zero bytes, they are omitted
  std::vector<uint8_t> expected2 = {
    1,              // dwarfs_version.is_some = true
    2, 0, 0, 0,     // dwarfs_version.inner.count = 2 (little-endian u32)
  };
  EXPECT_EQ(out2, expected2);
}

// Port of ser_frozen.rs:917-960
TEST(Frozen2SerializerTest, CollectionTest) {
  domain::metadata meta;

  // Create chunks (ser_frozen.rs:921-924)
  meta.chunks = {
    domain::chunk(0, 0, 42),
    domain::chunk(0, 100, 42),
  };

  // Create symlink_table (ser_frozen.rs:925)
  meta.symlink_table = {0, 0, 0};

  auto [schema, out] = Frozen2Serializer::serialize(meta);

  // Verify exact byte output (ser_frozen.rs:928-959)
  std::vector<uint8_t> expected = {
    12, 0, 0, 0,    // chunks.distance = 12 (little-endian u32)
    2, 0, 0, 0,     // chunks.count = 2 (little-endian u32)
    3, 0, 0, 0,     // symlink_table.count = 3 (little-endian u32)
    // Outlined chunk data:
    0, 0, 0, 0,     // chunks[0].offset = 0 (little-endian u32)
    42, 0, 0, 0,    // chunks[0].size = 42 (little-endian u32)
    100, 0, 0, 0,   // chunks[1].offset = 100 (little-endian u32)
    42, 0, 0, 0,    // chunks[1].size = 42 (little-endian u32)
  };
  EXPECT_EQ(out, expected);
}

} // namespace dwarfs::metadata::legacy::test