/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Directory domain model
 * \author Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <cereal/cereal.hpp>

namespace dwarfs::metadata::domain {

/**
 * One directory
 *
 * This structure represents the links between directory entries.
 * The `parent_entry` references the parent directory's `dir_entry`.
 * The `first_entry` members can be used to access the entries contained
 * in the directory.
 *
 * The range of contained entries is:
 *
 *    dir_entries[directory[inode].first_entry]
 *    ..
 *    dir_entries[directory[inode + 1].first_entry - 1]
 *
 * Note that as of v2.3, directory entries can be stored "packed", in
 * which case only the `first_entry` fields are populated and stored
 * delta-compressed. The `first_entry` field must be unpacked before
 * using and the `parent_entry` and `self_entry` fields must be built
 * by traversing the `dir_entries` using the unpacked `first_entry`
 * fields.
 */
struct directory {
  /// Indexes into `dir_entries`
  uint32_t parent_entry{0};

  /// Indexes into `dir_entries`
  uint32_t first_entry{0};

  /// Indexes into `dir_entries` (added with dwarfs-0.11.0, file system version 2.5)
  uint32_t self_entry{0};

  /**
   * Cereal serialization support
   */
  template <class Archive>
  void serialize(Archive& ar, std::uint32_t const version) {
    ar(CEREAL_NVP(parent_entry), CEREAL_NVP(first_entry));

    // self_entry added in version 2
    if (version >= 2) {
      ar(CEREAL_NVP(self_entry));
    }
  }
};

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::directory, 2)