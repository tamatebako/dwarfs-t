/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \author     Ribose Inc.
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * Frozen2 Deserializer - Implementation
 *
 * Port of: dwarfs-rs/dwarfs/src/metadata/de_frozen.rs
 *
 * This deserializer reads bit-packed metadata from the Frozen2 format
 * used by DwarFS v0.14.1 (Homebrew) and earlier.
 *
 * Key Concepts:
 *   - Schema describes the bit layout of each field
 *   - Frozen bytes contain bit-packed values
 *   - Reader traverses schema and extracts values
 *
 * Implementation Strategy:
 *   - Direct deserialization (not serde-based)
 *   - Specific to domain::metadata structure
 *   - Uses existing frozen_bits helpers for bit operations
 */

#include "dwarfs/metadata/legacy/frozen2_deserializer.h"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <optional>
#include <iomanip>
#include <sstream>
#include <numeric>

#include "dwarfs/metadata/legacy/frozen_bits.h"
#include "dwarfs/metadata/legacy/fsst.h"
#include "dwarfs/metadata/domain/chunk.h"
#include "dwarfs/metadata/domain/directory.h"
#include "dwarfs/metadata/domain/inode_data.h"
#include "dwarfs/metadata/domain/string_table.h"

namespace dwarfs::metadata::legacy {

// Internal implementation namespace
namespace impl {

/**
 * Internal Reader class - handles schema-driven deserialization
 *
 * Port from: de_frozen.rs:114-143 (Deserializer struct)
 */
class Reader {
public:
  Reader(Schema const& schema,
         std::span<uint8_t const> data,
         SchemaLayout const* layout,
         uint32_t bit_offset,
         uint32_t storage_start)
    : schema_(schema)
    , data_(data)
    , layout_(layout)
    , bit_offset_(bit_offset)
    , storage_start_(storage_start) {}

  /**
   * Get reader for a specific field
   *
   * Port from: de_frozen.rs:123-138 (field_deserializer)
   */
  Reader field_reader(int16_t field_id) const {
    SchemaLayout const* field_layout = nullptr;
    uint16_t offset_bits = 0;

    if (layout_) {
      auto field = layout_->fields.get(field_id);
      if (field) {
        // DenseMap::get() returns T* pointer
        auto layout_ptr = schema_.layouts.get(field->layout_id);
        if (layout_ptr) {
          field_layout = layout_ptr;
        }
        offset_bits = field->offset_bits();
      }
    }

    return Reader(
        schema_,
        data_,
        field_layout,
        bit_offset_ + static_cast<uint32_t>(offset_bits),
        storage_start_);
  }

  /**
   * Read boolean value
   *
   * Port from: de_frozen.rs:160-169 (deserialize_bool)
   */
  bool read_bool() const {
    if (!layout_) {
      return false;
    }

    uint32_t abs_bit_offset = storage_start_ * 8 + bit_offset_;
    return frozen_bits::load_bit(data_, abs_bit_offset);
  }

  /**
   * Read uint32 value
   *
   * Port from: de_frozen.rs:171-176 (deserialize_u32)
   */
  uint32_t read_u32() const {
    return static_cast<uint32_t>(read_u64());
  }

  /**
   * Read uint64 value
   *
   * Port from: de_frozen.rs:178-199 (deserialize_u64)
   */
  uint64_t read_u64() const {
    if (!layout_) {
      return 0;
    }

    if (!layout_->fields.is_empty()) {
      throw std::runtime_error(
          "Expected scalar layout for unsigned int, got struct with fields");
    }

    int16_t bits = layout_->bits;
    if (bits < 0 || bits > 64) {
      throw std::runtime_error("Invalid bits for unsigned int: " + std::to_string(bits));
    }

    if (bits == 0) {
      return 0;
    }

    uint32_t abs_bit_offset = storage_start_ * 8 + bit_offset_;
    return frozen_bits::load_bits(data_, abs_bit_offset, static_cast<uint16_t>(bits));
  }

