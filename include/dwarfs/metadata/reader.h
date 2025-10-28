/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief High-level metadata reader
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

#include "domain/metadata.h"
#include "serialization/format_detector.h"
#include "serialization/serializer_factory.h"
#include "serialization/serialization_format.h"

namespace dwarfs::metadata {

/**
 * High-level metadata reader
 *
 * Provides a simple, high-level API for reading metadata from binary data.
 * Handles format detection and serializer selection automatically.
 *
 * Design Principles:
 * - **Facade Pattern**: Simplifies the serialization subsystem interface
 * - **Dependency Inversion**: Depends on abstractions (IMetadataSerializer)
 * - **Single Responsibility**: Only responsible for reading metadata
 *
 * Key Features:
 * - Automatic format detection (default)
 * - Explicit format specification (optional)
 * - Exception-safe error handling
 * - Format information reporting
 *
 * \example
 * \code
 * // Read with automatic format detection
 * MetadataReader reader;
 * std::vector<uint8_t> data = read_file("metadata.bin");
 * auto meta = reader.read(data);
 * std::cout << "Block size: " << meta->block_size << "\n";
 *
 * // Read with explicit format
 * MetadataReader cereal_reader(SerializationFormat::CEREAL_BINARY);
 * auto meta2 = cereal_reader.read(data);
 * \endcode
 */
class MetadataReader {
public:
  /**
   * Construct reader with automatic format detection
   *
   * Creates a reader that will automatically detect the serialization
   * format from magic bytes when reading data.
   *
   * \example
   * \code
   * MetadataReader reader;  // Auto-detect format
   * auto meta = reader.read(data);
   * \endcode
   */
  MetadataReader()
      : format_(serialization::SerializationFormat::AUTO_DETECT) {}

  /**
   * Construct reader with explicit format
   *
   * Creates a reader that expects data in the specified format.
   * Format detection is skipped when reading.
   *
   * \param format The expected serialization format
   *
   * \throws std::invalid_argument if format is not supported
   *
   * \example
   * \code
   * // Reader for Cereal binary format only
   * MetadataReader reader(SerializationFormat::CEREAL_BINARY);
   * auto meta = reader.read(cereal_data);
   * \endcode
   */
  explicit MetadataReader(serialization::SerializationFormat format)
      : format_(format) {
    // Validate format is supported (except AUTO_DETECT)
    if (format != serialization::SerializationFormat::AUTO_DETECT &&
        !serialization::SerializerFactory::is_supported(format)) {
      throw std::invalid_argument(
          std::string("Unsupported serialization format: ") +
          std::string(serialization::get_format_name(format)));
    }
  }

  /**
   * Read metadata from binary data
   *
   * Deserializes metadata using the configured format (auto-detect or explicit).
   *
   * Process:
   * 1. Detect format if AUTO_DETECT is configured
   * 2. Create appropriate serializer
   * 3. Deserialize metadata
   * 4. Return metadata object
   *
   * \param data Binary data containing serialized metadata
   * \return Unique pointer to the deserialized metadata object
   *
   * \throws std::invalid_argument if data is invalid or format unsupported
   * \throws std::runtime_error if deserialization fails
   *
   * \example
   * \code
   * MetadataReader reader;
   * std::vector<uint8_t> data = {0xCE, 0xEA, 0x01, ...};  // Cereal format
   * auto meta = reader.read(data);
   * std::cout << "Total size: " << meta->total_fs_size << "\n";
   * \endcode
   */
  std::unique_ptr<domain::metadata> read(
      const std::vector<uint8_t>& data) const {
    try {
      // Determine the format to use
      auto format = format_;
      if (format == serialization::SerializationFormat::AUTO_DETECT) {
        format = serialization::FormatDetector::detect_format(data);
      }

      // Create serializer and deserialize
      auto serializer = serialization::SerializerFactory::create(format);
      return serializer->deserialize(data);

    } catch (const std::exception& e) {
      throw std::runtime_error(
          std::string("Failed to read metadata: ") + e.what());
    }
  }

  /**
   * Detect the format of binary data
   *
   * Analyzes the data and returns the detected format without
   * actually deserializing the metadata.
   *
   * \param data Binary data to analyze
   * \return The detected serialization format
   *
   * \throws std::invalid_argument if data is too small
   * \throws std::runtime_error if format cannot be determined
   *
   * \example
   * \code
   * MetadataReader reader;
   * std::vector<uint8_t> data = read_file("metadata.bin");
   * auto format = reader.detect_format(data);
   *
   * std::cout << "Detected format: "
   *           << serialization::get_format_name(format) << "\n";
   * \endcode
   */
  serialization::SerializationFormat detect_format(
      const std::vector<uint8_t>& data) const {
    return serialization::FormatDetector::detect_format(data);
  }

  /**
   * Get detailed format information
   *
   * Returns a human-readable description of the format in the data,
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
   * MetadataReader reader;
   * std::vector<uint8_t> data = read_file("metadata.bin");
   * std::cout << reader.get_format_info(data) << "\n";
   * // Output: "Cereal Binary (version 1)"
   * \endcode
   */
  std::string get_format_info(const std::vector<uint8_t>& data) const {
    return serialization::FormatDetector::get_format_info(data);
  }

  /**
   * Get the configured format
   *
   * Returns the format this reader is configured to use.
   *
   * \return The serialization format (may be AUTO_DETECT)
   *
   * \example
   * \code
   * MetadataReader reader(SerializationFormat::CEREAL_BINARY);
   * auto format = reader.get_format();
   * // Returns SerializationFormat::CEREAL_BINARY
   * \endcode
   */
  serialization::SerializationFormat get_format() const noexcept {
    return format_;
  }

  /**
   * Check if reader uses automatic format detection
   *
   * \return true if format is AUTO_DETECT, false otherwise
   *
   * \example
   * \code
   * MetadataReader reader;
   * if (reader.is_auto_detect()) {
   *   std::cout << "Will auto-detect format\n";
   * }
   * \endcode
   */
  bool is_auto_detect() const noexcept {
    return format_ == serialization::SerializationFormat::AUTO_DETECT;
  }

private:
  /// Configured serialization format
  serialization::SerializationFormat format_;
};

} // namespace dwarfs::metadata