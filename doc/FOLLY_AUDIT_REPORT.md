# Folly Usage Audit Report

**Date:** 2025-10-28
**Phase:** Track B Phase 5 - Folly Replacement
**Branch:** feature/thrift-folly-removal

## Executive Summary

Total Folly usage found:
- **24 unique Folly headers** included
- **101 instances** of `folly::` namespace usage
- **Primary categories:** Endian utilities, String operations, Containers, System utilities

## Detailed Findings

### 1. Folly Headers Used

#### High-Priority Headers (Core Functionality)
1. `<folly/Conv.h>` - String conversions
2. `<folly/String.h>` - String utilities
3. `<folly/Function.h>` - Type-erased function wrapper
4. `<folly/small_vector.h>` - Small-size optimized vector
5. `<folly/sorted_vector_types.h>` - Sorted containers
6. `<folly/Synchronized.h>` - Thread-safe wrapper
7. `<folly/Varint.h>` - Variable-length integer encoding

#### Medium-Priority Headers (Bit/Endian Operations)
8. `<folly/lang/Bits.h>` - Bit manipulation
9. `<folly/lang/BitsClass.h>` - Bit operations class
10. `<folly/Hash.h>` - Hash functions
11. `<folly/hash/Hash.h>` - Hash utilities

#### Medium-Priority Headers (System Utilities)
12. `<folly/FileUtil.h>` - File I/O utilities
13. `<folly/system/HardwareConcurrency.h>` - CPU count
14. `<folly/system/ThreadName.h>` - Thread naming
15. `<folly/stats/Histogram.h>` - Statistics histogram

#### Low-Priority Headers (Portability)
16. `<folly/portability/Fcntl.h>` - File control portability
17. `<folly/portability/PThread.h>` - pthread portability
18. `<folly/portability/Stdlib.h>` - stdlib portability
19. `<folly/portability/SysStat.h>` - sys/stat portability
20. `<folly/portability/Unistd.h>` - unistd portability
21. `<folly/portability/Windows.h>` - Windows portability

#### Low-Priority Headers (Utilities)
22. `<folly/CPortability.h>` - C compatibility
23. `<folly/Demangle.h>` - Symbol demangling
24. `<folly/ExceptionString.h>` - Exception formatting
25. `<folly/lang/Assume.h>` - Compiler hints

### 2. Folly Namespace Usage Frequency

#### Critical (>10 uses)
- `folly::Endian::big` (15) - Big-endian conversions
- `folly::Function` (10) - Function wrapper
- `folly::Endian::little` (10) - Little-endian conversions

#### High (5-10 uses)
- `folly::Histogram` (7) - Performance statistics
- `folly::small_vector` (6) - Small-optimized vector
- `folly::Synchronized` (5) - Thread-safe wrapper
- `folly::setThreadName` (5) - Thread naming
- `folly::ByteRange` (5) - Byte range view

#### Medium (2-4 uses)
- `folly::to` (4) - String conversion
- `folly::assume_unreachable` (4) - Compiler hint
- `folly::exceptionStr` (3) - Exception formatting
- `folly::tryTo` (2) - Safe string conversion
- `folly::to_underlying` (2) - Enum to underlying type
- `folly::hexlify` (2) - Hex encoding
- `folly::demangle` (2) - Symbol demangling
- `folly::Bits` (2) - Bit operations

#### Low (1 use)
- `folly::writeFile` (1)
- `folly::readFile` (1)
- `folly::span` (1)
- `folly::sorted_vector_set` (1)
- `folly::sorted_vector_map` (1)
- `folly::Range` (1)
- `folly::MutableByteRange` (1)
- `folly::makeConversionError` (1)
- `folly::hexDump` (1)
- `folly::hash::jenkins_rev_mix32` (1)
- `folly::hash::hash_combine` (1)
- `folly::hardware_concurrency` (1)
- `folly::findLastSet` (1)
- `folly::encodeVarint` (1)
- `folly::decodeVarint` (1)
- `folly::PRETTY_TIME_HMS` (1)
- `folly::PRETTY_BYTES_IEC` (1)

### 3. Categorization by Replacement Complexity

#### Simple (Direct std/boost equivalent)
- Endian conversions → `std::byteswap` (C++23) or manual
- `folly::Function` → `std::function`
- `folly::small_vector` → `boost::container::small_vector`
- `folly::ByteRange` → `std::span<const uint8_t>`
- `folly::Range` → `std::span`
- `folly::span` → `std::span`
- `folly::to_underlying` → `std::to_underlying` (C++23) or manual
- `folly::hardware_concurrency` → `std::thread::hardware_concurrency()`
- `folly::sorted_vector_set/map` → `std::set/map` or custom

#### Moderate (Requires wrapper/adaptation)
- `folly::Synchronized` → `std::mutex` + wrapper
- `folly::setThreadName` → Platform-specific calls
- `folly::Histogram` → Custom or Boost.Accumulators
- `folly::to/tryTo` → `std::to_string` + `std::from_chars` or `boost::lexical_cast`
- `folly::Varint` → Custom implementation (already exists in dwarfs)
- `folly::Bits` → Custom bit manipulation
- `folly::assume_unreachable` → `__builtin_unreachable()` or `[[noreturn]]`

#### Complex (Significant effort)
- `folly::exceptionStr` → Custom exception formatting
- `folly::demangle` → Platform-specific demangling
- `folly::hexlify/hexDump` → Custom hex utilities
- `folly::hash::*` → Custom or Boost.Hash
- File I/O utilities → Custom or std::filesystem

### 4. Files with Highest Folly Dependency

```bash
# Top files by Folly usage count (to be generated)
```

## Priority Recommendations

### Phase 1: Core Replacements (Week 1)
1. **Endian conversions** (25 uses) - High impact
2. **Function wrapper** (10 uses) - Simple replacement
3. **String conversions** (6 uses) - Moderate complexity

### Phase 2: Container Replacements (Week 2)
4. **small_vector** (6 uses) - Boost equivalent available
5. **sorted_vector_*` (2 uses) - Custom or std alternative
6. **ByteRange/span** (6 uses) - std::span available

### Phase 3: System Utilities (Week 3)
7. **Synchronized** (5 uses) - Create wrapper
8. **Thread naming** (5 uses) - Platform-specific
9. **Histogram** (7 uses) - Custom implementation

### Phase 4: Portability & Utilities (Week 4)
10. Portability headers - Conditional compilation
11. Bit operations - Custom utilities
12. Hash functions - Boost.Hash or custom
13. Remaining utilities - Case-by-case basis

## Risk Assessment

### High Risk
- **Endian conversions**: Performance-critical, used extensively
- **Histogram**: May affect performance monitoring
- **Synchronized**: Thread-safety critical

### Medium Risk
- **small_vector**: Performance optimization, need benchmarks
- **String conversions**: Error handling differences

### Low Risk
- **Function wrapper**: std::function is equivalent
- **Thread naming**: Informational only
- **ByteRange**: std::span is modern equivalent

## Next Steps

1. Create detailed replacement strategy document
2. Implement compatibility shim layer for gradual migration
3. Start with simple replacements (Function, ByteRange)
4. Benchmark performance-critical replacements
5. Test thoroughly after each component replacement

## Notes

- Some Folly components (like Varint) may already have custom implementations in dwarfs
- Portability headers may be replaceable with conditional compilation
- Consider keeping Folly as optional dependency for comparison testing