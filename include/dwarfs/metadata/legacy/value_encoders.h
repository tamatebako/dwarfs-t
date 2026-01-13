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
#include <memory>
#include <span>

#include "dwarfs/metadata/legacy/frozen_schema.h"

namespace dwarfs::metadata::legacy {

// Forward declaration
class FrozenWriter;

/**
 * Base class for value encoders
 *
 * Value encoders are responsible for encoding values according to a schema
 * layout. This is part of the Frozen2 serialization implementation.
 */
class ValueEncoder {
public:
  virtual ~ValueEncoder() = default;

  /**
   * Encode a value to the FrozenWriter
   *
   * @param writer The writer to encode to
   * @param layout The schema layout describing how to encode
   * @param value Pointer to the value to encode
   * @return Number of bits written
   */
  virtual uint32_t encode(
    FrozenWriter& writer,
    SchemaLayout const& layout,
    void const* value) const = 0;
};

/**
 * Scalar encoder for primitive types
 *
 * Handles encoding of primitive scalar values (integers, booleans) to
 * bit-packed format.
 */
class ScalarEncoder : public ValueEncoder {
public:
  uint32_t encode(
    FrozenWriter& writer,
    SchemaLayout const& layout,
    void const* value) const override;
};

/**
 * Vector encoder for sequence types
 *
 * Handles encoding of std::vector<T> sequences. Vectors are encoded with:
 * - Field 1: distance (offset to element data in storage section)
 * - Field 2: length (number of elements)
 * - Element data stored in storage section
 */
class VectorEncoder : public ValueEncoder {
public:
  uint32_t encode(
    FrozenWriter& writer,
    SchemaLayout const& layout,
    void const* value) const override;
};

} // namespace dwarfs::metadata::legacy
