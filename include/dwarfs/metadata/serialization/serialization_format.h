/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#pragma once

#include <cstdint>
#include <string_view>

namespace dwarfs::metadata::serialization {

/**
 * Serialization format enumeration
 *
 * Identifies the serialization format used for DwarFS metadata.
 * Two formats are supported:
 * - THRIFT_COMPACT: Legacy Apache Thrift format (optional, for compatibility)
 * - FLATBUFFERS: Modern unified FlatBuffers format (required, default)
 * - AUTO_DETECT: Automatically detect format from magic bytes
 */
enum class SerializationFormat {
  THRIFT_COMPACT,   // Legacy Apache Thrift (no magic bytes, fallback)
  FLATBUFFERS,      // FlatBuffers (file identifier: "DFBF")
  AUTO_DETECT       // Auto-detect from magic bytes
};

/**
 * Magic byte constants for format detection
 *
 * Each serialization format uses a unique magic byte sequence at the start
 * of the serialized data for format identification.
 */
namespace magic_bytes {
  // FlatBuffers file identifier ("DFBF" = DwarFS FlatBuffers)
  constexpr uint8_t FLATBUFFERS_MAGIC_1 = 'D';
  constexpr uint8_t FLATBUFFERS_MAGIC_2 = 'F';
  constexpr uint8_t FLATBUFFERS_MAGIC_3 = 'B';
  constexpr uint8_t FLATBUFFERS_MAGIC_4 = 'F';
} // namespace magic_bytes

/**
 * Get human-readable format name
 *
 * @param format The serialization format
 * @return String view of the format name
 */
constexpr std::string_view get_format_name(SerializationFormat format) {
  switch (format) {
    case SerializationFormat::THRIFT_COMPACT:
      return "Thrift Compact";
    case SerializationFormat::FLATBUFFERS:
      return "FlatBuffers";
    case SerializationFormat::AUTO_DETECT:
      return "Auto-Detect";
    default:
      return "Unknown";
  }
}

} // namespace dwarfs::metadata::serialization