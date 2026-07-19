/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
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

#pragma once

#include <string>
#include <vector>

namespace dwarfs::tool {

#ifdef _WIN32
#define SYS_MAIN wmain
#define SYS_STR(x) L##x
using sys_char = wchar_t;
using sys_string = std::wstring;
#else
#define SYS_MAIN main
#define SYS_STR(x) x
using sys_char = char;
using sys_string = std::string;
#endif

std::string sys_string_to_string(sys_string const& in);
sys_string string_to_sys_string(std::string const& in);

// Helper to convert vector of strings
inline std::vector<sys_string> vector_to_sys_strings(std::vector<std::string> const& in) {
  std::vector<sys_string> out;
  out.reserve(in.size());
  for (auto const& s : in) {
    out.push_back(string_to_sys_string(s));
  }
  return out;
}

} // namespace dwarfs::tool
