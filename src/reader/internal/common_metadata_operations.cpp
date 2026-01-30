/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include "common_metadata_operations.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <system_error>

// Windows compatibility for POSIX macros
#ifdef _WIN32
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 1
#endif
#endif

#include <parallel_hashmap/phmap.h>

#include <dwarfs/error.h>
#include <dwarfs/file_stat.h>
#include <dwarfs/logger.h>
#include <dwarfs/performance_monitor.h>
#include <dwarfs/reader/fsinfo_options.h>
#include <dwarfs/reader/getattr_options.h>
#include <dwarfs/vfs_stat.h>
#include <dwarfs/fstypes.h>
#include <dwarfs/reader/internal/domain_metadata_views.h>

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
#include <dwarfs/gen-cpp2/metadata_types.h>
#endif

#include "backend_adapter.h"

namespace dwarfs::reader::internal {

namespace {

// Helper to calculate metadata size (serialized form not available in domain model)
// We approximate based on the actual data structures
size_t estimate_metadata_size(metadata::domain::metadata const& meta) {
  size_t size = 0;

  // Chunks
  size += meta.chunks.size() * sizeof(metadata::domain::chunk);

  // Directories
  size += meta.directories.size() * sizeof(metadata::domain::directory);

  // Inodes
  size += meta.inodes.size() * sizeof(metadata::domain::inode_data);

  // Tables
  size += meta.chunk_table.size() * sizeof(uint32_t);
  size += meta.entry_table_v2_2.size() * sizeof(uint32_t);
  size += meta.symlink_table.size() * sizeof(uint32_t);
  size += meta.uids.size() * sizeof(uint32_t);
  size += meta.gids.size() * sizeof(uint32_t);
  size += meta.modes.size() * sizeof(uint32_t);

  // Strings
  for (auto const& name : meta.names) {
    size += name.size() + 1;
  }
  for (auto const& symlink : meta.symlinks) {
    size += symlink.size() + 1;
  }

  // Optional data
  if (meta.devices) {
    size += meta.devices->size() * sizeof(uint64_t);
  }
  if (meta.dir_entries) {
    size += meta.dir_entries->size() * sizeof(metadata::domain::dir_entry);
  }
  if (meta.shared_files_table) {
    size += meta.shared_files_table->size() * sizeof(uint32_t);
  }

  return size;
}

} // anonymous namespace

common_metadata_operations::common_metadata_operations(
    logger& lgr,
    metadata::domain::metadata domain_meta,
    metadata_options const& options,
    int inode_offset,
    bool force_consistency_check,
    [[maybe_unused]] std::shared_ptr<performance_monitor const> const& perfmon)
    : domain_meta_{std::move(domain_meta)}
    , lgr_{lgr}
    , options_{options}
    , inode_offset_{inode_offset} {

  // TODO: Initialize cached data structures from domain model
  // - Build directory lookup caches
  // - Build inode offset tables
  // - Unpack compressed tables if needed

  if (options.check_consistency || force_consistency_check) {
    check_consistency();
  }
}

// ========== Simple query methods ==========

void common_metadata_operations::check_consistency() const {
  // TODO: Implement consistency checks on domain model
  // For now, basic validation
  if (domain_meta_.directories.empty()) {
    DWARFS_THROW(runtime_error, "metadata has no directories");
  }
  if (domain_meta_.inodes.empty()) {
    DWARFS_THROW(runtime_error, "metadata has no inodes");
  }
}

size_t common_metadata_operations::size() const {
  return estimate_metadata_size(domain_meta_);
}

size_t common_metadata_operations::block_size() const {
  return domain_meta_.block_size;
}

bool common_metadata_operations::has_symlinks() const {
  return !domain_meta_.symlink_table.empty();
}

bool common_metadata_operations::has_sparse_files() const {
  return domain_meta_.hole_block_index.has_value();
}

// ========== Filesystem stats ==========

void common_metadata_operations::statvfs(vfs_stat* stbuf) const {
  *stbuf = {};

  // Make sure bsize and frsize are the same
  stbuf->bsize = 1UL;
  stbuf->frsize = 1UL;

  stbuf->blocks = options_.enable_sparse_files
                      ? domain_meta_.total_allocated_fs_size.value_or(domain_meta_.total_fs_size)
                      : domain_meta_.total_fs_size;

  stbuf->files = domain_meta_.inodes.size();
  stbuf->readonly = true;
  stbuf->namemax = PATH_MAX;
}

// ========== Category and metadata queries ==========

std::optional<std::string>
common_metadata_operations::get_block_category(size_t block_number) const {
  if (domain_meta_.category_names && domain_meta_.block_categories) {
    if (block_number < domain_meta_.block_categories->size()) {
      auto cat_idx = (*domain_meta_.block_categories)[block_number];
      if (cat_idx < domain_meta_.category_names->size()) {
        return (*domain_meta_.category_names)[cat_idx];
      }
    }
  }
  return std::nullopt;
}

std::optional<nlohmann::json>
common_metadata_operations::get_block_category_metadata(size_t block_number) const {
  // TODO: Implement block category metadata lookup from domain model
  return std::nullopt;
}

std::vector<std::string>
common_metadata_operations::get_all_block_categories() const {
  std::vector<std::string> result;
  if (domain_meta_.category_names) {
    result.reserve(domain_meta_.category_names->size());
    for (auto const& name : *domain_meta_.category_names) {
      result.push_back(name);
    }
  }
  return result;
}

std::vector<size_t>
common_metadata_operations::get_block_numbers_by_category(
    std::string_view category) const {
  std::vector<size_t> result;

  if (domain_meta_.category_names && domain_meta_.block_categories) {
    // Find category index
    std::optional<size_t> category_idx;
    for (size_t i = 0; i < domain_meta_.category_names->size(); ++i) {
      if ((*domain_meta_.category_names)[i] == category) {
        category_idx = i;
        break;
      }
    }

    if (category_idx) {
      // Find all blocks with this category
      for (size_t block = 0; block < domain_meta_.block_categories->size(); ++block) {
        if ((*domain_meta_.block_categories)[block] == *category_idx) {
          result.push_back(block);
        }
      }
    }
  }

  return result;
}

std::vector<file_stat::uid_type>
common_metadata_operations::get_all_uids() const {
  std::vector<file_stat::uid_type> result;
  result.reserve(domain_meta_.uids.size());
  for (auto uid : domain_meta_.uids) {
    result.push_back(uid);
  }
  return result;
}

std::vector<file_stat::gid_type>
common_metadata_operations::get_all_gids() const {
  std::vector<file_stat::gid_type> result;
  result.reserve(domain_meta_.gids.size());
  for (auto gid : domain_meta_.gids) {
    result.push_back(gid);
  }
  return result;
}

// ========== Placeholder implementations for remaining methods ==========
// These need to be implemented based on domain model structure

void common_metadata_operations::walk(
    std::function<void(dir_entry_view)> const& func) const {
  // Depth-first traversal with cycle detection
  phmap::flat_hash_set<int> seen;
  domain_global_metadata global{domain_meta_};

  std::function<void(uint32_t, uint32_t)> walk_recursive =
      [&](uint32_t self_index, uint32_t parent_index) {
    // Create dir_entry_view and call callback
    auto concrete = global.make_dir_entry_view(self_index, parent_index);
    // Use backend adapter for proper type construction
    dir_entry_view dev = backend_adapter::make_dir_entry_view(concrete);
    func(dev);

    auto iv = dev.inode();
    if (!iv.is_directory()) {
      return;
    }

    auto inode = iv.inode_num() - inode_offset_;

    // Cycle detection
    if (!seen.emplace(inode).second) {
      DWARFS_THROW(runtime_error, "cycle detected during directory walk");
    }

    // Traverse children
    if (inode < domain_meta_.directories.size()) {
      auto const& dir = domain_meta_.directories[inode];
      uint32_t first = dir.first_entry();

      // Calculate entry count from next directory's first_entry
      uint32_t next_first = (inode + 1 < domain_meta_.directories.size())
          ? domain_meta_.directories[inode + 1].first_entry()
          : (domain_meta_.dir_entries ? domain_meta_.dir_entries->size() : 0);
      uint32_t count = next_first - first;

      for (uint32_t i = 0; i < count; ++i) {
        uint32_t child_index = first + i;
        walk_recursive(child_index, self_index);
      }
    }

    seen.erase(inode);
  };

  // Start from root (index 0)
  walk_recursive(0, 0);
}

void common_metadata_operations::walk_data_order(
    std::function<void(dir_entry_view)> const& func) const {
  domain_global_metadata global{domain_meta_};
  std::vector<std::pair<uint32_t, uint32_t>> entries;

  // First, collect all entries via normal walk
  phmap::flat_hash_set<int> seen;
  std::function<void(uint32_t, uint32_t)> collect =
      [&](uint32_t self_index, uint32_t parent_index) {
    entries.emplace_back(self_index, parent_index);

    // Get inode to check if directory
    uint32_t inode_num;
    uint32_t inode_index;
    if (domain_meta_.dir_entries) {
      auto const& entry = (*domain_meta_.dir_entries)[self_index];
      inode_num = entry.inode_num();
      inode_index = entry.inode_num();
    } else {
      inode_index = self_index;
      inode_num = domain_meta_.inodes[self_index].inode_v2_2.value_or(self_index);
    }

    auto const& inode = domain_meta_.inodes[inode_index];
    auto mode = domain_meta_.modes[inode.mode_index];

    if (posix_file_type::from_mode(mode) != posix_file_type::directory) {
      return;
    }

    // Calculate directory inode (after offset)
    uint32_t dir_inode = inode_num;

    // Cycle detection
    if (!seen.emplace(dir_inode).second) {
      DWARFS_THROW(runtime_error, "cycle detected during directory walk");
    }

    // Traverse children
    if (dir_inode < domain_meta_.directories.size()) {
      auto const& dir = domain_meta_.directories[dir_inode];
      uint32_t first = dir.first_entry();

      // Calculate entry count from next directory's first_entry
      uint32_t next_first = (dir_inode + 1 < domain_meta_.directories.size())
          ? domain_meta_.directories[dir_inode + 1].first_entry()
          : (domain_meta_.dir_entries ? domain_meta_.dir_entries->size() : 0);
      uint32_t count = next_first - first;

      for (uint32_t i = 0; i < count; ++i) {
        uint32_t child_index = first + i;
        collect(child_index, self_index);
      }
    }

    seen.erase(dir_inode);
  };

  collect(0, 0);

  // Now partition and sort
  if (domain_meta_.dir_entries) {
    // Calculate file inode range
    uint32_t symlink_inode_offset = domain_meta_.directories.size() - 1;
    uint32_t file_inode_offset = symlink_inode_offset + domain_meta_.symlink_table.size();
    uint32_t dev_inode_offset = file_inode_offset + domain_meta_.chunk_table.size() - 1;

    // Partition: non-files first, then files
    auto mid = std::stable_partition(entries.begin(), entries.end(),
        [&](auto const& p) {
          auto const& entry = (*domain_meta_.dir_entries)[p.first];
          int ino = entry.inode_num();
          return ino < static_cast<int>(file_inode_offset) ||
                 ino >= static_cast<int>(dev_inode_offset);
        });

    // Build first chunk block map for files
    std::vector<uint32_t> first_chunk_block(entries.size(), 0);
    for (auto it = mid; it != entries.end(); ++it) {
      auto const& entry = (*domain_meta_.dir_entries)[it->first];
      int ino = entry.inode_num();

      if (ino >= static_cast<int>(file_inode_offset)) {
        int file_index = ino - file_inode_offset;
        if (file_index >= 0 &&
            std::cmp_less(file_index + 1, domain_meta_.chunk_table.size())) {
          uint32_t chunk_begin = domain_meta_.chunk_table[file_index];
          uint32_t chunk_end = domain_meta_.chunk_table[file_index + 1];
          if (chunk_begin < chunk_end && chunk_begin < domain_meta_.chunks.size()) {
            first_chunk_block[std::distance(entries.begin(), it)] =
                domain_meta_.chunks[chunk_begin].block();
          }
        }
      }
    }

    // Sort files by first chunk block
    std::stable_sort(mid, entries.end(),
        [&first_chunk_block, &entries](std::pair<uint32_t, uint32_t> const& a,
                                        std::pair<uint32_t, uint32_t> const& b) {
          auto a_idx = static_cast<size_t>(&a - entries.data());
          auto b_idx = static_cast<size_t>(&b - entries.data());
          return first_chunk_block[a_idx] < first_chunk_block[b_idx];
        });
  }

  // Call callback for each entry in order
  for (auto [self_index, parent_index] : entries) {
    auto concrete = global.make_dir_entry_view(self_index, parent_index);
    // Use backend adapter for proper type construction
    func(backend_adapter::make_dir_entry_view(concrete));
  }
}

dir_entry_view common_metadata_operations::root() const {
  // Create domain-based dir entry view for root (index 0)
  domain_global_metadata global{domain_meta_};
  auto concrete = global.make_dir_entry_view(0, 0);

  // Use backend adapter for proper type construction
  return backend_adapter::make_dir_entry_view(concrete);
}

std::optional<dir_entry_view>
common_metadata_operations::find(std::string_view path) const {
  // Remove leading slashes
  auto start = path.find_first_not_of('/');
  if (start != std::string_view::npos) {
    path.remove_prefix(start);
  } else {
    path = {};
  }

  auto dev = std::make_optional(root());

  while (!path.empty()) {
    auto iv = dev->inode();

    if (!iv.is_directory()) {
      dev.reset();
      break;
    }

    // Extract next component
    auto name = path;
    if (auto sep = path.find('/'); sep != std::string_view::npos) {
      name = path.substr(0, sep);
      path.remove_prefix(sep + 1);
    } else {
      path = {};
    }

    // Find entry in current directory
    auto dir_view = opendir(iv);
    if (!dir_view) {
      dev.reset();
      break;
    }

    // Search directory for name
    dev = find(iv.inode_num(), name);

    if (!dev) {
      break;
    }
  }

  return dev;
}

std::optional<inode_view>
common_metadata_operations::find(int inode) const {
  inode -= inode_offset_;

  if (inode < 0 || std::cmp_greater_equal(inode, domain_meta_.inodes.size())) {
    return std::nullopt;
  }

  // Create domain-based inode view
  uint32_t inode_index;
  if (domain_meta_.dir_entries) {
    // v2.3+: inode number IS the index
    inode_index = inode;
  } else {
    // v2.2: need to use entry table
    if (std::cmp_greater_equal(inode, domain_meta_.entry_table_v2_2.size())) {
      return std::nullopt;
    }
    inode_index = domain_meta_.entry_table_v2_2[inode];
  }

  auto concrete = std::make_shared<domain_inode_view_impl>(
      domain_meta_, inode_index, inode);

  // Use backend adapter for proper type construction
  return backend_adapter::make_inode_view(concrete);
}

std::optional<dir_entry_view>
common_metadata_operations::find(int inode, std::string_view name) const {
  // Get the inode first
  auto iv = find(inode);
  if (!iv || !iv->is_directory()) {
    return std::nullopt;
  }

  // Open the directory
  auto dir_opt = opendir(*iv);
  if (!dir_opt) {
    return std::nullopt;
  }

  auto dir = *dir_opt;

  // Get directory from domain model
  uint32_t dir_inode = inode - inode_offset_;
  if (dir_inode >= domain_meta_.directories.size()) {
    return std::nullopt;
  }

  auto const& directory = domain_meta_.directories[dir_inode];
  uint32_t first = directory.first_entry();

  // Calculate entry count from next directory's first_entry
  uint32_t next_first = (dir_inode + 1 < domain_meta_.directories.size())
      ? domain_meta_.directories[dir_inode + 1].first_entry()
      : (domain_meta_.dir_entries ? domain_meta_.dir_entries->size() : 0);
  uint32_t count = next_first - first;

  // Search entries in this directory
  // TODO: Implement binary search when not case-insensitive
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t entry_idx = first + i;

    domain_global_metadata global{domain_meta_};

    // Get the entry name
    std::string entry_name;
    if (domain_meta_.dir_entries) {
      auto const& entry = (*domain_meta_.dir_entries)[entry_idx];
      entry_name = domain_meta_.names[entry.name_index()];
    } else {
      // Legacy v2.2 - skip for now
      continue;
    }

    // Compare names
    if (entry_name == name) {
      // Found it!
      auto concrete = global.make_dir_entry_view(
          entry_idx,
          global.self_dir_entry(dir_inode));

      // Use backend adapter for proper type construction
      return backend_adapter::make_dir_entry_view(concrete);
    }
  }

