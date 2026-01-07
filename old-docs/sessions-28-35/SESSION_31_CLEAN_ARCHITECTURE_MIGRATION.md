# Session 31: Clean OOP Architecture Migration

**Date**: 2025-12-22+
**Previous**: Session 30 - Architecture validated
**Goal**: Complete migration to clean OOP architecture
**Approach**: Replace backends, unwire old code, delete when done
**Timeline**: 12-15 hours (aggressive but achievable)

## Mission: Full OOP Replacement

Transform the backends from 12,000 lines of mixed concerns into clean, focused implementations using our validated OOP interfaces.

**No "new" or "oop" naming** - everything looks native from day one.

## Current State (Session 30 Findings)

### What Exists (Sessions 27-28)
✅ Clean OOP interfaces:
- `metadata_reader_interface.h` (381 lines) - Domain-based reader
- `metadata_writer_interface.h` (168 lines) - Domain-based writer
- Domain ↔ Thrift converter (789 lines)
- Domain ↔ FlatBuffers converter (946 lines)

### What Needs Replacement
❌ Mixed backend implementations:
- `metadata_v2_thrift.cpp` (2,470 lines) - Delete after replacement
- `metadata_v2_flatbuffers.cpp` (2,516 lines) - Delete after replacement
- `metadata_types_thrift.cpp` (1,151 lines) - Delete after replacement
- `metadata_types_flatbuffers.cpp` (1,151 lines) - Delete after replacement

**Total to delete**: ~7,300 lines of old mixed-concern code

## Architecture Transformation

### Before (Current - Mixed Concerns)
```
metadata_v2 (Facade)
└── unique_ptr<impl>
    ├── thrift_backend::metadata_<LoggerPolicy> (2,470 lines)
    │   └── metadata_v2_data (complex mixed logic)
    └── flatbuffers_backend::metadata_<LoggerPolicy> (2,516 lines)
        └── metadata_v2_data (complex mixed logic)
```

### After (Clean OOP)
```
metadata_v2 (Facade)
└── unique_ptr<impl>
    ├── thrift_metadata (clean adapter)
    │   ├── reader: metadata_reader_interface
    │   ├── writer: metadata_writer_interface
    │   └── converter: domain_thrift_converter
    └── flatbuffers_metadata (clean adapter)
        ├── reader: metadata_reader_interface
        ├── writer: metadata_writer_interface
        └── converter: domain_flatbuffers_converter
```

## Phase 1: Create Clean Backend Adapters (4-5 hours)

### Step 1.1: Thrift Backend Adapter (2-2.5h)
File: `src/reader/internal/thrift_metadata.cpp` (NEW)

**Goal**: Thin adapter that delegates to OOP interfaces

```cpp
namespace dwarfs::reader::internal {
namespace {

class thrift_metadata final : public metadata_v2::impl {
 public:
  thrift_metadata(logger& lgr, std::span<uint8_t const> schema,
                  std::span<uint8_t const> data,
                  metadata_options const& options, int inode_offset,
                  bool force_consistency_check,
                  std::shared_ptr<performance_monitor const> const& perfmon)
      : reader_{metadata_reader_factory::create_thrift(
            lgr, schema, data, options, inode_offset, 
            force_consistency_check, perfmon)} {}

  // Delegate ALL methods to reader_
  chunk_range get_chunks(int inode, std::error_code& ec) const override {
    return reader_->get_chunks(inode, ec);
  }
  
  dir_entry_view root() const override {
    return reader_->root();
  }
  
  // ... all other methods delegate similarly

 private:
  std::unique_ptr<metadata_reader_interface> reader_;
};

} // namespace

// Factory function for dual-format builds
metadata_v2 make_metadata_v2_thrift(/*...*/) {
  metadata_v2 result;
  result.impl_ = std::make_unique<thrift_metadata>(/*...*/);
  return result;
}

} // namespace dwarfs::reader::internal
```

**Size**: ~300-400 lines (vs 2,470 old)

