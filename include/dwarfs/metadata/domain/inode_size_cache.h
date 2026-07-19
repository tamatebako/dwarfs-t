#pragma once

#include <cstdint>
#include <map>

namespace dwarfs::metadata::domain {

/**
 * Performance cache for fragmented file sizes.
 *
 * Single Responsibility: Cache inode size lookups
 * Dependencies: None (uses only C++ stdlib)
 *
 * Provides fast size lookups for fragmented regular files,
 * avoiding expensive chunk table scans.
 */
class inode_size_cache {
public:
  /**
   * Default constructor for empty cache
   */
  inode_size_cache() = default;

  // Map from inode index to logical file size
  std::map<uint32_t, uint64_t> size_lookup;

  // Minimum number of chunks before caching is used
  uint64_t min_chunk_count = 0;

  // Map from inode index to allocated (physical) size
  std::map<uint32_t, uint64_t> allocated_size_lookup;

  /**
   * Equality comparison for testing and validation
   *
   * @param other Cache to compare with
   * @return true if all fields are equal
   */
  bool operator==(const inode_size_cache& other) const {
    return size_lookup == other.size_lookup
        && min_chunk_count == other.min_chunk_count
        && allocated_size_lookup == other.allocated_size_lookup;
  }

  bool operator!=(const inode_size_cache& other) const {
    return !(*this == other);
  }
};

} // namespace dwarfs::metadata::domain