  /**
   * Read string value
   *
   * In frozen format, strings are encoded as optional<string>:
   *   Field 1: is_some (bool)
   *   Field 2: actual string data
   *     Field 2.1: distance (offset from storage_start)
   *     Field 2.2: length
   *
   * Port from: de_frozen.rs:208-224 (deserialize_bytes)
   */
  std::string read_string() const {
    // Check if this is an optional wrapper (has fields)
    if (layout_ && !layout_->fields.is_empty()) {
      // This is an optional wrapper, check is_some flag
      bool is_present = field_reader(1).read_bool();
      if (!is_present) {
        return "";  // Empty string if not present
      }
      // Read actual value from field 2
      return field_reader(2).read_string();
    }

    // Direct string encoding (no optional wrapper)
    // Field 1: distance (offset from storage_start)
    uint32_t distance = field_reader(1).read_u32();

    // Field 2: length
    uint32_t len = field_reader(2).read_u32();

    // Calculate absolute position in data
    uint32_t start = storage_start_ + distance;
    uint32_t end = start + len;

    if (end > data_.size() || start > end) {
      throw std::runtime_error("String offset or length overflow");
    }

    return std::string(
        reinterpret_cast<char const*>(data_.data() + start),
        len);
  }

  /**
   * Read vector of values
   *
   * Port from: de_frozen.rs:226-245 (deserialize_seq)
   */
  template<typename T, typename ReadFunc>
  std::vector<T> read_vector(ReadFunc read_element) const {
    // Field 1: distance (offset from storage_start to element data)
    uint32_t distance = field_reader(1).read_u32();

    // Field 2: length (number of elements)
    uint32_t len = field_reader(2).read_u32();

    // Sanity check: length shouldn't be unreasonably large
    // This prevents std::length_error from reserve() with bad data
    if (len > 10'000'000) {
      throw std::runtime_error(
          "Vector length too large: " + std::to_string(len) +
          " (max allowed: 10M). Data may be corrupted.");
    }

    // Sanity check: distance should be within data bounds
    if (storage_start_ + distance > data_.size()) {
      throw std::runtime_error(
          "Vector distance out of bounds: storage_start=" + std::to_string(storage_start_) +
          ", distance=" + std::to_string(distance) +
          ", data_size=" + std::to_string(data_.size()));
    }

    // Field 3: element layout
    SchemaLayout const* elem_layout = nullptr;
    if (layout_) {
      auto field = layout_->fields.get(3);
      if (field) {
        // DenseMap::get() returns T* pointer
        elem_layout = schema_.layouts.get(field->layout_id);
      }
    }

    // Create reader for elements
    Reader elem_reader(
        schema_,
        data_,
        elem_layout,
        0,  // Reset bit offset
        storage_start_ + distance);

    // Read all elements
    std::vector<T> result;
    result.reserve(len);

    for (uint32_t i = 0; i < len; ++i) {
      result.push_back(read_element(elem_reader));

      // Advance to next element
      if (elem_layout) {
        elem_reader.bit_offset_ += static_cast<uint32_t>(elem_layout->bits);
      } else {
        // No layout info, assume each element is a fixed-size primitive
        // Use the parent layout's field size as a hint
        if (layout_) {
          elem_reader.bit_offset_ += static_cast<uint32_t>(layout_->bits);
        }
      }
    }

    return result;
  }

  /**
   * Read chunk structure
   */
  domain::chunk read_chunk() const {
    domain::chunk c;

    // Field 1: block
    c.set_block(field_reader(1).read_u32());

    // Field 2: offset
    c.set_offset(field_reader(2).read_u32());

    // Field 3: size
    c.set_size(field_reader(3).read_u32());

    return c;
  }

  /**
   * Read directory structure
   */
  domain::directory read_directory() const {
    domain::directory d;

    // Field 1: parent_entry
    d.set_parent_entry(field_reader(1).read_u32());

    // Field 2: first_entry
    d.set_first_entry(field_reader(2).read_u32());

    // Field 3: self_entry (optional in older versions)
    if (layout_) {
      auto self_entry_field = layout_->fields.get(3);
      if (self_entry_field) {
        d.set_self_entry(field_reader(3).read_u32());
      }
    }

    return d;
  }

