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
 * Folly Compatibility Layer
 *
 * Provides STL and Boost-based alternatives to Folly features.
 * When DWARFS_HAVE_FOLLY is defined, uses Folly directly for optimal performance.
 * Otherwise, provides functionally equivalent implementations.
 *
 * Categories:
 * - Portability (Windows, Unix, PThread APIs)
 * - Data Structures (Function, small_vector, Synchronized)
 * - Utilities (Conv, String, Hash)
 * - Performance (Histogram, ThreadName)
 * - Language Features (Bits, Assume)
 */

#include <dwarfs/config.h>

#ifdef DWARFS_HAVE_FOLLY
// Use Folly directly when available (optimal performance)
#include <folly/Conv.h>
#include <folly/Demangle.h>
#include <folly/ExceptionString.h>
#include <folly/FileUtil.h>
#include <folly/Function.h>
#include <folly/Hash.h>
#include <folly/String.h>
#include <folly/Synchronized.h>
#include <folly/Varint.h>
#include <folly/hash/Hash.h>
#include <folly/lang/Assume.h>
#include <folly/lang/Bits.h>
#include <folly/lang/BitsClass.h>
#include <folly/small_vector.h>
#include <folly/sorted_vector_types.h>
#include <folly/stats/Histogram.h>
#include <folly/system/HardwareConcurrency.h>
#include <folly/system/ThreadName.h>

// Portability headers
#include <folly/portability/Fcntl.h>
#include <folly/portability/PThread.h>
#include <folly/portability/Stdlib.h>
#include <folly/portability/SysStat.h>
#include <folly/portability/Unistd.h>
#include <folly/portability/Windows.h>

