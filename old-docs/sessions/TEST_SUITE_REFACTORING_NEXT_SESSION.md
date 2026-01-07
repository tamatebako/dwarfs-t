# Next Session - Modular Test Suite Extraction

**Date**: 2025-12-14
**Session**: 4 of 7
**Estimated Time**: 3 hours
**Focus**: Extract test modules from monolithic dwarfs_test.cpp

---

## Session Context

You are continuing the DwarFS Thrift Optional Refactoring project. The test fixture caching infrastructure is complete, and we've pivoted to a modular test suite architecture.

**Problem Solved**: The monolithic `dwarfs_test.cpp` (2,237 lines) violates architectural principles and prevented safe test conversion.

**Solution**: Extract into 12 focused test modules following MECE and Single Responsibility principles.

---

## Quick Start

### 1. Read Memory Bank + Architecture
```
Memory bank: .kilocode/rules/memory-bank/
Architecture: doc/TEST_SUITE_ARCHITECTURE.md
```

### 2. Review Current State
- `test/test_fixtures.h/cpp` - Caching infrastructure ✅
- `test/dwarfs_test.cpp` - Source file to refactor ⬜
- `cmake/tests.cmake` - Build system to update ⬜

---

## Session 4 Tasks (3 hours)

### Task 1: Create Common Utilities (30 minutes)

**File**: `test/test_common.h`

**Extract from dwarfs_test.cpp**:
- `build_dwarfs()` function (lines 82-133)
- `default_file_hash_algo` constant (line 79)
- `compressions` vector (lines 581-598)

**Result**: Header+impl with shared test utilities

### Task 2: Extract Segmenter Module (30 minutes)

**Create Directory**: `test/segmenter/`

**File**: `test/segmenter/segmenter_test.cpp`

**Extract**:
- `TEST(segmenter, regression_block_boundary)` (lines 761-800)
- `TEST(section_index_regression, github183)` (lines 1282-1372)

**Strategy**:
1. Copy test code to new file
2. Add necessary includes
3. Use `test::build_dwarfs()` from test_common.h
4. Verify compilation

### Task 3: Extract Filter Module (1 hour)

**Create Directory**: `test/filter/`

**File**: `test/filter/filter_test.cpp`

**Extract**:
- `class filter_test` (lines 958-1032)
- All 6 TEST_P instances (lines 1034-1112)
- INSTANTIATE_TEST_SUITE_P (lines 1114-1115)

**Note**: Filter tests are self-contained, minimal dependencies

### Task 4: Extract Compression Module (1 hour)

**Create Directory**: `test/compression/`

**Files**:
- `test/compression/compression_test.cpp`
- `test/compression/compression_regression_test.cpp`

**Extract**:
1. `compression_test.cpp`:
   - `class compression_test` (lines 602-607)
   - `TEST_P(compression_test, end_to_end)` (lines 622-640)
   - INSTANTIATE_TEST_SUITE_P (lines 730-739)

2. `compression_regression_test.cpp`:
   - `class compression_regression` (line 802)
   - `TEST_P(compression_regression, github45)` (lines 804-885)
   - INSTANTIATE_TEST_SUITE_P (lines 887-888)

---

## Extraction Pattern

For each module, follow this pattern:

### 1. Create File Structure
```cpp
/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * [License header from dwarfs_test.cpp]
 */

#include <gtest/gtest.h>

#include "test_common.h"
#include "test_fixtures.h"
#include "test_logger.h"

// Module-specific includes

using namespace dwarfs;

namespace {
  // Module-specific helpers
}

// Test classes and TEST/TEST_P functions
```

### 2. Extract Test Code
- Copy test class definitions
- Copy TEST/TEST_P functions
- Copy INSTANTIATE_TEST_SUITE_P macros
- Verify no missing dependencies

