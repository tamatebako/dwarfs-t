/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Format detection from magic bytes
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
#include <stdexcept>
#include <vector>

#include "serialization_format.h"
#include "serializer_registry.h"

namespace dwarfs::metadata::serialization {

/**
 * Format detector for serialized metadata
 *
 * Detects the serialization format from magic bytes at the start
 * of binary data. This enables automatic format detection when
 * reading metadata from unknown sources.
 *
 * Detection Strategy:
 * 1. Check for Cereal magic bytes (0xCE 0xEA 0x01)
 * 2. Check for Thrift Compact magic bytes (0x82 0x21)
 * 3. Throw exception if format is unrecognized
 *
 * \example
 * \code
 * std::vector<uint8_t> data = read_metadata_file("meta.bin");
 * auto format = FormatDetector::detect_format(data);
 *
 * if (format == SerializationFormat::CEREAL_BINARY) {
 *   std::cout << "New format detected\n";
 * } else {
 *   std::cout << "Legacy format detected\n";
 * }
 * \endcode
 */
class FormatDetector {
public:
  /**
   * Detect serialization format from binary data
   *
   * Examines the magic bytes at the start of the data to determine
   * which serialization format was used.
   *
   * \param data Binary data containing serialized metadata
   * \return The detected serialization format
   *
   * \throws std::invalid_argument if data is too small
   * \throws std::runtime_error if format cannot be determined
   *
   * \example
   * \code
   * std::vector<uint8_t> cereal_data = {0xCE, 0xEA, 0x01, ...};
   * auto format = FormatDetector::detect_format(cereal_data);
   * // Returns SerializationFormat::CEREAL_BINARY
   *
   * std::vector<uint8_t> thrift_data = {0x82, 0x21, ...};
   * format = FormatDetector::detect_format(thrift_data);
   * // Returns SerializationFormat::THRIFT_COMPACT
   * \endcode
   */
  static SerializationFormat detect_format(const std::vector<uint8_t>& data) {
    return SerializerRegistry::instance().detect_format(data);
  }

  /**
   * Check if data is in Cereal Binary format
   *
   * \param data Binary data to check
   * \return true if Cereal format is detected, false otherwise
   *
   * \example
   * \code
   * std::vector<uint8_t> data = {0xCE, 0xEA, 0x01, ...};
   * bool is_cereal = FormatDetector::is_cereal_binary(data);
   * // Returns true
   * \endcode
   */
  static bool is_cereal_binary(const std::vector<uint8_t>& data) noexcept {
    return data.size() >= magic_bytes::MIN_DETECTION_SIZE &&
           data[0] == magic_bytes::CEREAL_MAGIC_1 &&
           data[1] == magic_bytes::CEREAL_MAGIC_2 &&
           data[2] == magic_bytes::CEREAL_VERSION;
  }

  /**
   * Check if data is in Thrift Compact format
   *
   * \param data Binary data to check
   * \return true if Thrift format is detected, false otherwise
   *
   * \example
   * \code
   * std::vector<uint8_t> data = {0x82, 0x21, ...};
   * bool is_thrift = FormatDetector::is_thrift_compact(data);
   * // Returns true
   * \endcode
   */
  static bool is_thrift_compact(const std::vector<uint8_t>& data) noexcept {
    // Thrift Compact Protocol starts with protocol ID byte 0x82
    // followed by message type byte (typically 0x21 for struct)
    return data.size() >= 2 &&
           data[0] == magic_bytes::THRIFT_COMPACT_V1 &&
           data[1] == magic_bytes::THRIFT_COMPACT_V2;
  }

  /**
   * Get detailed format information
   *
   * Returns a human-readable description of the detected format
   * including version information when available.
   *
   * \param data Binary data to analyze
   * \return Detailed format description
   *
   * \throws std::invalid_argument if data is too small
   * \throws std::runtime_error if format cannot be determined
   *
   * \example
   * \code
   * std::vector<uint8_t> data = read_file("metadata.bin");
   * std::cout << FormatDetector::get_format_info(data) << "\n";
   * // Output: "Cereal Binary (version 1)"
   * \endcode
   */
  static std::string get_format_info(const std::vector<uint8_t>& data) {
    auto format = detect_format(data);

    std::string info{get_format_name(format)};

    if (format == SerializationFormat::CEREAL_BINARY &&
        data.size() >= magic_bytes::MIN_DETECTION_SIZE) {
      info += " (version ";
      info += std::to_string(data[2]);
      info += ")";
    }

    return info;
  }

private:
  /**
   * Convert byte to hexadecimal string
   *
   * Helper for error messages.
   *
   * \param byte Byte value to convert
   * \return Hexadecimal string representation
   */
  static std::string to_hex(uint8_t byte) {
    constexpr char hex_chars[] = "0123456789ABCDEF";
    std::string result;
    result += hex_chars[byte >> 4];
    result += hex_chars[byte & 0x0F];
    return result;
  }
};

} // namespace dwarfs::metadata::serialization