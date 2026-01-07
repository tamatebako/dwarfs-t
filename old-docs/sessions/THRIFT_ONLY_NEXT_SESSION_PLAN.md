# Thrift-Only Fix - Next Session Continuation Plan

**Date**: 2025-12-02 23:05 HKT  
**Session Time**: 6+ hours  
**Status**: Deep debugging in progress  
**Priority**: CRITICAL for release

---

## Session Summary

### ✅ Completed:
1. **Benchmark infrastructure**: 100% complete (all 3 priorities)
2. **Thrift-only tools**: All 4 tools compile and work
3. **Fixed 16+ compilation errors**
4. **Fixed multiple default format issues**
5. **Debugged root cause**: FlatBuffers hardcoded in multiple places

### ⚠️ Current Blocker:

**Issue**: metadata_builder constructors architecture
- Tests segfault because they call FlatBuffers builder
- Root cause: `metadata_builder.cpp` constructors hardcoded to FlatBuffers
- Attempted fix: Convert to factory pattern
- New issue: Template instantiation complexity

### Files Modified This Session (8):
1. `.github/workflows/build.yml` - Thrift-only experimental
2. `.github/workflows/benchmark-comprehensive.yml` - 3-build support
3. `test/global_metadata_test.cpp` - Fixed headers
4. `test/tool_mkdwarfs_integration_test.cpp` - Removed obsolete tests
5. `cmake/tests.cmake` - Library dependencies
6. `include/dwarfs/writer/metadata_options.h` - Conditional default
7. `include/dwarfs/writer/scanner_options.h` - Conditional default
8. `cmake/libdwarfs.cmake` - Conditional compilation

**Plus**: 3 factory files (metadata_builder_factory.cpp, header updates)

---

## Root Cause Identified

**Problem**: `src/writer/internal/metadata_builder.cpp` contains:
1. FlatBuffers implementation (1,310 lines, always compiled)
2. Hardcoded constructors using FlatBuffers

**Debugger Evidence**:
```
flatbuffers_metadata_builder::update_totals_and_size_cache() + 156
```

**What Was Tried**:
1. ✅ Fixed default formats in headers
2. ✅ Guarded factory switch cases
3. ⚠️ Attempted factory pattern (template issues)

---

## Next Session: Clean Solution Approach

### Strategy A: Conditional Compilation (RECOMMENDED - 2 hours)

**Simplest Fix**: Make `metadata_builder.cpp` compile only with FLATBUFFERS

**Steps**:
1. In `cmake/libdwarfs.cmake` line 214:
   ```cmake
   $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_metadata_builder.cpp>
   ```

2. Move metadata_builder constructors to separate file:
   - `src/writer/internal/metadata_builder_constructors.cpp`
   - Compile conditionally based on available formats

3. Create simple conditional logic:
   ```cpp
   #ifdef DWARFS_HAVE_FLATBUFFERS
     // Use FlatBuffers builder
   #elif defined(DWARFS_HAVE_THRIFT)
     // Use Thrift builder
   #endif
   ```

### Strategy B: Proper Factory Pattern (4-6 hours)

**Complete the factory refactoring**:
1. Move constructor logic to factory methods
2. Fix template instantiation
3. Ensure proper inheritance
4. Test all paths

---

## Key Files for Next Session

**Must Review**:
1. `src/writer/internal/metadata_builder.cpp` (1,310 lines)
2. `cmake/libdwarfs.cmake` (line 2 14)
3. `src/writer/internal/metadata_builder_factory.cpp`

**Test File**:
- `test/metadata_test.cpp` - Use this to verify fix

**Debugging Command**:
```bash
cd build-tb
lldb -b -o "run --gtest_filter=metadata_test.basic" -o "bt" ./dwarfs_unit_tests
```

---

## Realistic Assessment

**Time Invested**: 6+ hours  
**Progress**: Tools work, tests blocked by architecture  
**Remaining**: 2-4 hours with Strategy A, 4-6 hours with Strategy B

**Critical Decision**:
- Deadline pressure suggests Strategy A (conditional compilation)
- Cleaner but more complex: Strategy B (factory pattern)

---

## Quick Start for Next Session

```bash
cd /Users/mulgogi/src/external/dwarfs

# Read status
cat doc/THRIFT_ONLY_NEXT_SESSION_PLAN.md
cat doc/THRIFT_ONLY_NEXT_SESSION_STATUS.md

# Current state
git status

# Test current (will fail)
cd build-tb && ./dwarfs_unit_tests --gtest_filter="metadata_test.basic"
```

---

**Status**: Paused at complex architectural fix  
**Recommendation**: Strategy A (conditional compilation) for time efficiency  
**Next Session**: 2-4 hours with clear approach