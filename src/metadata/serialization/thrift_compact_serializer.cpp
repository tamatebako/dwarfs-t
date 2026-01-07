/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file thrift_compact_serializer.cpp
 *
 * Implementation of Modern Thrift Compact serializer
 *
 * \author Ribose (@riboseinc @tamatebako)
 * \date 2026-01-02
 * \copyright See LICENSE file
 */

#include "dwarfs/metadata/serialization/thrift_compact_serializer.h"
#include "dwarfs/metadata/serialization/serializer_registry.h"
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/modern/domain_to_thrift.h"
#include "dwarfs/metadata/modern/thrift_to_domain.h"

// fbthrift headers
#include <thrift/lib/cpp2/protocol/Serializer.h>
#include <metadata_modern_types.h>  // Generated from thrift/metadata_modern.thrift

#include <stdexcept>
#include <string>

namespace dwarfs::metadata::serialization {

std::vector<uint8_t> ThriftCompactSerializer::serialize(const void* metadata) const {
  if (metadata == nullptr) {
    throw std::invalid_argument("Cannot serialize null metadata");
  }

  auto* domain_meta = static_cast<const domain::metadata*>(metadata);

  // Convert domain → modern thrift using modern converters
  auto thrift_meta = modern::domain_to_thrift(*domain_meta);

  // Serialize with CompactSerializer
  std::string serialized = apache::thrift::CompactSerializer::serialize<std::string>(thrift_meta);

  // Convert to vector and add magic bytes at the beginning
  std::vector<uint8_t> result;
  result.reserve(serialized.size() + 2);

  // CompactProtocol magic bytes
  result.push_back(0x82);
  result.push_back(0x21);

  // Append serialized data
  result.insert(result.end(), serialized.begin(), serialized.end());

  return result;
}

std::unique_ptr<void, void(*)(void*)> ThriftCompactSerializer::deserialize(
    const std::vector<uint8_t>& data) const {

  if (data.size() < 2) {
    throw std::invalid_argument("Data too short for Thrift Compact format");
  }

  // Verify magic bytes
  if (data[0] != 0x82 || data[1] != 0x21) {
    throw std::runtime_error("Invalid Thrift Compact magic bytes");
  }

  // Skip magic bytes for deserialization
  std::string serialized(data.begin() + 2, data.end());

  // Deserialize with CompactSerializer
  dwarfs::thrift::modern::cpp2::Metadata thrift_meta;
  apache::thrift::CompactSerializer::deserialize(serialized, thrift_meta);

  // Convert modern thrift → domain using modern converters
  auto domain_meta = std::make_unique<domain::metadata>(
      modern::thrift_to_domain(thrift_meta));

  // Return with custom deleter
  return std::unique_ptr<void, void(*)(void*)>(
      domain_meta.release(),
      [](void* ptr) { delete static_cast<domain::metadata*>(ptr); }
  );
}

// Registration (called during static initialization)
void register_thrift_compact_serializer() {
  static SerializerRegistration<ThriftCompactSerializer> registration{
    "Modern Thrift Compact",
    {0x82, 0x21},  // CompactProtocol magic bytes
    100,  // Priority: between Legacy Thrift (50) and FlatBuffers (120)
    SerializationFormat::MODERN_THRIFT
  };
}

} // namespace dwarfs::metadata::serialization