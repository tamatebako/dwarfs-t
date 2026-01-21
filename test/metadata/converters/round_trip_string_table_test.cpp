#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT

#include <gtest/gtest.h>

#include "dwarfs/metadata/converters/thrift_metadata_converter.h"
#include "dwarfs/metadata/domain/metadata.h"
#include <dwarfs/gen-cpp2/metadata_types.h>

using namespace dwarfs::metadata;
using namespace dwarfs::metadata::converters;
using namespace dwarfs::metadata::domain;
namespace thrift = dwarfs::thrift;

/**
 * Comprehensive test for string table and index preservation
 * This test verifies that the name_index references remain valid
 * after round-trip conversion.
 */
TEST(RoundTripStringTableTest, PreservesNameIndicesAndTables) {
  ThriftMetadataConverter converter;

  // Create Thrift metadata with names and dir_entries that reference them
  thrift::metadata::metadata t_meta;
  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024 * 1024;
  *t_meta.timestamp_base() = 1234567890;

  // Populate names vector
  t_meta.names()->push_back("file1.txt");
  t_meta.names()->push_back("file2.txt");
  t_meta.names()->push_back("dir1");

  // Populate symlinks vector
  t_meta.symlinks()->push_back("/tmp/link1");
  t_meta.symlinks()->push_back("/usr/bin/link2");

  // Create dir_entries that reference the names
  {
    thrift::metadata::dir_entry entry;
    *entry.name_index() = 0;  // References "file1.txt"
    *entry.inode_num() = 1;
    t_meta.dir_entries()->push_back(entry);
  }

  {
    thrift::metadata::dir_entry entry;
    *entry.name_index() = 1;  // References "file2.txt"
    *entry.inode_num() = 2;
    t_meta.dir_entries()->push_back(entry);
  }

  {
    thrift::metadata::dir_entry entry;
    *entry.name_index() = 2;  // References "dir1"
    *entry.inode_num() = 3;
    t_meta.dir_entries()->push_back(entry);
  }

  // Add symlink_table that references symlinks
  t_meta.symlink_table()->push_back(0);  // References "/tmp/link1"
  t_meta.symlink_table()->push_back(1);  // References "/usr/bin/link2"

  // Add chunk_table
  t_meta.chunk_table()->push_back(0);
  t_meta.chunk_table()->push_back(1);

  // Perform round-trip conversion
  auto d_meta = converter.to_domain(&t_meta);
  auto t_meta_ptr = converter.from_domain(d_meta);
  auto* final_t_meta =
      static_cast<thrift::metadata::metadata*>(t_meta_ptr.get());

  // Verify names vector is preserved
  ASSERT_EQ(final_t_meta->names()->size(), 3u);
  EXPECT_EQ((*final_t_meta->names())[0], "file1.txt");
  EXPECT_EQ((*final_t_meta->names())[1], "file2.txt");
  EXPECT_EQ((*final_t_meta->names())[2], "dir1");

  // Verify symlinks vector is preserved
  ASSERT_EQ(final_t_meta->symlinks()->size(), 2u);
  EXPECT_EQ((*final_t_meta->symlinks())[0], "/tmp/link1");
  EXPECT_EQ((*final_t_meta->symlinks())[1], "/usr/bin/link2");

  // Verify dir_entries are preserved with correct indices
  ASSERT_TRUE(final_t_meta->dir_entries().has_value());
  ASSERT_EQ(final_t_meta->dir_entries()->size(), 3u);
  EXPECT_EQ(*(*final_t_meta->dir_entries())[0].name_index(), 0u);
  EXPECT_EQ(*(*final_t_meta->dir_entries())[1].name_index(), 1u);
  EXPECT_EQ(*(*final_t_meta->dir_entries())[2].name_index(), 2u);

  // Verify symlink_table is preserved
  ASSERT_EQ(final_t_meta->symlink_table()->size(), 2u);
  EXPECT_EQ((*final_t_meta->symlink_table())[0], 0u);
  EXPECT_EQ((*final_t_meta->symlink_table())[1], 1u);

  // Verify chunk_table is preserved
  ASSERT_EQ(final_t_meta->chunk_table()->size(), 2u);
  EXPECT_EQ((*final_t_meta->chunk_table())[0], 0u);
  EXPECT_EQ((*final_t_meta->chunk_table())[1], 1u);
}

