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
 * \file bits.h
 * \brief Bit manipulation utilities (folly::Bits replacement)
 *
 * Provides efficient bit manipulation operations.
 */

#include <cstdint>
#include <bit>

#if defined(__GNUC__) || defined(__clang__)
  #define DWARFS_HAS_BUILTIN_CLZ 1
#elif defined(_MSC_VER)
  #include <intrin.h>
  #define DWARFS_HAS_BUILTIN_CLZ 1
#endif

namespace dwarfs::compat {

/**
 * \brief Bit manipulation utilities
 */
class Bits {
 public:
  /**
   * \brief Find position of last (most significant) set bit
   *
   * Replacement for folly::Bits::findLastSet and folly::findLastSet.
   * Returns 0 if value is 0, otherwise returns 1-based position of MSB.
   *
   * \param value Input value
   * \return Position of last set bit (1-based), or 0 if value is 0
   */
  static inline int findLastSet(uint64_t value) {
    if (value == 0) {
      return 0;
    }

#if defined(__GNUC__) || defined(__clang__)
    return 64 - __builtin_clzll(value);

#elif defined(_MSC_VER)
    unsigned long index;
    _BitScanReverse64(&index, value);
    return static_cast<int>(index) + 1;

#else
    // Portable fallback using C++20 std::bit_width
    return std::bit_width(value);
#endif
  }

  /**
   * \brief Find position of first (least significant) set bit
   *
   * Returns 0 if value is 0, otherwise returns 1-based position of LSB.
   *
   * \param value Input value
   * \return Position of first set bit (1-based), or 0 if value is 0
   */
  static inline int findFirstSet(uint64_t value) {
    if (value == 0) {
      return 0;
    }

#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ffsll(value);

#elif defined(_MSC_VER)
    unsigned long index;
    _BitScanForward64(&index, value);
    return static_cast<int>(index) + 1;

#else
    // Portable fallback
    int pos = 1;
    while ((value & 1) == 0) {
      value >>= 1;
      ++pos;
    }
    return pos;
#endif
  }

  /**
   * \brief Count number of set bits (popcount)
   *
   * \param value Input value
   * \return Number of bits set to 1
   */
  static inline int popcount(uint64_t value) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(value);

#elif defined(_MSC_VER)
    return static_cast<int>(__popcnt64(value));

#else
    // Portable fallback using C++20 std::popcount
    return std::popcount(value);
#endif
  }

  /**
   * \brief Packed bit operations (folly::Bits<T> replacement)
   *
   * Provides bit-level get/set operations on arrays for packed integer vectors.
   */
  template <typename T>
  class PackedBits {
   public:
    using UnderlyingType = T;
    static constexpr size_t bitsPerBlock = sizeof(T) * 8;

    /**
     * \brief Get value from bit array
     * \param data Pointer to data array
     * \param bitOffset Bit offset from start
     * \param numBits Number of bits to read
     * \return Value read from bit array
     */
    static T get(T const* data, size_t bitOffset, size_t numBits) {
      if (numBits == 0 || numBits > bitsPerBlock) {
        return 0;
      }

      size_t const blockIndex = bitOffset / bitsPerBlock;
      size_t const bitIndex = bitOffset % bitsPerBlock;

      T result = data[blockIndex] >> bitIndex;

      if (bitIndex + numBits > bitsPerBlock) {
        // Value spans two blocks
        size_t const bitsInFirstBlock = bitsPerBlock - bitIndex;
        size_t const bitsInSecondBlock = numBits - bitsInFirstBlock;
        T const secondPart = data[blockIndex + 1] << bitsInFirstBlock;
        result |= secondPart;
      }

      // Mask to numBits
      T const mask = (numBits == bitsPerBlock)
          ? static_cast<T>(~T{0})
          : ((static_cast<T>(1) << numBits) - 1);
      return result & mask;
    }

    /**
     * \brief Set value in bit array
     * \param data Pointer to data array
     * \param bitOffset Bit offset from start
     * \param numBits Number of bits to write
     * \param value Value to write
     */
    static void set(T* data, size_t bitOffset, size_t numBits, T value) {
      if (numBits == 0 || numBits > bitsPerBlock) {
        return;
      }

      size_t const blockIndex = bitOffset / bitsPerBlock;
      size_t const bitIndex = bitOffset % bitsPerBlock;

      T const mask = (numBits == bitsPerBlock)
          ? static_cast<T>(~T{0})
          : ((static_cast<T>(1) << numBits) - 1);
      value &= mask;

      T const clearMask = ~(mask << bitIndex);
      data[blockIndex] = (data[blockIndex] & clearMask) | (value << bitIndex);

      if (bitIndex + numBits > bitsPerBlock) {
        // Value spans two blocks
        size_t const bitsInFirstBlock = bitsPerBlock - bitIndex;
        size_t const bitsInSecondBlock = numBits - bitsInFirstBlock;
        T const secondMask = ((static_cast<T>(1) << bitsInSecondBlock) - 1);
        T const secondClearMask = ~secondMask;
        data[blockIndex + 1] = (data[blockIndex + 1] & secondClearMask) |
                                (value >> bitsInFirstBlock);
      }
    }
  };
};

/**
 * \brief Find position of last set bit (convenience function)
 */
inline int findLastSet(uint64_t value) {
  return Bits::findLastSet(value);
}

/**
 * \brief Find position of first set bit (convenience function)
 */
inline int findFirstSet(uint64_t value) {
  return Bits::findFirstSet(value);
}

/**
 * \brief Packed bit operations template (folly::Bits<T> replacement)
 */
template <typename T>
using PackedBits = Bits::PackedBits<T>;

} // namespace dwarfs::compat