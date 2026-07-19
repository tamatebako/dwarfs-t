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

#include <optional>
#include <vector>

#include "dwarfs/metadata/legacy/frozen2_layout.h"
#include "dwarfs/metadata/legacy/frozen_schema.h"

namespace dwarfs::metadata::legacy {

/**
 * Schema converter - Layout tree to Schema
 *
 * Ported from: dwarfs-rs ser_frozen.rs:57-98
 *
 * Single Responsibility: Convert Layout hierarchy to flat Schema
 * Dependencies: frozen2_layout, frozen_schema
 *
 * Converts the Layout tree into a flat DenseMap<SchemaLayout> with
 * proper field offsets (negative bit offsets for inlined fields).
 */

/**
 * Convert a Layout tree to SchemaLayout entries
 *
 * @param layout Root layout to convert
 * @param layouts Output vector of SchemaLayout entries
 * @return Layout ID (index into layouts), or nullopt if layout is None
 * @throws std::runtime_error if layout is too large or count overflows
 */
std::optional<int16_t> cvt_layout(Layout const* layout,
                                   std::vector<SchemaLayout>& layouts);

} // namespace dwarfs::metadata::legacy