### Step 1.2: FlatBuffers Backend Adapter (2-2.5h)
File: `src/reader/internal/flatbuffers_metadata.cpp` (NEW)

Same pattern as Thrift:
```cpp
class flatbuffers_metadata final : public metadata_v2::impl {
  std::unique_ptr<metadata_reader_interface> reader_;
  // Delegate all methods
};
```

**Size**: ~300-400 lines (vs 2,516 old)

## Phase 2: Update Factory (1 hour)

### Step 2.1: Update Factory Includes
File: `src/reader/internal/metadata_v2_factory.cpp`

```cpp
// OLD includes - REMOVE
// #include "metadata_v2_thrift.cpp"  
// #include "metadata_v2_flatbuffers.cpp"

// NEW includes - ADD
#include "thrift_metadata.cpp"
#include "flatbuffers_metadata.cpp"
```

### Step 2.2: Update Factory Calls
No changes needed - factory already calls:
- `make_metadata_v2_thrift()` 
- `make_metadata_v2_flatbuffers()`

These now call our clean implementations.

## Phase 3: Update Build System (30 minutes)

### Step 3.1: Update CMakeLists
File: `cmake/libdwarfs.cmake`

```cmake
# OLD sources - REMOVE
# src/reader/internal/metadata_v2_thrift.cpp
# src/reader/internal/metadata_v2_flatbuffers.cpp
# src/reader/internal/metadata_types_thrift.cpp
# src/reader/internal/metadata_types_flatbuffers.cpp

# NEW sources - ADD
src/reader/internal/thrift_metadata.cpp
src/reader/internal/flatbuffers_metadata.cpp
```

## Phase 4: Test All Configurations (2-3 hours)

### Test Matrix
1. **FlatBuffers-only** (`DWARFS_WITH_THRIFT=OFF`)
   - Build: `cmake -B build -DDWARFS_WITH_THRIFT=OFF`
   - Test: `ctest --test-dir build`

2. **Thrift-only** (`DWARFS_WITH_FLATBUFFERS=OFF`)
   - Build: `cmake -B build -DDWARFS_WITH_FLATBUFFERS=OFF`
   - Test: `ctest --test-dir build`

3. **Both formats** (dual-format)
   - Build: `cmake -B build`
   - Test: `ctest --test-dir build`

### Validation Criteria
- ✅ All unit tests pass
- ✅ Integration tests pass
- ✅ Metadata round-trip tests pass
- ✅ Performance within 5% of baseline

## Phase 5: Unwire Old Code (1 hour)

### Step 5.1: Remove Old Backend Files
```bash
# Move to backup first (safety)
mkdir -p .backup/session-31
mv src/reader/internal/metadata_v2_thrift.cpp .backup/session-31/
mv src/reader/internal/metadata_v2_flatbuffers.cpp .backup/session-31/
mv src/reader/internal/metadata_types_thrift.cpp .backup/session-31/
mv src/reader/internal/metadata_types_flatbuffers.cpp .backup/session-31/
```

### Step 5.2: Update Includes
Find and remove any includes of old files:
```bash
grep -r "metadata_v2_thrift.cpp" include/ src/ --include="*.h" --include="*.cpp"
grep -r "metadata_v2_flatbuffers.cpp" include/ src/ --include="*.h" --include="*.cpp"
```

## Phase 6: Delete Old Code (30 minutes)

### Step 6.1: Build & Test Final
Ensure everything works without old files:
```bash
rm -rf build/
cmake -B build -DWITH_TESTS=ON
cmake --build build
ctest --test-dir build
```

### Step 6.2: Delete Backups
Only after all tests pass:
```bash
rm -rf .backup/session-31/
```

### Step 6.3: Commit Clean Architecture
```bash
git add -A
git commit -m "refactor(metadata): migrate to clean OOP architecture

- Replace 7,300 lines of mixed-concern backend code
- Use domain-based reader/writer interfaces
- Reduce backend implementations to ~600 lines total
- Maintain all functionality and test coverage
- Delete old backend files

This completes the OOP architecture migration started in Sessions 27-28."
```

