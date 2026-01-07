# DwarFS Thrift Optional Refactoring - Complete Continuation Plan

**Created**: 2025-12-14
**Status**: 🟢 ACTIVE - Phase 1.2 Complete
**Target**: v0.16.0 Release by 2025-12-27
**Total Estimated Time**: 24 hours remaining (of 50 hours total)
**Completion**: 52% (26/50 hours)

---

## Executive Summary

### Progress Overview

**Completed** (28 hours):
- ✅ Session 1 (10h): Test framework + root cause analysis
- ✅ Session 2 (1h): Test fixture caching infrastructure COMPLETE
- ✅ Session 3 (2h): Architectural design for modular test suite
- ✅ Cumulative: Infrastructure ready, architecture designed

**Remaining** (22 hours - COMPRESSED):
- ⏳ Phase 1: Modular test suite implementation (11h)
- ⏳ Phase 2: Documentation (6h compressed)
- ⏳ Phase 3: Release preparation (5h compressed)

### Key Discovery ✅

**Original Plan**: Patch monolithic `dwarfs_test.cpp` (2,237 lines) with cached fixtures

**Architectural Pivot** (Session 3): Refactor into modular test suite (12 modules)

**Rationale**:
- Monolithic file violates Single Responsibility Principle
- Tool issues prevented safe patching of large file
- Modular approach enables proper fixture usage
- Follows DwarFS architectural principles (MECE, OOP, separation of concerns)

**Impact**: Better architecture, easier maintenance, cleaner fixture integration

---

## COMPRESSED Timeline (22 hours → 4 sessions)

### Session 4: Module Extraction Part 1 (3 hours)
**Target**: 2025-12-15

1. Create test/test_common.h (shared utilities) (30min)
2. Extract segmenter tests (30min)
3. Extract filter tests (1h)
4. Extract compression tests (1h)

### Session 5: Module Extraction Part 2 (4 hours)
**Target**: 2025-12-16

1. Extract scanner tests (1.5h)
2. Extract metadata/packing tests (1h)
3. Extract filesystem tests (1.5h)

### Session 6: Integration + Validation (4 hours)
**Target**: 2025-12-17

1. Update CMake build system (1h)
2. Build and test all modules (1h)
3. Performance benchmarking (1h)
4. Update documentation (1h)

### Session 7: Release Preparation (5 hours)
**Target**: 2025-12-18

1. Clean up old documentation (1h)
2. Update CHANGES.md (1h)
3. CI/CD validation (2h)
4. Tag v0.16.0-rc1 (1h)

**Release Date**: 2025-12-27 (includes RC1 testing buffer)

---

## Phase 1: Modular Test Suite (16h → 11h remaining)

### ✅ Phase 1.1: Caching Infrastructure (1h) - COMPLETE
**Status**: Infrastructure built and validated

**Delivered**:
- [`test/test_fixtures.h`](../test/test_fixtures.h) (177 lines)
- [`test/test_fixtures.cpp`](../test/test_fixtures.cpp) (280 lines)
- 4 standard fixtures auto-registered

### ✅ Phase 1.2: Architecture Design (2h) - COMPLETE
**Status**: Comprehensive design document created

**Delivered**:
- [`doc/TEST_SUITE_ARCHITECTURE.md`](TEST_SUITE_ARCHITECTURE.md)
- Test categorization (6 modules)
- Migration strategy (4 phases)
- MECE structure with clear boundaries

### Phase 1.3: Extract Common Utilities (1h)

**Create**: `test/test_common.h`

**Content**:
```cpp
#pragma once

#include <string>
#include <memory>
#include <dwarfs/writer/segmenter_factory.h>
#include <dwarfs/writer/scanner_options.h>

namespace dwarfs::test {

// Common test utilities shared across test modules

// Build a DwarFS filesystem image from input
std::string build_dwarfs(
    logger& lgr,
    std::shared_ptr<os_access_mock> input,
    std::string const& compression,
    writer::segmenter::config const& cfg = {},
    writer::scanner_options const& options = {},
    writer::filesystem_writer_options const& writer_opts = {},
    writer::writer_progress* prog = nullptr,
    std::shared_ptr<filter_transformer_data> ftd = nullptr,
    std::optional<std::span<std::filesystem::path const>> input_list = std::nullopt,
    std::unique_ptr<writer::entry_filter> filter = nullptr);

// Common test constants
extern std::string const default_file_hash_algo;
extern std::vector<std::string> const compressions;

} // namespace dwarfs::test
```