  return std::nullopt;
}

// ========== Timestamp helpers ==========

uint32_t common_metadata_operations::get_time_resolution() const {
  if (domain_meta_.options && domain_meta_.options->time_resolution_sec) {
    return *domain_meta_.options->time_resolution_sec;
  }
  return 1; // Default: 1 second resolution
}

uint32_t common_metadata_operations::get_nsec_multiplier() const {
  if (domain_meta_.options && domain_meta_.options->subsecond_resolution_nsec_multiplier) {
    return *domain_meta_.options->subsecond_resolution_nsec_multiplier;
  }
  return 0; // Default: no subsecond precision
}

uint64_t common_metadata_operations::get_timestamp_base() const {
  return domain_meta_.timestamp_base;
}

bool common_metadata_operations::is_mtime_only() const {
  if (domain_meta_.options) {
    return domain_meta_.options->mtime_only;
  }
  return false; // Default: all timestamps available
}

void common_metadata_operations::fill_timestamps(
    file_stat& st, metadata::domain::inode_data const& inode) const {

  uint32_t resolution = get_time_resolution();
  uint32_t nsec_mult = get_nsec_multiplier();
  uint64_t timebase = get_timestamp_base();

  // mtime (always present)
  uint64_t mtime_sec = resolution * (timebase + inode.mtime_offset);
  uint32_t mtime_nsec = nsec_mult > 0 ? static_cast<uint32_t>(inode.mtime_subsec * nsec_mult) : 0;
  st.set_mtimespec(mtime_sec, mtime_nsec);

  if (is_mtime_only()) {
    // If mtime_only mode, copy mtime to atime and ctime
    st.set_atimespec(st.mtimespec_unchecked());
    st.set_ctimespec(st.mtimespec_unchecked());
  } else {
    // atime
    uint64_t atime_sec = resolution * (timebase + inode.atime_offset);
    uint32_t atime_nsec = nsec_mult > 0 ? static_cast<uint32_t>(inode.atime_subsec * nsec_mult) : 0;
    st.set_atimespec(atime_sec, atime_nsec);

    // ctime
    uint64_t ctime_sec = resolution * (timebase + inode.ctime_offset);
    uint32_t ctime_nsec = nsec_mult > 0 ? static_cast<uint32_t>(inode.ctime_subsec * nsec_mult) : 0;
    st.set_ctimespec(ctime_sec, ctime_nsec);
  }
}

