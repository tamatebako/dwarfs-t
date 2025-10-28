/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Inode data domain model
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
 * Inode Data
 *
 * This structure contains all necessary metadata for an inode, such as
 * its mode (i.e. permissions and inode type), its owner/group and its
 * timestamps.
 */
struct inode_data {
  /// Index into `metadata.modes[]`
  uint32_t mode_index{0};

  /// Index into `metadata.uids[]`
  uint32_t owner_index{0};

  /// Index into `metadata.gids[]`
  uint32_t group_index{0};

  /// atime relative to `metadata.timestamp_base`
  uint64_t atime_offset{0};

  /// mtime relative to `metadata.timestamp_base`
  uint64_t mtime_offset{0};

  /// ctime relative to `metadata.timestamp_base`
  uint64_t ctime_offset{0};

  //==========================================================//
  // fields added with dwarfs-0.14.0, file system version 2.5 //
  //==========================================================//

  /// btime (birth time) relative to `metadata.timestamp_base`
  uint64_t btime_offset{0};

  /// subsecond part of atime
  uint64_t atime_subsec{0};

  /// subsecond part of mtime
  uint64_t mtime_subsec{0};

  /// subsecond part of ctime
  uint64_t ctime_subsec{0};

  /// subsecond part of btime
  uint64_t btime_subsec{0};

  /// number of hard links (stored as nlink - 1)
  uint32_t nlink_minus_one{0};

  /**
   * ==================================================================
   * NOTE: These fields has been deprecated with filesystem version 2.3
   *       They are still being used to read older filesystem versions.
   *       They do *not* occupy any space in version 2.3 and above.
   */

  /// index into `metadata.names[]` (deprecated in v2.3)
  uint32_t name_index_v2_2{0};

  /// inode number (deprecated in v2.3)
  uint32_t inode_v2_2{0};

  /* ==================================================================
   */

  /**
   * Cereal serialization support
   */
  template <class Archive>
  void serialize(Archive& ar, std::uint32_t const version) {
    ar(CEREAL_NVP(mode_index), CEREAL_NVP(owner_index),
       CEREAL_NVP(group_index), CEREAL_NVP(atime_offset),
       CEREAL_NVP(mtime_offset), CEREAL_NVP(ctime_offset));

    // Deprecated fields from v2.2 (version 1)
    if (version >= 1) {
      ar(CEREAL_NVP(name_index_v2_2), CEREAL_NVP(inode_v2_2));
    }

    // Fields added in dwarfs-0.14.0 (version 2)
    if (version >= 2) {
      ar(CEREAL_NVP(btime_offset), CEREAL_NVP(atime_subsec),
         CEREAL_NVP(mtime_subsec), CEREAL_NVP(ctime_subsec),
         CEREAL_NVP(btime_subsec), CEREAL_NVP(nlink_minus_one));
    }
  }
};

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::inode_data, 2)