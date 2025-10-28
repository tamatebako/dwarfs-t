/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Serializer registry for auto-discovery and creation
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
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "metadata_serializer.h"
#include "serialization_format.h"

namespace dwarfs::metadata::serialization {

/**
 * Serializer factory function type
 *
 * Function signature for creating serializer instances.
 */
using SerializerCreator = std::function<std::unique_ptr<IMetadataSerializer>()>;

/**
 * Serializer registration metadata
 *
 * Contains all information needed to identify and create a serializer.
 */
struct SerializerRegistration {
  std::string name;                 ///< Format identifier
  SerializerCreator creator;        ///< Factory function
  std::vector<uint8_t> magic_bytes; ///< Magic bytes for detection
  int priority{0};                  ///< Detection priority (higher = checked first)
  SerializationFormat format;       ///< Format enum value
};

/**
 * Serializer registry for auto-discovery
 *
 * Thread-safe singleton registry that manages serializer implementations.
 * Serializers self-register at static initialization time using the
 * auto-registration helper.
 *
 * Design Principles:
 * - **Registry Pattern**: Central registration point for serializers
 * - **Auto-Discovery**: Serializers register themselves automatically
 * - **Thread Safety**: All operations protected by mutex
 * - **Open/Closed**: New serializers can be added without modifying core code
 *
 * \example
 * \code
 * auto& registry = SerializerRegistry::instance();
 *
 * // Create serializer by format
 * auto serializer = registry.create_serializer(SerializationFormat::CEREAL_BINARY);
 *
 * // Detect format from data
 * std::vector<uint8_t> data = read_file("metadata.bin");
 * auto format = registry.detect_format(data);
 * \endcode
 */
class SerializerRegistry {
public:
  /**
   * Get singleton instance
   *
   * Thread-safe lazy initialization using C++11 static initialization.
   *
   * \return Reference to the singleton instance
   */
  static SerializerRegistry& instance() {
    static SerializerRegistry instance;
    return instance;
  }

  /**
   * Register a serializer
   *
   * Called by auto-registration helper during static initialization.
   * Registers a serializer with its metadata.
   *
   * \param name Format identifier
   * \param creator Factory function to create serializer instances
   * \param magic_bytes Magic bytes for format detection
   * \param priority Detection priority (higher = checked first)
   * \param format Format enum value
   *
   * \throws std::runtime_error if format already registered
   *
   * \example
   * \code
   * SerializerRegistry::instance().register_serializer(
   *     "cereal_binary",
   *     []() { return std::make_unique<CerealBinarySerializer>(); },
   *     {0xCE, 0xEA, 0x01},
   *     100,
   *     SerializationFormat::CEREAL_BINARY
   * );
   * \endcode
   */
  void register_serializer(const std::string& name, SerializerCreator creator,
                           const std::vector<uint8_t>& magic_bytes,
                           int priority, SerializationFormat format) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (serializers_.find(name) != serializers_.end()) {
      throw std::runtime_error("Serializer already registered: " + name);
    }

    SerializerRegistration reg;
    reg.name = name;
    reg.creator = std::move(creator);
    reg.magic_bytes = magic_bytes;
    reg.priority = priority;
    reg.format = format;

    serializers_[name] = reg;

    // Add to format map for quick lookup
    format_map_[format] = name;
  }

