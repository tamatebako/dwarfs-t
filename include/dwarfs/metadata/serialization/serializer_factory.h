/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Serializer factory (Abstract Factory pattern)
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

#include <memory>
#include <stdexcept>

#include "format_detector.h"
#include "metadata_serializer.h"
#include "serialization_format.h"
#include "serializer_registry.h"

namespace dwarfs::metadata::serialization {

/**
 * Serializer factory (Abstract Factory pattern)
 *
 * Creates serializer instances based on the desired format.
 * Follows the Abstract Factory and Open/Closed principles.
 *
 * Design Principles:
 * - **Abstract Factory**: Encapsulates creation logic
 * - **Open/Closed**: Adding new formats doesn't modify existing code
 * - **Single Responsibility**: Only responsible for creating serializers
 * - **Dependency Inversion**: Returns interface, not concrete types
 *
 * Usage Pattern:
 * 1. Call create() with desired format
 * 2. Receive serializer through IMetadataSerializer interface
 * 3. Use serializer without knowledge of concrete implementation
 *
 * \example
 * \code
 * // Create a Cereal serializer
 * auto serializer = SerializerFactory::create(SerializationFormat::CEREAL_BINARY);
 * auto data = serializer->serialize(metadata);
 *
 * // Create based on auto-detection
 * auto format = FormatDetector::detect_format(data);
 * auto deserializer = SerializerFactory::create(format);
 * auto meta = deserializer->deserialize(data);
 * \endcode
 */
class SerializerFactory {
public:
  /**
   * Create a serializer for the specified format
   *
   * Factory method that instantiates the appropriate serializer
   * based on the requested format.
   *
   * \param format The serialization format to create
   * \return Unique pointer to the serializer interface
   *
   * \throws std::invalid_argument if format is AUTO_DETECT
   * \throws std::runtime_error if format is not supported
   *
   * \note AUTO_DETECT is not valid for creation. Use detect_format()
   *       first, then create with the detected format.
   *
   * \example
   * \code
   * // Create Cereal serializer
   * auto cereal = SerializerFactory::create(SerializationFormat::CEREAL_BINARY);
   *
   * // For auto-detection, detect first then create
   * auto format = FormatDetector::detect_format(data);
   * auto serializer = SerializerFactory::create(format);
   * \endcode
   */
  static std::unique_ptr<IMetadataSerializer> create(
      SerializationFormat format) {
    return SerializerRegistry::instance().create_serializer(format);
  }

  /**
   * Create a serializer by detecting format from data
   *
   * Convenience method that combines format detection and serializer
   * creation in a single call.
   *
   * \param data Binary data to detect format from
   * \return Unique pointer to the appropriate serializer
   *
   * \throws std::invalid_argument if data is too small
   * \throws std::runtime_error if format cannot be determined or is unsupported
   *
   * \example
   * \code
   * std::vector<uint8_t> data = read_metadata_file("meta.bin");
   * auto serializer = SerializerFactory::create_from_data(data);
   * auto meta = serializer->deserialize(data);
   * \endcode
   */
  static std::unique_ptr<IMetadataSerializer> create_from_data(
      const std::vector<uint8_t>& data) {
    auto format = FormatDetector::detect_format(data);
    return create(format);
  }

  /**
   * Get the default serializer for new files
   *
   * Returns the recommended serializer for creating new metadata.
   * Currently defaults to Cereal Binary format.
   *
   * \return Unique pointer to the default serializer
   *
   * \example
   * \code
   * auto serializer = SerializerFactory::create_default();
   * auto data = serializer->serialize(metadata);
   * // Uses Cereal Binary format
   * \endcode
   */
  static std::unique_ptr<IMetadataSerializer> create_default() {
    // Use the default format from configuration or fallback to Cereal Binary
    return create(SerializationFormat::CEREAL_BINARY);
  }

  /**
   * Check if a format is supported
   *
   * Tests whether a serializer implementation exists for the given format.
   *
   * \param format The serialization format to check
   * \return true if the format is supported, false otherwise
   *
   * \note AUTO_DETECT always returns false as it's not a concrete format
   *
   * \example
   * \code
   * if (SerializerFactory::is_supported(SerializationFormat::CEREAL_BINARY)) {
   *   std::cout << "Cereal format is available\n";
   * }
   * \endcode
   */
  static bool is_supported(SerializationFormat format) noexcept {
    return SerializerRegistry::instance().is_registered(format);
  }

private:
  // Static-only class, no instances allowed
  SerializerFactory() = delete;
  ~SerializerFactory() = delete;
  SerializerFactory(const SerializerFactory&) = delete;
  SerializerFactory& operator=(const SerializerFactory&) = delete;
  SerializerFactory(SerializerFactory&&) = delete;
  SerializerFactory& operator=(SerializerFactory&&) = delete;
};

} // namespace dwarfs::metadata::serialization