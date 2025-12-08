#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "dwarfs/metadata/domain/fs_options.h"

namespace dwarfs::metadata::domain {

/**
 * Metadata version history entry.
 *
 * Single Responsibility: Record filesystem version information
 * Dependencies: fs_options (domain type)
 *
 * Tracks the version and options used when creating or modifying
 * the filesystem, enabling version compatibility tracking.
 */
class history_entry {
public:
  /**
   * Default constructor with version 0.0
   */
  history_entry() = default;

  // Version numbers
  uint8_t major = 0;
  uint8_t minor = 0;

  // DwarFS version string used to create this version
  std::optional<std::string> dwarfs_version;

  // Block size at this version
  uint32_t block_size = 0;

  // Filesystem options at this version
  std::optional<fs_options> options;

  /**
   * Equality comparison for testing and validation
   *
   * @param other Entry to compare with
   * @return true if all fields are equal
   */
  bool operator==(const history_entry& other) const {
    return major == other.major
        && minor == other.minor
        && dwarfs_version == other.dwarfs_version
        && block_size == other.block_size
        && options == other.options;
  }

  bool operator!=(const history_entry& other) const {
    return !(*this == other);
  }
};

} // namespace dwarfs::metadata::domain