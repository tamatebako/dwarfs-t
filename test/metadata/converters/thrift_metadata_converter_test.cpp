#ifdef DWARFS_HAVE_THRIFT

#include <gtest/gtest.h>

#include "dwarfs/metadata/converters/thrift_metadata_converter.h"
#include "dwarfs/metadata/domain/metadata.h"
#include <dwarfs/gen-cpp2/metadata_types.h>

using namespace dwarfs::metadata;
using namespace dwarfs::metadata::converters;
using namespace dwarfs::metadata::domain;
namespace thrift = dwarfs::thrift;

namespace {

// =====================================================================
// Helper Functions for Creating Test Data
// =====================================================================

/**
 * Create a minimal Thrift metadata for basic testing
 */
thrift::metadata::metadata create_minimal_thrift_metadata() {
  thrift::metadata::metadata t_meta;

  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024 * 1024;
  *t_meta.timestamp_base() = 1234567890;

  return t_meta;
}

/**
 * Create a comprehensive Thrift metadata with all fields populated
 */
thrift::metadata::metadata create_comprehensive_thrift_metadata() {
  thrift::metadata::metadata t_meta;

  // Basic fields
  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024 * 1024;
  *t_meta.timestamp_base() = 1234567890;

  // Add chunks
  thrift::metadata::chunk chunk1;
  *chunk1.block() = 0;
  *chunk1.offset() = 100;
  *chunk1.size() = 200;
  t_meta.chunks()->push_back(chunk1);

  thrift::metadata::chunk chunk2;
  *chunk2.block() = 1;
  *chunk2.offset() = 300;
  *chunk2.size() = 400;
  t_meta.chunks()->push_back(chunk2);

  // Add directories
  thrift::metadata::directory dir;
  *dir.parent_entry() = 0;
  *dir.first_entry() = 1;
  *dir.self_entry() = 2;
  t_meta.directories()->push_back(dir);

  // Add inodes
  thrift::metadata::inode_data inode;
  *inode.mode_index() = 0;
  *inode.owner_index() = 0;
  *inode.group_index() = 0;
  *inode.atime_offset() = 100;
  *inode.mtime_offset() = 200;
  *inode.ctime_offset() = 300;
  inode.btime_offset() = 400;
  inode.atime_subsec() = 1000000;
  inode.mtime_subsec() = 2000000;
  inode.ctime_subsec() = 3000000;
  inode.btime_subsec() = 4000000;
  inode.nlink_minus_one() = 0;
  t_meta.inodes()->push_back(inode);

  // Add tables
  t_meta.chunk_table()->push_back(0);
  t_meta.chunk_table()->push_back(2);
  t_meta.symlink_table()->push_back(0);

  // Add lookup tables
  t_meta.uids()->push_back(1000);
  t_meta.gids()->push_back(1000);
  t_meta.modes()->push_back(0755);
  t_meta.names()->push_back("test.txt");
  t_meta.symlinks()->push_back("/tmp/link");

  // Optional v2.1+ fields
  t_meta.devices() = std::vector<uint64_t>{0x0801};

  thrift::metadata::fs_options opts;
  *opts.mtime_only() = false;
  *opts.packed_chunk_table() = true;
  *opts.packed_directories() = false;
  *opts.packed_shared_files_table() = false;
  *opts.has_btime() = true;
  *opts.inodes_have_nlink() = true;
  opts.time_resolution_sec() = 1;
  t_meta.options() = opts;

  // Optional v2.3+ fields
  thrift::metadata::dir_entry entry;
  *entry.name_index() = 0;
  *entry.inode_num() = 0;
  t_meta.dir_entries() = std::vector<thrift::metadata::dir_entry>{entry};

  t_meta.shared_files_table() = std::vector<uint32_t>{0, 1};
  t_meta.total_hardlink_size() = 512;
  t_meta.dwarfs_version() = "0.10.0";
  t_meta.create_timestamp() = 1234567890;

  // Optional v2.5+ fields
  t_meta.preferred_path_separator() = '/';
  t_meta.features() = std::set<std::string>{"fsst", "bitsery"};
  t_meta.category_names() = std::vector<std::string>{"text", "binary"};
  t_meta.block_categories() = std::vector<uint32_t>{0, 1};

  return t_meta;
}

/**
 * Create a minimal domain metadata for basic testing
 */
metadata create_minimal_domain_metadata() {
  metadata d_meta;
  d_meta.block_size = 4096;
  d_meta.total_fs_size = 1024 * 1024;
  d_meta.timestamp_base = 1234567890;
  return d_meta;
}

} // anonymous namespace