// ========== File attribute operations ==========

file_stat common_metadata_operations::getattr(inode_view iv,
                                               std::error_code& ec) const {
  return getattr(iv, getattr_options{}, ec);
}

file_stat common_metadata_operations::getattr(inode_view iv,
                                               getattr_options const& opts,
                                               std::error_code& ec) const {
  ec.clear();
  file_stat stbuf;

  stbuf.set_dev(0); // TODO: make configurable?

  auto mode = iv.mode();
  auto inode = iv.inode_num();

  if (options_.readonly) {
    // Mask out write bits
    uint16_t const READ_ONLY_MASK = ~uint16_t(
        std::filesystem::perms::owner_write |
        std::filesystem::perms::group_write |
        std::filesystem::perms::others_write);
    mode &= READ_ONLY_MASK;
  }

  stbuf.set_mode(mode);

  if (!opts.no_size) {
    file_size_t size = 0;
    file_size_t allocated_size = 0;

    if (stbuf.is_directory()) {
      // Directory size is entry count
      uint32_t dir_inode = inode - inode_offset_;
      if (dir_inode < domain_meta_.directories.size()) {
        // Calculate entry count from next directory's first_entry
        uint32_t first = domain_meta_.directories[dir_inode].first_entry();
        uint32_t next_first = (dir_inode + 1 < domain_meta_.directories.size())
            ? domain_meta_.directories[dir_inode + 1].first_entry()
            : (domain_meta_.dir_entries ? domain_meta_.dir_entries->size() : 0);
        size = next_first - first;
      }
      allocated_size = size;
    } else if (stbuf.is_regular_file()) {
      // File size - get chunks and sum
      std::error_code chunk_ec;
      auto cr = get_chunks(inode, chunk_ec);
      if (!chunk_ec) {
        for (size_t i = 0; i < cr.size(); ++i) {
          auto chunk = cr.at(i);
          auto chunk_size = chunk->size();
          size += chunk_size;
          if (!options_.enable_sparse_files || chunk->is_data()) {
            allocated_size += chunk_size;
          }
        }
      }
    } else if (stbuf.is_symlink()) {
      // Symlink size is the link target length
      uint32_t link_inode = inode - inode_offset_;
      // Find symlink offset
      // TODO: Need to calculate symlink inode offset properly
      if (!domain_meta_.symlink_table.empty()) {
        // This needs proper offset calculation like symlink_inode_offset_
        // For now, approximate
        size = 0; // Will be filled by readlink
      }
    }

    stbuf.set_size(size);
    stbuf.set_blocks((allocated_size + 511) / 512);
    stbuf.set_allocated_size(allocated_size);
  }

  stbuf.set_ino(inode + inode_offset_);
  stbuf.set_blksize(options_.block_size);
  stbuf.set_uid(options_.fs_uid.value_or(iv.getuid()));
  stbuf.set_gid(options_.fs_gid.value_or(iv.getgid()));

  // Fill timestamps from domain model
  // Calculate inode_index (same logic as in find(int inode))
  uint32_t inode_for_lookup = inode - inode_offset_;
  uint32_t inode_index;

  if (domain_meta_.dir_entries) {
    // v2.3+: inode number IS the index
    inode_index = inode_for_lookup;
  } else {
    // v2.2: need to use entry table
    if (inode_for_lookup < domain_meta_.entry_table_v2_2.size()) {
      inode_index = domain_meta_.entry_table_v2_2[inode_for_lookup];
    } else {
      inode_index = inode_for_lookup; // Fallback
    }
  }

  if (inode_index < domain_meta_.inodes.size()) {
    fill_timestamps(stbuf, domain_meta_.inodes[inode_index]);
  }

  // nlink
  stbuf.set_nlink(1); // TODO: implement hardlink counting from domain model

  // rdev for devices
  if (stbuf.is_device()) {
    // TODO: get device ID from domain model devices table
    stbuf.set_rdev(0);
  } else {
    stbuf.set_rdev(0);
  }

  return stbuf;
}

