/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file cpp_thrift_converter_test.cpp
 *
 * Tests for bidirectional conversion between Thrift and plain C++ metadata structures.
 *
 * \copyright See LICENSE file
 */

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT

#include <gtest/gtest.h>

#include "dwarfs/internal/cpp_thrift_converter.h"

using namespace dwarfs::internal;
namespace thrift = dwarfs::thrift;

// ============================================================================
// Chunk Converter Tests
// ============================================================================

TEST(CppThriftChunkConverter, FromThrift) {
  thrift::metadata::chunk tc;
  tc.block() = 42;
  tc.offset() = 1024;
  tc.size() = 4096;

  auto cpp_chunk = from_thrift(tc);

  EXPECT_EQ(cpp_chunk.block, 42u);
  EXPECT_EQ(cpp_chunk.offset, 1024u);
  EXPECT_EQ(cpp_chunk.size, 4096u);
}

TEST(CppThriftChunkConverter, ToThrift) {
  chunk c;
  c.block = 42;
  c.offset = 1024;
  c.size = 4096;

  auto tc = to_thrift(c);

  EXPECT_TRUE(tc.block().has_value());
  EXPECT_TRUE(tc.offset().has_value());
  EXPECT_TRUE(tc.size().has_value());
  EXPECT_EQ(tc.block().value(), 42u);
  EXPECT_EQ(tc.offset().value(), 1024u);
  EXPECT_EQ(tc.size().value(), 4096u);
}

TEST(CppThriftChunkConverter, RoundTrip) {
  chunk original;
  original.block = 42;
  original.offset = 1024;
  original.size = 4096;

  auto tc = to_thrift(original);
  auto restored = from_thrift(tc);

  EXPECT_EQ(original.block, restored.block);
  EXPECT_EQ(original.offset, restored.offset);
  EXPECT_EQ(original.size, restored.size);
}

// ============================================================================
// Directory Converter Tests
// ============================================================================

TEST(CppThriftDirectoryConverter, FromThrift) {
  thrift::metadata::directory td;
  td.parent_entry() = 10;
  td.first_entry() = 20;
  td.self_entry() = 15;

  auto cpp_dir = from_thrift(td);

  EXPECT_EQ(cpp_dir.parent_entry, 10u);
  EXPECT_EQ(cpp_dir.first_entry, 20u);
  EXPECT_EQ(cpp_dir.self_entry, 15u);
}

TEST(CppThriftDirectoryConverter, ToThrift) {
  directory d;
  d.parent_entry = 10;
  d.first_entry = 20;
  d.self_entry = 15;

  auto td = to_thrift(d);

  EXPECT_TRUE(td.parent_entry().has_value());
  EXPECT_TRUE(td.first_entry().has_value());
  EXPECT_TRUE(td.self_entry().has_value());
  EXPECT_EQ(td.parent_entry().value(), 10u);
  EXPECT_EQ(td.first_entry().value(), 20u);
  EXPECT_EQ(td.self_entry().value(), 15u);
}

TEST(CppThriftDirectoryConverter, RoundTrip) {
  directory original;
  original.parent_entry = 10;
  original.first_entry = 20;
  original.self_entry = 15;

  auto td = to_thrift(original);
  auto restored = from_thrift(td);

  EXPECT_EQ(original.parent_entry, restored.parent_entry);
  EXPECT_EQ(original.first_entry, restored.first_entry);
  EXPECT_EQ(original.self_entry, restored.self_entry);
}

// ============================================================================
// Inode Data Converter Tests
// ============================================================================

TEST(CppThriftInodeDataConverter, FromThrift) {
  thrift::metadata::inode_data ti;
  ti.mode_index() = 0;
  ti.owner_index() = 1;
  ti.group_index() = 2;
  ti.atime_offset() = 100;
  ti.mtime_offset() = 200;
  ti.ctime_offset() = 300;

  auto cpp_inode = from_thrift(ti);

  EXPECT_EQ(cpp_inode.mode_index, 0u);
  EXPECT_EQ(cpp_inode.owner_index, 1u);
  EXPECT_EQ(cpp_inode.group_index, 2u);
  EXPECT_EQ(cpp_inode.atime_offset, 100u);
  EXPECT_EQ(cpp_inode.mtime_offset, 200u);
  EXPECT_EQ(cpp_inode.ctime_offset, 300u);
}

