# Session 31D Continuation Prompt

**Date**: 2025-12-22
**Objective**: Complete domain view implementation and finish Session 31 migration
**Duration**: 4-6 hours
**Previous**: Session 31C discovered domain view gap

## Quick Context

Sessions 31A-C accomplished:
- ✅ **31B**: Created common_metadata_operations.cpp (700 lines)
- ✅ **31C**: Wired factory, updated build system, removed old backends

**Critical Gap**: Session 31B assumed domain view implementations that don't exist.

## What to Implement

### Phase 1A: Create Domain View Header (45 min)

**File**: `include/dwarfs/reader/internal/domain_metadata_views.h` (~200 lines)

**Three Classes Needed**:

1. **domain_inode_view_impl** : public inode_view_interface
   - Wraps `metadata::domain::inode_data`
   - Accesses mode/uid/gid via lookup tables
   - Implements all inode_view_interface methods

2. **domain_dir_entry_view_impl** : public dir_entry_view_interface
   - Wraps `metadata::domain::dir_entry`
   - Accesses names via string tables
   - Returns wrapped inodes

3. **domain_global_metadata** : public global_metadata_interface
   - Wraps `metadata::domain::metadata`
   - Provides counts, block_size, etc.
   - Handles optional fields

### Phase 1B: Implement Views (2-3 hours)

**File**: `src/reader/internal/domain_metadata_views.cpp` (~300 lines)

**Implementation Notes**:
- Use methods not fields: `entry.inode_num()` not `entry.inode_num`
- Handle optionals: `meta_.devices.value_or(std::vector<uint64_t>{})`
- Compact strings: Check `meta_.compact_names.has_value()` before using
- Directory iteration: Calculate entry count from next directory's first_entry

### Phase 1C: Fix Common Operations (30 min)

**File**: `src/reader/internal/common_metadata_operations.cpp`

**Changes Needed** (~20 occurrences):
```cpp
// Replace:
flatbuffers_backend::global_metadata → domain_global_metadata
flatbuffers_backend::inode_view_impl → domain_inode_view_impl
flatbuffers_backend::dir_entry_view_impl → domain_dir_entry_view_impl

// Fix directory iteration (no entry_count field):
auto const& dir = domain_meta_.directories[dir_index];
uint32_t first = dir.first_entry();
uint32_t next_first = (dir_index + 1 < domain_meta_.directories.size())
    ? domain_meta_.directories[dir_index + 1].first_entry()
    : domain_meta_.dir_entries->size();
uint32_t count = next_first - first;
```

### Phase 2: Build System (30 min)

Add to `cmake/libdwarfs.cmake`:
```cmake
# Session 31: Domain-based views
src/reader/internal/domain_metadata_views.cpp
```

### Phase 3: Testing (2 hours)

**3A. FlatBuffers-Only** (45 min):
```bash
rm -rf build-fb-only
cmake -B build-fb-only -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
ninja -C build-fb-only
ctest --test-dir build-fb-only
```

**3B. Dual-Format** (45 min):
```bash
rm -rf build-both
cmake -B build-both -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
ninja -C build-both
ctest --test-dir build-both
```

**3C. Integration** (30 min):
```bash
# Create image
./build-both/mkdwarfs -i /usr/bin -o test.dff --log-level=info

# Mount (in another terminal)
./build-both/dwarfs test.dff /mnt/test

# Test
ls -la /mnt/test/
md5sum /usr/bin/ls /mnt/test/ls  # Should match

# Extract
./build-both/dwarfsextract test.dff -o extracted/
diff -r /usr/bin extracted/

# Cleanup
umount /mnt/test
```

### Phase 4: Cleanup (30 min)

**Delete Old Backends** (7,288 lines):
```bash
git rm src/reader/internal/metadata_v2_thrift.cpp \
       src/reader/internal/metadata_v2_flatbuffers.cpp \
       src/reader/internal/metadata_types_thrift.cpp \
       src/reader/internal/metadata_types_flatbuffers.cpp
```

**Move Temp Docs**:
```bash
mkdir -p old-docs/session-31
mv doc/SESSION_31*.md old-docs/session-31/
```

**Keep**: `doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md` (update it)

## Key References

**Domain Model Types** (correct API):
- `metadata::domain::metadata` - Main container
- `metadata::domain::inode_data` - Use `meta_.inodes[index]`
- `metadata::domain::dir_entry` - Methods: `inode_num()`, `name_index()`
- `metadata::domain::directory` - Methods: `first_entry()`, `parent_entry()`, `self_entry()`

**Lookup Tables**:
- Names: `meta_.names[index]` OR `meta_.compact_names->get(index)` if compressed
- Modes: `meta_.modes[inode.mode_index]`
- UIDs: `meta_.uids[inode.owner_index]`
- GIDs: `meta_.gids[inode.group_index]`

**Existing Interfaces** (implement these):
- `inode_view_interface` - In `include/dwarfs/reader/metadata_types.h`
- `dir_entry_view_interface` - In `include/dwarfs/reader/metadata_types.h`
- `global_metadata_interface` - In `include/dwarfs/reader/metadata_types.h`

## Critical Files from Session 31C

**Already Modified**:
- ✅ `cmake/libdwarfs.cmake` - Old backends removed
- ✅ `src/reader/internal/common_metadata_operations.cpp` - Bug fixes
- ✅ `src/reader/internal/*_adapter.cpp` - Using Session 28 converters
- ✅ `src/reader/internal/metadata_v2_factory.cpp` - Wired up

**Need to Create**:
- ⏸️ `include/dwarfs/reader/internal/domain_metadata_views.h` - NEW
- ⏸️ `src/reader/internal/domain_metadata_views.cpp` - NEW

## Success Criteria

- ✅ Domain views compile and link
- ✅ common_metadata_operations.cpp compiles
- ✅ FlatBuffers-only build passes tests
- ✅ Dual-format build passes tests
- ✅ Can create, mount, extract images
- ✅ Old backends deleted (7,288 lines)
- ✅ Net code reduction: -6,738 lines (-85.6%)

## Quick Start Commands

```bash
# 1. Read architecture
cat doc/SESSION_31_ARCHITECTURE_CORRECT.md
cat doc/SESSION_31D_CONTINUATION_PLAN.md

# 2. Check status
cat doc/SESSION_31D_STATUS_TRACKER.md

# 3. Start Phase 1A
# Create include/dwarfs/reader/internal/domain_metadata_views.h
```

## Timeline

- Phase 1A+1B+1C: 3-4 hours (domain views)
- Phase 2: 30 min (build system)
- Phase 3: 2 hours (testing)
- Phase 4: 30 min (cleanup)
- **Total**: 4-6 hours

---

**Status**: Ready to implement
**Next**: Create domain_metadata_views.h