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

#include "dwarfs/metadata/legacy/frozen2_layout.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace dwarfs::metadata::legacy {

namespace {
// Maximum struct size in bytes (ser_frozen.rs:17)
constexpr uint16_t MAX_STRUCT_BYTE_SIZE =
    std::numeric_limits<int16_t>::max() / 8;
}

// LayoutStruct implementation

void LayoutStruct::add_field(std::unique_ptr<Layout> field) {
  fields_.push_back(std::move(field));
}

std::optional<uint16_t> LayoutStruct::finish() {
  // ser_frozen.rs:156-167
  uint16_t size = 0;

  for (auto& field : fields_) {
    auto field_size = field->finish();
    if (!field_size) {
      return std::nullopt; // Field too large
    }

    // If this field is a Collection, replace it with its converted struct
    // This matches Rust's "*self = Layout::Struct" in-place replacement
    if (auto* coll = dynamic_cast<LayoutCollection*>(field.get())) {
      // Transfer ownership of the converted struct
      field = coll->release_converted();
    }

    size += *field_size;
    if (size > MAX_STRUCT_BYTE_SIZE) {
      return std::nullopt; // Total size exceeds limit
    }
  }

  byte_size_ = size;

  // Return the size, even if it's 0
  // Empty structs are valid and should be preserved
  return byte_size_;
}

// LayoutCollection implementation

LayoutCollection::LayoutCollection(uint16_t count_size,
                                   std::unique_ptr<Layout> element)
    : count_size_(count_size)
    , element_(std::move(element)) {
}

uint16_t LayoutCollection::byte_size() const {
  // After finish(), return the stored inline size
  // Before finish(), this shouldn't be called (but return 0 for safety)
  return byte_size_;
}

std::optional<uint16_t> LayoutCollection::finish() {
  // ser_frozen.rs:168-192

  // Empty collection -> convert to None
  if (count_size_ == 0) {
    converted_struct_ = std::make_unique<LayoutNone>();
    byte_size_ = 0;
    return 0;
  }

  // Finalize element layout
  auto elem_size = element_->finish();
  if (!elem_size) {
    return std::nullopt; // Element too large
  }

  // Distance field: 0 if element is empty, 4 otherwise
  uint16_t distance_size = (*elem_size == 0) ? 0 : 4;
  uint16_t inline_size = distance_size + count_size_;

  // Convert to Struct{distance, count, element}
  auto st = std::make_unique<LayoutStruct>();

  if (distance_size > 0) {
    st->add_field(std::make_unique<LayoutPrimitive>(distance_size));
  } else {
    st->add_field(std::make_unique<LayoutNone>());
  }

  st->add_field(std::make_unique<LayoutPrimitive>(count_size_));
  st->add_field(std::move(element_));

  // CRITICAL: Call finish() on the converted struct to finalize it
  auto st_size = st->finish();
  if (!st_size) {
    return std::nullopt; // Struct too large
  }

  // OVERRIDE the struct's byte_size to exclude the element
  // The element is outlined data, only distance + count are inline
  st->set_byte_size(inline_size);

  converted_struct_ = std::move(st);

  // Store and return the inline size (distance + count)
  byte_size_ = inline_size;
  return byte_size_;
}

Layout const* LayoutCollection::to_struct() const {
  if (!converted_struct_) {
    throw std::logic_error(
        "LayoutCollection::to_struct() called before finish()");
  }
  return converted_struct_.get();
}

} // namespace dwarfs::metadata::legacy