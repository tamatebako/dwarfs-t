/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>
#include <vector>
#include <map>
#include <optional>

#include "metadata_serializer.h"
#include "serialization_format.h"

namespace dwarfs::metadata::serialization {

/**
 * Registry for metadata serializers (Singleton)
 *
 * This registry manages all available metadata serializers and provides:
 * - Auto-registration of serializers at static initialization time
 * - Format detection from magic bytes
 * - Serializer creation by format
 * - Query of available formats
 *
 * Usage:
 *   auto& registry = SerializerRegistry::instance();
 *   auto format = registry.detect_format(data);
 *   auto serializer = registry.create_serializer(format);
 */
class SerializerRegistry {
public:
  // Get singleton instance
  static SerializerRegistry& instance();

  // Register a serializer
  // Called automatically during static initialization by serializer implementations
  void register_serializer(
      std::string_view name,
      std::function<std::unique_ptr<IMetadataSerializer>()> creator,
      std::vector<uint8_t> magic_bytes,
      int priority,
      SerializationFormat format);

  // Create serializer for specific format
  std::unique_ptr<IMetadataSerializer> create_serializer(
      SerializationFormat format) const;

  // Detect format from data by examining magic bytes
  std::optional<SerializationFormat> detect_format(
      const std::vector<uint8_t>& data) const;

  // Check if format is available
  bool is_format_available(SerializationFormat format) const;

  // Get list of available formats
  std::vector<SerializationFormat> get_available_formats() const;

  // Get format name
  std::string_view get_format_name(SerializationFormat format) const;

private:
  SerializerRegistry() = default;
  ~SerializerRegistry() = default;

  // Prevent copying
  SerializerRegistry(const SerializerRegistry&) = delete;
  SerializerRegistry& operator=(const SerializerRegistry&) = delete;

  // Serializer entry
  struct SerializerEntry {
    std::string name;
    std::function<std::unique_ptr<IMetadataSerializer>()> creator;
    std::vector<uint8_t> magic_bytes;
    int priority;
    SerializationFormat format;
  };

  // Registered serializers by format
  std::map<SerializationFormat, SerializerEntry> serializers_;
};

/**
 * Helper class for auto-registration
 *
 * Serializer implementations create a static instance of this class
 * to automatically register themselves at program startup.
 *
 * Example:
 *   static SerializerRegistration<CerealBinarySerializer> registration{
 *     "Cereal Binary",
 *     {0xCE, 0xEA, 0x01},
 *     100,
 *     SerializationFormat::CEREAL_BINARY
 *   };
 */
template<typename SerializerType>
class SerializerRegistration {
public:
  SerializerRegistration(
      std::string_view name,
      std::vector<uint8_t> magic_bytes,
      int priority,
      SerializationFormat format) {
    SerializerRegistry::instance().register_serializer(
        name,
        []() { return std::make_unique<SerializerType>(); },
        std::move(magic_bytes),
        priority,
        format);
  }
};

} // namespace dwarfs::metadata::serialization