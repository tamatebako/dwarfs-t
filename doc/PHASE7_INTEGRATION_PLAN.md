# Phase 7: Folly Integration Replacement Plan

**Date:** 2025-10-28
**Phase:** Track B Phase 7 - Folly Integration
**Branch:** `feature/thrift-folly-removal`
**Status:** 🚧 IN PROGRESS

## Executive Summary

This document provides a detailed plan for systematically replacing 101 Folly usages across 26 source files in the dwarfs codebase with our compatibility layer.

**Scope:**
- **Files to modify:** 26 files in `src/` directory
- **Folly headers to replace:** 24 unique headers
- **Total usages:** 101 instances
- **Estimated effort:** 10-15 commits over 2-3 days

## Files Requiring Modification

### Category 1: Endian Operations (Priority 1 - Highest)

**Folly Headers:**
- `<folly/lang/Bits.h>` - Bit operations and endian conversion
- `<folly/lang/BitsClass.h>` - Bit operations class

**Files:**
1. `src/performance_monitor.cpp` - Uses `folly::lang::Bits.h`
2. `src/writer/categorizer/fits_categorizer.cpp` - Uses `folly/lang/Bits.h`
3. `src/writer/categorizer/pcmaudio_categorizer.cpp` - Uses `folly/lang/Bits.h`

**Replacement:**
```cpp
// Before
#include <folly/lang/Bits.h>
folly::Bits::findLastSet(value)

// After
#include "dwarfs/internal/bits.h"
dwarfs::compat::Bits::findLastSet(value)
```

**Estimated effort:** 30 minutes

---

### Category 2: Function/Callable (Priority 2)

**Folly Headers:**
- `<folly/Function.h>` - Type-erased function wrapper

**Files:**
- ✅ **No direct usage found in main source** (may be in headers)

**Replacement:**
```cpp
// Before
#include <folly/Function.h>
folly::Function<void()> callback;

// After
#include <functional>
std::function<void()> callback;
```

**Estimated effort:** 15 minutes (if needed)

---

### Category 3: String Utilities (Priority 3)

**Folly Headers:**
- `<folly/Conv.h>` - String conversions (to/tryTo)
- `<folly/String.h>` - String utilities

**Files:**
1. `src/reader/filesystem_options.cpp` - Uses `folly/Conv.h`
2. `src/writer/inode_fragments.cpp` - Uses `folly/Conv.h`
3. `src/conv.cpp` - Uses `folly/Conv.h`
4. `src/logger.cpp` - Uses `folly/Conv.h`
5. `src/util.cpp` - Uses `folly/String.h`

**Replacement:**
```cpp
// Before
#include <folly/Conv.h>
auto value = folly::to<int>(str);
auto result = folly::tryTo<int>(str);

// After
#include "dwarfs/internal/conv.h"
auto value = dwarfs::compat::to<int>(str);
auto result = dwarfs::compat::tryTo<int>(str);
```

**Estimated effort:** 1 hour

---

### Category 4: Container Types (Priority 4)

**Folly Headers:**
- `<folly/small_vector.h>` - Small-size optimized vector
- `<folly/Synchronized.h>` - Thread-safe wrapper

**Files:**
1. `src/logger.cpp` - Uses `folly/small_vector.h`
2. `src/reader/internal/metadata_v2.cpp` - Uses `folly/small_vector.h`, `folly/Synchronized.h`
3. `src/writer/categorizer/fits_categorizer.cpp` - Uses `folly/Synchronized.h`
4. `src/writer/categorizer/libmagic_categorizer.cpp` - Uses `folly/Synchronized.h`
5. `src/writer/categorizer/pcmaudio_categorizer.cpp` - Uses `folly/Synchronized.h`

**Replacement:**
```cpp
// Before (small_vector)
#include <folly/small_vector.h>
folly::small_vector<T, N> vec;

// After
#include <boost/container/small_vector.hpp>
boost::container::small_vector<T, N> vec;

// Before (Synchronized)
#include <folly/Synchronized.h>
folly::Synchronized<T> data;
auto locked = data.wlock();

// After
#include "dwarfs/internal/synchronized.h"
dwarfs::compat::Synchronized<T> data;
auto locked = data.wlock();
```

**Estimated effort:** 2 hours

---

### Category 5: System Utilities (Priority 5)

**Folly Headers:**
- `<folly/system/ThreadName.h>` - Thread naming
- `<folly/system/HardwareConcurrency.h>` - CPU count
- `<folly/stats/Histogram.h>` - Statistics histogram

**Files (ThreadName):**
1. `src/internal/worker_group.cpp`
2. `src/utility/filesystem_extractor.cpp`
3. `src/reader/internal/block_cache.cpp`
4. `src/reader/internal/periodic_executor.cpp`
5. `src/writer/writer_progress.cpp`
6. `src/writer/filesystem_writer.cpp`

