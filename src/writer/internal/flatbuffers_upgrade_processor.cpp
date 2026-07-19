/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc. (OOP refactoring)
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

#include "flatbuffers_upgrade_processor.h"

#include <algorithm>
#include <numeric>
#include <unordered_map>

#include <dwarfs/fstypes.h>
#include <dwarfs/logger.h>
#include <dwarfs/util.h>
#include <dwarfs/version.h>
#include <dwarfs/writer/metadata_options.h>

#include <dwarfs/internal/metadata_utils.h>
#include <dwarfs/metadata/domain/metadata.h>

namespace dwarfs::writer::internal {

flatbuffers_upgrade_processor::flatbuffers_upgrade_processor(
    logger& lgr, metadata::domain::metadata& md,
    metadata_options const& options)
    : lgr_{lgr}, md_{md}, options_{options} {}

void flatbuffers_upgrade_processor::upgrade_metadata(
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version) {
  // upgrading metadata...

  metadata::domain::history_entry histent;
  histent.major = orig_fs_version.major;
  histent.minor = orig_fs_version.minor;
  histent.dwarfs_version = md_.dwarfs_version;
  histent.block_size = md_.block_size;
  if (orig_fs_options) {
    histent.options = *orig_fs_options;
  }

  if (!md_.entry_table_v2_2.empty()) {
    DWARFS_CHECK(!md_.dir_entries.has_value(),
                 "unexpected dir_entries in metadata");

    upgrade_from_pre_v2_2();
  }

  if (options_.no_metadata_version_history) {
    md_.metadata_version_history.reset();
  } else {
    if (!md_.metadata_version_history.has_value()) {
      md_.metadata_version_history =
          std::vector<metadata::domain::history_entry>();
    }
    md_.metadata_version_history->push_back(std::move(histent));
  }
}

void flatbuffers_upgrade_processor::upgrade_from_pre_v2_2() {
  // === v2.2 metadata ===
  //
  // - `directories` is indexed by directory inode number; this is exactly
  //   the same as today.
  // - `entry_table_v2_2` is indexed by "inode number" and returns an index
  //   into `inodes`.
  // - `inodes` sort of combine the inode data with data from the new
  //   `dir_entries` array. Inodes are ordered by directory entry index
  //   (i.e. `first_entry`, `parent_entry` can be used to directly index
  //   into `inodes`).
  // - The format cannot properly represent hardlinks. Rather, it represents
  //   all shared files as hardlinks, which is not correct.
  //
  // In order to upgrade to the new format, we need to:
  //
  // - Keep the `directories` array as is.
  // - Rebuild the `inodes` array to be indexed by inode number; the new
  //   format potentially has *much* more inode numbers than the old format
  //   because shared files don't share inode numbers anymore, only hardlinks
  //   do. The order of the new `inodes` array is exactly the same as the
  //   old `entry_table_v2_2` array, *except* for regular files, where order
  //   needs to take shared files into account. This means regular file
  //   inode numbers will change and this needs to be tracked. This also
  //   means that both the `chunk_table` and `chunks` arrays need to be
  //   rebuilt.
  // - Build the `shared_files_table` array. If multiple entries in `inodes`
  //   share the same `inode_v2_2`, they are considered shared files.
  // - Don't try to perform any hardlink detection, as the old format doesn't
  //   properly represent hardlinks.
  // - Build the `dir_entries` array.
  //
  // Here's a rough outline of the upgrade process:
  //
  // - Determine the number of entries that reference the same `inode_v2_2`.
  //   This will allow us to distinguish between unique and shared files.

  // upgrading entry_table_v2_2 to dir_entries

  auto const lnk_offset =
      dwarfs::internal::find_inode_rank_offset(md_, dwarfs::internal::inode_rank::INO_LNK);
  auto const reg_offset =
      dwarfs::internal::find_inode_rank_offset(md_, dwarfs::internal::inode_rank::INO_REG);
  auto const dev_offset =
      dwarfs::internal::find_inode_rank_offset(md_, dwarfs::internal::inode_rank::INO_DEV);

  std::vector<uint32_t> reg_inode_refs(md_.chunk_table.size() - 1, 0);

  for (auto const& inode : md_.inodes) {
    auto const inode_v2_2 = inode.inode_v2_2.value();
    if (reg_offset <= inode_v2_2 && inode_v2_2 < dev_offset) {
      auto const index = inode_v2_2 - reg_offset;
      if (index < reg_inode_refs.size()) {
        ++reg_inode_refs[index];
      }
    }
  }

  auto const unique_files =
      std::count_if(reg_inode_refs.begin(), reg_inode_refs.end(),
                    [](auto ref) { return ref == 1; });

  auto const num_reg_files =
      std::accumulate(reg_inode_refs.begin(), reg_inode_refs.end(), 0,
                      [](auto sum, auto ref) { return sum + ref; });

  // unique_files and num_reg_files processed

  auto const& entry_table = md_.entry_table_v2_2;

  metadata::domain::metadata newmd;
  auto& dir_entries = newmd.dir_entries.emplace();
  dir_entries.reserve(md_.inodes.size());
  newmd.shared_files_table = std::vector<uint32_t>();
  auto& shared_files = newmd.shared_files_table.value();
  shared_files.reserve(num_reg_files - unique_files);
  auto& chunks = newmd.chunks;
  chunks.reserve(md_.chunks.size());
  auto& chunk_table = newmd.chunk_table;
  chunk_table.reserve(md_.chunk_table.size());
  chunk_table.push_back(0);
  auto& inodes = newmd.inodes;
  inodes.resize(md_.inodes.size());

  newmd.directories = md_.directories;
  for (auto& d : newmd.directories) {
    d.set_parent_entry(entry_table[d.parent_entry()]);
  }
  auto& dirs = newmd.directories;

  uint32_t const shared_offset = reg_offset + unique_files;
  uint32_t unique_inode = reg_offset;
  uint32_t shared_inode = shared_offset;
  uint32_t shared_chunk_index = 0;
  std::unordered_map<uint32_t, uint32_t> shared_inode_map;
  std::vector<metadata::domain::chunk> shared_chunks;
  std::vector<uint32_t> shared_chunk_table;
  shared_chunk_table.push_back(0);

  for (auto const& inode : md_.inodes) {
    auto const self_index = dir_entries.size();
    auto& de = dir_entries.emplace_back();
    de.set_name_index(inode.name_index_v2_2.value());
    auto inode_v2_2 = inode.inode_v2_2.value();

    if (inode_v2_2 < reg_offset) {
      de.set_inode_num(inode_v2_2);

      // must reconstruct self_entry for directories
      if (inode_v2_2 < lnk_offset) {
        dirs.at(inode_v2_2).set_self_entry(self_index);
      }
    } else if (inode_v2_2 < dev_offset) {
      auto const index = inode_v2_2 - reg_offset;
      auto const refs = reg_inode_refs[index];
      auto const chunk_begin = md_.chunk_table.at(index);
      auto const chunk_end = md_.chunk_table.at(index + 1);

      if (refs == 1) {
        chunk_table.push_back(chunk_table.back() + chunk_end - chunk_begin);
        for (uint32_t i = chunk_begin; i < chunk_end; ++i) {
          chunks.push_back(md_.chunks.at(i));
        }
        de.set_inode_num(unique_inode++);
      } else {
        auto [it, inserted] =
            shared_inode_map.emplace(inode_v2_2, shared_inode);
        if (inserted) {
          for (uint32_t i = 0; i < refs; ++i) {
            shared_files.push_back(shared_chunk_index);
          }
          ++shared_chunk_index;
          shared_inode += refs;
          shared_chunk_table.push_back(shared_chunk_table.back() +
                                       chunk_end - chunk_begin);
          for (uint32_t i = chunk_begin; i < chunk_end; ++i) {
            shared_chunks.push_back(md_.chunks.at(i));
          }
        }
        de.set_inode_num(it->second++);
      }
    } else {
      de.set_inode_num((inode_v2_2 - dev_offset) + reg_offset +
                       num_reg_files);
    }

    auto& ni = inodes.at(de.inode_num());
    ni.mode_index = inode.mode_index;
    ni.owner_index = inode.owner_index;
    ni.group_index = inode.group_index;
    ni.atime_offset = inode.atime_offset;
    ni.mtime_offset = inode.mtime_offset;
    ni.ctime_offset = inode.ctime_offset;
  }

  std::transform(shared_chunk_table.begin(), shared_chunk_table.end(),
                 shared_chunk_table.begin(),
                 [&](auto i) { return i + chunks.size(); });

  DWARFS_CHECK(chunk_table.back() == shared_chunk_table.front(),
               "inconsistent chunk tables");

  chunks.insert(chunks.end(), shared_chunks.begin(), shared_chunks.end());
  chunk_table.insert(chunk_table.end(), shared_chunk_table.begin() + 1,
                     shared_chunk_table.end());

  newmd.symlink_table = md_.symlink_table;
  newmd.uids = md_.uids;
  newmd.gids = md_.gids;
  newmd.modes = md_.modes;
  newmd.names = md_.names;
  newmd.symlinks = md_.symlinks;
  newmd.timestamp_base = md_.timestamp_base;
  newmd.block_size = md_.block_size;
  newmd.total_fs_size = md_.total_fs_size;
  newmd.devices = md_.devices;
  newmd.options = md_.options;

  md_ = std::move(newmd);
}

} // namespace dwarfs::writer::internal