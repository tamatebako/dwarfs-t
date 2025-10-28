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
 * \file exception_string.h
 * \brief Exception formatting utilities (folly::exceptionStr replacement)
 *
 * Provides utilities for converting exceptions to strings.
 */

#include <exception>
#include <string>

namespace dwarfs::compat {

/**
 * \brief Get string representation of an exception
 *
 * Replacement for folly::exceptionStr(const std::exception&).
 * Returns the what() message from the exception.
 *
 * \param ex Exception to convert
 * \return String representation of the exception
 */
inline std::string exceptionStr(const std::exception& ex) {
  return ex.what();
}

/**
 * \brief Get string representation of an exception pointer
 *
 * Replacement for folly::exceptionStr(std::exception_ptr).
 * Attempts to rethrow and extract the what() message.
 *
 * \param ep Exception pointer to convert
 * \return String representation of the exception
 */
inline std::string exceptionStr(const std::exception_ptr& ep) {
  if (!ep) {
    return "No exception";
  }

  try {
    std::rethrow_exception(ep);
  } catch (const std::exception& ex) {
    return ex.what();
  } catch (...) {
    return "Unknown exception";
  }
}

/**
 * \brief Get string representation of current exception
 *
 * Captures and returns the what() message from std::current_exception().
 *
 * \return String representation of the current exception
 */
inline std::string exceptionStr() {
  return exceptionStr(std::current_exception());
}

} // namespace dwarfs::compat