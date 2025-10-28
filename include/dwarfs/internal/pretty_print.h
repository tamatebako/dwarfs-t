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
 * \file pretty_print.h
 * \brief Pretty printing utilities (folly::prettyPrint replacement)
 *
 * Provides utilities for formatting numbers in human-readable form.
 */

#include <cmath>
#include <cstdint>
#include <string>

namespace dwarfs::compat {

/**
 * \brief Pretty print type
 */
enum PrettyType {
  PRETTY_BYTES_IEC, ///< Bytes with IEC units (KiB, MiB, etc.)
  PRETTY_TIME_HMS,  ///< Time in hours:minutes:seconds format
};

/**
 * \brief Format bytes with IEC units
 *
 * \param bytes Number of bytes
 * \param addSpace Add space between number and unit
 * \return Formatted string (e.g., "1.5 GiB")
 */
inline std::string prettyPrintBytes(uint64_t bytes, bool addSpace = false) {
  static constexpr const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
  static constexpr size_t num_units = sizeof(units) / sizeof(units[0]);

  if (bytes == 0) {
    return addSpace ? "0 B" : "0B";
  }

  size_t unit_index = 0;
  double value = static_cast<double>(bytes);

  while (value >= 1024.0 && unit_index < num_units - 1) {
    value /= 1024.0;
    ++unit_index;
  }

  char buffer[64];
  int len;

  if (unit_index == 0) {
    // Bytes - no decimal places
    len = std::snprintf(buffer, sizeof(buffer), addSpace ? "%.0f %s" : "%.0f%s",
                       value, units[unit_index]);
  } else if (value >= 100.0) {
    // 100+ - no decimal places
    len = std::snprintf(buffer, sizeof(buffer), addSpace ? "%.0f %s" : "%.0f%s",
                       value, units[unit_index]);
  } else if (value >= 10.0) {
    // 10-99 - 1 decimal place
    len = std::snprintf(buffer, sizeof(buffer), addSpace ? "%.1f %s" : "%.1f%s",
                       value, units[unit_index]);
  } else {
    // < 10 - 2 decimal places
    len = std::snprintf(buffer, sizeof(buffer), addSpace ? "%.2f %s" : "%.2f%s",
                       value, units[unit_index]);
  }

  return std::string(buffer, len);
}

/**
 * \brief Format time in HMS format
 *
 * \param seconds Number of seconds
 * \param addSpace Add space (currently ignored for compatibility)
 * \return Formatted string (e.g., "1h 23m 45s")
 */
inline std::string prettyPrintTime(double seconds, bool addSpace = false) {
  if (seconds < 0) {
    return "-" + prettyPrintTime(-seconds, addSpace);
  }

  if (seconds == 0) {
    return "0s";
  }

  uint64_t total_sec = static_cast<uint64_t>(seconds);
  uint64_t hours = total_sec / 3600;
  uint64_t minutes = (total_sec % 3600) / 60;
  uint64_t secs = total_sec % 60;

  // Get fractional seconds
  double frac_sec = seconds - total_sec;

  std::string result;

  if (hours > 0) {
    result += std::to_string(hours) + "h";
    if (minutes > 0 || secs > 0) {
      result += " ";
    }
  }

  if (minutes > 0) {
    result += std::to_string(minutes) + "m";
    if (secs > 0 || (hours == 0 && frac_sec > 0)) {
      result += " ";
    }
  }

  if (secs > 0 || (hours == 0 && minutes == 0)) {
    if (hours == 0 && minutes == 0 && frac_sec > 0) {
      // Show fractional seconds for sub-second values
      char buffer[32];
      std::snprintf(buffer, sizeof(buffer), "%.3fs", seconds);
      result += buffer;
    } else {
      result += std::to_string(secs) + "s";
    }
  }

  return result;
}

/**
 * \brief Pretty print a value
 *
 * Replacement for folly::prettyPrint.
 *
 * \param value Value to format
 * \param type Formatting type
 * \param addSpace Add space between value and unit
 * \return Formatted string
 */
inline std::string prettyPrint(uint64_t value, PrettyType type, bool addSpace) {
  switch (type) {
  case PRETTY_BYTES_IEC:
    return prettyPrintBytes(value, addSpace);
  default:
    return std::to_string(value);
  }
}

/**
 * \brief Pretty print a time value
 */
inline std::string prettyPrint(double value, PrettyType type, bool addSpace) {
  switch (type) {
  case PRETTY_TIME_HMS:
    return prettyPrintTime(value, addSpace);
  default:
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    return buffer;
  }
}

} // namespace dwarfs::compat