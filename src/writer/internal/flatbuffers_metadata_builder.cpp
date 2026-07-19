// Create new FlatBuffers strategy file
/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \author     Ribose Inc. (Strategy Pattern refactoring)
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

// STRATEGY PATTERN: FlatBuffers-specific metadata builder implementation
// This file implements the FlatBuffers strategy for building metadata.
// KEY ADVANTAGE: Builds domain model DIRECTLY - no conversion needed!

#include <algorithm>
#include <cassert>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>

#include <dwarfs/file_stat.h>
#include <dwarfs/fstypes.h>
#include <dwarfs/logger.h>
#include <dwarfs/util.h>
#include <dwarfs/version.h>

#include <dwarfs/internal/features.h>
#include <dwarfs/writer/metadata_options.h>
#include <dwarfs/internal/metadata_utils.h>
#include <dwarfs/internal/string_table.h>
#include <dwarfs/writer/internal/block_manager.h>
#include <dwarfs/writer/internal/chmod_transformer.h>
#include <dwarfs/writer/internal/entry.h>
#include <dwarfs/writer/internal/global_entry_data.h>
#include <dwarfs/writer/internal/inode_hole_mapper.h>
#include <dwarfs/writer/internal/inode_manager.h>
#include <dwarfs/writer/internal/metadata_builder.h>
#include <dwarfs/writer/internal/time_resolution_converter.h>

// Domain model types (building directly)
#include <dwarfs/metadata/domain/metadata.h>

// Strategy header (template class declaration)
#include <dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h>

// Processor implementations
#include "flatbuffers_chunk_processor.h"
#include "flatbuffers_entry_processor.h"
#include "flatbuffers_packing_processor.h"
#include "flatbuffers_upgrade_processor.h"

namespace dwarfs::writer::internal {

namespace {

using namespace dwarfs::internal;
using dwarfs::feature;

time_conversion_factors
get_conversion_factors(metadata::domain::fs_options const* fs_options) {
  time_conversion_factors rv;

  if (fs_options) {
    if (fs_options->time_resolution_sec.has_value()) {
      rv.sec = fs_options->time_resolution_sec.value();
    }
    if (fs_options->subsecond_resolution_nsec_multiplier.has_value()) {
      rv.nsec = fs_options->subsecond_resolution_nsec_multiplier.value();
    }
  }

  return rv;
}

class inode_size_provider {
 public:
  struct inode_size_info {
    size_t num_chunks;
    uint64_t size;
    uint64_t allocated_size;
  };

  inode_size_provider(metadata::domain::metadata const& md)
      : chunk_table_{md.chunk_table}
      , chunks_{md.chunks}
      , block_size_{md.block_size}
      , hole_ix_{md.hole_block_index.value_or(UINT32_MAX)} {
    if (md.large_hole_size.has_value()) {
      large_hole_size_ = &md.large_hole_size.value();
    }
    assert(std::has_single_bit(block_size_));
  }

  inode_size_info get(size_t index) const {
    assert(index + 1 < chunk_table_.size());

    auto const begin = chunk_table_[index];
    auto const end = chunk_table_[index + 1];
    auto const num_chunks = end - begin;
    uint64_t size{0};
    uint64_t allocated_size{0};

    for (uint32_t ix = begin; ix < end; ++ix) {
      auto const& chunk = chunks_[ix];
      auto const b = chunk.block();
      auto const o = chunk.offset();
      auto const s = chunk.size();

      if (b == hole_ix_) {
        if (o == kChunkOffsetIsLargeHole) {
          assert(large_hole_size_);
          assert(s < large_hole_size_->size());
          size += large_hole_size_->at(s);
        } else {
          assert(o < block_size_);
          size += s * block_size_ + o;
        }
      } else {
        size += s;
        allocated_size += s;
      }
    }

    return inode_size_info{num_chunks, size, allocated_size};
  }

