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

#include "flatbuffers_entry_processor.h"

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

  for (auto p : dirs) {
    if (!p->has_parent()) {
      p->set_entry_index(md_.dir_entries->size());
      p->pack_entry(md_, ge_data, timeres_);
    }

    p->pack(md_, ge_data, timeres_);
  }

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