**Files (HardwareConcurrency):**
1. `src/util.cpp`
2. `src/writer/scanner.cpp`

**Files (Histogram):**
1. `src/performance_monitor.cpp`
2. `src/reader/internal/metadata_v2.cpp`
3. `src/reader/internal/block_cache.cpp`

**Replacement:**
```cpp
// Before (ThreadName)
#include <folly/system/ThreadName.h>
folly::setThreadName(name);

// After
#include "dwarfs/internal/thread_name.h"
dwarfs::compat::setThreadName(name);

// Before (HardwareConcurrency)
#include <folly/system/HardwareConcurrency.h>
auto n = folly::hardware_concurrency();

// After
#include <thread>
auto n = std::thread::hardware_concurrency();

// Before (Histogram)
#include <folly/stats/Histogram.h>
folly::Histogram<T> hist;

// After
#include "dwarfs/internal/histogram.h"
dwarfs::compat::Histogram<T> hist;
```

**Estimated effort:** 3 hours

---

### Category 6: Portability Headers (Priority 6)

**Folly Headers:**
- `<folly/portability/Windows.h>`
- `<folly/portability/Fcntl.h>`
- `<folly/portability/Unistd.h>`
- `<folly/portability/PThread.h>`
- `<folly/portability/Stdlib.h>`
- `<folly/portability/SysStat.h>`

**Files:**
1. `src/internal/io_ops_win.cpp` - Windows.h
2. `src/internal/worker_group.cpp` - Windows.h
3. `src/utility/filesystem_extractor.cpp` - Windows.h, Fcntl.h, Unistd.h
4. `src/file_util.cpp` - Windows.h
5. `src/terminal_ansi.cpp` - Unistd.h, Windows.h
6. `src/util.cpp` - Fcntl.h, SysStat.h, Windows.h
7. `src/performance_monitor.cpp` - Windows.h, Unistd.h
8. `src/file_stat.cpp` - Windows.h, Unistd.h
9. `src/reader/internal/metadata_v2.cpp` - Stdlib.h, Unistd.h
10. `src/writer/writer_progress.cpp` - Windows.h
11. `src/writer/scanner.cpp` - Unistd.h
12. `src/detail/scoped_env.cpp` - Stdlib.h
13. `src/os_access_generic.cpp` - PThread.h, Unistd.h

**Replacement:**
```cpp
// Before
#include <folly/portability/Windows.h>
#include <folly/portability/Unistd.h>

// After
#ifdef _WIN32
  #include <windows.h>
  #include <io.h>
#else
  #include <unistd.h>
#endif
```

**Estimated effort:** 2 hours

---

### Category 7: Utility Functions (Priority 7)

**Folly Headers:**
- `<folly/ExceptionString.h>` - Exception formatting
- `<folly/FileUtil.h>` - File I/O utilities
- `<folly/Varint.h>` - Variable-length integer encoding
- `<folly/hash/Hash.h>` - Hash functions
- `<folly/lang/Assume.h>` - Compiler hints
- `<folly/CPortability.h>` - C portability

**Files:**
1. `src/utility/filesystem_extractor.cpp` - ExceptionString.h
2. `src/util.cpp` - ExceptionString.h
3. `src/file_util.cpp` - FileUtil.h
4. `src/varint.cpp` - Varint.h
5. `src/writer/fragment_category.cpp` - hash/Hash.h
6. `src/pcm_sample_transformer.cpp` - lang/Assume.h
7. `src/logger.cpp` - lang/Assume.h
8. `src/writer/scanner.cpp` - CPortability.h

**Replacement:**
```cpp
// Before (ExceptionString)
#include <folly/ExceptionString.h>
auto msg = folly::exceptionStr(ex);

// After
#include "dwarfs/internal/exception_string.h"
auto msg = dwarfs::compat::exceptionStr(ex);

// Before (Assume)
#include <folly/lang/Assume.h>
folly::assume_unreachable();

// After
#include "dwarfs/internal/unreachable.h"
DWARFS_UNREACHABLE();

// Before (Varint)
#include <folly/Varint.h>
folly::encodeVarint(value, buf);

// After
#include "dwarfs/varint.h"
// Use existing dwarfs implementation
```

**Estimated effort:** 1.5 hours

---

## Implementation Order

### Phase 1: Create Missing Compatibility Headers (30 min)

Need to create these compatibility headers:
- [x] `include/dwarfs/internal/endian.h` (already exists)
- [x] `include/dwarfs/internal/bits.h` (already exists)
- [x] `include/dwarfs/internal/conv.h` (already exists)
- [x] `include/dwarfs/internal/synchronized.h` (already exists)
- [x] `include/dwarfs/internal/thread_name.h` (already exists)
- [x] `include/dwarfs/internal/hex.h` (already exists)
- [ ] `include/dwarfs/internal/exception_string.h`
- [ ] `include/dwarfs/internal/histogram.h`
- [ ] `include/dwarfs/internal/unreachable.h`

