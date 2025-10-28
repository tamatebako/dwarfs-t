/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Thrift Compact serializer implementation (legacy format adapter)
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
#include <memory>
#include <stdexcept>
#include <vector>

#include <thrift/lib/cpp2/protocol/Serializer.h>

#include "../domain/metadata.h"
#include "metadata_serializer.h"
#include "serialization_format.h"
#include "thrift_converter.h"

namespace dwarfs::metadata::serialization {

/**
 * Thrift Compact serializer implementation
 *
 * Implements metadata serialization using Apache Thrift Compact Protocol.
 * This is the LEGACY format adapter for backward compatibility with existing
 * DwarFS images. New images should use CerealBinarySerializer.
 *
 * Design Principles:
 * - **Adapter Pattern**: Wraps existing Thrift code without exposing it
 * - **Read-Only**: Only supports deserialization (backward compatibility)
 * - **Single Responsibility**: Only handles Thrift Compact format
 * - **Dependency Inversion**: Implements IMetadataSerializer interface
 *
 * Format Details:
 * - Magic bytes: 0x82 0x21 (Thrift Compact Protocol header)
 * - Protocol: Apache Thrift Compact Protocol
 * - Schema: Defined in thrift/metadata.thrift
 *
 * Architecture:
 * ```
 * ┌──────────────────────────────────────────────────────────┐
 * │          ThriftCompactSerializer (Adapter)               │
 * │  ┌────────────────────────────────────────────────────┐  │
 * │  │ deserialize(bytes) → domain::metadata              │  │
 * │  │   1. Deserialize Thrift bytes → thrift::metadata   │  │
 * │  │   2. Convert thrift → domain (ThriftConverter)     │  │
 * │  │   3. Return domain model                           │  │
 * │  └────────────────────────────────────────────────────┘  │
 * │  ┌────────────────────────────────────────────────────┐  │
 * │  │ serialize() → throws "not supported"               │  │
 * │  │   (Read-only adapter for legacy format)            │  │
 * │  └────────────────────────────────────────────────────┘  │
 * └──────────────────────────────────────────────────────────┘
 * ```
 *
 * \example
 * \code
 * ThriftCompactSerializer serializer;
 *
 * // Read legacy .dwarfs metadata
 * std::vector<uint8_t> data = read_legacy_metadata();
 * auto meta = serializer.deserialize(data);
 *
 * // Serialization not supported (throws exception)
 * // auto data2 = serializer.serialize(*meta); // ERROR!
 * \endcode
 */
class ThriftCompactSerializer : public IMetadataSerializer {
public:
  /**
   * Default constructor
   */
  ThriftCompactSerializer() = default;

  /**
   * Virtual destructor
   */
  ~ThriftCompactSerializer() override = default;

  /**
   * Serialize metadata (NOT SUPPORTED - throws exception)
   *
   * This adapter is READ-ONLY for backward compatibility.
   * New images should use CerealBinarySerializer for writing.
   *
   * \param meta The metadata object (unused)
   * \return Never returns
   *
   * \throws std::runtime_error Always - serialization not supported
   *
   * \example
   * \code
   * ThriftCompactSerializer serializer;
   * metadata meta;
   *
   * try {
   *   auto data = serializer.serialize(meta);
   * } catch (const std::runtime_error& e) {
   *   // Caught: "Thrift serialization not supported..."
   * }
   * \endcode
   */
  std::vector<uint8_t> serialize(
      const domain::metadata& meta) const override {
    (void)meta; // Suppress unused parameter warning
    throw std::runtime_error(
        "Thrift serialization not supported. This is a read-only adapter for "
        "legacy format compatibility. Use CerealBinarySerializer for writing "
        "new metadata.");
  }

  /**
   * Deserialize metadata from Thrift Compact format
   *
   * Reconstructs a metadata object from legacy Thrift binary data.
   *
   * Deserialization process:
   * 1. Validate Thrift magic bytes (0x82 0x21)
   * 2. Deserialize using Apache Thrift CompactSerializer
   * 3. Convert thrift::metadata to domain::metadata using ThriftConverter
   * 4. Return domain model
   *
   * \param data Binary data containing Thrift-serialized metadata
   * \return Unique pointer to the deserialized metadata object
   *
   * \throws std::invalid_argument if data is too small or has wrong magic bytes
   * \throws std::runtime_error if deserialization or conversion fails
   *
   * \example
   * \code
   * std::vector<uint8_t> data = read_legacy_dwarfs_metadata();
   *
   * ThriftCompactSerializer serializer;
   * auto meta = serializer.deserialize(data);
   *
   * std::cout << "Block size: " << meta->block_size << "\n";
   * std::cout << "Total size: " << meta->total_fs_size << "\n";
   * \endcode
   */
  std::unique_ptr<domain::metadata> deserialize(
      const std::vector<uint8_t>& data) const override {
    // Validate minimum size
    if (data.size() < magic_bytes::MIN_DETECTION_SIZE) {
      throw std::invalid_argument(
          "Data too small for Thrift deserialization (need at least " +
          std::to_string(magic_bytes::MIN_DETECTION_SIZE) + " bytes, got " +
          std::to_string(data.size()) + ")");
    }

    // Validate Thrift magic bytes
    if (data[0] != magic_bytes::THRIFT_COMPACT_V1 ||
        data[1] != magic_bytes::THRIFT_COMPACT_V2) {
      throw std::invalid_argument(
          "Invalid magic bytes for Thrift Compact format. Expected [0x82 0x21], "
          "got [0x" + to_hex(data[0]) + " 0x" + to_hex(data[1]) + "]");
    }

    try {
      // Deserialize Thrift data
      thrift::metadata::metadata thrift_meta;
      folly::ByteRange byte_range(data.data(), data.size());

      size_t bytes_read = apache::thrift::CompactSerializer::deserialize(
          byte_range, thrift_meta);

      if (bytes_read == 0) {
        throw std::runtime_error(
            "Thrift deserialization returned 0 bytes read");
      }

      // Convert Thrift model to domain model
      auto domain_meta = ThriftConverter::to_domain(thrift_meta);

      return std::make_unique<domain::metadata>(std::move(domain_meta));

    } catch (const apache::thrift::protocol::TProtocolException& e) {
      throw std::runtime_error(
          std::string("Thrift protocol error during deserialization: ") + e.what());
    } catch (const std::exception& e) {
      throw std::runtime_error(
          std::string("Thrift deserialization failed: ") + e.what());
    }
  }

  /**
   * Get the format name
   *
   * \return "Thrift Compact"
   */
  std::string_view get_format_name() const noexcept override {
    return get_format_name(SerializationFormat::THRIFT_COMPACT);
  }

  /**
   * Get the serialization format
   *
   * \return SerializationFormat::THRIFT_COMPACT
   */
  SerializationFormat get_format() const noexcept override {
    return SerializationFormat::THRIFT_COMPACT;
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