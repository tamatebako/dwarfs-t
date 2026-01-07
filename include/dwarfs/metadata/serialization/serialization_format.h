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
 * Three formats are supported:
 * - LEGACY_THRIFT: Hand-coded Frozen2 (Homebrew v0.14.1 compat, always available)
 * - FLATBUFFERS: Modern unified format (header-only, recommended default)
 * - MODERN_THRIFT: Modern Thrift CompactProtocol (optional, requires fbthrift)
 * - AUTO_DETECT: Automatically detect format from magic bytes
 */
enum class SerializationFormat {
  LEGACY_THRIFT,    // Hand-coded Frozen2 (no magic bytes, priority 50, fallback)
  FLATBUFFERS,      // FlatBuffers (magic: "DFBF", priority 120, default)
  MODERN_THRIFT,    // Modern Thrift CompactProtocol (magic: {0x82, 0x21}, priority 100)
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

  // Modern Thrift CompactProtocol header bytes
  // {0x82, 0x21} = CompactProtocol struct header pattern
  constexpr uint8_t MODERN_THRIFT_MAGIC_1 = 0x82;
  constexpr uint8_t MODERN_THRIFT_MAGIC_2 = 0x21;
} // namespace magic_bytes

/**
 * Get human-readable format name
 *
 * @param format The serialization format
 * @return String view of the format name
 */
constexpr std::string_view get_format_name(SerializationFormat format) {
  switch (format) {
    case SerializationFormat::LEGACY_THRIFT:
      return "Legacy Thrift (Frozen2)";
    case SerializationFormat::FLATBUFFERS:
      return "FlatBuffers";
    case SerializationFormat::MODERN_THRIFT:
      return "Modern Thrift (CompactProtocol)";
    case SerializationFormat::AUTO_DETECT:
      return "Auto-Detect";
    default:
      return "Unknown";
  }
}

} // namespace dwarfs::metadata::serialization