namespace dwarfs::compat {

// Direct passthrough to Folly
using folly::Function;
using folly::Synchronized;
using folly::hash;
using folly::small_vector;
using folly::sorted_vector_map;
using folly::sorted_vector_set;

namespace portability {
using namespace folly::portability;
} // namespace portability

namespace system {
using folly::hardware_concurrency;
using folly::setThreadName;
} // namespace system

namespace stats {
using folly::Histogram;
} // namespace stats

namespace lang {
using folly::assume;
using folly::assume_unreachable;
using folly::Bits;
} // namespace lang

template<typename T, typename... Args>
inline auto to(Args&&... args) -> decltype(folly::to<T>(std::forward<Args>(args)...)) {
  return folly::to<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
inline auto tryTo(Args&&... args) -> decltype(folly::tryTo<T>(std::forward<Args>(args)...)) {
  return folly::tryTo<T>(std::forward<Args>(args)...);
}

inline std::string demangle(char const* name) {
  return folly::demangle(name);
}

inline std::string exceptionStr(std::exception const& e) {
  return folly::exceptionStr(e);
}

} // namespace dwarfs::compat

#else // !DWARFS_HAVE_FOLLY

// STL/Boost-based alternatives when Folly not available

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iomanip>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <boost/container/small_vector.hpp>
#include <boost/functional/hash.hpp>

#ifdef __GNUG__
#include <cxxabi.h>
#endif
#include <cstring>

// Platform-specific headers
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace dwarfs::compat {

//==============================================================================
// Data Structures
//==============================================================================

// Function: std::function-like but with move-only semantics
template<typename Signature>
using Function = std::function<Signature>;

// small_vector: Use Boost implementation
template<typename T, std::size_t N>
using small_vector = boost::container::small_vector<T, N>;

// Synchronized: Simple mutex-protected wrapper
template<typename T>
class Synchronized {
public:
  template<typename... Args>
  explicit Synchronized(Args&&... args) : data_(std::forward<Args>(args)...) {}

  template<typename F>
  auto withRLock(F&& f) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::forward<F>(f)(data_);
  }

  template<typename F>
  auto withWLock(F&& f) {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::forward<F>(f)(data_);
  }

  template<typename F>
  auto withLock(F&& f) {
    return withWLock(std::forward<F>(f));
  }

private:
  mutable std::mutex mutex_;
  T data_;
};

// sorted_vector_map: Simple wrapper around std::map
template<typename K, typename V>
using sorted_vector_map = std::map<K, V>;

// sorted_vector_set: Simple wrapper around std::set
template<typename K>
using sorted_vector_set = std::set<K>;

//==============================================================================
// Conversion Utilities
//==============================================================================

// to_underlying: Convert enum to underlying type (C++23 feature, implemented for C++20)
template<typename Enum>
constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept {
  return static_cast<std::underlying_type_t<Enum>>(e);
}

// tryTo: Type conversion with optional result
template<typename T>
std::optional<T> tryTo(std::string_view s) {
  if constexpr (std::is_same_v<T, bool>) {
    std::string lower;
    lower.reserve(s.size());
    std::transform(s.begin(), s.end(), std::back_inserter(lower),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on") {
      return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off") {
      return false;
    }
    return std::nullopt;
  } else if constexpr (std::is_integral_v<T>) {
    try {
      if constexpr (std::is_signed_v<T>) {
        if constexpr (sizeof(T) <= sizeof(long)) {
          return static_cast<T>(std::stol(std::string(s)));
        } else {
          return static_cast<T>(std::stoll(std::string(s)));
        }
      } else {
        if constexpr (sizeof(T) <= sizeof(unsigned long)) {
          return static_cast<T>(std::stoul(std::string(s)));
        } else {
          return static_cast<T>(std::stoull(std::string(s)));
        }
      }
    } catch (...) {
      return std::nullopt;
    }
  } else if constexpr (std::is_floating_point_v<T>) {
    try {
      if constexpr (std::is_same_v<T, float>) {
        return std::stof(std::string(s));
      } else if constexpr (std::is_same_v<T, double>) {
        return std::stod(std::string(s));
      } else {
        return std::stold(std::string(s));
      }
    } catch (...) {
      return std::nullopt;
    }
  }
  return std::nullopt;
}

// to: Type conversion that throws on failure
template<typename T, typename U>
T to(U&& u) {
  if constexpr (std::is_convertible_v<U, T>) {
    return static_cast<T>(std::forward<U>(u));
  } else {
    auto result = tryTo<T>(std::forward<U>(u));
    if (!result) {
      throw std::runtime_error("conversion failed");
    }
    return *result;
  }
}

//==============================================================================
// Hash Utilities
//==============================================================================

namespace hash {

template<typename T>
inline std::size_t hash_combine(std::size_t seed, T const& v) {
  boost::hash_combine(seed, v);
  return seed;
}

// Variadic hash_combine
template<typename T, typename... Args>
inline std::size_t hash_combine(std::size_t seed, T const& v, Args const&... args) {
  seed = hash_combine(seed, v);
  if constexpr (sizeof...(args) > 0) {
    seed = hash_combine(seed, args...);
  }
  return seed;
}

template<typename T>
inline std::size_t hash_value(T const& v) {
  return boost::hash<T>()(v);
}

} // namespace hash

// Global hash function (single name to avoid conflict with hash namespace)
template<typename T>
inline std::size_t hash_one(T const& v) {
  return boost::hash<T>()(v);
}

// Hash functor for unordered containers
template<typename T>
struct Hash {
  std::size_t operator()(T const& v) const {
    return hash_one(v);
  }
};

//==============================================================================
// String Utilities
//==============================================================================

// Pretty print unit types
enum PrettyType {
  PRETTY_BYTES_IEC,
  PRETTY_BYTES,
  PRETTY_TIME,
  PRETTY_TIME_HMS,
  PRETTY_SI,
  PRETTY_UNITS_METRIC,
  PRETTY_UNITS_BINARY,
  PRETTY_UNITS_BINARY_IEC
};

inline std::string prettyPrint(double value, PrettyType type, bool addSpace = true) {
  std::ostringstream oss;

  std::string space = addSpace ? " " : "";

  switch (type) {
    case PRETTY_BYTES_IEC:
    case PRETTY_UNITS_BINARY_IEC: {
      constexpr double KiB = 1024.0;
      constexpr double MiB = KiB * 1024.0;
      constexpr double GiB = MiB * 1024.0;
      constexpr double TiB = GiB * 1024.0;
      constexpr double PiB = TiB * 1024.0;

      auto format_size = [&](double val, const char* unit) {
        // Check if it's a whole number (within floating point precision)
        double intpart;
        constexpr double epsilon = 1e-9;
        if (std::fabs(std::modf(val, &intpart)) < epsilon) {
          oss << static_cast<int>(val) << space << unit;
        } else {
          // Format with up to 2 decimals, removing trailing zeros
          char buf[32];
          std::snprintf(buf, sizeof(buf), "%.2f", val);
          // Remove trailing zeros
          char* end = buf + std::strlen(buf) - 1;
          while (end > buf && *end == '0') {
            *end = '\0';
            --end;
          }
          // Remove trailing dot if present
          if (end > buf && *end == '.') {
            *end = '\0';
          }
          oss << buf << space << unit;
        }
      };

      if (value >= PiB) {
        format_size(value / PiB, "PiB");
      } else if (value >= TiB) {
        format_size(value / TiB, "TiB");
      } else if (value >= GiB) {
        format_size(value / GiB, "GiB");
      } else if (value >= MiB) {
        format_size(value / MiB, "MiB");
      } else if (value >= KiB) {
        format_size(value / KiB, "KiB");
      } else {
        oss << static_cast<int>(value) << space << "B";
      }
      break;
    }

    case PRETTY_TIME_HMS: {
      int hours = static_cast<int>(value / 3600.0);
      int minutes = static_cast<int>((value - hours * 3600.0) / 60.0);
      double seconds = value - hours * 3600.0 - minutes * 60.0;

      if (hours > 0) {
        oss << hours << "h" << minutes << "m" << std::fixed << std::setprecision(0) << seconds << "s";
      } else if (minutes > 0) {
        // Format as fractional minutes: "1.75m" for 105s
        double total_minutes = value / 60.0;
        double intpart;
        double fracpart = std::modf(total_minutes, &intpart);
        if (std::abs(fracpart) < 1e-9) {  // Check for nearly zero fractional part
          oss << static_cast<int>(intpart) << "m";
        } else {
          char buf[32];
          std::snprintf(buf, sizeof(buf), "%.3f", total_minutes);
          // Remove trailing zeros and potentially trailing dot
          char* end = buf + std::strlen(buf) - 1;
          while (end > buf && *end == '0') {
            --end;
          }
          if (end > buf && *end == '.') {
            --end;
          }
          *(end + 1) = '\0';
          oss << buf << "m";
        }
      } else if (seconds >= 1.0) {
        // Format seconds
        double intpart;
        double fracpart = std::modf(seconds, &intpart);
        if (std::abs(fracpart) < 1e-9) {
          oss << static_cast<int>(intpart) << space << "s";
        } else {
          char buf[32];
          std::snprintf(buf, sizeof(buf), "%.1f", seconds);
          oss << buf << space << "s";
        }
      } else if (std::fabs(seconds) < 1e-9) {
        oss << "0s";
      } else if (seconds >= 0.001) {
        // Milliseconds
        double ms = seconds * 1000.0;
        double intpart;
        double fracpart = std::modf(ms, &intpart);
        if (std::abs(fracpart) < 1e-9) {
          oss << static_cast<int>(intpart) << space << "ms";
        } else {
          oss << std::fixed << std::setprecision(2) << ms << space << "ms";
        }
      } else if (seconds >= 0.000001) {
        // Microseconds
        double us = seconds * 1000000.0;
        double intpart;
        double fracpart = std::modf(us, &intpart);
        if (std::abs(fracpart) < 1e-9) {
          oss << static_cast<int>(intpart) << space << "us";
        } else {
          oss << std::fixed << std::setprecision(1) << us << space << "us";
        }
      } else {
        // Nanoseconds
        double ns = seconds * 1000000000.0;
        double intpart;
        double fracpart = std::modf(ns, &intpart);
        if (std::abs(fracpart) < 1e-9) {
          oss << static_cast<int>(intpart) << space << "ns";
        } else {
          oss << std::fixed << std::setprecision(2) << ns << space << "ns";
        }
      }
      break;
    }

    default:
      oss << value;
      break;
  }

  return oss.str();
}

inline std::string hexDump(void const* data, size_t size) {
  std::ostringstream oss;
  auto const* bytes = static_cast<uint8_t const*>(data);

  for (size_t i = 0; i < size; ++i) {
    if (i > 0 && i % 16 == 0) {
      oss << '\n';
    } else if (i > 0 && i % 8 == 0) {
      oss << "  ";
    } else if (i > 0) {
      oss << ' ';
    }
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
  }

  return oss.str();
}

inline std::string hexlify(std::span<uint8_t const> data) {
  constexpr char hex_chars[] = "0123456789abcdef";
  std::string result;
  result.reserve(data.size() * 2);

  for (uint8_t byte : data) {
    result.push_back(hex_chars[byte >> 4]);
    result.push_back(hex_chars[byte & 0x0F]);
  }

  return result;
}

inline std::string hexlify(std::string_view sv) {
  auto const* data = reinterpret_cast<uint8_t const*>(sv.data());
  return hexlify(std::span<uint8_t const>(data, sv.size()));
}

inline std::vector<std::string> split(std::string_view s, char delim) {
  std::vector<std::string> result;
  size_t start = 0;
  size_t end = s.find(delim);

  while (end != std::string_view::npos) {
    result.emplace_back(s.substr(start, end - start));
    start = end + 1;
    end = s.find(delim, start);
  }

  result.emplace_back(s.substr(start));
  return result;
}

inline std::string join(std::vector<std::string> const& parts, std::string_view delim) {
  if (parts.empty()) {
    return "";
  }

  std::string result = parts[0];
  for (size_t i = 1; i < parts.size(); ++i) {
    result += delim;
    result += parts[i];
  }
  return result;
}

//==============================================================================
// Exception Utilities
//==============================================================================

inline std::string exceptionStr(std::exception const& e) {
  return e.what();
}

inline std::string exceptionStr(std::exception_ptr const& e) {
  try {
    if (e) {
      std::rethrow_exception(e);
    }
  } catch (std::exception const& ex) {
    return ex.what();
  } catch (...) {
    return "unknown exception";
  }
  return "no exception";
}

inline std::string demangle(char const* name) {
#ifdef __GNUG__
  int status = 0;
  char* demangled = ::abi::__cxa_demangle(name, nullptr, nullptr, &status);
  if (status == 0 && demangled) {
    std::string result(demangled);
    std::free(demangled);
    return result;
  }
#endif
  return name;
}

//==============================================================================
// System Utilities
//==============================================================================

namespace system {

inline unsigned int hardware_concurrency() {
  auto hc = std::thread::hardware_concurrency();
  return hc > 0 ? hc : 1;
}

inline bool setThreadName(std::string_view name) {
#if defined(__linux__) && !defined(__ANDROID__)
  return pthread_setname_np(pthread_self(), std::string(name).c_str()) == 0;
#elif defined(__APPLE__)
  return pthread_setname_np(std::string(name).c_str()) == 0;
#elif defined(_WIN32)
  // Windows thread naming requires Win32 API
  (void)name;
  return false; // Not critical - just skip
#else
  (void)name;
  return false;
#endif
}

} // namespace system

//==============================================================================
// Performance Monitoring
//==============================================================================

namespace stats {

// Simple histogram implementation (when Folly unavailable)
template<typename T>
class Histogram {
public:
  Histogram(T bucket_size, T min, T max)
    : bucket_size_(bucket_size), min_(min), max_(max)
  {
    size_t num_buckets = static_cast<size_t>((max - min) / bucket_size) + 1;
    buckets_.resize(num_buckets, 0);
  }

