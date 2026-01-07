/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file metadata_writer_interface.h
 *
 * Format-Agnostic Metadata Writer Interface (Strategy Pattern)
 *
 * This interface abstracts metadata writing operations from the underlying
 * serialization format (Thrift or FlatBuffers). Implementations provide
 * format-specific writing logic while the application code works with
 * the domain model only.
 *
 * Design Pattern: Strategy Pattern + Builder Pattern
 * - Interface defines the contract for writing operations
 * - Concrete strategies implement format-specific serialization
 * - Builder pattern for incremental construction
 * - Factory creates appropriate writer based on configuration
 *
 * \author Ribose Inc.
 * \date 2025-12-22
 * \copyright See LICENSE file
 */

#pragma once

#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/byte_buffer.h"
#include <memory>
#include <string_view>

namespace dwarfs::writer {

/**
 * Abstract interface for metadata writing operations
 *
 * This interface provides format-agnostic serialization of metadata to
 * DwarFS images. Concrete implementations handle Thrift or FlatBuffers
 * serialization formats, allowing the rest of the codebase to work with
 * the domain model only.
 */
class metadata_writer_interface {
public:
  virtual ~metadata_writer_interface() = default;

  /**
   * Serialize complete metadata to byte buffer
   *
   * Takes domain model metadata and serializes it to the format-specific
   * wire format.
   *
   * @param meta Domain model metadata to serialize
   * @return Byte buffer containing serialized metadata
   */
  virtual mutable_byte_buffer serialize(const metadata::domain::metadata& meta) = 0;

  /**
   * Get the serialization format name for debugging/logging
   *
   * @return Format name (e.g., "Thrift", "FlatBuffers")
   */
  virtual std::string_view get_format_name() const = 0;
};

} // namespace dwarfs::writer