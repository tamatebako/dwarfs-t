/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#include "dwarfs/metadata/serialization/serialization_facade.h"
#include "dwarfs/metadata/converters/metadata_converter.h"
#include "dwarfs/metadata/serialization/metadata_serializer.h"
#include "dwarfs/metadata/serialization/serialization_format.h"
#include "dwarfs/metadata/domain/metadata.h"

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
#include <dwarfs/gen-cpp2/metadata_types.h>
#endif

#include <stdexcept>

namespace dwarfs::metadata::serialization {

MetadataSerializationFacade::MetadataSerializationFacade(
    std::unique_ptr<converters::IMetadataConverter> converter,
    std::unique_ptr<IMetadataSerializer> serializer)
    : converter_(std::move(converter))
    , serializer_(std::move(serializer)) {

  // Converter is optional (nullptr for FlatBuffers-only builds)
  // Only required for Thrift compatibility methods

  if (!serializer_) {
    throw std::invalid_argument("Serializer cannot be null");
  }
}

MetadataSerializationFacade::~MetadataSerializationFacade() = default;

MetadataSerializationFacade::MetadataSerializationFacade(
    MetadataSerializationFacade&&) noexcept = default;

MetadataSerializationFacade& MetadataSerializationFacade::operator=(
    MetadataSerializationFacade&&) noexcept = default;

#ifdef DWARFS_HAVE_EXPERIMENTAL_THRIFT
// Thrift-specific methods (only when Thrift available)

// Writer API: Thrift → Bytes
std::vector<uint8_t> MetadataSerializationFacade::serialize_from_thrift(
    const ::dwarfs::thrift::metadata::metadata& thrift_meta) const {

  if (!converter_) {
    throw std::runtime_error("Thrift converter not available");
  }

  // Step 1: Convert Thrift → Domain
  auto domain_meta = converter_->to_domain(&thrift_meta);

  // Step 2: Serialize Domain → Bytes
  return serializer_->serialize(&domain_meta);
}

// Reader API: Bytes → Thrift
std::unique_ptr<::dwarfs::thrift::metadata::metadata>
MetadataSerializationFacade::deserialize_to_thrift(
    const std::vector<uint8_t>& data) const {

  if (!converter_) {
    throw std::runtime_error("Thrift converter not available");
  }

  // Step 1: Deserialize Bytes → Domain
  auto domain_meta_ptr = serializer_->deserialize(data);
  auto* domain_meta = static_cast<domain::metadata*>(domain_meta_ptr.get());

  if (!domain_meta) {
    throw std::runtime_error("Deserialization produced null metadata");
  }

  // Step 2: Convert Domain → Thrift
  auto thrift_meta_ptr = converter_->from_domain(*domain_meta);

  // Cast to concrete Thrift type
  return std::unique_ptr<::dwarfs::thrift::metadata::metadata>(
      static_cast<::dwarfs::thrift::metadata::metadata*>(thrift_meta_ptr.release()));
}

#endif // DWARFS_HAVE_EXPERIMENTAL_THRIFT

// NEW: Direct domain model serialization (preferred method, always available)
std::vector<uint8_t> MetadataSerializationFacade::serialize(
    const ::dwarfs::metadata::domain::metadata& domain_meta) const {

  // Serialize Domain → Bytes directly (no conversion needed)
  return serializer_->serialize(&domain_meta);
}

// NEW: Direct domain model deserialization (preferred method, always available)
std::unique_ptr<::dwarfs::metadata::domain::metadata>
MetadataSerializationFacade::deserialize(
    const std::vector<uint8_t>& data) const {

  // Deserialize Bytes → Domain directly (no conversion needed)
  auto domain_meta_ptr = serializer_->deserialize(data);

  // Cast to concrete domain type
  return std::unique_ptr<::dwarfs::metadata::domain::metadata>(
      static_cast<::dwarfs::metadata::domain::metadata*>(domain_meta_ptr.release()));
}

SerializationFormat MetadataSerializationFacade::get_format() const {
  return serializer_->get_format();
}

std::string MetadataSerializationFacade::get_format_name() const {
  return std::string(serializer_->get_format_name());
}

bool MetadataSerializationFacade::can_read() const {
  return serializer_->can_read();
}

bool MetadataSerializationFacade::can_write() const {
  return serializer_->can_write();
}

} // namespace dwarfs::metadata::serialization