/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */

#ifdef DWARFS_HAVE_THRIFT

#include "dwarfs/metadata/serialization/thrift_compact_serializer.h"
#include "dwarfs/metadata/serialization/serializer_registry.h"

#include <stdexcept>

namespace dwarfs::metadata::serialization {

std::vector<uint8_t> ThriftCompactSerializer::serialize(
    const void* metadata) const {

  // Thrift serializer is read-only for legacy compatibility
  throw std::runtime_error(
      "ThriftCompactSerializer does not support write operations. "
      "Use CerealBinarySerializer or BitseryBinarySerializer for new filesystems.");
}

std::unique_ptr<void, void(*)(void*)> ThriftCompactSerializer::deserialize(
    const std::vector<uint8_t>& data) const {

  if (data.size() < MAGIC_BYTES.size()) {
    throw std::invalid_argument("Data too short to contain magic bytes");
  }

  // Verify magic bytes
  for (size_t i = 0; i < MAGIC_BYTES.size(); ++i) {
    if (data[i] != MAGIC_BYTES[i]) {
      throw std::invalid_argument("Invalid magic bytes for Thrift format");
    }
  }

  // TODO: Integration point with existing Thrift frozen deserialization code
  // This will need to:
  // 1. Call existing Thrift deserialization functions
  // 2. Convert Thrift metadata to domain::metadata
  // 3. Return wrapped domain::metadata pointer
  //
  // For now, this is a placeholder that will be integrated with
  // the existing Thrift code in the full implementation.

  throw std::runtime_error(
      "ThriftCompactSerializer deserialization not yet integrated. "
      "This requires connection to existing Thrift frozen code.");
}

// Registration function called by init_serializers()
// Note: Legacy Thrift images don't have magic bytes, so we register with
// an empty magic bytes vector. Detection will fall back to Thrift when
// no other format's magic bytes match.
void register_thrift_serializer() {
  static SerializerRegistration<ThriftCompactSerializer> registration{
    "Thrift Compact",
    {},  // Empty - legacy Thrift has no magic bytes
    50,  // Lower priority than Cereal and Bitsery
    SerializationFormat::THRIFT_COMPACT
  };
}

} // namespace dwarfs::metadata::serialization

#endif // DWARFS_HAVE_THRIFT