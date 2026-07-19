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

#pragma once

#include <cstdint>
#include <vector>

namespace dwarfs::metadata::legacy {

/**
 * Frozen2 bit-level writer
 *
 * Provides bit-level write operations for Frozen2 bit-packing serialization.
 * Handles writing arbitrary bit widths (1-64 bits) at arbitrary bit offsets.
 *
 * Ported from: dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs
 * Specifically: Serializer::put_primitive (lines 540-551)
 */
class FrozenBitWriter {
 public:
  /**
   * Construct bit writer with initial buffer size
   *
   * @param initial_capacity Initial buffer capacity in bytes
   */
  explicit FrozenBitWriter(size_t initial_capacity = 4096);

  /**
   * Write bits at specific bit offset
   *
   * Writes `bit_width` bits from `value` (low bits) to buffer at `bit_offset`.
   * Buffer auto-expands as needed.
   *
   * @param bit_offset Absolute bit offset in buffer (0-based)
   * @param bit_width Number of bits to write (1-64)
   * @param value Value to write (uses low `bit_width` bits)
   *
   * Ported from: ser_frozen.rs:540-551 (put_primitive, inline write)
   */
  void write_bits(size_t bit_offset, size_t bit_width, uint64_t value);

  /**
   * Write bits at current position and advance
   *
   * Convenience wrapper around write_bits that writes at current position
   * and advances the write position.
   *
   * @param bit_width Number of bits to write (1-64)
   * @param value Value to write (uses low `bit_width` bits)
   */
  void write_bits_advancing(size_t bit_width, uint64_t value);

  /**
   * Align to next byte boundary
   *
   * Advances write position to next multiple of 8 bits.
   */
  void align_to_byte();

  /**
   * Get current write position (in bits)
   */
  size_t position_bits() const { return write_pos_bits_; }

  /**
   * Get current write position (in bytes, rounded up)
   */
  size_t position_bytes() const { return (write_pos_bits_ + 7) / 8; }

  /**
   * Get final buffer
   *
   * Returns the buffer with size trimmed to actual written data.
   * This invalidates the writer.
   */
  std::vector<uint8_t> take_buffer();

  /**
   * Get const reference to buffer
   */
  std::vector<uint8_t> const& buffer() const { return buf_; }

  /**
   * Reserve space in buffer
   *
   * @param bytes Number of bytes to reserve
   */
  void reserve(size_t bytes) { buf_.reserve(bytes); }

  /**
   * Resize buffer to given byte size
   *
   * @param bytes New size in bytes
   */
  void resize(size_t bytes);

 private:
  /**
   * Ensure buffer has capacity for given bit offset
   */
  void ensure_capacity(size_t bit_offset);

  std::vector<uint8_t> buf_;
  size_t write_pos_bits_{0}; // Current write position in bits
};

} // namespace dwarfs::metadata::legacy