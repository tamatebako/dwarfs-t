# Session 24: OOP Architecture Fix for Duplicate Symbols

**Date**: 2025-12-22
**Status**: 🔴 **CRITICAL** - Session 22 has architectural flaw blocking all progress
**Estimated Duration**: 6-8 hours

## Problem Analysis

### Root Cause: Violation of Single Responsibility Principle

Session 22 created **two identical classes** in the same namespace:

```cpp
// metadata_v2_thrift.cpp line 298
namespace dwarfs::reader::internal {
  class metadata_v2_data { ... };  // 2439 lines
}

// metadata_v2_flatbuffers.cpp line 311
namespace dwarfs::reader::internal {
  class metadata_v2_data { ... };  // 2485 lines - DUPLICATE!
}
```

**Result**: 20 duplicate symbols, link failure, ODR violation.

### Why This Violates OOP Principles

1. **No Separation of Concerns**: Same class name for different responsibilities
2. **No Open/Closed**: Cannot extend without modifying both files
3. **Not MECE**: Overlapping definitions, not exhaustive separation
4. **Code Duplication**: 95% identical code in two places

## Proper OOP Solution: Strategy Pattern with Bridge Pattern

### Architecture: Three-Layer Separation

```
┌─────────────────────────────────────────────────────┐
│          Abstract Interface (metadata_v2::impl)      │
│                                                      │
│  Virtual methods for all operations                 │
│  - find(), walk(), getattr(), etc.                  │
└──────────────────┬──────────────────────────────────┘
                   │
         ┌─────────┴─────────┐
         │ implements        │ implements
         ▼                   ▼
┌────────────────┐    ┌────────────────┐
│ Thrift Backend │    │FlatBuf Backend │
│   (internal)   │    │   (internal)   │
└────────────────┘    └────────────────┘
         │                   │
         ▼                   ▼
  Anonymous namespace   Anonymous namespace
  (translation-unit    (translation-unit
   local class)         local class)
```

### Key OOP Principles Applied

1. **Strategy Pattern**: Factory chooses backend based on format
2. **Bridge Pattern**: Public interface bridges to internal implementation
3. **Encapsulation**: Internal classes hidden in anonymous namespaces
4. **Single Responsibility**: Each backend handles ONE format only
5. **Open/Closed**: Adding new format = new file, no changes to existing

## Implementation Plan

### Phase A: Isolate Thrift Backend (2 hours)

**Objective**: Make Thrift backend's `metadata_v2_data` translation-unit-local

**Files**:
- `src/reader/internal/metadata_v2_thrift.cpp`

**Changes**:
1. Wrap `metadata_v2_data` and `metadata_` classes in `namespace { }`
2. Keep factory function `make_metadata_v2_thrift()` in outer namespace
3. Keep `metadata_v2_utils` in outer namespace (Thrift-specific API)

**Example**:
```cpp
namespace dwarfs::reader::internal {

namespace {  // Anonymous - translation-unit local

class metadata_v2_data {
  // All implementation here
};

template <typename LoggerPolicy>
class metadata_ final : public metadata_v2::impl {
  // Wraps metadata_v2_data
  metadata_v2_data const data_;
};

} // anonymous namespace

// Factory function - MUST be in outer namespace
metadata_v2 make_metadata_v2_thrift(...) {
  metadata_v2 result;
  result.impl_ = make_unique_logging_object<metadata_v2::impl, metadata_, ...>(...);
  return result;
}

} // namespace dwarfs::reader::internal
```

### Phase B: Isolate FlatBuffers Backend (2 hours)

**Objective**: Make FlatBuffers backend's `metadata_v2_data` translation-unit-local

**Files**:
- `src/reader/internal/metadata_v2_flatbuffers.cpp`

**Changes**: Same pattern as Thrift backend

### Phase C: Verify Compilation (30 min)

**Objective**: Confirm no duplicate symbols

```bash
cd build
ninja -j4 dwarfsck dwarfsextract mkdwarfs
```

**Expected**: Clean compilation, no duplicate symbol errors

### Phase D: Test FlatBuffers Images (1 hour)

**Objective**: Verify FlatBuffers images work with direct backend

**Tests**:
1. `dwarfsck -l example/static-site-server/aesop.dff`
2. `build/static-site-server/static-site-server --image aesop.dff`
3. Mount and browse FlatBuffers image

**Success Criteria**:
- ✅ No "converting to Thrift" message
- ✅ Files list correctly
- ✅ static-site-server serves content
- ✅ Zero-copy performance