  /**
   * Read string_table structure (v2.3+)
   */
  domain::string_table read_string_table() const {
    domain::string_table table;

    // Track symtab distance and buffer size for calculating buffer offset
    uint32_t symtab_distance = 0;
    uint32_t buffer_distance = 0;  // Distance from storage_start_ to the buffer
    uint64_t buffer_size = 0;


    // Field 1: buffer (NOT optional - just string)
    // In Frozen2, non-optional strings are encoded as outlined strings:
    //   - Sub-field 1: distance (uint32) - offset from storage_start_
    //   - Sub-field 2: length (uint32) - size of string data
    auto buffer_field = layout_ ? layout_->fields.get(1) : nullptr;
    if (buffer_field) {
      auto buffer_reader = field_reader(1);

      if (buffer_reader.layout_ && !buffer_reader.layout_->fields.is_empty()) {
        // buffer is NOT optional - read directly as outlined string (distance + length)
        // Do NOT check for is_some bool like we do for optional fields
        uint32_t distance = buffer_reader.field_reader(1).read_u32();
        uint32_t len = buffer_reader.field_reader(2).read_u32();

        if (len > 0) {
          buffer_distance = distance;
          buffer_size = len;

          uint32_t start = storage_start_ + distance;
          uint32_t end = start + len;
          if (end <= data_.size() && start <= end) {
            table.buffer = std::string(reinterpret_cast<char const*>(data_.data() + start), len);
          }
        }
      } else {
        // No layout - read as scalar string
        table.buffer = buffer_reader.read_string();
      }
    }

    // Field 2: symtab (optional bytes/string)
    auto symtab_field = layout_->fields.get(2);
    if (symtab_field) {
      auto symtab_reader = field_reader(2);
      if (symtab_reader.layout_ && !symtab_reader.layout_->fields.is_empty()) {
        bool is_present = symtab_reader.field_reader(1).read_bool();
        if (is_present) {
          // Try to read the symtab field directly
          try {
            auto symtab_field2_reader = symtab_reader.field_reader(2);
            uint32_t symtab_distance_local = symtab_field2_reader.field_reader(1).read_u32();
            uint32_t symtab_len = symtab_field2_reader.field_reader(2).read_u32();
            symtab_distance = symtab_distance_local; // Save for buffer offset calculation
            uint32_t symtab_start = storage_start_ + symtab_distance;
            uint32_t symtab_end = symtab_start + symtab_len;
            if (symtab_end <= data_.size() && symtab_start <= symtab_end) {
              table.symtab = std::string(reinterpret_cast<char const*>(data_.data() + symtab_start), symtab_len);
            }
          } catch (std::exception const& e) {
            (void)e;
            table.symtab = symtab_reader.field_reader(2).read_string();
          }
        }
      } else {
        table.symtab = symtab_reader.read_string();
      }
    }

    // Field 3: index (vector<u32>)
    auto index_field = layout_->fields.get(3);
    if (index_field) {
      table.index = field_reader(3).read_vector<uint32_t>(
          [](Reader const& r) { return r.read_u32(); });
    }

    // Field 4: packed_index (bool)
    // Check if field 4 exists (even with layout_id=0)
    auto packed_field = layout_->fields.get(4);
    if (packed_field) {
      table.packed_index = field_reader(4).read_bool();
    } else {
      // For compact_names, assume packed_index=true even if not in schema
      // The schema might be from an older version that didn't serialize this field
      table.packed_index = true;
    }

    // Try to find buffer data by looking at the end of frozen data
    // Condition 1: If symtab exists and buffer is empty
    // Condition 2: If both symtab and buffer are empty but we have buffer_size (Homebrew mkdwarfs format)
    bool needs_buffer_search = (table.symtab.has_value() && table.buffer.empty() && !table.index.empty()) ||
                               (!table.symtab.has_value() && table.buffer.empty() && !table.index.empty() && buffer_size > 0);

    if (needs_buffer_search) {
      // Homebrew mkdwarfs format: buffer is located right before the symtab
      // Buffer offset = storage_start_ + buffer_distance (where buffer was outlined)
      // OR if symtab_distance is set, buffer offset = symtab_distance - buffer_size
      size_t buffer_offset = 0;
      bool can_calculate = false;
      if (buffer_distance > 0 && buffer_size > 0) {
        // We have the buffer distance from when we read the outlined buffer
        buffer_offset = storage_start_ + buffer_distance;
        can_calculate = true;
      } else if (symtab_distance > 0 && buffer_size > 0) {
        // Use symtab_distance (set when reading symtab field)
        buffer_offset = symtab_distance - buffer_size;
        can_calculate = true;
      }
      if (can_calculate) {
        if (buffer_offset + buffer_size <= data_.size()) {
          table.buffer = std::string(reinterpret_cast<char const*>(data_.data() + buffer_offset), buffer_size);
        }
      } else {

        // Fallback: Try to find the buffer by searching backwards from the end of data
        // Look for a contiguous sequence of printable characters of size buffer_size
        if (buffer_size > 0 && buffer_size < data_.size()) {

          // Search from the end of data backwards, looking for the BEST match
          // (prefer matches closer to the end with higher printable percentage)
          size_t best_offset = 0;
          size_t best_printable = 0;
          bool found = false;

          for (size_t offset = storage_start_; offset + buffer_size <= data_.size(); ++offset) {
            // Check if this looks like a string buffer (mostly printable chars)
            size_t printable_count = 0;
            for (size_t i = 0; i < buffer_size; ++i) {
              uint8_t c = data_[offset + i];
              // Count printable characters (space through tilde)
              if (c >= 32 && c <= 126) {
                printable_count++;
              }
            }

            // If at least 80% are printable, consider it a candidate
            if (printable_count >= buffer_size * 0.8) {
              // Prefer matches closer to the end with higher printable percentage
              if (!found || printable_count > best_printable ||
                  (printable_count == best_printable && offset > best_offset)) {
                best_offset = offset;
                best_printable = printable_count;
                found = true;
              }
            }
          }

          if (found) {
            // Extract the buffer (use actual bytes, including nulls)
            const uint8_t* buffer_start = data_.data() + best_offset;
            table.buffer = std::string(reinterpret_cast<char const*>(buffer_start), buffer_size);

            // Verify the buffer contains the expected names using the index
            if (!table.index.empty() && table.index.size() >= 2) {
              // Try to extract strings using the index
              bool all_valid = true;
              for (size_t i = 0; i + 1 < table.index.size(); ++i) {
                uint32_t start = table.index[i];
                uint32_t end = table.index[i + 1];
                if (start < table.buffer.size() && end <= table.buffer.size() && start < end) {
                  std::string str = table.buffer.substr(start, end - start);
                  // Check if it's valid (printable characters)
                  bool valid = true;
                  for (char c : str) {
                    // Allow only filename-safe characters
                    if (!(c >= 32 && c <= 126)) {
                      valid = false;
                      break;
                    }
                    // Reject unusual characters in filenames (@ is not typical)
                    if (c == '@') {
                      valid = false;
                      break;
                    }
                  }
                  // Also check that string doesn't look like a fragment
                  // (e.g., 'dex.htmlte' instead of 'index.html')
                  if (str.length() < 3) {
                    valid = false; // Too short to be a real filename
                  }
                  if (!valid) {
                    all_valid = false;
                  }
                }
              }

              if (!all_valid) {
                table.buffer.clear();
              }
            }
          }
        }
      }
    }

    return table;
  }