### Phase 2: Replace String Utilities (1 hour)

Priority: High usage, simple replacement

**Files:**
1. `src/reader/filesystem_options.cpp`
2. `src/writer/inode_fragments.cpp`
3. `src/conv.cpp`
4. `src/logger.cpp`
5. `src/util.cpp`

**Test:** Build and run unit tests

### Phase 3: Replace Portability Headers (2 hours)

Priority: Many files, straightforward replacement

**Files:** 13 files using portability headers

**Test:** Build on Linux, macOS, Windows

### Phase 4: Replace System Utilities (3 hours)

Priority: Critical functionality (threading, performance)

**Files:**
- ThreadName: 6 files
- HardwareConcurrency: 2 files
- Histogram: 3 files

**Test:** Build and run threading tests, performance tests

### Phase 5: Replace Container Types (2 hours)

Priority: Medium risk, needs careful testing

**Files:**
- small_vector: 2 files
- Synchronized: 4 files

**Test:** Build and run concurrent tests

### Phase 6: Replace Bit Operations (30 min)

Priority: Performance-critical

**Files:**
- Bits: 3 files

**Test:** Build and run bit operation tests

### Phase 7: Replace Remaining Utilities (1.5 hours)

Priority: Low usage, various utilities

**Files:**
- ExceptionString: 2 files
- FileUtil: 1 file
- Varint: 1 file (may already have dwarfs implementation)
- Hash: 1 file
- Assume: 2 files
- CPortability: 1 file

**Test:** Full integration test suite

---

## Testing Strategy

### After Each Phase

```bash
# Configure with tests
cmake -B build-test -DWITH_TESTS=ON -DTEBAKO_BUILD=ON

# Build
cmake --build build-test

# Run tests
ctest --test-dir build-test --output-on-failure

# Check for any new warnings
cmake --build build-test 2>&1 | grep -i warning
```

### Final Validation

```bash
# Full rebuild
rm -rf build-test
cmake -B build-test -DWITH_TESTS=ON -DTEBAKO_BUILD=ON
cmake --build build-test

# Run all tests
ctest --test-dir build-test --verbose

# Performance comparison (if needed)
./build-test/performance_tests
```

---

## Commit Strategy

One commit per category/phase:

1. `refactor: add missing compatibility headers for folly replacement`
2. `refactor: replace folly string utilities with dwarfs::compat`
3. `refactor: replace folly portability headers with standard headers`
4. `refactor: replace folly system utilities (ThreadName, HardwareConcurrency)`
5. `refactor: replace folly::Histogram with dwarfs::compat::Histogram`
6. `refactor: replace folly containers (small_vector, Synchronized)`
7. `refactor: replace folly::Bits with dwarfs::compat::Bits`
8. `refactor: replace remaining folly utilities`
9. `docs: update PHASE7_STATUS.md with completion report`

---

## Risk Mitigation

### High-Risk Areas

1. **Histogram replacement** - Performance monitoring critical
   - Mitigation: Ensure API compatibility, benchmark before/after

2. **Synchronized replacement** - Thread safety critical
   - Mitigation: Careful review of locking semantics, stress testing

3. **Bit operations** - Performance-critical in data processing
   - Mitigation: Ensure compiler optimizations, benchmark

### Rollback Plan

- Each commit is atomic and can be reverted independently
- Keep detailed notes of any issues encountered
- Document any behavioral differences

---

## Success Metrics

- [ ] All 26 files successfully modified
- [ ] All 101 Folly usages replaced
- [ ] All tests passing (unit, integration, performance)
- [ ] No new compiler warnings
- [ ] No performance regressions (< 5% acceptable)
- [ ] Code compiles on Linux, macOS, Windows
- [ ] Static library compilation enabled

---

## Estimated Timeline

| Phase | Duration | Cumulative |
|-------|----------|------------|
| Phase 1: Create headers | 30 min | 30 min |
| Phase 2: String utilities | 1 hour | 1.5 hours |
| Phase 3: Portability headers | 2 hours | 3.5 hours |
| Phase 4: System utilities | 3 hours | 6.5 hours |
| Phase 5: Container types | 2 hours | 8.5 hours |
| Phase 6: Bit operations | 30 min | 9 hours |
| Phase 7: Remaining utilities | 1.5 hours | 10.5 hours |
| Testing & documentation | 1.5 hours | 12 hours |
| **Total** | **~12 hours** | **~2 days** |

---

## Notes

- Varint: Check if `include/dwarfs/varint.h` already provides what we need
- Some files may have multiple Folly dependencies and will be modified in multiple phases
- Focus on one component at a time to minimize risk
- Document any unexpected issues or behavioral differences
- Keep build times in check by using ccache if available

---

**Status:** Ready to begin implementation
**Next Step:** Create missing compatibility headers (Phase 1)