### Phase E: Refactor to Component Classes (3-4 hours)

**Objective**: Break large backend files into OOP components

**Pattern**: Extract responsibilities into separate classes

**Thrift Backend Components**:
1. `thrift_consistency_checker` - Validation logic
2. `thrift_directory_navigator` - find(), readdir(), walk()
3. `thrift_file_operations` - get_chunks(), seek(), file_size()
4. `thrift_metadata_formatter` - as_json(), dump(), serialize()
5. `thrift_cache_builder` - build_nlinks(), unpack_chunk_table()

**FlatBuffers Backend Components**: Mirror structure

**Each component**:
- Single responsibility
- <400 lines
- Independently testable
- Clear interface

### Phase F: Update Documentation (1 hour)

**Files**:
- `.kilocode/rules/memory-bank/architecture.md`
- `.kilocode/rules/memory-bank/context.md`
- `doc/old-docs/session-22/` - Archive Session 22 docs
- `doc/old-docs/session-23/` - Archive this session's docs

## OOP Design Patterns Applied

### 1. Strategy Pattern
**Intent**: Define family of algorithms (Thrift/FlatBuffers backends), encapsulate each, make them interchangeable.

**Implementation**:
- `metadata_v2::impl` = Strategy interface
- `metadata_<LoggerPolicy>` (Thrift) = Concrete strategy A
- `metadata_<LoggerPolicy>` (FlatBuffers) = Concrete strategy B
- Factory = Context that selects strategy

### 2. Bridge Pattern
**Intent**: Decouple abstraction from implementation

**Implementation**:
- `metadata_v2` = Abstraction
- `metadata_v2::impl` = Implementor interface
- Anonymous namespace classes = Concrete implementors

### 3. Template Method Pattern
**Intent**: Define skeleton, let subclasses override steps

**Implementation**: Each backend implements virtual methods in its own way

### 4. Facade Pattern
**Intent**: Provide unified interface to complex subsystem

**Implementation**: `metadata_v2_data` facades over:
- Metadata access (`global_metadata`)
- Cache management (`packed_int_vector`)
- String tables
- Directory traversal

## Benefits of Proper OOP Architecture

1. **No Code Duplication**: Each backend in ONE file only
2. **Clear Separation**: Anonymous namespaces enforce translation-unit locality
3. **Type Safety**: Compiler enforces proper backend selection
4. **Extensibility**: Add backend = new file + factory registration
5. **Testability**: Each backend independently testable
6. **Maintainability**: Changes localized to single backend

## Anti-Patterns Avoided

❌ **God Class**: 2400+ line classes doing everything
❌ **Copy-Paste Programming**: Duplicating entire class
❌ **Tight Coupling**: Same class name in global namespace
❌ **Feature Envy**: Backends accessing each other's internals

## Estimated Timeline

| Phase | Duration | Cumulative |
|-------|----------|------------|
| A: Isolate Thrift | 2 hours | 2 hours |
| B: Isolate FlatBuffers | 2 hours | 4 hours |
| C: Verify compilation | 30 min | 4.5 hours |
| D: Test images | 1 hour | 5.5 hours |
| E: Component refactoring | 3-4 hours | 8.5-9.5 hours |
| F: Documentation | 1 hour | 9.5-10.5 hours |

**Total**: 9.5-10.5 hours (compressed from original 2-week estimate)

## Success Criteria

### Phase A-B (Isolation)
- ✅ Clean compilation
- ✅ No duplicate symbols
- ✅ Each backend self-contained

### Phase D (Testing)
- ✅ FlatBuffers images work
- ✅ Thrift images still work
- ✅ Factory dispatch correct
- ✅ Zero-copy performance

### Phase E (Refactoring)
- ✅ All files <800 lines
- ✅ Each class single responsibility
- ✅ Component boundaries clear
- ✅ No code duplication

### Phase F (Documentation)
- ✅ Architecture documented
- ✅ Session docs archived
- ✅ Memory bank updated

## Next Session Start Instructions

1. **Read this plan completely**
2. **Execute Phase A**: Wrap Thrift backend classes in `namespace { }`
3. **Execute Phase B**: Wrap FlatBuffers backend classes in `namespace { }`
4. **Build and test**: Verify no duplicate symbols
5. **Test FlatBuffers images**: Confirm they work
6. **Only then proceed to Phase E** (component refactoring)

---

**Created**: 2025-12-22
**Session**: 24 (Session 23 blocked by this issue)
**Priority**: CRITICAL - Blocks all other work