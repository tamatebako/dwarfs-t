/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief History entry domain model
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
#include <string>
#include <cereal/cereal.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>

#include "fs_options.h"

namespace dwarfs::metadata::domain {

/**
 * This structure contains the version of the metadata format used
 * for tracking metadata rewrite history.
 */
struct history_entry {
  /// Major version number corresponding to the block header
  uint8_t major{0};

  /// Minor version number corresponding to the block header
  uint8_t minor{0};

  /// Version string of dwarfs library used to create the metadata
  std::optional<std::string> dwarfs_version;

  uint32_t block_size{0};

  std::optional<fs_options> options;

  /**
   * Cereal serialization support
   */
  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(major), CEREAL_NVP(minor),
       CEREAL_NVP(dwarfs_version), CEREAL_NVP(block_size),
       CEREAL_NVP(options));
  }
};

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::history_entry, 1)