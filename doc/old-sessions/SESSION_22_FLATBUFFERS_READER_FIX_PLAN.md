# Session 22: FlatBuffers Reader Architecture Fix

**Date**: 2025-12-22
**Status**: 🔴 **CRITICAL BUG** - FlatBuffers images broken due to incorrect converter
**Priority**: HIGH - Blocks FlatBuffers-only builds and all examples

## Problem Statement

The current architecture **incorrectly converts FlatBuffers → Thrift**:

```
dwarfsck detects FlatBuffers image
  ↓
Converts to internal Thrift format (WRONG!)
  ↓
Conversion fails: "data size mismatch for compact names: 18 != 486"
  ↓
Server returns 404 for all files (metadata corrupt)
```

**Root Cause**: [`src/reader/internal/metadata_types_thrift.cpp:561`](../src/reader/internal/metadata_types_thrift.cpp:561)

**Why This Is Wrong**:
- FlatBuffers images should use **FlatBuffers reader directly**
- Conversion to Thrift is **unnecessary** and **error-prone**
- Prevents FlatBuffers-only builds (always requires Thrift)
- Violates separation of concerns

## Correct Architecture

```
┌─────────────────────────────────────────────┐
│         Format Detection (Registry)         │
└──────────────┬──────────────────────────────┘
               │
       ┌───────┴────────┐
       ▼                ▼
┌──────────────┐  ┌──────────────┐
│ FlatBuffers  │  │   Thrift     │
│   Reader     │  │   Reader     │
│              │  │              │
│ Works on FB  │  │ Works on TFT │
│ directly     │  │ directly     │
└──────────────┘  └──────────────┘
       │                │
       └────────┬───────┘
                ▼
        Common Interface
```

## Implementation Plan

### Phase 1: Create FlatBuffers Reader (without Thrift dependency)

**Files to Create**:
- `src/reader/internal/metadata_types_flatbuffers.cpp` - Direct FlatBuffers reader
- `include/dwarfs/reader/internal/metadata_types_flatbuffers.h` - Header

**Implementation**:
- Implement `flatbuffers_backend::global_metadata` class
- Direct access to FlatBuffers tables (no conversion)
- Zero-copy access via `GetRoot<>()`
- Same interface as `thrift_backend::global_metadata`

### Phase 2: Fix Format Detection in filesystem_v2

**File**: [`src/reader/internal/metadata_v2.cpp`](../src/reader/internal/metadata_v2.cpp)

**Current (WRONG)**:
```cpp
if (detected FlatBuffers) {
  convert_to_thrift();  // BUG!
  use_thrift_reader();
}
```

**Correct**:
```cpp
if (detected FlatBuffers) {
  use_flatbuffers_reader();  // Direct!
} else {
  use_thrift_reader();
}
```

### Phase 3: Remove Broken Converter

**File**: [`src/metadata/converters/flatbuffers_to_thrift_converter.cpp`](../src/metadata/converters/flatbuffers_to_thrift_converter.cpp) (if exists)

**Action**: Delete or deprecate - no longer needed

### Phase 4: Update CMake

**File**: [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)

**Add**:
- Conditional compilation: `metadata_types_flatbuffers.cpp` only if `DWARFS_HAVE_FLATBUFFERS`
- Update `libdwarfs_reader` target

### Phase 5: Test FlatBuffers-Only Build

**Build Command**:
```bash
rm -rf build-fb-only
cmake -B build-fb-only -GNinja \
  -DDWARFS_WITH_THRIFT=OFF \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DWITH_TESTS=ON
ninja -C build-fb-only
ctest --test-dir build-fb-only
```

**Expected**: All tests pass, static-site-server works

### Phase 6: Fix string_table for FlatBuffers

**Issue**: Lines 561 in converter check compact_names size

**Root Cause**: FlatBuffers schema may have different string_table structure

**Fix**: Implement proper FlatBuffers string_table accessor in `flatbuffers_backend`

## Testing Strategy

1. **Unit Tests**: Test FlatBuffers reader directly
2. **Integration**: dwarfsck -l on FB images
3. **FUSE Mount**: Mount and browse FB images
4. **Example**: static-site-server with FB images

## Success Criteria

- ✅ FlatBuffers images work without Thrift
- ✅ No format conversion happens
- ✅ FlatBuffers-only builds succeed
- ✅ static-site-server serves files correctly
- ✅ All tests pass

## Timeline Estimate

- Phase 1-2: 2-3 hours (core reader implementation)
- Phase 3-4: 30 min (cleanup + CMake)
- Phase 5-6: 1 hour (testing + string table fix)
- **Total**: 3.5-4.5 hours

## Files to Modify

1. **New**: `src/reader/internal/metadata_types_flatbuffers.cpp` (~800 lines)
2. **New**: `include/dwarfs/reader/internal/metadata_types_flatbuffers.h` (~150 lines)
3. **Modify**: `src/reader/internal/metadata_v2.cpp` (format dispatch)
4. **Modify**: `cmake/metadata_serialization.cmake` (build config)
5. **Delete/Deprecate**: Broken converter files

## Dependencies

- FlatBuffers library (already fetched via FetchContent)
- No Thrift dependency for FlatBuffers reader
- Shared interface from `metadata_types_fwd.h`

## Risks

- **Low**: FlatBuffers schema is stable, zero-copy API is straightforward
- **Medium**: May need to handle packed vs unpacked differences
- **Low**: Testing coverage is comprehensive

## Next Session Start

Read this document, then immediately:
1. Create `metadata_types_flatbuffers.cpp` with direct FlatBuffers reader
2. Fix format dispatch in `metadata_v2.cpp`
3. Build and test

---

**Created**: 2025-12-22
**Last Updated**: 2025-12-22