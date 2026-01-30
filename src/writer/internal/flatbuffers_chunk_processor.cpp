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

#include "flatbuffers_chunk_processor.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include <dwarfs/fstypes.h>
#include <dwarfs/logger.h>
#include <dwarfs/util.h>
#include <dwarfs/writer/metadata_options.h>

#include <dwarfs/internal/features.h>
#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/writer/internal/block_manager.h>
#include <dwarfs/writer/internal/entry.h>
#include <dwarfs/writer/internal/inode_hole_mapper.h>
#include <dwarfs/writer/internal/inode_manager.h>

namespace dwarfs::writer::internal {

// For block_chunk
using dwarfs::feature;

flatbuffers_chunk_processor::flatbuffers_chunk_processor(
    logger& lgr, metadata::domain::metadata& md,
    dwarfs::internal::feature_set& features, metadata_options const& options,
    std::optional<uint32_t> old_block_size)
    : lgr_{lgr}
    , md_{md}
    , features_{features}
    , options_{options}
    , old_block_size_{old_block_size} {}

void flatbuffers_chunk_processor::gather_chunks(
    inode_manager const& im, block_manager const& bm, size_t chunk_count) {
  std::cerr << "[GATHER_CHUNKS] im.count()=" << im.count() << ", chunk_count=" << chunk_count << std::endl;
  std::cerr << "[GATHER_CHUNKS] md_.inodes.size()=" << md_.inodes.size() << std::endl;

  md_.chunk_table.resize(im.count() + 1);
  md_.chunks.reserve(chunk_count);

  std::cerr << "[GATHER_CHUNKS] After resize, md_.chunk_table.size()=" << md_.chunk_table.size() << std::endl;

  std::optional<inode_hole_mapper> hole_mapper;

  if (options_.enable_sparse_files) {
    assert(md_.block_size > 0);
    hole_mapper.emplace(bm.num_blocks(), md_.block_size,
                        im.get_max_data_chunk_size());
  }

  size_t inode_count = 0;
  im.for_each_inode_in_order([&](std::shared_ptr<inode> const& ino) {
    auto const total_chunks = md_.chunks.size();
    std::cerr << "[GATHER_CHUNKS] Processing inode " << ino->num() << ", total_chunks=" << total_chunks << std::endl;
    DWARFS_NOTHROW(md_.chunk_table.at(ino->num())) = total_chunks;
    ++inode_count;
    if (!ino->append_chunks_to(md_.chunks, hole_mapper)) {
      std::ostringstream oss;
      for (auto fp : ino->all()) {
        oss << "\n  " << fp->path_as_string();
      }
      level_log_entry(lgr_, logger::LVL_ERROR, DWARFS_CURRENT_SOURCE_LOCATION)
          << "inconsistent fragments in inode " << ino->num()
          << ", the following files will be empty:" << oss.str();
    }
  });

  bm.map_logical_blocks(md_.chunks, hole_mapper);

  // Insert sentinel
  DWARFS_NOTHROW(md_.chunk_table.at(im.count())) = md_.chunks.size();

  std::cerr << "[GATHER_CHUNKS] After sentinel, chunk_table.size()=" << md_.chunk_table.size() << std::endl;
  std::cerr << "[GATHER_CHUNKS] Processed " << inode_count << " inodes" << std::endl;

  if (hole_mapper && hole_mapper->has_holes()) {
    md_.hole_block_index = hole_mapper->hole_block_index();
    md_.large_hole_size = hole_mapper->large_hole_sizes();
    features_.add(feature::sparsefiles);
  }

  level_log_entry(lgr_, logger::DEBUG, DWARFS_CURRENT_SOURCE_LOCATION)
      << "total number of unique files: " << im.count();
  level_log_entry(lgr_, logger::DEBUG, DWARFS_CURRENT_SOURCE_LOCATION)
      << "total number of chunks: " << md_.chunks.size();
}

void flatbuffers_chunk_processor::remap_blocks(
    std::span<block_mapping const> mapping, size_t new_block_count) {
  timed_level_log_entry tv(lgr_, logger::VERBOSE, DWARFS_CURRENT_SOURCE_LOCATION);

  std::span<metadata::domain::chunk> old_chunks = md_.chunks;
  std::span<uint32_t> old_chunk_table = md_.chunk_table;

  DWARFS_CHECK(!old_chunk_table.empty(), "chunk table must not be empty");

  auto const old_hole_ix = md_.hole_block_index;

  DWARFS_CHECK(old_hole_ix.has_value() == features_.has(feature::sparsefiles),
               "inconsistent sparse files feature flag");

  chunks_t new_chunks;
  chunk_table_t new_chunk_table;
  size_t max_data_chunk_size{0};

  new_chunk_table.push_back(0);

  for (size_t i = 0; i < old_chunk_table.size() - 1; ++i) {
    auto chunks = old_chunks.subspan(
        old_chunk_table[i], old_chunk_table[i + 1] - old_chunk_table[i]);

    std::vector<block_chunk> mapped_chunks;

    for (auto const& chunk : chunks) {
      if (old_hole_ix && chunk.block() == *old_hole_ix) {
        level_log_entry(lgr_, logger::TRACE, DWARFS_CURRENT_SOURCE_LOCATION)
            << "mapping hole chunk: offset=" << chunk.offset()
            << ", size=" << chunk.size();

        mapped_chunks.push_back({kTmpHoleIx, chunk.offset(), chunk.size()});
      } else {
        level_log_entry(lgr_, logger::TRACE, DWARFS_CURRENT_SOURCE_LOCATION)
            << "mapping data chunk: block=" << chunk.block()
            << ", offset=" << chunk.offset() << ", size=" << chunk.size();

        DWARFS_CHECK(chunk.block() < mapping.size(),
                     "chunk block out of range");

        auto mapped =
            mapping[chunk.block()].map_chunk(chunk.offset(), chunk.size());

        DWARFS_CHECK(!mapped.empty(), "mapped chunk list is empty");
        level_log_entry(lgr_, logger::TRACE, DWARFS_CURRENT_SOURCE_LOCATION)
            << "  mapped to " << mapped.size() << " chunks";

        for (auto const& mc : mapped) {
          level_log_entry(lgr_, logger::TRACE, DWARFS_CURRENT_SOURCE_LOCATION)
              << "    block=" << mc.block << ", offset=" << mc.offset
              << ", size=" << mc.size;
        }

        auto first = mapped.begin();

        if (!mapped_chunks.empty() &&
            mapped_chunks.back().block != kTmpHoleIx &&
            mapped_chunks.back().block == mapped.front().block &&
            mapped_chunks.back().offset + mapped_chunks.back().size ==
                mapped.front().offset) {
          level_log_entry(lgr_, logger::TRACE, DWARFS_CURRENT_SOURCE_LOCATION)
              << "  merging with previous chunk";
          mapped_chunks.back().size += mapped.front().size;
          ++first;
        }

        mapped_chunks.insert(mapped_chunks.end(), first, mapped.end());
      }
    }

    for (auto const& chunk : mapped_chunks) {
      auto& nc = new_chunks.emplace_back();
      nc.set_block(chunk.block);
      nc.set_offset(chunk.offset);
      nc.set_size(chunk.size);

      if (chunk.block != kTmpHoleIx) {
        max_data_chunk_size = std::max(max_data_chunk_size, chunk.size);
      }
    }

    new_chunk_table.push_back(new_chunks.size());
  }

  if (old_hole_ix) {
    remap_holes(new_chunks, new_block_count, max_data_chunk_size);
  }

  auto const& old_categories = md_.block_categories;
  auto const& old_category_metadata = md_.block_category_metadata;

  if ((old_categories.has_value() && !old_categories->empty()) ||
      (old_category_metadata.has_value() && !old_category_metadata->empty())) {
    std::unordered_map<uint32_t, uint32_t> block_map;
    for (auto const& m : mapping) {
      for (auto const& c : m.chunks) {
        block_map[c.block] = m.old_block;
      }
    }

    if (old_categories.has_value() && !old_categories->empty()) {
      categories_t new_categories;
      new_categories.resize(block_map.size());
      for (auto const& [new_block, old_block] : block_map) {
        new_categories[new_block] = old_categories->at(old_block);
      }
      md_.block_categories = std::move(new_categories);
    }

    if (old_category_metadata.has_value() && !old_category_metadata->empty()) {
      category_metadata_t new_category_metadata;
      for (auto const& [new_block, old_block] : block_map) {
        auto it = old_category_metadata->find(old_block);
        if (it != old_category_metadata->end()) {
          new_category_metadata[new_block] = it->second;
        }
      }
      md_.block_category_metadata = std::move(new_category_metadata);
    }
  }

  md_.chunks = std::move(new_chunks);
  md_.chunk_table = std::move(new_chunk_table);
  
  tv << "remapping blocks...";
}

void flatbuffers_chunk_processor::remap_holes(chunks_t& new_chunks,
                                               size_t new_hole_index,
                                               size_t max_data_chunk_size) {
  level_log_entry(lgr_, logger::DEBUG, DWARFS_CURRENT_SOURCE_LOCATION)
      << "remapping holes (hole index: " << md_.hole_block_index.value()
      << " -> " << new_hole_index << ")";

  auto const old_block_size = old_block_size_.value();
  auto const new_block_size = md_.block_size;

  inode_hole_mapper hole_mapper(new_hole_index, new_block_size,
                                max_data_chunk_size);

  for (auto& c : new_chunks) {
    if (c.block() == kTmpHoleIx) {
      auto const offset = c.offset();
      auto const size = c.size();
      file_size_t hole_size{0};

      if (offset == kChunkOffsetIsLargeHole) {
        assert(md_.large_hole_size.has_value());
        assert(size < md_.large_hole_size->size());
        hole_size = md_.large_hole_size->at(size);
      } else {
        hole_size = static_cast<file_size_t>(size) * old_block_size + offset;
      }

      hole_mapper.map_hole(c, hole_size);
    }
  }

  md_.hole_block_index = hole_mapper.hole_block_index();
  md_.large_hole_size = hole_mapper.large_hole_sizes();
}

} // namespace dwarfs::writer::internal