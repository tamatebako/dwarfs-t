# Session 12: Folly Allocator Fix - Implementation Status

**Created**: 2025-12-17
**Last Updated**: 2025-12-17
**Status**: 🟡 READY TO START

---

## Quick Status

| Phase | Status | Duration | Completion |
|-------|--------|----------|------------|
| A: Investigate Folly Configuration | ⬜ Not Started | 30 min | 0% |
| B: Fix Allocator Configuration | ⬜ Not Started | 45 min | 0% |
| C: Verify Build and Linking | ⬜ Not Started | 30 min | 0% |
| D: Update Test Script | ⬜ Not Started | 15 min | 0% |
| E: Documentation | ⬜ Not Started | 15 min | 0% |
| **TOTAL** | **⬜ Not Started** | **2h 15min** | **0%** |

---

## Phase A: Investigate Folly Configuration (⬜ Not Started)

### Objectives
- [ ] Read `cmake/folly.cmake` completely
- [ ] Identify allocator configuration location
- [ ] Check if `FOLLY_USE_JEMALLOC` is set
- [ ] Verify compile definitions propagate

### Files to Examine
- `cmake/folly.cmake` (primary)
- `folly/CMakeLists.txt`
- `CMakeLists.txt` (Folly includes)

### Expected Findings
- Current allocator detection logic
- Missing `FOLLY_USE_JEMALLOC=0` definition
- Incomplete target_compile_definitions

### Deliverables
- [ ] Analysis document created
- [ ] Missing settings identified
- [ ] Fix approach confirmed

---

## Phase B: Fix Allocator Configuration (⬜ Not Started)

### Objectives
- [ ] Add explicit `FOLLY_USE_JEMALLOC=0` when `USE_JEMALLOC=OFF`
- [ ] Add explicit `FOLLY_USE_TCMALLOC=0` when `USE_JEMALLOC=OFF`
- [ ] Ensure definitions propagate to all Folly targets
- [ ] Add macOS ARM64-specific handling

### Files to Modify
- `cmake/folly.cmake` (primary fix location)

### Implementation Checklist
- [ ] Add compile definitions to folly_base target
- [ ] Use PUBLIC visibility for propagation
- [ ] Add platform-specific definitions for macOS ARM64
- [ ] Verify no conflicting definitions elsewhere

### Expected Changes
```cmake
# In cmake/folly.cmake, near folly_base target definition:

# Explicit allocator control
if(NOT USE_JEMALLOC)
  target_compile_definitions(folly_base PUBLIC
    FOLLY_USE_JEMALLOC=0
    FOLLY_USE_TCMALLOC=0
  )
endif()

# macOS ARM64 specific
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
  target_compile_definitions(folly_base PUBLIC
    FOLLY_HAVE_MALLOC_USABLE_SIZE=0
  )
endif()
```

### Validation
- [ ] Build succeeds with changes
- [ ] No allocator symbols in object files
- [ ] All Folly targets inherit definitions

---

## Phase C: Verify Build and Linking (⬜ Not Started)

### Test Matrix

#### Test 1: Thrift-Only Build
```bash
rm -rf build-thrift-verify
cmake -B build-thrift-verify -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DUSE_JEMALLOC=OFF \
  -DENABLE_RICEPP=OFF \
  -DWITH_TESTS=ON \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=OFF

ninja -C build-thrift-verify dwarfs_filesystem_tests
./build-thrift-verify/dwarfs_filesystem_tests
```

**Expected**: ✅ Build success, 18/18 tests passing

#### Test 2: Dual-Format Build
```bash
rm -rf build-both-verify
cmake -B build-both-verify -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DUSE_JEMALLOC=OFF \
  -DENABLE_RICEPP=OFF \
  -DWITH_TESTS=ON \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=OFF

ninja -C build-both-verify dwarfs_filesystem_tests
./build-both-verify/dwarfs_filesystem_tests
```

**Expected**: ✅ Build success, 18/18 tests passing

