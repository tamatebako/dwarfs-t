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
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dwarfs::metadata::legacy {

/**
 * Layout base class - represents structure of a type
 *
 * Ported from: dwarfs-rs ser_frozen.rs:100-257
 *
 * Single Responsibility: Define layout structure for types
 * Hierarchy: Base class for None/Primitive/Struct/Collection
 */
class Layout {
 public:
  virtual ~Layout() = default;

  virtual bool is_none() const = 0;
  virtual uint16_t byte_size() const = 0;

  /**
   * Finalize layout - optimize and validate
   * Returns byte size if valid, nullopt if too large
   */
  virtual std::optional<uint16_t> finish() = 0;

  // For debugging
  virtual std::string type_name() const = 0;
};

/**
 * LayoutNone - empty layout (no data stored)
 */
class LayoutNone : public Layout {
 public:
  bool is_none() const override { return true; }
  uint16_t byte_size() const override { return 0; }
  std::optional<uint16_t> finish() override { return 0; }
  std::string type_name() const override { return "None"; }
};

/**
 * LayoutPrimitive - fixed-size primitive type
 */
class LayoutPrimitive : public Layout {
 public:
  explicit LayoutPrimitive(uint16_t size) : byte_size_(size) {}

  bool is_none() const override { return false; }
  uint16_t byte_size() const override { return byte_size_; }
  std::optional<uint16_t> finish() override { return byte_size_; }
  std::string type_name() const override { return "Primitive"; }

 private:
  uint16_t byte_size_;
};

/**
 * LayoutStruct - struct with multiple fields
 */
class LayoutStruct : public Layout {
 public:
  bool is_none() const override { return false; }
  uint16_t byte_size() const override { return byte_size_; }
  std::string type_name() const override { return "Struct"; }

  void add_field(std::unique_ptr<Layout> field);

  std::vector<std::unique_ptr<Layout>> const& fields() const { return fields_; }

  /**
   * Finish - sum field sizes, validate <= MAX_STRUCT_BYTE_SIZE
   * Converts to None if total size is 0
   */
  std::optional<uint16_t> finish() override;

 private:
  friend class LayoutCollection; // Allow Collection to set byte_size_

  void set_byte_size(uint16_t size) { byte_size_ = size; }

  uint16_t byte_size_ = 0;
  std::vector<std::unique_ptr<Layout>> fields_;
};

/**
 * LayoutCollection - collection type
 * Will be converted to Struct{distance, count, element} after finish()
 */
class LayoutCollection : public Layout {
 public:
  LayoutCollection(uint16_t count_size, std::unique_ptr<Layout> element);

  bool is_none() const override { return false; }
  uint16_t byte_size() const override;
  std::string type_name() const override { return "Collection"; }

  Layout const* element() const { return element_.get(); }

  /**
   * Finish - convert to Struct{distance, count, element}
   * distance_size = 0 if element is empty, 4 otherwise
   */
  std::optional<uint16_t> finish() override;

  /**
   * Get converted struct (only valid after finish())
   */
  Layout const* to_struct() const;

  // Transfer ownership of converted struct (called by parent during finish())
  std::unique_ptr<Layout> release_converted() {
    return std::move(converted_struct_);
  }

 private:
  uint16_t count_size_;
  std::unique_ptr<Layout> element_;
  std::unique_ptr<Layout> converted_struct_; // Set by finish()
  uint16_t byte_size_ = 0; // Set by finish(), inline size (distance + count)
};

} // namespace dwarfs::metadata::legacy