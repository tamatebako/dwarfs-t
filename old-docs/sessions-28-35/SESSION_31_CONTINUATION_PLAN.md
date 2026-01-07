// ... existing code ...

# Session 31 Continuation Plan: Domain-Based Metadata Migration

**Date**: 2025-12-22
**Architecture Reference**: `doc/SESSION_31_ARCHITECTURE_CORRECT.md`
**Timeline**: 16-20 hours (revised from 11-13.5h due to view implementation requirement)

## CRITICAL REALIZATION (Session 31A - 2025-12-22)

**Discovery**: The view types (`inode_view`, `dir_entry_view`, `directory_view`) are tightly coupled to backend-specific implementations through interfaces. We cannot implement common operations without first creating domain-based view implementations.

**Architectural Impact**:
- Original plan assumed we could work directly with domain model
- Reality: Must create domain-based implementations of view interfaces first
- This adds ~4-6 hours to Phase 1 (domain view implementations)

**Revised Approach**:
```
Original Plan:
  Domain Model → Common Operations → Adapters

Correct Plan:
  Domain Model → Domain Views → Common Operations → Adapters
                    ↑ NEW         ↑ uses views
```

**Required New Work**:
1. Create `domain_inode_view_impl` (~100 lines)
2. Create `domain_dir_entry_view_impl` (~100 lines)
3. Create `domain_global_metadata` (~100 lines)
4. Create `domain_directory_view` (uses existing directory_view)

## Quick Start for Next Session

```bash
# 1. Read the architecture doc first
cat doc/SESSION_31_ARCHITECTURE_CORRECT.md

# 2. Check REVISED status
cat doc/SESSION_31_STATUS_TRACKER.md

# 3. Review what's been created
ls -la src/reader/internal/common_metadata_operations.*

# 4. Continue with Phase 1A (NEW): Domain View Implementations
```

## Compressed Timeline Strategy

### Phase 1A: Domain View Implementations (NEW - 4-6 hours)

**Objective**: Create domain-based implementations of view interfaces

**Critical Files to Create**:
1. `include/dwarfs/reader/internal/domain_metadata_views.h` (~200 lines)
   - `domain_inode_view_impl` : public `inode_view_interface`
   - `domain_dir_entry_view_impl` : public `dir_entry_view_interface`
   - `domain_global_metadata` : public `global_metadata_interface`

2. `src/reader/internal/domain_metadata_views.cpp` (~300 lines)
   - Implementation of all view interface methods
   - Works with `metadata::domain::metadata` directly

**Why This Is Critical**:
- All 40 methods in `common_metadata_operations` use view types
- View types expect interface implementations
- Cannot proceed without domain-based views

## Implementation Phases

### Phase 1: Create Common Domain-Based Operations (4-5 hours)

**Objective**: Implement all 40 filesystem methods using domain model

**Files to Create**:
- `src/reader/internal/common_metadata_operations.h` (~150 lines)
- `src/reader/internal/common_metadata_operations.cpp` (~500-700 lines)

**Key Points**:
- ALL operations work on `metadata::domain::metadata`
- NO access to frozen Thrift or FlatBuffers types
- Implements `metadata_v2::impl` interface (40 virtual methods)
- Store domain model as member variable

