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

#include "dwarfs/metadata/serialization/metadata_serializer.h"
#include "dwarfs/metadata/serialization/serialization_format.h"

#include <memory>
#include <string_view>
#include <vector>

namespace dwarfs::metadata::serialization {

/**
 * Legacy Thrift metadata serializer
 *
 * Hand-coded Thrift Compact implementation for Homebrew v0.14.1 compatibility.
 * No fbthrift dependency required.
 *
 * Format characteristics:
 * - Wire format: Thrift Compact Protocol
 * - Magic bytes: None (legacy format detection via fallback)
 * - Compatibility: Homebrew dwarfs v0.14.1
 * - Priority: 50 (lower than FlatBuffers 120, but usable as fallback)
 */
class LegacyThriftSerializer : public IMetadataSerializer {
public:
  LegacyThriftSerializer() = default;
  ~LegacyThriftSerializer() override = default;

  // Serialize domain::metadata to Legacy Thrift format
  std::vector<uint8_t> serialize(const void* metadata) const override;

  // Deserialize Legacy Thrift to domain::metadata
  std::unique_ptr<void, void(*)(void*)> deserialize(
      const std::vector<uint8_t>& data) const override;

  // Format identification
  std::string_view get_format_name() const noexcept override {
    return "Legacy Thrift";
  }

  SerializationFormat get_format() const noexcept override {
    return SerializationFormat::LEGACY_THRIFT;
  }

  // Capabilities
  bool can_write() const noexcept override { return true; }
  bool can_read() const noexcept override { return true; }

  // Magic bytes (empty = no magic, fallback detection)
  std::vector<uint8_t> get_magic_bytes() const noexcept override {
    return {}; // Legacy Thrift has no magic bytes
  }
};

// Registration function (called by init_serializers)
void register_legacy_thrift_serializer();

} // namespace dwarfs::metadata::serialization