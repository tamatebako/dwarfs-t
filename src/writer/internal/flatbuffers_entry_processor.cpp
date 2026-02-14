/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc. (OOP refactoring)
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

#include "flatbuffers_entry_processor.h"

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <dwarfs/file_stat.h>
#include <dwarfs/fstypes.h>
#include <dwarfs/logger.h>
#include <dwarfs/writer/metadata_options.h>

#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/writer/internal/chmod_transformer.h>
#include <dwarfs/writer/internal/entry.h>
#include <dwarfs/writer/internal/global_entry_data.h>
#include <dwarfs/writer/internal/time_resolution_converter.h>

namespace dwarfs::writer::internal {

flatbuffers_entry_processor::flatbuffers_entry_processor(
    logger& lgr, metadata::domain::metadata& md,
    metadata_options const& options, time_resolution_converter& timeres)
    : lgr_{lgr}, md_{md}, options_{options}, timeres_{timeres} {}

void flatbuffers_entry_processor::gather_entries(
    std::span<dir*> dirs, global_entry_data const& ge_data,
    uint32_t num_inodes) {
  md_.dir_entries = std::vector<metadata::domain::dir_entry>();
  md_.inodes.resize(num_inodes);
  md_.directories.reserve(dirs.size() + 1);

  // Single-pass: For each directory in depth-first order,
  // add its entry and then its children.
  // This maintains the depth-first ordering required by the walk algorithm,
  // where first_entry correctly points to the first child of each directory.
  for (auto p : dirs) {
    // Check if entry_index is already set (by parent's pack())
    // If so, the directory's entry was already added as a child of its parent
    if (!p->entry_index()) {
      // Set entry_index and add directory's own entry to dir_entries
      uint32_t entry_idx = md_.dir_entries->size();
      p->set_entry_index(entry_idx);
      p->pack_entry(md_, ge_data, timeres_);
    }

    // Now pack directory metadata and add children
    // first_entry will be set to the current size of dir_entries,
    // which is correct because children come after the directory's entry
    p->pack(md_, ge_data, timeres_);
  }

  // Add sentinel directory
  metadata::domain::directory sentinel;
  sentinel.set_parent_entry(0);
  sentinel.set_first_entry(md_.dir_entries->size());
  sentinel.set_self_entry(0);
  md_.directories.push_back(sentinel);
}

void flatbuffers_entry_processor::gather_global_entry_data(
    global_entry_data const& ge_data) {
  md_.names = ge_data.get_names();
  md_.symlinks = ge_data.get_symlinks();

  md_.uids = options_.uid
                 ? std::vector<file_stat::uid_type>{*options_.uid}
                 : ge_data.get_uids();

  md_.gids = options_.gid
                 ? std::vector<file_stat::gid_type>{*options_.gid}
                 : ge_data.get_gids();

  md_.modes = ge_data.get_modes();
  md_.timestamp_base = timeres_.convert_offset(ge_data.get_timestamp_base());

  apply_chmod();
}

void flatbuffers_entry_processor::apply_chmod() {
  if (options_.chmod_specifiers.empty()) {
    return;
  }

  std::vector<uint32_t> new_modes;
  std::unordered_map<uint32_t, uint32_t> new_mode_index_map;
  auto xfm = chmod_transformer::build_chain(options_.chmod_specifiers,
                                             options_.umask);

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

} // namespace dwarfs::writer::internal
