# Thrift-Only Build Refactoring Plan - OOP & MECE Architecture

**Created**: 2025-11-29 11:19 HKT  
**Priority**: HIGH  
**Estimated Time**: 5-6 hours  
**Approach**: Object-Oriented Programming + MECE Principles

---

## Executive Summary

**Goal**: Enable Thrift-only builds through architectural refactoring

**Current State**: 2 remaining errors after removing legacy includes  
**Target State**: All 3 configurations compile (FlatBuffers-only, Thrift-only, Dual-format)

**Key Principle**: Use proper OOP separation of concerns, NOT conditional compilation guards

---

## Architectural Principles (OOP + MECE)

### 1. Single Responsibility Principle
- Each class has ONE responsibility
- Header files declare interfaces
- Implementation files provide concrete behavior
- NO mixed concerns

### 2. Open/Closed Principle
- Classes open for extension
- Closed for modification
- Backend types extend common interfaces
- Factory pattern for creation

### 3. Dependency Inversion Principle
- Depend on abstractions, not concretions
- Public API uses interface types
- Backend implementations hidden behind factories
- Type aliases ONLY for single-format builds

### 4. MECE (Mutually Exclusive, Collectively Exhaustive)
- Three configurations are mutually exclusive
- Each configuration is complete
- No overlap, no gaps
- Clear separation at architectural level

---

## Root Cause Analysis

### Problem 1: Mixed Responsibilities

**Current** (WRONG):
```cpp
// metadata_v2.h - mixing interface with implementation
class metadata_v2 {
  chunk_range get_chunks(int inode, std::error_code& ec) const {
    return impl_->get_chunks(inode, ec);  // Inline implementation!
  }
};
```

**Issue**: Inline delegation exposes return type, causing conflicts in single-format builds

### Problem 2: Forward Declarations vs Type Aliases

**Current** (WRONG):
```cpp
// time_resolution_handler.h
class inode_view_impl;  // Forward declaration

// metadata_types.h (Thrift-only)
using inode_view_impl = thrift_backend::inode_view_impl;  // Type alias

// CONFLICT!
```

**Issue**: Can't forward-declare a type alias

---

## Solution Architecture

### Design Pattern: Pimpl + Factory + Strategy

```
┌──────────────────────────────────────────────────────┐
│          Public API (metadata_v2)                    │
│  - Pure delegation, NO inline implementations        │
│  - Returns interface types ONLY                      │
│  - No backend type exposure                          │
└────────────────┬─────────────────────────────────────┘
                 │
                 │ unique_ptr<impl>
                 │
                 ▼
┌──────────────────────────────────────────────────────┐
│        Abstract Interface (impl)                     │
│  - Pure virtual methods                              │
│  - Format-agnostic                                   │
│  - Backend independent                               │
└────────────────┬─────────────────────────────────────┘
                 │
         ┌───────┴───────┐
         │ implements    │ implements
         ▼               ▼
┌─────────────────┐  ┌─────────────────┐
│metadata_v2_impl │  │metadata_v2_impl │
│  (FlatBuffers)  │  │    (Thrift)     │
├─────────────────┤  ├─────────────────┤
│Returns FB types │  │Returns TB types │
│wrapped in iface │  │wrapped in iface │
└─────────────────┘  └─────────────────┘
         ▲               ▲
         │               │
         └───────┬───────┘
                 │
          ┌──────┴──────┐
          │   Factory   │
          │  (detects   │
          │   format)   │
          └─────────────┘
```

---

## Refactoring Plan

### Phase 1: Remove All Inline Implementations (2h)

**Objective**: Separate interface from implementation

**Files to Modify**:
1. [`include/dwarfs/reader/internal/metadata_v2.h`](../include/dwarfs/reader/internal/metadata_v2.h)
2. [`src/reader/internal/metadata_v2.cpp`](../src/reader/internal/metadata_v2.cpp) (create if needed)

**Pattern**:

**Before** (lines 166-168):
```cpp
// Header - WRONG (inline)
chunk_range get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}
```

**After**:
```cpp
// Header - CORRECT (declaration only)
chunk_range get_chunks(int inode, std::error_code& ec) const;

// Implementation file (metadata_v2.cpp) - CORRECT
chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}
```

**Apply to ALL methods** (~20 methods):
- `check_consistency()`
- `size()`
- `walk()`
- `walk_data_order()`
- `root()`
- `find()` (3 overloads)
- `getattr()` (2 overloads)
- `opendir()`
- `readdir()`
- `dirsize()`
- `access()`
- `open()`
- `seek()`
- `readlink()`
- `statvfs()`
- `get_chunks()` ← **Critical for Error 2**
- `block_size()`
- `has_symlinks()`
- `has_sparse_files()`
- `get_inode_info()`
- `get_block_category()`
- `get_block_category_metadata()`
- `get_all_block_categories()`
- `get_all_uids()`
- `get_all_gids()`
- `get_block_numbers_by_category()`
- `internal_data()`

