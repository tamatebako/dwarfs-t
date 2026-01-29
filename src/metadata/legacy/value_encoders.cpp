/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhx.github.io)
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

#include "dwarfs/metadata/legacy/value_encoders.h"
#include "dwarfs/metadata/legacy/frozen_writer.h"
#include "dwarfs/metadata/domain/chunk.h"
#include "dwarfs/metadata/domain/directory.h"
#include "dwarfs/metadata/domain/inode_data.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fmt/core.h>
#include <stdexcept>
#include <span>
#include <string>
#include <vector>

namespace dwarfs::metadata::legacy {

uint32_t ScalarEncoder::encode(
  FrozenWriter& writer,
  SchemaLayout const& layout,
  void const* value) const {

  // Validate: null pointer check
  if (!value) {
    throw std::invalid_argument("ScalarEncoder::encode: value pointer is null");
  }

  uint16_t bits = layout.bits;

  // Validate: bits must be 1-64
  if (bits == 0 || bits > 64) {
    throw std::invalid_argument(
        fmt::format("ScalarEncoder::encode: bits must be 1-64, got {}", bits));
  }

  uint64_t u64_value = 0;

  // Use memcpy for safe type punning (avoids undefined behavior from
  // strict aliasing violations). This is the recommended way in C++ for
  // type casting between unrelated types.
  //
  // Endianness: The Frozen2 format uses little-endian byte order, which
  // matches x86/x86_64 and most modern systems. No byte swapping needed.
  if (bits == 1) {
    // Boolean special case: read as uint8_t (true=1, false=0)
    uint8_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint8_t));
    u64_value = temp ? 1 : 0;
  } else if (bits <= 8) {
    uint8_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint8_t));
    u64_value = temp;
  } else if (bits <= 16) {
    uint16_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint16_t));
    u64_value = temp;
  } else if (bits <= 32) {
    uint32_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint32_t));
    u64_value = temp;
  } else {
    uint64_t temp = 0;
    std::memcpy(&temp, value, sizeof(uint64_t));
    u64_value = temp;
  }

  writer.write_scalar(u64_value, bits);
  return bits;
}

uint32_t VectorEncoder::encode(
  FrozenWriter& writer,
  SchemaLayout const& layout,
  void const* value) const {

  // Validate: null pointer check
  if (!value) {
    throw std::invalid_argument("VectorEncoder::encode: value pointer is null");
  }

  // Validate: layout.bits must be 64 (2 fields of 32 bits each)
  if (layout.bits != 64) {
    throw std::invalid_argument(
        fmt::format("VectorEncoder expects layout.bits=64, got {}", layout.bits));
  }

  // TEMPORARY: Hardcoded for std::vector<uint32_t>
  // TODO: Generalize to support any element type via layout.type
  // This is a transitional implementation per the Frozen2 serializer plan
  auto* vec = static_cast<const std::vector<uint32_t>*>(value);

  // Overflow protection: check size * elem_size won't overflow uint32_t
  uint32_t elem_size = sizeof(uint32_t);
  if (vec->size() > std::numeric_limits<uint32_t>::max() / elem_size) {
    throw std::length_error(
        fmt::format("VectorEncoder: size overflow ({} elements)", vec->size()));
  }

  // Reserve storage for elements
  uint32_t storage_offset = writer.reserve_storage(vec->size() * elem_size);

  // Write field 1: distance (offset to element data in bytes)
  writer.write_scalar(storage_offset, 32);

  // Write field 2: length (number of elements)
  writer.write_scalar(static_cast<uint32_t>(vec->size()), 32);

  // Write element data to storage (all at once for efficiency)
  if (!vec->empty()) {
    std::span<uint8_t const> all_data(
        reinterpret_cast<uint8_t const*>(vec->data()),
        vec->size() * elem_size);
    writer.write_storage(storage_offset, all_data);
  }

  return 64; // 2 fields of 32 bits each
}

