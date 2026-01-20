/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
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

#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dwarfs::metadata::legacy {

/**
 * Dense map: i16 -> T
 *
 * Stored as Vec<Optional<T>> for quick O(1) indexing by i16 key.
 * Keys start at 0, with None representing missing keys.
 *
 * Ported from: dwarfs-rs/dwarfs/src/metadata.rs:49-136
 */
template <typename T>
class DenseMap {
 public:
  DenseMap() = default;
  explicit DenseMap(std::vector<std::optional<T>> data)
      : data_(std::move(data)) {}

  // Insert key-value pair
  void insert(int16_t key, T value) {
    if (key < 0) {
      throw std::runtime_error("DenseMap::insert: negative key not allowed: " +
                               std::to_string(key));
    }
    auto idx = static_cast<size_t>(key);
    if (idx >= data_.size()) {
      // Sanity check to prevent vector length errors
      if (idx > 10000) {
        throw std::runtime_error("DenseMap::insert: key too large: " +
                                 std::to_string(key) + " (max allowed: 10000)");
      }
      data_.resize(idx + 1);
    }
    data_[idx] = std::move(value);
  }

  // Get value by key
  // Returns pointer to value in vector, or nullptr if not found
  // Note: The pointer is valid as long as the DenseMap is not modified
  T* get(int16_t key) {
    auto idx = static_cast<size_t>(key);
    if (idx >= data_.size() || !data_[idx].has_value()) {
      return nullptr;
    }
    return &(*data_[idx]);
  }

  // Const version
  T const* get(int16_t key) const {
    auto idx = static_cast<size_t>(key);
    if (idx >= data_.size() || !data_[idx].has_value()) {
      return nullptr;
    }
    return &(*data_[idx]);
  }

  // Check if empty
  bool is_empty() const { return data_.empty(); }

  // Get size of underlying vector
  size_t size() const { return data_.size(); }

  // Iterator support (returns (key, value) pairs)
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<int16_t, T const*>;
    using pointer = value_type*;
    using reference = value_type;

    Iterator(std::vector<std::optional<T>> const& data, size_t pos)
        : data_(&data)
        , pos_(pos) {
      skip_to_next();
    }

    reference operator*() const {
      return {static_cast<int16_t>(pos_), &(*data_)[pos_].value()};
    }

    Iterator& operator++() {
      ++pos_;
      skip_to_next();
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    friend bool operator==(Iterator const& a, Iterator const& b) {
      return a.pos_ == b.pos_;
    }

    friend bool operator!=(Iterator const& a, Iterator const& b) {
      return a.pos_ != b.pos_;
    }

   private:
    void skip_to_next() {
      while (pos_ < data_->size() && !(*data_)[pos_].has_value()) {
        ++pos_;
      }
    }

    std::vector<std::optional<T>> const* data_;
    size_t pos_;
  };

  Iterator begin() const { return Iterator(data_, 0); }
  Iterator end() const { return Iterator(data_, data_.size()); }

  // Access underlying vector (for serialization)
  std::vector<std::optional<T>> const& raw_data() const { return data_; }
  std::vector<std::optional<T>>& raw_data() { return data_; }

 private:
  std::vector<std::optional<T>> data_;
};

/**
 * Frozen schema field
 *
 * Represents a field within a struct layout.
 * - layout_id: ID of the layout for this field's type
 * - offset: Byte offset if >= 0 (multiply by 8 for bit offset),
 *           bit offset if < 0 (negate to get actual bit offset)
 *
 * Ported from: dwarfs-rs/dwarfs/src/metadata.rs:178-193
 */
struct SchemaField {
  int16_t layout_id = 0;
  int16_t offset = 0;

  /**
   * Get bit offset for this field
   *
   * If offset >= 0: byte offset, return offset * 8
   * If offset < 0: bit offset, return -offset
   */
  uint16_t offset_bits() const {
    if (offset >= 0) {
      return static_cast<uint16_t>(offset) * 8;
    } else {
      return static_cast<uint16_t>(-offset);
    }
  }

  bool operator==(SchemaField const& other) const {
    return layout_id == other.layout_id && offset == other.offset;
  }

  bool operator!=(SchemaField const& other) const { return !(*this == other); }
};

/**
 * Frozen schema layout
 *
 * Describes the bit-level layout of a type:
 * - size: Total size in bytes (for root layout only, 0 for others)
 * - bits: Bit width for primitives, OR total bits for structs
 * - fields: Map of field_id -> SchemaField for struct types (empty for
 * primitives)
 * - type_name: Human-readable type name (optional, for debugging)
 *
 * Ported from: dwarfs-rs/dwarfs/src/metadata.rs:158-169
 */
struct SchemaLayout {
  int32_t size = 0;
  int16_t bits = 0;
  DenseMap<SchemaField> fields;
  std::string type_name;

  bool operator==(SchemaLayout const& other) const {
    // Note: We compare field-by-field since DenseMap doesn't have operator==
    if (size != other.size || bits != other.bits || type_name != other.type_name) {
      return false;
    }
    // Compare fields
    if (fields.size() != other.fields.size()) {
      return false;
    }
    for (auto [k, v] : fields) {
      auto o = other.fields.get(k);
      if (!o || *v != *o) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(SchemaLayout const& other) const { return !(*this == other); }
};

/**
 * Frozen schema (file_version = 1)
 *
 * Top-level schema structure describing the complete bit-packed layout:
 * - relax_type_checks: Whether to relax type checking (usually true)
 * - layouts: Map of layout_id -> SchemaLayout
 * - root_layout: ID of the root (top-level) layout
 * - file_version: Schema version (must be 1)
 *
 * Ported from: dwarfs-rs/dwarfs/src/metadata.rs:138-289
 */
struct Schema {
  bool relax_type_checks = false;
  DenseMap<SchemaLayout> layouts;
  int16_t root_layout = 0;
  int32_t file_version = 1;

  /**
   * Validate schema integrity
   *
   * Checks:
   * - file_version == 1
   * - root_layout exists in layouts
   * - All layout references are valid
   * - Field offsets don't overflow
   * - Bit widths are reasonable
   *
   * Throws std::runtime_error on validation failure.
   *
   * Ported from: dwarfs-rs/dwarfs/src/metadata.rs:238-288
   */
  void validate() const;

  bool operator==(Schema const& other) const {
    if (relax_type_checks != other.relax_type_checks ||
        root_layout != other.root_layout || file_version != other.file_version) {
      return false;
    }
    // Compare layouts
    if (layouts.size() != other.layouts.size()) {
      return false;
    }
    for (auto [k, v] : layouts) {
      auto o = other.layouts.get(k);
      if (!o || *v != *o) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(Schema const& other) const { return !(*this == other); }
};

} // namespace dwarfs::metadata::legacy