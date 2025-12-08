/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file test_metadata_structures_compile.cpp
 *
 * Compilation test for metadata structures and accessor interface
 *
 * This file verifies that:
 * - metadata_structures.h compiles standalone
 * - metadata_accessor.h compiles standalone
 * - Basic structure instantiation works
 * - structured_metadata_accessor interface is functional
 * - MetadataAccessor concept is satisfied
 *
 * \author Ribose
 * \date 2025-11-12
 */

#include <dwarfs/internal/metadata_structures.h>
#include <dwarfs/internal/metadata_accessor.h>

#include <cassert>
#include <iostream>

namespace dwarfs::internal {

// ============================================================================
// Test Structure Instantiation
// ============================================================================

void test_structure_instantiation() {
  std::cout << "Testing structure instantiation...\n";

  // Test chunk structure
  chunk c{1, 2, 3};
  assert(c.block == 1);
  assert(c.offset == 2);
  assert(c.size == 3);

  // Test directory structure
  directory d{0, 1, 2};
  assert(d.parent_entry == 0);
  assert(d.first_entry == 1);
  assert(d.self_entry == 2);

  // Test inode_data structure
  inode_data inode{};
  inode.mode_index = 0;
  inode.owner_index = 1000;
  inode.group_index = 1000;
  inode.atime_offset = 0;
  inode.mtime_offset = 0;
  inode.ctime_offset = 0;

  // Test dir_entry structure
  dir_entry entry{0, 1};
  assert(entry.name_index == 0);
  assert(entry.inode_num == 1);

  // Test fs_options structure
  fs_options opts{};
  opts.mtime_only = false;
  opts.packed_chunk_table = true;
  opts.packed_directories = true;
  opts.packed_shared_files_table = false;
  opts.has_btime = false;
  opts.inodes_have_nlink = false;

  // Test string_table structure
  string_table table{};
  table.buffer = "test";
  table.packed_index = false;

  // Test inode_size_cache structure
  inode_size_cache cache{};
  cache.min_chunk_count = 10;

  // Test metadata root structure
  metadata meta{};
  meta.block_size = 4096;
  meta.timestamp_base = 1609459200;  // 2021-01-01 00:00:00 UTC
  meta.total_fs_size = 1024 * 1024;
  meta.chunks.push_back(c);
  meta.directories.push_back(d);
  meta.inodes.push_back(inode);

  std::cout << "  ✓ All structures instantiated successfully\n";
}

// ============================================================================
// Test Structured Metadata Accessor
// ============================================================================

void test_structured_accessor() {
  std::cout << "Testing structured_metadata_accessor...\n";

  // Create sample metadata
  metadata meta{};
  meta.block_size = 4096;
  meta.timestamp_base = 1609459200;
  meta.total_fs_size = 1024 * 1024;

  // Add some chunks
  meta.chunks.push_back({0, 0, 100});
  meta.chunks.push_back({1, 100, 200});
  meta.chunks.push_back({1, 300, 150});

  // Add directories
  meta.directories.push_back({0, 0, 0});
  meta.directories.push_back({0, 3, 1});

  // Add inodes
  inode_data inode{};
  inode.mode_index = 0;
  inode.owner_index = 0;
  inode.group_index = 0;
  meta.inodes.push_back(inode);

  // Add lookups
  meta.chunk_table = {0, 1, 3};
  meta.symlink_table = {};
  meta.uids = {0, 1000};
  meta.gids = {0, 1000};
  meta.modes = {0755, 0644};
  meta.names = {"root", "file1.txt", "file2.dat"};
  meta.symlinks = {};

  // Create accessor
  structured_metadata_accessor accessor(meta);

  // Test core scalar accessors
  assert(accessor.block_size() == 4096);
  assert(accessor.timestamp_base() == 1609459200);
  assert(accessor.total_fs_size() == 1024 * 1024);

  // Test collection accessors
  auto chunks = accessor.chunks();
  assert(chunks.size() == 3);
  assert(chunks[0].block == 0);
  assert(chunks[1].block == 1);
  assert(chunks[2].block == 1);

  auto directories = accessor.directories();
  assert(directories.size() == 2);
  assert(directories[0].parent_entry == 0);
  assert(directories[1].parent_entry == 0);

  auto inodes = accessor.inodes();
  assert(inodes.size() == 1);
  assert(inodes[0].mode_index == 0);

  auto chunk_table = accessor.chunk_table();
  assert(chunk_table.size() == 3);

  auto uids = accessor.uids();
  assert(uids.size() == 2);
  assert(uids[0] == 0);
  assert(uids[1] == 1000);

  auto gids = accessor.gids();
  assert(gids.size() == 2);

  auto modes = accessor.modes();
  assert(modes.size() == 2);

  auto names = accessor.names();
  assert(names.size() == 3);
  assert(names[0] == "root");
  assert(names[1] == "file1.txt");

  auto symlinks = accessor.symlinks();
  assert(symlinks.empty());

  // Test convenience methods
  assert(!accessor.has_symlinks());
  assert(!accessor.has_devices());
  assert(!accessor.has_options());
  assert(!accessor.has_dir_entries());
  assert(!accessor.has_features());

  // Test with optional fields present
  metadata meta2{};
  meta2.block_size = 4096;
  meta2.timestamp_base = 1609459200;
  meta2.total_fs_size = 1024 * 1024;
  meta2.devices = std::vector<uint64_t>{0x0801};
  meta2.options = fs_options{};
  meta2.options->mtime_only = true;
  meta2.features = std::unordered_set<std::string>{"sparsefiles"};

  structured_metadata_accessor accessor2(meta2);
  assert(accessor2.has_devices());
  assert(accessor2.has_options());
  assert(accessor2.has_features());

  auto opts = accessor2.options();
  assert(opts.has_value());
  assert(opts->mtime_only == true);

  std::cout << "  ✓ structured_metadata_accessor works correctly\n";
}

// ============================================================================
// Test Concept Satisfaction
// ============================================================================

void test_concept_satisfaction() {
  std::cout << "Testing MetadataAccessor concept satisfaction...\n";

  // These static_asserts are already in metadata_accessor.h, but we verify
  // them here as well for explicit testing
  static_assert(MetadataAccessor<structured_metadata_accessor>,
                "structured_metadata_accessor must satisfy MetadataAccessor");

#ifdef DWARFS_HAVE_THRIFT
  static_assert(MetadataAccessor<frozen_metadata_accessor>,
                "frozen_metadata_accessor must satisfy MetadataAccessor");
#endif

  std::cout << "  ✓ Concept requirements satisfied\n";
}

// ============================================================================
// Test Generic Code with Concept
// ============================================================================

template <MetadataAccessor Accessor>
uint64_t calculate_total_chunk_size(Accessor const& accessor) {
  uint64_t total = 0;
  for (auto const& chunk : accessor.chunks()) {
    total += chunk.size;
  }
  return total;
}

void test_generic_accessor_usage() {
  std::cout << "Testing generic accessor usage with concept...\n";

  metadata meta{};
  meta.block_size = 4096;
  meta.timestamp_base = 1609459200;
  meta.total_fs_size = 1024 * 1024;
  meta.chunks = {
    {0, 0, 100},
    {1, 100, 200},
    {1, 300, 150}
  };

  structured_metadata_accessor accessor(meta);
  uint64_t total = calculate_total_chunk_size(accessor);
  assert(total == 450);

  std::cout << "  ✓ Generic code with concept works correctly\n";
}

} // namespace dwarfs::internal

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
  std::cout << "=== DwarFS Metadata Structures Compilation Test ===\n\n";

  try {
    dwarfs::internal::test_structure_instantiation();
    dwarfs::internal::test_structured_accessor();
    dwarfs::internal::test_concept_satisfaction();
    dwarfs::internal::test_generic_accessor_usage();

    std::cout << "\n=== All tests passed! ===\n";
    return 0;
  } catch (std::exception const& e) {
    std::cerr << "\n=== Test failed with exception: " << e.what() << " ===\n";
    return 1;
  }
}