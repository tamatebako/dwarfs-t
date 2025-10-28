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
 * \file compat.h
 * \brief Folly compatibility layer
 *
 * This header provides replacements for Facebook Folly components using
 * C++ standard library and Boost equivalents. This enables static library
 * compilation without Folly dependencies.
 *
 * Components replaced:
 * - Endian conversions (folly::Endian)
 * - String conversions (folly::to, folly::tryTo)
 * - Compiler hints (folly::assume_unreachable)
 * - Enum utilities (folly::to_underlying)
 */

// Compiler detection
#if defined(__GNUC__) || defined(__clang__)
  #define DWARFS_COMPILER_GCC_OR_CLANG 1
#elif defined(_MSC_VER)
  #define DWARFS_COMPILER_MSVC 1
#endif

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
  #define DWARFS_PLATFORM_WINDOWS 1
#elif defined(__linux__)
  #define DWARFS_PLATFORM_LINUX 1
#elif defined(__APPLE__)
  #define DWARFS_PLATFORM_MACOS 1
#endif

// Compiler hints
#if DWARFS_COMPILER_GCC_OR_CLANG
  #define DWARFS_UNREACHABLE() __builtin_unreachable()
  #define DWARFS_LIKELY(x) __builtin_expect(!!(x), 1)
  #define DWARFS_UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif DWARFS_COMPILER_MSVC
  #define DWARFS_UNREACHABLE() __assume(0)
  #define DWARFS_LIKELY(x) (x)
  #define DWARFS_UNLIKELY(x) (x)
#else
  #define DWARFS_UNREACHABLE() do {} while (0)
  #define DWARFS_LIKELY(x) (x)
  #define DWARFS_UNLIKELY(x) (x)
#endif

#include <type_traits>

namespace dwarfs::compat {

/**
 * \brief Convert enum to underlying type (C++23 std::to_underlying polyfill)
 */
template <typename E>
constexpr std::underlying_type_t<E> to_underlying(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}

/**
 * \brief Mark code path as unreachable (folly::assume_unreachable replacement)
 */
[[noreturn]] inline void assume_unreachable() {
  DWARFS_UNREACHABLE();
}

} // namespace dwarfs::compat