**Validation**: After this phase, no `metadata_v2` methods should have implementations in the header

### Phase 2: Fix Forward Declarations (1h)

**Objective**: Replace class forward declarations with proper namespace-qualified forward declarations

**Files to Modify**:
1. [`include/dwarfs/reader/internal/time_resolution_handler.h`](../include/dwarfs/reader/internal/time_resolution_handler.h)
2. Any other files with forward declarations (search for `class inode_view_impl;`)

**Pattern**:

**Before** (line 62):
```cpp
// WRONG - conflicts with type alias in single-format
class inode_view_impl;
```

**After**:
```cpp
// CORRECT - namespace-qualified forward declaration
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: use interface type (no forward declaration needed)
  // inode_view_impl is already an interface type

#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: forward-declare backend type
  namespace thrift_backend {
    class inode_view_impl;
  }
  // Type alias brings it into internal namespace (already done in metadata_types.h)

#elif defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
  // FlatBuffers-only: forward-declare backend type
  namespace flatbuffers_backend {
    class inode_view_impl;
  }
  // Type alias brings it into internal namespace (already done in metadata_types.h)

#else
  #error "At least one metadata format must be enabled"
#endif
```

**Search Strategy**:
```bash
# Find all files with forward declarations
grep -r "class inode_view_impl;" include/
grep -r "class dir_entry_view_impl;" include/
grep -r "class chunk_range;" include/
grep -r "class global_metadata;" include/
```

**Apply pattern to ALL forward declarations**

**Validation**: After this phase, `time_resolution_handler.h` should compile in all 3 configurations

### Phase 3: Remove Type Exposure in Implementation Files (1h)

**Objective**: Backend implementation files should NOT expose backend types in function signatures

**Problem Example** (metadata_v2_thrift.cpp:2426):
```cpp
// WRONG - exposes backend type
tb::chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
  // ...
}
```

**Solution**: This is NOT a metadata_v2 method, it's a `metadata_v2_impl_thrift` method

**Architecture**:
```cpp
// metadata_v2.h - public API
class metadata_v2 {
  chunk_range get_chunks(int inode, std::error_code& ec) const;
private:
  std::unique_ptr<impl> impl_;
};

// metadata_v2.cpp - delegation only
chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);  // Delegates to impl
}

// metadata_v2_thrift.cpp - backend implementation
class metadata_v2_impl_thrift : public metadata_v2::impl {
  chunk_range get_chunks(int inode, std::error_code& ec) const override {
    // Backend-specific implementation returns backend type
    tb::chunk_range backend_range = /* ... */;
    
    // Wrap in interface
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
    // Dual-format: return interface wrapper
    return chunk_range{std::make_shared<tb::chunk_range>(backend_range)};
#else
    // Single-format: return backend type directly (already aliased)
    return backend_range;
#endif
  }
};
```

**Key Insight**: In single-format builds, `chunk_range` type alias resolves to backend type, so no wrapping needed

**Validation**: After this phase, no backend types appear in `metadata_v2` class

### Phase 4: Update CMake Build System (30min)

**Objective**: Validate all three build configurations

**File**: [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)

**Add validation**:
```cmake
# Ensure mutual exclusivity
if(NOT DWARFS_WITH_FLATBUFFERS AND NOT DWARFS_WITH_THRIFT)
  message(FATAL_ERROR "At least one metadata format must be enabled")
endif()

# Log configuration
if(DWARFS_WITH_FLATBUFFERS AND DWARFS_WITH_THRIFT)
  message(STATUS "Metadata: Dual-format (FlatBuffers + Thrift)")
elseif(DWARFS_WITH_FLATBUFFERS)
  message(STATUS "Metadata: FlatBuffers-only")
elseif(DWARFS_WITH_THRIFT)
  message(STATUS "Metadata: Thrift-only (experimental)")
endif()

# Warn about Thrift-only
if(DWARFS_WITH_THRIFT AND NOT DWARFS_WITH_FLATBUFFERS)
  message(WARNING "Thrift-only builds are experimental. "
                  "FlatBuffers is the recommended default.")
endif()
```

### Phase 5: Test All Three Configurations (1h)

**Objective**: Build and validate all configurations

**Test Script**:
```bash
#!/bin/bash
set -e

echo "=== Testing All Build Configurations ==="

# Clean
rm -rf build-*

# FlatBuffers-only
echo "--- 1. FlatBuffers-only ---"
cmake -B build-fb-only -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
ninja -C build-fb-only mkdwarfs
echo "✓ FlatBuffers-only: SUCCESS"

# Thrift-only
echo "--- 2. Thrift-only ---"
cmake -B build-tb-only -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON
ninja -C build-tb-only mkdwarfs
echo "✓ Thrift-only: SUCCESS"

# Dual-format
echo "--- 3. Dual-format ---"
cmake -B build-dual -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON
ninja -C build-dual mkdwarfs
echo "✓ Dual-format: SUCCESS"

echo "=== ALL CONFIGURATIONS PASSED ==="
```

