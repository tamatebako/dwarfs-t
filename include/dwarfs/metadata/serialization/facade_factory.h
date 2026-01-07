/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <cstdint>

namespace dwarfs::metadata::serialization {
  class MetadataSerializationFacade;
  enum class SerializationFormat;
}

namespace dwarfs::metadata::serialization {

/**
 * Factory for creating configured serialization facades.
 *
 * Single Responsibility: Facade creation and configuration
 * Provides auto-detection and manual creation
 *
 * Design Pattern: Factory Pattern
 * - Encapsulates complex object creation
 * - Auto-configures facade with correct converter + serializer
 * - Supports auto-detection from binary data
 *
 * This factory coordinates:
 * 1. Creating the appropriate converter (e.g., ThriftMetadataConverter)
 * 2. Creating the appropriate serializer via SerializerRegistry
 * 3. Combining them into a ready-to-use facade
 *
 * Usage Examples:
 *   // Create facade for specific format
 *   auto facade = FacadeFactory::create(SerializationFormat::FLATBUFFERS);
 *
 *   // Auto-detect format from data
 *   auto facade = FacadeFactory::create_from_data(binary_data);
 *
 *   // Just detect format (no facade creation)
 *   auto format = FacadeFactory::detect_format(binary_data);
 */
class FacadeFactory {
public:
  /**
   * Create facade for specific format.
   *
   * This creates and configures:
   * - A ThriftMetadataConverter (for Thrift ↔ Domain conversion)
   * - A format-specific serializer (from SerializerRegistry)
   * - A MetadataSerializationFacade combining both
   *
   * @param format Desired serialization format
   * @return Configured facade ready to use
   * @throws std::runtime_error if format is not available
   */
  static std::unique_ptr<MetadataSerializationFacade> create(
      SerializationFormat format);

  /**
   * Create facade by auto-detecting format from data.
   *
   * This:
   * 1. Examines magic bytes in data
   * 2. Detects the serialization format
   * 3. Creates facade for detected format
   *
   * @param data Serialized metadata bytes
   * @return Configured facade for detected format, or nullptr if unknown
   */
  static std::unique_ptr<MetadataSerializationFacade> create_from_data(
      const std::vector<uint8_t>& data);

  /**
   * Detect format from magic bytes (without creating facade).
   *
   * Useful for:
   * - Format validation
   * - Format reporting
   * - Pre-flight checks
   *
   * @param data Serialized metadata bytes
   * @return Detected format or nullopt if unknown
   */
  static std::optional<SerializationFormat> detect_format(
      const std::vector<uint8_t>& data);
};

} // namespace dwarfs::metadata::serialization