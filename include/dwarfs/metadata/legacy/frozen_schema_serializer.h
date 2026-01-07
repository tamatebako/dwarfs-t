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

#include <span>
#include <vector>

#include "dwarfs/metadata/legacy/frozen_schema.h"

namespace dwarfs::metadata::legacy {

/**
 * Frozen2 schema serializer/deserializer
 *
 * Converts Schema to/from Thrift CompactProtocol format.
 * This is used to serialize the METADATA_V2_SCHEMA section in DwarFS images.
 *
 * Ported from:
 * - dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs (serialization)
 * - dwarfs-rs/dwarfs/src/metadata/de_thrift.rs (deserialization)
 */
class FrozenSchemaSerializer {
 public:
  /**
   * Serialize schema to Thrift CompactProtocol format
   *
   * @param schema Schema to serialize
   * @return Serialized bytes
   *
   * Ported from: metadata.rs:233-236 (to_bytes)
   */
  static std::vector<uint8_t> serialize(Schema const& schema);

  /**
   * Deserialize schema from Thrift CompactProtocol format
   *
   * @param data Serialized schema bytes
   * @return Parsed schema
   * @throws std::runtime_error on parse failure or validation failure
   *
   * Ported from: metadata.rs:207-212 (parse)
   */
  static Schema deserialize(std::span<uint8_t const> data);

 private:
  class Writer;
  class Reader;
};

} // namespace dwarfs::metadata::legacy