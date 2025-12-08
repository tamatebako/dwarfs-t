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

#include <string>

namespace dwarfs::metadata::domain {
struct metadata;
}

namespace dwarfs::writer::internal {

/**
 * Utility class for validating metadata consistency.
 * 
 * This class provides validation checks for metadata structures
 * to ensure internal consistency before serialization. It verifies:
 * - Chunk table indices are valid
 * - Directory structure is consistent
 * - String table indices are in range
 * - Shared files table is sorted
 * - Feature flags match actual structure
 */
class metadata_validator {
 public:
  /**
   * Validate metadata structure.
   * 
   * Performs comprehensive validation of all metadata fields
   * and ensures internal consistency.
   * 
   * @param md Metadata to validate
   * @return Empty string if valid, error message otherwise
   */
  static std::string validate(metadata::domain::metadata const& md);

  /**
   * Validate chunk table consistency.
   * 
   * Checks that:
   * - chunk_table size matches inode count + 1
   * - All indices are monotonically increasing
   * - Final index equals chunks.size()
   * 
   * @param md Metadata to validate
   * @return Empty string if valid, error message otherwise
   */
  static std::string validate_chunk_table(metadata::domain::metadata const& md);

  /**
   * Validate shared files table.
   * 
   * Checks that shared_files_table (if present) is sorted
   * and contains valid indices.
   * 
   * @param md Metadata to validate
   * @return Empty string if valid, error message otherwise
   */
  static std::string validate_shared_files(metadata::domain::metadata const& md);
};

} // namespace dwarfs::writer::internal