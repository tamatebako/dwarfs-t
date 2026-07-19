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

#include "dwarfs/metadata/legacy/frozen2_schema_converter.h"

#include <stdexcept>

namespace dwarfs::metadata::legacy {

std::optional<int16_t> cvt_layout(Layout const* layout,
                                   std::vector<SchemaLayout>& layouts) {
  if (!layout || layout->is_none()) {
    return std::nullopt;
  }

  // Handle primitives - register and return layout ID
  if (auto const* prim = dynamic_cast<LayoutPrimitive const*>(layout)) {
    uint16_t size = prim->byte_size();
    int16_t bits = static_cast<int16_t>(size * 8);

    // Check if primitive already registered
    for (size_t i = 0; i < layouts.size(); ++i) {
      if (layouts[i].bits == bits && layouts[i].fields.is_empty()) {
        return static_cast<int16_t>(i);
      }
    }

    // Register new primitive
    SchemaLayout schema_layout;
    schema_layout.size = 0;  // Primitives have size=0
    schema_layout.bits = bits;
    // fields is empty for primitives

    layouts.push_back(schema_layout);
    return static_cast<int16_t>(layouts.size() - 1);
  }

  // Handle structs
  if (auto const* st = dynamic_cast<LayoutStruct const*>(layout)) {
    auto const& fields = st->fields();

    // Convert all fields and collect their layout IDs and offsets
    DenseMap<SchemaField> schema_fields;

    int16_t current_offset = 0;
    int16_t field_id = 0;

    for (auto const& field : fields) {
      auto layout_id = cvt_layout(field.get(), layouts);

      if (layout_id) {
        // Field has a layout - calculate its offset
        uint16_t field_size = field->byte_size();
        int16_t field_offset = current_offset - static_cast<int16_t>(field_size * 8);

        SchemaField schema_field;
        schema_field.layout_id = *layout_id;
        schema_field.offset = field_offset;

        schema_fields.insert(field_id, schema_field);
        current_offset = field_offset;
      }
      field_id++;
      // Skip None fields - they don't contribute to schema
    }

    // Calculate total bits
    uint16_t struct_size = st->byte_size();
    int16_t total_bits = static_cast<int16_t>(struct_size * 8);

    // Register struct layout
    SchemaLayout schema_layout;
    schema_layout.size = 0;  // Only root has non-zero size
    schema_layout.bits = total_bits;
    schema_layout.fields = std::move(schema_fields);

    layouts.push_back(schema_layout);
    return static_cast<int16_t>(layouts.size() - 1);
  }

  throw std::logic_error("unknown layout type in cvt_layout");
}

} // namespace dwarfs::metadata::legacy