### 3. Use Cached Fixtures Where Appropriate
```cpp
// If test uses standard data:
auto& fixtures = test::CachedTestFixtures::instance();
auto image_path = fixtures.get_image("basic_test_data");
auto mm = test::make_real_file_view(image_path);

// If test uses dynamic data (random, custom):
auto fsimage = test::build_dwarfs(lgr, input, "null", cfg);
auto mm = test::make_mock_file_view(std::move(fsimage));
```

### 4. Update CMake
```cmake
add_executable(dwarfs_${module}_tests
  test/${module}/${module}_test.cpp
)
target_link_libraries(dwarfs_${module}_tests
  dwarfs_test_helpers
  dwarfs_common
  dwarfs_reader
  dwarfs_writer
  GTest::gtest
  GTest::gtest_main
)
```

---

## Critical Requirements

### Architectural Principles
1. **Single Responsibility**: Each test file tests ONE component
2. **MECE**: No overlap, complete coverage
3. **Separation of Concerns**: Test categories clearly separated
4. **DRY**: Common code in test_common.h
5. **Open/Closed**: Easy to add new modules

### Code Quality
- **No code guards**: Use architecture, not #ifdef
- **Clean compilation**: Zero warnings
- **No shortcuts**: Tests must verify correct behavior
- **100% pass rate**: All tests must pass

### File Size Limits
- **Target**: <400 lines per file
- **Maximum**: 500 lines (if absolutely necessary)
- **Current worst**: dwarfs_test.cpp at 2,237 lines ❌

---

## Build Verification Strategy

### After Each Extraction
```bash
# Build the new module
cmake --build build --target dwarfs_${module}_tests

# Run the module tests
ctest --test-dir build -R ${module} --output-on-failure

# Verify pass rate
```

### Before Moving to Next Module
```bash
# Ensure clean build
cmake --build build

# Ensure all tests still pass
ctest --test-dir build --output-on-failure

# Check no regressions
```

---

## Session 4 Deliverables

### Files to Create
1. `test/test_common.h` (header)
2. `test/test_common.cpp` (implementation)
3. `test/segmenter/segmenter_test.cpp`
4. `test/filter/filter_test.cpp`
5. `test/compression/compression_test.cpp`
6. `test/compression/compression_regression_test.cpp`

### Files to Update
1. `cmake/tests.cmake` (add new test targets)
2. `test/dwarfs_test.cpp` (remove extracted tests)

### Expected Outcome
- 6 new test files created
- ~900 lines extracted from dwarfs_test.cpp
- dwarfs_test.cpp reduced to ~1,337 lines
- All tests compile and pass
- Build time possibly improved (parallel compilation)

---

## Troubleshooting

### If Extraction Causes Compile Errors
1. Check includes: All dependencies must be included
2. Check namespaces: `using namespace dwarfs;` if needed
3. Check test_common.h: Shared utilities accessible
4. Compare with original: Ensure no code left behind

### If Tests Fail After Extraction
1. Verify test logic unchanged
2. Check fixture usage (cached vs dynamic)
3. Compare test outputs before/after
4. Ensure test data identical

### If Build Time Increases
1. Check CMake targets properly configured
2. Verify parallel compilation enabled
3. May need to adjust test linking

---

## Success Criteria

### Must Achieve
- ✅ 4 modules extracted (segmenter, filter, compression)
- ✅ All extracted tests compile
- ✅ All extracted tests pass
- ✅ dwarfs_test.cpp reduced by ~900 lines

### Nice to Have
- Extract scanner module too
- Extract some filesystem tests
- Performance improvement measurable

---

## Next Session (Session 5)

**Focus**: Complete extraction (scanner, metadata, filesystem modules)
**Duration**: 4 hours
**Outcome**: All 12 modules extracted, monolithic file eliminated

---

**Ready to Start**: Yes ✅
**Blockers**: None (architecture designed)
**Estimated Duration**: 3 hours
**Confidence**: High - Clear modular boundaries identified