// =====================================================================
// Basic Conversion Tests
// =====================================================================

TEST(ThriftMetadataConverterTest, ConverterName) {
  ThriftMetadataConverter converter;
  EXPECT_EQ(converter.get_name(), "Thrift Metadata Converter");
}

TEST(ThriftMetadataConverterTest, ToDomainNullPointer) {
  ThriftMetadataConverter converter;
  EXPECT_THROW(converter.to_domain(nullptr), std::invalid_argument);
}

TEST(ThriftMetadataConverterTest, ToDomainMinimal) {
  ThriftMetadataConverter converter;
  auto t_meta = create_minimal_thrift_metadata();

  auto d_meta = converter.to_domain(&t_meta);

  EXPECT_EQ(d_meta.block_size, 4096u);
  EXPECT_EQ(d_meta.total_fs_size, 1024u * 1024u);
  EXPECT_EQ(d_meta.timestamp_base, 1234567890u);
  EXPECT_TRUE(d_meta.chunks.empty());
  EXPECT_TRUE(d_meta.directories.empty());
  EXPECT_TRUE(d_meta.inodes.empty());
}

TEST(ThriftMetadataConverterTest, FromDomainMinimal) {
  ThriftMetadataConverter converter;
  auto d_meta = create_minimal_domain_metadata();

  auto t_meta_ptr = converter.from_domain(d_meta);
  auto* t_meta = static_cast<thrift::metadata::metadata*>(t_meta_ptr.get());

  EXPECT_EQ(*t_meta->block_size(), 4096u);
  EXPECT_EQ(*t_meta->total_fs_size(), 1024u * 1024u);
  EXPECT_EQ(*t_meta->timestamp_base(), 1234567890u);
  EXPECT_TRUE(t_meta->chunks()->empty());
  EXPECT_TRUE(t_meta->directories()->empty());
  EXPECT_TRUE(t_meta->inodes()->empty());
}

// =====================================================================
// Chunk Conversion Tests
// =====================================================================

TEST(ThriftMetadataConverterTest, ChunkConversion) {
  ThriftMetadataConverter converter;

  thrift::metadata::metadata t_meta;
  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024;
  *t_meta.timestamp_base() = 0;

  thrift::metadata::chunk t_chunk;
  *t_chunk.block() = 5;
  *t_chunk.offset() = 1024;
  *t_chunk.size() = 512;
  t_meta.chunks()->push_back(t_chunk);

  auto d_meta = converter.to_domain(&t_meta);

  ASSERT_EQ(d_meta.chunks.size(), 1u);
  EXPECT_EQ(d_meta.chunks[0].block(), 5u);
  EXPECT_EQ(d_meta.chunks[0].offset(), 1024u);
  EXPECT_EQ(d_meta.chunks[0].size(), 512u);
}

TEST(ThriftMetadataConverterTest, MultipleChunks) {
  ThriftMetadataConverter converter;

  thrift::metadata::metadata t_meta;
  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024;
  *t_meta.timestamp_base() = 0;

  for (uint32_t i = 0; i < 10; ++i) {
    thrift::metadata::chunk chunk;
    *chunk.block() = i;
    *chunk.offset() = i * 100;
    *chunk.size() = i * 10;
    t_meta.chunks()->push_back(chunk);
  }

  auto d_meta = converter.to_domain(&t_meta);

  ASSERT_EQ(d_meta.chunks.size(), 10u);
  for (uint32_t i = 0; i < 10; ++i) {
    EXPECT_EQ(d_meta.chunks[i].block(), i);
    EXPECT_EQ(d_meta.chunks[i].offset(), i * 100);
    EXPECT_EQ(d_meta.chunks[i].size(), i * 10);
  }
}

