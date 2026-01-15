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

#include "flatbuffers_packing_processor.h"

#include <algorithm>
#include <iostream>
#include <numeric>

#include <dwarfs/logger.h>
#include <dwarfs/util.h>
#include <dwarfs/writer/metadata_options.h>

#include <dwarfs/internal/string_table.h>
#include <dwarfs/metadata/domain/metadata.h>

namespace dwarfs::writer::internal {

using dwarfs::internal::string_table;

flatbuffers_packing_processor::flatbuffers_packing_processor(
    logger& lgr, metadata::domain::metadata& md,
    metadata_options const& options)
    : lgr_{lgr}, md_{md}, options_{options} {}

void flatbuffers_packing_processor::pack_metadata() {
  if (options_.pack_directories) {
    // pack directories
    uint32_t last_first_entry = 0;

    for (size_t i = 0; i < md_.directories.size(); ++i) {
      auto& d = md_.directories[i];
      d.set_parent_entry(0); // this will be recovered
      d.set_self_entry(0);   // this will be recovered
      auto delta = d.first_entry() - last_first_entry;
      last_first_entry = d.first_entry();
      d.set_first_entry(delta);
    }
  } else {
    // Check sentinel directory and fix if necessary.
    //
    // For the sentinel, only the `first_entry` field is relevant, but due to
    // an off-by-one bug in `unpack_directories()`, the `self_entry` field
    // could get populated with a non-zero value. We simply clear it here.

    auto& sentinel = md_.directories.back();

    if (sentinel.self_entry() != 0) {
      // Note: fixing inconsistent sentinel directory
      sentinel.set_self_entry(0);
    }
  }

  if (options_.pack_chunk_table) {
    // delta-compress chunk table
    std::adjacent_difference(md_.chunk_table.begin(), md_.chunk_table.end(),
                             md_.chunk_table.begin());
  }

  if (options_.pack_shared_files_table) {
    if (md_.shared_files_table.has_value() &&
        !md_.shared_files_table->empty()) {
      auto& sf = md_.shared_files_table.value();
      DWARFS_CHECK(std::ranges::is_sorted(sf),
                   "shared files vector not sorted");
      std::vector<uint32_t> compressed;
      compressed.reserve(sf.back() + 1);

      uint32_t count = 0;
      uint32_t index = 0;
      for (auto i : sf) {
        if (i == index) {
          ++count;
        } else {
          ++index;
          DWARFS_CHECK(i == index, "inconsistent shared files vector");
          DWARFS_CHECK(count >= 2, "unique file in shared files vector");
          compressed.emplace_back(count - 2);
          count = 1;
        }
      }

      compressed.emplace_back(count - 2);

      DWARFS_CHECK(compressed.size() == sf.back() + 1,
                   "unexpected compressed vector size");

      sf.swap(compressed);
    }
  }

  if (!options_.plain_names_table) {
    auto packed = string_table::pack_domain(
        md_.names,
        string_table::pack_options(
            options_.pack_names, options_.pack_names_index,
            options_.force_pack_string_tables));

    // Validate packed result before clearing source data
    // For N strings, we need N+1 index entries (N offsets + 1 buffer size marker)
    bool pack_valid =
      !packed.buffer.empty() &&
      packed.index.size() == md_.names.size() + 1 &&
      packed.symtab.has_value();

    if (pack_valid) {
      md_.compact_names = std::move(packed);
      md_.names.clear();
    } else {
      // Packing failed - keep plain names, don't set compact_names
      // (compact_names stays as nullopt)
    }
  }

  if (!options_.plain_symlinks_table) {
    auto packed = string_table::pack_domain(
        md_.symlinks, string_table::pack_options(
                          options_.pack_symlinks, options_.pack_symlinks_index,
                          options_.force_pack_string_tables));

    // Validate packed result before clearing source data
    // For N strings, we need N+1 index entries (N offsets + 1 buffer size marker)
    bool pack_valid =
      !packed.buffer.empty() &&
      packed.index.size() == md_.symlinks.size() + 1 &&
      packed.symtab.has_value();

    if (pack_valid) {
      md_.compact_symlinks = std::move(packed);
      md_.symlinks.clear();
    } else {
      // Packing failed - keep plain symlinks
      // (compact_symlinks stays as nullopt)
    }
  }

  if (options_.no_category_names) {
    md_.category_names.reset();
    md_.block_categories.reset();
  }

  if (options_.no_category_names || options_.no_category_metadata) {
    md_.category_metadata_json.reset();
    md_.block_category_metadata.reset();
  }
}

} // namespace dwarfs::writer::internal