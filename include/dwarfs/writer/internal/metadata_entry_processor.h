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

#include <cstddef>
#include <cstdint>
#include <span>

namespace dwarfs::writer::internal {

class dir;
class global_entry_data;

/**
 * Abstract interface for directory entry processing operations.
 * 
 * Handles gathering directory entries and global entry data
 * (names, symlinks, UIDs, GIDs, modes, timestamps) into the
 * metadata structure. This interface defines operations for:
 * - Processing directories and their entries
 * - Gathering global entry data tables
 * - Managing directory structure and entry indices
 * 
 * Implementations must be format-specific (FlatBuffers or Thrift)
 * as they work directly with the underlying metadata representation.
 */
class metadata_entry_processor {
 public:
  virtual ~metadata_entry_processor() = default;

  /**
   * Gather directory entries from directory tree.
   * 
   * This method processes the directory tree and populates:
   * - dir_entries array with all directory entries
   * - inodes array sized for all inodes
   * - directories array with directory metadata
   * 
   * Each directory's entries are packed into the metadata,
   * and the directory structure is preserved through indices.
   * 
   * @param dirs Array of directory pointers to process
   * @param ge_data Global entry data (names, symlinks, etc.)
   * @param num_inodes Total number of inodes
   */
  virtual void gather_entries(std::span<dir*> dirs,
                               global_entry_data const& ge_data,
                               uint32_t num_inodes) = 0;

  /**
   * Gather global entry data into metadata.
   * 
   * This method populates the global data tables:
   * - names: File/directory names
   * - symlinks: Symlink targets
   * - uids: User IDs
   * - gids: Group IDs
   * - modes: File modes/permissions
   * - timestamp_base: Base timestamp for offsets
   * 
   * If UID/GID overrides are specified in options, those
   * are used instead of the values from ge_data.
   * 
   * @param ge_data Global entry data to gather
   */
  virtual void gather_global_entry_data(global_entry_data const& ge_data) = 0;
};

} // namespace dwarfs::writer::internal