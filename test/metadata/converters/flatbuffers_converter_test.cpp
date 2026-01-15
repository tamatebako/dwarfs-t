/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file flatbuffers_converter_test.cpp
 *
 * Tests for FlatBuffers ↔ Domain model converters
 *
 * NO PREPROCESSOR GUARDS: CMake controls compilation of this file.
 * This file is ONLY compiled when DWARFS_WITH_FLATBUFFERS=ON.
 *
 * \copyright See LICENSE file
 */

#include <gtest/gtest.h>
#include "dwarfs/metadata/converters/domain_flatbuffers_converter.h"
#include <dwarfs/gen-flatbuffers/metadata.h>
#include <flatbuffers/flatbuffers.h>

using namespace dwarfs::metadata;

// ============================================================================
// Chunk Converter Tests
// ============================================================================

TEST(FlatBuffersChunkConverter, ToDomain) {
  flatbuffers::FlatBufferBuilder builder;
  auto chunk_offset = dwarfs::flatbuffers::CreateChunk(builder, 42, 1024, 4096);
  builder.Finish(chunk_offset);

  auto* fb_chunk = flatbuffers::GetRoot<dwarfs::flatbuffers::Chunk>(builder.GetBufferPointer());
  auto domain_chunk = converters::from_flatbuffers(*fb_chunk);

  EXPECT_EQ(domain_chunk.block(), 42u);
  EXPECT_EQ(domain_chunk.offset(), 1024u);
  EXPECT_EQ(domain_chunk.size(), 4096u);
}

TEST(FlatBuffersChunkConverter, FromDomain) {
  domain::chunk domain_chunk(42, 1024, 4096);
  flatbuffers::FlatBufferBuilder builder;

  auto chunk_offset = converters::to_flatbuffers(builder, domain_chunk);
  builder.Finish(chunk_offset);

  auto* fb_chunk = flatbuffers::GetRoot<dwarfs::flatbuffers::Chunk>(builder.GetBufferPointer());

  EXPECT_EQ(fb_chunk->block(), 42u);
  EXPECT_EQ(fb_chunk->offset(), 1024u);
  EXPECT_EQ(fb_chunk->size(), 4096u);
}

TEST(FlatBuffersChunkConverter, RoundTrip) {
  domain::chunk original(42, 1024, 4096);
  flatbuffers::FlatBufferBuilder builder;

  auto chunk_offset = converters::to_flatbuffers(builder, original);
  builder.Finish(chunk_offset);

  auto* fb_chunk = flatbuffers::GetRoot<dwarfs::flatbuffers::Chunk>(builder.GetBufferPointer());
  auto restored = converters::from_flatbuffers(*fb_chunk);

  EXPECT_EQ(original, restored);
}

// ============================================================================
// Directory Converter Tests
// ============================================================================

TEST(FlatBuffersDirectoryConverter, ToDomain) {
  flatbuffers::FlatBufferBuilder builder;
  auto dir_offset = dwarfs::flatbuffers::CreateDirectory(builder, 1, 2, 3);
  builder.Finish(dir_offset);

  auto* fb_dir = flatbuffers::GetRoot<dwarfs::flatbuffers::Directory>(builder.GetBufferPointer());
  auto domain_dir = converters::from_flatbuffers(*fb_dir);

  EXPECT_EQ(domain_dir.parent_entry(), 1u);
  EXPECT_EQ(domain_dir.first_entry(), 2u);
  EXPECT_EQ(domain_dir.self_entry(), 3u);
}

TEST(FlatBuffersDirectoryConverter, FromDomain) {
  domain::directory domain_dir(1, 2, 3);
  flatbuffers::FlatBufferBuilder builder;

  auto dir_offset = converters::to_flatbuffers(builder, domain_dir);
  builder.Finish(dir_offset);

  auto* fb_dir = flatbuffers::GetRoot<dwarfs::flatbuffers::Directory>(builder.GetBufferPointer());

  EXPECT_EQ(fb_dir->parent_entry(), 1u);
  EXPECT_EQ(fb_dir->first_entry(), 2u);
  EXPECT_EQ(fb_dir->self_entry(), 3u);
}

TEST(FlatBuffersDirectoryConverter, RoundTrip) {
  domain::directory original(1, 2, 3);
  flatbuffers::FlatBufferBuilder builder;

  auto dir_offset = converters::to_flatbuffers(builder, original);
  builder.Finish(dir_offset);

  auto* fb_dir = flatbuffers::GetRoot<dwarfs::flatbuffers::Directory>(builder.GetBufferPointer());
  auto restored = converters::from_flatbuffers(*fb_dir);

  EXPECT_EQ(original, restored);
}

// ============================================================================
// Dir Entry Converter Tests
// ============================================================================

TEST(FlatBuffersDirEntryConverter, ToDomain) {
  flatbuffers::FlatBufferBuilder builder;
  auto entry_offset = dwarfs::flatbuffers::CreateDirEntry(builder, 10, 20);
  builder.Finish(entry_offset);

  auto* fb_entry = flatbuffers::GetRoot<dwarfs::flatbuffers::DirEntry>(builder.GetBufferPointer());
  auto domain_entry = converters::from_flatbuffers(*fb_entry);

  EXPECT_EQ(domain_entry.name_index(), 10u);
  EXPECT_EQ(domain_entry.inode_num(), 20u);
}

