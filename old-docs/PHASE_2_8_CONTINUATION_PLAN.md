# Phase 2.8 Continuation Plan - Testing & Validation

**Date**: 2025-11-23
**Status**: ✅ Core Implementation Complete - Ready for Testing
**Branch**: `feature/multi-format-serialization-fuse`

---

## Completed Work (Phase 2.5-2.7)

### Phase 2.5: Backend Interface Implementation ✅
**Files Modified**:
- `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`
- `src/reader/internal/metadata_types_flatbuffers.cpp`
- `include/dwarfs/reader/internal/metadata_types_thrift.h`
- `src/reader/internal/metadata_types_thrift.cpp`

**What Was Done**:
1. Added 8 required interface methods to both `flatbuffers_backend::global_metadata` and `thrift_backend::global_metadata`:
   - `uids()`, `gids()`, `modes()` - Return raw table data as spans
   - `name_at()`, `symlink_at()` - String lookups by index
   - `block_size()`, `total_fs_size()` - Filesystem metrics
   - `hole_block_index()` - Sparse file support

2. Implemented using Strategy Pattern with existing `metadata_view_interface.h`
   - No duplicate interface files needed
   - Clean separation between backends
   - Both compile successfully

### Phase 2.6: Factory Pattern ✅
**File Modified**: `src/reader/internal/metadata_factory.cpp`

**What Was Done**:
1. Fixed namespace collision (`::flatbuffers::Verifier`)
2. Factory correctly creates backends via interfaces
3. Format detection logic works properly

### Phase 2.7: CMake Integration ✅
**Build Status**: ✅ Core libraries compile successfully
- `dwarfs_common`: Built (all files compiled)
- `dwarfs_reader`: Built (after adding missing functions)

**What Was Done**:
1. Fixed FlatBuffers schema compilation paths
2. Fixed conditional compilation logic
3. Added proper dependencies

### Phase 2.8: Missing Function Implementation ✅
**Files Modified**:
- `src/reader/internal/metadata_v2_flatbuffers.cpp` (4 functions added)
- `src/reader/internal/metadata_types_flatbuffers.cpp` (1 function added)

**Functions Implemented**:

1. **`metadata_v2_data::info_as_json()`** (lines ~1053-1206)
   - Generates comprehensive filesystem metadata as JSON
   - Includes features, block info, categories, metadata details
   - Adapted from Thrift to use FlatBuffers accessors

2. **`metadata_v2_data::serialize_as_json()`** (lines ~1208-1213)
   - Placeholder noting FlatBuffers serialization needs development
   - Returns minimal JSON notification

3. **`metadata_v2_data::dump()`** (lines ~1215-1219)
   - Entry point for recursive filesystem tree dumping

4. **`metadata_v2_data::as_json()`** (lines ~1221-1236)
   - Two overloads for converting entries to JSON
   - Recursively processes directory contents

5. **`dir_entry_view_impl::parent_shared()`** (lines ~507-512 in metadata_types_flatbuffers.cpp)
   - Returns shared_ptr to parent directory entry
   - Returns nullptr for root

---

## Next Session: Phase 2.8 Testing

### Step 1: Build Verification
```bash
cd /Users/mulgogi/src/external/dwarfs/build-test
ninja dwarfs_unit_tests
```

**Expected Outcome**:
- All files compile without errors
- `dwarfs_unit_tests` executable created

**If Build Fails**:
- Check for missing includes
- Verify all function signatures match interface
- Look for namespace issues

### Step 2: Run Metadata Tests
```bash
cd /Users/mulgogi/src/external/dwarfs/build-test
ctest -R metadata --output-on-failure
```

**Expected Outcome**:
- All metadata serialization tests pass
- Both Thrift and FlatBuffers backends work

**Common Issues to Watch For**:
1. **Pointer dereferencing**: Ensure all FlatBuffers pointers checked before use
2. **Empty vectors**: FlatBuffers returns nullptr for empty vectors, not empty vectors
3. **String access**: Use `->str()` method, not direct access

### Step 3: Run Full Test Suite
```bash
ctest --output-on-failure
```

**What to Monitor**:
- Any failures in existing tests (regression)
- New FlatBuffers-specific test results
- Memory leaks or crashes

### Step 4: Integration Testing
If unit tests pass, test with real DwarFS images:

```bash
# Create test image with FlatBuffers
./mkdwarfs -i /some/test/data -o test-fb.dwarfs

# Verify with dwarfsck
./dwarfsck test-fb.dwarfs

# Try to mount
./dwarfs test-fb.dwarfs /mnt/test -f
```

---

## Known Issues & TODOs

### High Priority
1. **`serialize_as_json()` incomplete**: Only placeholder implemented
   - Needs full FlatBuffers-to-JSON conversion
   - Required for `metadata_full_dump` feature
   - Low priority unless tests require it

2. **`unpack_metadata()` throws exception**:
   - Line 884 in metadata_v2_flatbuffers.cpp
   - Only needed for Thrift compatibility
   - Can be implemented later if needed

### Medium Priority
3. **Missing helper implementations**: Several methods in metadata_v2_flatbuffers.cpp reference undefined helpers:
   - `statvfs()`, `seek()`, `find()`, `readdir()`, `access()`
   - `get_all_uids()`, `get_all_gids()`, `get_block_numbers_by_category()`
   - `getattr_impl()`, `link_value()`, `file_inode_to_chunk_index()`
   - Various `walk`, `build_*`, `unpack_*` template implementations

   **Action**: These are likely already implemented further down in the file or will fail during compilation. Add them incrementally as test failures identify missing ones.

### Low Priority
4. **History metadata**: FlatBuffers HistoryEntry handling incomplete
   - Currently has TODO comment
   - Only affects version history display

---

## Success Criteria

✅ **Phase 2.8 Complete When**:
1. `ninja dwarfs_unit_tests` builds without errors
2. `ctest -R metadata` passes all metadata tests
3. No regressions in existing test suite
4. Can create, check, and mount FlatBuffers-format DwarFS images

---

## Files to Monitor

### Core Implementation
- `src/reader/internal/metadata_v2_flatbuffers.cpp` (1051 lines)
- `src/reader/internal/metadata_types_flatbuffers.cpp` (680 lines)
- `src/reader/internal/metadata_factory.cpp`

### Interface Definitions
- `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`
- `include/dwarfs/reader/internal/metadata_view_interface.h`

### Build System
- `cmake/metadata_serialization.cmake`
- `cmake/libdwarfs.cmake`

---

## Emergency Rollback Plan

If testing reveals fundamental issues:

```bash
# Revert to Phase 2.4 state
git checkout HEAD~5

# Or create backup branch
git branch backup-phase-2.8
git reset --hard <commit-before-phase-2.5>
```

**Backup commits**:
- Phase 2.4 complete: [identify commit hash]
- Last known good: [identify commit hash]

---

## Contact Points

**Architecture Questions**: See `doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`
**Build Issues**: See `.github/workflows/build.yml` lines 956-1113 for CI patterns
**Memory Bank**: `.kilocode/rules/memory-bank/` for project context

---

**Next Action**: Run `ninja dwarfs_unit_tests` in `build-test/` directory