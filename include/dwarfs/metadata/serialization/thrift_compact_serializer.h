/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file thrift_compact_serializer.h
 *
 * Modern Thrift Compact serializer using fbthrift v2025.12.29.00+
 *
 * This serializer uses apache::thrift::CompactSerializer for optimal size,
 * providing the smallest possible metadata format. It requires the full
 * Facebook stack (folly, fizz, mvfst, wangle, fbthrift).
 *
 * Wire format: [2-byte CompactProtocol magic][compact protocol data]
 * Magic bytes: {0x82, 0x21}
 * Priority: 100 (between Legacy Thrift 50 and FlatBuffers 120)
 *
 * \author Ribose (@riboseinc @tamatebako)
 * \date 2026-01-02
 * \copyright See LICENSE file
 */

#pragma once

#include "metadata_serializer.h"
#include "serialization_format.h"

#include <memory>
#include <vector>
#include <cstdint>
#include <string_view>

namespace dwarfs::metadata::serialization {

/**
 * Modern Thrift Compact serializer
 *
 * Uses apache::thrift::CompactSerializer from fbthrift for metadata
 * serialization. Provides the smallest possible format but requires
 * the full Facebook dependency stack.
 *
 * Features:
 * - Smallest metadata size (100% baseline)
 * - Fast serialization/deserialization
 * - Requires fbthrift stack (folly, fizz, mvfst, wangle, fbthrift)
 * - CompactProtocol magic bytes for format detection
 */
class ThriftCompactSerializer : public IMetadataSerializer {
public:
  ThriftCompactSerializer() = default;
  ~ThriftCompactSerializer() override = default;

  // IMetadataSerializer interface
  std::vector<uint8_t> serialize(const void* metadata) const override;

  std::unique_ptr<void, void(*)(void*)> deserialize(
      const std::vector<uint8_t>& data) const override;

  std::string_view get_format_name() const noexcept override {
    return "Modern Thrift Compact";
  }

  SerializationFormat get_format() const noexcept override {
    return SerializationFormat::MODERN_THRIFT;
  }

  bool can_write() const noexcept override {
    return true;
  }

  bool can_read() const noexcept override {
    return true;
  }

  std::vector<uint8_t> get_magic_bytes() const noexcept override {
    return {0x82, 0x21}; // CompactProtocol magic
  }
};

// Registration function (called during static initialization)
void register_thrift_compact_serializer();

} // namespace dwarfs::metadata::serialization