uint32_t VectorEncoder::encode_with_element_layout(
  FrozenWriter& writer,
  SchemaLayout const& layout,
  SchemaLayout const& element_layout,
  void const* value) const {

  (void)element_layout;
  // Validate: null pointer check
  if (!value) {
    throw std::invalid_argument("VectorEncoder::encode_with_element_layout: value pointer is null");
  }

  // Validate: layout.bits must be 64 (2 fields of 32 bits each)
  if (layout.bits != 64) {
    throw std::invalid_argument(
        fmt::format("VectorEncoder expects layout.bits=64, got {}", layout.bits));
  }

  // This method is specifically for encoding std::vector<domain::chunk>
  // where each chunk has 3 uint32_t fields: block, offset, size
  auto* vec = static_cast<const std::vector<domain::chunk>*>(value);

  // Each chunk is 3 x uint32_t = 12 bytes
  uint32_t elem_size = 12;

  // Overflow protection
  if (vec->size() > std::numeric_limits<uint32_t>::max() / elem_size) {
    throw std::length_error(
        fmt::format("VectorEncoder: size overflow ({} elements)", vec->size()));
  }

  // Reserve storage for all elements
  uint32_t storage_offset = writer.reserve_storage(
      static_cast<uint32_t>(vec->size() * elem_size));

  // Write field 1: distance (offset to element data in bytes)
  writer.write_scalar(storage_offset, 32);

  // Write field 2: length (number of elements)
  writer.write_scalar(static_cast<uint32_t>(vec->size()), 32);

  // Write each chunk to storage using domain::chunk accessor methods
  uint32_t current_offset = storage_offset;
  for (size_t i = 0; i < vec->size(); ++i) {
    const domain::chunk& c = (*vec)[i];

    // Write block field (32 bits) using accessor
    uint32_t block_val = c.block();
    std::span<uint8_t const> block_bytes(
        reinterpret_cast<uint8_t const*>(&block_val), sizeof(uint32_t));
    writer.write_storage(current_offset, block_bytes);
    current_offset += sizeof(uint32_t);

    // Write offset field (32 bits) using accessor
    uint32_t offset_val = c.offset();
    std::span<uint8_t const> offset_bytes(
        reinterpret_cast<uint8_t const*>(&offset_val), sizeof(uint32_t));
    writer.write_storage(current_offset, offset_bytes);
    current_offset += sizeof(uint32_t);

    // Write size field (32 bits) using accessor
    uint32_t size_val = c.size();
    std::span<uint8_t const> size_bytes(
        reinterpret_cast<uint8_t const*>(&size_val), sizeof(uint32_t));
    writer.write_storage(current_offset, size_bytes);
    current_offset += sizeof(uint32_t);
  }

  return 64; // 2 fields of 32 bits each
}

