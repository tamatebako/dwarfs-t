/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file test_cpp_thrift_converter.cpp
 *
 * Test bidirectional conversion between Thrift and plain C++ metadata structures.
 *
 * This test demonstrates round-trip conversion correctness by:
 * 1. Creating Thrift metadata structures
 * 2. Converting to C++ structures
 * 3. Converting back to Thrift
 * 4. Verifying equality
 *
 * \author Ribose
 * \date 2025-11-12
 */

#ifdef DWARFS_HAVE_THRIFT

#include "dwarfs/internal/cpp_thrift_converter.h"
#include <cassert>
#include <iostream>

using namespace dwarfs::internal;

void test_chunk_roundtrip() {
  std::cout << "Testing chunk round-trip conversion..." << std::endl;

  // Create Thrift chunk
  thrift::metadata::chunk tc;
  tc.block() = 42;
  tc.offset() = 1024;
  tc.size() = 2048;

  // Convert to C++
  auto cpp_chunk = from_thrift(tc);

  // Verify C++ values
  assert(cpp_chunk.block == 42);
  assert(cpp_chunk.offset == 1024);
  assert(cpp_chunk.size == 2048);

  // Convert back to Thrift
  auto tc2 = to_thrift(cpp_chunk);

  // Verify round-trip equality
  assert(tc2.block() == tc.block());
  assert(tc2.offset() == tc.offset());
  assert(tc2.size() == tc.size());

  std::cout << "✓ chunk round-trip successful" << std::endl;
}

void test_directory_roundtrip() {
  std::cout << "Testing directory round-trip conversion..." << std::endl;

  // Create Thrift directory
  thrift::metadata::directory td;
  td.parent_entry() = 10;
  td.first_entry() = 20;
  td.self_entry() = 15;

  // Convert to C++
  auto cpp_dir = from_thrift(td);

  // Verify C++ values
  assert(cpp_dir.parent_entry == 10);
  assert(cpp_dir.first_entry == 20);
  assert(cpp_dir.self_entry == 15);

  // Convert back to Thrift
  auto td2 = to_thrift(cpp_dir);

  // Verify round-trip equality
  assert(td2.parent_entry() == td.parent_entry());
  assert(td2.first_entry() == td.first_entry());
  assert(td2.self_entry() == td.self_entry());

  std::cout << "✓ directory round-trip successful" << std::endl;
}

void test_inode_data_roundtrip() {
  std::cout << "Testing inode_data round-trip conversion..." << std::endl;

  // Create Thrift inode_data
  thrift::metadata::inode_data ti;
  ti.mode_index() = 1;
  ti.owner_index() = 1000;
  ti.group_index() = 1000;
  ti.atime_offset() = 100;
  ti.mtime_offset() = 200;
  ti.ctime_offset() = 300;
  ti.btime_offset() = 400;
  ti.atime_subsec() = 500000;
  ti.mtime_subsec() = 600000;
  ti.nlink_minus_one() = 2;

  // Convert to C++
  auto cpp_inode = from_thrift(ti);

  // Verify C++ values
  assert(cpp_inode.mode_index == 1);
  assert(cpp_inode.owner_index == 1000);
  assert(cpp_inode.group_index == 1000);
  assert(cpp_inode.atime_offset == 100);
  assert(cpp_inode.mtime_offset == 200);
  assert(cpp_inode.ctime_offset == 300);
  assert(cpp_inode.btime_offset == 400);
  assert(cpp_inode.atime_subsec == 500000);
  assert(cpp_inode.mtime_subsec == 600000);
  assert(cpp_inode.nlink_minus_one == 2);

  // Convert back to Thrift
  auto ti2 = to_thrift(cpp_inode);

  // Verify round-trip equality for required fields
  assert(ti2.mode_index() == ti.mode_index());
  assert(ti2.owner_index() == ti.owner_index());
  assert(ti2.group_index() == ti.group_index());
  assert(ti2.atime_offset() == ti.atime_offset());
  assert(ti2.mtime_offset() == ti.mtime_offset());
  assert(ti2.ctime_offset() == ti.ctime_offset());

  // Verify optional fields
  assert(ti2.btime_offset().has_value());
  assert(ti2.btime_offset().value() == 400);
  assert(ti2.nlink_minus_one().has_value());
  assert(ti2.nlink_minus_one().value() == 2);

  std::cout << "✓ inode_data round-trip successful" << std::endl;
}

void test_fs_options_roundtrip() {
  std::cout << "Testing fs_options round-trip conversion..." << std::endl;

  // Create Thrift fs_options
  thrift::metadata::fs_options to;
  to.mtime_only() = true;
  to.time_resolution_sec() = 1;
  to.packed_chunk_table() = false;
  to.packed_directories() = true;
  to.packed_shared_files_table() = false;
  to.subsecond_resolution_nsec_multiplier() = 1000;
  to.has_btime() = true;
  to.inodes_have_nlink() = true;

  // Convert to C++
  auto cpp_opts = from_thrift(to);

  // Verify C++ values
  assert(cpp_opts.mtime_only == true);
  assert(cpp_opts.time_resolution_sec.has_value());
  assert(cpp_opts.time_resolution_sec.value() == 1);
  assert(cpp_opts.packed_chunk_table == false);
  assert(cpp_opts.packed_directories == true);
  assert(cpp_opts.has_btime == true);
  assert(cpp_opts.inodes_have_nlink == true);

  // Convert back to Thrift
  auto to2 = to_thrift(cpp_opts);

  // Verify round-trip equality
  assert(to2.mtime_only() == to.mtime_only());
  assert(to2.packed_chunk_table() == to.packed_chunk_table());
  assert(to2.packed_directories() == to.packed_directories());
  assert(to2.has_btime() == to.has_btime());
  assert(to2.time_resolution_sec().has_value());
  assert(to2.time_resolution_sec().value() == 1);

  std::cout << "✓ fs_options round-trip successful" << std::endl;
}

