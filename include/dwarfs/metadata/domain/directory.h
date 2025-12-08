#pragma once

#include <cstdint>

namespace dwarfs::metadata::domain {

/**
 * Represents a directory structure in the filesystem.
 *
 * Single Responsibility: Hold directory metadata and relationships
 * Dependencies: None (uses only C++ stdlib)
 *
 * A directory contains indices that reference entries in the dir_entries
 * vector, forming the filesystem's directory hierarchy.
 */
class directory {
public:
  /**
   * Construct a directory with specified entry references
   *
   * @param parent_entry Index into dir_entries for parent directory
   * @param first_entry Index into dir_entries for first child
   * @param self_entry Index into dir_entries for this directory
   */
  directory(uint32_t parent_entry, uint32_t first_entry, uint32_t self_entry)
      : parent_entry_(parent_entry)
      , first_entry_(first_entry)
      , self_entry_(self_entry) {}

  /**
   * Default constructor for empty directory
   */
  directory() : parent_entry_(0), first_entry_(0), self_entry_(0) {}

  // Accessors (const)
  uint32_t parent_entry() const { return parent_entry_; }
  uint32_t first_entry() const { return first_entry_; }
  uint32_t self_entry() const { return self_entry_; }

  // Mutators for writer (allows incremental construction)
  void set_parent_entry(uint32_t parent_entry) { parent_entry_ = parent_entry; }
  void set_first_entry(uint32_t first_entry) { first_entry_ = first_entry; }
  void set_self_entry(uint32_t self_entry) { self_entry_ = self_entry; }

  // Direct field access (alternative to mutators)
  uint32_t& parent_entry_ref() { return parent_entry_; }
  uint32_t& first_entry_ref() { return first_entry_; }
  uint32_t& self_entry_ref() { return self_entry_; }

  /**
   * Equality comparison for testing and validation
   *
   * @param other Directory to compare with
   * @return true if all fields are equal
   */
  bool operator==(const directory& other) const {
    return parent_entry_ == other.parent_entry_
        && first_entry_ == other.first_entry_
        && self_entry_ == other.self_entry_;
  }

  bool operator!=(const directory& other) const {
    return !(*this == other);
  }

private:
  uint32_t parent_entry_;  // Index into dir_entries for parent
  uint32_t first_entry_;   // Index into dir_entries for first child
  uint32_t self_entry_;    // Index into dir_entries for this directory
};

} // namespace dwarfs::metadata::domain