## Code Reduction Summary

| Component | Before | After | Reduction |
|-----------|--------|-------|-----------|
| Thrift backend | 2,470 lines | ~300 lines | **88%** |
| FlatBuffers backend | 2,516 lines | ~300 lines | **88%** |
| Thrift types | 1,151 lines | 0 (deleted) | **100%** |
| FlatBuffers types | 1,151 lines | 0 (deleted) | **100%** |
| **Total** | **7,288 lines** | **~600 lines** | **92%** |

## Success Criteria

### Functional
- ✅ All 3 build configs compile
- ✅ All tests pass (unit, integration, metadata)
- ✅ Can create DwarFS images (both formats)
- ✅ Can mount DwarFS images (both formats)
- ✅ Can extract DwarFS images (both formats)

### Architectural
- ✅ Clean separation of concerns
- ✅ Domain model used throughout
- ✅ No backend-specific types in public interfaces
- ✅ No "new" or "oop" in any names
- ✅ Code looks native and purpose-built

### Performance
- ✅ Compression speed within 5% of baseline
- ✅ Extraction speed within 5% of baseline
- ✅ Mount latency within 5% of baseline

## Risk Mitigation

### Safety Measures
1. **Backup first**: Move old files to `.backup/` before deleting
2. **Test continuously**: Run tests after each phase
3. **Git commits**: Commit after each working phase
4. **Reversible**: Can restore from backup if issues arise

### Rollback Plan
If Phase 4 tests fail:
1. Restore old files from `.backup/session-31/`
2. Revert CMakeLists changes
3. Rebuild and verify old code works
4. Analyze failures, fix clean implementation
5. Try again

## Timeline Breakdown

| Phase | Task | Duration |
|-------|------|----------|
| 1.1 | Thrift backend adapter | 2-2.5h |
| 1.2 | FlatBuffers backend adapter | 2-2.5h |
| 2 | Update factory | 1h |
| 3 | Update build system | 0.5h |
| 4 | Test all configs | 2-3h |
| 5 | Unwire old code | 1h |
| 6 | Delete old code | 0.5h |
| **Total** | | **12-15h** |

## File Changes Summary

### Files to Create
1. `src/reader/internal/thrift_metadata.cpp` (~300 lines)
2. `src/reader/internal/flatbuffers_metadata.cpp` (~300 lines)

### Files to Modify
1. `src/reader/internal/metadata_v2_factory.cpp` (update includes)
2. `cmake/libdwarfs.cmake` (update source list)

### Files to Delete (after validation)
1. `src/reader/internal/metadata_v2_thrift.cpp` (2,470 lines)
2. `src/reader/internal/metadata_v2_flatbuffers.cpp` (2,516 lines)
3. `src/reader/internal/metadata_types_thrift.cpp` (1,151 lines)
4. `src/reader/internal/metadata_types_flatbuffers.cpp` (1,151 lines)

**Net change**: -6,688 lines (92% reduction)

## Post-Migration Benefits

### Code Quality
- ✅ **92% less code** to maintain
- ✅ **Clean separation** of concerns
- ✅ **Domain-driven** design throughout
- ✅ **Testable** components

### Extensibility
- ✅ Easy to add new metadata formats
- ✅ Easy to test format conversions
- ✅ Easy to swap serialization libraries
- ✅ Easy to optimize individual layers

### Developer Experience
- ✅ Clear, focused implementations
- ✅ Native-looking code (no "oop" or "new")
- ✅ Predictable patterns
- ✅ Easy to navigate

---

**Recommended Approach**: Execute phases sequentially, test after each, commit working states.

**Start Command**: Begin with Phase 1.1 (Thrift backend adapter)

**Last Updated**: 2025-12-22
**Status**: Ready to execute
**Risk Level**: MEDIUM (mitigated by backups and testing)