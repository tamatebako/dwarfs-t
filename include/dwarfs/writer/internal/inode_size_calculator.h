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
#include <vector>

namespace dwarfs::metadata::domain {
struct metadata;
struct chunk;
}

namespace dwarfs::writer::internal {

/**
 * Utility class for calculating inode sizes from metadata.
 * 
 * This class provides efficient size calculation for inodes,
 * taking into account:
 * - Regular data chunks
 * - Sparse file holes
 * - Large hole sizes
 * 
 * The calculator reads from the metadata's chunk_table and
 * chunks arrays to determine the actual and allocated sizes
 * of each inode.
 */
class inode_size_calculator {
 public:
  /**
   * Size information for an inode.
   */
  struct size_info {
    size_t num_chunks;      ///< Number of chunks (data + holes)
    uint64_t size;          ///< Total file size (including holes)
    uint64_t allocated_size; ///< Actual allocated size (excluding holes)
  };

  /**
   * Construct calculator from metadata.
   * 
   * @param md Metadata containing chunk_table, chunks, and hole info
   */
  explicit inode_size_calculator(metadata::domain::metadata const& md);

  /**
   * Calculate size information for an inode.
   * 
   * @param index Inode index in chunk_table
   * @return Size information with num_chunks, size, allocated_size
   * 
   * @throws std::out_of_range if index is invalid
   */
  size_info calculate(size_t index) const;

 private:
  std::vector<uint32_t> const& chunk_table_;
  std::vector<metadata::domain::chunk> const& chunks_;
  uint32_t block_size_;
  uint32_t hole_ix_;
  std::vector<uint64_t> const* large_hole_size_{nullptr};
};

} // namespace dwarfs::writer::internal