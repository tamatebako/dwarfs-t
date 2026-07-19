// Compilation test for domain model headers
// This file verifies that all domain types can be included and instantiated

#include "dwarfs/metadata/domain/chunk.h"
#include "dwarfs/metadata/domain/directory.h"
#include "dwarfs/metadata/domain/dir_entry.h"
#include "dwarfs/metadata/domain/fs_options.h"
#include "dwarfs/metadata/domain/history_entry.h"
#include "dwarfs/metadata/domain/inode_data.h"
#include "dwarfs/metadata/domain/inode_size_cache.h"
#include "dwarfs/metadata/domain/string_table.h"
#include "dwarfs/metadata/domain/metadata.h"

using namespace dwarfs::metadata::domain;

int main() {
  // Test basic types
  chunk c{0, 100, 1024};
  directory d{0, 1, 2};
  dir_entry e{0, 0};

  // Test composite types
  inode_data inode;
  inode.mode_index = 0;

  fs_options opts;
  opts.mtime_only = true;

  string_table st;
  st.buffer = "test";

  inode_size_cache cache;
  cache.min_chunk_count = 10;

  history_entry hist;
  hist.major = 2;
  hist.minor = 5;

  // Test top-level container
  metadata meta;
  meta.block_size = 4096;
  meta.chunks.push_back(c);
  meta.directories.push_back(d);
  meta.inodes.push_back(inode);

  // Test equality
  metadata meta2 = meta;
  return (meta == meta2) ? 0 : 1;
}