/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <string_view>

#include "serialization_format.h"

namespace dwarfs::metadata {

// Forward declarations
namespace thrift::metadata {
  class metadata;
}

namespace domain {
  class metadata;
}

namespace serialization {

/**
 * Strategy interface for metadata serialization
 *
 * This interface defines the contract for serializing and deserializing
 * DwarFS metadata in different formats (Thrift, Cereal, Bitsery).
 *
 * Implementations should:
 * - Handle their specific format's magic bytes
 * - Provide efficient serialization/deserialization
 * - Auto-register with SerializerRegistry
 * - Support format detection via magic bytes
 */
class IMetadataSerializer {
public:
  virtual ~IMetadataSerializer() = default;

  // Serialize metadata to binary format
  // Returns binary data with format-specific magic bytes prepended
  virtual std::vector<uint8_t> serialize(
      const void* metadata) const = 0;

  // Deserialize metadata from binary format
  // Returns unique pointer to deserialized metadata
  virtual std::unique_ptr<void, void(*)(void*)> deserialize(
      const std::vector<uint8_t>& data) const = 0;

  // Get human-readable format name
  virtual std::string_view get_format_name() const noexcept = 0;

  // Get serialization format identifier
  virtual SerializationFormat get_format() const noexcept = 0;

  // Check if this serializer can write metadata
  virtual bool can_write() const noexcept = 0;

  // Check if this serializer can read metadata
  virtual bool can_read() const noexcept = 0;

  // Get magic bytes for this format
  virtual std::vector<uint8_t> get_magic_bytes() const noexcept = 0;
};

} // namespace serialization
} // namespace dwarfs::metadata