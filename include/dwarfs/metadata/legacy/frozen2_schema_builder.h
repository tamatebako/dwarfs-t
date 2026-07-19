/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     DwarFS Implementation
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

#include "dwarfs/metadata/legacy/frozen_schema.h"
#include "dwarfs/metadata/domain/metadata.h"

namespace dwarfs::metadata::legacy {

/**
 * SchemaBuilder for Frozen2 metadata format
 *
 * Analyzes domain::metadata and builds a Schema describing the bit-level
 * layout for Frozen2 serialization. This is the first step in the
 * serialization process - we must understand the structure before we
 * can encode values.
 *
 * Ported from: dwarfs-rs SchemaBuilder pattern (Task 4)
 */
class SchemaBuilder {
public:
  SchemaBuilder() = default;

  /**
   * Build complete schema from domain metadata
   *
   * @param meta The domain metadata to analyze
   * @return Schema describing the bit-level layout
   */
  Schema build_from(domain::metadata const& meta);

  /**
   * Get the chunk layout ID from the last built schema
   *
   * This is used by the serializer to know which layout to use for
   * encoding chunk elements.
   *
   * @return The chunk layout ID, or -1 if not yet built
   */
  int16_t chunk_layout_id() const { return chunk_layout_id_; }

  /**
   * Get the directory layout ID from the last built schema
   *
   * @return The directory layout ID, or -1 if not yet built
   */
  int16_t directory_layout_id() const { return directory_layout_id_; }

  /**
   * Get the inode layout ID from the last built schema
   *
   * @return The inode layout ID, or -1 if not yet built
   */
  int16_t inode_layout_id() const { return inode_layout_id_; }

private:
  SchemaLayout build_chunk_layout(int16_t u32_layout_id);
  SchemaLayout build_directory_layout(int16_t u32_layout_id);
  SchemaLayout build_inode_layout(int16_t u32_layout_id, int16_t u64_layout_id);
  DenseMap<SchemaLayout> layouts_;
  int16_t next_layout_id_ = 1;
  int16_t chunk_layout_id_ = -1;
  int16_t directory_layout_id_ = -1;
  int16_t inode_layout_id_ = -1;
};

} // namespace dwarfs::metadata::legacy