  void addValue(T value) {
    ++count_;
    sum_ += value;
    values_.push_back(value);
    if (count_ == 1 || value < min_val_) min_val_ = value;
    if (count_ == 1 || value > max_val_) max_val_ = value;

    // Add to bucket
    if (value >= min_ && value <= max_) {
      size_t bucket = static_cast<size_t>((value - min_) / bucket_size_);
      if (bucket < buckets_.size()) {
        buckets_[bucket]++;
      }
    }
  }

  uint64_t computeTotalCount() const { return count_; }
  T getPercentileEstimate(double percentile) const {
    if (values_.empty()) return T{};

    std::vector<T> sorted = values_;
    std::sort(sorted.begin(), sorted.end());

    size_t index = static_cast<size_t>(percentile * sorted.size());
    if (index >= sorted.size()) index = sorted.size() - 1;

    return sorted[index];
  }

  uint64_t count() const { return count_; }
  T sum() const { return sum_; }
  T min_value() const { return min_val_; }
  T max_value() const { return max_val_; }
  double avg() const { return count_ > 0 ? static_cast<double>(sum_) / count_ : 0.0; }

private:
  T bucket_size_;
  T min_;
  T max_;
  std::vector<uint64_t> buckets_;
  std::vector<T> values_; // For percentile calculation
  uint64_t count_{0};
  T sum_{};
  T min_val_{};
  T max_val_{};
};

} // namespace stats

//==============================================================================
// Language Features
//==============================================================================

namespace lang {

[[noreturn]] inline void assume_unreachable() {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_unreachable();
#elif defined(_MSC_VER)
  __assume(false);
#else
  std::abort(); // Fallback for unknown compilers
#endif
}

// assume: Compiler hint (or no-op)
inline void assume([[maybe_unused]] bool condition) {
#if defined(__clang__)
  __builtin_assume(condition);
#elif defined(__GNUC__) && __GNUC__ >= 13
  if (!condition) __builtin_unreachable();
#elif defined(_MSC_VER)
  __assume(condition);
#endif
}

// findLastSet: Find position of highest bit set (1-indexed, 0 if no bits set)
inline int findLastSet(uint64_t value) {
  if (value == 0) return 0;
#if defined(__GNUC__) || defined(__clang__)
  return 64 - __builtin_clzll(value);
#elif defined(_MSC_VER)
  unsigned long index;
  _BitScanReverse64(&index, value);
  return static_cast<int>(index) + 1;
#else
  int pos = 0;
  while (value != 0) {
    value >>= 1;
    ++pos;
  }
  return pos;
#endif
}

// Bits: Bit manipulation utilities
template<typename T>
struct Bits {
  using UnderlyingType = T;  // Add this for packed_int_vector compatibility
  static constexpr size_t bitsPerBlock = sizeof(T) * 8;

