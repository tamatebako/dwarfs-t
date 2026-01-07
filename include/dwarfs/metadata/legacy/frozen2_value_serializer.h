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
 * Value serializer - packs values into bytes according to layout
 *
 * Ported from: dwarfs-rs ser_frozen.rs:511-857
 *
 * Single Responsibility: Serialize domain values to bit-packed format
 * Dependencies: frozen2_layout, domain types
 *
 * Buffer layout:
 * |...| previous elements... | current struct in list | .. | outlined data... |
 * |   ^         serialized              |     0            | serialized       |
 *     ^(storage) base                   ^inline_pos                    buf.len()^
 */
class Serializer {
 public:
  Serializer(Layout const* layout,
             std::vector<uint8_t>& buf,
             uint32_t base,
             uint32_t inline_pos);

  // Primitive serializers
  void serialize_bool(bool v);
  void serialize_u32(uint32_t v);
  void serialize_u64(uint64_t v);
  void serialize_bytes(std::string const& v);

  // Optional serializer
  template<typename T, typename Func>
  void serialize_optional(
      std::optional<T> const& opt,
      Func&& serializer);

  // Collection serializers
  template<typename T, typename Func>
  void serialize_vector(
      std::vector<T> const& vec,
      Func&& serializer);

  template<typename T, typename Func>
  void serialize_set(
      std::set<T> const& set,
      Func&& serializer);

  template<typename K, typename V, typename KFunc, typename VFunc>
  void serialize_map(
      std::map<K, V> const& map,
      KFunc&& kserializer,
      VFunc&& vserializer);

  // Domain type serializers
  void serialize_chunk(domain::chunk const& c);
  void serialize_directory(domain::directory const& d);
  void serialize_inode_data(domain::inode_data const& i);
  void serialize_dir_entry(domain::dir_entry const& e);
  void serialize_fs_options(domain::fs_options const& opts);
  void serialize_string_table(domain::string_table const& st);
  void serialize_inode_size_cache(domain::inode_size_cache const& cache);
  void serialize_history_entry(domain::history_entry const& entry);
  void serialize_metadata(domain::metadata const& meta);

 private:
  friend class StructSerializer;

  uint32_t distance() const;
  void put_primitive(uint8_t const* data, size_t size);
  LayoutStruct const* as_struct(size_t field_count) const;

  Layout const* layout_;
  std::vector<uint8_t>& buf_;
  uint32_t base_;       // Storage base for 'distance'
  uint32_t inline_pos_; // Next inline serialization position
};

/**
 * Struct serializer helper - handles field iteration
 */
class StructSerializer {
 public:
  StructSerializer(Serializer& ser, std::vector<std::unique_ptr<Layout>> const& fields);

  template<typename T, typename Func>
  void serialize_field(T const& value,
                       Func&& serializer);

  void skip_field();

 private:
  Serializer& ser_;
  std::vector<std::unique_ptr<Layout>> const& fields_;
  size_t field_idx_;
};

// Template implementations

namespace {
// Forward declare helper from .cpp (inline version for template)
inline Layout const* get_actual_layout(Layout const* layout) {
  if (!layout || layout->is_none()) {
    return layout;
  }

  auto const* coll = dynamic_cast<LayoutCollection const*>(layout);
  if (coll) {
    return coll->to_struct();
  }

  return layout;
}
} // namespace

template<typename T, typename Func>
void Serializer::serialize_optional(
    std::optional<T> const& opt,
    Func&& serializer) {
  auto st_layout = as_struct(2);
  if (!st_layout) return;  // Entire optional is None - nothing to serialize

  StructSerializer st(*this, st_layout->fields());
  st.serialize_field(opt.has_value(), [](auto& s, bool v) { s.serialize_bool(v); });

  if (opt.has_value()) {
    st.serialize_field(*opt, std::forward<Func>(serializer));
  } else {
    st.skip_field();
  }
}

template<typename T, typename Func>
void Serializer::serialize_vector(
    std::vector<T> const& vec,
    Func&& serializer) {
  auto st_layout = as_struct(3);
  if (!st_layout) return;

  uint32_t distance = this->distance();
  uint32_t len = static_cast<uint32_t>(vec.size());

  // Serialize elements (outlined) - use actual layout after finish()
  Layout const* elem_layout = get_actual_layout(st_layout->fields()[2].get());

  // Write distance and count inline WITHOUT using StructSerializer
  // The outer serialize_field will advance inline_pos by Collection's byte_size
  Layout const* distance_field = st_layout->fields()[0].get();
  Layout const* count_field = st_layout->fields()[1].get();

  uint32_t write_pos = inline_pos_;  // Save starting position

  if (!distance_field->is_none()) {
    Layout const* prev_layout = layout_;
    layout_ = distance_field;
    serialize_u32(distance);
    write_pos += distance_field->byte_size();  // Track position for count
    layout_ = prev_layout;
  }

  // Write count at the next position
  Layout const* prev_layout = layout_;
  layout_ = count_field;
  uint32_t saved_inline_pos = inline_pos_;
  inline_pos_ = write_pos;  // Position for count
  serialize_u32(len);
  inline_pos_ = saved_inline_pos;  // Restore - outer will advance
  layout_ = prev_layout;

  if (elem_layout && !elem_layout->is_none() && !vec.empty()) {
    uint16_t elem_size = elem_layout->byte_size();
    size_t new_base = buf_.size();
    buf_.resize(new_base + len * elem_size, 0);

    Serializer elem_ser(elem_layout, buf_, static_cast<uint32_t>(new_base),
                        static_cast<uint32_t>(new_base));

    for (auto const& elem : vec) {
      serializer(elem_ser, elem);
    }
  }
}

template<typename T, typename Func>
void Serializer::serialize_set(
    std::set<T> const& set,
    Func&& serializer) {
  std::vector<T> vec(set.begin(), set.end());
  serialize_vector(vec, std::forward<Func>(serializer));
}

template<typename K, typename V, typename KFunc, typename VFunc>
void Serializer::serialize_map(
    std::map<K, V> const& map,
    KFunc&& kserializer,
    VFunc&& vserializer) {
  // Map entries are struct{key, value}
  serialize_vector<std::pair<K const, V>>(
      std::vector<std::pair<K const, V>>(map.begin(), map.end()),
      [&](Serializer& s, std::pair<K const, V> const& p) {
        auto st_layout = s.as_struct(2);
        if (!st_layout) return;

        StructSerializer st(s, st_layout->fields());
        st.serialize_field(p.first, std::forward<KFunc>(kserializer));
        st.serialize_field(p.second, std::forward<VFunc>(vserializer));
      });
}

template<typename T, typename Func>
void StructSerializer::serialize_field(
    T const& value,
    Func&& serializer) {
  if (field_idx_ >= fields_.size()) return;

  Layout const* field = fields_[field_idx_].get();
  field_idx_++;

  if (!field->is_none()) {
    Layout const* prev_layout = ser_.layout_;
    ser_.layout_ = field;
    std::forward<Func>(serializer)(ser_, value);
    ser_.inline_pos_ += field->byte_size();
    ser_.layout_ = prev_layout;
  }
}

} // namespace dwarfs::metadata::legacy