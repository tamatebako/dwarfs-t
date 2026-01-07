// ... existing code ...

# Session 31: Correct Architecture - Domain-Based Metadata

**Date**: 2025-12-22
**Status**: Architecture Validated, Ready for Implementation

## The Correct Understanding

### On-Disk Formats (Unchanged)
- **Thrift images**: Remain as Frozen Thrift format (backward compatible, cannot change)
- **FlatBuffers images**: Remain as FlatBuffers format

### Runtime Architecture (What We're Migrating)

#### Current State (4,986 lines of duplicate logic):
```
┌───────────────────────────┐  ┌───────────────────────────┐
│ thrift_backend::          │  │ flatbuffers_backend::     │
│ metadata_v2_data          │  │ metadata_v2_data          │
│                           │  │                           │
│ 2,470 lines               │  │ 2,516 lines               │
│                           │  │                           │
│ Frozen Thrift → ops       │  │ FlatBuffers → ops         │
│ All 40 filesystem methods │  │ All 40 filesystem methods │
│ Format-specific access    │  │ Format-specific access    │
└───────────────────────────┘  └───────────────────────────┘
```

**Problem**: Both backends implement the SAME 40 filesystem operations, just accessing different serialization formats. This is massive code duplication.

#### Target State (~700 lines of shared logic):
```
              ┌─────────────────────────────────┐
              │ common_metadata_operations      │
              │ (domain-based)                  │
              │                                 │
              │ ~500-700 lines                  │
              │                                 │
              │ Implements all 40 methods of    │
              │ metadata_v2::impl interface     │
              │                                 │
              │ Works ONLY with domain model:   │
              │ metadata::domain::metadata      │
              └──────────────┬──────────────────┘
                             │
                  ┌──────────┴─────────┐
                  ▼                    ▼
         ┌────────────────┐    ┌────────────────┐
         │ Thrift         │    │ FlatBuffers    │
         │ Deserializer   │    │ Deserializer   │
         │                │    │                │
         │ ~100 lines     │    │ ~100 lines     │
         │                │    │                │
         │ Load Frozen    │    │ Load FlatBuf   │
         │ Convert→Domain │    │ Convert→Domain │
         └────────────────┘    └────────────────┘
```

**Solution**:
1. Both formats deserialize to `metadata::domain::metadata` at load time
2. All filesystem operations work on domain model (single shared implementation)
3. Eliminates 4,986 lines of format-specific duplicate code
4. Results in ~80-86% code reduction

## Key Architectural Principles

### 1. Domain Model as Single Source of Truth
- `metadata::domain::metadata` is the runtime representation
- No direct access to frozen Thrift or FlatBuffers after deserialization
- All operations work with C++ native types

### 2. Separation of Concerns
- **Deserialization**: Format-specific, happens once at load
- **Filesystem Operations**: Format-agnostic, works on domain model
- **Serialization (writing)**: Format-specific, uses converters

### 3. No Format Hotswapping
- Images stay in their original format on disk
- We're NOT making Thrift/FlatBuffers interchangeable at the image level
- We ARE making both formats usable through a common runtime interface

## What Gets Deleted

### Files to Remove (7,288 lines total):
1. `src/reader/internal/metadata_v2_thrift.cpp` (2,470 lines)
   - Lines 299-2400: `metadata_v2_data` class

2. `src/reader/internal/metadata_v2_flatbuffers.cpp` (2,516 lines)
   - Similar duplicate implementation

3. `src/reader/internal/metadata_types_thrift.cpp` (1,151 lines)
   - Backend-specific type wrappers

4. `src/reader/internal/metadata_types_flatbuffers.cpp` (1,151 lines)
   - Backend-specific type wrappers

### Files to Keep (with modifications):
- `src/reader/internal/metadata_v2_thrift.cpp` (lines 1704-1902)
  - Keep `class metadata_` thin adapter (~200 lines)
  - Modify to delegate to common operations

## What Gets Created

### New Files (~700-900 lines total):

1. **`src/reader/internal/common_metadata_operations.h`** (~150 lines)
   - Class definition for domain-based filesystem operations
   - Implements `metadata_v2::impl` interface

2. **`src/reader/internal/common_metadata_operations.cpp`** (~500-700 lines)
   - All 40 filesystem methods working on domain model
   - Single implementation, no format-specific code

