# Folly Replacement Strategy

**Date:** 2025-10-28
**Phase:** Track B Phase 5 - Folly Replacement
**Branch:** feature/thrift-folly-removal
**Related:** See [FOLLY_AUDIT_REPORT.md](FOLLY_AUDIT_REPORT.md) for usage analysis

## Overview

This document provides a systematic strategy for replacing Facebook Folly dependencies with C++ standard library and Boost equivalents. The approach prioritizes:

1. **Separation of Concerns**: Replace component-by-component
2. **Minimal Disruption**: Maintain API compatibility where possible
3. **Gradual Migration**: Test after each replacement
4. **Performance Preservation**: Benchmark critical paths

## Replacement Mapping

### Priority 1: Critical Components (High Usage)

#### 1.1 Endian Conversions (25 uses)

**Current:**
```cpp
#include <folly/lang/Bits.h>
folly::Endian::big(value)
folly::Endian::little(value)
```

**Replacement Strategy:**
```cpp
// Option A: C++23 std::byteswap (future-proof)
#include <bit>
std::byteswap(value)

// Option B: Boost (current compatibility)
#include <boost/endian/conversion.hpp>
boost::endian::native_to_big(value)
boost::endian::native_to_little(value)
boost::endian::big_to_native(value)
boost::endian::little_to_native(value)

// Option C: Custom inline functions (no dependencies)
// See include/dwarfs/internal/endian.h
```

**Recommended:** Option B (Boost) for immediate compatibility, transition to Option A when C++23 is baseline.

**Effort:** Low
**Risk:** Low (well-defined behavior)
**Performance Impact:** None (compiler optimizes to same instructions)

#### 1.2 Function Wrapper (10 uses)

**Current:**
```cpp
#include <folly/Function.h>
folly::Function<void()>
```

**Replacement Strategy:**
```cpp
#include <functional>
std::function<void()>
```

**Notes:**
- `folly::Function` is move-only, `std::function` is copyable
- May need to add `const` qualifiers in some places
- Check for move semantics assumptions

**Effort:** Low
**Risk:** Low
**Performance Impact:** Negligible

#### 1.3 String Conversions (6 uses)

**Current:**
```cpp
#include <folly/Conv.h>
folly::to<int>(str)
folly::tryTo<int>(str)
folly::to<std::string>(42)
folly::makeConversionError(...)
```

**Replacement Strategy:**
```cpp
// Option A: C++17 std::from_chars / std::to_chars (best performance)
#include <charconv>
int value;
auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
if (ec == std::errc{}) { /* success */ }

std::array<char, 32> buffer;
auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);

// Option B: Boost.LexicalCast (easier API)
#include <boost/lexical_cast.hpp>
int value = boost::lexical_cast<int>(str);
try {
  int value = boost::lexical_cast<int>(str);
} catch (const boost::bad_lexical_cast&) {
  // Handle error
}

// Option C: Custom wrapper combining both
#include <dwarfs/internal/conv.h>
dwarfs::to<int>(str)
dwarfs::try_to<int>(str)
```

**Recommended:** Option C (custom wrapper) for API compatibility and flexibility.

**Effort:** Medium
**Risk:** Medium (error handling behavior must match)
**Performance Impact:** Comparable with std::from_chars

### Priority 2: Container Types

#### 2.1 small_vector (6 uses)

**Current:**
```cpp
#include <folly/small_vector.h>
folly::small_vector<T, N>
```

**Replacement Strategy:**
```cpp
#include <boost/container/small_vector.hpp>
boost::container::small_vector<T, N>
```

**Notes:**
- API is very similar
- May need to check growth strategies
- Performance characteristics should be equivalent

**Effort:** Low
**Risk:** Low
**Performance Impact:** None expected

#### 2.2 sorted_vector_set/map (2 uses)

**Current:**
```cpp
#include <folly/sorted_vector_types.h>
folly::sorted_vector_set<T>
folly::sorted_vector_map<K, V>
```

**Replacement Strategy:**
```cpp
// Option A: Use std::set/map (simpler, may be slower)
#include <set>
#include <map>
std::set<T>
std::map<K, V>

// Option B: boost::flat_set/flat_map (similar to folly)
#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>
boost::container::flat_set<T>
boost::container::flat_map<K, V>

// Option C: Custom sorted vector wrapper
```

