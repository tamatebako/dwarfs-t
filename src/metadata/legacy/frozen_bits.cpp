/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhx.github.io)
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

#include "dwarfs/metadata/legacy/frozen_bits.h"

#include <cstring>
#include <stdexcept>

namespace dwarfs::metadata::legacy {

uint64_t frozen_bits::load_bits(std::span<uint8_t const> data,
                                 uint32_t base_bit, uint16_t bits) {
  if (bits == 0 || bits > 64) {
    throw std::invalid_argument("bits must be between 1 and 64");
  }

  // Calculate byte position and bit offset within byte
  uint32_t byte_idx = base_bit / 8;
  uint16_t bit_start = base_bit % 8;

  // Check if we have enough data
  if (byte_idx >= data.size()) {
    throw std::out_of_range("base_bit exceeds data size");
  }

  // Load 8-byte chunk (little-endian)
  uint64_t x;
  if (byte_idx + 8 <= data.size()) {
    // Simple case: full 8 bytes available
    std::memcpy(&x, &data[byte_idx], 8);
  } else {
    // Near end: zero-pad remaining bytes
    uint8_t buf[8] = {0};
    size_t available = data.size() - byte_idx;
    std::memcpy(buf, &data[byte_idx], available);
    std::memcpy(&x, buf, 8);
  }

  // Extract bits from the loaded chunk
  uint16_t start_and_bits = bit_start + bits;

  if (start_and_bits <= 64) {
    // Simple case: all bits within the 8-byte chunk
    // Shift left to align to MSB, then shift right to get desired bits
    return (x << (64 - start_and_bits)) >> (64 - bits);
  } else {
    // Overshooting case: need bits from 9th byte
    // This happens when reading 64 bits from a non-aligned position
    uint16_t overshooting_bits = start_and_bits & 63; // equivalent to % 64

    // Check if 9th byte exists
    if (byte_idx + 8 >= data.size()) {
      // No 9th byte available, just use what we have
      return (x << (64 - start_and_bits)) >> (64 - bits);
    }

    // Load the 9th byte
    uint64_t hi = data[byte_idx + 8];

    // Combine: lower bits from x, upper bits from hi
    // x >> bit_start gives us the lower part
    // hi << (64 - overshooting_bits) >> (64 - bits) gives us the upper part
    return (x >> bit_start) | ((hi << (64 - overshooting_bits)) >> (64 - bits));
  }
}

void frozen_bits::store_bits(std::span<uint8_t> data, uint32_t base_bit,
                              uint16_t bits, uint64_t value) {
  if (bits == 0 || bits > 64) {
    throw std::invalid_argument("bits must be between 1 and 64");
  }

  // Calculate byte position and bit offset within byte
  uint32_t byte_idx = base_bit / 8;
  uint16_t bit_start = base_bit % 8;

  // Check if we have enough space
  uint32_t bytes_needed = bytes_for_range(base_bit, bits);
  if (bytes_needed > data.size()) {
    throw std::out_of_range("insufficient space in data buffer");
  }

  // Mask to keep only the lower `bits` bits of value
  uint64_t mask = (bits == 64) ? UINT64_MAX : ((1ULL << bits) - 1);
  value &= mask;

  // Load existing 8-byte chunk (little-endian)
  uint64_t x;
  if (byte_idx + 8 <= data.size()) {
    std::memcpy(&x, &data[byte_idx], 8);
  } else {
    // Near end: zero-pad
    uint8_t buf[8] = {0};
    size_t available = data.size() - byte_idx;
    std::memcpy(buf, &data[byte_idx], available);
    std::memcpy(&x, buf, 8);
  }

  uint16_t start_and_bits = bit_start + bits;

  if (start_and_bits <= 64) {
    // Simple case: all bits fit within 8 bytes
    // Create mask for the bits we're modifying
    uint64_t write_mask = (bits == 64) ? UINT64_MAX : ((1ULL << bits) - 1);
    write_mask <<= bit_start;

    // Clear the target bits and set new value
    x = (x & ~write_mask) | (value << bit_start);

    // Write back
    if (byte_idx + 8 <= data.size()) {
      std::memcpy(&data[byte_idx], &x, 8);
    } else {
      // Near end: only write available bytes
      size_t available = data.size() - byte_idx;
      std::memcpy(&data[byte_idx], &x, available);
    }
  } else {
    // Overshooting case: bits span into 9th byte
    uint16_t overshooting_bits = start_and_bits & 63;

    // Split value into low and high parts
    uint64_t value_low = value << bit_start;
    uint64_t value_high = value >> (64 - bit_start);

    // Create masks
    uint64_t mask_low = ~(UINT64_MAX << bit_start);
    uint64_t mask_high = (1ULL << overshooting_bits) - 1;

    // Update low 8 bytes
    x = (x & mask_low) | value_low;
    std::memcpy(&data[byte_idx], &x, 8);

    // Update 9th byte
    uint8_t byte9 = data[byte_idx + 8];
    byte9 = (byte9 & ~mask_high) | static_cast<uint8_t>(value_high);
    data[byte_idx + 8] = byte9;
  }
}

bool frozen_bits::load_bit(std::span<uint8_t const> data, uint32_t base_bit) {
  uint32_t byte_idx = base_bit / 8;
  uint16_t bit_idx = base_bit % 8;

  if (byte_idx >= data.size()) {
    throw std::out_of_range("base_bit exceeds data size");
  }

  return (data[byte_idx] & (1 << bit_idx)) != 0;
}

void frozen_bits::store_bit(std::span<uint8_t> data, uint32_t base_bit,
                             bool value) {
  uint32_t byte_idx = base_bit / 8;
  uint16_t bit_idx = base_bit % 8;

  if (byte_idx >= data.size()) {
    throw std::out_of_range("base_bit exceeds data size");
  }

  if (value) {
    data[byte_idx] |= (1 << bit_idx);
  } else {
    data[byte_idx] &= ~(1 << bit_idx);
  }
}

} // namespace dwarfs::metadata::legacy