// =====================================================================
// Directory Conversion Tests
// =====================================================================

TEST(ThriftMetadataConverterTest, DirectoryConversion) {
  ThriftMetadataConverter converter;

  thrift::metadata::metadata t_meta;
  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024;
  *t_meta.timestamp_base() = 0;

  thrift::metadata::directory t_dir;
  *t_dir.parent_entry() = 10;
  *t_dir.first_entry() = 20;
  *t_dir.self_entry() = 30;
  t_meta.directories()->push_back(t_dir);

  auto d_meta = converter.to_domain(&t_meta);

  ASSERT_EQ(d_meta.directories.size(), 1u);
  EXPECT_EQ(d_meta.directories[0].parent_entry(), 10u);
  EXPECT_EQ(d_meta.directories[0].first_entry(), 20u);
  EXPECT_EQ(d_meta.directories[0].self_entry(), 30u);
}

// =====================================================================
// Inode Conversion Tests
// =====================================================================

TEST(ThriftMetadataConverterTest, InodeConversionBasic) {
  ThriftMetadataConverter converter;

  thrift::metadata::metadata t_meta;
  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024;
  *t_meta.timestamp_base() = 0;

  thrift::metadata::inode_data t_inode;
  *t_inode.mode_index() = 1;
  *t_inode.owner_index() = 2;
  *t_inode.group_index() = 3;
  *t_inode.atime_offset() = 100;
  *t_inode.mtime_offset() = 200;
  *t_inode.ctime_offset() = 300;
  t_meta.inodes()->push_back(t_inode);

  auto d_meta = converter.to_domain(&t_meta);

  ASSERT_EQ(d_meta.inodes.size(), 1u);
  EXPECT_EQ(d_meta.inodes[0].mode_index, 1u);
  EXPECT_EQ(d_meta.inodes[0].owner_index, 2u);
  EXPECT_EQ(d_meta.inodes[0].group_index, 3u);
  EXPECT_EQ(d_meta.inodes[0].atime_offset, 100u);
  EXPECT_EQ(d_meta.inodes[0].mtime_offset, 200u);
  EXPECT_EQ(d_meta.inodes[0].ctime_offset, 300u);
}

TEST(ThriftMetadataConverterTest, InodeConversionWithOptionalFields) {
  ThriftMetadataConverter converter;

  thrift::metadata::metadata t_meta;
  *t_meta.block_size() = 4096;
  *t_meta.total_fs_size() = 1024;
  *t_meta.timestamp_base() = 0;

  thrift::metadata::inode_data t_inode;
  *t_inode.mode_index() = 1;
  *t_inode.owner_index() = 2;
  *t_inode.group_index() = 3;
  *t_inode.atime_offset() = 100;
  *t_inode.mtime_offset() = 200;
  *t_inode.ctime_offset() = 300;
  t_inode.btime_offset() = 400;
  t_inode.atime_subsec() = 1000000;
  t_inode.mtime_subsec() = 2000000;
  t_inode.ctime_subsec() = 3000000;
  t_inode.btime_subsec() = 4000000;
  t_inode.nlink_minus_one() = 5;
  t_meta.inodes()->push_back(t_inode);

  auto d_meta = converter.to_domain(&t_meta);

  ASSERT_EQ(d_meta.inodes.size(), 1u);
  EXPECT_EQ(d_meta.inodes[0].btime_offset, 400u);
  EXPECT_EQ(d_meta.inodes[0].atime_subsec, 1000000u);
  EXPECT_EQ(d_meta.inodes[0].mtime_subsec, 2000000u);
  EXPECT_EQ(d_meta.inodes[0].ctime_subsec, 3000000u);
  EXPECT_EQ(d_meta.inodes[0].btime_subsec, 4000000u);
  EXPECT_EQ(d_meta.inodes[0].nlink_minus_one, 5u);
}