**Recommended:** Option B (Boost flat containers) for similar performance.

**Effort:** Low
**Risk:** Low
**Performance Impact:** Minimal

#### 2.3 ByteRange / span (6 uses)

**Current:**
```cpp
#include <folly/Range.h>
folly::ByteRange
folly::MutableByteRange
folly::Range<T*>
folly::span<T>
```

**Replacement Strategy:**
```cpp
#include <span>
std::span<const uint8_t>  // ByteRange
std::span<uint8_t>        // MutableByteRange
std::span<T>              // Range<T*>
```

**Notes:**
- C++20 std::span is the standard replacement
- API is very similar
- May need to adjust iterator usage

**Effort:** Low
**Risk:** Low
**Performance Impact:** None

### Priority 3: System & Thread Utilities

#### 3.1 Synchronized (5 uses)

**Current:**
```cpp
#include <folly/Synchronized.h>
folly::Synchronized<T> data;
auto locked = data.wlock();
auto locked = data.rlock();
```

**Replacement Strategy:**
```cpp
// Custom wrapper with std::shared_mutex
#include <dwarfs/internal/synchronized.h>

template<typename T>
class Synchronized {
  T data_;
  mutable std::shared_mutex mutex_;

public:
  template<typename F>
  auto with_wlock(F&& f) {
    std::unique_lock lock(mutex_);
    return f(data_);
  }

  template<typename F>
  auto with_rlock(F&& f) const {
    std::shared_lock lock(mutex_);
    return f(data_);
  }
};
```

**Effort:** Medium
**Risk:** Medium (locking semantics must be correct)
**Performance Impact:** None

#### 3.2 Thread Naming (5 uses)

**Current:**
```cpp
#include <folly/system/ThreadName.h>
folly::setThreadName(name)
```

**Replacement Strategy:**
```cpp
// Platform-specific implementation
#include <dwarfs/internal/thread_name.h>

inline void setThreadName(const std::string& name) {
#if defined(__linux__)
  pthread_setname_np(pthread_self(), name.c_str());
#elif defined(__APPLE__)
  pthread_setname_np(name.c_str());
#elif defined(_WIN32)
  // Windows API SetThreadDescription
#endif
}
```

**Effort:** Low
**Risk:** Low
**Performance Impact:** None

#### 3.3 Hardware Concurrency (1 use)

**Current:**
```cpp
#include <folly/system/HardwareConcurrency.h>
folly::hardware_concurrency()
```

**Replacement Strategy:**
```cpp
#include <thread>
std::thread::hardware_concurrency()
```

**Effort:** Trivial
**Risk:** None
**Performance Impact:** None

#### 3.4 Histogram (7 uses)

**Current:**
```cpp
#include <folly/stats/Histogram.h>
folly::Histogram<T>
```

**Replacement Strategy:**
```cpp
// Option A: Boost.Accumulators
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

// Option B: Custom histogram implementation
#include <dwarfs/internal/histogram.h>

template<typename T>
class Histogram {
  std::vector<uint64_t> buckets_;
  std::vector<T> boundaries_;

public:
  void addValue(T value);
  T getPercentileEstimate(double pct) const;
  // ... other methods
};
```

**Recommended:** Option B for exact API compatibility.

**Effort:** Medium-High
**Risk:** Medium
**Performance Impact:** Should be comparable

### Priority 4: Bit Operations & Utilities

#### 4.1 Bit Operations (2 uses)

**Current:**
```cpp
#include <folly/lang/Bits.h>
folly::Bits::findLastSet(value)
folly::findLastSet(value)
```

**Replacement Strategy:**
```cpp
// C++20 std::bit operations
#include <bit>
std::bit_width(value)  // Similar to findLastSet

// Or custom inline functions
inline int findLastSet(uint64_t value) {
#if defined(__GNUC__) || defined(__clang__)
  return value ? 64 - __builtin_clzll(value) : 0;
#elif defined(_MSC_VER)
  unsigned long index;
  return _BitScanReverse64(&index, value) ? index + 1 : 0;
#else
  // Portable fallback
  int pos = 0;
  while (value >>= 1) ++pos;
  return pos;
#endif
}
```

