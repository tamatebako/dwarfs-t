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

#include "dwarfs/metadata/legacy/value_encoders.h"
#include "dwarfs/metadata/legacy/frozen_writer.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#include <stdexcept>
#include <span>

namespace dwarfs::metadata::legacy {

uint32_t ScalarEncoder::encode(
  FrozenWriter& writer,
  SchemaLayout const& layout,
  void const* value) const {

  // Validate: null pointer check
  if (!value) {
    throw std::invalid_argument("ScalarEncoder::encode: value pointer is null");
  }

  uint16_t bits = layout.bits;

  // Validate: bits must be 1-64
  if (bits == 0 || bits > 64) {
    throw std::invalid_argument(
        fmt::format("ScalarEncoder::encode: bits must be 1-64, got {}", bits));
  }

  uint64_t u64_value = 0;

  // Use memcpy for safe type punning (avoids undefined behavior from
  // strict aliasing violations). This is the recommended way in C++ for
  // type casting between unrelated types.
  //
  // Endianness: The Frozen2 format uses little-endian byte order, which
  // matches x86/x86_64 and most modern systems. No byte swapping needed.
  if (bits == 1) {
    // Boolean special case: read as uint8_t (true=1, false=0)
    uint8_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint8_t));
    u64_value = temp ? 1 : 0;
  } else if (bits <= 8) {
    uint8_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint8_t));
    u64_value = temp;
  } else if (bits <= 16) {
    uint16_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint16_t));
    u64_value = temp;
  } else if (bits <= 32) {
    uint32_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint32_t));
    u64_value = temp;
  } else {
    uint64_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint64_t));
    u64_value = temp;
  }

  writer.write_scalar(u64_value, bits);
  return bits;
}

} // namespace dwarfs::metadata::legacy