uint32_t VectorEncoder::encode_directories(
  FrozenWriter& writer,
  SchemaLayout const& layout,
  SchemaLayout const& element_layout,
  void const* value) const {

  (void)element_layout;
  // Validate: null pointer check
  if (!value) {
    throw std::invalid_argument("VectorEncoder::encode_directories: value pointer is null");
  }

  // Validate: layout.bits must be 64 (2 fields of 32 bits each)
  if (layout.bits != 64) {
    throw std::invalid_argument(
        fmt::format("VectorEncoder expects layout.bits=64, got {}", layout.bits));
  }

  // Encode std::vector<domain::directory>
  // where each directory has 3 uint32_t fields: parent_entry, first_entry, self_entry
  auto* vec = static_cast<const std::vector<domain::directory>*>(value);

  // Each directory is 3 x uint32_t = 12 bytes
  uint32_t elem_size = 12;

  // Overflow protection
  if (vec->size() > std::numeric_limits<uint32_t>::max() / elem_size) {
    throw std::length_error(
        fmt::format("VectorEncoder: size overflow ({} elements)", vec->size()));
  }

  // Reserve storage for all elements
  uint32_t storage_offset = writer.reserve_storage(
      static_cast<uint32_t>(vec->size() * elem_size));

  // Write field 1: distance (offset to element data in bytes)
  writer.write_scalar(storage_offset, 32);

  // Write field 2: length (number of elements)
  writer.write_scalar(static_cast<uint32_t>(vec->size()), 32);

  // Write each directory to storage using domain::directory accessor methods
  uint32_t current_offset = storage_offset;
  for (size_t i = 0; i < vec->size(); ++i) {
    const domain::directory& d = (*vec)[i];

    // Write parent_entry field (32 bits) using accessor
    uint32_t parent_val = d.parent_entry();
    std::span<uint8_t const> parent_bytes(
        reinterpret_cast<uint8_t const*>(&parent_val), sizeof(uint32_t));
    writer.write_storage(current_offset, parent_bytes);
    current_offset += sizeof(uint32_t);

    // Write first_entry field (32 bits) using accessor
    uint32_t first_val = d.first_entry();
    std::span<uint8_t const> first_bytes(
        reinterpret_cast<uint8_t const*>(&first_val), sizeof(uint32_t));
    writer.write_storage(current_offset, first_bytes);
    current_offset += sizeof(uint32_t);

    // Write self_entry field (32 bits) using accessor
    uint32_t self_val = d.self_entry();
    std::span<uint8_t const> self_bytes(
        reinterpret_cast<uint8_t const*>(&self_val), sizeof(uint32_t));
    writer.write_storage(current_offset, self_bytes);
    current_offset += sizeof(uint32_t);
  }

  return 64; // 2 fields of 32 bits each
}

