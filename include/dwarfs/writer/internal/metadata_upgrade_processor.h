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

namespace dwarfs::metadata::domain {
class fs_options;
}

namespace dwarfs {
struct filesystem_version;
}

namespace dwarfs::writer::internal {

/**
 * Abstract interface for metadata upgrade operations.
 * 
 * Handles upgrading metadata from older DwarFS versions to the
 * current format. This interface defines operations for:
 * - Upgrading from pre-v2.2 entry table format
 * - Updating metadata version history
 * - Converting between metadata schema versions
 * - Validating metadata consistency during upgrades
 * 
 * The most complex upgrade is from pre-v2.2, which requires:
 * - Rebuilding inodes array (different ordering)
 * - Building shared_files_table (was implicit before)
 * - Building dir_entries array (didn't exist before)
 * - Reconstructing chunk and chunk_table arrays
 * 
 * Implementations must be format-specific (FlatBuffers or Thrift)
 * as they work directly with the underlying metadata representation.
 */
class metadata_upgrade_processor {
 public:
  virtual ~metadata_upgrade_processor() = default;

  /**
   * Upgrade metadata from older version to current format.
   * 
   * This method performs the following upgrades:
   * 
   * 1. Pre-v2.2 upgrade: If entry_table_v2_2 is present,
   *    converts to dir_entries format with proper shared files
   *    and hardlink detection.
   * 
   * 2. Version history: Records the original version, options,
   *    and block size in metadata_version_history (unless
   *    history is disabled).
   * 
   * The upgrade process preserves all file data and directory
   * structure while updating the metadata format for better
   * performance and features.
   * 
   * @param orig_fs_options Original filesystem options (may be null)
   * @param orig_fs_version Original filesystem version
   */
  virtual void upgrade_metadata(
      metadata::domain::fs_options const* orig_fs_options,
      filesystem_version const& orig_fs_version) = 0;

  /**
   * Upgrade from pre-v2.2 entry table format.
   * 
   * This is the most complex upgrade operation, converting:
   * - entry_table_v2_2 → dir_entries
   * - implicit shared files → explicit shared_files_table
   * - old inode ordering → new inode ordering
   * 
   * The old format combined hardlinks and shared files into
   * a single concept, which was incorrect. The new format
   * properly distinguishes between:
   * - Hardlinks: Same inode, multiple directory entries
   * - Shared files: Different inodes, same data chunks
   * 
   * This method rebuilds all affected data structures to
   * ensure correctness in the new format.
   */
  virtual void upgrade_from_pre_v2_2() = 0;
};

} // namespace dwarfs::writer::internal