// =====================================================================
// Optional Fields Tests
// =====================================================================

TEST(ThriftMetadataConverterTest, OptionalDevices) {
  ThriftMetadataConverter converter;

  thrift::metadata::metadata t_meta = create_minimal_thrift_metadata();
  t_meta.devices() = std::vector<uint64_t>{0x0801, 0x0802};

  auto d_meta = converter.to_domain(&t_meta);

  ASSERT_TRUE(d_meta.devices.has_value());
  ASSERT_EQ(d_meta.devices->size(), 2u);
  EXPECT_EQ((*d_meta.devices)[0], 0x0801u);
  EXPECT_EQ((*d_meta.devices)[1], 0x0802u);
}

TEST(ThriftMetadataConverterTest, OptionalFeaturesSet) {
  ThriftMetadataConverter converter;

  thrift::metadata::metadata t_meta = create_minimal_thrift_metadata();
  t_meta.features() = std::set<std::string>{"fsst", "bitsery", "sparse"};

  auto d_meta = converter.to_domain(&t_meta);

  ASSERT_TRUE(d_meta.features.has_value());
  EXPECT_EQ(d_meta.features->size(), 3u);
  EXPECT_TRUE(d_meta.features->count("fsst"));
  EXPECT_TRUE(d_meta.features->count("bitsery"));
  EXPECT_TRUE(d_meta.features->count("sparse"));
}

// =====================================================================
// Round-Trip Tests
// =====================================================================

TEST(ThriftMetadataConverterTest, RoundTripMinimal) {
  ThriftMetadataConverter converter;

  auto original_t_meta = create_minimal_thrift_metadata();

  // Thrift → Domain → Thrift
  auto d_meta = converter.to_domain(&original_t_meta);
  auto t_meta_ptr = converter.from_domain(d_meta);
  auto* final_t_meta = static_cast<thrift::metadata::metadata*>(t_meta_ptr.get());

  EXPECT_EQ(*original_t_meta.block_size(), *final_t_meta->block_size());
  EXPECT_EQ(*original_t_meta.total_fs_size(), *final_t_meta->total_fs_size());
  EXPECT_EQ(*original_t_meta.timestamp_base(), *final_t_meta->timestamp_base());
}

TEST(ThriftMetadataConverterTest, RoundTripComprehensive) {
  ThriftMetadataConverter converter;

  auto original_t_meta = create_comprehensive_thrift_metadata();

  // Thrift → Domain → Thrift
  auto d_meta = converter.to_domain(&original_t_meta);
  auto t_meta_ptr = converter.from_domain(d_meta);
  auto* final_t_meta = static_cast<thrift::metadata::metadata*>(t_meta_ptr.get());

  // Verify basic fields
  EXPECT_EQ(*original_t_meta.block_size(), *final_t_meta->block_size());
  EXPECT_EQ(*original_t_meta.total_fs_size(), *final_t_meta->total_fs_size());
  EXPECT_EQ(*original_t_meta.timestamp_base(), *final_t_meta->timestamp_base());

  // Verify chunks
  ASSERT_EQ(original_t_meta.chunks()->size(), final_t_meta->chunks()->size());
  for (size_t i = 0; i < original_t_meta.chunks()->size(); ++i) {
    EXPECT_EQ(*(*original_t_meta.chunks())[i].block(),
              *(*final_t_meta->chunks())[i].block());
    EXPECT_EQ(*(*original_t_meta.chunks())[i].offset(),
              *(*final_t_meta->chunks())[i].offset());
    EXPECT_EQ(*(*original_t_meta.chunks())[i].size(),
              *(*final_t_meta->chunks())[i].size());
  }

  // Verify lookup tables
  EXPECT_EQ(*original_t_meta.uids(), *final_t_meta->uids());
  EXPECT_EQ(*original_t_meta.gids(), *final_t_meta->gids());
  EXPECT_EQ(*original_t_meta.modes(), *final_t_meta->modes());
  EXPECT_EQ(*original_t_meta.names(), *final_t_meta->names());
  EXPECT_EQ(*original_t_meta.symlinks(), *final_t_meta->symlinks());

  // Verify optional fields
  EXPECT_EQ(original_t_meta.dwarfs_version(), final_t_meta->dwarfs_version());
  EXPECT_EQ(original_t_meta.features(), final_t_meta->features());
}

