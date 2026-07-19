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

#include <dwarfs/writer/internal/inode_size_calculator.h>

#include <bit>
#include <cassert>

#include <dwarfs/metadata/domain/metadata.h>

namespace dwarfs::writer::internal {

namespace {
constexpr uint32_t kChunkOffsetIsLargeHole = 0xFFFFFFFF;
} // namespace

inode_size_calculator::inode_size_calculator(
    metadata::domain::metadata const& md)
    : chunk_table_{md.chunk_table}
    , chunks_{md.chunks}
    , block_size_{md.block_size}
    , hole_ix_{md.hole_block_index.value_or(UINT32_MAX)} {
  if (md.large_hole_size.has_value()) {
    large_hole_size_ = &md.large_hole_size.value();
  }
  assert(std::has_single_bit(block_size_));
}

inode_size_calculator::size_info
inode_size_calculator::calculate(size_t index) const {
  assert(index + 1 < chunk_table_.size());

  auto const begin = chunk_table_[index];
  auto const end = chunk_table_[index + 1];
  auto const num_chunks = end - begin;
  uint64_t size{0};
  uint64_t allocated_size{0};

  for (uint32_t ix = begin; ix < end; ++ix) {
    auto const& chunk = chunks_[ix];
    auto const b = chunk.block();
    auto const o = chunk.offset();
    auto const s = chunk.size();

    if (b == hole_ix_) {
      if (o == kChunkOffsetIsLargeHole) {
        assert(large_hole_size_);
        assert(s < large_hole_size_->size());
        size += large_hole_size_->at(s);
      } else {
        assert(o < block_size_);
        size += s * block_size_ + o;
      }
    } else {
      size += s;
      allocated_size += s;
    }
  }

  return size_info{num_chunks, size, allocated_size};
}

} // namespace dwarfs::writer::internal