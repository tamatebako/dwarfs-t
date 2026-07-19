/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */

#include "dwarfs/metadata/serialization/serializer_registry.h"

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace dwarfs::metadata::serialization {

SerializerRegistry& SerializerRegistry::instance() {
  static SerializerRegistry registry;
  return registry;
}

void SerializerRegistry::register_serializer(
    std::string_view name,
    std::function<std::unique_ptr<IMetadataSerializer>()> creator,
    std::vector<uint8_t> magic_bytes,
    int priority,
    SerializationFormat format) {

  SerializerEntry entry{
    std::string(name),
    std::move(creator),
    std::move(magic_bytes),
    priority,
    format
  };

  serializers_[format] = std::move(entry);
}

std::unique_ptr<IMetadataSerializer> SerializerRegistry::create_serializer(
    SerializationFormat format) const {

  auto it = serializers_.find(format);
  if (it == serializers_.end()) {
    return nullptr;
  }

  return it->second.creator();
}

std::optional<SerializationFormat> SerializerRegistry::detect_format(
    const std::vector<uint8_t>& data) const {

  if (data.empty()) {
    return std::nullopt;
  }

  // Check each registered serializer's magic bytes
  // Sort by priority (higher first) for deterministic detection
  std::vector<std::pair<SerializationFormat, const SerializerEntry*>> sorted_serializers;
  for (const auto& [format, entry] : serializers_) {
    sorted_serializers.emplace_back(format, &entry);
  }

  std::sort(sorted_serializers.begin(), sorted_serializers.end(),
      [](const auto& a, const auto& b) {
        return a.second->priority > b.second->priority;
      });

  // Check magic bytes for each serializer
  for (const auto& [format, entry] : sorted_serializers) {
    const auto& magic = entry->magic_bytes;

    // Skip serializers with no magic bytes
    if (magic.empty()) {
      continue;
    }

    if (data.size() < magic.size()) {
      continue;
    }

    // Check at offset 0 (non-size-prefixed formats)
    bool matches = true;
    for (size_t i = 0; i < magic.size(); ++i) {
      if (data[i] != magic[i]) {
        matches = false;
        break;
      }
    }

    if (matches) {
      return format;
    }

    // Also check at offset 8 (FlatBuffers size-prefixed: [size][offset][file-id])
    if (data.size() >= magic.size() + 8) {
      matches = true;
      for (size_t i = 0; i < magic.size(); ++i) {
        if (data[i + 8] != magic[i]) {
          matches = false;
          break;
        }
      }

      if (matches) {
        return format;
      }
    }
  }

  // No magic bytes found - check if this could be legacy Thrift format
  // Legacy Thrift images don't have magic bytes, but they should have
  // a reasonable minimum size to be valid metadata
  if (data.size() >= 16 && is_format_available(SerializationFormat::LEGACY_THRIFT)) {
    return SerializationFormat::LEGACY_THRIFT;
  }

  // If data is too small or Legacy Thrift support is not available,
  // we can't read this image
  return std::nullopt;
}

bool SerializerRegistry::is_format_available(SerializationFormat format) const {
  return serializers_.find(format) != serializers_.end();
}

std::vector<SerializationFormat> SerializerRegistry::get_available_formats() const {
  std::vector<SerializationFormat> formats;
  formats.reserve(serializers_.size());

  for (const auto& [format, _] : serializers_) {
    formats.push_back(format);
  }

  return formats;
}

std::string_view SerializerRegistry::get_format_name(SerializationFormat format) const {
  auto it = serializers_.find(format);
  if (it != serializers_.end()) {
    return it->second.name;
  }
  return get_format_name(format); // Use the constexpr function from serialization_format.h
}

} // namespace dwarfs::metadata::serialization