### Verification Checklist
- [ ] Thrift-only: CMake configures successfully
- [ ] Thrift-only: Build completes without undefined symbols
- [ ] Thrift-only: Tests pass (18/18)
- [ ] Dual-format: CMake configures successfully
- [ ] Dual-format: Build completes without undefined symbols
- [ ] Dual-format: Tests pass (18/18)
- [ ] No jemalloc/tcmalloc in link command

---

## Phase D: Update Test Script (⬜ Not Started)

### Objectives
- [ ] Remove macOS platform skip logic
- [ ] Test all 3 configs on macOS
- [ ] Verify comprehensive output

### File to Modify
- `scripts/test-all-configs.sh`

### Changes Required
Remove lines:
```bash
# Skip problematic configs on macOS
if [[ "$PLATFORM" == "Darwin" ]]; then
  echo "⚠️  macOS detected: Skipping Thrift/dual-format (Folly linking issues)"
  echo ""
  CONFIGS=("flatbuffers-only")
  FLATBUFFERS_FLAGS=("ON")
  THRIFT_FLAGS=("OFF")
fi
```

### Expected Output
```
Platform: Darwin arm64

Testing: flatbuffers-only
✅ PASSED: flatbuffers-only (18 tests)

Testing: thrift-only
✅ PASSED: thrift-only (18 tests)

Testing: both-formats
✅ PASSED: both-formats (18 tests)

✅ ALL TESTED CONFIGURATIONS PASSED
```

### Validation Checklist
- [ ] Script tests all 3 configs
- [ ] All configs pass on macOS
- [ ] Clear success messages

---

## Phase E: Documentation (⬜ Not Started)

### Objectives
- [ ] Document the fix in session summary
- [ ] Update memory bank context
- [ ] Remove platform limitation from README

### Files to Create/Modify
1. **Create**: `doc/SESSION_12_COMPLETE_SUMMARY.md`
2. **Update**: `.kilocode/rules/memory-bank/context.md`
3. **Update**: Session 11 summary (note fix in Session 12)

### Documentation Checklist
- [ ] SESSION_12_COMPLETE_SUMMARY.md created
- [ ] Technical details documented
- [ ] Fix approach explained
- [ ] Test results included
- [ ] Memory bank updated
- [ ] Platform limitation removed from all docs

---

## Known Issues to Fix

### Issue 1: Undefined Allocator Symbols
**Status**: 🟡 Root cause identified, fix planned
**Fix**: Add explicit `FOLLY_USE_JEMALLOC=0` compile definitions

### Issue 2: Platform Skip Logic
**Status**: 🟡 Temporary workaround in test script
**Fix**: Remove after allocator fix verified

---

## Success Criteria

### Build Success
- [x] ~~Session 11: FlatBuffers-only builds~~
- [ ] Thrift-only builds on macOS ARM64
- [ ] Dual-format builds on macOS ARM64
- [ ] No undefined symbols in any config

### Test Success
- [x] ~~Session 11: FlatBuffers-only 18/18 tests~~
- [ ] Thrift-only 18/18 tests
- [ ] Dual-format 18/18 tests
- [ ] Automation script tests all configs

### Code Quality
- [ ] Centralized allocator configuration
- [ ] Platform-aware defaults
- [ ] No scattered code guards
- [ ] Clean architectural solution

### Documentation
- [ ] Fix documented
- [ ] Platform limitation removed
- [ ] Test matrix complete
- [ ] Memory bank updated

---

## Implementation Notes

### Key Insight
The issue is purely CMake configuration - Folly's allocator detection runs even when `USE_JEMALLOC=OFF`, generating code that expects allocator symbols. Solution: Override with explicit compile definitions.

### Architecture Principle
**MECE Configuration**: One source of truth (CMake options) → Explicit overrides → Consistent behavior across all targets.

**No Code Guards**: Fix at configuration level, not with scattered `#ifdef` throughout code.

---

## Rollback Plan

If fix causes regressions:
1. Revert `cmake/folly.cmake` changes
2. Restore platform skip in test script
3. Document as known limitation
4. Investigate alternative approaches

**Probability**: Low (changes are isolated and well-tested)

---

**Status**: 🟡 READY TO START
**Blocker**: None
**Next Action**: Read `cmake/folly.cmake` (Phase A)