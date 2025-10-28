/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
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

/**
 * \file endian.h
 * \brief Endian conversion utilities (folly::Endian replacement)
 *
 * Provides endian conversion functions compatible with folly::Endian API.
 * Uses Boost.Endian for implementation.
 */

#include <boost/endian/conversion.hpp>
#include <cstdint>

namespace dwarfs::compat {

/**
 * \brief Endian conversion utilities
 *
 * Replacement for folly::Endian providing the same API.
 */
class Endian {
 public:
  // Big-endian conversions
  template <typename T>
  static T big(T value) {
    return boost::endian::native_to_big(value);
  }

  template <typename T>
  static T bigToHost(T value) {
    return boost::endian::big_to_native(value);
  }

  // Little-endian conversions
  template <typename T>
  static T little(T value) {
    return boost::endian::native_to_little(value);
  }

  template <typename T>
  static T littleToHost(T value) {
    return boost::endian::little_to_native(value);
  }

  // Byteswap operations
  template <typename T>
  static T swap(T value) {
    return boost::endian::endian_reverse(value);
  }
};

} // namespace dwarfs::compat