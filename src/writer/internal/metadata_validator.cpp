/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose Inc. (OOP refactoring)
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

#include <dwarfs/writer/internal/metadata_validator.h>

#include <algorithm>

#include <dwarfs/metadata/domain/metadata.h>

namespace dwarfs::writer::internal {

std::string
metadata_validator::validate(metadata::domain::metadata const& md) {
  if (auto err = validate_chunk_table(md); !err.empty()) {
    return err;
  }
  if (auto err = validate_shared_files(md); !err.empty()) {
    return err;
  }
  return {};
}

std::string metadata_validator::validate_chunk_table(
    metadata::domain::metadata const& md) {
  if (md.chunk_table.empty()) {
    return "chunk table is empty";
  }

  // Check monotonically increasing
  for (size_t i = 1; i < md.chunk_table.size(); ++i) {
    if (md.chunk_table[i] < md.chunk_table[i - 1]) {
      return "chunk table not monotonically increasing at index " +
             std::to_string(i);
    }
  }

  // Final index should equal chunks size
  if (md.chunk_table.back() != md.chunks.size()) {
    return "chunk table final index (" +
           std::to_string(md.chunk_table.back()) +
           ") doesn't match chunks size (" + std::to_string(md.chunks.size()) +
           ")";
  }

  return {};
}

std::string metadata_validator::validate_shared_files(
    metadata::domain::metadata const& md) {
  if (!md.shared_files_table.has_value() || md.shared_files_table->empty()) {
    return {};
  }

  auto const& sf = md.shared_files_table.value();

  // Check sorted
  if (!std::ranges::is_sorted(sf)) {
    return "shared files table not sorted";
  }

  return {};
}

} // namespace dwarfs::writer::internal