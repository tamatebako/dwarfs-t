#pragma once

#include <cstdint>
#include <optional>

namespace dwarfs::metadata::domain {

/**
 * Inode metadata for files and directories.
 *
 * Single Responsibility: Hold inode attributes and timestamps
 * Dependencies: None (uses only C++ stdlib)
 *
 * Contains all metadata for a filesystem inode including permissions,
 * ownership, timestamps, and compatibility fields for older versions.
 */
class inode_data {
public:
  /**
   * Default constructor with zero values
   */
  inode_data() = default;

  // Lookup table indices
  uint32_t mode_index = 0;    // Index into modes[] vector
  uint32_t owner_index = 0;   // Index into uids[] vector
  uint32_t group_index = 0;   // Index into gids[] vector

  // Timestamp offsets (relative to timestamp_base)
  uint64_t atime_offset = 0;
  uint64_t mtime_offset = 0;
  uint64_t ctime_offset = 0;
  uint64_t btime_offset = 0;  // Birth time (optional)

  // Subsecond timestamp components
  uint64_t atime_subsec = 0;
  uint64_t mtime_subsec = 0;
  uint64_t ctime_subsec = 0;
  uint64_t btime_subsec = 0;

  // Link count (stored as nlink-1 for space efficiency)
  uint32_t nlink_minus_one = 0;

  // Deprecated v2.2 compatibility fields
  std::optional<uint32_t> name_index_v2_2;
  std::optional<uint32_t> inode_v2_2;

  /**
   * Equality comparison for testing and validation
   *
   * @param other Inode data to compare with
   * @return true if all fields are equal
   */
  bool operator==(const inode_data& other) const {
    return mode_index == other.mode_index
        && owner_index == other.owner_index
        && group_index == other.group_index
        && atime_offset == other.atime_offset
        && mtime_offset == other.mtime_offset
        && ctime_offset == other.ctime_offset
        && btime_offset == other.btime_offset
        && atime_subsec == other.atime_subsec
        && mtime_subsec == other.mtime_subsec
        && ctime_subsec == other.ctime_subsec
        && btime_subsec == other.btime_subsec
        && nlink_minus_one == other.nlink_minus_one
        && name_index_v2_2 == other.name_index_v2_2
        && inode_v2_2 == other.inode_v2_2;
  }

  bool operator!=(const inode_data& other) const {
    return !(*this == other);
  }
};

} // namespace dwarfs::metadata::domain