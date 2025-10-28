/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief File system options domain model
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
#include <optional>
#include <cereal/cereal.hpp>
#include <cereal/types/optional.hpp>

namespace dwarfs::metadata::domain {

/**
 * File system options
 */
struct fs_options {
  /// File system contains only mtime time stamps
  bool mtime_only{false};

  /// Time base and offsets are stored with this resolution
  /// 1 = seconds, 60 = minutes, 3600 = hours, ...
  std::optional<uint32_t> time_resolution_sec;

  bool packed_chunk_table{false};
  bool packed_directories{false};
  bool packed_shared_files_table{false};

  //==========================================================//
  // fields added with dwarfs-0.14.0, file system version 2.5 //
  //==========================================================//

  /// If time stamps are stored with subsecond resolution,
  /// this multiplier is used to convert the subsecond part
  /// to nanoseconds; e.g. if the subsecond parts are stored
  /// with millisecond resolution, this would be 1,000,000
  std::optional<uint32_t> subsecond_resolution_nsec_multiplier;

  /// File system contains btime (birth time) time stamps
  bool has_btime{false};

  /// Inodes contain valid nlink values in `inode_data.nlink_minus_one`
  bool inodes_have_nlink{false};

  /**
   * Cereal serialization support
   */
  template <class Archive>
  void serialize(Archive& ar, std::uint32_t const version) {
    ar(CEREAL_NVP(mtime_only), CEREAL_NVP(time_resolution_sec),
       CEREAL_NVP(packed_chunk_table), CEREAL_NVP(packed_directories),
       CEREAL_NVP(packed_shared_files_table));

    // Fields added in dwarfs-0.14.0 (version 2)
    if (version >= 2) {
      ar(CEREAL_NVP(subsecond_resolution_nsec_multiplier),
         CEREAL_NVP(has_btime), CEREAL_NVP(inodes_have_nlink));
    }
  }
};

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::fs_options, 2)