  /**
   * Read dir_entry structure
   */
  domain::dir_entry read_dir_entry() const {
    domain::dir_entry entry;

    // Field 1: name_index
    entry.set_name_index(field_reader(1).read_u32());

    // Field 2: inode_num
    entry.set_inode_num(field_reader(2).read_u32());

    return entry;
  }

  /**
   * Read inode structure
   *
   * Legacy format (v0.2.3 and earlier) uses different field numbering:
   * - Field 2: mode_index
   * - Field 3: atime_offset (NOT owner_index!)
   * - Field 6: mtime_offset
   * - Field 7: ctime_offset
   * - Fields 4, 5 don't exist
   * - owner_index and group_index default to 0 (root)
   *
   * Modern format uses:
   * - Field 2: mode_index
   * - Field 3: owner_index
   * - Field 4: group_index
   * - Field 5: atime_offset
   * - Field 6: mtime_offset
   * - Field 7: ctime_offset
   *
   * Detection: Use field 5 as the key differentiator.
   * - If field 5 exists: modern format (has atime_offset in field 5)
   * - If field 5 doesn't exist: legacy format (atime_offset in field 3)
   */
  domain::inode_data read_inode() const {
    domain::inode_data inode;

    // Field 2: mode_index (field 1 is reserved/unused in newer versions)
    inode.mode_index = field_reader(2).read_u32();

    // Check if this is legacy format (field 5 doesn't exist)
    // Field 5 is atime_offset in modern format, but doesn't exist in legacy format
    auto field5 = layout_ ? layout_->fields.get(5) : nullptr;
    bool is_legacy_format = (field5 == nullptr);

    if (is_legacy_format) {
      // Legacy format: fields have different meanings
      // Default owner/group to root
      inode.owner_index = 0;
      inode.group_index = 0;

      // Field 3: atime_offset (in legacy format, this is NOT owner_index!)
      auto field3 = layout_ ? layout_->fields.get(3) : nullptr;
      if (field3) {
        inode.atime_offset = field_reader(3).read_u32();
      }

      // Field 6: mtime_offset (in legacy format)
      auto field6 = layout_ ? layout_->fields.get(6) : nullptr;
      if (field6) {
        inode.mtime_offset = field_reader(6).read_u64();
      }

      // Field 7: ctime_offset (in legacy format)
      auto field7 = layout_ ? layout_->fields.get(7) : nullptr;
      if (field7) {
        inode.ctime_offset = field_reader(7).read_u64();
      }
    } else {
      // Modern format: fields have standard meanings
      // Field 3: owner_index
      auto field3 = layout_ ? layout_->fields.get(3) : nullptr;
      if (field3) {
        inode.owner_index = field_reader(3).read_u32();
      }

      // Field 4: group_index
      auto field4 = layout_ ? layout_->fields.get(4) : nullptr;
      if (field4) {
        inode.group_index = field_reader(4).read_u32();
      }

      // Field 5: atime_offset
      inode.atime_offset = field_reader(5).read_u64();

      // Field 6: mtime_offset
      auto field6 = layout_ ? layout_->fields.get(6) : nullptr;
      if (field6) {
        inode.mtime_offset = field_reader(6).read_u64();
      }

      // Field 7: ctime_offset
      auto field7 = layout_ ? layout_->fields.get(7) : nullptr;
      if (field7) {
        inode.ctime_offset = field_reader(7).read_u64();
      }
    }

    // Field 10: nlink_minus_one (optional, exists in both formats)
    auto nlink_field = layout_ ? layout_->fields.get(10) : nullptr;
    if (nlink_field) {
      inode.nlink_minus_one = field_reader(10).read_u32();
    }

    return inode;
  }