void test_string_table_roundtrip() {
  std::cout << "Testing string_table round-trip conversion..." << std::endl;

  // Create Thrift string_table
  thrift::metadata::string_table tst;
  tst.buffer() = "file1\0file2\0file3\0";
  tst.packed_index() = false;
  std::vector<uint32_t> index = {0, 6, 12};
  tst.index() = index;
  tst.symtab() = "FSST_SYMBOL_TABLE";

  // Convert to C++
  auto cpp_st = from_thrift(tst);

  // Verify C++ values
  assert(cpp_st.buffer == "file1\0file2\0file3\0");
  assert(cpp_st.packed_index == false);
  assert(cpp_st.index.size() == 3);
  assert(cpp_st.symtab.has_value());
  assert(cpp_st.symtab.value() == "FSST_SYMBOL_TABLE");

  // Convert back to Thrift
  auto tst2 = to_thrift(cpp_st);

  // Verify round-trip equality
  assert(tst2.buffer() == tst.buffer());
  assert(tst2.packed_index() == tst.packed_index());
  assert(tst2.index()->size() == index.size());
  assert(tst2.symtab().has_value());
  assert(tst2.symtab().value() == "FSST_SYMBOL_TABLE");

  std::cout << "✓ string_table round-trip successful" << std::endl;
}

void test_metadata_roundtrip() {
  std::cout << "Testing metadata round-trip conversion..." << std::endl;

  // Create Thrift metadata with core fields
  thrift::metadata::metadata tm;

  // Add chunks
  thrift::metadata::chunk tc1, tc2;
  tc1.block() = 1;
  tc1.offset() = 0;
  tc1.size() = 1024;
  tc2.block() = 2;
  tc2.offset() = 1024;
  tc2.size() = 2048;
  std::vector<thrift::metadata::chunk> chunks = {tc1, tc2};
  tm.chunks() = chunks;

  // Add basic fields
  tm.timestamp_base() = 1609459200; // 2021-01-01 00:00:00 UTC
  tm.block_size() = 131072;
  tm.total_fs_size() = 1048576;

  // Add some lookup tables
  std::vector<uint32_t> uids = {0, 1000, 1001};
  std::vector<uint32_t> gids = {0, 1000};
  std::vector<uint32_t> modes = {0644, 0755};
  tm.uids() = uids;
  tm.gids() = gids;
  tm.modes() = modes;

  // Add optional v2.3+ field
  tm.dwarfs_version() = "0.7.0";
  tm.create_timestamp() = 1609459200;

  // Convert to C++
  auto cpp_meta = from_thrift(tm);

  // Verify C++ values
  assert(cpp_meta.chunks.size() == 2);
  assert(cpp_meta.chunks[0].block == 1);
  assert(cpp_meta.chunks[1].size == 2048);
  assert(cpp_meta.timestamp_base == 1609459200);
  assert(cpp_meta.block_size == 131072);
  assert(cpp_meta.total_fs_size == 1048576);
  assert(cpp_meta.uids.size() == 3);
  assert(cpp_meta.gids.size() == 2);
  assert(cpp_meta.modes.size() == 2);
  assert(cpp_meta.dwarfs_version.has_value());
  assert(cpp_meta.dwarfs_version.value() == "0.7.0");
  assert(cpp_meta.create_timestamp.has_value());
  assert(cpp_meta.create_timestamp.value() == 1609459200);

  // Convert back to Thrift
  auto tm2 = to_thrift(cpp_meta);

  // Verify round-trip equality
  assert(tm2.chunks()->size() == 2);
  assert(tm2.timestamp_base() == tm.timestamp_base());
  assert(tm2.block_size() == tm.block_size());
  assert(tm2.total_fs_size() == tm.total_fs_size());
  assert(tm2.uids()->size() == 3);
  assert(tm2.dwarfs_version().has_value());
  assert(tm2.dwarfs_version().value() == "0.7.0");

  std::cout << "✓ metadata round-trip successful" << std::endl;
}

int main() {
  std::cout << "=== C++ ↔ Thrift Converter Round-Trip Tests ===" << std::endl;
  std::cout << std::endl;

  try {
    test_chunk_roundtrip();
    test_directory_roundtrip();
    test_inode_data_roundtrip();
    test_fs_options_roundtrip();
    test_string_table_roundtrip();
    test_metadata_roundtrip();

    std::cout << std::endl;
    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Test failed with exception: " << e.what() << std::endl;
    return 1;
  }
}

#else

#include <iostream>

int main() {
  std::cout << "Thrift support not enabled (DWARFS_HAVE_THRIFT not defined)" << std::endl;
  return 0;
}

#endif // DWARFS_HAVE_THRIFT