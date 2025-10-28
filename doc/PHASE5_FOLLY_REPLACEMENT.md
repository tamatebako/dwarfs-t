# Phase 5: Folly Replacement Implementation

**Date:** 2025-10-28
**Track:** Track B - Static Library Compilation
**Branch:** feature/thrift-folly-removal
**Status:** Foundation Complete - Ready for Integration

## Overview

Phase 5 implements systematic replacement of Facebook Folly dependencies with C++ standard library and Boost equivalents. This enables static library compilation without Folly dependencies while maintaining API compatibility.

## Objectives Completed

✅ **Audit Completed** - Complete analysis of Folly usage
✅ **Strategy Documented** - Detailed replacement plan created
✅ **Foundation Implemented** - Core compatibility headers created
⏳ **Integration Pending** - Actual codebase replacement
⏳ **Testing Pending** - Validation and benchmarks

## Deliverables

### 1. Documentation

#### [`FOLLY_AUDIT_REPORT.md`](FOLLY_AUDIT_REPORT.md)
- Identified 24 unique Folly headers
- Found 101 instances of `folly::` usage
- Categorized by usage frequency and complexity
- Prioritized replacement order

**Key Findings:**
- **Critical Components** (>10 uses):
  - `folly::Endian::big/little` (25 uses)
  - `folly::Function` (10 uses)

- **High Priority** (5-10 uses):
  - `folly::Histogram` (7 uses)
  - `folly::small_vector` (6 uses)
  - `folly::Synchronized` (5 uses)
  - `folly::setThreadName` (5 uses)
  - `folly::ByteRange` (5 uses)

#### [`FOLLY_REPLACEMENT_STRATEGY.md`](FOLLY_REPLACEMENT_STRATEGY.md)
- Detailed replacement mapping for all components
- API compatibility considerations
- Performance impact analysis
- 4-phase implementation plan (10 days estimated)

### 2. Compatibility Layer Headers

All headers created in [`include/dwarfs/internal/`](../include/dwarfs/internal/):

#### Core Utilities

**[`compat.h`](../include/dwarfs/internal/compat.h)**
- Compiler detection macros
- Platform detection
- `DWARFS_UNREACHABLE()` - Replaces `folly::assume_unreachable()`
- `DWARFS_LIKELY/UNLIKELY()` - Branch prediction hints
- `to_underlying()` - Replaces `folly::to_underlying()` (C++23 polyfill)

**[`endian.h`](../include/dwarfs/internal/endian.h)**
- `Endian::big()` - Native to big-endian conversion
- `Endian::little()` - Native to little-endian conversion
- `Endian::bigToHost()` - Big-endian to native
- `Endian::littleToHost()` - Little-endian to native
- Uses Boost.Endian for implementation

**[`conv.h`](../include/dwarfs/internal/conv.h)**
- `to<T>()` - String to type conversion (replaces `folly::to`)
- `tryTo<T>()` - Safe conversion returning optional (replaces `folly::tryTo`)
- `ConversionError` - Exception type for conversion failures
- Uses `std::from_chars`/`std::to_chars` for performance

#### String & Data Utilities

**[`hex.h`](../include/dwarfs/internal/hex.h)**
- `hexlify()` - Convert bytes to hex string (replaces `folly::hexlify`)
- `hexDump()` - Format hex dump (replaces `folly::hexDump`)
- Support for both span and string_view

**[`demangle.h`](../include/dwarfs/internal/demangle.h)**
- `demangle()` - C++ symbol demangling (replaces `folly::demangle`)
- Platform-specific implementation (GCC/Clang: `__cxa_demangle`)

#### Bit Operations

**[`bits.h`](../include/dwarfs/internal/bits.h)**
- `Bits::findLastSet()` - Find MSB position (replaces `folly::Bits::findLastSet`)
- `Bits::findFirstSet()` - Find LSB position
- `Bits::popcount()` - Count set bits
- Platform-optimized using compiler builtins

#### Thread Utilities

**[`thread_name.h`](../include/dwarfs/internal/thread_name.h)**
- `setThreadName()` - Set current thread name (replaces `folly::setThreadName`)
- Platform-specific:
  - Linux: `pthread_setname_np(pthread_self(), name)`
  - macOS: `pthread_setname_np(name)`
  - Windows: `SetThreadDescription()` (Windows 10 1607+)

**[`synchronized.h`](../include/dwarfs/internal/synchronized.h)**
- `Synchronized<T>` - Thread-safe wrapper (replaces `folly::Synchronized`)
- `wlock()` - Acquire write lock
- `rlock()` - Acquire read lock
- `with_wlock()/with_rlock()` - Execute function with lock
- Uses `std::shared_mutex` for reader-writer locking

## Implementation Details

### Design Principles Applied

1. **Separation of Concerns**: Each header handles one category of replacements
2. **Single Responsibility**: Each function/class has one clear purpose
3. **Minimal Dependencies**: Only standard library and Boost
4. **API Compatibility**: Drop-in replacements where possible
5. **Performance**: Use efficient implementations (compiler builtins, zero-copy)

### Technical Decisions