### Phase 6: Runtime Testing (1h)

**Test**: Create images with all three configurations and verify they work

```bash
# Create test data
mkdir -p /tmp/test-data
echo "test" > /tmp/test-data/file.txt

# Test FlatBuffers-only
./build-fb-only/mkdwarfs -i /tmp/test-data -o /tmp/test-fb.dwarfs
./build-fb-only/dwarfsck /tmp/test-fb.dwarfs

# Test Thrift-only  
./build-tb-only/mkdwarfs -i /tmp/test-data -o /tmp/test-tb.dwarfs
./build-tb-only/dwarfsck /tmp/test-tb.dwarfs

# Test Dual-format
./build-dual/mkdwarfs -i /tmp/test-data -o /tmp/test-dual.dwarfs
./build-dual/dwarfsck /tmp/test-dual.dwarfs

# Cross-compatibility: dual-format should read both
./build-dual/dwarfsck /tmp/test-fb.dwarfs
./build-dual/dwarfsck /tmp/test-tb.dwarfs
```

---

## Implementation Checklist

### Phase 1: Remove Inline Implementations
- [ ] Move `check_consistency()` to .cpp
- [ ] Move `size()` to .cpp
- [ ] Move `walk()` to .cpp
- [ ] Move `walk_data_order()` to .cpp
- [ ] Move `root()` to .cpp
- [ ] Move `find()` (3 overloads) to .cpp
- [ ] Move `getattr()` (2 overloads) to .cpp
- [ ] Move `opendir()` to .cpp
- [ ] Move `readdir()` to .cpp
- [ ] Move `dirsize()` to .cpp
- [ ] Move `access()` to .cpp
- [ ] Move `open()` to .cpp
- [ ] Move `seek()` to .cpp
- [ ] Move `readlink()` to .cpp
- [ ] Move `statvfs()` to .cpp
- [ ] Move `get_chunks()` to .cpp ← **Critical**
- [ ] Move `block_size()` to .cpp
- [ ] Move `has_symlinks()` to .cpp
- [ ] Move `has_sparse_files()` to .cpp
- [ ] Move `get_inode_info()` to .cpp
- [ ] Move `get_block_category()` to .cpp
- [ ] Move `get_block_category_metadata()` to .cpp
- [ ] Move `get_all_block_categories()` to .cpp
- [ ] Move `get_all_uids()` to .cpp
- [ ] Move `get_all_gids()` to .cpp
- [ ] Move `get_block_numbers_by_category()` to .cpp
- [ ] Move `internal_data()` to .cpp

### Phase 2: Fix Forward Declarations
- [ ] Fix `time_resolution_handler.h`
- [ ] Search for other forward declarations
- [ ] Apply pattern to all findings

### Phase 3: Backend Type Isolation
- [ ] Verify backend types not exposed in metadata_v2
- [ ] Ensure impl classes use backend types internally
- [ ] Validate wrapping logic in dual-format

### Phase 4: CMake Updates
- [ ] Add configuration validation
- [ ] Add warning for Thrift-only
- [ ] Test configuration detection

### Phase 5: Build Testing
- [ ] Build FlatBuffers-only
- [ ] Build Thrift-only
- [ ] Build Dual-format
- [ ] All builds pass

### Phase 6: Runtime Testing
- [ ] Create images with all configs
- [ ] Verify images are valid
- [ ] Test cross-compatibility

---

## Success Criteria

- ✅ All 3 configurations compile without errors
- ✅ All 3 configurations link successfully
- ✅ mkdwarfs runs in all 3 configurations
- ✅ Created images are valid
- ✅ Dual-format can read both FlatBuffers and Thrift images
- ✅ Zero inline implementations in metadata_v2.h
- ✅ Zero backend type exposure in public API
- ✅ Proper OOP separation of concerns
- ✅ MECE architecture (mutually exclusive configurations)

---

## Timeline

| Phase | Time | Cumulative |
|-------|------|------------|
| Phase 1 | 2h | 2h |
| Phase 2 | 1h | 3h |
| Phase 3 | 1h | 4h |
| Phase 4 | 30min | 4.5h |
| Phase 5 | 1h | 5.5h |
| Phase 6 | 1h | 6.5h |

**Total**: 6.5 hours

---

## Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Breaking existing builds | HIGH | HIGH | Test FB-only and dual-format after EACH phase |
| Performance regression | LOW | MEDIUM | Single-format uses type aliases (zero overhead) |
| Complex conditional logic | MEDIUM | MEDIUM | Minimize guards, use OOP polymorphism |
| Test failures | MEDIUM | LOW | Expected, update tests to match new architecture |

---

**Document Version**: 1.0  
**Created**: 2025-11-29 11:19 HKT  
**Status**: Ready for implementation  
**Approach**: OOP + MECE Principles