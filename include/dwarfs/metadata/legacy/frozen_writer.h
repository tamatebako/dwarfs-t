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
#include <stdexcept>
#include <string>
#include <vector>

namespace dwarfs::metadata::legacy {

/**
 * FrozenWriter - Bit-packing writer for Frozen2 format
 *
 * This class handles writing bit-packed data to a buffer, with support for:
 * - Scalar value encoding with arbitrary bit widths
 * - Storage section management for variable-length data
 * - Finalization to combine compact data and storage sections
 */
class FrozenWriter {
public:
  /**
   * Construct a FrozenWriter
   *
   * @param buffer The output buffer to write to
   * @throws std::invalid_argument if buffer is too small (< 16 bytes)
   */
  explicit FrozenWriter(std::span<uint8_t> buffer);

  /**
   * Write a scalar value at the current bit position
   *
   * @param value The value to write
   * @param bits The number of bits to write (1-64)
   * @throws std::invalid_argument if bits is not in range [1, 64]
   * @throws std::runtime_error if buffer overflow would occur
   */
  void write_scalar(uint64_t value, uint16_t bits);

  /**
   * Reserve space in the storage section
   *
   * @param bytes Number of bytes to reserve
   * @return The offset of the reserved space in the storage section
   */
  uint32_t reserve_storage(size_t bytes);

  /**
   * Write data to the storage section at a specific offset
   *
   * @param offset The offset in the storage section to write to
   * @param data The data to write
   * @throws std::out_of_range if offset + data.size() exceeds storage size
   */
  void write_storage(uint32_t offset, std::span<uint8_t const> data);

  /**
   * Get the current bit position in the compact data section
   *
   * @return The current bit offset
   */
  uint32_t current_bit_offset() const { return bit_offset_; }

  /**
   * Get the size of the storage section
   *
   * @return The storage section size in bytes
   */
  size_t storage_size() const { return storage_section_.size(); }

  /**
   * Seek to a specific bit offset in the compact data section
   *
   * This allows seeking forward to write fields at specific positions.
   * Can only seek forward (to higher offsets).
   *
   * @param bit_offset The bit offset to seek to
   * @throws std::invalid_argument if trying to seek backwards
   */
  void seek_to_bit_offset(uint32_t bit_offset) {
    if (bit_offset < bit_offset_) {
      throw std::invalid_argument(
          "seek_to_bit_offset: cannot seek backwards (current=" +
          std::to_string(bit_offset_) + ", target=" +
          std::to_string(bit_offset) + ")");
    }
    bit_offset_ = bit_offset;
  }

  /**
   * Finalize and combine the compact data and storage sections
   *
   * This appends the storage section after the compact data in the buffer.
   * @throws std::runtime_error if buffer overflow would occur
   */
  void finalize();

private:
  std::span<uint8_t> buffer_;
  std::vector<uint8_t> storage_section_;
  uint32_t bit_offset_ = 0;
};

} // namespace dwarfs::metadata::legacy
