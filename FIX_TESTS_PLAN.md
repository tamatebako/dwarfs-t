# Test Failure Fix Plan

**Branch**: `fix/test-failures`
**Created**: 2026-02-12
**Status**: In Progress

## Overview

Current test pass rate: 96% across all platforms (~88-92 failures out of ~2050 tests)

### Priority Order (Based on Severity)

1. **Phase 1**: Quick wins - Skip tests that legitimately cannot pass in CI
2. **Phase 2**: Core bugs - Fix actual functionality issues
3. **Phase 4**: Memory safety - Investigate segfaults/overflows in bad_fs.test (CRITICAL)
4. **Phase 3**: Platform-specific - Windows/MinGW differences

---

## Phase 1: Quick Wins (Skip Tests That Cannot Pass)

### 1.1 Skip root_access test when not root
- [ ] Add `SKIP_IF_NOT_ROOT()` macro
- [ ] Apply to `FilesystemBasicTest.root_access_github204`
- [ ] Test locally
- [ ] Commit

### 1.2 Fix CI compat image availability
- [ ] Investigate why compat images are not available in CI
- [ ] Add CI step to generate or download compat images
- [ ] Or skip `LegacyThriftCompatibilityTest` if images unavailable
- [ ] Test locally
- [ ] Commit

### 1.3 Skip symlink tests on Windows
- [ ] Add platform detection for Windows symlink limitations
- [ ] Skip `file_utils.file_stat_symlink` on Windows
- [ ] Skip `compare_directories_test.sanity` on Windows (if symlink-related)
- [ ] Test locally
- [ ] Commit

---

## Phase 2: Core Bugs (Fix Functionality Issues)

### 2.1 Fix filter_test.filesystem/* (CRITICAL)
- [ ] Investigate why `md_.inodes.size()=0`
- [ ] Check test fixture setup
- [ ] Fix data population issue
- [ ] Test locally
- [ ] Commit

**Affected tests**:
- `dwarfs/filter_test.filesystem/IncludeAllSharedObjs`
- `dwarfs/filter_test.filesystem/IncludeSomeObjects`
- `dwarfs/filter_test.filesystem/IgnoreCase`

### 2.2 Fix frozen2_schema_builder_tests
- [ ] Get detailed failure output locally
- [ ] Investigate schema builder logic
- [ ] Fix the issue
- [ ] Test locally
- [ ] Commit

---

## Phase 4: Memory Safety (Segfaults/Overflows - CRITICAL)

### 4.1 Investigate bad_fs.test failures
- [ ] Run bad_fs tests locally with AddressSanitizer
- [ ] Identify specific crash patterns
- [ ] Categorize: segfault vs assertion failure vs stack overflow

### 4.2 Fix stack overflow issues (Exit code 0xc0000409)
- [ ] Add bounds checking to recursive functions
- [ ] Fix unbounded stack allocation
- [ ] Test with ASan
- [ ] Commit

### 4.3 Fix segfault issues
- [ ] Add null pointer checks
- [ ] Add buffer bounds validation
- [ ] Test with ASan
- [ ] Commit

### 4.4 Improve corrupt data handling
- [ ] Add graceful error recovery for malformed data
- [ ] Ensure exceptions are caught properly
- [ ] Test with ASan
- [ ] Commit

---

## Phase 3: Platform-Specific Issues

### 3.1 Windows MSVC: UTF-8 special chars
- [ ] Investigate Windows UTF-8 console handling
- [ ] Adjust test expectations or skip on Windows
- [ ] Test locally
- [ ] Commit

### 3.2 Windows MSVC: error_test.* (3 tests)
- [ ] Investigate Windows SEH vs POSIX signals
- [ ] Implement platform-specific error handling or skip
- [ ] Test locally
- [ ] Commit

### 3.3 MinGW: FSST compression ratio
- [ ] Investigate FSST behavior on MinGW
- [ ] Adjust threshold or skip on MinGW
- [ ] Test locally
- [ ] Commit

---

## Workflow Improvements

### W.1 Separate build and test phases
- [ ] Modify CI workflows to have distinct build and test jobs
- [ ] Build job: compile all targets
- [ ] Test job: run tests (depends on build job)
- [ ] Allows faster iteration on test failures

### W.2 Create new PR for test fixes
- [ ] Create branch `fix/test-failures`
- [ ] All fixes go to this branch
- [ ] Single PR with all test fixes
- [ ] Squash merge when complete

---

## Local Testing Checklist

Before each commit:
- [ ] Build succeeds: `cd build && ninja`
- [ ] Tests pass: `cd build && ctest --output-on-failure`
- [ ] No new warnings introduced
- [ ] Commit message follows semantic format

---

## Progress Tracking

| Phase | Task | Status | Commit |
|-------|------|--------|--------|
| 1.1 | Skip root_access test | ⬜ Pending | - |
| 1.2 | Fix compat image availability | ✅ Done | 5b39fc05 |
| 1.3 | Skip symlink tests on Windows | ✅ Done | de5653f0 |
| 2.1 | Fix filter_test.filesystem | ⬜ Pending | - |
| 2.2 | Fix frozen2_schema_builder | ✅ Done | 5b39fc05 |
| 4.1 | Investigate bad_fs.test | ⬜ Pending | - |
| 4.2 | Fix stack overflow | ⬜ Pending | - |
| 4.3 | Fix segfault | ⬜ Pending | - |
| 4.4 | Improve corrupt data handling | ⬜ Pending | - |
| 3.1 | Windows UTF-8 | ⬜ Pending | - |
| 3.2 | Windows error_test | ⬜ Pending | - |
| 3.3 | MinGW FSST | ⬜ Pending | - |
| W.1 | Separate build/test phases | ⬜ Pending | - |
| W.2 | Create PR | ⬜ Pending | - |

---

## Summary Statistics

**Starting Point**:
- Linux: 88 failures / 2050 tests (96% pass)
- macOS: 88 failures / 2050 tests (96% pass)
- Windows MSVC: 91 failures / 2036 tests (96% pass)
- Windows MinGW: 92 failures / 2047 tests (96% pass)

**Target**: 100% pass rate on all platforms (or documented skips for legitimate limitations)
