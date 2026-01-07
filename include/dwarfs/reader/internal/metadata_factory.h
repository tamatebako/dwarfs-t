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

namespace metadata::domain {
  class metadata;
}
}

namespace dwarfs::reader::internal {

/**
 * Factory for loading metadata using SerializerRegistry
 *
 * This factory uses the Strategy Pattern via SerializerRegistry to:
 * - Auto-detect metadata format from magic bytes
 * - Create appropriate serializer for the format
 * - Deserialize to domain::metadata
 *
 * Design Principles:
 * - Single Responsibility: Only loads metadata
 * - Open/Closed: Extensible via SerializerRegistry
 * - Dependency Inversion: Depends on SerializerRegistry abstraction
 * - MECE: No format logic here (all in serializers)
 *
 * Clean Architecture:
 * - NO backend classes
 * - NO format-specific code paths
 * - Uses ONLY SerializerRegistry
 * - Returns domain::metadata directly
 */
class metadata_factory {
 public:
  /**
   * Load metadata from raw data
   *
   * Automatically detects format using SerializerRegistry and
   * deserializes to domain::metadata.
   *
   * @param lgr Logger for diagnostics
   * @param data Raw metadata section data
   * @return Unique pointer to domain::metadata
   * @throws runtime_error if format detection fails or deserialization fails
   */
  static std::unique_ptr<dwarfs::metadata::domain::metadata>
  load_metadata(logger& lgr, std::span<uint8_t const> data);
};

} // namespace dwarfs::reader::internal