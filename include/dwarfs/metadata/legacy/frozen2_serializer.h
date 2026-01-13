/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     DwarFS Implementation
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
#include <memory>

namespace dwarfs::metadata::domain {
  struct metadata;
}

namespace dwarfs::metadata::legacy {

/**
 * Frozen2Serializer - Main orchestrator for Frozen2 serialization
 *
 * This is the simplified API for serializing domain::metadata to the
 * Homebrew-compatible Frozen2 format. It orchestrates:
 * 1. SchemaBuilder - generates schema from metadata
 * 2. FrozenSchemaSerializer - encodes schema to Thrift Compact Protocol
 * 3. (Future Task 6) Metadata encoding - encodes values to bit-packed format
 *
 * Output format:
 *   [8 bytes]  Size prefix (little-endian uint64)
 *   [N bytes]  Schema section (Thrift Compact Protocol)
 *   [M bytes]  Frozen metadata data (bit-packed, Task 6)
 *
 * Task 5: Implement main orchestrator
 */
class Frozen2Serializer {
public:
  /**
   * Serialize metadata to Frozen2 format
   *
   * @param metadata Pointer to domain metadata (void* for registry compatibility)
   * @return Serialized bytes with size prefix
   *
   * Format:
   *   [8 bytes]  Size prefix (little-endian uint64, size of data after prefix)
   *   [N bytes]  Schema section (Thrift Compact Protocol)
   *   [M bytes]  Frozen metadata data (bit-packed, Task 6 - currently empty)
   */
  std::vector<uint8_t> serialize(const void* metadata) const;
};

} // namespace dwarfs::metadata::legacy