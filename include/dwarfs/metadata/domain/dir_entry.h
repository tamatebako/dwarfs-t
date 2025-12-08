#pragma once

#include <cstdint>

namespace dwarfs::metadata::domain {

/**
 * Represents a directory entry (file or subdirectory).
 *
 * Single Responsibility: Link names to inodes
 * Dependencies: None (uses only C++ stdlib)
 *
 * A directory entry connects a filename (via name_index) to
 * inode metadata (via inode_num), forming the filesystem's
 * name-to-inode mapping.
 */
class dir_entry {
public:
  /**
   * Construct a directory entry with name and inode references
   *
   * @param name_index Index into names[] vector
   * @param inode_num Index into inodes[] vector
   */
  dir_entry(uint32_t name_index, uint32_t inode_num)
      : name_index_(name_index), inode_num_(inode_num) {}

  /**
   * Default constructor for empty entry
   */
  dir_entry() : name_index_(0), inode_num_(0) {}

  // Accessors (const)
  uint32_t name_index() const { return name_index_; }
  uint32_t inode_num() const { return inode_num_; }

  // Mutators for writer (allows incremental construction)
  void set_name_index(uint32_t name_index) { name_index_ = name_index; }
  void set_inode_num(uint32_t inode_num) { inode_num_ = inode_num; }

  // Direct field access (alternative to mutators)
  uint32_t& name_index_ref() { return name_index_; }
  uint32_t& inode_num_ref() { return inode_num_; }

  /**
   * Equality comparison for testing and validation
   *
   * @param other Entry to compare with
   * @return true if all fields are equal
   */
  bool operator==(const dir_entry& other) const {
    return name_index_ == other.name_index_
        && inode_num_ == other.inode_num_;
  }

  bool operator!=(const dir_entry& other) const {
    return !(*this == other);
  }

private:
  uint32_t name_index_;  // Index into names[] vector
  uint32_t inode_num_;   // Index into inodes[] vector
};

} // namespace dwarfs::metadata::domain