  static constexpr int findFirstSet(T value) {
    if (value == 0) return 0;
#if defined(__GNUC__) || defined(__clang__)
    if constexpr (sizeof(T) <= sizeof(unsigned int)) {
      return __builtin_ffs(static_cast<unsigned int>(value));
    } else if constexpr (sizeof(T) <= sizeof(unsigned long)) {
      return __builtin_ffsl(static_cast<unsigned long>(value));
    } else {
      return __builtin_ffsll(static_cast<unsigned long long>(value));
    }
#else
    int pos = 1;
    while (pos <= static_cast<int>(sizeof(T) * 8)) {
      if (value & (T(1) << (pos - 1))) {
        return pos;
      }
      ++pos;
    }
    return 0;
#endif
  }

  static constexpr unsigned int popcount(T value) {
#if defined(__GNUC__) || defined(__clang__)
    if constexpr (sizeof(T) <= sizeof(unsigned int)) {
      return __builtin_popcount(static_cast<unsigned int>(value));
    } else if constexpr (sizeof(T) <= sizeof(unsigned long)) {
      return __builtin_popcountl(static_cast<unsigned long>(value));
    } else {
      return __builtin_popcountll(static_cast<unsigned long long>(value));
    }
#else
    unsigned int count = 0;
    while (value) {
      count += value & 1;
      value >>= 1;
    }
    return count;
#endif
  }

