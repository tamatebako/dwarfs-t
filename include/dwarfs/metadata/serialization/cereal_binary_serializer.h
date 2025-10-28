/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Cereal binary serializer implementation
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
#include <sstream>
#include <stdexcept>
#include <vector>

#include <cereal/archives/binary.hpp>

#include "../domain/metadata.h"
#include "metadata_serializer.h"
#include "serialization_format.h"

namespace dwarfs::metadata::serialization {

/**
 * Cereal binary serializer implementation
 *
 * Implements metadata serialization using the Cereal binary archive format.
 * This is the new standard format for DwarFS metadata, replacing the legacy
 * Thrift format.
 *
 * Design Principles:
 * - **Strategy Pattern**: Concrete implementation of IMetadataSerializer
 * - **Single Responsibility**: Only handles Cereal binary format
 * - **Header-only**: Required for Cereal template instantiation
 *
 * Format Details:
 * - Magic bytes: 0xCE 0xEA 0x01 (custom header for format detection)
 * - Archive: Cereal BinaryArchive (portable binary format)
 * - Versioning: Supports schema evolution through Cereal's versioning
 *
 * \example
 * \code
 * CerealBinarySerializer serializer;
 *
 * // Serialize metadata
 * metadata meta;
 * meta.block_size = 131072;
 * auto data = serializer.serialize(meta);
 *
 * // Deserialize metadata
 * auto meta2 = serializer.deserialize(data);
 * assert(meta2->block_size == 131072);
 * \endcode
 */
class CerealBinarySerializer : public IMetadataSerializer {
public:
  /**
   * Default constructor
   */
  CerealBinarySerializer() = default;

  /**
   * Virtual destructor
   */
  ~CerealBinarySerializer() override = default;

  /**
   * Serialize metadata to Cereal binary format
   *
   * Converts a metadata object into binary format with magic byte header
   * for format detection.
   *
   * Format structure:
   * 1. Magic bytes (3 bytes): 0xCE 0xEA 0x01
   * 2. Cereal binary archive data (variable length)
   *
   * \param meta The metadata object to serialize
   * \return Binary data with magic header and serialized metadata
   *
   * \throws std::runtime_error if serialization fails
   *
   * \example
   * \code
   * metadata meta;
   * meta.block_size = 131072;
   * meta.total_fs_size = 1024 * 1024 * 1024;
   *
   * CerealBinarySerializer serializer;
   * auto data = serializer.serialize(meta);
   *
   * // Data starts with magic bytes: 0xCE 0xEA 0x01
   * assert(data[0] == 0xCE && data[1] == 0xEA && data[2] == 0x01);
   * \endcode
   */
  std::vector<uint8_t> serialize(const domain::metadata& meta) const override {
    try {
      // Create string stream for binary data
      std::ostringstream oss(std::ios::binary);

      // Write magic bytes for format detection
      oss.put(static_cast<char>(magic_bytes::CEREAL_MAGIC_1));
      oss.put(static_cast<char>(magic_bytes::CEREAL_MAGIC_2));
      oss.put(static_cast<char>(magic_bytes::CEREAL_VERSION));

      // Serialize metadata using Cereal binary archive
      {
        cereal::BinaryOutputArchive archive(oss);
        archive(meta);
      } // Archive destructor flushes data

      // Convert to vector
      std::string str = oss.str();
      return std::vector<uint8_t>(str.begin(), str.end());

    } catch (const cereal::Exception& e) {
      throw std::runtime_error(
          std::string("Cereal serialization failed: ") + e.what());
    } catch (const std::exception& e) {
      throw std::runtime_error(
          std::string("Serialization failed: ") + e.what());
    }
  }

  /**
   * Deserialize metadata from Cereal binary format
   *
   * Reconstructs a metadata object from binary data with magic byte validation.
   *
   * Format validation:
   * 1. Checks for minimum data size
   * 2. Validates magic bytes
   * 3. Deserializes using Cereal
   *
   * \param data Binary data containing serialized metadata
   * \return Unique pointer to the deserialized metadata object
   *
   * \throws std::invalid_argument if data is too small or has wrong magic bytes
   * \throws std::runtime_error if deserialization fails
   *
   * \example
   * \code
   * std::vector<uint8_t> data = read_metadata_file("meta.bin");
   *
   * CerealBinarySerializer serializer;
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
          "Data too small for Cereal deserialization (need at least " +
          std::to_string(magic_bytes::MIN_DETECTION_SIZE) + " bytes, got " +
          std::to_string(data.size()) + ")");
    }

    // Validate magic bytes
    if (data[0] != magic_bytes::CEREAL_MAGIC_1 ||
        data[1] != magic_bytes::CEREAL_MAGIC_2 ||
        data[2] != magic_bytes::CEREAL_VERSION) {
      throw std::invalid_argument(
          "Invalid magic bytes for Cereal format. Expected [0xCE 0xEA 0x01], "
          "got [0x" + to_hex(data[0]) + " 0x" + to_hex(data[1]) + " 0x" +
          to_hex(data[2]) + "]");
    }

    try {
      // Create string stream from data (skip magic bytes)
      std::string str(data.begin() + magic_bytes::MIN_DETECTION_SIZE,
                      data.end());
      std::istringstream iss(str, std::ios::binary);

      // Deserialize using Cereal binary archive
      auto meta = std::make_unique<domain::metadata>();
      {
        cereal::BinaryInputArchive archive(iss);
        archive(*meta);
      }

      return meta;

    } catch (const cereal::Exception& e) {
      throw std::runtime_error(
          std::string("Cereal deserialization failed: ") + e.what());
    } catch (const std::exception& e) {
      throw std::runtime_error(
          std::string("Deserialization failed: ") + e.what());
    }
  }

  /**
   * Get the format name
   *
   * \return "Cereal Binary"
   */
  std::string_view get_format_name() const noexcept override {
    return get_format_name(SerializationFormat::CEREAL_BINARY);
  }

  /**
   * Get the serialization format
   *
   * \return SerializationFormat::CEREAL_BINARY
   */
  SerializationFormat get_format() const noexcept override {
    return SerializationFormat::CEREAL_BINARY;
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