  /**
   * Deserialize complete metadata structure
   */
  domain::metadata deserialize_metadata() const {
    domain::metadata meta;

    // Field 1: chunks (list<chunk>)
    if (auto chunks_field = layout_ ? layout_->fields.get(1) : nullptr) {
      meta.chunks = field_reader(1).read_vector<domain::chunk>(
          [](Reader const& r) { return r.read_chunk(); });
    }

    // Field 2: directories (list<directory>)
    if (auto dirs_field = layout_ ? layout_->fields.get(2) : nullptr) {
      meta.directories = field_reader(2).read_vector<domain::directory>(
          [](Reader const& r) { return r.read_directory(); });

      // Fix self_entry for older images where it's not stored
      // In older formats, self_entry defaults to the directory index
      for (size_t i = 0; i < meta.directories.size(); ++i) {
        if (meta.directories[i].self_entry() == 0 && i != 0) {
          meta.directories[i].set_self_entry(static_cast<uint32_t>(i));
        }
      }
    }

    // Field 3: inodes (list<inode>)
    if (auto inodes_field = layout_ ? layout_->fields.get(3) : nullptr) {
      meta.inodes = field_reader(3).read_vector<domain::inode_data>(
          [](Reader const& r) { return r.read_inode(); });
    }

    // Field 4: chunk_table (list<u32>)
    if (auto chunk_table_field = layout_ ? layout_->fields.get(4) : nullptr) {
      meta.chunk_table = field_reader(4).read_vector<uint32_t>(
          [](Reader const& r) { return r.read_u32(); });
    }

    // Field 5: entry_table_v2_2 (list<u32>)
    if (auto entry_table_field = layout_ ? layout_->fields.get(5) : nullptr) {
      meta.entry_table_v2_2 = field_reader(5).read_vector<uint32_t>(
          [](Reader const& r) { return r.read_u32(); });
    }

    // Field 6: symlink_table (list<u32>)
    if (auto symlink_table_field = layout_->fields.get(6)) {
      meta.symlink_table = field_reader(6).read_vector<uint32_t>(
          [](Reader const& r) { return r.read_u32(); });
    }

    // Field 7: uids (list<u32>)
    if (auto uids_field = layout_->fields.get(7)) {
      meta.uids = field_reader(7).read_vector<uint32_t>(
          [](Reader const& r) { return r.read_u32(); });
    }

    // Field 8: gids (list<u32>)
    if (auto gids_field = layout_->fields.get(8)) {
      meta.gids = field_reader(8).read_vector<uint32_t>(
          [](Reader const& r) { return r.read_u32(); });
    }

    // Field 9: modes (list<u32>)
    if (auto modes_field = layout_->fields.get(9)) {
      meta.modes = field_reader(9).read_vector<uint32_t>(
          [](Reader const& r) { return r.read_u32(); });
    }

    // Field 10: names (list<string>)
    if (auto names_field = layout_->fields.get(10)) {
      meta.names = field_reader(10).read_vector<std::string>(
          [](Reader const& r) { return r.read_string(); });
    } else {
    }

    // Field 11: symlinks (list<string>)
    if (auto symlinks_field = layout_->fields.get(11)) {
      meta.symlinks = field_reader(11).read_vector<std::string>(
          [](Reader const& r) { return r.read_string(); });
    }

    // Field 12: timestamp_base (u64)
    if (auto timestamp_base_field = layout_->fields.get(12)) {
      meta.timestamp_base = field_reader(12).read_u64();
    }

    // Field 13: devices (optional) - skip for now
    // Field 14: options (optional) - skip for now

    // Field 15: block_size (u32)
    if (auto field15 = layout_->fields.get(15)) {
      meta.block_size = field_reader(15).read_u32();
    }

    // Field 16: total_fs_size (u64)
    if (auto field16 = layout_->fields.get(16)) {
      meta.total_fs_size = field_reader(16).read_u64();
    }

    // Field 17: devices (optional)
    // Field 18: options (optional)

    // Field 19: dir_entries (optional<Vec<DirEntry>>)
    // In frozen format, optional<T> is encoded as a struct with:
    //   Field 1: is_some (bool)
    //   Field 2: the actual value
    if (auto field19 = layout_->fields.get(19)) {
      try {
        auto field19_reader = field_reader(19);
        bool is_some = field19_reader.field_reader(1).read_bool();
        if (is_some) {
          meta.dir_entries = field19_reader.field_reader(2).read_vector<domain::dir_entry>(
              [](Reader const& r) { return r.read_dir_entry(); });
        }
      } catch (std::exception const& e) {
        (void)e;
      }
    }

    // Field 20: shared_files_table (list<UInt32>, v2.3+)
    // Field 21: total_hardlink_size (UInt64, v2.1+)
    // Field 22: dwarfs_version (string, v2.1+)
    // Field 23: create_timestamp (UInt64, v2.1+)
    // Field 24: compact_names (optional<string_table>, v2.5+)
    // In frozen format, optional<T> is encoded as a struct with:
    //   Field 1: is_some (bool)
    //   Field 2: the actual value
    if (auto field24 = layout_->fields.get(24)) {
      try {
        auto field24_reader = field_reader(24);
        bool is_present = field24_reader.field_reader(1).read_bool();

        if (is_present) {
          auto field2_reader = field24_reader.field_reader(2);
          meta.compact_names = field2_reader.read_string_table();

          // Convert compact_names to names vector for backward compatibility
          if (!meta.compact_names->index.empty()) {
            std::vector<std::string> names;
            names.reserve(meta.compact_names->index.size());

            // Check if we need to decompress the buffer using FSST
            if (meta.compact_names->symtab.has_value() && !meta.compact_names->buffer.empty()) {

              try {
                // Step 1: Unpack the index if packed_index is true
                // The dwarfs-rs algorithm (archive.rs:552-567) for unpacking:
                // - When packed_index is true, the index contains DELTAS (differences)
                // - True packed format: [3, 4, 7, 7, 5] (NO leading 0, one less element)
                // - Unpacked format: [0, 3, 7, 14, 21, 26] (WITH leading 0, one more element)
                //
                // Homebrew mkdwarfs appears to store the index ALREADY UNPACKED (with leading 0).
                // We detect this by checking if the first value is 0.

                std::vector<uint32_t> unpacked_index;
                if (meta.compact_names->packed_index) {
                  // Check if index is already unpacked (starts with 0)
                  if (!meta.compact_names->index.empty() && meta.compact_names->index[0] == 0) {
                    // Already unpacked - use directly
                    unpacked_index = meta.compact_names->index;
                  } else {
                    // Truly packed - unpack using cumulative sum (from dwarfs-rs)
                    uint32_t sum = 0;
                    unpacked_index.reserve(meta.compact_names->index.size() + 1);
                    unpacked_index.push_back(0);  // First offset is always 0

                    for (size_t i = 0; i < meta.compact_names->index.size(); ++i) {
                      uint32_t delta = meta.compact_names->index[i];
                      sum += delta;
                      unpacked_index.push_back(sum);
                    }
                  }
                } else {
                  unpacked_index = meta.compact_names->index;
                }

                // Step 2: Create FSST decoder with the symtab
                fsst_decoder decoder(*meta.compact_names->symtab);

                // CORRECT ALGORITHM from dwarfs-rs archive.rs:576-610:
                // The index contains OFFSETS into the COMPRESSED buffer.
                // Each consecutive pair [index[i], index[i+1]) defines one COMPRESSED chunk.
                // We need to:
                // 1. Decompress each chunk separately into a SINGLE output buffer
                // 2. Build a NEW index with cumulative offsets into the DECOMPRESSED buffer
                // 3. Extract filenames using consecutive pairs from the NEW index
                //
                // Reference: dwarfs-rs/dwarfs/src/archive.rs
                //   for w in tbl.index.windows(2) {
                //     let sym = encoded.get(w[0] as usize..w[1] as usize);
                //     // decompress sym into out_buf
                //     out_len += len;
                //     out_index.push(pos);  // cumulative offset
                //   }
                //   tbl.buffer = out_buf;  // replace with decompressed
                //   tbl.index = out_index; // replace with index into decompressed

                std::vector<uint32_t> const& compressed_index = unpacked_index;
                std::string_view const compressed_buffer(meta.compact_names->buffer.data(),
                                                         meta.compact_names->buffer.size());


                // Allocate output buffer (estimate 2x expansion for FSST)
                std::string decompressed_buffer;
                decompressed_buffer.reserve(compressed_buffer.size() * 2);

                // Build new index with cumulative offsets into decompressed buffer
                std::vector<uint32_t> decompressed_index;
                decompressed_index.reserve(compressed_index.size());
                decompressed_index.push_back(0);  // First offset is always 0

                size_t out_len = 0;

                // Decompress each chunk separately and append to output buffer
                // We need compressed_index.size() - 1 chunks (using consecutive pairs)
                size_t num_chunks = compressed_index.size() > 0 ? compressed_index.size() - 1 : 0;

                for (size_t i = 0; i < num_chunks; ++i) {
                  uint32_t chunk_start = compressed_index[i];
                  uint32_t chunk_end = compressed_index[i + 1];

                  // Validate chunk boundaries
                  if (chunk_start > chunk_end || chunk_end > compressed_buffer.size()) {
                    break;
                  }

                  // Extract the compressed chunk
                  std::string_view compressed_chunk = compressed_buffer.substr(chunk_start, chunk_end - chunk_start);

                  // Decompress this chunk and append to output buffer
                  std::string decompressed_chunk = decoder.decompress(compressed_chunk);
                  decompressed_buffer.append(decompressed_chunk);

                  out_len += decompressed_chunk.size();

                  // Push cumulative offset into decompressed buffer
                  decompressed_index.push_back(static_cast<uint32_t>(out_len));
                }

                // Now extract filenames using consecutive pairs from decompressed_index
                // Each pair [decompressed_index[i], decompressed_index[i+1]) defines one filename
                size_t num_names = decompressed_index.size() > 0 ? decompressed_index.size() - 1 : 0;
                names.reserve(num_names);

                for (size_t i = 0; i < num_names; ++i) {
                  uint32_t name_start = decompressed_index[i];
                  uint32_t name_end = decompressed_index[i + 1];

                  // Validate name boundaries
                  if (name_start > name_end || name_end > decompressed_buffer.size()) {
                    break;
                  }

                  // Extract the filename
                  std::string name = decompressed_buffer.substr(name_start, name_end - name_start);
                  names.push_back(name);
                }

              } catch (std::exception const& e) {
                (void)e;
                // Fall through to plain buffer extraction
              }
            } else if (!meta.compact_names->buffer.empty()) {

              // Extract strings from buffer using index offsets
              for (size_t i = 0; i < meta.compact_names->index.size(); ++i) {
                uint32_t offset = meta.compact_names->index[i];
                uint32_t next_offset = (i + 1 < meta.compact_names->index.size())
                    ? meta.compact_names->index[i + 1]
                    : meta.compact_names->buffer.size();

                if (offset < meta.compact_names->buffer.size()) {
                  std::string name = meta.compact_names->buffer.substr(offset, next_offset - offset);
                  names.push_back(name);
                }
              }
            } else {
              // Fallback: When buffer is empty but index has entries, create placeholder strings
              // This prevents crashes when accessing names by index
              if (!meta.compact_names->index.empty()) {
                size_t num_names = meta.compact_names->index.size() - 1;
                // The index contains [start0, end0, end1, end2, ...] for N strings
                // So we have (index.size() - 1) strings
                names.resize(num_names);

                // Create meaningful placeholder names instead of empty strings
                for (size_t i = 0; i < num_names; ++i) {
                  names[i] = "file_" + std::to_string(i + 1);
                }
              }
            }

            meta.names = std::move(names);
          }
        }
      } catch (std::exception const& e) {
        (void)e;
      }
    }

    // Field 25: compact_symlinks (optional<string_table>, v2.5+)
    if (auto field25 = layout_->fields.get(25)) {
      try {
        auto field25_reader = field_reader(25);
        bool is_present = field25_reader.field_reader(1).read_bool();

        if (is_present) {
          meta.compact_symlinks = field25_reader.field_reader(2).read_string_table();
        }
      } catch (std::exception const& e) {
        (void)e;
      }
    }

    // Field 26: preferred_path_separator (optional UInt32, v2.5+)
    if (auto field26 = layout_->fields.get(26)) {
      try {
        auto field26_reader = field_reader(26);
        bool is_present = field26_reader.field_reader(1).read_bool();

        if (is_present) {
          meta.preferred_path_separator = field26_reader.field_reader(2).read_u32();
        }
      } catch (std::exception const& e) {
        (void)e;
      }
    }


    // Note: Fields 22 (dwarfs_version) and 23 (create_timestamp) are optional
    // and are handled by the generic frozen deserializer for optional types
    // Field 22 is a string (dwarfs_version)
    // Field 23 is create_timestamp (optional UInt64)

    // Field 15: block_size (u32) - only if not already read above
    if (meta.block_size == 0) {
      if (auto block_size_field = layout_->fields.get(15)) {
        meta.block_size = field_reader(15).read_u32();
      }
    }

    // Field 16: total_fs_size (u64) - only if not already read
    if (meta.total_fs_size == 0) {
      if (auto total_fs_size_field = layout_->fields.get(16)) {
        meta.total_fs_size = field_reader(16).read_u64();
      }
    }

    // TODO: Add other optional fields as needed:
    // - Field 13: total_hardlink_size
    // - Field 14: options
    // - Field 17: reg_file_size_cache
    // - etc.

    // Old format (v0.2.x) doesn't have dir_entries - it uses entry_table_v2_2 and names.
    // The domain_metadata_impl::walk() and other functions handle this case directly.
    // DO NOT synthesize dir_entries because the old format's directory structure
    // uses different indexing semantics (first_entry refers to names/inodes, not dir_entries).
    //
    // Old format structure:
    //   - entry_table_v2_2[i] = inode_index for entry i
    //   - names[i] = name for entry i
    //   - directories[i].first_entry = index of first child in names/inodes (NOT dir_entries)
    //   - directories[i].self_entry = index of this directory in names/inodes (NOT dir_entries)
    //
    // The code in domain_metadata_impl and domain_metadata_views handles both formats.

    return meta;
  }

private:
  Schema const& schema_;
  std::span<uint8_t const> data_;
  SchemaLayout const* layout_;
  mutable uint32_t bit_offset_;
  uint32_t storage_start_;
};

} // namespace impl

// Public API implementation - uses internal implementation namespace
domain::metadata Frozen2Deserializer::deserialize(
    Schema const& schema,
    std::span<uint8_t const> data) {

  // Get root layout from schema
  auto root_layout_opt = schema.layouts.get(schema.root_layout);
  if (!root_layout_opt) {
    throw std::runtime_error("Invalid schema: root layout not found");
  }

  // Create reader at root (pass pointer to the optional's value)
  // Use fully-qualified name to avoid confusion with forward declaration
  impl::Reader root_reader(schema, data, &(*root_layout_opt), 0, 0);

  // Deserialize metadata
  auto meta = root_reader.deserialize_metadata();

  return meta;
}

} // namespace dwarfs::metadata::legacy
