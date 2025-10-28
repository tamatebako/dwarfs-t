/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Directory entry domain model
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
 * A directory entry
 *
 * This structure represents a single directory entry and just combines
 * a name with an inode number. The inode number can then be used to
 * look up almost all other metadata.
 */
struct dir_entry {
  /// Index into metadata.names
  uint32_t name_index{0};

  /// Index into metadata.inodes
  uint32_t inode_num{0};

  /**
   * Cereal serialization support
   */
  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(name_index), CEREAL_NVP(inode_num));
  }
};

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::dir_entry, 1)