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

#include "dwarfs/metadata/legacy/frozen_schema.h"

#include <stdexcept>
#include <fmt/format.h>

namespace dwarfs::metadata::legacy {

namespace {

// Helper for overflow-safe checked_mul and checked_add
std::optional<int16_t> checked_mul_i16(int16_t a, int16_t b) {
  int32_t result = static_cast<int32_t>(a) * static_cast<int32_t>(b);
  if (result < INT16_MIN || result > INT16_MAX) {
    return std::nullopt;
  }
  return static_cast<int16_t>(result);
}

std::optional<int16_t> checked_neg_i16(int16_t a) {
  if (a == INT16_MIN) {
    return std::nullopt; // -INT16_MIN would overflow
  }
  return -a;
}

std::optional<uint16_t> checked_add_u16(uint16_t a, uint16_t b) {
  uint32_t result = static_cast<uint32_t>(a) + static_cast<uint32_t>(b);
  if (result > UINT16_MAX) {
    return std::nullopt;
  }
  return static_cast<uint16_t>(result);
}

} // anonymous namespace

void Schema::validate() const {
  // Ported from: dwarfs-rs/dwarfs/src/metadata.rs:238-288
  constexpr int32_t FILE_VERSION = 1;

  // Check file_version
  if (file_version != FILE_VERSION) {
    throw std::runtime_error(
        fmt::format("unsupported schema file_version {}", file_version));
  }

  // Check root_layout exists
  if (!layouts.get(root_layout)) {
    throw std::runtime_error("missing root_layout");
  }

  // Validate each layout
  for (auto [layout_id, layout_ptr] : layouts) {
    auto const& layout = *layout_ptr;

    // For primitive types (no fields), check bit width
    if (layout.fields.is_empty() && layout.bits > 64) {
      throw std::runtime_error(fmt::format(
          "layout {}: primitive type is too large to have {} bits", layout_id,
          layout.bits));
    }

    // Validate each field
    for (auto [field_id, field_ptr] : layout.fields) {
      auto const& field = *field_ptr;

      // Check field layout_id is valid
      auto field_layout_ptr = layouts.get(field.layout_id);
      if (!field_layout_ptr) {
        throw std::runtime_error(fmt::format(
            "field {} of layout {}: layout index out of range", field_id,
            layout_id));
      }
      auto const& field_layout = *field_layout_ptr;

      // Check field_layout.bits is non-negative
      if (field_layout.bits < 0) {
        throw std::runtime_error(fmt::format(
            "field {} of layout {}: layout bits cannot be negative", field_id,
            layout_id));
      }

      // Calculate bit_offset from field.offset
      std::optional<int16_t> bit_offset;
      if (field.offset >= 0) {
        bit_offset = checked_mul_i16(field.offset, 8);
      } else {
        bit_offset = checked_neg_i16(field.offset);
      }

      if (!bit_offset.has_value()) {
        throw std::runtime_error(fmt::format(
            "field {} of layout {}: offset overflows", field_id, layout_id));
      }

      // Check bit_offset + field_layout.bits doesn't overflow
      auto bit_total_size =
          checked_add_u16(static_cast<uint16_t>(*bit_offset),
                          static_cast<uint16_t>(field_layout.bits));
      if (!bit_total_size.has_value()) {
        throw std::runtime_error(fmt::format(
            "field {} of layout {}: offset overflows", field_id, layout_id));
      }
    }
  }
}

} // namespace dwarfs::metadata::legacy