# Thrift-Only Build Fix - Continuation Plan

**Created**: 2025-11-29 11:06 HKT  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Status**: New architectural work required  
**Priority**: HIGH

---

## Problem Statement

**Current State**: Thrift-only build FAILS with 6 type conflict errors

```
error: definition of type 'dir_entry_view_impl' conflicts with type alias
error: definition of type 'chunk_range' conflicts with type alias  
error: definition of type 'inode_view_impl' conflicts with type alias
error: redefinition of 'get_chunks'
```

**Root Cause**: Type aliasing conflicts in single-format builds

In [`include/dwarfs/reader/metadata_types.h`](../include/dwarfs/reader/metadata_types.h), type aliases are unconditionally defined:
```cpp
using dir_entry_view_impl = thrift_backend::dir_entry_view_impl;
using chunk_range = thrift_backend::chunk_range;
using inode_view_impl = thrift_backend::inode_view_impl;
```

But in [`include/dwarfs/reader/internal/metadata_types.h`](../include/dwarfs/reader/internal/metadata_types.h), classes are also defined:
```cpp
class dir_entry_view_impl { ... };  // CONFLICT!
class chunk_range { ... };           // CONFLICT!
class inode_view_impl { ... };       // CONFLICT!
```

## Architecture Analysis

### Current Design (Broken for Thrift-only)

```
metadata_types.h (public)
├── Type aliases (ALWAYS defined)
│   ├── using dir_entry_view_impl = thrift_backend::dir_entry_view_impl;
│   ├── using chunk_range = thrift_backend::chunk_range;
│   └── using inode_view_impl = thrift_backend::inode_view_impl;
└── CONFLICT with internal/metadata_types.h class definitions
```

### Correct Design (MECE + OOP)

**Principle**: Single-format builds should NOT use type aliases. Dual-format builds use aliases for polymorphism.

```
╔════════════════════════════════════════════════════╗
║         Format Configuration (CMake)                ║
╚════════════════════════════════════════════════════╝
         │
    ┌────┴────┬────────────┬─────────────┐
    │         │            │             │
    ▼         ▼            ▼             ▼
┌─────────────────┐  ┌──────────────┐  ┌──────────────┐
│ FlatBuffers-only│  │ Thrift-only  │  │ Dual-format  │
├─────────────────┤  ├──────────────┤  ├──────────────┤
│ NO type aliases │  │NO type aliases│  │YES type alias│
│ Direct access   │  │Direct access  │  │Polymorphism  │
│ to fb:: types   │  │to tb:: types  │  │via aliases   │
└─────────────────┘  └──────────────┘  └──────────────┘
```

## Solution Architecture

### Phase 1: Conditional Type Aliasing (2h)

**File**: [`include/dwarfs/reader/metadata_types.h`](../include/dwarfs/reader/metadata_types.h)

**Current** (lines 65-75):
```cpp
// WRONG: Always defines aliases
namespace dwarfs::reader {
using inode_view_impl = thrift_backend::inode_view_impl;
using dir_entry_view_impl = thrift_backend::dir_entry_view_impl;
using chunk_range = thrift_backend::chunk_range;
}
```

**Correct** (MECE):
```cpp
namespace dwarfs::reader {

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: Use type aliases for polymorphism
  using inode_view_impl = thrift_backend::inode_view_impl;
  using dir_entry_view_impl = thrift_backend::dir_entry_view_impl;
  using chunk_range = thrift_backend::chunk_range;

#elif defined(DWARFS_HAVE_FLATBUFFERS)
  // FlatBuffers-only: Use FlatBuffers types directly
  namespace fb = flatbuffers_backend;
  using inode_view_impl = fb::inode_view_impl;
  using dir_entry_view_impl = fb::dir_entry_view_impl;
  using chunk_range = fb::chunk_range;

#elif defined(DWARFS_HAVE_THRIFT)
  // Thrift-only: Use Thrift types directly
  namespace tb = thrift_backend;
  using inode_view_impl = tb::inode_view_impl;
  using dir_entry_view_impl = tb::dir_entry_view_impl;
  using chunk_range = tb::chunk_range;

#else
  #error "At least one metadata format must be enabled"
#endif

} // namespace dwarfs::reader
```

### Phase 2: Namespace-Qualified Access (1h)

**File**: [`include/dwarfs/reader/internal/metadata_v2.h`](../include/dwarfs/reader/internal/metadata_v2.h)

**Issue**: Methods use unqualified types that conflict with aliases

**Solution**: Use fully-qualified types in method signatures

