/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#include "dwarfs/metadata/serialization/facade_factory.h"
#include "dwarfs/metadata/serialization/serialization_facade.h"
#include "dwarfs/metadata/serialization/serializer_registry.h"
#include "dwarfs/metadata/serialization/serialization_format.h"
#include "dwarfs/metadata/converters/metadata_converter.h"

#ifdef DWARFS_HAVE_THRIFT
#include "dwarfs/metadata/converters/thrift_metadata_converter.h"
#endif

#include <stdexcept>

namespace dwarfs::metadata::serialization {

std::unique_ptr<MetadataSerializationFacade>
FacadeFactory::create(SerializationFormat format) {

  // Create converter (only needed for Thrift compatibility)
  std::unique_ptr<converters::IMetadataConverter> converter;

#ifdef DWARFS_HAVE_THRIFT
  // Thrift converter (for Thrift↔Domain conversion)
  converter = std::make_unique<converters::ThriftMetadataConverter>();
#else
  // FlatBuffers works directly with domain model - no converter needed
  converter = nullptr;
#endif

  // Create serializer from registry
  auto& registry = SerializerRegistry::instance();
  auto serializer = registry.create_serializer(format);

  if (!serializer) {
    throw std::runtime_error(
        std::string("Failed to create serializer for format: ") +
        std::string(get_format_name(format)));
  }

  // Combine into facade
  return std::make_unique<MetadataSerializationFacade>(
      std::move(converter),
      std::move(serializer));
}

std::unique_ptr<MetadataSerializationFacade>
FacadeFactory::create_from_data(const std::vector<uint8_t>& data) {

  auto format = detect_format(data);
  if (!format) {
    return nullptr;
  }

  return create(*format);
}

std::optional<SerializationFormat>
FacadeFactory::detect_format(const std::vector<uint8_t>& data) {

  auto& registry = SerializerRegistry::instance();
  return registry.detect_format(data);
}

} // namespace dwarfs::metadata::serialization