std::optional<directory_view>
common_metadata_operations::opendir(inode_view iv) const {
  if (!iv.is_directory()) {
    return std::nullopt;
  }

  // Create directory_view from the inode
  domain_global_metadata global{domain_meta_};
  uint32_t dir_inode = iv.inode_num() - inode_offset_;

  if (dir_inode >= domain_meta_.directories.size()) {
    return std::nullopt;
  }

  // directory_view wraps the global metadata and inode number
  // In FlatBuffers-only builds, internal::global_metadata is aliased to domain_global_metadata
  return backend_adapter::make_directory_view(iv.inode_num(), global);
}

std::optional<dir_entry_view>
common_metadata_operations::readdir(directory_view dir, size_t offset) const {
  domain_global_metadata global{domain_meta_};
  uint32_t dir_inode = dir.inode() - inode_offset_;

  if (dir_inode >= domain_meta_.directories.size()) {
    return std::nullopt;
  }

  switch (offset) {
  case 0: {
    // "." - self entry
    auto self_idx = global.self_dir_entry(dir_inode);
    auto concrete = global.make_dir_entry_view(self_idx, self_idx);
    // Use backend adapter for proper type construction
    return backend_adapter::make_dir_entry_view(concrete);
  }

  case 1: {
    // ".." - parent entry
    auto parent_inode = domain_meta_.directories[dir_inode].parent_entry();
    auto parent_idx = global.self_dir_entry(parent_inode);
    auto concrete = global.make_dir_entry_view(parent_idx, global.self_dir_entry(dir_inode));
    // Use backend adapter for proper type construction
    return backend_adapter::make_dir_entry_view(concrete);
  }

  default:
    offset -= 2; // Account for . and ..

    auto const& directory = domain_meta_.directories[dir_inode];
    uint32_t first = directory.first_entry();

    // Calculate entry count from next directory's first_entry
    uint32_t next_first = (dir_inode + 1 < domain_meta_.directories.size())
        ? domain_meta_.directories[dir_inode + 1].first_entry()
        : (domain_meta_.dir_entries ? domain_meta_.dir_entries->size() : 0);
    uint32_t count = next_first - first;

    if (offset >= count) {
      return std::nullopt;
    }

    uint32_t entry_idx = first + offset;
    auto concrete = global.make_dir_entry_view(entry_idx, global.self_dir_entry(dir_inode));

    // Use backend adapter for proper type construction
    return backend_adapter::make_dir_entry_view(concrete);
  }
}