**Methods to Implement**:
1. `check_consistency()` - Validate domain model integrity
2. `size()` - Return metadata size
3. `walk()` - Tree traversal using domain entries
4. `walk_data_order()` - Traverse by file data order
5. `root()` - Get root directory entry
6. `find(std::string_view path)` - Path lookup
7. `find(int inode)` - Inode lookup
8. `find(int inode, std::string_view name)` - Directory entry lookup
9. `getattr()` - Get file attributes (2 overloads)
10. `opendir()` - Open directory
11. `readdir()` - Read directory entry
12. `dirsize()` - Get directory size
13. `access()` - Check access permissions
14. `open()` - Open file
15. `seek()` - Seek in file
16. `readlink()` - Read symlink
17. `statvfs()` - Get filesystem stats
18. `get_chunks()` - Get file chunks
19. `block_size()` - Get block size
20. `has_symlinks()` - Check if has symlinks
21. `has_sparse_files()` - Check if has sparse files
22. `get_inode_info()` - Get inode info as JSON
23. `get_block_category()` - Get block category
24. `get_block_category_metadata()` - Get category metadata
25. `get_all_block_categories()` - List all categories
26. `get_all_uids()` - List all UIDs
27. `get_all_gids()` - List all GIDs
28. `get_block_numbers_by_category()` - Filter blocks by category
29. `dump()` - Dump filesystem tree
30. `info_as_json()` - Get filesystem info as JSON
31. `as_json()` - Convert to JSON
32. `serialize_as_json()` - Serialize metadata
33. `thaw()` - Convert to Thrift (for Thrift-backed images)
34. `unpack()` - Unpack metadata (for Thrift-backed images)
35. `thaw_fs_options()` - Get filesystem options (for Thrift-backed images)

**Tips**:
- Start with simple methods (size, block_size, has_symlinks)
- Then implement lookup methods (find, getattr)
- Finally implement complex methods (walk, get_chunks)
- Reference existing `metadata_v2_data` for logic patterns

### Phase 2: Create Format Adapters (2-3 hours)

**Objective**: Create thin deserializers that convert format → domain model

#### Phase 2A: Thrift Adapter (~100 lines)
**File**: `src/reader/internal/thrift_metadata_adapter.cpp`

```cpp
namespace dwarfs::reader::internal {

metadata_v2 make_metadata_v2_thrift(
    logger& lgr, std::span<uint8_t const> schema,
    std::span<uint8_t const> data, metadata_options const& options,
    int inode_offset, bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon) {

  // 1. Map frozen Thrift (existing code ~20 lines)
  auto frozen = map_frozen<thrift::metadata::metadata>(schema, data);

  // 2. Convert to domain model (~5 lines)
  auto domain = metadata::converters::from_thrift(frozen.thaw());

  // 3. Create common operations (~10 lines)
  metadata_v2 result;
  result.impl_ = std::make_unique<common_metadata_operations>(
      lgr, std::move(domain), options, inode_offset,
      force_consistency_check, perfmon);
  return result;
}

} // namespace
```

#### Phase 2B: FlatBuffers Adapter (~100 lines)
**File**: `src/reader/internal/flatbuffers_metadata_adapter.cpp`

Similar structure, but:
1. Deserialize FlatBuffers root
2. Convert to domain model using FlatBuffers converter
3. Create common operations

### Phase 3: Wire Up Factory (1 hour)

**File**: `src/reader/internal/metadata_v2_factory.cpp`

**Changes**:
```cpp
// Update includes
#include "common_metadata_operations.h"
#include "thrift_metadata_adapter.cpp"
#include "flatbuffers_metadata_adapter.cpp"

// Factory already calls make_metadata_v2_thrift/flatbuffers
// No other changes needed!
```

### Phase 4: Update Build System (30 minutes)

**File**: `cmake/libdwarfs.cmake`

**Remove from dwarfs_reader sources**:
```cmake
# OLD - Remove these
src/reader/internal/metadata_v2_thrift.cpp
src/reader/internal/metadata_v2_flatbuffers.cpp
src/reader/internal/metadata_types_thrift.cpp
src/reader/internal/metadata_types_flatbuffers.cpp
```

**Add to dwarfs_reader sources**:
```cmake
# NEW - Add these
src/reader/internal/common_metadata_operations.cpp
src/reader/internal/thrift_metadata_adapter.cpp
src/reader/internal/flatbuffers_metadata_adapter.cpp
```

### Phase 5: Testing (2-3 hours)

**Test Matrix**:

