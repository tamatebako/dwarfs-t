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

#pragma once

#include <dwarfs/writer/internal/metadata_entry_processor.h>

#include <cstdint>
#include <span>

namespace dwarfs {
class logger;
}

namespace dwarfs::metadata::domain {
struct metadata;
}

namespace dwarfs::writer {
struct metadata_options;
}

namespace dwarfs::writer::internal {

class time_resolution_converter;

/**
 * FlatBuffers implementation of entry processor.
 * 
 * Handles:
 * - Processing directories and their entries
 * - Gathering global entry data (names, symlinks, UIDs, GIDs, modes)
 * - Managing directory structure
 * - Applying chmod transformations
 */
class flatbuffers_entry_processor final : public metadata_entry_processor {
 public:
  /**
   * Construct entry processor.
   * 
   * @param lgr Logger instance
   * @param md Metadata to populate
   * @param options Metadata options
   * @param timeres Time resolution converter
   */
  flatbuffers_entry_processor(logger& lgr,
                              metadata::domain::metadata& md,
                              metadata_options const& options,
                              time_resolution_converter& timeres);

  // Implement interface
  void gather_entries(std::span<dir*> dirs,
                      global_entry_data const& ge_data,
                      uint32_t num_inodes) override;

  void gather_global_entry_data(global_entry_data const& ge_data) override;

 private:
  /**
   * Apply chmod transformations to modes.
   */
  void apply_chmod();

  logger& lgr_;
  metadata::domain::metadata& md_;
  metadata_options const& options_;
  time_resolution_converter& timeres_;
};

} // namespace dwarfs::writer::internal