#### Why Boost.Endian?
- Well-tested, production-ready
- Zero overhead (inline functions)
- Supports all integer types
- Gradual migration path to C++23 `std::byteswap`

#### Why std::from_chars for Conversions?
- Fastest C++ parsing (benchmarked faster than `strtol`, `sscanf`)
- Locale-independent
- No dynamic allocation
- Available in C++17

#### Why std::shared_mutex for Synchronized?
- Standard library (no external dependencies)
- Reader-writer locking (better than plain mutex)
- RAII lock guards ensure exception safety

### Namespace Strategy

All replacements are in `dwarfs::compat::` namespace to:
- Avoid naming conflicts with folly
- Allow gradual migration
- Make dependencies explicit
- Enable easy future refactoring

## Next Steps

### Integration Phase (Week 1-2)

1. **Update CMake Configuration**
   - Add Boost.Endian dependency
   - Update include paths
   - Configure compatibility headers

2. **Phase 1: Simple Replacements**
   ```cpp
   // Replace in codebase:
   folly::Function         → std::function
   folly::to_underlying()  → dwarfs::compat::to_underlying()
   folly::assume_unreachable() → DWARFS_UNREACHABLE()
   ```

3. **Phase 2: Core Utilities**
   ```cpp
   // Replace:
   folly::Endian::big()    → dwarfs::compat::Endian::big()
   folly::to<int>()        → dwarfs::compat::to<int>()
   folly::ByteRange        → std::span<const uint8_t>
   ```

4. **Phase 3: Containers**
   ```cpp
   // Replace:
   folly::small_vector     → boost::container::small_vector
   folly::sorted_vector_*  → boost::container::flat_*
   ```

5. **Phase 4: Advanced Features**
   ```cpp
   // Replace:
   folly::Synchronized     → dwarfs::compat::Synchronized
   folly::setThreadName()  → dwarfs::compat::setThreadName()
   folly::Histogram        → Custom implementation (TBD)
   ```

### Testing Strategy

1. **Unit Tests** - Test each replacement independently
2. **Integration Tests** - Run existing test suite after each phase
3. **Performance Tests** - Benchmark critical paths
4. **Platform Tests** - Verify on Linux, macOS, Windows

### Success Criteria

- [ ] All Folly headers removed from source
- [ ] All tests passing
- [ ] Performance within 5% of baseline
- [ ] Compiles on all platforms
- [ ] Static library compilation enabled

## Known Limitations

1. **Histogram** - Needs custom implementation or Boost.Accumulators
2. **Portability Headers** - May need platform-specific adjustments
3. **Hash Functions** - `jenkins_rev_mix32` may need alternative
4. **File I/O** - `readFile/writeFile` need std::filesystem integration

## Risk Mitigation

1. **Incremental Approach** - Replace one component at a time
2. **Compatibility Testing** - Comprehensive test coverage
3. **Performance Monitoring** - Benchmark before/after
4. **Rollback Plan** - Each replacement in separate commit

## Performance Considerations

### Expected Performance

| Component | Expected Impact | Mitigation |
|-----------|----------------|------------|
| Endian conversions | Neutral | Boost uses same intrinsics |
| String conversions | Neutral/Better | std::from_chars is fastest |
| Synchronized | Neutral | std::shared_mutex is standard |
| Bit operations | Neutral | Compiler builtins |
| Thread naming | N/A | Non-critical path |

### Benchmarking Plan

Critical paths to benchmark:
1. Endian conversions in deserialization
2. Varint encoding/decoding
3. String parsing
4. Synchronized access patterns

## Dependencies Added

- **Boost.Endian** - Endian conversions
- **Boost.Container** (optional) - small_vector, flat containers
- **Boost.Hash** (optional) - Hash functions

All dependencies are header-only or have minimal runtime requirements.

## Files Created

```
include/dwarfs/internal/
├── bits.h           - Bit manipulation utilities
├── compat.h         - Core compatibility layer
├── conv.h           - String conversion utilities
├── demangle.h       - Symbol demangling
├── endian.h         - Endian conversions
├── hex.h            - Hex encoding utilities
├── synchronized.h   - Thread-safe wrapper
└── thread_name.h    - Thread naming

doc/
├── FOLLY_AUDIT_REPORT.md           - Usage analysis
├── FOLLY_REPLACEMENT_STRATEGY.md   - Replacement plan
└── PHASE5_FOLLY_REPLACEMENT.md     - This document
```

## Conclusion

Phase 5 foundation is complete with:
- Comprehensive audit identifying all Folly usage
- Detailed replacement strategy
- Complete compatibility layer implementation
- Clear integration plan

The next step is to systematically integrate these replacements into the codebase, following the 4-phase plan outlined in the strategy document.

## References

- [FOLLY_AUDIT_REPORT.md](FOLLY_AUDIT_REPORT.md) - Usage analysis
- [FOLLY_REPLACEMENT_STRATEGY.md](FOLLY_REPLACEMENT_STRATEGY.md) - Replacement mappings
- [Boost.Endian Documentation](https://www.boost.org/doc/libs/release/libs/endian/)
- [C++17 std::from_chars](https://en.cppreference.com/w/cpp/utility/from_chars)