TEST(ThriftMetadataConverterTest, RoundTripDomainFirst) {
  ThriftMetadataConverter converter;

  metadata original_d_meta = create_minimal_domain_metadata();
  original_d_meta.chunks.push_back(chunk(0, 100, 200));
  original_d_meta.names.push_back("test.txt");

  // Domain → Thrift → Domain
  auto t_meta_ptr = converter.from_domain(original_d_meta);
  auto final_d_meta = converter.to_domain(t_meta_ptr.get());

  EXPECT_EQ(original_d_meta.block_size, final_d_meta.block_size);
  EXPECT_EQ(original_d_meta.total_fs_size, final_d_meta.total_fs_size);
  EXPECT_EQ(original_d_meta.timestamp_base, final_d_meta.timestamp_base);
  EXPECT_EQ(original_d_meta.chunks.size(), final_d_meta.chunks.size());
  EXPECT_EQ(original_d_meta.names.size(), final_d_meta.names.size());

  if (!original_d_meta.chunks.empty()) {
    EXPECT_EQ(original_d_meta.chunks[0], final_d_meta.chunks[0]);
  }

  if (!original_d_meta.names.empty()) {
    EXPECT_EQ(original_d_meta.names[0], final_d_meta.names[0]);
  }
}

// =====================================================================
// Edge Cases and Error Handling
// =====================================================================

TEST(ThriftMetadataConverterTest, EmptyVectors) {
  ThriftMetadataConverter converter;

  metadata d_meta;
  d_meta.block_size = 4096;
  d_meta.total_fs_size = 0;
  d_meta.timestamp_base = 0;

  auto t_meta_ptr = converter.from_domain(d_meta);
  auto* t_meta = static_cast<thrift::metadata::metadata*>(t_meta_ptr.get());

  EXPECT_TRUE(t_meta->chunks()->empty());
  EXPECT_TRUE(t_meta->directories()->empty());
  EXPECT_TRUE(t_meta->inodes()->empty());
  EXPECT_TRUE(t_meta->uids()->empty());
  EXPECT_TRUE(t_meta->gids()->empty());
  EXPECT_TRUE(t_meta->modes()->empty());
}

TEST(ThriftMetadataConverterTest, LargeValues) {
  ThriftMetadataConverter converter;

  metadata d_meta;
  d_meta.block_size = UINT32_MAX;
  d_meta.total_fs_size = UINT64_MAX;
  d_meta.timestamp_base = UINT64_MAX;

  auto t_meta_ptr = converter.from_domain(d_meta);
  auto* t_meta = static_cast<thrift::metadata::metadata*>(t_meta_ptr.get());

  EXPECT_EQ(*t_meta->block_size(), UINT32_MAX);
  EXPECT_EQ(*t_meta->total_fs_size(), UINT64_MAX);
  EXPECT_EQ(*t_meta->timestamp_base(), UINT64_MAX);
}

#else

#include <gtest/gtest.h>

TEST(ThriftMetadataConverterTest, thrift_unavailable) {
  GTEST_SKIP() << "Thrift not enabled - skipping Thrift metadata converter tests";
}

#endif // DWARFS_HAVE_THRIFT