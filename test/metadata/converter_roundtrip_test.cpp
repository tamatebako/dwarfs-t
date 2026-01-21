// New file: test/metadata/converter_roundtrip_test.cpp
#include <gtest/gtest.h>

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT

#include "dwarfs/internal/cpp_thrift_converter.h"

// Use fully qualified names to avoid conflicts
using dwarfs::internal::chunk;
using dwarfs::internal::directory;
using dwarfs::internal::string_table;
using dwarfs::internal::metadata;

using namespace dwarfs::internal;

// ============================================================================
// string_table Round-Trip Tests
// ============================================================================

TEST(ConverterRoundTripTest, StringTableEmpty) {
  // Create empty string table
  string_table original;

  // Round-trip: C++ → Thrift → C++
  auto thrift_version = to_thrift(original);
  string_table roundtrip = from_thrift(thrift_version);

  // Verify equality (field by field since no operator==)
  EXPECT_EQ(original.buffer, roundtrip.buffer);
  EXPECT_EQ(original.index, roundtrip.index);
  EXPECT_EQ(original.packed_index, roundtrip.packed_index);
  EXPECT_EQ(original.symtab.has_value(), roundtrip.symtab.has_value());
}

TEST(ConverterRoundTripTest, StringTableWithData) {
  // Create string table with data
  string_table original;
  original.buffer = "hello\0world\0test";
  original.index = {0, 6, 12};
  original.packed_index = false;

  // Round-trip: C++ → Thrift → C++
  auto thrift_version = to_thrift(original);
  string_table roundtrip = from_thrift(thrift_version);

  // Verify equality
  EXPECT_EQ(original.buffer, roundtrip.buffer);
  EXPECT_EQ(original.index, roundtrip.index);
  EXPECT_EQ(original.packed_index, roundtrip.packed_index);
  EXPECT_EQ(original.symtab.has_value(), roundtrip.symtab.has_value());
}

TEST(ConverterRoundTripTest, StringTableWithFSST) {
  // Create string table with FSST compression
  string_table original;
  original.buffer = "compressed";
  original.index = {0};
  original.packed_index = true;
  original.symtab = "FSST_SYMBOL_TABLE";

  // Round-trip: C++ → Thrift → C++
  auto thrift_version = to_thrift(original);
  string_table roundtrip = from_thrift(thrift_version);

  // Verify equality
  EXPECT_EQ(original.buffer, roundtrip.buffer);
  EXPECT_EQ(original.index, roundtrip.index);
  EXPECT_EQ(original.packed_index, roundtrip.packed_index);
  ASSERT_TRUE(roundtrip.symtab.has_value());
  EXPECT_EQ(original.symtab.value(), roundtrip.symtab.value());
}

// ============================================================================
// chunk Round-Trip Tests
// ============================================================================

TEST(ConverterRoundTripTest, ChunkBasic) {
  chunk original;
  original.block = 42;
  original.offset = 1024;
  original.size = 4096;

  auto thrift_version = to_thrift(original);
  chunk roundtrip = from_thrift(thrift_version);

  EXPECT_EQ(original.block, roundtrip.block);
  EXPECT_EQ(original.offset, roundtrip.offset);
  EXPECT_EQ(original.size, roundtrip.size);
}

// ============================================================================
// directory Round-Trip Tests
// ============================================================================

TEST(ConverterRoundTripTest, DirectoryBasic) {
  directory original;
  original.parent_entry = 1;
  original.first_entry = 2;
  original.self_entry = 3;

  auto thrift_version = to_thrift(original);
  directory roundtrip = from_thrift(thrift_version);

  EXPECT_EQ(original.parent_entry, roundtrip.parent_entry);
  EXPECT_EQ(original.first_entry, roundtrip.first_entry);
  EXPECT_EQ(original.self_entry, roundtrip.self_entry);
}

// ============================================================================
// metadata Round-Trip Tests (Full Integration)
// ============================================================================

TEST(ConverterRoundTripTest, MetadataMinimal) {
  metadata original;
  original.timestamp_base = 1234567890;
  original.block_size = 65536;
  original.total_fs_size = 1048576;

  // Add one chunk
  chunk c;
  c.block = 0;
  c.offset = 0;
  c.size = 100;
  original.chunks.push_back(c);

  // Round-trip
  auto thrift_version = to_thrift(original);
  metadata roundtrip = from_thrift(thrift_version);

  // Verify core fields
  EXPECT_EQ(original.timestamp_base, roundtrip.timestamp_base);
  EXPECT_EQ(original.block_size, roundtrip.block_size);
  EXPECT_EQ(original.total_fs_size, roundtrip.total_fs_size);
  EXPECT_EQ(original.chunks.size(), roundtrip.chunks.size());
}

TEST(ConverterRoundTripTest, MetadataWithStringTables) {
  metadata original;
  original.timestamp_base = 1234567890;
  original.block_size = 65536;
  original.total_fs_size = 1048576;

  // Add compact names
  string_table names;
  names.buffer = "test.txt";
  names.index = {0};
  names.packed_index = false;
  original.compact_names = names;

  // Round-trip
  auto thrift_version = to_thrift(original);
  metadata roundtrip = from_thrift(thrift_version);

  // Verify string table preserved
  ASSERT_TRUE(roundtrip.compact_names.has_value());
  EXPECT_EQ(original.compact_names->buffer, roundtrip.compact_names->buffer);
  EXPECT_EQ(original.compact_names->index, roundtrip.compact_names->index);
  EXPECT_EQ(original.compact_names->packed_index, roundtrip.compact_names->packed_index);
}

#endif // DWARFS_HAVE_EXPERIMENTAL_THRIFT