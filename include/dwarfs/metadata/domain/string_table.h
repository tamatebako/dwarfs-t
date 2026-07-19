#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace dwarfs::metadata::domain {

/**
 * String table with optional compression.
 *
 * Single Responsibility: Store and index strings efficiently
 * Dependencies: None (uses only C++ stdlib)
 *
 * Stores strings in a concatenated buffer with an index for lookups.
 * Optionally uses FSST compression for space efficiency.
 */
class string_table {
public:
  /**
   * Default constructor for empty table
   */
  string_table() = default;

  // Concatenated string data
  std::string buffer;

  // Optional FSST symbol table for compression
  std::optional<std::string> symtab;

  // Offsets into buffer for each string
  std::vector<uint32_t> index;

  // Whether index uses packed encoding
  bool packed_index = false;

  /**
   * Equality comparison for testing and validation
   *
   * @param other Table to compare with
   * @return true if all fields are equal
   */
  bool operator==(const string_table& other) const {
    return buffer == other.buffer
        && symtab == other.symtab
        && index == other.index
        && packed_index == other.packed_index;
  }

  bool operator!=(const string_table& other) const {
    return !(*this == other);
  }
};

} // namespace dwarfs::metadata::domain