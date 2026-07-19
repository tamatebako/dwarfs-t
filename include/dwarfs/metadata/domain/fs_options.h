#pragma once

#include <cstdint>
#include <optional>

namespace dwarfs::metadata::domain {

/**
 * Filesystem creation and feature options.
 *
 * Single Responsibility: Hold filesystem configuration flags
 * Dependencies: None (uses only C++ stdlib)
 *
 * Records which optional features and optimizations were enabled
 * when the filesystem was created, affecting how metadata is
 * stored and interpreted.
 */
class fs_options {
public:
  /**
   * Default constructor with standard defaults
   */
  fs_options() = default;

  // Feature flags
  bool mtime_only = false;
  bool packed_chunk_table = false;
  bool packed_directories = false;
  bool packed_shared_files_table = false;
  bool has_btime = false;
  bool inodes_have_nlink = false;

  // Optional resolution settings
  std::optional<uint32_t> time_resolution_sec;
  std::optional<uint32_t> subsecond_resolution_nsec_multiplier;

  /**
   * Equality comparison for testing and validation
   *
   * @param other Options to compare with
   * @return true if all fields are equal
   */
  bool operator==(const fs_options& other) const {
    return mtime_only == other.mtime_only
        && time_resolution_sec == other.time_resolution_sec
        && packed_chunk_table == other.packed_chunk_table
        && packed_directories == other.packed_directories
        && packed_shared_files_table == other.packed_shared_files_table
        && subsecond_resolution_nsec_multiplier == other.subsecond_resolution_nsec_multiplier
        && has_btime == other.has_btime
        && inodes_have_nlink == other.inodes_have_nlink;
  }

  bool operator!=(const fs_options& other) const {
    return !(*this == other);
  }
};

} // namespace dwarfs::metadata::domain