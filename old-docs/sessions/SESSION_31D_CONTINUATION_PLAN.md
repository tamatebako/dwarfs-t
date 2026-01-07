# Session 31D: Complete Domain View Implementation & Migration

**Date**: 2025-12-22
**Status**: Ready to begin
**Previous**: Session 31C (discovered domain view gap)
**Estimated Duration**: 4-6 hours

## Critical Discovery from Session 31C

Session 31B's `common_metadata_operations.cpp` was built on **non-existent domain view implementations**. The Session 31A phase that should have created these views was skipped, creating a foundational gap.

### What's Missing

**Domain View Implementations** (~500 lines):
- `domain_inode_view_impl` - Wraps `metadata::domain::inode_data`
- `domain_dir_entry_view_impl` - Wraps `metadata::domain::dir_entry`
- `domain_global_metadata` - Wraps `metadata::domain::metadata`

These views implement existing interfaces (`inode_view_interface`, etc.) but operate on domain model, not backend-specific types.

## Architecture (Correct)

```
Domain Model (format-agnostic)
      ↓
Domain Views (implement interfaces, wrap domain)
      ↓
Common Operations (use views, format-agnostic)
      ↓
Format Adapters (deserialize → domain)
```

## Phase 1: Domain View Implementation (3-4 hours)

### 1A: Create Header (45 min)

**File**: `include/dwarfs/reader/internal/domain_metadata_views.h` (~200 lines)

**Content**:
```cpp
#pragma once

#include <dwarfs/metadata/domain/metadata.h>
#include <dwarfs/reader/metadata_types.h>
#include <memory>

namespace dwarfs::reader::internal {

// Forward declarations
class logger;
class performance_monitor;

/**
 * Domain-based inode view implementation.
 * Wraps metadata::domain::inode_data and provides inode_view_interface.
 */
class domain_inode_view_impl : public inode_view_interface {
public:
  domain_inode_view_impl(
      metadata::domain::metadata const& meta,
      size_t inode_index,
      int inode_offset = 0);

  // Implement inode_view_interface
  uint32_t inode_num() const override;
  file_stat::mode_type mode() const override;
  file_stat::uid_type uid() const override;
  file_stat::gid_type gid() const override;
  uint64_t size() const override;
  // ... all interface methods

private:
  metadata::domain::metadata const& meta_;
  size_t inode_index_;
  int inode_offset_;
};

/**
 * Domain-based directory entry view.
 */
class domain_dir_entry_view_impl : public dir_entry_view_interface {
public:
  domain_dir_entry_view_impl(
      metadata::domain::metadata const& meta,
      size_t entry_index);

  // Implement dir_entry_view_interface
  std::string_view name() const override;
  inode_view inode() const override;
  bool is_root() const override;

private:
  metadata::domain::metadata const& meta_;
  size_t entry_index_;
};

/**
 * Domain-based global metadata.
 */
class domain_global_metadata : public global_metadata_interface {
public:
  explicit domain_global_metadata(
      metadata::domain::metadata const& meta);

  // Implement global_metadata_interface
  size_t chunk_count() const override;
  size_t directory_count() const override;
  size_t inode_count() const override;
  uint32_t block_size() const override;
  // ... all interface methods

private:
  metadata::domain::metadata const& meta_;
};

} // namespace dwarfs::reader::internal
```

### 1B: Implement Views (2-3 hours)

**File**: `src/reader/internal/domain_metadata_views.cpp` (~300 lines)

**Implementation Guidelines**:
1. **domain_inode_view_impl**:
   - Access `meta_.inodes[inode_index_]`
   - Lookup mode via `meta_.modes[inode.mode_index]`
   - Lookup uid via `meta_.uids[inode.owner_index]`
   - Lookup gid via `meta_.gids[inode.group_index]`
   - Handle timestamps via `meta_.timestamp_base + offset`

2. **domain_dir_entry_view_impl**:
   - Access `(*meta_.dir_entries)[entry_index_]`
   - Get name via `meta_.names[entry.name_index()]` OR `meta_.compact_names->get(entry.name_index())`
   - Return wrapped inode via `std::make_shared<domain_inode_view_impl>`

3. **domain_global_metadata**:
   - Direct access: `meta_.chunks.size()`, `meta_.directories.size()`, etc
   - Handle optional fields with `.value_or()` or `.has_value()` checks

### 1C: Update Common Operations (30 min)