**Effort:** Low
**Risk:** Low
**Performance Impact:** None

#### 4.2 Compiler Hints (4 uses)

**Current:**
```cpp
#include <folly/lang/Assume.h>
folly::assume_unreachable()
```

**Replacement Strategy:**
```cpp
// Compiler-specific macros
#if defined(__GNUC__) || defined(__clang__)
  #define DWARFS_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
  #define DWARFS_UNREACHABLE() __assume(0)
#else
  #define DWARFS_UNREACHABLE() do {} while (0)
#endif
```

**Effort:** Trivial
**Risk:** None
**Performance Impact:** None

#### 4.3 Enum Utilities (2 uses)

**Current:**
```cpp
folly::to_underlying(enum_value)
```

**Replacement Strategy:**
```cpp
// C++23 std::to_underlying, or custom for C++20
template<typename E>
constexpr auto to_underlying(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}
```

**Effort:** Trivial
**Risk:** None
**Performance Impact:** None

### Priority 5: File & String Utilities

#### 5.1 File I/O (2 uses)

**Current:**
```cpp
#include <folly/FileUtil.h>
folly::readFile(path, data)
folly::writeFile(data, path)
```

**Replacement Strategy:**
```cpp
#include <fstream>
#include <iterator>

inline bool readFile(const std::string& path, std::string& data) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  data.assign(std::istreambuf_iterator<char>(file),
              std::istreambuf_iterator<char>());
  return true;
}

inline bool writeFile(const std::string& data, const std::string& path) {
  std::ofstream file(path, std::ios::binary);
  if (!file) return false;
  file.write(data.data(), data.size());
  return file.good();
}
```

**Effort:** Low
**Risk:** Low
**Performance Impact:** Minimal

#### 5.2 Hex Utilities (3 uses)

**Current:**
```cpp
folly::hexlify(data)
folly::hexDump(data)
```

**Replacement Strategy:**
```cpp
#include <dwarfs/internal/hex.h>

std::string hexlify(std::span<const uint8_t> data) {
  static const char hex[] = "0123456789abcdef";
  std::string result;
  result.reserve(data.size() * 2);
  for (uint8_t byte : data) {
    result += hex[byte >> 4];
    result += hex[byte & 0x0f];
  }
  return result;
}
```

**Effort:** Low
**Risk:** Low
**Performance Impact:** None

#### 5.3 Exception Utilities (3 uses)

**Current:**
```cpp
#include <folly/ExceptionString.h>
folly::exceptionStr(ex)
```

**Replacement Strategy:**
```cpp
inline std::string exceptionStr(const std::exception& ex) {
  return ex.what();
}

// For nested exceptions
std::string exceptionStr(const std::exception_ptr& ep) {
  try {
    if (ep) std::rethrow_exception(ep);
  } catch (const std::exception& ex) {
    return ex.what();
  }
  return "unknown exception";
}
```

**Effort:** Low
**Risk:** Low
**Performance Impact:** None

#### 5.4 Symbol Demangling (2 uses)

**Current:**
```cpp
#include <folly/Demangle.h>
folly::demangle(typeid(T).name())
```

**Replacement Strategy:**
```cpp
#include <dwarfs/internal/demangle.h>

inline std::string demangle(const char* name) {
#if defined(__GNUC__) || defined(__clang__)
  int status;
  char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
  if (status == 0 && demangled) {
    std::string result(demangled);
    free(demangled);
    return result;
  }
#endif
  return name;
}
```

**Effort:** Low
**Risk:** Low
**Performance Impact:** None

### Priority 6: Hash Functions (2 uses)

**Current:**
```cpp
#include <folly/hash/Hash.h>
folly::hash::hash_combine(h1, h2)
folly::hash::jenkins_rev_mix32(value)
```

**Replacement Strategy:**
```cpp
#include <boost/container_hash/hash.hpp>

// hash_combine is available in Boost
boost::hash_combine(h1, h2)

// jenkins_rev_mix32 - custom implementation or alternative
inline uint32_t hash_mix32(uint32_t value) {
  // Alternative mixing function
  value = ((value >> 16) ^ value) * 0x45d9f3b;
  value = ((value >> 16) ^ value) * 0x45d9f3b;
  value = (value >> 16) ^ value;
  return value;
}
```