size_t common_metadata_operations::dirsize(directory_view dir) const {
  uint32_t dir_inode = dir.inode() - inode_offset_;

  if (dir_inode >= domain_meta_.directories.size()) {
    return 2; // . and ..
  }

  // Calculate entry count from next directory's first_entry
  uint32_t first = domain_meta_.directories[dir_inode].first_entry();
  uint32_t next_first = (dir_inode + 1 < domain_meta_.directories.size())
      ? domain_meta_.directories[dir_inode + 1].first_entry()
      : (domain_meta_.dir_entries ? domain_meta_.dir_entries->size() : 0);
  uint32_t count = next_first - first;

  return 2 + count;
}

void common_metadata_operations::access(inode_view iv, int mode,
                                         file_stat::uid_type uid,
                                         file_stat::gid_type gid,
                                         std::error_code& ec) const {
  if (mode == F_OK) {
    // Easy - only checking existence
    ec.clear();
    return;
  }

  int access_mode = 0;

  auto set_xok = [&access_mode]() {
#ifdef _WIN32
    access_mode |= 1; // Windows has no X_OK
#else
    access_mode |= X_OK;
#endif
  };

  auto e_mode = iv.mode();

  if (uid == 0) {
    // Root has R/W access always
    access_mode = R_OK | W_OK;

    // Root has X access if anyone has X access
    if (e_mode & uint16_t(std::filesystem::perms::owner_exec |
                          std::filesystem::perms::group_exec |
                          std::filesystem::perms::others_exec)) {
      set_xok();
    }
  } else {
    auto test = [e_mode, &access_mode, &set_xok, readonly = options_.readonly](
        std::filesystem::perms r_bit,
        std::filesystem::perms w_bit,
        std::filesystem::perms x_bit) {
      if (e_mode & uint16_t(r_bit)) {
        access_mode |= R_OK;
      }
      if (e_mode & uint16_t(w_bit)) {
        if (!readonly) {
          access_mode |= W_OK;
        }
      }
      if (e_mode & uint16_t(x_bit)) {
        set_xok();
      }
    };

    // Build access mask (check in order: others, group, owner)
    test(std::filesystem::perms::others_read,
         std::filesystem::perms::others_write,
         std::filesystem::perms::others_exec);

    if (iv.getgid() == gid) {
      test(std::filesystem::perms::group_read,
           std::filesystem::perms::group_write,
           std::filesystem::perms::group_exec);
    }

    if (iv.getuid() == uid) {
      test(std::filesystem::perms::owner_read,
           std::filesystem::perms::owner_write,
           std::filesystem::perms::owner_exec);
    }
  }

  if ((access_mode & mode) == mode) {
    ec.clear();
  } else {
    ec = std::make_error_code(std::errc::permission_denied);
  }
}

