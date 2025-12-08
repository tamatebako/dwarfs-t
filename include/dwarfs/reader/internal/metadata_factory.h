/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Ribose Inc.
 *
 * This file is part of dwarfs.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <memory>
#include <span>

namespace dwarfs {
class logger;
}

namespace dwarfs::reader::internal {

// Forward declarations
class global_metadata_interface;
class inode_view_interface;
class dir_entry_view_interface;

/**
 * Metadata format enumeration
 *
 * Identifies the serialization format used for metadata storage.
 */
enum class metadata_format {
  flatbuffers,  ///< FlatBuffers format (modern default, required)
  thrift        ///< Thrift Compact format (legacy, optional)
};

/**
 * Factory for creating metadata backend instances
 *
 * This factory implements the Strategy Pattern to create appropriate
 * backend instances based on the detected or specified format.
 *
 * Design Pattern: Factory Method + Strategy
 * Purpose: Decouple metadata creation from specific backend implementations
 * Principle: Dependency Inversion (depend on abstract factory)
 *
 * Key Features:
 * - Automatic format detection via magic bytes
 * - Runtime polymorphism via interface pointers
 * - Support for multiple serialization formats
 * - Clean separation of format-specific logic
 */
class metadata_factory {
 public:
  /**
   * Detect metadata format from raw data
   *
   * @param data Raw metadata section data
   * @return Detected format, or flatbuffers as default
   */
  static metadata_format detect_format(std::span<uint8_t const> data);

  /**
   * Create global metadata instance from raw data
   *
   * Automatically detects format and creates appropriate backend.
   *
   * @param lgr Logger for diagnostics
   * @param data Raw metadata section data
   * @return Pointer to global_metadata_interface implementation
   * @throws runtime_error if format detection fails or data is invalid
   */
  static std::unique_ptr<global_metadata_interface>
  create_global_metadata(logger& lgr, std::span<uint8_t const> data);

  /**
   * Create global metadata instance with explicit format
   *
   * @param lgr Logger for diagnostics
   * @param data Raw metadata section data
   * @param format Explicit format to use
   * @return Pointer to global_metadata_interface implementation
   * @throws runtime_error if specified format is not available or data is invalid
   */
  static std::unique_ptr<global_metadata_interface>
  create_global_metadata(logger& lgr, std::span<uint8_t const> data,
                        metadata_format format);
};

} // namespace dwarfs::reader::internal