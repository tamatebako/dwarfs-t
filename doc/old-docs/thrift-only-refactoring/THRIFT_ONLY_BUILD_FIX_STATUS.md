# Thrift-Only Build Fix - Status Report

**Date**: 2025-11-29 11:11 HKT  
**Session**: 10 (Benchmarking + Investigation)  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Status**: ⚠️ IN PROGRESS - Requires deeper refactoring

---

## Problem Summary

**Goal**: Enable Thrift-only builds (currently fails with 6 type conflict errors)

**Root Cause**: Legacy `internal/metadata_types.h` file defines classes that conflict with type aliases in single-format builds

---

## Fixes Applied (Session 10)

### Fix 1: Remove Legacy Include from metadata_v2.h ✅
**File**: [`include/dwarfs/reader/internal/metadata_v2.h:46`](../include/dwarfs/reader/internal/metadata_v2.h:46)

**Before**:
```cpp
#include <dwarfs/reader/internal/metadata_types.h>  // WRONG!
```

**After**:
```cpp
// DO NOT include legacy internal/metadata_types.h - it conflicts with type aliases
```

**Result**: Removed conflict source from header

### Fix 2: Remove Legacy Include from metadata_types.cpp ✅
**File**: [`src/reader/metadata_types.cpp:49`](../src/reader/metadata_types.cpp:49)

**Before**:
```cpp
#include <dwarfs/reader/internal/metadata_types.h>  // WRONG!
```

**After**:
```cpp
// DO NOT include legacy internal/metadata_types.h - it conflicts with type aliases
```

**Result**: Removed conflict source from implementation

---

## Remaining Errors (2)

### Error 1: Forward Declaration Conflict

**File**: [`include/dwarfs/reader/internal/time_resolution_handler.h:62`](../include/dwarfs/reader/internal/time_resolution_handler.h:62)

**Error**:
```
error: definition of type 'inode_view_impl' conflicts with type alias
   62 | class inode_view_impl;
```

**Issue**: Forward declaration conflicts with type alias in Thrift-only builds

**In Thrift-only mode**:
- `inode_view_impl` is a type alias: `using inode_view_impl = thrift_backend::inode_view_impl;`
- Forward declaration `class inode_view_impl;` creates conflict

**Solution Needed**: Conditional forward declaration
```cpp
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: forward declare interface type
  class inode_view_impl;
#elif defined(DWARFS_HAVE_THRIFT)
  // Thrift-only: forward declare backend type
  namespace thrift_backend {
    class inode_view_impl;
  }
#elif defined(DWARFS_HAVE_FLATBUFFERS)
  // FlatBuffers-only: forward declare backend type
  namespace flatbuffers_backend {
    class inode_view_impl;
  }
#endif
```

### Error 2: Method Redefinition

**File**: [`src/reader/internal/metadata_v2_thrift.cpp:2426`](../src/reader/internal/metadata_v2_thrift.cpp:2426)

**Error**:
```
error: redefinition of 'get_chunks'
 2426 | tb::chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
```

**Issue**: Return type mismatch in Thrift-only builds

**In metadata_v2.h**:
```cpp
chunk_range get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}
```

**In metadata_v2_thrift.cpp**:
```cpp
tb::chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
  // Implementation
}
```

**Problem**: 
- In Thrift-only: `chunk_range` is `thrift_backend::chunk_range`
- Method is defined inline in header
- Implementation in .cpp redefines it

**Solution Needed**: Make method virtual or move to impl class only

---

## Architecture Analysis

### Current Design Issues

1. **Inline Methods**: `metadata_v2.h` defines methods inline that delegate to `impl_`
2. **Type Aliasing**: Single-format builds alias types to backend types
3. **Forward Declarations**: Headers forward-declare classes that are now type aliases

### Correct Architecture (MECE + OOP)

