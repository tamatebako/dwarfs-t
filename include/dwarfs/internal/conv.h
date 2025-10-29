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
 * \file conv.h
 * \brief String conversion utilities (folly::Conv replacement)
 *
 * Provides string conversion functions compatible with folly::to/tryTo API.
 * Uses std::from_chars/to_chars for performance and boost::lexical_cast
 * for fallback.
 */

#include <charconv>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace dwarfs::compat {

/**
 * \brief Exception thrown on conversion errors
 */
class ConversionError : public std::runtime_error {
 public:
  explicit ConversionError(const std::string& msg)
      : std::runtime_error(msg) {}
};

/**
 * \brief Create a conversion error with details
 */
inline ConversionError makeConversionError(const std::string& from_type,
                                           const std::string& to_type,
                                           const std::string& value) {
  return ConversionError("Cannot convert '" + value + "' from " + from_type +
                         " to " + to_type);
}

namespace detail {

// Helper: Check if type is supported by std::from_chars
template <typename T>
struct is_from_chars_compatible
    : std::bool_constant<std::is_integral_v<T> || std::is_floating_point_v<T>> {
};

// String to arithmetic type using std::from_chars
template <typename T>
std::enable_if_t<is_from_chars_compatible<T>::value, T>
from_string_impl(std::string_view sv) {
  T value{};
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

  if (ec != std::errc{}) {
    if (ec == std::errc::invalid_argument) {
      throw makeConversionError("string", "numeric", std::string(sv));
    } else if (ec == std::errc::result_out_of_range) {
      throw ConversionError("Numeric value out of range: " + std::string(sv));
    }
  }

  // Check if entire string was consumed
  if (ptr != sv.data() + sv.size()) {
    throw makeConversionError("string", "numeric", std::string(sv));
  }

  return value;
}

// Arithmetic type to string using std::to_chars
template <typename T>
std::enable_if_t<is_from_chars_compatible<T>::value, std::string>
to_string_impl(T value) {
  std::array<char, 64> buffer;
  auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);

  if (ec != std::errc{}) {
    throw ConversionError("Failed to convert numeric value to string");
  }

  return std::string(buffer.data(), ptr - buffer.data());
}

} // namespace detail

// Forward declarations for specializations
template <typename T>
std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, T>
to(std::string_view value);

template <typename T>
std::enable_if_t<std::is_same<T, bool>::value, bool>
to(std::string_view value);

template <typename T>
std::enable_if_t<std::is_same<T, std::string>::value, std::string>
to(std::string_view value);

template <typename T>
std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, std::string>
to(T value);

template <typename T>
std::enable_if_t<std::is_same<T, bool>::value, std::string>
to(bool value);

/**
 * \brief Convert string to arithmetic type (folly::to replacement)
 */
template <typename T>
std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, T>
to(std::string_view value) {
  return detail::from_string_impl<T>(value);
}

// Specialization for bool
template <typename T>
std::enable_if_t<std::is_same<T, bool>::value, bool>
to(std::string_view value) {
  if (value == "1" || value == "true" || value == "True" || value == "TRUE") {
    return true;
  }
  if (value == "0" || value == "false" || value == "False" || value == "FALSE") {
    return false;
  }
  throw makeConversionError("string", "bool", std::string(value));
}

// Specialization for std::string
template <typename T>
std::enable_if_t<std::is_same<T, std::string>::value, std::string>
to(std::string_view value) {
  return std::string(value);
}

/**
 * \brief Convert arithmetic type to string
 */
template <typename T>
std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, std::string>
to(T value) {
  return detail::to_string_impl(value);
}

// Specialization for bool to string
template <typename T>
std::enable_if_t<std::is_same<T, bool>::value, std::string>
to(bool value) {
  return value ? "true" : "false";
}

/**
 * \brief Try to convert string to type T (folly::tryTo replacement)
 *
 * Returns std::nullopt on failure instead of throwing.
 *
 * \tparam T Target type
 * \param value String representation
 * \return Optional containing converted value, or nullopt on error
 */
template <typename T>
std::optional<T> tryTo(std::string_view value) {
  try {
    return to<T>(value);
  } catch (const ConversionError&) {
    return std::nullopt;
  }
}

} // namespace dwarfs::compat