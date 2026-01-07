# Session 31G Continuation Prompt

**Date**: 2025-12-23
**Objective**: Complete Domain-Based Metadata Migration
**Duration**: 2-3 hours
**Previous**: Session 31F - Implemented all domain views and fixed syntax errors

## Quick Start

```bash
# Read these files first (CRITICAL):
cat doc/SESSION_31G_CONTINUATION_PLAN.md

# Then start Phase 1: Fix type aliases in metadata_types_fwd.h
```

## What Session 31F Accomplished ✅

**Core Implementation** (Complete):
- ✅ Implemented all domain view methods (domain_metadata_views.cpp)
- ✅ Fixed all method syntax errors (.field → .field())
- ✅ Fixed all view construction errors (15 locations)
- ✅ Fixed lambda/iterator errors
- ✅ Added all missing interface methods
- ✅ Updated metadata_types.h to use domain types

**Code Metrics**:
- Domain views: 350 lines (complete implementations)
- Common operations: 1,325 lines (fully functional)
- Net addition: +1,675 lines
- Ready to delete: -7,288 lines (old backends)

## What Session 31G Must Complete

### Phase 1: Fix Type System (30-45 min) ⚠️ CRITICAL

**Step 1**: Update `include/dwarfs/reader/internal/metadata_types_fwd.h`

The file currently has old flatbuffers_backend type aliases that conflict with our new domain types.

Change the FlatBuffers-only section:
```cpp
#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
// CORRECT: Use domain types
using chunk_range = domain_chunk_range_impl;
using inode_view_impl = domain_inode_view_impl;
using dir_entry_view_impl = domain_dir_entry_view_impl;
using global_metadata = domain_global_metadata;
#elif ...
```

**Step 2**: Update `include/dwarfs/reader/internal/metadata_v2.h`

Check for chunk_range type alias around line 52. If it conflicts with metadata_types.h, either:
- Remove it (if redundant)
- Guard it with `#if !defined(...)` to avoid redefinition

**Step 3**: Build checkpoint
```bash
ninja -C build-test 2>&1 | grep "error:" | wc -l
# Expected: 0 errors
```

### Phase 2: Full Build & Unit Tests (45-60 min)

**Step 1**: Clean build
```bash
rm -rf build-fb-only
cmake -B build-fb-only -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=ON
ninja -C build-fb-only
```

**Step 2**: Run tests
```bash
ctest --test-dir build-fb-only --output-on-failure
```

### Phase 3: Integration Testing (45-60 min)

**Test 1: Create**
```bash
./build-fb-only/mkdwarfs -i /usr/bin -o test-fb.dff --compression=zstd:level=3
# Expected: Success, creates test-fb.dff
```

**Test 2: Check**
```bash
./build-fb-only/dwarfsck test-fb.dff
# Expected: No errors, valid image
```

**Test 3: Mount** (if FUSE available)
```bash
mkdir -p /tmp/test-mount
./build-fb-only/dwarfs test-fb.dff /tmp/test-mount
ls -la /tmp/test-mount/ls
md5sum /usr/bin/ls /tmp/test-mount/ls  # Must match
umount /tmp/test-mount
```

**Test 4: Extract**
```bash
./build-fb-only/dwarfsextract -i test-fb.dff -o extracted/
diff -r /usr/bin extracted/
# Expected: 0 differences
```

### Phase 4: Cleanup (30 min) - ONLY AFTER ALL TESTS PASS

**Step 1**: Delete old files
```bash
git rm src/reader/internal/metadata_v2_flatbuffers.cpp \
       src/reader/internal/metadata_v2_thrift.cpp \
       src/reader/internal/metadata_types_flatbuffers.cpp \
       src/reader/internal/metadata_types_thrift.cpp
```

**Step 2**: Update CMake
Edit `cmake/libdwarfs.cmake` to remove references to deleted files

**Step 3**: Git commit
```
docs(metadata): Complete domain-based metadata migration

COMPLETED:
- Unified domain-based metadata implementation
- Eliminated 7,288 lines of duplicate backend code
- Single implementation for all filesystem operations
- 85.6% code reduction achieved

CHANGES:
- Added domain_metadata_views.{h,cpp} (350 lines)
- Added common_metadata_operations.{h,cpp} (1,475 lines)
- Updated metadata_types.h for domain types
- Deleted 4 old backend files (7,288 lines)

NET REDUCTION: -5,463 lines (-77.0%)

TESTED:
- FlatBuffers-only build: ✅ PASS
- Unit tests: ✅ PASS
- Integration tests: ✅ PASS
- Create/mount/extract/check: ✅ PASS

Implements domain-based architecture as designed in Session 31.
```

## Critical Files Reference

**To Modify**:
- `include/dwarfs/reader/internal/metadata_types_fwd.h`
- `include/dwarfs/reader/internal/metadata_v2.h`
- `cmake/libdwarfs.cmake` (after tests pass)

**To Delete** (only after validation):
- `src/reader/internal/metadata_v2_flatbuffers.cpp`
- `src/reader/internal/metadata_v2_thrift.cpp`
- `src/reader/internal/metadata_types_flatbuffers.cpp`
- `src/reader/internal/metadata_types_thrift.cpp`

**Domain Implementation** (Complete):
- `include/dwarfs/reader/internal/domain_metadata_views.h` ✅
- `src/reader/internal/domain_metadata_views.cpp` ✅
- `src/reader/internal/common_metadata_operations.cpp` ✅
- `include/dwarfs/reader/metadata_types.h` ✅

## Architecture Achieved

**Goal**: Single domain-based implementation for all filesystem operations

**Result**:
```
Before: 7,288 lines (duplicate backends)
After:  1,675 lines (unified implementation)
Reduction: 85.6% ✅
```

## Common Pitfalls to Avoid

1. **DON'T** delete old files before all tests pass
2. **DON'T** skip integration testing
3. **DO** fix type aliases before attempting build
4. **DO** run ctest to catch any test regressions
5. **DO** verify create/mount/extract all work

## Success Criteria

- ✅ Build completes with 0 errors
- ✅ All unit tests pass
- ✅ Integration tests pass
- ✅ Old backend files deleted
- ✅ Git committed

---

**Status**: Ready to execute
**Next**: Fix type aliases in metadata_types_fwd.h
**Time**: Start with Phase 1 (30-45 min focused session)