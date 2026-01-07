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

#include "dwarfs/metadata/serialization/legacy_thrift_serializer.h"
#include "dwarfs/metadata/serialization/serializer_registry.h"
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/legacy/legacy_metadata_serializer.h"

#include <stdexcept>

namespace dwarfs::metadata::serialization {

std::vector<uint8_t> LegacyThriftSerializer::serialize(
    const void* metadata) const {

  if (metadata == nullptr) {
    throw std::invalid_argument("Cannot serialize null metadata");
  }

  auto* domain_meta = static_cast<const domain::metadata*>(metadata);

  std::vector<uint8_t> output;
  legacy::LegacyMetadataSerializer::serialize(*domain_meta, output);

  return output;
}

std::unique_ptr<void, void(*)(void*)> LegacyThriftSerializer::deserialize(
    const std::vector<uint8_t>& data) const {

  if (data.empty()) {
    throw std::invalid_argument("Cannot deserialize empty data");
  }

  auto domain_meta = std::make_unique<domain::metadata>();

  legacy::LegacyMetadataSerializer::deserialize(data, *domain_meta);

  // Return with custom deleter
  return std::unique_ptr<void, void(*)(void*)>(
      domain_meta.release(),
      [](void* ptr) { delete static_cast<domain::metadata*>(ptr); }
  );
}

// Registration function called by init_serializers()
void register_legacy_thrift_serializer() {
  static SerializerRegistration<LegacyThriftSerializer> registration{
    "Legacy Thrift",
    {},  // No magic bytes (fallback detection)
    50,  // Medium priority (lower than FlatBuffers 120)
    SerializationFormat::LEGACY_THRIFT
  };
}

} // namespace dwarfs::metadata::serialization