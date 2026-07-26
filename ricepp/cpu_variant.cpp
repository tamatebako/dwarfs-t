/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of ricepp.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdlib>
#include <iostream>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#include "cpu_variant.h"

namespace ricepp::detail {

namespace {

#if defined(__APPLE__)

// On Apple platforms, __builtin_cpu_supports() is implemented via the
// compiler-rt builtins __cpu_model and __cpu_indicator_init, which live in
// libclang_rt.osx.a. That archive is only linked implicitly by the clang
// driver; links that don't go through the driver (e.g. statically linked
// binaries) fail with undefined ___cpu_model symbols. Use the hw.optional.*
// sysctls instead, which only depend on libSystem. Keys that don't exist on
// older macOS versions simply report the feature as unavailable.
bool has_cpu_feature(char const* name) {
  int value = 0;
  size_t size = sizeof(value);
  return ::sysctlbyname(name, &value, &size, nullptr, 0) == 0 && value != 0;
}

#endif

detail::cpu_variant get_cpu_variant_init() {
#ifndef _WIN32
#if defined(RICEPP_CPU_BMI2) || defined(RICEPP_CPU_BMI2_AVX512)
  bool has_avx512vl = false;
  bool has_avx512vbmi = false;
  bool has_bmi2 = false;
#if defined(__APPLE__)
  has_avx512vl = has_cpu_feature("hw.optional.avx512vl");
  has_avx512vbmi = has_cpu_feature("hw.optional.avx512vbmi");
  has_bmi2 = has_cpu_feature("hw.optional.bmi2");
#elif defined(__has_builtin)
#if __has_builtin(__builtin_cpu_supports)
  __builtin_cpu_init();

  has_avx512vl = __builtin_cpu_supports("avx512vl");
  has_avx512vbmi = __builtin_cpu_supports("avx512vbmi");
  has_bmi2 = __builtin_cpu_supports("bmi2");
#endif
#endif

  if (has_avx512vl && has_avx512vbmi && has_bmi2) {
    return detail::cpu_variant::has_bmi2_avx512;
  }

  if (has_bmi2) {
    return detail::cpu_variant::has_bmi2;
  }
#endif
#endif

  return detail::cpu_variant::fallback;
}

} // namespace

detail::cpu_variant get_cpu_variant() {
  static detail::cpu_variant const variant = get_cpu_variant_init();
  return variant;
}

void show_cpu_variant(std::string_view variant) {
  if (std::getenv("RICEPP_SHOW_CPU_VARIANT")) {
    std::cerr << "ricepp: using " << variant << " CPU variant\n";
  }
}

void show_cpu_variant_once(std::string_view variant) {
  static auto const _ = [&variant]() {
    show_cpu_variant(variant);
    return true;
  }();
}

} // namespace ricepp::detail
