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
#include <span>

namespace dwarfs::writer::internal {

class inode_manager;
class block_manager;
struct block_mapping;

/**
 * Abstract interface for chunk processing operations.
 * 
 * Handles chunk gathering from inodes and block remapping during
 * recompression. This interface defines operations for:
 * - Gathering chunks from inode manager and block manager
 * - Remapping blocks according to new compression layout
 * - Managing holes for sparse files
 * 
 * Implementations must be format-specific (FlatBuffers or Thrift)
 * as they work directly with the underlying metadata representation.
 */
class metadata_chunk_processor {
 public:
  virtual ~metadata_chunk_processor() = default;

  /**
   * Gather chunks from inode manager and block manager.
   * 
   * This method collects all chunks for all inodes in the order
   * specified by the inode manager. It populates the chunk table
   * and chunks array in the underlying metadata structure.
   * 
   * For sparse files (if enabled), this method also manages hole
   * mapping to represent sparse regions efficiently.
   * 
   * @param im Inode manager containing file data
   * @param bm Block manager with compression blocks
   * @param chunk_count Expected total number of chunks
   * 
   * @throws std::runtime_error if inconsistent fragments detected
   */
  virtual void gather_chunks(inode_manager const& im,
                             block_manager const& bm,
                             size_t chunk_count) = 0;

  /**
   * Remap blocks according to new compression layout.
   * 
   * This method is used during recompression to update chunk
   * references when blocks are recompressed with different
   * settings. It handles:
   * - Mapping old block numbers to new block numbers
   * - Splitting chunks that span multiple new blocks
   * - Merging consecutive chunks in the same block
   * - Preserving hole chunks for sparse files
   * - Updating block categories and metadata
   * 
   * @param mapping Block remapping information (old → new)
   * @param new_block_count Total number of blocks after remapping
   * 
   * @throws std::runtime_error if mapping is inconsistent
   */
  virtual void remap_blocks(std::span<block_mapping const> mapping,
                            size_t new_block_count) = 0;
};

} // namespace dwarfs::writer::internal