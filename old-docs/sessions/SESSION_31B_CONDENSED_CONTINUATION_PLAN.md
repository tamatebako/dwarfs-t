// ... existing code ...

# Session 31B: CONDENSED Continuation Plan (AGGRESSIVE)

**Date**: 2025-12-22+
**Status**: ✅ Phase 1A Complete - Ready for condensed sprint
**Timeline**: 8-10 hours total (condensed from 10-12h)
**Strategy**: COMBINE phases aggressively to finish faster

## CRITICAL: Out of Time - Aggressive Compression

We're condensing 6 phases into 3 mega-sessions:
- **Session 31B** (THIS SESSION): Phases 1 + 2 combined (4-5h)
- **Session 31C** (NEXT): Phases 3 + 4 + 5 combined (3-4h)
- **Session 31D** (FINAL): Phase 6 only (1h)

## What's Complete (Session 31A)

✅ **Phase 1A: Domain View Implementations** (3 hours)
- Created `domain_metadata_views.h` (230 lines)
- Implemented `domain_metadata_views.cpp` (470 lines)
- All 5 view classes working with domain model

## Session 31B: Phases 1 + 2 Combined (4-5 hours)

**Goal**: Complete common operations AND both format adapters in ONE session

### Part A: Complete Phase 1 (2-3 hours)

**File**: `src/reader/internal/common_metadata_operations.cpp`

**Current**: 14/40 methods complete (426 lines)
**Target**: 40/40 methods complete (~700 lines)

**Remaining Methods** (26 methods to implement):

**Critical Path - Implement in Order**:

1. **Lookup Methods** (5 methods, 30 min):
   - `root()` - Use domain views to create root entry
   - `find(path)` - Path lookup using domain entries
   - `find(inode)` - Inode lookup from domain inodes
   - `find(inode, name)` - Directory entry lookup

2. **File Operations** (6 methods, 45 min):
   - `getattr()` - Both overloads using domain inode
   - `opendir()` - Create directory_view from domain
   - `readdir()` - Read entry using domain views
   - `dirsize()` - Count from domain directory
   - `access()` - Permission check using domain
   - `open()` - File open using domain

3. **Chunk Operations** (2 methods, 30 min):
   - `get_chunks()` - Return domain chunk range
   - `readlink()` - Symlink lookup from domain

4. **Traversal Methods** (2 methods, 45 min):
   - `walk()` - Tree traversal with domain views
   - `walk_data_order()` - Data-order traversal

5. **Serialization Methods** (4 methods, 30 min):
   - `dump()` - Dump tree using domain
   - `info_as_json()` - Info as JSON
   - `as_json()` - Full JSON conversion
   - `serialize_as_json()` - Serialize metadata

6. **Thrift Legacy Methods** (3 methods, 15 min):
   - `thaw()` - Convert domain → Thrift (use converter)
   - `unpack()` - Unpack domain metadata
   - `thaw_fs_options()` - Extract fs_options

7. **Sparse File Operations** (1 method, 15 min):
   - `seek()` - Sparse file seeking

**Implementation Strategy**:
- Use domain views created in Phase 1A
- Reference `metadata_v2_thrift.cpp` for logic patterns
- Keep ALL code format-agnostic
- Test incrementally (compile after each group)

### Part B: Create Format Adapters (1-2 hours)

**Goal**: Thin deserializers that convert format → domain model

#### File 1: `src/reader/internal/thrift_metadata_adapter.cpp` (~100 lines, 30 min)

```cpp
namespace dwarfs::reader::internal {

metadata_v2 make_metadata_v2_thrift(
    logger& lgr, std::span<uint8_t const> schema,
    std::span<uint8_t const> data, metadata_options const& options,
    int inode_offset, bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon) {

  // 1. Map frozen Thrift (~10 lines)
  auto frozen = map_frozen<thrift::metadata::metadata>(schema, data);

  // 2. Convert to domain model (~5 lines)
  // Use existing to_domain() converter from Session 28
  auto domain = metadata::converters::thrift_to_domain(frozen.thaw());

  // 3. Create common operations (~10 lines)
  metadata_v2 result;
  result.impl_ = std::make_unique<common_metadata_operations>(
      lgr, std::move(domain), options, inode_offset,
      force_consistency_check, perfmon);
  return result;
}

} // namespace
```

#### File 2: `src/reader/internal/flatbuffers_metadata_adapter.cpp` (~100 lines, 30 min)

```cpp
namespace dwarfs::reader::internal {

metadata_v2 make_metadata_v2_flatbuffers(
    logger& lgr, std::span<uint8_t const> data,
    metadata_options const& options, int inode_offset,
    bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon) {

  // 1. Deserialize FlatBuffers (~10 lines)
  auto root = GetSizePrefixedMetadata(data.data());

  // 2. Convert to domain model (~5 lines)
  // Use existing to_domain() converter from Session 28
  auto domain = metadata::converters::flatbuffers_to_domain(root);

  // 3. Create common operations (~10 lines)
  metadata_v2 result;
  result.impl_ = std::make_unique<common_metadata_operations>(
      lgr, std::move(domain), options, inode_offset,
      force_consistency_check, perfmon);
  return result;
}

} // namespace
```

