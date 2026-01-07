/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \author     Ribose Inc.
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

#include <span>
#include <memory>
#include <vector>
#include <string>

#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/legacy/frozen_schema.h"

namespace dwarfs::metadata::legacy {

/**
 * Frozen2 Deserializer - Reads bit-packed metadata using schema
 *
 * This class deserializes DwarFS metadata from the Frozen2 format,
 * which is a bit-packed representation used by DwarFS v0.14.1 (Homebrew)
 * and earlier versions.
 *
 * Port of: dwarfs-rs/dwarfs/src/metadata/de_frozen.rs
 *
 * Architecture:
 * ┌──────────────────────────────────────────────────────────┐
 * │ DwarFS Image (Legacy Thrift Format)                      │
 * ├──────────────────────────────────────────────────────────┤
 * │  1. Schema Section (Thrift Compact Protocol)             │
 * │     - Describes bit layout of metadata                   │
 * │     - Field offsets, sizes, types                        │
 * │                                                           │
 * │  2. Metadata Section (Frozen2 Bit-Packed)                │
 * │     - Actual metadata values                             │
 * │     - Bit-packed according to schema                     │
 * └──────────────────────────────────────────────────────────┘
 *                       ↓
 *          ┌────────────────────────┐
 *          │ Frozen2Deserializer    │
 *          │  - Takes: Schema       │
 *          │  - Takes: Frozen bytes │
 *          │  - Returns: Metadata   │
 *          └────────────────────────┘
 *
 * Bit Layout Example:
 *   If schema says field "block_size" is at:
 *     - Offset: 0 bits
 *     - Size: 18 bits
 *
 *   Then deserializer:
 *     1. Reads bits [0..18) from frozen bytes
 *     2. Interprets as uint32_t
 *     3. Assigns to metadata.block_size
 *
 * Usage:
 *   auto schema = FrozenSchemaSerializer::deserialize(schema_bytes);
 *   auto metadata = Frozen2Deserializer::deserialize(schema, frozen_bytes);
 *
 * Thread Safety:
 *   - Safe for concurrent reads (immutable after construction)
 *   - Not safe for concurrent writes
 *
 * Performance:
 *   - O(n) where n = number of metadata fields
 *   - Bit operations use optimized frozen_bits helpers
 *   - No dynamic allocation during field reading
 */
class Frozen2Deserializer {
public:
  /**
   * Deserialize Frozen2 format to domain::metadata
   *
   * @param schema Parsed schema describing bit layout
   * @param data Frozen bytes containing bit-packed metadata
   * @return Deserialized metadata object
   * @throws std::runtime_error if validation or parsing fails
   *
   * Port from: de_frozen.rs:38-47 (deserialize function)
   *
   * Example:
   *   Schema schema = FrozenSchemaSerializer::deserialize(schema_bytes);
   *   std::vector<uint8_t> frozen = read_frozen_bytes();
   *
   *   auto meta = Frozen2Deserializer::deserialize(schema, frozen);
   *
   *   // meta.block_size, meta.chunks, etc. are now populated
   */
  static domain::metadata deserialize(
      Schema const& schema,
      std::span<uint8_t const> data);

private:
  /**
   * Internal reader class implementing serde-like deserialization
   *
   * Maintains state during deserialization:
   *   - Current schema layout
   *   - Current bit offset in frozen data
   *   - Storage start position
   *
   * Port from: de_frozen.rs:114-143 (Deserializer struct)
   */
  class Reader;
};

} // namespace dwarfs::metadata::legacy
