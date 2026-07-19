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

// Windows.h defines macros that conflict with our enum values
// Undefine ALL potentially conflicting macros before any includes
#ifdef _WIN32
#undef ERROR
#undef WARN
#undef FATAL
#undef INFO
#undef VERBOSE
#undef DEBUG
#undef TRACE
#endif

#include <cstdint>

namespace dwarfs {

// Define logger level enum OUTSIDE of logger class to avoid MSVC parsing issues
// Using a standalone enum with explicit values to prevent macro conflicts
enum logger_level_type : unsigned {
  LOGGER_LEVEL_FATAL = 0,
  LOGGER_LEVEL_ERROR = 1,
  LOGGER_LEVEL_WARN = 2,
  LOGGER_LEVEL_INFO = 3,
  LOGGER_LEVEL_VERBOSE = 4,
  LOGGER_LEVEL_DEBUG = 5,
  LOGGER_LEVEL_TRACE = 6
};

} // namespace dwarfs