**Before**:
```cpp
chunk_range get_chunks(int inode, std::error_code& ec) const;
```

**After** (lines 166-170):
```cpp
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: Return interface type
  chunk_range get_chunks(int inode, std::error_code& ec) const;
#else
  // Single-format: Return backend type directly
  auto get_chunks(int inode, std::error_code& ec) const 
    -> decltype(data_.get_chunks(inode, ec));
#endif
```

### Phase 3: Factory Pattern Refinement (1h)

**File**: [`src/reader/internal/metadata_v2_factory.cpp`](../src/reader/internal/metadata_v2_factory.cpp)

**Current**: Factory assumes dual-format

**Solution**: Factory should work for all three configurations

```cpp
std::unique_ptr<metadata_v2> 
create_metadata_v2(logger& lgr, std::shared_ptr<mmif> mm) {
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: Detect and route
  auto format = detect_format(mm);
  if (format == Format::FlatBuffers) {
    return std::make_unique<metadata_v2_flatbuffers>(lgr, mm);
  } else {
    return std::make_unique<metadata_v2_thrift>(lgr, mm);
  }
  
#elif defined(DWARFS_HAVE_FLATBUFFERS)
  // FlatBuffers-only
  return std::make_unique<metadata_v2_flatbuffers>(lgr, mm);
  
#elif defined(DWARFS_HAVE_THRIFT)
  // Thrift-only
  return std::make_unique<metadata_v2_thrift>(lgr, mm);
  
#endif
}
```

### Phase 4: CMake Validation (30min)

**File**: [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)

**Add validation**:
```cmake
# Ensure at least one format is enabled
if(NOT DWARFS_WITH_FLATBUFFERS AND NOT DWARFS_WITH_THRIFT)
  message(FATAL_ERROR "At least one metadata format must be enabled")
endif()

# Validate configuration
if(DWARFS_WITH_THRIFT AND NOT DWARFS_WITH_FLATBUFFERS)
  message(STATUS "Thrift-only build enabled")
  message(WARNING "Thrift-only builds are experimental")
endif()
```

### Phase 5: Test All Three Configurations (1h)

Build and validate:
```bash
# FlatBuffers-only
cmake -B build-fb-only -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
ninja -C build-fb-only mkdwarfs

# Thrift-only  
cmake -B build-tb-only -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
ninja -C build-tb-only mkdwarfs

# Dual-format
cmake -B build-dual -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
ninja -C build-dual mkdwarfs
```

## Implementation Plan

### Task Breakdown

| Phase | Task | Files | Time | Priority |
|-------|------|-------|------|----------|
| 1 | Conditional type aliasing | metadata_types.h | 2h | CRITICAL |
| 2 | Namespace-qualified access | metadata_v2.h, implementations | 1h | HIGH |
| 3 | Factory pattern refinement | metadata_v2_factory.cpp | 1h | HIGH |
| 4 | CMake validation | metadata_serialization.cmake | 30min | MEDIUM |
| 5 | Test all configurations | Build scripts | 1h | HIGH |

**Total Estimated Time**: 5.5 hours

## Success Criteria

- ✅ FlatBuffers-only: Compiles, mkdwarfs works
- ✅ Thrift-only: Compiles, mkdwarfs works (NEW!)
- ✅ Dual-format: Compiles, mkdwarfs works
- ✅ All three configurations tested
- ✅ No type aliasing conflicts
- ✅ Proper MECE separation of concerns

## Design Principles Applied

1. **MECE**: Each configuration is mutually exclusive, collectively exhaustive
2. **OOP**: Proper namespace separation, no mixing of concerns
3. **Single Responsibility**: Each configuration has its own type resolution strategy
4. **Open/Closed**: Easy to add new formats without modifying existing code
5. **DRY**: Type aliasing logic centralized in one header

## Risks & Mitigation

| Risk | Impact | Mitigation |
|------|--------|------------|
| Breaking existing code | HIGH | Test all configs thoroughly |
| Complex conditional logic | MEDIUM | Keep conditionals in headers only |
| Performance regression | LOW | Single-format uses direct types |

## Dependencies

**Blocked by**: None  
**Blocks**: Thrift-only production use

## Next Session Start

Read this plan, then:

1. Start with Phase 1 (conditional type aliasing)
2. Test FlatBuffers-only and dual-format don't break
3. Test Thrift-only compiles
4. Proceed with Phase 2-5 sequentially

---

**Document Version**: 1.0  
**Created**: 2025-11-29 11:06 HKT  
**Status**: Ready for implementation