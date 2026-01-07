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

#include "dwarfs/metadata/legacy/frozen_writer.h"
#include "dwarfs/metadata/legacy/frozen_bits.h"

#include <stdexcept>
#include <cstring>

namespace dwarfs::metadata::legacy {

FrozenWriter::FrozenWriter(std::span<uint8_t> buffer)
    : buffer_(buffer) {
  if (buffer_.size() < 16) {
    throw std::invalid_argument("Buffer too small (minimum 16 bytes)");
  }
}

void FrozenWriter::write_scalar(uint64_t value, uint16_t bits) {
  if (bits == 0 || bits > 64) {
    throw std::invalid_argument(
        "write_scalar: bits must be in range [1, 64]");
  }

  // Check buffer bounds
  uint32_t required_bytes = frozen_bits::bytes_for_range(bit_offset_, bits);
  if (required_bytes > buffer_.size()) {
    throw std::runtime_error(
        "write_scalar: buffer overflow");
  }

  frozen_bits::store_bits(buffer_, bit_offset_, bits, value);
  bit_offset_ += bits;
}

uint32_t FrozenWriter::reserve_storage(size_t bytes) {
  uint32_t offset = static_cast<uint32_t>(storage_section_.size());
  storage_section_.resize(offset + bytes, 0);
  return offset;
}

void FrozenWriter::write_storage(uint32_t offset, std::span<uint8_t const> data) {
  if (offset + data.size() > storage_section_.size()) {
    throw std::out_of_range(
        "write_storage: offset out of range");
  }

  std::memcpy(storage_section_.data() + offset, data.data(), data.size());
}

void FrozenWriter::finalize() {
  size_t data_bytes = (bit_offset_ + 7) / 8;

  if (data_bytes + storage_section_.size() > buffer_.size()) {
    throw std::runtime_error(
        "finalize: buffer overflow during finalization");
  }

  if (!storage_section_.empty()) {
    std::memcpy(buffer_.data() + data_bytes,
                storage_section_.data(),
                storage_section_.size());
  }
}

} // namespace dwarfs::metadata::legacy
