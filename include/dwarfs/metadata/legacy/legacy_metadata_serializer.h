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

#include <span>
#include <vector>

namespace dwarfs::metadata::domain {
class metadata;
}

namespace dwarfs::metadata::legacy {

/**
 * Legacy Thrift metadata serializer
 *
 * Port of dwarfs-rs's metadata serialization using hand-coded Thrift Compact.
 * Provides Homebrew v0.14.1 compatibility without fbthrift dependency.
 *
 * Reference: dwarfs-rs/dwarfs/src/metadata/metadata.rs
 */
class LegacyMetadataSerializer {
public:
  /**
   * Serialize domain::metadata to Thrift Compact format
   *
   * @param meta Metadata to serialize
   * @param output Output buffer (will be cleared and written to)
   */
  static void serialize(domain::metadata const& meta,
                        std::vector<uint8_t>& output);

  /**
   * Deserialize Thrift Compact format to domain::metadata
   *
   * @param data Input data span
   * @param meta Output metadata (will be populated)
   */
  static void deserialize(std::span<uint8_t const> data,
                          domain::metadata& meta);
};

} // namespace dwarfs::metadata::legacy