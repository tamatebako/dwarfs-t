/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Serialization format enumeration and magic byte constants
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace dwarfs::metadata::serialization {

/**
 * Serialization format identifier
 *
 * Defines the available serialization formats for metadata persistence.
 * This enum supports the Open/Closed principle by allowing new formats
 * to be added without modifying existing code.
 */
enum class SerializationFormat {
  /**
   * Apache Thrift Compact Protocol (legacy format)
   *
   * Used by DwarFS versions prior to the Cereal migration.
   * Maintained for backward compatibility with existing images.
   *
   * Magic bytes: 0x82 0x21 (Thrift Compact Protocol header)
   */
  THRIFT_COMPACT,

  /**
   * Cereal Binary Archive (new format)
   *
   * Modern serialization format using Cereal library.
   * Preferred format for new file system images.
   *
   * Magic bytes: 0xCE 0xEA 0x01 ('CE'real version 1)
   */
  CEREAL_BINARY,

  /**
   * Automatic format detection
   *
   * Uses magic byte detection to determine the format.
   * This is the recommended setting for reading operations.
   */
  AUTO_DETECT
};

/**
 * Magic byte constants for format detection
 *
 * These constants are used by FormatDetector to identify
 * the serialization format from raw byte data.
 */
namespace magic_bytes {

/// Thrift Compact Protocol magic bytes
constexpr uint8_t THRIFT_COMPACT_V1 = 0x82;
constexpr uint8_t THRIFT_COMPACT_V2 = 0x21;

/// Cereal Binary Archive magic bytes (custom header)
constexpr uint8_t CEREAL_MAGIC_1 = 0xCE;  // 'CE' from Cereal
constexpr uint8_t CEREAL_MAGIC_2 = 0xEA;  // 'EA' from cerEAl
constexpr uint8_t CEREAL_VERSION = 0x01;  // Version 1

/// Minimum bytes needed for format detection
constexpr std::size_t MIN_DETECTION_SIZE = 3;

} // namespace magic_bytes

/**
 * Get human-readable name for a serialization format
 *
 * \param format The serialization format
 * \return String representation of the format
 *
 * \example
 * \code
 * auto name = get_format_name(SerializationFormat::CEREAL_BINARY);
 * // Returns "Cereal Binary"
 * \endcode
 */
inline constexpr std::string_view get_format_name(SerializationFormat format) {
  switch (format) {
    case SerializationFormat::THRIFT_COMPACT:
      return "Thrift Compact";
    case SerializationFormat::CEREAL_BINARY:
      return "Cereal Binary";
    case SerializationFormat::AUTO_DETECT:
      return "Auto Detect";
  }
  return "Unknown";
}

} // namespace dwarfs::metadata::serialization