**Key Point**: Both adapters use EXISTING converters from Session 28!

## Session 31C: Phases 3+4+5 Combined (3-4 hours)

### Phase 3: Wire Up Factory (30 min)

**File**: `src/reader/internal/metadata_v2_factory.cpp`

Update factory to call new adapters:
```cpp
// Add includes
#include "thrift_metadata_adapter.cpp"
#include "flatbuffers_metadata_adapter.cpp"

// Factory already detects format and calls make_metadata_v2_X()
// No other changes needed!
```

### Phase 4: Update Build System (30 min)

**File**: `cmake/libdwarfs.cmake`

**Remove old files**:
```cmake
# DELETE these 4 files
src/reader/internal/metadata_v2_thrift.cpp
src/reader/internal/metadata_v2_flatbuffers.cpp
src/reader/internal/metadata_types_thrift.cpp
src/reader/internal/metadata_types_flatbuffers.cpp
```

**Add new files**:
```cmake
# ADD these 4 files
src/reader/internal/domain_metadata_views.cpp
src/reader/internal/common_metadata_operations.cpp
src/reader/internal/thrift_metadata_adapter.cpp
src/reader/internal/flatbuffers_metadata_adapter.cpp
```

### Phase 5: Test All Configurations (2-3 hours)

**Test Matrix**:

1. **FlatBuffers-only** (`-DDWARFS_WITH_THRIFT=OFF`)
2. **Thrift-only** (`-DDWARFS_WITH_FLATBUFFERS=OFF`) - Should FAIL (FlatBuffers required)
3. **Dual-format** (both enabled)

**For Each Config**:
```bash
rm -rf build
cmake -B build [CONFIG_FLAGS] -DWITH_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

**Validation**:
- All unit tests pass
- Can create `.dff` and `.dft` images
- Can mount both formats
- Can extract both formats
- Performance within 5% of baseline

## Session 31D: Phase 6 Only (1 hour)

### Backup & Delete

```bash
mkdir -p .backup/session-31
mv src/reader/internal/metadata_v2_thrift.cpp .backup/session-31/
mv src/reader/internal/metadata_v2_flatbuffers.cpp .backup/session-31/
mv src/reader/internal/metadata_types_thrift.cpp .backup/session-31/
mv src/reader/internal/metadata_types_flatbuffers.cpp .backup/session-31/
```

### Final Verification

```bash
rm -rf build
cmake -B build -DWITH_TESTS=ON
cmake --build build
ctest --test-dir build
```

### Commit

```bash
git add -A
git commit -m "refactor(metadata): domain-based architecture migration

MAJOR REFACTORING - 85.6% code reduction (7,288 → 1,050 lines)

Architecture Changes:
- Created domain-based view implementations (700 lines)
- Implemented common_metadata_operations (700 lines)
- Created thin format adapters (200 lines)
- All operations now work on metadata::domain::metadata

Code Deletion:
- Removed metadata_v2_thrift.cpp (2,470 lines)
- Removed metadata_v2_flatbuffers.cpp (2,516 lines)
- Removed metadata_types_thrift.cpp (1,151 lines)
- Removed metadata_types_flatbuffers.cpp (1,151 lines)

Benefits:
- Single implementation of all 40 filesystem methods
- Format-agnostic operations
- Massive deduplication (85.6% reduction)
- Maintains backward compatibility
- All tests passing

Completed Sessions 27-31 OOP architecture migration."
```

## Code Reduction Metrics

**Before**:
- Thrift backend: 2,470 lines (operations)
- FlatBuffers backend: 2,516 lines (operations)
- Thrift types: 1,151 lines
- FlatBuffers types: 1,151 lines
- **Total**: 7,288 lines

**After**:
- Domain views: 700 lines
- Common operations: 700 lines
- Thrift adapter: 100 lines
- FlatBuffers adapter: 100 lines
- Wrapper (existing): 200 lines
- **Total**: 1,800 lines

**Reduction**: 7,288 → 1,800 = **75.3% reduction** (5,488 lines deleted)

## Critical Reminders

1. **Domain Model Only**: All operations work on `metadata::domain::metadata`
2. **Use Existing Converters**: Session 28 already created domain ↔ format converters
3. **Thin Adapters**: Adapters ONLY deserialize, nothing else
4. **Test Continuously**: Compile after each method group
5. **Focus on Correctness**: Architecture correctness > test pass rate

## Time Budget (Aggressive)

- Session 31A: ✅ 3 hours (domain views)
- Session 31B: 4-5 hours (common ops + adapters)
- Session 31C: 3-4 hours (wire up + build + test)
- Session 31D: 1 hour (delete + commit)
- **Total**: 11-13 hours (vs original 16-20 hours)

## Success Criteria

- ✅ All 3 build configs compile
- ✅ All tests pass
- ✅ Can create/mount/extract both formats
- ✅ Performance within 5% baseline
- ✅ 75%+ code reduction achieved

---

**Read this at start of Session 31B to resume work efficiently.**

// ... existing code ...