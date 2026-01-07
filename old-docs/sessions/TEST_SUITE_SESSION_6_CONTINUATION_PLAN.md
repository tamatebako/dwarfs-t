# Test Suite Refactoring - Session 6 Continuation Plan

**Created**: 2025-12-14
**Session**: 6 of 7
**Estimated Time**: 4 hours
**Focus**: Fix namespaces + Complete filesystem extraction + Validation

---

## CRITICAL: Namespace Fix Required First

**Problem**: All extracted test files use `test_common::` instead of `test::`

**Affected Files** (9 files):
- `test/scanner/scanner_test.cpp`
- `test/scanner/inode_ordering_test.cpp`
- `test/scanner/input_list_test.cpp`
- `test/metadata/packing_test.cpp`
- `test/filesystem/filesystem_uid_gid_test.cpp`
- `test/filesystem/filesystem_basic_test.cpp`

**Fix Pattern**:
```cpp
// WRONG (current)
test_common::build_dwarfs(...)
test_common::default_compression()
test_common::default_file_hash_algo()

// CORRECT (should be)
test::build_dwarfs(...)
test::default_compression()
test::default_file_hash_algo()
```

**Note**: Since tests have `using namespace dwarfs;`, we just use `test::` prefix directly.

---

## Session 6 Objectives

### 1. Fix Namespace Issues (30 minutes)

**Task**: Global search/replace in all extracted test files

**Files to Fix**:
1. All scanner test files
2. All metadata test files
3. All filesystem test files

**Verification**:
```bash
cd build-test
ninja dwarfs_scanner_tests dwarfs_metadata_tests dwarfs_filesystem_tests
```

### 2. Complete Filesystem Module Extraction (2.5 hours)

**Remaining Tests** (from dwarfs_test.cpp lines 1282-1595):

#### File 3: `test/filesystem/section_index_test.cpp` (~90 lines)
**Extract**:
- `TEST(section_index_regression, github183)` (lines 1282-1372)

#### File 4: `test/filesystem/filesystem_checksum_test.cpp` (~150 lines)
**Extract** (if exists - need to verify):
- Checksum-related filesystem tests

#### File 5: `test/filesystem/filesystem_read_test.cpp` (~200 lines)
**Extract** (if exists - need to verify):
- File reading tests
- Content verification tests

#### File 6: `test/filesystem/filesystem_walk_test.cpp` (~100 lines)
**Extract** (if exists - need to verify):
- Directory walking tests
- Tree traversal tests

**Process**:
1. Read dwarfs_test.cpp lines 1282-1595
2. Identify all remaining filesystem tests
3. Group by functionality
4. Create appropriately named test files
5. Update CMake
6. Build & verify

### 3. Final Integration & Validation (1 hour)

**Tasks**:

#### 3.1 Build Validation
```bash
cd build-test
ninja clean
ninja dwarfs_scanner_tests dwarfs_metadata_tests dwarfs_filesystem_tests
```

#### 3.2 Test Execution
```bash
ctest -R "scanner|metadata|filesystem" --output-on-failure
```

#### 3.3 Metrics Collection
- Count final dwarfs_test.cpp lines
- Verify all tests moved
- Document extraction statistics

### 4. Update CMake (included in extraction)

Add new filesystem test files to `cmake/tests.cmake`:
```cmake
add_executable(dwarfs_filesystem_tests
  test/filesystem/filesystem_uid_gid_test.cpp
  test/filesystem/filesystem_basic_test.cpp
  test/filesystem/section_index_test.cpp
  # Add more as extracted
)
```

---

## Expected Outcomes

### Files to Create (Session 6)
1. `test/filesystem/section_index_test.cpp`
2. Additional filesystem test files (TBD based on content)

### Files to Modify
1. All extracted test files (namespace fixes)
2. `cmake/tests.cmake` (add new test files)

### Metrics Target
- **dwarfs_test.cpp**: Reduce to 0 lines (complete extraction)
- **Total extracted**: ~1,500 lines
- **Test modules**: 3 complete (scanner, metadata, filesystem)
- **Build status**: ✅ All modules compile
- **Test status**: ✅ All modules pass (except pre-existing bugs)

---

## Session 6 Success Criteria

### Must Achieve
- [ ] All namespace issues fixed
- [ ] All tests compile successfully
- [ ] Filesystem module complete (all tests extracted)
- [ ] CMake fully integrated
- [ ] Build succeeds: `ninja dwarfs_scanner_tests dwarfs_metadata_tests dwarfs_filesystem_tests`

### Nice to Have
- [ ] All tests passing (except known pre-existing bugs)
- [ ] dwarfs_test.cpp completely empty
- [ ] Compression bug root cause identified

---

## Known Issues to Work Around

### Pre-Existing Bugs (Do NOT try to fix)
1. **Compression tests**: 262 SEGFAULT (device node issue)
2. **Regression tests**: 8 fail (dev==false expected true)
3. **Section index**: 1 fail (same dev node issue)

**Action**: Document, skip, continue. These are NOT caused by refactoring.

---

## Quick Start Commands

### Read Context
```bash
# Review previous session
cat doc/TEST_SUITE_SESSION_5_STATUS.md

# Check architecture
cat doc/TEST_SUITE_ARCHITECTURE.md
```

### Fix Namespaces
```bash
# In all test files, replace:
find test/scanner test/metadata test/filesystem -name "*.cpp" -exec sed -i '' 's/test_common::/test::/g' {} \;
```

### Build & Test
```bash
cd build-test
ninja dwarfs_scanner_tests dwarfs_metadata_tests dwarfs_filesystem_tests
ctest -R "scanner|metadata|filesystem" --output-on-failure
```

---

## Timeline

**Session 6**: 4 hours
- Namespace fixes: 30min
- Filesystem extraction: 2.5h
- Integration & validation: 1h

**Overall Progress After Session 6**: 72% (36/50 hours)
**Remaining**: 1 session (Session 7)

**Target**: v0.16.0 by 2025-12-27 ✅ **ON TRACK**

---

## Files Reference

### Created in Session 5
- ✅ `test/scanner/scanner_test.cpp`
- ✅ `test/scanner/inode_ordering_test.cpp`
- ✅ `test/scanner/input_list_test.cpp`
- ✅ `test/metadata/packing_test.cpp`
- ✅ `test/filesystem/filesystem_uid_gid_test.cpp`
- ✅ `test/filesystem/filesystem_basic_test.cpp`

### To Create in Session 6
- ⬜ `test/filesystem/section_index_test.cpp`
- ⬜ Additional filesystem test files (discover during extraction)

### Modified Files
- ⬜ All Session 5 files (namespace fixes)
- ⬜ `cmake/tests.cmake` (add new files)

---

**Status**: 🟡 **READY TO START** - Clear path, namespace fixes first
**Confidence**: Very High - Architecture validated, mechanical work remains
**Next Session Focus**: Fix, extract, validate