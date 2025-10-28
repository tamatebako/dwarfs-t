/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief High-level metadata writer
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
#include <string>
#include <vector>

#include "domain/metadata.h"
#include "serialization/serializer_factory.h"
#include "serialization/serialization_format.h"

namespace dwarfs::metadata {

/**
 * High-level metadata writer
 *
 * Provides a simple, high-level API for writing metadata to binary format.
 * Handles serializer selection and binary encoding automatically.
 *
 * Design Principles:
 * - **Facade Pattern**: Simplifies the serialization subsystem interface
 * - **Dependency Inversion**: Depends on abstractions (IMetadataSerializer)
 * - **Single Responsibility**: Only responsible for writing metadata
 *
 * Key Features:
 * - Configurable output format (defaults to Cereal Binary)
 * - Exception-safe error handling
 * - Format information reporting
 *
 * \example
 * \code
 * // Write with default format (Cereal Binary)
 * MetadataWriter writer;
 * metadata meta;
 * meta.block_size = 131072;
 * auto data = writer.write(meta);
 *
 * // Write with explicit format
 * MetadataWriter cereal_writer(SerializationFormat::CEREAL_BINARY);
 * auto data2 = cereal_writer.write(meta);
 * \endcode
 */
class MetadataWriter {
public:
  /**
   * Construct writer with default format (Cereal Binary)
   *
   * Creates a writer that will use the new Cereal Binary format,
   * which is the recommended format for new file system images.
   *
   * \example
   * \code
   * MetadataWriter writer;  // Uses Cereal Binary
   * auto data = writer.write(metadata);
   * \endcode
   */
  MetadataWriter()
      : format_(serialization::SerializationFormat::CEREAL_BINARY) {}

  /**
   * Construct writer with explicit format
   *
   * Creates a writer that will use the specified serialization format.
   *
   * \param format The desired serialization format
   *
   * \throws std::invalid_argument if format is AUTO_DETECT or not supported
   *
   * \example
   * \code
   * // Writer for Cereal binary format
   * MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
   * auto data = writer.write(metadata);
   * \endcode
   */
  explicit MetadataWriter(serialization::SerializationFormat format)
      : format_(format) {
    // Validate format
    if (format == serialization::SerializationFormat::AUTO_DETECT) {
      throw std::invalid_argument(
          "Cannot create writer with AUTO_DETECT format. "
          "Please specify a concrete serialization format.");
    }

    if (!serialization::SerializerFactory::is_supported(format)) {
      throw std::invalid_argument(
          std::string("Unsupported serialization format: ") +
          std::string(serialization::get_format_name(format)));
    }
  }

  /**
   * Write metadata to binary format
   *
   * Serializes metadata using the configured format.
   *
   * Process:
   * 1. Create appropriate serializer
   * 2. Serialize metadata
   * 3. Return binary data
   *
   * \param meta The metadata object to serialize
   * \return Binary data containing serialized metadata
   *
   * \throws std::runtime_error if serialization fails
   *
   * \example
   * \code
   * MetadataWriter writer;
   *
   * metadata meta;
   * meta.block_size = 131072;
   * meta.total_fs_size = 1024 * 1024 * 1024;
   *
   * auto data = writer.write(meta);
   * write_to_file("metadata.bin", data);
   * \endcode
   */
  std::vector<uint8_t> write(const domain::metadata& meta) const {
    try {
      // Create serializer and serialize
      auto serializer = serialization::SerializerFactory::create(format_);
      return serializer->serialize(meta);

    } catch (const std::exception& e) {
      throw std::runtime_error(
          std::string("Failed to write metadata: ") + e.what());
    }
  }

  /**
   * Get the configured format
   *
   * Returns the format this writer will use for serialization.
   *
   * \return The serialization format
   *
   * \example
   * \code
   * MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
   * auto format = writer.get_format();
   * // Returns SerializationFormat::CEREAL_BINARY
   * \endcode
   */
  serialization::SerializationFormat get_format() const noexcept {
    return format_;
  }

  /**
   * Get the format name
   *
   * Returns a human-readable name of the configured format.
   *
   * \return String view containing the format name
   *
   * \example
   * \code
   * MetadataWriter writer;
   * std::cout << "Using format: " << writer.get_format_name() << "\n";
   * // Output: "Using format: Cereal Binary"
   * \endcode
   */
  std::string_view get_format_name() const noexcept {
    return serialization::get_format_name(format_);
  }

  /**
   * Check if format is supported
   *
   * Tests whether the configured format can actually be used for writing.
   *
   * \return true if format is supported, false otherwise
   *
   * \note This should always return true for a successfully constructed writer
   *
   * \example
   * \code
   * MetadataWriter writer(SerializationFormat::CEREAL_BINARY);
   * assert(writer.is_format_supported());
   * \endcode
   */
  bool is_format_supported() const noexcept {
    return serialization::SerializerFactory::is_supported(format_);
  }

private:
  /// Configured serialization format
  serialization::SerializationFormat format_;
};

} // namespace dwarfs::metadata