int common_metadata_operations::open(inode_view iv,
                                      std::error_code& ec) const {
  if (iv.is_regular_file()) {
    ec.clear();
    return iv.inode_num();
  }

  ec = std::make_error_code(std::errc::invalid_argument);
  return 0;
}

file_off_t common_metadata_operations::seek(uint32_t inode, file_off_t offset,
                                             seek_whence whence,
                                             std::error_code& ec) const {
  if (!options_.enable_sparse_files) {
    ec = std::make_error_code(std::errc::not_supported);
    return -1;
  }

  // Get chunks for this inode
  auto cr = get_chunks(inode, ec);
  if (ec) {
    return -1;
  }

  // Build a simple sparse file seeker
  file_off_t current_offset = 0;

  switch (whence) {
  case seek_whence::data: {
    // SEEK_DATA: Find next data segment >= offset
    for (size_t i = 0; i < cr.size(); ++i) {
      auto chunk = cr.at(i);
      file_off_t chunk_end = current_offset + chunk->size();

      if (chunk->is_data() && chunk_end > offset) {
        // Found data chunk that extends past offset
        if (current_offset >= offset) {
          ec.clear();
          return current_offset;
        } else {
          ec.clear();
          return offset;
        }
      }

      current_offset = chunk_end;
    }

    // No data found after offset
    ec = std::make_error_code(std::errc::no_such_file_or_directory);
    return -1;
  }

  case seek_whence::hole: {
    // SEEK_HOLE: Find next hole >= offset
    for (size_t i = 0; i < cr.size(); ++i) {
      auto chunk = cr.at(i);
      file_off_t chunk_end = current_offset + chunk->size();

      if (chunk->is_hole() && chunk_end > offset) {
        // Found hole chunk that extends past offset
        if (current_offset >= offset) {
          ec.clear();
          return current_offset;
        } else {
          ec.clear();
          return offset;
        }
      }

      current_offset = chunk_end;
    }

    // No hole found, return EOF
    ec.clear();
    return current_offset;
  }

  default:
    ec = std::make_error_code(std::errc::invalid_argument);
    return -1;
  }
}

std::string common_metadata_operations::readlink(inode_view iv,
                                                  readlink_mode mode,
                                                  std::error_code& ec) const {
  if (!iv.is_symlink()) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return {};
  }

  ec.clear();

  // Calculate symlink offset
  // TODO: Need proper symlink_inode_offset_ calculation
  // For now, assume symlinks come after directories
  uint32_t inode = iv.inode_num() - inode_offset_;
  uint32_t symlink_inode_offset = domain_meta_.directories.size() - 1;

  if (inode < symlink_inode_offset) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return {};
  }

  uint32_t symlink_index = inode - symlink_inode_offset;

  if (symlink_index >= domain_meta_.symlink_table.size()) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return {};
  }

  uint32_t symlink_str_index = domain_meta_.symlink_table[symlink_index];

  if (symlink_str_index >= domain_meta_.symlinks.size()) {
    ec = std::make_error_code(std::errc::invalid_argument);
    return {};
  }

  std::string rv = domain_meta_.symlinks[symlink_str_index];

  // Handle path separator conversion
  if (mode != readlink_mode::raw) {
    char meta_preferred = '/';
    if (domain_meta_.preferred_path_separator) {
      meta_preferred = static_cast<char>(*domain_meta_.preferred_path_separator);
    }

    char host_preferred = static_cast<char>(std::filesystem::path::preferred_separator);
    if (mode == readlink_mode::posix) {
      host_preferred = '/';
    }

    if (meta_preferred != host_preferred) {
      std::ranges::replace(rv, meta_preferred, host_preferred);
    }
  }

  return rv;
}

