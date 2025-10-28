/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Inode size cache domain model
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
#include <map>
#include <cereal/cereal.hpp>
#include <cereal/types/map.hpp>

namespace dwarfs::metadata::domain {

/**
 * For highly fragmented inodes, computing the size from the
 * individual chunks can be extremely slow. This cache can be
 * used to bypass the chunk lookup and size computation.
 */
struct inode_size_cache {
  /// Lookup from inode number to size
  std::map<uint32_t, uint64_t> size_lookup;

  /// Minimum number of chunks for a file to be found in the cache,
  /// corresponds to scanner_options.inode_size_cache_min_chunk_count
  uint64_t min_chunk_count{0};

  //==========================================================//
  // fields added with dwarfs-0.14.0, file system version 2.5 //
  //==========================================================//

  /// Lookup from inode number to allocated_size
  /// only used if the inode is sparse
  std::map<uint32_t, uint64_t> allocated_size_lookup;

  /**
   * Cereal serialization support
   */
  template <class Archive>
  void serialize(Archive& ar, std::uint32_t const version) {
    ar(CEREAL_NVP(size_lookup), CEREAL_NVP(min_chunk_count));

    // allocated_size_lookup added in version 2
    if (version >= 2) {
      ar(CEREAL_NVP(allocated_size_lookup));
    }
  }
};

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::inode_size_cache, 2)