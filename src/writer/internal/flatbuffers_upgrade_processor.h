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

#include <dwarfs/writer/internal/metadata_upgrade_processor.h>

namespace dwarfs {
class logger;
}

namespace dwarfs::metadata::domain {
class metadata;
}

namespace dwarfs::writer {
struct metadata_options;
}

namespace dwarfs::writer::internal {

/**
 * FlatBuffers implementation of upgrade processor.
 * 
 * Handles:
 * - Upgrading from pre-v2.2 entry table format
 * - Recording metadata version history
 * - Reconstructing inodes, chunks, and shared files
 */
class flatbuffers_upgrade_processor final : public metadata_upgrade_processor {
 public:
  /**
   * Construct upgrade processor.
   * 
   * @param lgr Logger instance
   * @param md Metadata to upgrade
   * @param options Metadata options
   */
  flatbuffers_upgrade_processor(logger& lgr,
                                metadata::domain::metadata& md,
                                metadata_options const& options);

  // Implement interface
  void upgrade_metadata(metadata::domain::fs_options const* orig_fs_options,
                        filesystem_version const& orig_fs_version) override;

  void upgrade_from_pre_v2_2() override;

 private:
  logger& lgr_;
  metadata::domain::metadata& md_;
  metadata_options const& options_;
};

} // namespace dwarfs::writer::internal