  // get: Get N bits starting at bit position
  static T get(T const* data, size_t bit_offset, size_t num_bits) {
    if (num_bits == 0) return 0;

    size_t start_block = bit_offset / bitsPerBlock;
    size_t start_bit = bit_offset % bitsPerBlock;

    T value = 0;
    size_t bits_read = 0;

    while (bits_read < num_bits) {
      size_t bits_in_block = std::min(num_bits - bits_read, bitsPerBlock - start_bit);
      T mask = (T(1) << bits_in_block) - 1;
      value |= ((data[start_block] >> start_bit) & mask) << bits_read;

      bits_read += bits_in_block;
      start_block++;
      start_bit = 0;
    }

    // Sign-extend if T is signed and the sign bit is set
    if constexpr (std::is_signed_v<T>) {
      if (num_bits < bitsPerBlock) {
        T sign_bit = T(1) << (num_bits - 1);
        if (value & sign_bit) {
          // Extend the sign bit to fill the rest of T
          T extend_mask = ~((T(1) << num_bits) - 1);
          value |= extend_mask;
        }
      }
    }

    return value;
  }

  // set: Set N bits starting at bit position
  static void set(T* data, size_t bit_offset, size_t num_bits, T value) {
    if (num_bits == 0) return;

    size_t start_block = bit_offset / bitsPerBlock;
    size_t start_bit = bit_offset % bitsPerBlock;

    size_t bits_written = 0;

    while (bits_written < num_bits) {
      size_t bits_in_block = std::min(num_bits - bits_written, bitsPerBlock - start_bit);
      T mask = (T(1) << bits_in_block) - 1;

      data[start_block] &= ~(mask << start_bit);  // Clear bits
      data[start_block] |= ((value >> bits_written) & mask) << start_bit;  // Set bits

      bits_written += bits_in_block;
      start_block++;
      start_bit = 0;
    }
  }

  // test: Test bit at position
  static bool test(T const* data, size_t bit) {
    size_t byte_index = bit / bitsPerBlock;
    size_t bit_index = bit % bitsPerBlock;
    return (data[byte_index] & (T(1) << bit_index)) != 0;
  }