**Effort:** Low
**Risk:** Low (hash values may differ but shouldn't matter)
**Performance Impact:** Minimal

### Priority 7: Varint (2 uses)

**Current:**
```cpp
#include <folly/Varint.h>
folly::encodeVarint(value, buf)
folly::decodeVarint(buf)
```

**Replacement Strategy:**
```cpp
// Check existing implementation in include/dwarfs/varint.h
// If already exists, just use that instead of folly
#include <dwarfs/varint.h>
```

**Note:** Dwarfs may already have a varint implementation. Check before implementing.

**Effort:** Trivial (if exists) or Low (if implementing)
**Risk:** Low
**Performance Impact:** None

### Priority 8: Portability Headers

**Current:**
```cpp
#include <folly/portability/Fcntl.h>
#include <folly/portability/PThread.h>
#include <folly/portability/Stdlib.h>
#include <folly/portability/SysStat.h>
#include <folly/portability/Unistd.h>
#include <folly/portability/Windows.h>
```

**Replacement Strategy:**
```cpp
// Replace with standard headers and conditional compilation
#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
#else
  #include <fcntl.h>
  #include <pthread.h>
  #include <unistd.h>
  #include <sys/stat.h>
#endif
```

**Effort:** Low
**Risk:** Low
**Performance Impact:** None

## Implementation Plan

### Phase 1: Foundation (Week 1)
1. Create compatibility header framework
2. Implement trivial replacements:
   - `folly::Function` → `std::function`
   - `folly::hardware_concurrency()` → `std::thread::hardware_concurrency()`
   - `folly::to_underlying()` → custom template
   - `folly::assume_unreachable()` → compiler macros

### Phase 2: Core Utilities (Week 2)
3. Implement endian conversions (Boost)
4. Implement string conversions wrapper
5. Replace ByteRange with std::span
6. Test and verify

### Phase 3: Containers (Week 3)
7. Replace small_vector (Boost)
8. Replace sorted_vector_* (Boost flat containers)
9. Benchmark performance
10. Test and verify

### Phase 4: System & Advanced (Week 4)
11. Implement Synchronized wrapper
12. Implement thread naming
13. Implement Histogram (custom or Boost.Accumulators)
14. Implement remaining utilities
15. Final testing and documentation

## Testing Strategy

### Unit Tests
- Create tests for each replacement in `test/compat/`
- Verify API compatibility
- Test error handling
- Test edge cases

### Integration Tests
- Run existing test suite after each replacement
- Verify no regressions
- Check for memory leaks (valgrind)

### Performance Tests
- Benchmark critical paths:
  - Endian conversions
  - String conversions
  - Container operations
- Compare before/after performance
- Document any differences

### Platform Tests
- Test on Linux, macOS, Windows
- Verify portability replacements work
- Check compiler compatibility (GCC, Clang, MSVC)

## Risk Mitigation

1. **Incremental Approach**: Replace one component at a time
2. **Compatibility Layer**: Create shim headers for gradual migration
3. **Comprehensive Testing**: Test after each replacement
4. **Performance Monitoring**: Benchmark critical operations
5. **Rollback Plan**: Keep changes in separate commits for easy revert

## Success Criteria

- [ ] All Folly dependencies removed from headers and source
- [ ] All tests passing
- [ ] No performance regressions (< 5% slowdown acceptable)
- [ ] Code compiles on all platforms (Linux, macOS, Windows)
- [ ] Static library compilation enabled
- [ ] Documentation updated

## Estimated Effort

| Phase | Components | Effort | Risk |
|-------|-----------|--------|------|
| Phase 1 | Trivial replacements | 2 days | Low |
| Phase 2 | Core utilities | 3 days | Medium |
| Phase 3 | Containers | 2 days | Low |
| Phase 4 | Advanced | 3 days | Medium |
| **Total** | | **10 days** | |

## Notes

- Some replacements (like Histogram) may require more time if custom implementation needed
- Performance testing may reveal areas needing optimization
- Platform-specific code requires testing on all platforms
- Consider keeping Folly as build option for comparison testing initially