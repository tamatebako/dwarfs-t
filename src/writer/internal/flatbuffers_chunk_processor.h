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

#include <dwarfs/writer/internal/metadata_chunk_processor.h>
#include <dwarfs/writer/internal/metadata_builder.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace dwarfs {
class logger;
}

namespace dwarfs::metadata::domain {
class metadata;
class chunk;
}

namespace dwarfs::writer {
struct metadata_options;
}

namespace dwarfs::internal {
class feature_set;
}

namespace dwarfs::writer::internal {

/**
 * FlatBuffers implementation of chunk processor.
 * 
 * Handles:
 * - Gathering chunks from inodes and blocks
 * - Mapping holes for sparse files
 * - Remapping blocks during recompression
 * - Updating block categories and metadata
 */
class flatbuffers_chunk_processor final : public metadata_chunk_processor {
 public:
  /**
   * Construct chunk processor.
   * 
   * @param lgr Logger instance
   * @param md Metadata to populate
   * @param features Feature set to update
   * @param options Metadata options
   * @param old_block_size Original block size (for recompression)
   */
  flatbuffers_chunk_processor(logger& lgr,
                              metadata::domain::metadata& md,
                              dwarfs::internal::feature_set& features,
                              metadata_options const& options,
                              std::optional<uint32_t> old_block_size = std::nullopt);

  // Implement interface
  void gather_chunks(inode_manager const& im,
                     block_manager const& bm,
                     size_t chunk_count) override;

  void remap_blocks(std::span<block_mapping const> mapping,
                    size_t new_block_count) override;

 private:
  using chunks_t = std::vector<metadata::domain::chunk>;
  using chunk_table_t = std::vector<uint32_t>;
  using categories_t = std::vector<uint32_t>;
  using category_metadata_t = std::map<uint32_t, uint32_t>;

  static constexpr auto kTmpHoleIx = std::numeric_limits<uint32_t>::max();

  /**
   * Remap holes to new block layout.
   * 
   * @param new_chunks Chunks with temporary hole indices
   * @param new_hole_index New hole block index
   * @param max_data_chunk_size Maximum data chunk size
   */
  void remap_holes(chunks_t& new_chunks,
                   size_t new_hole_index,
                   size_t max_data_chunk_size);

  logger& lgr_;
  metadata::domain::metadata& md_;
  dwarfs::internal::feature_set& features_;
  metadata_options const& options_;
  std::optional<uint32_t> old_block_size_;
};

} // namespace dwarfs::writer::internal