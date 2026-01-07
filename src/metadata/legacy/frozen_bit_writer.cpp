/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
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

#include "dwarfs/metadata/legacy/frozen_bit_writer.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace dwarfs::metadata::legacy {

FrozenBitWriter::FrozenBitWriter(size_t initial_capacity) {
  buf_.reserve(initial_capacity);
}

void FrozenBitWriter::ensure_capacity(size_t bit_offset) {
  size_t required_bytes = (bit_offset + 7) / 8;
  if (buf_.size() < required_bytes) {
    buf_.resize(required_bytes, 0);
  }
}

void FrozenBitWriter::write_bits(size_t bit_offset, size_t bit_width,
                                   uint64_t value) {
  // Ported from: ser_frozen.rs:540-551 (put_primitive inline write)
  //
  // Writing strategy:
  // 1. Little-endian byte order
  // 2. Bits packed from low to high within each byte
  // 3. Can span multiple bytes
  //
  // Example: write 12 bits at offset 4
  //   Byte 0: bits 4-7 (4 bits from value)
  //   Byte 1: bits 0-7 (8 bits from value, shifted right 4)

  if (bit_width == 0) {
    return; // Nothing to write
  }

  if (bit_width > 64) {
    throw std::runtime_error("bit_width exceeds 64 bits");
  }

  ensure_capacity(bit_offset + bit_width);

  size_t byte_idx = bit_offset / 8;
  size_t bit_start = bit_offset % 8;

  // Mask to extract only the bits we want to write
  uint64_t mask = bit_width == 64 ? ~0ULL : (1ULL << bit_width) - 1;
  value &= mask;

  // Write bits byte by byte (little-endian)
  size_t remaining_bits = bit_width;
  size_t value_offset = 0;

  while (remaining_bits > 0) {
    // How many bits can we write to current byte?
    size_t bits_in_byte = std::min(8 - bit_start, remaining_bits);

    // Extract bits to write
    uint8_t chunk = static_cast<uint8_t>((value >> value_offset) &
                                          ((1ULL << bits_in_byte) - 1));

    // Create byte mask for bits we're writing
    uint8_t byte_mask = static_cast<uint8_t>(((1ULL << bits_in_byte) - 1)
                                              << bit_start);

    // Clear target bits and write new value
    buf_[byte_idx] = (buf_[byte_idx] & ~byte_mask) | (chunk << bit_start);

    // Advance
    ++byte_idx;
    value_offset += bits_in_byte;
    remaining_bits -= bits_in_byte;
    bit_start = 0; // Subsequent bytes start at bit 0
  }
}

void FrozenBitWriter::write_bits_advancing(size_t bit_width, uint64_t value) {
  write_bits(write_pos_bits_, bit_width, value);
  write_pos_bits_ += bit_width;
}

void FrozenBitWriter::align_to_byte() {
  if (write_pos_bits_ % 8 != 0) {
    write_pos_bits_ = (write_pos_bits_ + 7) / 8 * 8;
  }
}

void FrozenBitWriter::resize(size_t bytes) {
  buf_.resize(bytes, 0);
}

std::vector<uint8_t> FrozenBitWriter::take_buffer() {
  // Trim to actual written size
  size_t final_size = position_bytes();
  buf_.resize(final_size);
  return std::move(buf_);
}

} // namespace dwarfs::metadata::legacy