/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \file
 * \brief Rice++ block header domain model
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
 * Rice++ block header for compression metadata
 */
struct ricepp_block_header {
  uint32_t block_size{0};
  uint16_t component_count{0};
  uint8_t bytes_per_sample{0};
  uint8_t unused_lsb_count{0};
  bool big_endian{false};
  uint16_t ricepp_version{0};

  /**
   * Cereal serialization support
   */
  template <class Archive>
  void serialize(Archive& ar) {
    ar(CEREAL_NVP(block_size), CEREAL_NVP(component_count),
       CEREAL_NVP(bytes_per_sample), CEREAL_NVP(unused_lsb_count),
       CEREAL_NVP(big_endian), CEREAL_NVP(ricepp_version));
  }
};

} // namespace dwarfs::metadata::domain

CEREAL_CLASS_VERSION(dwarfs::metadata::domain::ricepp_block_header, 1)