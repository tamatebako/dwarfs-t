/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace dwarfs::metadata {
  namespace domain { class metadata; }
  namespace converters { class IMetadataConverter; }
  namespace serialization {
    class IMetadataSerializer;
    enum class SerializationFormat;
  }
}

namespace dwarfs::thrift::metadata {
  class metadata;
}

namespace dwarfs::metadata::serialization {

/**
 * Facade that orchestrates conversion and serialization.
 *
 * Single Responsibility: Coordinate converter + serializer
 * Provides simple API for Writer (Thrift → bytes) and Reader (bytes → Thrift)
 *
 * Design Pattern: Facade Pattern
 * - Hides complexity of converter + serializer coordination
 * - Provides simple, high-level interface
 * - Delegates to specialized components
 *
 * This facade orchestrates two key components:
 * 1. IMetadataConverter: Adapts between Thrift and domain models
 * 2. IMetadataSerializer: Encodes/decodes domain models to binary
 *
 * Usage Example:
 *   auto facade = FacadeFactory::create(SerializationFormat::FLATBUFFERS);
 *   auto bytes = facade->serialize_from_thrift(thrift_meta);
 *   auto thrift = facade->deserialize_to_thrift(bytes);
 */
class MetadataSerializationFacade {
public:
  /**
   * Construct facade with specific converter and serializer.
   *
   * @param converter Converter for format adaptation (Thrift ↔ Domain)
   * @param serializer Serializer for binary encoding (Domain ↔ Bytes)
   * @throws std::invalid_argument if converter or serializer is null
   */
  MetadataSerializationFacade(
      std::unique_ptr<converters::IMetadataConverter> converter,
      std::unique_ptr<IMetadataSerializer> serializer);

  ~MetadataSerializationFacade();

  // Prevent copying (manage unique resources)
  MetadataSerializationFacade(const MetadataSerializationFacade&) = delete;
  MetadataSerializationFacade& operator=(const MetadataSerializationFacade&) = delete;

  // Allow moving
  MetadataSerializationFacade(MetadataSerializationFacade&&) noexcept;
  MetadataSerializationFacade& operator=(MetadataSerializationFacade&&) noexcept;

  /**
   * Simple API for Writer: Thrift model → Serialized bytes
   *
   * This orchestrates:
   * 1. Thrift → Domain (via converter)
   * 2. Domain → Bytes (via serializer)
   *
   * @param thrift_meta Thrift metadata to serialize
   * @return Binary data with format-specific magic bytes
   * @throws std::runtime_error if serialization fails
   */
  std::vector<uint8_t> serialize_from_thrift(
      const ::dwarfs::thrift::metadata::metadata& thrift_meta) const;

  /**
   * Direct API for Writer: Domain model → Serialized bytes
   *
   * This uses the serializer directly without conversion.
   * Preferred method for new code using domain model.
   *
   * @param domain_meta Domain metadata to serialize
   * @return Binary data with format-specific magic bytes
   * @throws std::runtime_error if serialization fails
   */
  std::vector<uint8_t> serialize(
      const ::dwarfs::metadata::domain::metadata& domain_meta) const;

  /**
   * Simple API for Reader: Serialized bytes → Thrift model
   *
   * This orchestrates:
   * 1. Bytes → Domain (via serializer)
   * 2. Domain → Thrift (via converter)
   *
   * @param data Binary data with format-specific magic bytes
   * @return Deserialized Thrift metadata
   * @throws std::runtime_error if deserialization fails
   */
  std::unique_ptr<::dwarfs::thrift::metadata::metadata> deserialize_to_thrift(
      const std::vector<uint8_t>& data) const;

  /**
   * Direct API for Reader: Serialized bytes → Domain model
   *
   * This uses the serializer directly without conversion.
   * Preferred method for new code using domain model.
   *
   * @param data Binary data with format-specific magic bytes
   * @return Deserialized domain metadata
   * @throws std::runtime_error if deserialization fails
   */
  std::unique_ptr<::dwarfs::metadata::domain::metadata> deserialize(
      const std::vector<uint8_t>& data) const;

  /**
   * Get the serialization format used by this facade
   * @return Format identifier (e.g. FLATBUFFERS, THRIFT_COMPACT)
   */
  SerializationFormat get_format() const;

  /**
   * Get human-readable format name
   * @return Format name (e.g. "FlatBuffers", "Thrift Compact")
   */
  std::string get_format_name() const;

  /**
   * Check if this facade can read (deserialize) metadata
   * @return true if reading is supported
   */
  bool can_read() const;

  /**
   * Check if this facade can write (serialize) metadata
   * @return true if writing is supported
   */
  bool can_write() const;

private:
  std::unique_ptr<converters::IMetadataConverter> converter_;
  std::unique_ptr<IMetadataSerializer> serializer_;
};

} // namespace dwarfs::metadata::serialization