```
┌─────────────────────────────────────────────────┐
│         metadata_v2 (facade/pimpl)              │
├─────────────────────────────────────────────────┤
│ - NO inline implementations                     │
│ - ALL methods delegate to impl_                 │
│ - Return interface types (not backend types)    │
└──────────────────┬──────────────────────────────┘
                   │
          ┌────────┴────────┐
          │                 │
          ▼                 ▼
┌──────────────────┐  ┌──────────────────┐
│ metadata_v2_impl │  │ metadata_v2_impl │
│ (FlatBuffers)    │  │ (Thrift)         │
├──────────────────┤  ├──────────────────┤
│ Returns FB types │  │ Returns TB types │
│ wrapped in       │  │ wrapped in       │
│ interface        │  │ interface        │
└──────────────────┘  └──────────────────┘
```

---

## Required Refactoring (5-6 hours)

### Phase 1: Fix Forward Declarations (1h)

**Files**:
- [`include/dwarfs/reader/internal/time_resolution_handler.h`](../include/dwarfs/reader/internal/time_resolution_handler.h)
- Any other files with forward declarations

**Pattern**:
```cpp
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: use interface
  class inode_view_impl;
#else
  // Single-format: forward-declare from backend namespace
  #ifdef DWARFS_HAVE_THRIFT
  namespace thrift_backend {
    class inode_view_impl;
  }
  using thrift_backend::inode_view_impl;
  #endif
  
  #ifdef DWARFS_HAVE_FLATBUFFERS
  namespace flatbuffers_backend {
    class inode_view_impl;
  }
  using flatbuffers_backend::inode_view_impl;
  #endif
#endif
```

### Phase 2: Remove Inline Implementations (2h)

**File**: [`include/dwarfs/reader/internal/metadata_v2.h`](../include/dwarfs/reader/internal/metadata_v2.h)

**Current** (lines 166-168):
```cpp
chunk_range get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}
```

**Correct**:
```cpp
// Header: declare only
chunk_range get_chunks(int inode, std::error_code& ec) const;

// Implementation in metadata_v2.cpp:
chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}
```

**Apply to ALL inline methods** (~15 methods)

### Phase 3: Test All Configurations (1h)

Build and validate:
```bash
# FlatBuffers-only
cmake -B build-fb -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
ninja -C build-fb mkdwarfs

# Thrift-only
cmake -B build-tb -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
ninja -C build-tb mkdwarfs

# Dual-format
cmake -B build- dual -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
ninja -C build-dual mkdwarfs
```

### Phase 4: Update Benchmarks (1h)

Re-run benchmarks with working Thrift-only build

---

## Current Build Status

| Build | Status | Errors | Notes |
|-------|--------|--------|-------|
| FlatBuffers-only | ✅ WORKS | 0 | Fully functional |
| Dual-format | ✅ WORKS | 0 | Fully functional |
| Thrift-only | ❌ FAILS | 2 | Forward declaration + inline method conflicts |

---

## Recommendation

**For v0.16.0 Release**:
- Document that Thrift-only is NOT supported (FlatBuffers required)
- FlatBuffers-only and Dual-format are production-ready
- Thrift-only support can be added in future release if needed

**Rationale**:
- Thrift-only requires significant refactoring (5-6 hours)
- FlatBuffers is the modern default anyway
- Dual-format provides backward compatibility for Thrift images
- Very few users would need Thrift-only (legacy compatibility is covered by dual-format)

---

## Next Session Actions

If fixing Thrift-only is required:

1. Read this status document
2. Read [`doc/THRIFT_ONLY_BUILD_CONTINUATION_PLAN.md`](THRIFT_ONLY_BUILD_CONTINUATION_PLAN.md)
3. Start with Phase 1: Fix forward declarations
4. Proceed with Phase 2: Remove inline implementations
5. Test all three configurations
6. Update benchmarks

---

**Document Version**: 1.0  
**Created**: 2025-11-29 11:11 HKT  
**Status**: Investigation complete, refactoring plan ready