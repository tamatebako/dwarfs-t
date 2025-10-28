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
 * \file demangle.h
 * \brief Symbol demangling utilities (folly::demangle replacement)
 *
 * Provides C++ symbol demangling for debugging and diagnostics.
 */

#include <string>
#include <typeinfo>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#include <cstdlib>
#endif

namespace dwarfs::compat {

/**
 * \brief Demangle C++ symbol name
 *
 * Replacement for folly::demangle.
 * Uses platform-specific demangling (GCC/Clang: __cxa_demangle).
 *
 * \param mangled_name Mangled symbol name (e.g., from typeid(T).name())
 * \return Demangled name, or original name if demangling fails
 */
inline std::string demangle(const char* mangled_name) {
#if defined(__GNUC__) || defined(__clang__)
  int status = 0;
  char* demangled = abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);

  if (status == 0 && demangled) {
    std::string result(demangled);
    std::free(demangled);
    return result;
  }

  // Demangling failed, return original name
  return mangled_name;

#else
  // MSVC doesn't mangle names in typeid().name() the same way
  // Just return the original name
  return mangled_name;
#endif
}

/**
 * \brief Demangle C++ type name
 *
 * Template wrapper for demangling type names.
 *
 * \tparam T Type to demangle
 * \return Demangled type name
 */
template <typename T>
std::string demangle() {
  return demangle(typeid(T).name());
}

} // namespace dwarfs::compat