**File**: `src/reader/internal/common_metadata_operations.cpp`

**Changes**:
- Replace `flatbuffers_backend::global_metadata` with `domain_global_metadata`
- Replace `flatbuffers_backend::inode_view_impl` with `domain_inode_view_impl`
- Replace ALL backend-specific view types with domain equivalents
- Fix directory iteration (no `entry_count` field in domain model)

**Directory Iteration Fix**:
```cpp
// OLD (wrong - assumes entry_count field):
for (uint32_t i = 0; i < directory.entry_count; ++i) {
  auto entry_idx = directory.first_entry + i;
}

// NEW (correct - calculate from dir_entries):
auto const& dir = domain_meta_.directories[dir_index];
uint32_t first = dir.first_entry();
// Calculate count by finding next directory's first_entry
uint32_t next_first = (dir_index + 1 < domain_meta_.directories.size())
    ? domain_meta_.directories[dir_index + 1].first_entry()
    : domain_meta_.dir_entries->size();
uint32_t count = next_first - first;

for (uint32_t i = 0; i < count; ++i) {
  auto entry_idx = first + i;
}
```

## Phase 2: Build System Integration (30 min)

### 2A: Update CMakeLists

**File**: `cmake/libdwarfs.cmake`

Add to `dwarfs_reader` sources:
```cmake
# Session 31: Domain-based views (NEW)
src/reader/internal/domain_metadata_views.cpp
```

### 2B: Verify Dependencies

Ensure converters are properly linked (already done in Session 31C).

## Phase 3: Testing & Validation (2 hours)

### 3A: FlatBuffers-Only Build (45 min)

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

**Expected**: All compile, tests pass

### 3B: Dual-Format Build (45 min)

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

**Expected**: All compile, tests pass

### 3C: Integration Testing (30 min)

**Test Scenarios**:
1. Create FlatBuffers image: `mkdwarfs -i /usr/bin -o test.dff`
2. Mount: `dwarfs test.dff /mnt/test`
3. Verify: `ls -la /mnt/test`, compare checksums
4. Extract: `dwarfsextract test.dff -o extracted/`
5. Compare: `diff -r /usr/bin extracted/`

## Phase 4: Cleanup & Documentation (30 min)

### 4A: Delete Old Backend Files

**Files to Remove** (7,288 lines):
- `src/reader/internal/metadata_v2_thrift.cpp` (2,470 lines)
- `src/reader/internal/metadata_v2_flatbuffers.cpp` (2,516 lines)
- `src/reader/internal/metadata_types_thrift.cpp` (1,151 lines)
- `src/reader/internal/metadata_types_flatbuffers.cpp` (1,151 lines)

**Action**: `git rm` these files after validation

### 4B: Update Documentation

**File**: `doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`

Update with:
- Domain view layer description
- Complete architecture diagram
- Performance impact notes

### 4C: Move Temporary Docs

```bash
mkdir -p old-docs/session-31
mv doc/SESSION_31*.md old-docs/session-31/
mv doc/FLATBUFFERS_METADATA_FIX_STATUS.md old-docs/session-31/
```

Keep: `METADATA_ARCHITECTURE_STRATEGY_PATTERN.md` (official)

## Success Criteria

- ✅ All domain views implemented and tested
- ✅ common_metadata_operations.cpp compiles and uses views
- ✅ FlatBuffers-only build works
- ✅ Dual-format build works
- ✅ Can create, mount, extract images
- ✅ Tests pass
- ✅ Old backend files deleted
- ✅ Documentation updated

## Timeline

| Phase | Task | Duration |
|-------|------|----------|
| 1A | Domain view header | 45 min |
| 1B | Domain view implementation | 2-3 hours |
| 1C | Fix common operations | 30 min |
| 2 | Build system | 30 min |
| 3A | Test FB-only | 45 min |
| 3B | Test dual-format | 45 min |
| 3C | Integration tests | 30 min |
| 4 | Cleanup & docs | 30 min |
| **Total** | | **4-6 hours** |

## Recovery from Session 31C

Session 31C completed:
- ✅ Factory wired up
- ✅ Build system updated
- ✅ Old backends removed
- ✅ Adapters use Session 28 converters
- ✅ Bug fixes applied

Session 31D completes:
- Domain views (the missing layer)
- Testing & validation
- Final cleanup

---

**Ready to begin Session 31D implementation!**