 private:
  std::vector<uint32_t> const& chunk_table_;
  std::vector<metadata::domain::chunk> const& chunks_;
  uint32_t block_size_;
  uint32_t hole_ix_;
  std::vector<uint64_t> const* large_hole_size_{nullptr};
};

} // namespace

// STRATEGY IMPLEMENTATION: FlatBuffers builder
// Template class is now declared in header, implementations follow

// Type aliases for clarity
template <typename LoggerPolicy>
using chunks_t = std::vector<metadata::domain::chunk>;
template <typename LoggerPolicy>
using chunk_table_t = std::vector<uint32_t>;

// Constants
namespace {
constexpr auto kTmpHoleIx = std::numeric_limits<uint32_t>::max();
}

// Destructor (required for Pimpl with unique_ptr)
template <typename LoggerPolicy>
flatbuffers_metadata_builder<LoggerPolicy>::~flatbuffers_metadata_builder() = default;

// Constructor for creating new filesystem
template <typename LoggerPolicy>
flatbuffers_metadata_builder<LoggerPolicy>::flatbuffers_metadata_builder(
    logger& lgr, metadata_options const& options)
    : LOG_PROXY_INIT(lgr)
    , options_{options}
    , timeres_{std::make_unique<time_resolution_converter>(options.time_resolution)} {
  initialize_processors();
}

// Constructor for recompressing (const metadata)
template <typename LoggerPolicy>
flatbuffers_metadata_builder<LoggerPolicy>::flatbuffers_metadata_builder(
    logger& lgr,
    metadata::domain::metadata const& md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options)
    : LOG_PROXY_INIT(lgr)
    , md_{md}
    , options_{options}
    , old_block_size_{md_.block_size}
    , timeres_{std::make_unique<time_resolution_converter>(
          options.time_resolution, get_conversion_factors(orig_fs_options))} {
  initialize_for_recompression(orig_fs_options, orig_fs_version);
  initialize_processors();
}

// Constructor for recompressing (move metadata)
template <typename LoggerPolicy>
flatbuffers_metadata_builder<LoggerPolicy>::flatbuffers_metadata_builder(
    logger& lgr,
    metadata::domain::metadata&& md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options)
    : LOG_PROXY_INIT(lgr)
    , md_{std::move(md)}
    , options_{options}
    , old_block_size_{md_.block_size}
    , timeres_{std::make_unique<time_resolution_converter>(
          options.time_resolution, get_conversion_factors(orig_fs_options))} {
  initialize_for_recompression(orig_fs_options, orig_fs_version);
  initialize_processors();
}

// Initialize processors (composition pattern)
template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::initialize_processors() {
  chunk_proc_ = std::make_unique<flatbuffers_chunk_processor>(
      LOG_GET_LOGGER, md_, features_, options_, old_block_size_);
  entry_proc_ = std::make_unique<flatbuffers_entry_processor>(
      LOG_GET_LOGGER, md_, options_, *timeres_);
  pack_proc_ = std::make_unique<flatbuffers_packing_processor>(
      LOG_GET_LOGGER, md_, options_);
  upgrade_proc_ = std::make_unique<flatbuffers_upgrade_processor>(
      LOG_GET_LOGGER, md_, options_);
}

// Common initialization for recompression
template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::initialize_for_recompression(
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version) {
  if (md_.features.has_value()) {
    features_.set(md_.features.value());
    bool const non_sparse_image = !features_.has(feature::sparsefiles);
    DWARFS_CHECK(
        non_sparse_image || options_.enable_sparse_files,
        "image uses sparse files but sparse files support is disabled");
  }

  upgrade_proc_->upgrade_metadata(orig_fs_options, orig_fs_version);
  update_inodes();
}

// Interface method implementations (simple setters)
template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::set_devices(
    std::vector<uint64_t> devices) {
  md_.devices = std::move(devices);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::set_symlink_table_size(
    size_t size) {
  md_.symlink_table.resize(size);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::set_block_size(
    uint32_t block_size) {
  md_.block_size = block_size;
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::set_shared_files_table(
    std::vector<uint32_t> shared_files) {
  md_.shared_files_table = std::move(shared_files);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::set_category_names(
    std::vector<std::string> category_names) {
  md_.category_names = std::move(category_names);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::set_block_categories(
    std::vector<uint32_t> block_categories) {
  md_.block_categories = std::move(block_categories);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::set_category_metadata_json(
    std::vector<std::string> metadata_json) {
  md_.category_metadata_json = std::move(metadata_json);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::set_block_category_metadata(
    std::map<uint32_t, uint32_t> block_metadata) {
  md_.block_category_metadata = std::move(block_metadata);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::add_symlink_table_entry(
    size_t index, uint32_t entry) {
  DWARFS_NOTHROW(md_.symlink_table.at(index)) = entry;
}

// Helper methods
template <typename LoggerPolicy>
uint32_t flatbuffers_metadata_builder<LoggerPolicy>::get_time_resolution() const {
  uint32_t resolution = 1;
  if (md_.options.has_value()) {
    if (md_.options->time_resolution_sec.has_value()) {
      resolution = md_.options->time_resolution_sec.value();
    }
  }
  return resolution;
}

template <typename LoggerPolicy>
uint32_t flatbuffers_metadata_builder<LoggerPolicy>::get_subsec_mult() const {
  uint32_t mult = 0;
  if (md_.options.has_value()) {
    if (md_.options->subsecond_resolution_nsec_multiplier.has_value()) {
      mult = md_.options->subsecond_resolution_nsec_multiplier.value();
    }
  }
  return mult;
}

// Delegate to processors (composition pattern)

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::gather_chunks(
    inode_manager const& im, block_manager const& bm, size_t chunk_count) {
  chunk_proc_->gather_chunks(im, bm, chunk_count);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::gather_entries(
    std::span<dir*> dirs, global_entry_data const& ge_data,
    uint32_t num_inodes) {
  entry_proc_->gather_entries(dirs, ge_data, num_inodes);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::gather_global_entry_data(
    global_entry_data const& ge_data) {
  entry_proc_->gather_global_entry_data(ge_data);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::remap_blocks(
    std::span<block_mapping const> mapping, size_t new_block_count) {
  chunk_proc_->remap_blocks(mapping, new_block_count);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::update_inodes() {
  bool const update_uid{options_.uid.has_value()};
  bool const update_gid{options_.gid.has_value()};
  bool const set_timestamp{options_.timestamp.has_value()};
  bool const remove_atime_ctime{
      !options_.keep_all_times &&
      !(md_.options.has_value() && md_.options->mtime_only)};
  bool const update_resolution{timeres_->requires_conversion()};

  if (!update_uid && !update_gid && !set_timestamp && !remove_atime_ctime &&
      !update_resolution && options_.chmod_specifiers.empty()) {
    // nothing to do
    return;
  }

  auto const tb_adjust =
      update_resolution
          ? timeres_->offset_conversion_remainder(md_.timestamp_base)
          : 0;

  for (auto& inode : md_.inodes) {
    if (update_uid) {
      inode.owner_index = 0;
    }

    if (update_gid) {
      inode.group_index = 0;
    }

    if (set_timestamp) {
      inode.mtime_offset = 0;
    } else if (update_resolution) {
      inode.mtime_offset =
          timeres_->convert_offset(inode.mtime_offset + tb_adjust);
      inode.mtime_subsec =
          timeres_->convert_subsec(inode.mtime_subsec);
    }

    if (set_timestamp || remove_atime_ctime) {
      inode.atime_offset = 0;
      inode.ctime_offset = 0;
    } else if (update_resolution) {
      inode.atime_offset =
          timeres_->convert_offset(inode.atime_offset + tb_adjust);
      inode.atime_subsec =
          timeres_->convert_subsec(inode.atime_subsec);
      inode.ctime_offset =
          timeres_->convert_offset(inode.ctime_offset + tb_adjust);
      inode.ctime_subsec =
          timeres_->convert_subsec(inode.ctime_subsec);
    }
  }

  apply_chmod();

  if (update_uid) {
    md_.uids = std::vector<uid_type>{*options_.uid};
  }

  if (update_gid) {
    md_.gids = std::vector<gid_type>{*options_.gid};
  }

  if (set_timestamp) {
    md_.timestamp_base = timeres_->convert_offset(*options_.timestamp);
  } else if (update_resolution) {
    md_.timestamp_base = timeres_->convert_offset(md_.timestamp_base);
  }
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::apply_chmod() {
  if (options_.chmod_specifiers.empty()) {
    return;
  }

  std::vector<uint32_t> new_modes;
  std::unordered_map<uint32_t, uint32_t> new_mode_index_map;
  auto xfm =
      chmod_transformer::build_chain(options_.chmod_specifiers, options_.umask);

  for (auto& inode : md_.inodes) {
    static constexpr uint32_t kPermissionsMask = 07777;
    auto const mode_index = inode.mode_index;
    auto const mode = md_.modes.at(mode_index);
    auto permissions = mode & kPermissionsMask;
    auto const file_type = posix_file_type::from_mode(mode);

    for (auto& c : xfm) {
      if (auto new_perm = c.transform(
              permissions, file_type == posix_file_type::directory)) {
        permissions = *new_perm;
      }
    }

    auto const new_mode = (mode & ~kPermissionsMask) | permissions;
    auto [it, inserted] =
        new_mode_index_map.emplace(new_mode, new_modes.size());
    if (inserted) {
      new_modes.push_back(new_mode);
    }
    inode.mode_index = it->second;
  }

  md_.modes = std::move(new_modes);
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::recover_directory_entries() {
  if (!md_.dir_entries.has_value() || md_.directories.empty()) {
    return;
  }

  // CRITICAL FIX: Do NOT decompress first_entry here!
  // The first_entry values should stay delta-compressed so they can be correctly
  // serialized by the FlatBuffers serializer and then decompressed by the reader.
  // This function should only recover parent_entry and self_entry.

  // Step 1: Identify which entries are directories by checking their inode modes
  std::unordered_set<uint32_t> directory_entry_indices;
  for (size_t entry_idx = 0; entry_idx < md_.dir_entries->size(); ++entry_idx) {
    auto const& de = md_.dir_entries->at(entry_idx);
    uint32_t inode_num = de.inode_num();
    if (inode_num < md_.inodes.size()) {
      uint32_t mode_index = md_.inodes[inode_num].mode_index;
      if (mode_index < md_.modes.size()) {
        uint32_t mode = md_.modes[mode_index];
        // Check if this is a directory (S_IFDIR = 0040000)
        if ((mode & 0170000) == 0040000) {
          directory_entry_indices.insert(entry_idx);
        }
      }
    }
  }

  // Step 2: Match each directory object to its entry and recover self_entry/parent_entry
  // Directory entries are in order, so we can match them sequentially
  size_t dir_entry_idx = 0;
  for (size_t dir_idx = 0; dir_idx < md_.directories.size(); ++dir_idx) {
    auto& d = md_.directories[dir_idx];

    // Find the next directory entry (they're in order)
    while (dir_entry_idx < md_.dir_entries->size() &&
           directory_entry_indices.find(dir_entry_idx) == directory_entry_indices.end()) {
      ++dir_entry_idx;
    }

    if (dir_entry_idx >= md_.dir_entries->size()) {
      // No more directory entries, must be the sentinel
      if (dir_idx == md_.directories.size() - 1) {
        d.set_self_entry(0);
        d.set_parent_entry(0);
      }
      continue;
    }

    // Set self_entry to the entry index
    d.set_self_entry(dir_entry_idx);

    // Step 3: Set parent_entry
    // parent_entry should be the self_entry of the parent directory
    // For the well-formed directory structure created by the entry processor,
    // directory indices match entry indices, so the parent of directory i is
    // directory i-1 (for i > 0).
    if (dir_idx == 0) {
      // First directory is the root
      d.set_parent_entry(0);
    } else {
      // CRITICAL FIX: The parent of directory i is directory i-1
      // The parent_entry should be the self_entry of the parent directory
      // Since directory indices match entry indices, parent's self_entry is dir_idx - 1
      d.set_parent_entry(md_.directories[dir_idx - 1].self_entry());
    }

    ++dir_entry_idx;
  }
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::update_nlink() {
  LOG_DEBUG << "DEBUG: update_nlink() called";
  LOG_DEBUG << "DEBUG: md_.options.has_value() = " << md_.options.has_value();
  LOG_DEBUG << "DEBUG: md_.dir_entries.has_value() = " << md_.dir_entries.has_value();
  LOG_DEBUG << "DEBUG: md_.inodes.size() = " << md_.inodes.size();
  
  if (md_.options.has_value() &&
      md_.options->inodes_have_nlink != options_.no_hardlink_table) {
    LOG_DEBUG << "keeping existing nlink fields";
    return;
  }

  if (md_.inodes.empty()) {
    LOG_DEBUG << "no inodes, skipping nlink update";
    return;
  }

  auto td = LOG_TIMED_DEBUG;

  if (options_.no_hardlink_table) {
    LOG_DEBUG << "hardlink table disabled, clearing nlink fields";

    // simply set nlink_minus_one to 0 for all inodes
    for (auto& inode : md_.inodes) {
      inode.nlink_minus_one = 0;
    }
  } else {
    LOG_DEBUG << "DEBUG: About to call find_inode_rank_offset";
    auto const dev_offset = find_inode_rank_offset(md_, inode_rank::INO_DEV);
    auto const reg_offset = find_inode_rank_offset(md_, inode_rank::INO_REG);
    LOG_DEBUG << "DEBUG: dev_offset = " << dev_offset << ", reg_offset = " << reg_offset;

    assert(std::ranges::all_of(md_.inodes, [](auto const& inode) {
      return inode.nlink_minus_one == 0;
    }));

    LOG_DEBUG << "DEBUG: About to check if should update nlink, dir_entries.has_value = " << md_.dir_entries.has_value();
    if (dev_offset > reg_offset && md_.dir_entries.has_value()) {
      for (auto& de : md_.dir_entries.value()) {
        auto const inode_num = de.inode_num();
        assert(inode_num < md_.inodes.size());
        // only need to update regular files
        if (reg_offset <= inode_num && inode_num < dev_offset) {
          auto& inode = md_.inodes.at(inode_num);
          inode.nlink_minus_one = inode.nlink_minus_one + 1;
        }
      }

      for (auto inode_num = reg_offset; inode_num < dev_offset; ++inode_num) {
        auto& inode = md_.inodes.at(inode_num);
        assert(inode.nlink_minus_one >= 1);
        inode.nlink_minus_one = inode.nlink_minus_one - 1;
      }
    }
  }

  td << "updating inode nlink fields...";
}

template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::update_totals_and_size_cache() {
  auto tv = LOG_TIMED_VERBOSE;

  LOG_DEBUG << "DEBUG: Starting update_totals_and_size_cache";
  LOG_DEBUG << "DEBUG: md_.dir_entries.has_value() = " << md_.dir_entries.has_value();
  LOG_DEBUG << "DEBUG: md_.options.has_value() = " << md_.options.has_value();
  LOG_DEBUG << "DEBUG: md_.reg_file_size_cache.has_value() = " << md_.reg_file_size_cache.has_value();

  uint64_t total_fs_size{0};
  uint64_t total_allocated_fs_size{0};
  uint64_t total_hardlink_size{0};

  auto const dev_offset = find_inode_rank_offset(md_, inode_rank::INO_DEV);
  auto const reg_offset = find_inode_rank_offset(md_, inode_rank::INO_REG);

  LOG_DEBUG << "DEBUG: dev_offset = " << dev_offset << ", reg_offset = " << reg_offset;

  auto const& symlink_table = md_.symlink_table;
  assert(symlink_table.size() ==
         reg_offset - find_inode_rank_offset(md_, inode_rank::INO_LNK));

  if (!symlink_table.empty()) {
    auto const& symlinks = md_.symlinks;

    for (auto const ix : symlink_table) {
      assert(ix < symlinks.size());
      auto const size = symlinks[ix].size();
      total_fs_size += size;
      total_allocated_fs_size += size;
    }
  }

  if (reg_offset < dev_offset) {
    LOG_DEBUG << "DEBUG: Processing regular files (reg_offset < dev_offset)";
    std::vector<uint32_t> nlink_table;

    // Populate uncompressed_file_sizes for all regular files
    // This provides a fallback for files that don't meet min_chunk_count threshold
    md_.uncompressed_file_sizes = std::vector<uint64_t>(dev_offset);

    if (options_.no_hardlink_table) {
      LOG_DEBUG << "DEBUG: no_hardlink_table is true, resizing nlink_table to " << (dev_offset - reg_offset);
      nlink_table.resize(dev_offset - reg_offset);

      LOG_DEBUG << "DEBUG: About to access md_.dir_entries, has_value = " << md_.dir_entries.has_value();
      if (md_.dir_entries.has_value()) {
        LOG_DEBUG << "DEBUG: Iterating over " << md_.dir_entries->size() << " dir entries";
        for (auto& de : md_.dir_entries.value()) {
          auto const inode_num = de.inode_num();
          assert(inode_num < md_.inodes.size());

          if (reg_offset <= inode_num && inode_num < dev_offset) {
            ++nlink_table[inode_num - reg_offset];
          }
        }
      }
    }

    if (!md_.reg_file_size_cache.has_value()) {
      md_.reg_file_size_cache = metadata::domain::inode_size_cache();
    }
    auto& cache = md_.reg_file_size_cache.value();
    cache.min_chunk_count = options_.inode_size_cache_min_chunk_count;

    auto const& shared = md_.shared_files_table;
    auto const shared_size = shared.has_value() ? shared->size() : 0;
    auto const num_unique_files = (dev_offset - reg_offset) - shared_size;
    inode_size_provider isp(md_);

    // WORKAROUND: Cache all inode_nums from reg_offset to dev_offset
    // The original complex loop logic has issues with shared file handling,
    // so we use a simpler approach that processes each inode_num individually.
    for (uint32_t inode_num = reg_offset; inode_num < dev_offset; ++inode_num) {
      auto const reg_inode_num = inode_num - reg_offset;

      // Calculate chunk_table_index for this inode_num
      std::optional<uint32_t> const shared_index =
          reg_inode_num >= num_unique_files && shared.has_value()
              ? std::optional<uint32_t>{shared->at(reg_inode_num -
                                                  num_unique_files)}
              : std::nullopt;
      auto const chunk_table_index = shared_index.has_value()
                                         ? num_unique_files + *shared_index
                                         : reg_inode_num;
      auto const info = isp.get(chunk_table_index);

      // CRITICAL FIX: Always populate cache for ALL regular files
      // regardless of chunk count. This ensures file sizes are available
      // even when min_chunk_count threshold isn't met.
      cache.size_lookup[inode_num] = info.size;

      if (info.allocated_size != info.size) {
        cache.allocated_size_lookup[inode_num] = info.allocated_size;
      }

      // Populate uncompressed_file_sizes as fallback
      (*md_.uncompressed_file_sizes)[inode_num] = info.size;

      // For total calculation, count this file once per inode_num
      // (shared files will be counted multiple times, once per dir_entry)
      total_fs_size += info.size;
      total_allocated_fs_size += info.allocated_size;
    }
  }

  if (md_.total_fs_size > 0 && md_.total_fs_size != total_fs_size) {
    LOG_WARN << "correcting total file system size: was " << md_.total_fs_size
             << ", now " << total_fs_size;
  }

  md_.total_fs_size = total_fs_size;

  if (options_.enable_sparse_files) {
    if (md_.total_allocated_fs_size.has_value() &&
        md_.total_allocated_fs_size.value() != total_allocated_fs_size) {
      if (total_allocated_fs_size == total_fs_size) {
        LOG_WARN
            << "clearing total allocated file system size for non-sparse image";
        md_.total_allocated_fs_size.reset();
      } else {
        LOG_WARN << "correcting total allocated file system size: was "
                 << md_.total_allocated_fs_size.value() << ", now "
                 << total_allocated_fs_size;
        md_.total_allocated_fs_size = total_allocated_fs_size;
      }
    } else if (total_allocated_fs_size != total_fs_size) {
      LOG_DEBUG << "setting total allocated file system size to "
                << total_allocated_fs_size;
      md_.total_allocated_fs_size = total_allocated_fs_size;
    }
  } else {
    assert(!md_.total_allocated_fs_size.has_value());
    if (total_allocated_fs_size != total_fs_size) {
      LOG_WARN << "non-sparse image has allocated size different from total "
                  "size: allocated="
               << total_allocated_fs_size << ", total=" << total_fs_size;
    }
  }

  if (md_.total_hardlink_size.has_value() &&
      md_.total_hardlink_size.value() != total_hardlink_size) {
    if (total_hardlink_size == 0) {
      LOG_WARN << "clearing total hardlink size";
      md_.total_hardlink_size.reset();
    } else {
      LOG_WARN << "correcting total hardlink size: was "
               << md_.total_hardlink_size.value() << ", now "
               << total_hardlink_size;
      md_.total_hardlink_size = total_hardlink_size;
    }
  } else if (total_hardlink_size != 0) {
    LOG_DEBUG << "setting total hardlink size to " << total_hardlink_size;
    md_.total_hardlink_size = total_hardlink_size;
  }

  tv << "updating total sizes and inode size cache...";
}

// *** KEY ADVANTAGE: Build returns domain model directly! ***
template <typename LoggerPolicy>
metadata::domain::metadata flatbuffers_metadata_builder<LoggerPolicy>::build() {
  LOG_VERBOSE << "building metadata (FlatBuffers strategy - direct domain)";

  // Create fs_options in domain model
  if (!md_.options.has_value()) {
    md_.options = metadata::domain::fs_options();
  }

  auto& fsopts = md_.options.value();
  fsopts.mtime_only = !options_.keep_all_times;

  {
    auto const new_conv = timeres_->new_conversion_factors();
    if (auto const sec = new_conv.sec) {
      fsopts.time_resolution_sec = *sec;
    }
    if (auto const nsec = new_conv.nsec) {
      fsopts.subsecond_resolution_nsec_multiplier = *nsec;
    }
  }

  fsopts.packed_chunk_table = options_.pack_chunk_table;
  fsopts.packed_directories = options_.pack_directories;
  fsopts.packed_shared_files_table = options_.pack_shared_files_table;
  fsopts.inodes_have_nlink = !options_.no_hardlink_table;

  update_nlink();
  update_totals_and_size_cache();
  pack_proc_->pack_metadata();

  // Recover self_entry and parent_entry after delta-compression
  if (options_.pack_directories) {
    recover_directory_entries();
  }

  md_.features = features_.get();
  md_.dwarfs_version = std::string("libdwarfs ") + DWARFS_GIT_ID;

  if (options_.no_create_timestamp) {
    md_.create_timestamp.reset();
  } else {
    md_.create_timestamp = std::time(nullptr);
  }

  md_.preferred_path_separator =
      static_cast<uint32_t>(std::filesystem::path::preferred_separator);

  // Return domain model directly - no conversion needed!
  return std::move(md_);
}

} // namespace dwarfs::writer::internal

// Explicit template instantiations for factory
template class dwarfs::writer::internal::flatbuffers_metadata_builder<dwarfs::debug_logger_policy>;
template class dwarfs::writer::internal::flatbuffers_metadata_builder<dwarfs::prod_logger_policy>;