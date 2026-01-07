# Session 31E Continuation Prompt

**Date**: 2025-12-22
**Objective**: Fix compilation errors and complete domain-based metadata migration
**Duration**: 2-3 hours
**Previous**: Session 31D created domain views (627 lines), hit ~20 compilation errors

## Quick Start

```bash
# Read these files first:
cat doc/SESSION_31E_CONTINUATION_PLAN.md
cat doc/SESSION_31E_IMPLEMENTATION_STATUS.md
cat doc/SESSION_31_ARCHITECTURE_CORRECT.md

# Then start Phase 1: Fix constructor signature
# Edit: include/dwarfs/reader/internal/common_metadata_operations.h
```

## What Session 31D Accomplished

✅ **Domain Views Created** (627 lines total):
- `domain_metadata_views.h` (182 lines) - Interface wrappers
- `domain_metadata_views.cpp` (445 lines) - Implementations
- Updated CMake, fixed entry_count calculations

**Classes**: `domain_chunk_view`, `domain_chunk_range_impl`, `domain_inode_view_impl`, `domain_dir_entry_view_impl`, `domain_global_metadata`

## What Session 31E Must Fix

### 🔴 Critical Issues (~20 compilation errors)

**1. Constructor Signature** (PRIORITY 1 - 30 min)
- Missing `logger&` as first parameter in `common_metadata_operations`
- Both adapters need to pass logger

**2. Type Aliases** (PRIORITY 2 - 45 min)
- FlatBuffers-only build references deleted `flatbuffers_backend::*` types
- Update `metadata_types.h` to use `domain_*` view types

**3. Method Syntax** (PRIORITY 3 - 30 min)
- Domain model uses `.field()` not `.field`
- Fix ~8 locations in `common_metadata_operations.cpp`

**4. Type Construction** (PRIORITY 4 - 30 min)
- Fix domain view construction calls
- Verify all casts

## Execution Plan

### Phase 1: Constructor Fix (30 min)

**Files**: `common_metadata_operations.h`, `common_metadata_operations.cpp`, both adapters

Add `logger& lgr` as first parameter everywhere.

### Phase 2: Type Aliases (45 min)

**File**: `include/dwarfs/reader/metadata_types.h`

Change FlatBuffers-only section to:
```cpp
#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
#include <dwarfs/reader/internal/domain_metadata_views.h>
namespace dwarfs::reader::internal {
using inode_view_impl = domain_inode_view_impl;
using dir_entry_view_impl = domain_dir_entry_view_impl;
using global_metadata = domain_global_metadata;
using chunk_range = domain_chunk_range_impl;
}
```

### Phase 3: Method Syntax (30 min)

**File**: `src/reader/internal/common_metadata_operations.cpp`

Search/replace:
- `entry.name_index` → `entry.name_index()`
- `entry.inode_num` → `entry.inode_num()`
- `dir.first_entry` → `dir.first_entry()`
- `dir.parent_entry` → `dir.parent_entry()`
- `chunk.block` → `chunk.block()` (chunks only)

### Phase 4: Build Tests (2 hours)

```bash
# FlatBuffers-only
rm -rf build-fb-only
cmake -B build-fb-only -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=ON
ninja -C build-fb-only
ctest --test-dir build-fb-only

# Dual-format
rm -rf build-both
cmake -B build-both -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON -DWITH_TESTS=ON
ninja -C build-both
ctest --test-dir build-both
```

### Phase 5: Cleanup (30 min)

Delete 7,288 lines of old backend code:
```bash
git rm src/reader/internal/metadata_v2_{thrift,flatbuffers}.cpp \
       src/reader/internal/metadata_types_{thrift,flatbuffers}.cpp
```

## Key References

**Domain Model Types** (correct API):
- `metadata::domain::metadata` - Main container
- `metadata::domain::inode_data` - Access: `meta_.inodes[index]`
- `metadata::domain::dir_entry` - Methods: `inode_num()`, `name_index()`
- `metadata::domain::directory` - Methods: `first_entry()`, `parent_entry()`
- `metadata::domain::chunk` - Methods: `block()`, `offset()`, `size()`

**Lookup Tables**:
- Names: `meta_.names[index]` OR `meta_.compact_names->get(index)` if compressed
- Modes: `meta_.modes[inode.mode_index]`
- UIDs: `meta_.uids[inode.owner_index]`
- GIDs: `meta_.gids[inode.group_index]`

## Success Criteria

- ✅ FlatBuffers-only build passes
- ✅ Dual-format build passes
- ✅ All tests pass
- ✅ Integration tests pass
- ✅ Old backends deleted (7,288 lines)
- ✅ Net: -6,661 lines (-85%)

## Architecture Achievement

**Before**: 7,288 lines of duplicate backend implementations  
**After**: 1,550 lines with single domain-based implementation  
**Result**: 85% code reduction, format-agnostic operations

---

**Status**: Ready to execute  
**Next**: Phase 1 - Fix constructor signature  
**Time**: 2-3 hours to completion
