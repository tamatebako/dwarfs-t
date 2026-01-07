# Session 31C Continuation Prompt

**Date**: 2025-12-22
**Status**: Ready to begin
**Previous**: Session 31B (Phase 1+2 complete)
**Deadline**: 3-4 hours

## Quick Context

Session 31B completed Phases 1+2 successfully:
- ✅ All 40 methods in `common_metadata_operations.cpp` (700 lines)
- ✅ Thrift adapter created (100 lines)
- ✅ FlatBuffers adapter created (100 lines)

**Total**: ~900 lines of new format-agnostic code

## Mission: Session 31C (3-4 hours)

Complete the remaining integration work:

### Phase 3: Wire Up Factory (30 min)

**File**: `src/reader/internal/metadata_v2_factory.cpp`

**Current State**: Factory uses old backend detection
**Target State**: Factory calls new adapters

**Implementation**:
```cpp
// In metadata_v2_factory.cpp

#ifdef DWARFS_HAVE_THRIFT
metadata_v2 make_metadata_v2_thrift(...);  // Forward declare
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
metadata_v2 make_metadata_v2_flatbuffers(...);  // Forward declare
#endif

metadata_v2 metadata_v2_factory::create(...) {
  // Detect format (existing logic)
  auto format = detect_format(data);
  
  if (format == SerializationFormat::FLATBUFFERS) {
#ifdef DWARFS_HAVE_FLATBUFFERS
    return make_metadata_v2_flatbuffers(lgr, data, options, ...);
#else
    DWARFS_THROW(runtime_error, "FlatBuffers support not compiled");
#endif
  }
  
  // Thrift (with/without schema)
#ifdef DWARFS_HAVE_THRIFT
  return make_metadata_v2_thrift(lgr, schema, data, options, ...);
#else
  DWARFS_THROW(runtime_error, "Thrift support not compiled");
#endif
}
```

### Phase 4: Update Build System (30 min)

**File**: `cmake/libdwarfs.cmake`

**Changes Needed**:

1. Add new source files to `dwarfs_reader`:
```cmake
# In dwarfs_reader sources
src/reader/internal/common_metadata_operations.cpp
src/reader/internal/domain_metadata_views.cpp  # From Session 31A
```

2. Conditionally add adapters:
```cmake
if(DWARFS_WITH_THRIFT)
  target_sources(dwarfs_reader PRIVATE
    src/reader/internal/thrift_metadata_adapter.cpp
  )
endif()

if(DWARFS_WITH_FLATBUFFERS)
  target_sources(dwarfs_reader PRIVATE
    src/reader/internal/flatbuffers_metadata_adapter.cpp
  )
endif()
```

3. Link domain converters (Session 28):
```cmake
target_link_libraries(dwarfs_reader
  PRIVATE
    dwarfs_metadata_domain_converters  # If available
)
```

### Phase 5: Test All Configurations (2-3 hours)

**Three Test Configurations**:

#### 5.1 FlatBuffers-Only Build (45 min)
```bash
rm -rf build-fb-only
cmake -B build-fb-only -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
ninja -C build-fb-only
ctest --test-dir build-fb-only --output-on-failure
```

**Expected**:
- ✅ Builds successfully
- ✅ All tests pass (FlatBuffers images only)
- ✅ Can create .dff images
- ✅ Can read .dff images
- ❌ Cannot read .dft images (expected)

#### 5.2 Thrift-Only Build (45 min)
```bash
rm -rf build-thrift-only
cmake -B build-thrift-only -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
ninja -C build-thrift-only
ctest --test-dir build-thrift-only --output-on-failure
```

**Expected**:
- ❌ Build FAILS (FlatBuffers required for common operations)
- This is CORRECT behavior post-migration

**Why**: Common operations require domain model, which needs converters. FlatBuffers is now the base format.

#### 5.3 Dual-Format Build (45 min)
```bash
rm -rf build-both
cmake -B build-both -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
ninja -C build-both
ctest --test-dir build-both --output-on-failure
```

**Expected**:
- ✅ Builds successfully
- ✅ All tests pass
- ✅ Can create both .dff and .dft images
- ✅ Can read both .dff and .dft images
- ✅ Cross-format compatibility

#### 5.4 Integration Testing (30 min)

**Test Scenarios**:
1. Create FlatBuffers image, extract, compare
2. Create Thrift image (if dual), extract, compare
3. Read legacy Thrift images (backward compat)
4. Verify metadata correctness (checksums, sizes)
5. FUSE mount test (basic operations)

### Critical Blockers to Resolve

#### Blocker 1: Session 28 Converters Not Linked

**Problem**: Adapters expect `metadata::converters::from_thrift()` and `from_flatbuffers()`

**Solutions**:
1. **Best**: Link Session 28 converters (if available)
2. **Temporary**: Implement minimal converters inline
3. **Workaround**: Continue using old backends until converters ready

**Check**: Does `include/dwarfs/metadata/converters/` exist?

#### Blocker 2: Domain View Completeness

**Problem**: Some domain view methods may be incomplete

**Check Points**:
- Compact string tables
- Legacy v2.2 compatibility
- Path building in dir_entry_view
- Parent traversal

**Solution**: Fix issues as tests reveal them

#### Blocker 3: Inode Offset Calculations

**Problem**: Multiple offset calculations (symlink, file, dev) may be incorrect

**Check**: Verify against Thrift backend logic in `metadata_v2_thrift.cpp:724-730`

**Fix**: Refactor offset calculations into helper functions

## Success Criteria for Session 31C

- ✅ Factory wired up correctly
- ✅ Build system updated
- ✅ FlatBuffers-only build works
- ✅ Dual-format build works
- ✅ Thrift-only build FAILS appropriately (with clear message)
- ✅ Basic integration tests pass
- ✅ Can create and mount images
- ✅ Backward compatibility with legacy images

## Files to Modify/Check

**To Modify**:
- `src/reader/internal/metadata_v2_factory.cpp` (wire up adapters)
- `cmake/libdwarfs.cmake` (add new sources)

**To Check**:
- `include/dwarfs/metadata/converters/` (Session 28 converters)
- `src/reader/internal/common_metadata_operations.cpp` (fix issues found)
- `src/reader/internal/domain_metadata_views.cpp` (fix issues found)

**Reference**:
- `src/reader/internal/metadata_v2_thrift.cpp:299-2400` (old Thrift backend logic)
- `doc/SESSION_31_ARCHITECTURE_CORRECT.md` (architecture reference)

## Timeline

| Phase | Task | Duration |
|-------|------|----------|
| 3 | Wire up factory | 30 min |
| 4 | Update build system | 30 min |
| 5.1 | Test FlatBuffers-only | 45 min |
| 5.2 | Test Thrift-only | 45 min |
| 5.3 | Test dual-format | 45 min |
| 5.4 | Integration testing | 30 min |
| **Total** | | **3-4 hours** |

## Quick Start Commands

```bash
# Step 1: Wire up factory
code src/reader/internal/metadata_v2_factory.cpp

# Step 2: Update build
code cmake/libdwarfs.cmake

# Step 3: Test FlatBuffers-only
rm -rf build-fb-only
cmake -B build-fb-only -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
ninja -C build-fb-only
ctest --test-dir build-fb-only

# Step 4: Test dual-format
rm -rf build-both
cmake -B build-both -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
ninja -C build-both
ctest --test-dir build-both
```

## Notes

- **Phase 3+4 are quick** (1 hour total)
- **Phase 5 is thorough testing** (2-3 hours)
- Be prepared to fix issues discovered during testing
- Converters from Session 28 are critical - check availability first

---

**Ready to begin Session 31C!**