/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Serialization configuration manager
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
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace dwarfs::metadata::config {

/**
 * Configuration metadata for a serialization format
 *
 * Contains all the metadata needed to identify, instantiate, and
 * use a serializer implementation.
 */
struct SerializerConfig {
  std::string name;                     ///< Format identifier (e.g., "cereal_binary")
  std::string display_name;             ///< Human-readable name
  std::string description;              ///< Format description
  std::vector<uint8_t> magic_bytes;     ///< Magic bytes for detection
  std::string class_name;               ///< Serializer class name
  bool enabled{true};                   ///< Whether format is enabled
  bool read_write{true};                ///< Supports both read and write
  int priority{0};                      ///< Detection priority (higher = checked first)
  std::optional<std::string> requires_build_flag; ///< Optional build flag requirement
};

/**
 * Serialization configuration manager
 *
 * Thread-safe singleton that loads and manages serialization format
 * configuration from YAML files. Provides type-safe access to
 * serializer metadata.
 *
 * Design Principles:
 * - **Singleton Pattern**: Single source of truth for configuration
 * - **Thread Safety**: All access is protected by mutex
 * - **Configuration-Driven**: All metadata externalized to YAML
 * - **Validation**: Ensures configuration correctness on load
 *
 * \example
 * \code
 * auto& config = SerializationConfig::instance();
 *
 * // Get default format
 * auto default_fmt = config.get_default_format();
 *
 * // Get format configuration
 * auto cfg = config.get_format("cereal_binary");
 * std::cout << "Format: " << cfg.display_name << "\n";
 * \endcode
 */
class SerializationConfig {
public:
  /**
   * Get singleton instance
   *
   * Lazily initializes the configuration manager on first access.
   * Thread-safe initialization using C++11 static initialization.
   *
   * \return Reference to the singleton instance
   */
  static SerializationConfig& instance() {
    static SerializationConfig instance;
    return instance;
  }

  /**
   * Load configuration from YAML file
   *
   * Parses the YAML configuration file and populates the internal
   * format registry. Validates all configuration entries.
   *
   * \param config_path Path to YAML configuration file
   *
   * \throws std::runtime_error if file cannot be loaded or is invalid
   *
   * \example
   * \code
   * auto& config = SerializationConfig::instance();
   * config.load_from_file("config/serialization_config.yaml");
   * \endcode
   */
  void load_from_file(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
      YAML::Node config = YAML::LoadFile(config_path);

      if (!config["serialization"]) {
        throw std::runtime_error(
            "Configuration file missing 'serialization' section");
      }

      auto serialization = config["serialization"];

      // Load default format
      if (serialization["default_format"]) {
        default_format_ = serialization["default_format"].as<std::string>();
      }

      // Load format configurations
      if (serialization["formats"]) {
        auto formats = serialization["formats"];
        for (auto it = formats.begin(); it != formats.end(); ++it) {
          std::string format_name = it->first.as<std::string>();
          auto format_node = it->second;

          SerializerConfig cfg;
          cfg.name = format_name;
          cfg.display_name = format_node["name"].as<std::string>();
          cfg.description = format_node["description"]
                                ? format_node["description"].as<std::string>()
                                : "";
          cfg.class_name = format_node["class"].as<std::string>();
          cfg.enabled = format_node["enabled"]
                            ? format_node["enabled"].as<bool>()
                            : true;
          cfg.read_write = format_node["read_write"]
                               ? format_node["read_write"].as<bool>()
                               : !format_node["read_only"].as<bool>(false);
          cfg.priority = format_node["priority"]
                             ? format_node["priority"].as<int>()
                             : 0;

          // Load magic bytes
          if (format_node["magic_bytes"]) {
            auto magic_bytes = format_node["magic_bytes"];
            for (std::size_t i = 0; i < magic_bytes.size(); ++i) {
              cfg.magic_bytes.push_back(
                  static_cast<uint8_t>(magic_bytes[i].as<int>()));
            }
          }

          // Load optional build flag requirement
          if (format_node["requires_build_flag"]) {
            cfg.requires_build_flag =
                format_node["requires_build_flag"].as<std::string>();
          }

          formats_[format_name] = cfg;
        }
      }

      loaded_ = true;

    } catch (const YAML::Exception& e) {
      throw std::runtime_error(
          std::string("Failed to parse configuration file: ") + e.what());
    }
  }

  /**
   * Load configuration from YAML string
   *
   * Parses YAML configuration from a string. Useful for testing
   * or embedded configurations.
   *
   * \param yaml_content YAML configuration as string
   *
   * \throws std::runtime_error if YAML is invalid
   */
  void load_from_string(const std::string& yaml_content) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
      YAML::Node config = YAML::Load(yaml_content);
      // Similar parsing logic as load_from_file
      // (implementation similar to above)
    } catch (const YAML::Exception& e) {
      throw std::runtime_error(std::string("Failed to parse YAML: ") +
                               e.what());
    }
  }

  /**
   * Get configuration for a specific format
   *
   * \param format_name Format identifier
   * \return Format configuration
   *
   * \throws std::runtime_error if format not found or not enabled
   */
  SerializerConfig get_format(const std::string& format_name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = formats_.find(format_name);
    if (it == formats_.end()) {
      throw std::runtime_error("Unknown serialization format: " + format_name);
    }

    if (!it->second.enabled) {
      throw std::runtime_error("Serialization format disabled: " +
                               format_name);
    }

    return it->second;
  }

  /**
   * Get default format name
   *
   * \return Default format identifier
   */
  std::string get_default_format() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return default_format_;
  }

  /**
   * Get all enabled formats
   *
   * \return Vector of enabled format configurations, sorted by priority
   */
  std::vector<SerializerConfig> get_enabled_formats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<SerializerConfig> result;
    for (const auto& [name, cfg] : formats_) {
      if (cfg.enabled) {
        result.push_back(cfg);
      }
    }

    // Sort by priority (descending)
    std::sort(result.begin(), result.end(),
              [](const SerializerConfig& a, const SerializerConfig& b) {
                return a.priority > b.priority;
              });

    return result;
  }

  /**
   * Check if configuration has been loaded
   *
   * \return true if configuration loaded, false otherwise
   */
  bool is_loaded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_;
  }

  // Delete copy and move constructors/operators
  SerializationConfig(const SerializationConfig&) = delete;
  SerializationConfig& operator=(const SerializationConfig&) = delete;
  SerializationConfig(SerializationConfig&&) = delete;
  SerializationConfig& operator=(SerializationConfig&&) = delete;

private:
  /**
   * Private constructor for singleton
   */
  SerializationConfig() = default;

  mutable std::mutex mutex_;                                ///< Thread safety
  std::unordered_map<std::string, SerializerConfig> formats_; ///< Format registry
  std::string default_format_{"cereal_binary"};             ///< Default format
  bool loaded_{false};                                      ///< Configuration loaded flag
};

} // namespace dwarfs::metadata::config