### Phase 1.4: Extract Modules (8h)

**Order of extraction** (simplest → most complex):

#### 1.4.1: Segmenter Module (30min)
**Create**: `test/segmenter/segmenter_test.cpp`

**Tests to extract**:
- `TEST(segmenter, regression_block_boundary)` (lines 761-800)
- `TEST(section_index_regression, github183)` (lines 1282-1372)

**Uses**: Dynamic fixtures (block boundary), `basic_test_data` (github183)

#### 1.4.2: Filter Module (1h)
**Create**: `test/filter/filter_test.cpp`

**Tests to extract**:
- `class filter_test` + 6 TEST_P instances (lines 958-1115)
- Complete self-contained module

#### 1.4.3: Compression Module (1h)
**Create**: `test/compression/compression_test.cpp`
**Create**: `test/compression/compression_regression_test.cpp`

**Tests to extract**:
- `class compression_test` + TEST_P (lines 602-640)
- `class compression_regression` + TEST_P (lines 802-888)

#### 1.4.4: Scanner Module (1.5h)
**Create**: `test/scanner/scanner_options_test.cpp`
**Create**: `test/scanner/file_hashing_test.cpp`
**Create**: `test/scanner/inode_ordering_test.cpp`

**Tests to extract**:
- `class scanner_test` + TEST_P (lines 609-650)
- `class hashing_test` + TEST_P (lines 652-657)
- `class file_scanner` + TEST_P (lines 890-957)
- `TEST(file_scanner, input_list)` (lines 1117-1152)
- `TEST(file_scanner, file_start_hash)` (lines 1395-1426)

#### 1.4.5: Metadata/Packing Module (1h)
**Create**: `test/metadata/packing_test.cpp`
**Create**: `test/metadata/plain_tables_test.cpp`

**Tests to extract**:
- `class packing_test` + 2 TEST_P instances (lines 616-728)
- `class plain_tables_test` + TEST_P (lines 619-677)

#### 1.4.6: Filesystem Module (3h)
**Create**: `test/filesystem/filesystem_uid_gid_test.cpp`
**Create**: `test/filesystem/filesystem_basic_test.cpp`
**Create**: `test/filesystem/filesystem_read_test.cpp`
**Create**: `test/filesystem/filesystem_lookup_test.cpp`
**Create**: `test/filesystem/filesystem_multi_image_test.cpp`

**Tests to extract**:
- `TEST(filesystem, uid_gid_32bit)` (lines 1154-1184)
- `TEST(filesystem, uid_gid_count)` (lines 1186-1223)
- `TEST(filesystem, uid_gid_override)` (lines 1225-1280)
- `TEST(filesystem, find_by_path)` (lines 1374-1393)
- `TEST(filesystem, root_access_github204)` (lines 1428-1595)
- `TEST(filesystem, read)` (lines 1597-1915)
- `TEST(filesystem, inode_size_cache)` (lines 1917-1987)
- `TEST(filesystem, multi_image)` (lines 1989-2057)
- `TEST(filesystem, case_insensitive_lookup)` (lines 2059-2237)

### Phase 1.5: CMake Integration (1h)

**Update**: `cmake/tests.cmake`

**Add test targets**:
```cmake
# Modular test suite
add_executable(dwarfs_segmenter_tests
  test/segmenter/segmenter_test.cpp
)
target_link_libraries(dwarfs_segmenter_tests ${TEST_COMMON_LIBS})

add_executable(dwarfs_filter_tests
  test/filter/filter_test.cpp
)
target_link_libraries(dwarfs_filter_tests ${TEST_COMMON_LIBS})

# ... etc for each module
```

### Phase 1.6: Validation (1h)

**Steps**:
```bash
# Build all modules
cmake --build build

# Run each module independently
ctest --test-dir build -R segmenter --output-on-failure
ctest --test-dir build -R filter --output-on-failure
# ... etc

# Run all tests
ctest --test-dir build --output-on-failure

# Verify pass rate: 3,132 tests, 0 failures
```

---