/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhx.io)
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

namespace dwarfs::metadata::legacy {

/**
 * Thrift Compact protocol type tags
 *
 * These correspond to the wire format type identifiers used in Thrift Compact.
 * Reference: dwarfs-rs/dwarfs/src/metadata/de_thrift.rs:48-60
 */
enum class Tag : uint8_t {
  UNKNOWN_BOOL = 0, // For non-inline bool (separate byte)
  BOOL_TRUE = 1,    // Inline true in field header
  BOOL_FALSE = 2,   // Inline false in field header
  I16 = 4,          // 16-bit signed integer (zigzag encoded)
  I32 = 5,          // 32-bit signed integer (zigzag encoded)
  I64 = 6,          // 64-bit signed integer (zigzag encoded)
  BINARY = 8,       // String/binary data (varint length + bytes)
  LIST = 9,         // List container
  MAP = 11,         // Map container
  STRUCT = 12,      // Struct container
  INVALID = 15,     // Invalid/unknown tag
};

/**
 * Convert tag to human-readable name (for debugging/logging)
 */
const char* tag_name(Tag t);

} // namespace dwarfs::metadata::legacy