chunk_range common_metadata_operations::get_chunks(int inode,
                                                    std::error_code& ec) const {
  inode -= inode_offset_;

  // Calculate file_inode_offset (files come after symlinks)
  uint32_t symlink_inode_offset = domain_meta_.directories.size() - 1;
  uint32_t file_inode_offset = symlink_inode_offset + domain_meta_.symlink_table.size();

  if (inode < static_cast<int>(file_inode_offset)) {
    ec = std::make_error_code(std::errc::invalid_argument);
    // Return empty chunk_range using adapter
    return backend_adapter::make_chunk_range(domain_meta_, 0, 0);
  }

  int file_index = inode - file_inode_offset;

  // Handle shared files if present
  // TODO: Need to handle shared_files_table properly

  // Get chunk range from chunk_table
  if (file_index < 0 || std::cmp_greater_equal(file_index + 1, domain_meta_.chunk_table.size())) {
    ec = std::make_error_code(std::errc::invalid_argument);
    // Return empty chunk_range using adapter
    return backend_adapter::make_chunk_range(domain_meta_, 0, 0);
  }

  uint32_t begin = domain_meta_.chunk_table[file_index];
  uint32_t end = domain_meta_.chunk_table[file_index + 1];

  if (begin > end || end > domain_meta_.chunks.size()) {
    ec = std::make_error_code(std::errc::invalid_argument);
    // Return empty chunk_range using adapter
    return backend_adapter::make_chunk_range(domain_meta_, 0, 0);
  }

  ec.clear();

  // Use backend adapter to create chunk_range
  return backend_adapter::make_chunk_range(domain_meta_, begin, end);
}

nlohmann::json common_metadata_operations::get_inode_info(inode_view iv,
                                                           size_t max_chunks) const {
  // TODO: Implement get_inode_info using domain model
  return nlohmann::json::object();
}

void common_metadata_operations::dump(
    std::ostream& os, fsinfo_options const& opts, filesystem_info const* fsinfo,
    std::function<void(std::string const&, uint32_t)> const& icb) const {
  // Recursive dump starting from root
  std::function<void(std::string const&, dir_entry_view const&)> dump_entry =
      [&](std::string const& indent, dir_entry_view const& entry) {
    auto iv = entry.inode();
    auto inode = iv.inode_num();

    os << indent << "<inode:" << inode << "> "
       << file_stat::mode_string(iv.mode());

    if (inode > 0) {
      os << " " << entry.name();
    }

    auto type = posix_file_type::from_mode(iv.mode());

    if (type == posix_file_type::regular) {
      std::error_code ec;
      auto cr = get_chunks(inode, ec);
      if (!ec) {
        file_size_t size = 0;
        file_size_t allocated = 0;
        for (size_t i = 0; i < cr.size(); ++i) {
          auto chunk = cr.at(i);
          size += chunk->size();
          if (!options_.enable_sparse_files || chunk->is_data()) {
            allocated += chunk->size();
          }
        }
        os << " [" << cr.size() << " chunks] " << size;
        if (allocated != size) {
          os << " (allocated: " << allocated << ")";
        }
        os << "\n";

        if (opts.features.has(fsinfo_feature::chunk_details)) {
          icb(indent + "  ", inode);
        }
      } else {
        os << " [error getting chunks]\n";
      }
    } else if (type == posix_file_type::directory) {
      auto dir_opt = opendir(iv);
      if (dir_opt) {
        auto dir = *dir_opt;
        os << " (" << dirsize(dir) - 2 << " entries)\n";

        // Dump children
        for (size_t i = 2; i < dirsize(dir); ++i) {
          if (auto child = readdir(dir, i)) {
            dump_entry(indent + "  ", *child);
          }
        }
      } else {
        os << " [error opening directory]\n";
      }
    } else if (type == posix_file_type::symlink) {
      std::error_code ec;
      auto target = readlink(iv, readlink_mode::raw, ec);
      if (!ec) {
        os << " -> " << target << "\n";
      } else {
        os << " [error reading link]\n";
      }
    } else if (type == posix_file_type::block || type == posix_file_type::character) {
      os << " (device)\n";
    } else if (type == posix_file_type::fifo) {
      os << " (named pipe)\n";
    } else if (type == posix_file_type::socket) {
      os << " (socket)\n";
    } else {
      os << "\n";
    }
  };

  dump_entry("", root());
}