TEST(FlatBuffersDirEntryConverter, FromDomain) {
  domain::dir_entry domain_entry(10, 20);
  flatbuffers::FlatBufferBuilder builder;

  auto entry_offset = converters::to_flatbuffers(builder, domain_entry);
  builder.Finish(entry_offset);

  auto* fb_entry = flatbuffers::GetRoot<dwarfs::flatbuffers::DirEntry>(builder.GetBufferPointer());

  EXPECT_EQ(fb_entry->name_index(), 10u);
  EXPECT_EQ(fb_entry->inode_num(), 20u);
}

TEST(FlatBuffersDirEntryConverter, RoundTrip) {
  domain::dir_entry original(10, 20);
  flatbuffers::FlatBufferBuilder builder;

  auto entry_offset = converters::to_flatbuffers(builder, original);
  builder.Finish(entry_offset);

  auto* fb_entry = flatbuffers::GetRoot<dwarfs::flatbuffers::DirEntry>(builder.GetBufferPointer());
  auto restored = converters::from_flatbuffers(*fb_entry);

  EXPECT_EQ(original, restored);
}

// ============================================================================
// Inode Data Converter Tests
// ============================================================================

TEST(FlatBuffersInodeDataConverter, CoreFields) {
  flatbuffers::FlatBufferBuilder builder;
  auto inode_offset = dwarfs::flatbuffers::CreateInodeData(
    builder,
    1,    // mode_index
    2,    // owner_index
    3,    // group_index
    100,  // atime_offset
    200,  // mtime_offset
    300   // ctime_offset
  );
  builder.Finish(inode_offset);

  auto* fb_inode = flatbuffers::GetRoot<dwarfs::flatbuffers::InodeData>(builder.GetBufferPointer());
  auto domain_inode = converters::from_flatbuffers(*fb_inode);

  EXPECT_EQ(domain_inode.mode_index, 1u);
  EXPECT_EQ(domain_inode.owner_index, 2u);
  EXPECT_EQ(domain_inode.group_index, 3u);
  EXPECT_EQ(domain_inode.atime_offset, 100u);
  EXPECT_EQ(domain_inode.mtime_offset, 200u);
  EXPECT_EQ(domain_inode.ctime_offset, 300u);
}

TEST(FlatBuffersInodeDataConverter, OptionalFields) {
  flatbuffers::FlatBufferBuilder builder;
  auto inode_offset = dwarfs::flatbuffers::CreateInodeData(
    builder,
    1, 2, 3, 100, 200, 300,  // core fields
    400,  // btime_offset
    10,   // atime_subsec
    20,   // mtime_subsec
    30,   // ctime_subsec
    40,   // btime_subsec
    5     // nlink_minus_one
  );
  builder.Finish(inode_offset);

  auto* fb_inode = flatbuffers::GetRoot<dwarfs::flatbuffers::InodeData>(builder.GetBufferPointer());
  auto domain_inode = converters::from_flatbuffers(*fb_inode);

  EXPECT_EQ(domain_inode.btime_offset, 400u);
  EXPECT_EQ(domain_inode.atime_subsec, 10u);
  EXPECT_EQ(domain_inode.mtime_subsec, 20u);
  EXPECT_EQ(domain_inode.ctime_subsec, 30u);
  EXPECT_EQ(domain_inode.btime_subsec, 40u);
  EXPECT_EQ(domain_inode.nlink_minus_one, 5u);
}

TEST(FlatBuffersInodeDataConverter, RoundTrip) {
  domain::inode_data original;
  original.mode_index = 1;
  original.owner_index = 2;
  original.group_index = 3;
  original.atime_offset = 100;
  original.mtime_offset = 200;
  original.ctime_offset = 300;
  original.btime_offset = 400;
  original.nlink_minus_one = 5;

  flatbuffers::FlatBufferBuilder builder;
  auto inode_offset = converters::to_flatbuffers(builder, original);
  builder.Finish(inode_offset);

  auto* fb_inode = flatbuffers::GetRoot<dwarfs::flatbuffers::InodeData>(builder.GetBufferPointer());
  auto restored = converters::from_flatbuffers(*fb_inode);

  EXPECT_EQ(original.mode_index, restored.mode_index);
  EXPECT_EQ(original.owner_index, restored.owner_index);
  EXPECT_EQ(original.group_index, restored.group_index);
  EXPECT_EQ(original.atime_offset, restored.atime_offset);
  EXPECT_EQ(original.mtime_offset, restored.mtime_offset);
  EXPECT_EQ(original.ctime_offset, restored.ctime_offset);
  EXPECT_EQ(original.btime_offset, restored.btime_offset);
  EXPECT_EQ(original.nlink_minus_one, restored.nlink_minus_one);
}

// ============================================================================
// FsOptions Converter Tests
// ============================================================================