  /**
   * Create a serializer by format
   *
   * Factory method that creates a serializer instance for the
   * specified format.
   *
   * \param format The serialization format
   * \return Unique pointer to the serializer instance
   *
   * \throws std::runtime_error if format is not registered or AUTO_DETECT
   *
   * \example
   * \code
   * auto serializer = SerializerRegistry::instance()
   *     .create_serializer(SerializationFormat::CEREAL_BINARY);
   * \endcode
   */
  std::unique_ptr<IMetadataSerializer> create_serializer(
      SerializationFormat format) {
    if (format == SerializationFormat::AUTO_DETECT) {
      throw std::invalid_argument(
          "Cannot create serializer with AUTO_DETECT format. "
          "Use detect_format() first, then create with the detected format.");
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = format_map_.find(format);
    if (it == format_map_.end()) {
      throw std::runtime_error(
          "No serializer registered for format: " +
          std::string(get_format_name(format)));
    }

    const auto& reg = serializers_[it->second];
    return reg.creator();
  }

  /**
   * Create a serializer by name
   *
   * Factory method that creates a serializer instance by format name.
   *
   * \param name Format identifier
   * \return Unique pointer to the serializer instance
   *
   * \throws std::runtime_error if format name is not registered
   */
  std::unique_ptr<IMetadataSerializer> create_serializer(
      const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = serializers_.find(name);
    if (it == serializers_.end()) {
      throw std::runtime_error("No serializer registered with name: " + name);
    }

    return it->second.creator();
  }

  /**
   * Detect format from binary data
   *
   * Examines magic bytes to determine the serialization format.
   * Checks formats in priority order (highest priority first).
   *
   * \param data Binary data to analyze
   * \return Detected serialization format
   *
   * \throws std::invalid_argument if data is too small
   * \throws std::runtime_error if format cannot be determined
   *
   * \example
   * \code
   * std::vector<uint8_t> data = {0xCE, 0xEA, 0x01, ...};
   * auto format = SerializerRegistry::instance().detect_format(data);
   * // Returns SerializationFormat::CEREAL_BINARY
   * \endcode
   */
  SerializationFormat detect_format(const std::vector<uint8_t>& data) {
    if (data.size() < magic_bytes::MIN_DETECTION_SIZE) {
      throw std::invalid_argument(
          "Data too small for format detection (need at least " +
          std::to_string(magic_bytes::MIN_DETECTION_SIZE) + " bytes, got " +
          std::to_string(data.size()) + ")");
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Get all registrations sorted by priority
    std::vector<SerializerRegistration> sorted_regs;
    for (const auto& [name, reg] : serializers_) {
      sorted_regs.push_back(reg);
    }

    std::sort(sorted_regs.begin(), sorted_regs.end(),
              [](const SerializerRegistration& a,
                 const SerializerRegistration& b) {
                return a.priority > b.priority; // Higher priority first
              });

    // Check each format in priority order
    for (const auto& reg : sorted_regs) {
      if (matches_magic_bytes(data, reg.magic_bytes)) {
        return reg.format;
      }
    }

    // Unknown format
    throw std::runtime_error(
        "Unknown serialization format: magic bytes [0x" + to_hex(data[0]) +
        " 0x" + to_hex(data[1]) + " 0x" + to_hex(data[2]) + "]");
  }

  /**
   * Get all available formats
   *
   * Returns a list of all registered format identifiers.
   *
   * \return Vector of format names
   */
  std::vector<std::string> get_available_formats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> formats;
    for (const auto& [name, reg] : serializers_) {
      formats.push_back(name);
    }

    return formats;
  }

  /**
   * Check if a format is registered
   *
   * \param format Format to check
   * \return true if format is registered, false otherwise
   */
  bool is_registered(SerializationFormat format) const {
    if (format == SerializationFormat::AUTO_DETECT) {
      return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return format_map_.find(format) != format_map_.end();
  }

  /**
   * Check if a format name is registered
   *
   * \param name Format name to check
   * \return true if format is registered, false otherwise
   */
  bool is_registered(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return serializers_.find(name) != serializers_.end();
  }

  // Delete copy and move constructors/operators
  SerializerRegistry(const SerializerRegistry&) = delete;
  SerializerRegistry& operator=(const SerializerRegistry&) = delete;
  SerializerRegistry(SerializerRegistry&&) = delete;
  SerializerRegistry& operator=(SerializerRegistry&&) = delete;

private:
  /**
   * Private constructor for singleton
   */
  SerializerRegistry() = default;

  /**
   * Check if data matches magic bytes
   *
   * \param data Binary data to check
   * \param magic_bytes Magic bytes to match
   * \return true if matches, false otherwise
   */
  static bool matches_magic_bytes(const std::vector<uint8_t>& data,
                                   const std::vector<uint8_t>& magic_bytes) {
    if (data.size() < magic_bytes.size()) {
      return false;
    }

    for (std::size_t i = 0; i < magic_bytes.size(); ++i) {
      if (data[i] != magic_bytes[i]) {
        return false;
      }
    }

    return true;
  }

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

  mutable std::mutex mutex_; ///< Thread safety
  std::unordered_map<std::string, SerializerRegistration>
      serializers_;                                          ///< Registered serializers by name
  std::unordered_map<SerializationFormat, std::string> format_map_; ///< Format enum to name mapping
};

} // namespace dwarfs::metadata::serialization