1. **FlatBuffers-only build** (`-DDWARFS_WITH_THRIFT=OFF`)
   ```bash
   rm -rf build && cmake -B build -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=ON
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

2. **Thrift-only build** (`-DDWARFS_WITH_FLATBUFFERS=OFF`)
   ```bash
   rm -rf build && cmake -B build -DDWARFS_WITH_FLATBUFFERS=OFF -DWITH_TESTS=ON
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

3. **Dual-format build** (both enabled)
   ```bash
   rm -rf build && cmake -B build -DWITH_TESTS=ON
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

**Validation**:
- All unit tests pass
- Can create `.dff` images (FlatBuffers)
- Can create `.dft` images (Thrift)
- Can mount both format images
- Can extract both format images
- Performance within 5% of baseline

### Phase 6: Delete Old Code & Commit (1 hour)

**Safety First**:
```bash
# 1. Backup old files
mkdir -p .backup/session-31
mv src/reader/internal/metadata_v2_thrift.cpp .backup/session-31/
mv src/reader/internal/metadata_v2_flatbuffers.cpp .backup/session-31/
mv src/reader/internal/metadata_types_thrift.cpp .backup/session-31/
mv src/reader/internal/metadata_types_flatbuffers.cpp .backup/session-31/
```

**Verify**:
```bash
# Rebuild from scratch
rm -rf build
cmake -B build -DWITH_TESTS=ON
cmake --build build
ctest --test-dir build
```

**Commit**:
```bash
git add -A
git commit -m "refactor(metadata): migrate to domain-based architecture

- Eliminate 7,288 lines of format-specific duplicate code
- Create common_metadata_operations (~650 lines) for all filesystem ops
- Create thin format adapters (~200 lines) for deserialization
- All operations now work on metadata::domain::metadata
- 85.6% code reduction (7,288 → 1,050 lines)
- Maintains backward compatibility with both formats
- All tests passing

This completes the clean OOP architecture migration from Sessions 27-31."
```

**Delete backups** (only after commit):
```bash
rm -rf .backup/session-31/
```

## Common Issues & Solutions

### Issue 1: Compilation Errors in common_metadata_operations
**Symptoms**: Missing includes, undefined types
**Solution**:
- Ensure all domain types are included
- Check [`include/dwarfs/metadata/domain/`](../include/dwarfs/metadata/domain/) for all types
- Domain model uses std:: types, not backend-specific wrappers

### Issue 2: Tests Fail After Migration
**Symptoms**: Tests expect frozen types, get domain types
**Solution**:
- Tests should work with public API (metadata_v2)
- If tests access internals, update them to use domain model
- May need to update test expectations

### Issue 3: Performance Regression
**Symptoms**: Slower than baseline
**Solution**:
- Check if domain model is being copied (use std::move)
- Profile hot paths
- May need caching for computed values

### Issue 4: Memory Usage Increase
**Symptoms**: Higher memory usage than frozen types
**Solution**:
- Domain model is uncompressed; frozen types are packed
- This is expected and acceptable (easier to work with)
- Monitor that it's reasonable (<2x increase)

## Monitoring Progress

**Use the STATUS_TRACKER.md**:
```bash
# Check status
cat doc/SESSION_31_STATUS_TRACKER.md

# Update as you complete phases
# Mark tasks as [x] completed
```

##  Critical Reminders

1. **Reference Architecture Doc**: Always refer to `SESSION_31_ARCHITECTURE_CORRECT.md`
2. **Domain Model Only**: Common operations MUST NOT access frozen types
3. **Thin Adapters**: Adapters ONLY deserialize, nothing else
4. **Test Each Phase**: Don't proceed without passing tests
5. **Backup Before Delete**: Safety first!

## Next Steps

1. ✅ Create `common_metadata_operations.h` (structure only)
2. ⏸️ Implement simple methods first (size, block_size, etc.)
3. ⏸️ Implement lookup methods (find, getattr)
4. ⏸️ Implement complex methods (walk, get_chunks)
5. ⏸️ Create format adapters
6. ⏸️ Test and validate
7. ⏸️ Delete old code

---

**Read this document at the start of each session to maintain context.**

// ... existing code ...