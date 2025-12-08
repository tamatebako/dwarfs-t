#pragma once

#include <cstdint>

namespace dwarfs::metadata::domain {

/**
 * Represents a chunk of compressed file data.
 *
 * Single Responsibility: Hold chunk metadata for data blocks
 * Dependencies: None (uses only C++ stdlib)
 *
 * A chunk represents a contiguous piece of file data within a
 * filesystem block, identified by block number, offset, and size.
 */
class chunk {
public:
  /**
   * Construct a chunk with specified location and size
   *
   * @param block Filesystem block number
   * @param offset Offset within block (bytes)
   * @param size Chunk size (bytes)
   */
  chunk(uint32_t block, uint32_t offset, uint32_t size)
      : block_(block), offset_(offset), size_(size) {}

  /**
   * Default constructor for empty chunk
   */
  chunk() : block_(0), offset_(0), size_(0) {}

  // Accessors (const)
  uint32_t block() const { return block_; }
  uint32_t offset() const { return offset_; }
  uint32_t size() const { return size_; }

  // Mutators for writer (allows incremental construction)
  void set_block(uint32_t block) { block_ = block; }
  void set_offset(uint32_t offset) { offset_ = offset; }
  void set_size(uint32_t size) { size_ = size; }

  // Direct field access (alternative to mutators)
  uint32_t& block_ref() { return block_; }
  uint32_t& offset_ref() { return offset_; }
  uint32_t& size_ref() { return size_; }

  /**
   * Equality comparison for testing and validation
   *
   * @param other Chunk to compare with
   * @return true if all fields are equal
   */
  bool operator==(const chunk& other) const {
    return block_ == other.block_
        && offset_ == other.offset_
        && size_ == other.size_;
  }

  bool operator!=(const chunk& other) const {
    return !(*this == other);
  }

private:
  uint32_t block_;   // Filesystem block number
  uint32_t offset_;  // Offset within block (bytes)
  uint32_t size_;    // Chunk size (bytes)
};

} // namespace dwarfs::metadata::domain