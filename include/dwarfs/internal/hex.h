/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
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

/**
 * \file hex.h
 * \brief Hexadecimal encoding utilities (folly::hexlify replacement)
 *
 * Provides hexadecimal encoding and dump functions.
 */

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <span>
#include <string>
#include <string_view>

namespace dwarfs::compat {

/**
 * \brief Convert bytes to hexadecimal string
 *
 * Replacement for folly::hexlify.
 *
 * \param data Byte span to encode
 * \return Hexadecimal string representation (lowercase)
 */
inline std::string hexlify(std::span<const uint8_t> data) {
  static constexpr char hex_chars[] = "0123456789abcdef";
  std::string result;
  result.reserve(data.size() * 2);

  for (uint8_t byte : data) {
    result += hex_chars[byte >> 4];
    result += hex_chars[byte & 0x0f];
  }

  return result;
}

/**
 * \brief Convert string to hexadecimal string
 */
inline std::string hexlify(std::string_view str) {
  return hexlify(std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(str.data()), str.size()));
}

/**
 * \brief Create hexadecimal dump of data
 *
 * Replacement for folly::hexDump.
 * Creates a formatted hex dump similar to `hexdump -C`.
 *
 * \param data Byte span to dump
 * \param bytes_per_line Number of bytes per line (default: 16)
 * \return Formatted hex dump
 */
inline std::string hexDump(std::span<const uint8_t> data,
                           size_t bytes_per_line = 16) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');

  for (size_t offset = 0; offset < data.size(); offset += bytes_per_line) {
    // Print offset
    oss << std::setw(8) << offset << "  ";

    // Print hex bytes
    size_t line_bytes = std::min(bytes_per_line, data.size() - offset);
    for (size_t i = 0; i < bytes_per_line; ++i) {
      if (i < line_bytes) {
        oss << std::setw(2) << static_cast<unsigned>(data[offset + i]) << ' ';
      } else {
        oss << "   ";
      }

      // Add extra space after 8 bytes
      if (i == 7) {
        oss << ' ';
      }
    }

    // Print ASCII representation
    oss << " |";
    for (size_t i = 0; i < line_bytes; ++i) {
      uint8_t byte = data[offset + i];
      if (byte >= 32 && byte <= 126) {
        oss << static_cast<char>(byte);
      } else {
        oss << '.';
      }
    }
    oss << "|\n";
  }

  return oss.str();
}

/**
 * \brief Create hexadecimal dump of string
 */
inline std::string hexDump(std::string_view str, size_t bytes_per_line = 16) {
  return hexDump(std::span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(str.data()), str.size()),
      bytes_per_line);
}

} // namespace dwarfs::compat