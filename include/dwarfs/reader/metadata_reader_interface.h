/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file metadata_reader_interface.h
 *
 * Format-Agnostic Metadata Reader Interface (Strategy Pattern)
 *
 * This interface abstracts metadata reading operations from the underlying
 * serialization format (Thrift or FlatBuffers). Implementations provide
 * format-specific reading logic while the application code works with
 * the domain model only.
 *
 * Design Pattern: Strategy Pattern
 * - Interface defines the contract for reading operations
 * - Concrete strategies implement format-specific reading
 * - Factory creates appropriate reader based on format detection
 *
 * \author Ribose Inc.
 * \date 2025-12-22
 * \copyright See LICENSE file
 */

#pragma once

#include "dwarfs/metadata/domain/metadata.h"
#include <memory>
#include <string_view>

namespace dwarfs::reader {

/**
 * Abstract interface for metadata reading operations
 *
 * This interface provides format-agnostic access to metadata stored in
 * DwarFS images. Concrete implementations handle Thrift or FlatBuffers
 * serialization formats, allowing the rest of the codebase to work with
 * the domain model only.
 */
class metadata_reader_interface {
public:
  virtual ~metadata_reader_interface() = default;

  /**
   * Read complete metadata from the serialized form
   *
   * @return Complete metadata in domain model format
   */
  virtual metadata::domain::metadata read() = 0;

  /**
   * Get a specific chunk by index
   *
   * @param index Chunk index
   * @return Domain model chunk
   */
  virtual metadata::domain::chunk get_chunk(size_t index) = 0;

  /**
   * Get a specific directory by index
   *
   * @param index Directory index
   * @return Domain model directory
   */
  virtual metadata::domain::directory get_directory(size_t index) = 0;

  /**
   * Get a specific inode by index
   *
   * @param index Inode index
   * @return Domain model inode_data
   */
  virtual metadata::domain::inode_data get_inode(size_t index) = 0;

  /**
   * Get a specific directory entry by index
   *
   * @param index Directory entry index
   * @return Domain model dir_entry
   */
  virtual metadata::domain::dir_entry get_dir_entry(size_t index) = 0;

  /**
   * Get the serialization format name for debugging/logging
   *
   * @return Format name (e.g., "Thrift", "FlatBuffers")
   */
  virtual std::string_view get_format_name() const = 0;
};

} // namespace dwarfs::reader