3. **`src/reader/internal/thrift_metadata_adapter.cpp`** (~100 lines)
   - Deserialize Frozen Thrift → domain model
   - Wrap common operations

4. **`src/reader/internal/flatbuffers_metadata_adapter.cpp`** (~100 lines)
   - Deserialize FlatBuffers → domain model
   - Wrap common operations

## Code Reduction Calculation

**Before**:
- Thrift backend: 2,470 lines (operations)
- FlatBuffers backend: 2,516 lines (operations)
- Thrift types: 1,151 lines
- FlatBuffers types: 1,151 lines
- **Total**: 7,288 lines

**After**:
- Common operations: 650 lines (filesystem ops)
- Thrift adapter: 100 lines (deserialize only)
- FlatBuffers adapter: 100 lines (deserialize only)
- Thin wrapper (existing): 200 lines
- **Total**: ~1,050 lines

**Reduction**: 7,288 → 1,050 = **85.6% reduction** (6,238 lines deleted)

## Migration Strategy

### Phase 1: Create Common Operations (~500-700 lines)
**File**: `src/reader/internal/common_metadata_operations.cpp`

Implement all 40 methods of `metadata_v2::impl` working on:
```cpp
class common_metadata_operations : public metadata_v2::impl {
  metadata::domain::metadata domain_meta_;  // Stored at construction

  // All operations work on domain_meta_ directly
  dir_entry_view root() const override;
  std::optional<dir_entry_view> find(std::string_view path) const override;
  // ... 38 more methods
};
```

### Phase 2: Create Format Adapters (~200 lines)

**Thrift Adapter** (`thrift_metadata_adapter.cpp`):
```cpp
metadata_v2 make_metadata_v2_thrift(
    logger& lgr, std::span<uint8_t const> schema,
    std::span<uint8_t const> data, ...) {

  // 1. Deserialize Frozen Thrift
  auto frozen = map_frozen<thrift::metadata::metadata>(schema, data);

  // 2. Convert to domain using existing converter
  auto domain = metadata::converters::from_thrift(frozen.thaw());

  // 3. Create common operations with domain model
  metadata_v2 result;
  result.impl_ = std::make_unique<common_metadata_operations>(
      std::move(domain), options, inode_offset, ...);
  return result;
}
```

**FlatBuffers Adapter**: Similar pattern.

### Phase 3: Wire Up Factory
Update `metadata_v2_factory.cpp` to call new adapters.

### Phase 4: Update Build System
- Remove old backend files from CMakeLists
- Add new common operations + adapters

### Phase 5: Testing
- FlatBuffers-only build
- Thrift-only build
- Dual-format build
- All tests must pass

### Phase 6: Delete Old Code
After all tests pass, delete 7,288 lines of old backend code.

## Success Criteria

### Functional
- ✅ All 3 build configs compile
- ✅ All existing tests pass
- ✅ Can create DwarFS images (both formats)
- ✅ Can mount DwarFS images (both formats)
- ✅ Can extract DwarFS images (both formats)

### Architectural
- ✅ Clean separation: deserialization vs operations
- ✅ Domain model used throughout operations
- ✅ No format-specific types in common operations
- ✅ Single implementation of each filesystem method

### Performance
- ✅ Compression speed within 5% of baseline
- ✅ Extraction speed within 5% of baseline
- ✅ Mount latency within 5% of baseline

## Timeline

| Phase | Task | Duration |
|-------|------|----------|
| 1 | Create common_metadata_operations.cpp | 4-5h |
| 2 | Create format adapters (both) | 2-3h |
| 3 | Wire up factory | 1h |
| 4 | Update build system | 0.5h |
| 5 | Test all configurations | 2-3h |
| 6 | Delete old code & commit | 1h |
| **Total** | | **11-13.5h** |

## Critical Notes

1. **Thrift images are frozen** - We cannot change the on-disk format, only how we process it internally
2. **Domain model is the key** - All operations MUST work on `metadata::domain::metadata`, not on frozen types
3. **Adapters are thin** - They ONLY deserialize, nothing else
4. **No hotswapping** - We're not making formats interchangeable at the image level
5. **Massive deduplication** - The win comes from eliminating duplicate filesystem operation implementations

---

**This document is the authoritative architecture reference for Session 31 and beyond.**

// ... existing code ...