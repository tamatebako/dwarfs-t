# Test Suite Refactoring - Session 5 Continuation Plan

**Created**: 2025-12-14
**Session**: 5 of 7
**Estimated Time**: 4 hours
**Focus**: Debug failures + Continue module extraction

---

## Session Context

You are continuing the DwarFS Thrift Optional Refactoring project. Session 4 successfully validated the modular architecture with filter module achieving 100% test pass rate (28/28 tests).

**Architecture Validated**: ✅ Filter module proves modular design works
**Build System**: ✅ CMake integration seamless
**Known Issues**: ⚠️ Compression/section_index tests fail (appears pre-existing)

---

## Session 5 Objectives

### 1. Investigate Test Failures (30 minutes)

**Goal**: Determine if compression/section_index failures are pre-existing bugs

**Tasks**:
1. Check if original `dwarfs_test.cpp` has same failures
2. Compare FlatBuffers-only vs both-formats builds
3. Document findings in `doc/TEST_FAILURE_ROOT_CAUSE_UPDATE.md`

**Expected Outcome**:
- Confirm failures are pre-existing (not modularization-caused)
- File upstream bug report if needed
- Continue extraction regardless (can fix bugs in parallel)

### 2. Extract Scanner Module (1 hour)

**Create Directory**: `test/scanner/`

**Files to Create**:
- `test/scanner/scanner_test.cpp` - Scanner options tests
- `test/scanner/inode_ordering_test.cpp` - Ordering modes test
- `test/scanner/input_list_test.cpp` - Input list functionality

**Extract from dwarfs_test.cpp**:
- `class scanner_test` (lines 609-611)
- `TEST_P(scanner_test, end_to_end)` (lines 642-650)
- `INSTANTIATE_TEST_SUITE_P(scanner_test)` (lines 741-746)
- `class file_scanner` (lines 890-894)
- `TEST_P(file_scanner, inode_ordering)` (lines 896-947)
- `INSTANTIATE_TEST_SUITE_P(file_scanner)` (lines 949-956)
- `TEST(file_scanner, input_list)` (lines 1117-1152)
- `TEST(file_scanner, file_start_hash)` (lines 1395-1426)

**Expected Lines**: ~300

### 3. Extract Metadata/Packing Module (1 hour)

**Create Directory**: `test/metadata/`

**Files to Create**:
- `test/metadata/packing_test.cpp` - Packing options tests

**Extract from dwarfs_test.cpp**:
- `class packing_test` (lines 615-617)
- `TEST_P(packing_test, end_to_end)` (lines 659-668)
- `TEST_P(packing_test, regression_empty_fs)` (lines 679-728)
- `INSTANTIATE_TEST_SUITE_P(packing_test)` (lines 751-755)
- `class plain_tables_test` (lines 619-620)
- `TEST_P(plain_tables_test, end_to_end)` (lines 670-677)
- `INSTANTIATE_TEST_SUITE_P(plain_tables_test)` (lines 757-759)

**Expected Lines**: ~150

### 4. Extract Filesystem Module (Part 1) (1.5 hours)

**Create Directory**: `test/filesystem/`

**Files to Create** (first 2 of 6):
- `test/filesystem/filesystem_uid_gid_test.cpp`
- `test/filesystem/filesystem_basic_test.cpp`

**Extract from dwarfs_test.cpp**:
- `TEST(filesystem, uid_gid_32bit)` (lines 1154-1184)
- `TEST(filesystem, uid_gid_count)` (lines 1186-1223)
- `TEST(filesystem, uid_gid_override)` (lines 1225-1280)
- `TEST(filesystem, find_by_path)` (lines 1374-1393)
- `TEST(filesystem, root_access_github204)` (lines 1428-1595)

**Expected Lines**: ~350 total (2 files)

---

## Technical Requirements

### Include Path Pattern
```cpp
// In test/module/module_test.cpp
#include "../test_common.h"
#include "../test_helpers.h"
#include "../test_logger.h"
#include "../mmap_mock.h"
```

### CMake Pattern
```cmake
add_executable(dwarfs_${module}_tests
  test/${module}/${module}_test.cpp
  # ... more files if needed
)

list(APPEND DWARFS_TESTS
  dwarfs_${module}_tests
)
```

### Build Verification
After each module:
```bash
cd build-test
ninja dwarfs_${module}_tests
ctest -R ${module} --output-on-failure
```

---

## Expected Deliverables

### Files to Create (Session 5)
1. `test/scanner/scanner_test.cpp`
2. `test/scanner/inode_ordering_test.cpp`
3. `test/scanner/input_list_test.cpp`
4. `test/metadata/packing_test.cpp`
5. `test/filesystem/filesystem_uid_gid_test.cpp`
6. `test/filesystem/filesystem_basic_test.cpp`
7. `doc/TEST_FAILURE_ROOT_CAUSE_UPDATE.md` (investigation)

### Files to Update
1. `cmake/tests.cmake` (add 3 more test executables)
2. `doc/THRIFT_OPTIONAL_IMPLEMENTATION_STATUS.md` (add Session 5)

### Expected Metrics
- ~800 more lines extracted
- 3 new test modules
- dwarfs_test.cpp reduced to ~484 lines (from 1,284)

---

## Session 5 Success Criteria

### Must Achieve
- [ ] Compression failure root cause identified
- [ ] Scanner module extracted (3 files)
- [ ] Metadata module extracted (1 file)
- [ ] At least 2 filesystem test files extracted
- [ ] All new modules compile cleanly
- [ ] CMake integration complete

### Nice to Have
- [ ] Compression failures fixed
- [ ] All scanner tests passing
- [ ] All metadata tests passing

---

## Troubleshooting Guide

### If Compression Tests Still Fail
- Document as pre-existing bug
- Create minimal reproduction case
- File issue for separate investigation
- Continue extraction (not blocking)

### If Scanner Tests Fail
- Compare with original `dwarfs_test.cpp` behavior
- Check fixture dependencies
- Verify `test_common::build_dwarfs()` usage

### If Filesystem Tests Fail
- May be related to compression failures
- Document pattern
- Continue extraction

---

## Timeline Update

**Completed**: 31 hours (Sessions 1-4)
**Session 5**: 4 hours (debug + extraction)
**Remaining**: 15 hours (Sessions 6-7)
  - Session 6: Complete filesystem extraction + integration (4h)
  - Session 7: Final validation + release prep (5h)
  - RC1 Testing: 3-5 days
  - Buffer: 6 hours

**Target**: v0.16.0 by 2025-12-27 ✅ **STILL ON TRACK**

---

## Quick Start Commands

### Read Previous Work
```
Memory bank: .kilocode/rules/memory-bank/context.md
Session 4: doc/TEST_SUITE_SESSION_4_COMPLETE.md
Architecture: doc/TEST_SUITE_ARCHITECTURE.md
```

### Build & Test
```bash
cd build-test
ninja dwarfs_scanner_tests
ctest -R scanner --output-on-failure
```

---

**Ready to Start**: Yes ✅
**Blockers**: None (architecture validated)
**Confidence**: Very High - Sound approach, clear path forward