/**
 * Test compact string tables (fsst-compressed names/symlinks)
 */
TEST(RoundTripStringTableTest, PreservesCompactTables) {
  ThriftMetadataConverter converter;

  thrift::metadata::metadata t_meta;
  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024;
  *t_meta.timestamp_base() = 0;

  // Create compact_names
  thrift::metadata::string_table compact_names;
  *compact_names.buffer() = "compressed_data_here";
  compact_names.symtab() = "symbol_table_data";
  compact_names.index()->push_back(0);
  compact_names.index()->push_back(10);
  compact_names.index()->push_back(20);
  *compact_names.packed_index() = true;
  t_meta.compact_names() = compact_names;

  // Create compact_symlinks
  thrift::metadata::string_table compact_symlinks;
  *compact_symlinks.buffer() = "symlink_compressed";
  compact_symlinks.index()->push_back(0);
  compact_symlinks.index()->push_back(8);
  *compact_symlinks.packed_index() = false;
  t_meta.compact_symlinks() = compact_symlinks;

  // Round-trip conversion
  auto d_meta = converter.to_domain(&t_meta);
  auto t_meta_ptr = converter.from_domain(d_meta);
  auto* final_t_meta =
      static_cast<thrift::metadata::metadata*>(t_meta_ptr.get());

  // Verify compact_names
  ASSERT_TRUE(final_t_meta->compact_names().has_value());
  EXPECT_EQ(*final_t_meta->compact_names()->buffer(), "compressed_data_here");
  EXPECT_EQ(*final_t_meta->compact_names()->symtab(), "symbol_table_data");
  EXPECT_EQ(*final_t_meta->compact_names()->packed_index(), true);
  ASSERT_EQ(final_t_meta->compact_names()->index()->size(), 3u);
  EXPECT_EQ((*final_t_meta->compact_names()->index())[0], 0u);
  EXPECT_EQ((*final_t_meta->compact_names()->index())[1], 10u);
  EXPECT_EQ((*final_t_meta->compact_names()->index())[2], 20u);

  // Verify compact_symlinks
  ASSERT_TRUE(final_t_meta->compact_symlinks().has_value());
  EXPECT_EQ(*final_t_meta->compact_symlinks()->buffer(), "symlink_compressed");
  EXPECT_FALSE(final_t_meta->compact_symlinks()->symtab().has_value());
  EXPECT_EQ(*final_t_meta->compact_symlinks()->packed_index(), false);
  ASSERT_EQ(final_t_meta->compact_symlinks()->index()->size(), 2u);
}

/**
 * Test with all index tables populated
 */
TEST(RoundTripStringTableTest, AllIndexTablesPreserved) {
  ThriftMetadataConverter converter;

  thrift::metadata::metadata t_meta;
  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024;
  *t_meta.timestamp_base() = 0;

  // Populate all lookup tables
  t_meta.uids()->push_back(1000);
  t_meta.uids()->push_back(1001);
  t_meta.gids()->push_back(100);
  t_meta.gids()->push_back(101);
  t_meta.modes()->push_back(0644);
  t_meta.modes()->push_back(0755);

  // Round-trip
  auto d_meta = converter.to_domain(&t_meta);
  auto t_meta_ptr = converter.from_domain(d_meta);
  auto* final_t_meta =
      static_cast<thrift::metadata::metadata*>(t_meta_ptr.get());

  // Verify all tables
  ASSERT_EQ(final_t_meta->uids()->size(), 2u);
  EXPECT_EQ((*final_t_meta->uids())[0], 1000u);
  EXPECT_EQ((*final_t_meta->uids())[1], 1001u);

  ASSERT_EQ(final_t_meta->gids()->size(), 2u);
  EXPECT_EQ((*final_t_meta->gids())[0], 100u);
  EXPECT_EQ((*final_t_meta->gids())[1], 101u);

  ASSERT_EQ(final_t_meta->modes()->size(), 2u);
  EXPECT_EQ((*final_t_meta->modes())[0], 0644u);
  EXPECT_EQ((*final_t_meta->modes())[1], 0755u);
}

#else

#include <gtest/gtest.h>

TEST(RoundTripStringTableTest, thrift_unavailable) {
  GTEST_SKIP() << "Thrift not enabled - skipping converter round-trip tests";
}

#endif // DWARFS_HAVE_EXPERIMENTAL_THRIFT