TEST(CppThriftInodeDataConverter, ToThrift) {
  inode_data i;
  i.mode_index = 0;
  i.owner_index = 1;
  i.group_index = 2;
  i.atime_offset = 100;
  i.mtime_offset = 200;
  i.ctime_offset = 300;

  auto ti = to_thrift(i);

  EXPECT_TRUE(ti.mode_index().has_value());
  EXPECT_TRUE(ti.owner_index().has_value());
  EXPECT_TRUE(ti.group_index().has_value());
  EXPECT_EQ(ti.mode_index().value(), 0u);
  EXPECT_EQ(ti.owner_index().value(), 1u);
  EXPECT_EQ(ti.group_index().value(), 2u);
}

TEST(CppThriftInodeDataConverter, RoundTrip) {
  inode_data original;
  original.mode_index = 0;
  original.owner_index = 1;
  original.group_index = 2;
  original.atime_offset = 100;
  original.mtime_offset = 200;
  original.ctime_offset = 300;

  auto ti = to_thrift(original);
  auto restored = from_thrift(ti);

  EXPECT_EQ(original.mode_index, restored.mode_index);
  EXPECT_EQ(original.owner_index, restored.owner_index);
  EXPECT_EQ(original.group_index, restored.group_index);
}

// ============================================================================
// Dir Entry Converter Tests
// ============================================================================

TEST(CppThriftDirEntryConverter, FromThrift) {
  thrift::metadata::dir_entry te;
  te.name_index() = 5;
  te.inode_num() = 42;

  auto cpp_entry = from_thrift(te);

  EXPECT_EQ(cpp_entry.name_index, 5u);
  EXPECT_EQ(cpp_entry.inode_num, 42u);
}

TEST(CppThriftDirEntryConverter, ToThrift) {
  dir_entry e;
  e.name_index = 5;
  e.inode_num = 42;

  auto te = to_thrift(e);

  EXPECT_TRUE(te.name_index().has_value());
  EXPECT_TRUE(te.inode_num().has_value());
  EXPECT_EQ(te.name_index().value(), 5u);
  EXPECT_EQ(te.inode_num().value(), 42u);
}

TEST(CppThriftDirEntryConverter, RoundTrip) {
  dir_entry original;
  original.name_index = 5;
  original.inode_num = 42;

  auto te = to_thrift(original);
  auto restored = from_thrift(te);

  EXPECT_EQ(original.name_index, restored.name_index);
  EXPECT_EQ(original.inode_num, restored.inode_num);
}

// ============================================================================
// FS Options Converter Tests
// ============================================================================

TEST(CppThriftFsOptionsConverter, FromThrift) {
  thrift::metadata::fs_options to;
  to.mtime_only() = false;
  to.packed_chunk_table() = true;
  to.packed_directories() = false;
  to.has_btime() = true;

  auto cpp_opts = from_thrift(to);

  EXPECT_FALSE(cpp_opts.mtime_only);
  EXPECT_TRUE(cpp_opts.packed_chunk_table);
  EXPECT_FALSE(cpp_opts.packed_directories);
  EXPECT_TRUE(cpp_opts.has_btime);
}

TEST(CppThriftFsOptionsConverter, ToThrift) {
  fs_options opts;
  opts.mtime_only = false;
  opts.packed_chunk_table = true;
  opts.packed_directories = false;
  opts.has_btime = true;

  auto to = to_thrift(opts);

  EXPECT_TRUE(to.mtime_only().has_value());
  EXPECT_TRUE(to.packed_chunk_table().has_value());
  EXPECT_TRUE(to.packed_directories().has_value());
  EXPECT_TRUE(to.has_btime().has_value());
  EXPECT_FALSE(to.mtime_only().value());
  EXPECT_TRUE(to.packed_chunk_table().value());
  EXPECT_FALSE(to.packed_directories().value());
  EXPECT_TRUE(to.has_btime().value());
}

TEST(CppThriftFsOptionsConverter, RoundTrip) {
  fs_options original;
  original.mtime_only = false;
  original.packed_chunk_table = true;
  original.packed_directories = false;
  original.has_btime = true;

  auto to = to_thrift(original);
  auto restored = from_thrift(to);

  EXPECT_EQ(original.mtime_only, restored.mtime_only);
  EXPECT_EQ(original.packed_chunk_table, restored.packed_chunk_table);
  EXPECT_EQ(original.packed_directories, restored.packed_directories);
  EXPECT_EQ(original.has_btime, restored.has_btime);
}

#endif // DWARFS_HAVE_EXPERIMENTAL_THRIFT
