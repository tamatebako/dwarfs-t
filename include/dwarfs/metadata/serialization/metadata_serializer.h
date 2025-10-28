/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Metadata serializer interface (Strategy pattern)
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
#include <string_view>
#include <vector>

#include "serialization_format.h"

namespace dwarfs::metadata {
namespace domain {
// Forward declaration to avoid circular dependency
struct metadata;
} // namespace domain

namespace serialization {

/**
 * Metadata serializer interface
 *
 * This interface defines the contract for all metadata serializers,
 * following the Strategy pattern and Open/Closed principle.
 *
 * Key Design Principles:
 * - **Strategy Pattern**: Different serializers are interchangeable
 * - **Open/Closed**: Open for extension (new serializers), closed for modification
 * - **Dependency Inversion**: Client code depends on this abstraction, not concrete implementations
 * - **Single Responsibility**: Each serializer handles exactly ONE format
 *
 * Implementations:
 * - CerealBinarySerializer: Cereal binary archive format
 * - ThriftCompactSerializer: Apache Thrift compact protocol (legacy)
 *
 * \example
 * \code
 * // Using the serializer through the interface
 * std::unique_ptr<IMetadataSerializer> serializer =
 *     SerializerFactory::create(SerializationFormat::CEREAL_BINARY);
 *
 * // Serialize metadata
 * metadata meta;
 * auto data = serializer->serialize(meta);
 *
 * // Deserialize metadata
 * auto meta2 = serializer->deserialize(data);
 * \endcode
 */
class IMetadataSerializer {
public:
  /// Virtual destructor for proper cleanup of derived classes
  virtual ~IMetadataSerializer() = default;

  /**
   * Serialize metadata to binary format
   *
   * Converts a metadata object into a binary representation
   * using the specific serialization format.
   *
   * \param meta The metadata object to serialize
   * \return Binary data representing the serialized metadata
   *
   * \throws std::runtime_error if serialization fails
   *
   * \example
   * \code
   * metadata meta;
   * meta.block_size = 131072;
   * auto data = serializer->serialize(meta);
   * // data now contains serialized bytes
   * \endcode
   */
  virtual std::vector<uint8_t> serialize(
      const domain::metadata& meta) const = 0;

  /**
   * Deserialize metadata from binary format
   *
   * Reconstructs a metadata object from its binary representation
   * using the specific serialization format.
   *
   * \param data Binary data containing serialized metadata
   * \return Unique pointer to the deserialized metadata object
   *
   * \throws std::runtime_error if deserialization fails
   * \throws std::invalid_argument if data format is invalid
   *
   * \example
   * \code
   * std::vector<uint8_t> data = read_from_file("metadata.bin");
   * auto meta = serializer->deserialize(data);
   * std::cout << "Block size: " << meta->block_size << "\n";
   * \endcode
   */
  virtual std::unique_ptr<domain::metadata> deserialize(
      const std::vector<uint8_t>& data) const = 0;

  /**
   * Get the name of this serialization format
   *
   * Returns a human-readable name for logging and diagnostics.
   *
   * \return String view containing the format name
   *
   * \example
   * \code
   * std::cout << "Using format: " << serializer->get_format_name() << "\n";
   * // Output: "Using format: Cereal Binary"
   * \endcode
   */
  virtual std::string_view get_format_name() const noexcept = 0;

  /**
   * Get the serialization format enum value
   *
   * Returns the format identifier for this serializer.
   *
   * \return The serialization format enum value
   *
   * \example
   * \code
   * if (serializer->get_format() == SerializationFormat::CEREAL_BINARY) {
   *   std::cout << "Using new format\n";
   * }
   * \endcode
   */
  virtual SerializationFormat get_format() const noexcept = 0;

protected:
  /**
   * Protected default constructor
   *
   * Interface should only be instantiated through derived classes.
   */
  IMetadataSerializer() = default;

  /**
   * Protected copy constructor
   *
   * Prevents slicing while allowing derived classes to be copyable.
   */
  IMetadataSerializer(const IMetadataSerializer&) = default;

  /**
   * Protected copy assignment
   *
   * Prevents slicing while allowing derived classes to be assignable.
   */
  IMetadataSerializer& operator=(const IMetadataSerializer&) = default;

  /**
   * Protected move constructor
   *
   * Prevents slicing while allowing derived classes to be movable.
   */
  IMetadataSerializer(IMetadataSerializer&&) = default;

  /**
   * Protected move assignment
   *
   * Prevents slicing while allowing derived classes to be move-assignable.
   */
  IMetadataSerializer& operator=(IMetadataSerializer&&) = default;
};

} // namespace serialization
} // namespace dwarfs::metadata