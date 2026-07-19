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

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dwarfs::metadata::legacy {

/**
 * Frozen2 bit-level read/write operations
 *
 * This class provides low-level bit manipulation operations used by the
 * Frozen2 serialization format. It handles reading and writing arbitrary
 * numbers of bits (1-64) from/to byte arrays at bit-level precision.
 *
 * The implementation is based on the dwarfs-rs Frozen2 deserializer,
 * which uses little-endian byte order and supports reading/writing
 * values that span byte boundaries.
 *
 * Key characteristics:
 * - Supports 1-64 bit operations
 * - Little-endian byte order
 * - Handles bit offsets within bytes
 * - Handles values spanning multiple bytes (up to 9 bytes for 64-bit values)
 */
class frozen_bits {
public:
  /**
   * Load N bits (1-64) from data at bit offset
   *
   * @param data Source byte array
   * @param base_bit Bit offset from start of data (0-based)
   * @param bits Number of bits to load (1-64)
   * @return Value read from the specified bit range
   *
   * Example:
   *   data = [0x12, 0x34, 0x56, ...]
   *   load_bits(data, 4, 8) reads bits 4-11, spanning bytes 0-1
   */
  static uint64_t load_bits(std::span<uint8_t const> data, uint32_t base_bit,
                            uint16_t bits);

  /**
   * Store N bits (1-64) to data at bit offset
   *
   * @param data Destination byte array (must have sufficient space)
   * @param base_bit Bit offset from start of data (0-based)
   * @param bits Number of bits to store (1-64)
   * @param value Value to write (only lower `bits` bits are used)
   *
   * Example:
   *   data = [0x00, 0x00, ...]
   *   store_bits(data, 4, 8, 0xFF) stores 0xFF at bits 4-11
   */
  static void store_bits(std::span<uint8_t> data, uint32_t base_bit,
                         uint16_t bits, uint64_t value);

  /**
   * Load single bit from data at bit offset
   *
   * @param data Source byte array
   * @param base_bit Bit offset from start of data (0-based)
   * @return true if bit is set, false otherwise
   */
  static bool load_bit(std::span<uint8_t const> data, uint32_t base_bit);

  /**
   * Store single bit to data at bit offset
   *
   * @param data Destination byte array
   * @param base_bit Bit offset from start of data (0-based)
   * @param value Bit value to store
   */
  static void store_bit(std::span<uint8_t> data, uint32_t base_bit,
                        bool value);

  /**
   * Calculate the number of bytes needed to store N bits
   *
   * @param bits Number of bits
   * @return Number of bytes required (rounded up)
   */
  static constexpr uint32_t bytes_for_bits(uint32_t bits) {
    return (bits + 7) / 8;
  }

  /**
   * Calculate the number of bytes needed for a bit range
   *
   * @param base_bit Starting bit offset
   * @param bits Number of bits
   * @return Number of bytes required to store the bit range
   */
  static constexpr uint32_t bytes_for_range(uint32_t base_bit, uint32_t bits) {
    return bytes_for_bits(base_bit + bits);
  }
};

} // namespace dwarfs::metadata::legacy