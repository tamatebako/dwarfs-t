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
 * \file unreachable.h
 * \brief Compiler hints for unreachable code (folly::assume_unreachable replacement)
 *
 * Provides platform-specific macros for marking unreachable code paths.
 */

#include <exception>

namespace dwarfs::compat {

/**
 * \brief Mark code path as unreachable
 *
 * Replacement for folly::assume_unreachable().
 * This is a compiler hint that helps with optimization and may improve
 * code generation by allowing the compiler to assume this path is never taken.
 *
 * WARNING: If this path is actually reached at runtime, the behavior is undefined.
 */
#if defined(__GNUC__) || defined(__clang__)
  #define DWARFS_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
  #define DWARFS_UNREACHABLE() __assume(0)
#else
  // Fallback: do nothing (safe but no optimization)
  #define DWARFS_UNREACHABLE() do {} while (0)
#endif

/**
 * \brief Assume a condition is true (compiler hint)
 *
 * Similar to assume_unreachable, but for conditions.
 * Replacement for folly::assume(condition).
 *
 * \param condition Condition to assume is true
 */
#if defined(__clang__)
  #define DWARFS_ASSUME(condition) __builtin_assume(condition)
#elif defined(_MSC_VER)
  #define DWARFS_ASSUME(condition) __assume(condition)
#elif defined(__GNUC__)
  #define DWARFS_ASSUME(condition) \
    do { if (!(condition)) __builtin_unreachable(); } while (0)
#else
  #define DWARFS_ASSUME(condition) do { (void)(condition); } while (0)
#endif

/**
 * \brief Function wrapper for assume_unreachable (for compatibility)
 */
[[noreturn]] inline void assume_unreachable() {
  DWARFS_UNREACHABLE();
  // Should never reach here, but add explicit termination for safety
  std::terminate();
}

} // namespace dwarfs::compat