TEST(FlatBuffersFsOptionsConverter, AllFields) {
  flatbuffers::FlatBufferBuilder builder;
  auto opts_offset = dwarfs::flatbuffers::CreateFsOptions(
    builder,
    true,   // mtime_only
    60,     // time_resolution_sec
    1000,   // subsecond_resolution_nsec_multiplier
    true,   // packed_chunk_table
    true,   // packed_directories
    true,   // packed_shared_files_table
    true,   // has_btime
    true    // inodes_have_nlink
  );
  builder.Finish(opts_offset);

  auto* fb_opts = flatbuffers::GetRoot<dwarfs::flatbuffers::FsOptions>(builder.GetBufferPointer());
  auto domain_opts = converters::from_flatbuffers(*fb_opts);

  EXPECT_TRUE(domain_opts.mtime_only);
  EXPECT_EQ(domain_opts.time_resolution_sec, 60u);
  EXPECT_EQ(domain_opts.subsecond_resolution_nsec_multiplier, 1000u);
  EXPECT_TRUE(domain_opts.packed_chunk_table);
  EXPECT_TRUE(domain_opts.packed_directories);
  EXPECT_TRUE(domain_opts.packed_shared_files_table);
  EXPECT_TRUE(domain_opts.has_btime);
  EXPECT_TRUE(domain_opts.inodes_have_nlink);
}

TEST(FlatBuffersFsOptionsConverter, RoundTrip) {
  domain::fs_options original;
  original.mtime_only = true;
  original.time_resolution_sec = 60;
  original.subsecond_resolution_nsec_multiplier = 1000;
  original.packed_chunk_table = true;
  original.packed_directories = true;
  original.packed_shared_files_table = true;
  original.has_btime = true;
  original.inodes_have_nlink = true;

  flatbuffers::FlatBufferBuilder builder;
  auto opts_offset = converters::to_flatbuffers(builder, original);
  builder.Finish(opts_offset);

  auto* fb_opts = flatbuffers::GetRoot<dwarfs::flatbuffers::FsOptions>(builder.GetBufferPointer());
  auto restored = converters::from_flatbuffers(*fb_opts);

  EXPECT_EQ(original.mtime_only, restored.mtime_only);
  EXPECT_EQ(original.time_resolution_sec, restored.time_resolution_sec);
  EXPECT_EQ(original.subsecond_resolution_nsec_multiplier,
            restored.subsecond_resolution_nsec_multiplier);
  EXPECT_EQ(original.packed_chunk_table, restored.packed_chunk_table);
  EXPECT_EQ(original.packed_directories, restored.packed_directories);
  EXPECT_EQ(original.packed_shared_files_table, restored.packed_shared_files_table);
  EXPECT_EQ(original.has_btime, restored.has_btime);
  EXPECT_EQ(original.inodes_have_nlink, restored.inodes_have_nlink);
}

// ============================================================================
// StringTable Converter Tests
// ============================================================================

TEST(FlatBuffersStringTableConverter, BasicTable) {
  flatbuffers::FlatBufferBuilder builder;
  // Use CreateVector for binary data (not CreateString)
  auto buffer_offset = builder.CreateVector(
      reinterpret_cast<const uint8_t*>("hello\0world"), 11);
  std::vector<uint32_t> index = {0, 6};
  auto index_offset = builder.CreateVector(index);

  auto table_offset = dwarfs::flatbuffers::CreateStringTable(
    builder,
    buffer_offset,
    0,  // no symtab
    index_offset,
    false  // not packed
  );
  builder.Finish(table_offset);

  auto* fb_table = flatbuffers::GetRoot<dwarfs::flatbuffers::StringTable>(builder.GetBufferPointer());
  auto domain_table = converters::from_flatbuffers(*fb_table);

  EXPECT_EQ(domain_table.buffer.size(), 11u);
  EXPECT_EQ(domain_table.index.size(), 2u);
  EXPECT_EQ(domain_table.index[0], 0u);
  EXPECT_EQ(domain_table.index[1], 6u);
  EXPECT_FALSE(domain_table.packed_index);
  EXPECT_FALSE(domain_table.symtab.has_value());
}

TEST(FlatBuffersStringTableConverter, WithFSST) {
  domain::string_table original;
  original.buffer = "compressed_data";
  original.symtab = "fsst_symbol_table";
  original.index = {0, 5, 10};
  original.packed_index = true;

  flatbuffers::FlatBufferBuilder builder;
  auto table_offset = converters::to_flatbuffers(builder, original);
  builder.Finish(table_offset);

  auto* fb_table = flatbuffers::GetRoot<dwarfs::flatbuffers::StringTable>(builder.GetBufferPointer());
  auto restored = converters::from_flatbuffers(*fb_table);

  EXPECT_EQ(original.buffer, restored.buffer);
  EXPECT_EQ(original.symtab, restored.symtab);
  EXPECT_EQ(original.index, restored.index);
  EXPECT_EQ(original.packed_index, restored.packed_index);
}

// ============================================================================
// Main Test Entry Point
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}