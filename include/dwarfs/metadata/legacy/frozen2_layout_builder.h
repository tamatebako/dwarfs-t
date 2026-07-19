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

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/legacy/frozen2_layout.h"

namespace dwarfs::metadata::legacy {

/**
 * Layout builder functions
 *
 * Ported from: dwarfs-rs ser_frozen.rs:195-256 + domain type builders
 *
 * Single Responsibility: Build Layout objects from domain types
 * Dependencies: frozen2_layout.h, domain types
 */

// Primitive helpers (ser_frozen.rs:135-141, 270-290)
std::unique_ptr<Layout> make_primitive(uint16_t byte_size);
std::unique_ptr<Layout> build_bool(bool v);
std::unique_ptr<Layout> build_u32(uint32_t v);
std::unique_ptr<Layout> build_u64(uint64_t v);
std::unique_ptr<Layout> build_bytes(std::string const& v);

// Optional builder
template<typename T, typename Builder>
std::unique_ptr<Layout> build_optional(
    std::optional<T> const& opt,
    Builder&& builder);

// Collection builders
template<typename T, typename Builder>
std::unique_ptr<Layout> build_vector(
    std::vector<T> const& vec,
    Builder&& builder);

template<typename T, typename Builder>
std::unique_ptr<Layout> build_set(
    std::set<T> const& set,
    Builder&& builder);

template<typename K, typename V, typename KBuilder, typename VBuilder>
std::unique_ptr<Layout> build_map(
    std::map<K, V> const& map,
    KBuilder&& kbuilder,
    VBuilder&& vbuilder);

// Domain type builders
std::unique_ptr<Layout> build_chunk(domain::chunk const& c);
std::unique_ptr<Layout> build_directory(domain::directory const& d);
std::unique_ptr<Layout> build_inode_data(domain::inode_data const& i);
std::unique_ptr<Layout> build_dir_entry(domain::dir_entry const& e);
std::unique_ptr<Layout> build_fs_options(domain::fs_options const& opts);
std::unique_ptr<Layout> build_string_table(domain::string_table const& st);
std::unique_ptr<Layout> build_inode_size_cache(domain::inode_size_cache const& cache);
std::unique_ptr<Layout> build_history_entry(domain::history_entry const& entry);

// Top-level metadata builder
std::unique_ptr<Layout> build_metadata(domain::metadata const& meta);

// Template implementations

template<typename T, typename Builder>
std::unique_ptr<Layout> build_optional(
    std::optional<T> const& opt,
    Builder&& builder) {
  if (opt.has_value()) {
    // Some(value): struct with is_some + inner value
    auto st = std::make_unique<LayoutStruct>();
    st->add_field(std::make_unique<LayoutPrimitive>(1)); // is_some = true
    st->add_field(std::forward<Builder>(builder)(*opt));
    return st;
  } else {
    // None: the entire optional is None (0 bytes)
    return std::make_unique<LayoutNone>();
  }
}

template<typename T, typename Builder>
std::unique_ptr<Layout> build_vector(
    std::vector<T> const& vec,
    Builder&& builder) {
  uint32_t len = static_cast<uint32_t>(vec.size());
  uint16_t len_size = (len != 0) ? 4 : 0;

  // Build layouts for ALL elements and merge them
  std::vector<std::unique_ptr<Layout>> elem_layouts;
  elem_layouts.reserve(vec.size());

  for (auto const& elem : vec) {
    elem_layouts.push_back(builder(elem));
  }

  // Merge all element layouts
  // For structs: take the maximum byte_size field from each position
  // For primitives: use the largest one
  // For now, use simplified logic: take first non-None OR merge structs
  std::unique_ptr<Layout> merged_elem = std::make_unique<LayoutNone>();

  if (!elem_layouts.empty() && !elem_layouts[0]->is_none()) {
    // If first element is a struct, need to merge all structs
    auto const* first_struct = dynamic_cast<LayoutStruct const*>(elem_layouts[0].get());

    if (first_struct && first_struct->fields().size() > 0) {
      // Merge struct fields across all elements
      size_t field_count = first_struct->fields().size();
      auto merged_struct = std::make_unique<LayoutStruct>();

      for (size_t i = 0; i < field_count; ++i) {
        // Find the largest non-None field at this position across all elements
        std::unique_ptr<Layout> merged_field = std::make_unique<LayoutNone>();

        for (auto const& elem_layout : elem_layouts) {
          if (elem_layout->is_none()) continue;

          auto const* elem_struct = dynamic_cast<LayoutStruct const*>(elem_layout.get());
          if (!elem_struct || i >= elem_struct->fields().size()) continue;

          auto const& field = elem_struct->fields()[i];
          if (!field->is_none() && merged_field->is_none()) {
            // Clone the field (simplified: create new primitive/none of same size)
            if (auto const* prim = dynamic_cast<LayoutPrimitive const*>(field.get())) {
              merged_field = std::make_unique<LayoutPrimitive>(prim->byte_size());
            }
          }
        }

        merged_struct->add_field(std::move(merged_field));
      }

      // CRITICAL: Must call finish() on merged struct so byte_size is calculated
      merged_struct->finish();

      merged_elem = std::move(merged_struct);
    } else {
      // Not a struct, use first non-None element
      merged_elem = std::move(elem_layouts[0]);
    }
  }

  return std::make_unique<LayoutCollection>(len_size, std::move(merged_elem));
}

template<typename T, typename Builder>
std::unique_ptr<Layout> build_set(
    std::set<T> const& set,
    Builder&& builder) {
  std::vector<T> vec(set.begin(), set.end());
  return build_vector(vec, std::forward<Builder>(builder));
}

template<typename K, typename V, typename KBuilder, typename VBuilder>
std::unique_ptr<Layout> build_map(
    std::map<K, V> const& map,
    KBuilder&& kbuilder,
    VBuilder&& vbuilder) {
  uint32_t len = static_cast<uint32_t>(map.size());
  uint16_t len_size = (len != 0) ? 4 : 0;

  // Each map entry is a struct{key, value}
  std::unique_ptr<Layout> merged_elem = std::make_unique<LayoutNone>();

  for (auto const& [key, val] : map) {
    auto entry = std::make_unique<LayoutStruct>();
    entry->add_field(kbuilder(key));
    entry->add_field(vbuilder(val));

    if (merged_elem->is_none()) {
      merged_elem = std::move(entry);
    }
  }

  return std::make_unique<LayoutCollection>(len_size, std::move(merged_elem));
}

} // namespace dwarfs::metadata::legacy