  // set: Set single bit at position
  static void set(T* data, size_t bit) {
    size_t byte_index = bit / bitsPerBlock;
    size_t bit_index = bit % bitsPerBlock;
    data[byte_index] |= (T(1) << bit_index);
  }

  // clear: Clear bit at position
  static void clear(T* data, size_t bit) {
    size_t byte_index = bit / bitsPerBlock;
    size_t bit_index = bit % bitsPerBlock;
    data[byte_index] &= ~(T(1) << bit_index);
  }
};

} // namespace lang

//==============================================================================
// Portability Layer
//==============================================================================

namespace portability {

// The portability headers mostly just include standard headers
// We can directly include those instead

} // namespace portability

//==============================================================================
// File Utilities
//==============================================================================

namespace file {

inline ssize_t readFull(int fd, void* buf, size_t count) {
  char* ptr = static_cast<char*>(buf);
  size_t remaining = count;

  while (remaining > 0) {
    ssize_t n = ::read(fd, ptr, remaining);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (n == 0) break; // EOF
    ptr += n;
    remaining -= n;
  }

  return count - remaining;
}

inline ssize_t writeFull(int fd, void const* buf, size_t count) {
  char const* ptr = static_cast<char const*>(buf);
  size_t remaining = count;

  while (remaining > 0) {
    ssize_t n = ::write(fd, ptr, remaining);
    if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    ptr += n;
    remaining -= n;
  }

  return count;
}

} // namespace file

//==============================================================================
// Endian Utilities
//==============================================================================

namespace endian {

template<typename T>
inline T big(T value) {
  if constexpr (std::endian::native == std::endian::big || sizeof(T) == 1) {
    return value;
  } else {
    static_assert(sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);
#ifdef _MSC_VER
    if constexpr (sizeof(T) == 2) {
      return static_cast<T>(_byteswap_ushort(static_cast<uint16_t>(value)));
    } else if constexpr (sizeof(T) == 4) {
      return static_cast<T>(_byteswap_ulong(static_cast<uint32_t>(value)));
    } else if constexpr (sizeof(T) == 8) {
      return static_cast<T>(_byteswap_uint64(static_cast<uint64_t>(value)));
    }
#else
    if constexpr (sizeof(T) == 2) {
      return static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(value)));
    } else if constexpr (sizeof(T) == 4) {
      return static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(value)));
    } else if constexpr (sizeof(T) == 8) {
      return static_cast<T>(__builtin_bswap64(static_cast<uint64_t>(value)));
    }
#endif
  }
}

template<typename T>
inline T little(T value) {
  if constexpr (std::endian::native == std::endian::little || sizeof(T) == 1) {
    return value;
  } else {
    return big(value); // On big-endian system, swap to get little
  }
}

} // namespace endian

//==============================================================================
// Varint (Variable-length integer encoding)
//==============================================================================

inline size_t encodeVarint(uint64_t val, uint8_t* buf) {
  size_t size = 0;
  while (val >= 128) {
    buf[size++] = static_cast<uint8_t>((val & 0x7F) | 0x80);
    val >>= 7;
  }
  buf[size++] = static_cast<uint8_t>(val);
  return size;
}

inline size_t decodeVarint(uint8_t const* buf, uint64_t* val) {
  *val = 0;
  unsigned int shift = 0;
  size_t size = 0;

  while (true) {
    uint8_t byte = buf[size++];
    *val |= static_cast<uint64_t>(byte & 0x7F) << shift;
    if (!(byte & 0x80)) break;
    shift += 7;
    if (shift >= 64) return 0; // Invalid varint
  }

  return size;
}

} // namespace dwarfs::compat

// Struct packing macros
#if defined(__GNUC__) || defined(__clang__)
  #define FOLLY_PACK_ATTR __attribute__((__packed__))
  #define FOLLY_PACK_PUSH
  #define FOLLY_PACK_POP
#elif defined(_MSC_VER)
  #define FOLLY_PACK_ATTR
  #define FOLLY_PACK_PUSH __pragma(pack(push, 1))
  #define FOLLY_PACK_POP __pragma(pack(pop))
#else
  #define FOLLY_PACK_ATTR
  #define FOLLY_PACK_PUSH
  #define FOLLY_PACK_POP
  #warning "Struct packing not supported on this compiler"
#endif

#endif // DWARFS_HAVE_FOLLY