uint32_t VectorEncoder::encode_inodes(
  FrozenWriter& writer,
  SchemaLayout const& layout,
  SchemaLayout const& element_layout,
  void const* value) const {

  (void)element_layout;
  // Validate: null pointer check
  if (!value) {
    throw std::invalid_argument("VectorEncoder::encode_inodes: value pointer is null");
  }

  // Validate: layout.bits must be 64 (2 fields of 32 bits each)
  if (layout.bits != 64) {
    throw std::invalid_argument(
        fmt::format("VectorEncoder expects layout.bits=64, got {}", layout.bits));
  }

  // Encode std::vector<domain::inode_data>
  // inode_data has: 3 u32 + 8 u64 + 1 u32 = 12 fields = 72 bytes
  auto* vec = static_cast<const std::vector<domain::inode_data>*>(value);

  // Each inode is 3 x uint32_t + 8 x uint64_t + 1 x uint32_t = 12 + 64 + 4 = 80 bytes
  uint32_t elem_size = 80;

  // Overflow protection
  if (vec->size() > std::numeric_limits<uint32_t>::max() / elem_size) {
    throw std::length_error(
        fmt::format("VectorEncoder: size overflow ({} elements)", vec->size()));
  }

  // Reserve storage for all elements
  uint32_t storage_offset = writer.reserve_storage(
      static_cast<uint32_t>(vec->size() * elem_size));

  // Write field 1: distance (offset to element data in bytes)
  writer.write_scalar(storage_offset, 32);

  // Write field 2: length (number of elements)
  writer.write_scalar(static_cast<uint32_t>(vec->size()), 32);

  // Write each inode to storage
  uint32_t current_offset = storage_offset;
  for (size_t i = 0; i < vec->size(); ++i) {
    const domain::inode_data& inode = (*vec)[i];

    // Write u32 fields
    std::span<uint8_t const> mode_bytes(
        reinterpret_cast<uint8_t const*>(&inode.mode_index), sizeof(uint32_t));
    writer.write_storage(current_offset, mode_bytes);
    current_offset += sizeof(uint32_t);

    std::span<uint8_t const> owner_bytes(
        reinterpret_cast<uint8_t const*>(&inode.owner_index), sizeof(uint32_t));
    writer.write_storage(current_offset, owner_bytes);
    current_offset += sizeof(uint32_t);

    std::span<uint8_t const> group_bytes(
        reinterpret_cast<uint8_t const*>(&inode.group_index), sizeof(uint32_t));
    writer.write_storage(current_offset, group_bytes);
    current_offset += sizeof(uint32_t);

    // Write u64 fields
    std::span<uint8_t const> atime_off_bytes(
        reinterpret_cast<uint8_t const*>(&inode.atime_offset), sizeof(uint64_t));
    writer.write_storage(current_offset, atime_off_bytes);
    current_offset += sizeof(uint64_t);

    std::span<uint8_t const> mtime_off_bytes(
        reinterpret_cast<uint8_t const*>(&inode.mtime_offset), sizeof(uint64_t));
    writer.write_storage(current_offset, mtime_off_bytes);
    current_offset += sizeof(uint64_t);

    std::span<uint8_t const> ctime_off_bytes(
        reinterpret_cast<uint8_t const*>(&inode.ctime_offset), sizeof(uint64_t));
    writer.write_storage(current_offset, ctime_off_bytes);
    current_offset += sizeof(uint64_t);

    std::span<uint8_t const> btime_off_bytes(
        reinterpret_cast<uint8_t const*>(&inode.btime_offset), sizeof(uint64_t));
    writer.write_storage(current_offset, btime_off_bytes);
    current_offset += sizeof(uint64_t);

    std::span<uint8_t const> atime_sub_bytes(
        reinterpret_cast<uint8_t const*>(&inode.atime_subsec), sizeof(uint64_t));
    writer.write_storage(current_offset, atime_sub_bytes);
    current_offset += sizeof(uint64_t);

    std::span<uint8_t const> mtime_sub_bytes(
        reinterpret_cast<uint8_t const*>(&inode.mtime_subsec), sizeof(uint64_t));
    writer.write_storage(current_offset, mtime_sub_bytes);
    current_offset += sizeof(uint64_t);

    std::span<uint8_t const> ctime_sub_bytes(
        reinterpret_cast<uint8_t const*>(&inode.ctime_subsec), sizeof(uint64_t));
    writer.write_storage(current_offset, ctime_sub_bytes);
    current_offset += sizeof(uint64_t);

    std::span<uint8_t const> btime_sub_bytes(
        reinterpret_cast<uint8_t const*>(&inode.btime_subsec), sizeof(uint64_t));
    writer.write_storage(current_offset, btime_sub_bytes);
    current_offset += sizeof(uint64_t);

    // Write nlink_minus_one (u32)
    std::span<uint8_t const> nlink_bytes(
        reinterpret_cast<uint8_t const*>(&inode.nlink_minus_one), sizeof(uint32_t));
    writer.write_storage(current_offset, nlink_bytes);
    current_offset += sizeof(uint32_t);
  }

  return 64; // 2 fields of 32 bits each
}

uint32_t StringEncoder::encode(
  FrozenWriter& writer,
  SchemaLayout const& layout,
  void const* value) const {

  (void)layout;
  // Validate: null pointer check
  if (!value) {
    throw std::invalid_argument("StringEncoder::encode: value pointer is null");
  }

  auto* str = static_cast<const std::string*>(value);

  // Reserve storage for string bytes
  uint32_t storage_offset = writer.reserve_storage(str->size());

  // Write field 1: distance (offset in storage)
  writer.write_scalar(storage_offset, 32);

  // Write field 2: length
  writer.write_scalar(static_cast<uint32_t>(str->size()), 32);

  // Write string bytes to storage
  if (!str->empty()) {
    std::span<uint8_t const> str_bytes(
      reinterpret_cast<const uint8_t*>(str->data()),
      str->size());
    writer.write_storage(storage_offset, str_bytes);
  }

  return 64; // 2 fields of 32 bits each
}

} // namespace dwarfs::metadata::legacy