nlohmann::json common_metadata_operations::info_as_json(
    fsinfo_options const& opts, filesystem_info const* fsinfo) const {
  nlohmann::json info;

  // Basic metadata info
  if (opts.features.has(fsinfo_feature::metadata_summary)) {
    info["block_size"] = domain_meta_.block_size;
    if (fsinfo) {
      info["block_count"] = fsinfo->block_count;
    }
    info["inode_count"] = domain_meta_.inodes.size();

    if (domain_meta_.preferred_path_separator) {
      info["preferred_path_separator"] =
          std::string(1, static_cast<char>(*domain_meta_.preferred_path_separator));
    }

    info["original_filesystem_size"] = domain_meta_.total_fs_size;
    if (domain_meta_.total_allocated_fs_size) {
      info["original_allocated_filesystem_size"] = *domain_meta_.total_allocated_fs_size;
    }

    if (fsinfo) {
      info["compressed_block_size"] = fsinfo->compressed_block_size;
      if (!fsinfo->uncompressed_block_size_is_estimate) {
        info["uncompressed_block_size"] = fsinfo->uncompressed_block_size;
      }
      info["compressed_metadata_size"] = fsinfo->compressed_metadata_size;
      if (!fsinfo->uncompressed_metadata_size_is_estimate) {
        info["uncompressed_metadata_size"] = fsinfo->uncompressed_metadata_size;
      }
    }

    // Version info
    if (domain_meta_.dwarfs_version) {
      info["created_by"] = *domain_meta_.dwarfs_version;
    }
    if (domain_meta_.create_timestamp) {
      info["created_on"] = *domain_meta_.create_timestamp;
    }

    // Features
    if (domain_meta_.features) {
      info["features"] = *domain_meta_.features;
    }

    // Categories
    if (domain_meta_.category_names && domain_meta_.block_categories) {
      nlohmann::json& categories = info["categories"];
      for (size_t cat_idx = 0; cat_idx < domain_meta_.category_names->size(); ++cat_idx) {
        std::string const& name = (*domain_meta_.category_names)[cat_idx];
        size_t count = 0;
        for (auto block_cat : *domain_meta_.block_categories) {
          if (block_cat == cat_idx) {
            ++count;
          }
        }
        categories[name] = {{"block_count", count}};
      }
    }
  }

  // Detailed metadata
  if (opts.features.has(fsinfo_feature::metadata_details)) {
    nlohmann::json meta;
    meta["chunks"] = domain_meta_.chunks.size();
    meta["directories"] = domain_meta_.directories.size();
    meta["inodes"] = domain_meta_.inodes.size();
    meta["chunk_table"] = domain_meta_.chunk_table.size();
    meta["symlink_table"] = domain_meta_.symlink_table.size();
    meta["uids"] = domain_meta_.uids.size();
    meta["gids"] = domain_meta_.gids.size();
    meta["modes"] = domain_meta_.modes.size();
    meta["names"] = domain_meta_.names.size();
    meta["symlinks"] = domain_meta_.symlinks.size();

    if (domain_meta_.devices) {
      meta["devices"] = domain_meta_.devices->size();
    }
    if (domain_meta_.dir_entries) {
      meta["dir_entries"] = domain_meta_.dir_entries->size();
    }
    if (domain_meta_.shared_files_table) {
      meta["shared_files_table"] = domain_meta_.shared_files_table->size();
    }

    info["meta"] = std::move(meta);
  }

  // Directory tree
  if (opts.features.has(fsinfo_feature::directory_tree)) {
    info["root"] = as_json();
  }

  return info;
}

nlohmann::json common_metadata_operations::as_json() const {
  // Recursively build JSON tree
  std::function<nlohmann::json(dir_entry_view const&)> entry_to_json =
      [&](dir_entry_view const& entry) -> nlohmann::json {
    nlohmann::json obj;
    obj["name"] = entry.name();
    obj["inode"] = entry.inode().inode_num();
    obj["mode"] = entry.inode().mode();

    if (entry.inode().is_directory()) {
      auto dir_opt = opendir(entry.inode());
      if (dir_opt) {
        nlohmann::json children = nlohmann::json::array();
        auto dir = *dir_opt;
        for (size_t i = 2; i < dirsize(dir); ++i) {
          if (auto child = readdir(dir, i)) {
            children.push_back(entry_to_json(*child));
          }
        }
        obj["children"] = std::move(children);
      }
    }

    return obj;
  };

  vfs_stat stbuf;
  statvfs(&stbuf);

  nlohmann::json result{
      {"statvfs",
       {{"f_bsize", stbuf.bsize},
        {"f_files", stbuf.files},
        {"f_blocks", stbuf.blocks}}},
      {"root", entry_to_json(root())},
  };

  return result;
}

std::string common_metadata_operations::serialize_as_json(bool simple) const {
  // Convert domain model back to Thrift format for serialization
  // This requires the domain→Thrift converter from Session 28
  // For now, throw as not yet fully implemented
  DWARFS_THROW(runtime_error,
      "serialize_as_json() requires domain→Thrift converter (Session 28)");
}

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
std::unique_ptr<thrift::metadata::metadata>
common_metadata_operations::thaw() const {
  // Convert domain model to Thrift format using Session 28 converter
  // TODO: Use metadata::converters::to_thrift(domain_meta_)
  // For now, return nullptr as converters may not be linked yet
  return nullptr;
}

std::unique_ptr<thrift::metadata::metadata>
common_metadata_operations::unpack() const {
  // Same as thaw but with unpacked/expanded tables
  // TODO: Use metadata::converters::to_thrift with unpack flag
  return nullptr;
}

std::unique_ptr<thrift::metadata::fs_options>
common_metadata_operations::thaw_fs_options() const {
  if (domain_meta_.options) {
    // Convert domain fs_options to Thrift format
    auto opts = std::make_unique<thrift::metadata::fs_options>();

    auto const& domain_opts = *domain_meta_.options;
    opts->mtime_only() = domain_opts.mtime_only;
    opts->packed_chunk_table() = domain_opts.packed_chunk_table;
    opts->packed_directories() = domain_opts.packed_directories;
    opts->packed_shared_files_table() = domain_opts.packed_shared_files_table;

    if (domain_opts.time_resolution_sec) {
      opts->time_resolution_sec() = *domain_opts.time_resolution_sec;
    }
    // Note: Domain model only has time_resolution_sec
    // The separate atime/mtime/ctime_resolution_ns fields don't exist in domain model

    return opts;
  }
  return nullptr;
}
#endif

} // namespace dwarfs::reader::internal