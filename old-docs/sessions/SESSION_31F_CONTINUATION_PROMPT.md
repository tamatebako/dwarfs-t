# Session 31F Continuation Prompt

**Date**: 2025-12-22
**Objective**: Complete Domain-Based Metadata Migration
**Duration**: 4-6 hours (compressed timeline)
**Previous**: Session 31E discovered empty files, created stubs

## Quick Start

```bash
# Read these files first (CRITICAL):
cat doc/SESSION_31F_IMPLEMENTATION_STATUS.md
cat doc/SESSION_31E_CONTINUATION_PLAN.md
cat doc/SESSION_31_ARCHITECTURE_CORRECT.md

# Then start Phase 1: Complete domain view implementations
# Edit: src/reader/internal/domain_metadata_views.cpp
```

## What Session 31E Accomplished

✅ **Fixed Critical Issues**:
- Constructor signature fixed (logger parameter added)
- Friend declaration fixed in metadata_v2.h
- Created domain view header stubs (150 lines)
- Created domain view implementation stubs (200 lines)

⚠️ **Current State**:
- ~20 compilation errors remain
- Domain view classes incomplete (missing implementations)
- `common_metadata_operations.cpp` needs syntax fixes

## What Session 31F Must Complete

### Phase 1: Complete Domain View Implementations (2-3 hours)

**Files to Edit**:
1. `src/reader/internal/domain_metadata_views.cpp` (~300 lines to add)
2. `include/dwarfs/reader/internal/domain_metadata_views.h` (~50 lines to add)

**Critical Implementations Needed**:

1. **`domain_global_metadata::make_dir_entry_view()`**
   - Currently returns generic entry
   - MUST implement proper lookup using `dir_entries` or `entry_table_v2_2`
   - Handle v2.2 vs v2.3 format differences

2. **`domain_global_metadata::self_dir_entry()`**
   - Currently returns dir_inode as-is
   - MUST map directory inode number to its entry index
   - Look up in directories table to find entry

3. **`domain_dir_entry_view_impl::inode()`**
   - Currently uses direct mapping (inode_num = inode_index)
   - MUST handle v2.2 format with `entry_table_v2_2`
   - In v2.3+, inode_num IS the index

4. **`domain_dir_entry_view_impl::path()`**
   - Currently returns only name()
   - MUST traverse parent chain to build full path
   - Stop at root (parent_index == self_index)

5. **`domain_chunk_range_impl` dual-format support**
   - Add iterator interface implementation
   - Only needed if BOTH formats enabled
   - Must wrap in `#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)`

### Phase 2: Fix common_metadata_operations.cpp (2-3 hours)

**20+ Errors to Fix** in `src/reader/internal/common_metadata_operations.cpp`:

**Method Syntax** (8 locations):
```cpp
// WRONG:
domain_meta_.chunks[i].block
entry.name_index
dir.first_entry

// CORRECT:
domain_meta_.chunks[i].block()
entry.name_index()
dir.first_entry()
```

**Locations to Fix**:
- Line 396: `.block` → `.block()`
- Line 558: `.name_index` → `.name_index()`
- Line 285, 286, 289, 347, 350: Verify `.first_entry()` syntax
- Search ALL `.` field accesses on domain model types

**View Construction** (15 locations):

Lines with errors:
- 266, 417, 432, 576: `dir_entry_view` construction in single-format build
- 512: `inode_view` construction (shared_ptr mismatch)
- 695: `directory_view` (private constructor)
- 715, 727, 753: `dir_entry_view` in readdir
- 1002, 1013, 1021: chunk_range early returns
- 1035: chunk_range final construction

**Fix Pattern**:
```cpp
// In FlatBuffers-only mode (#else case):
// WRONG:
dir_entry_view dev{std::static_pointer_cast<domain_dir_entry_view_impl const>(concrete)};

// CORRECT:
// Just use the concrete pointer directly since it IS the impl type
dir_entry_view dev{concrete};
```

**Lambda/Iterator Fix** (lines 405-407):
```cpp
// WRONG:
std::stable_sort(mid, entries.end(),
    [&first_chunk_block, &entries](auto const& a, auto const& b) {
      auto a_idx = std::distance(entries.data(), &a);  // ERROR
      auto b_idx = std::distance(entries.data(), &b);  // ERROR
      return first_chunk_block[a_idx] < first_chunk_block[b_idx];
    });

// CORRECT:
std::stable_sort(mid, entries.end(),
    [&first_chunk_block](auto const& a, auto const& b) {
      // Use indices directly from entries vector
      // Calculate offset from beginning
      return first_chunk_block[&a - entries.data()] <
             first_chunk_block[&b - entries.data()];
    });
```

### Phase 3: Build Validation (30 minutes)

**Checkpoint After Each Major Fix**:
```bash
# After domain views complete:
ninja -C build-test 2>&1 | grep "error:" | wc -l
# Expect: ~10-15 errors (down from 20)

# After method syntax fixes:
ninja -C build-test 2>&1 | grep "error:" | wc -l
# Expect: ~5 errors

# After constructor fixes:
ninja -C build-test 2>&1 | grep "error:" | wc -l
# Expect: 0 errors

# Full build:
ninja -C build-test
# Expect: SUCCESS
```

## Key Architecture Points

### Domain Model Access Patterns

**Correct API** (from `metadata::domain::metadata`):
```cpp
// Chunks - use methods:
meta_.chunks[i].block()   // NOT .block
meta_.chunks[i].offset()  // NOT .offset
meta_.chunks[i].size()    // NOT .size

// Directory - use methods:
meta_.directories[i].first_entry()  // NOT .first_entry
meta_.directories[i].parent_entry() // NOT .parent_entry

// Dir entries - use methods:
(*meta_.dir_entries)[i].name_index()  // NOT .name_index
(*meta_.dir_entries)[i].inode_num()   // NOT .inode_num

// Lookup tables - direct access:
meta_.names[idx]        // String, no method
meta_.uids[idx]         // uint32_t, no method
meta_.modes[idx]        // uint32_t, no method
```

### v2.2 vs v2.3 Format Handling

**v2.3+ Format** (modern, what we're testing):
- Has `dir_entries` optional vector
- `inode_num()` IS the inode index
- Direct mapping everywhere

**v2.2 Format** (legacy):
- NO `dir_entries`
- Has `entry_table_v2_2` mapping
- Must use `entry_table_v2_2[inode_num]` to get inode index

**Pattern**:
```cpp
if (meta_.dir_entries) {
  // v2.3+ path
  auto const& entry = (*meta_.dir_entries)[entry_index];
  uint32_t inode_num = entry.inode_num();
  uint32_t inode_index = inode_num;  // Direct mapping
} else {
  // v2.2 path
  uint32_t inode_num = /* from somewhere */;
  uint32_t inode_index = meta_.entry_table_v2_2[inode_num];
}
```

## Common Pitfalls to Avoid

1. **DON'T** access domain model fields without `()` - they're methods
2. **DON'T** use static_pointer_cast in single-format builds - not needed
3. **DON'T** forget to handle both v2.2 and v2.3 formats
4. **DON'T** assume `inode_num == inode_index` - only true in v2.3+
5. **DO** check `dir_entries.has_value()` before using
6. **DO** validate all indices before access
7. **DO** test incrementally - don't fix everything then build

## Build Commands

**Initial Test Build** (FlatBuffers-only):
```bash
rm -rf build-test
cmake -B build-test -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=OFF
```

**Incremental Builds** (after each fix):
```bash
ninja -C build-test 2>&1 | head -200
```

**Final Validation Build** (with tests):
```bash
rm -rf build-fb-only
cmake -B build-fb-only -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=ON
ninja -C build-fb-only
ctest --test-dir build-fb-only --output-on-failure
```

## Success Criteria

- ✅ 0 compilation errors in FlatBuffers-only build
- ✅ All domain view methods implemented correctly
- ✅ All syntax fixed in common_metadata_operations.cpp
- ✅ Build succeeds
- ✅ Ready for Session 31G testing

## Next Session (31G)

After Session 31F completes:
1. Run full test suite
2. Integration testing (create/mount/extract)
3. Dual-format build validation
4. Delete old backend files (7,288 lines)
5. Git commit

## Files Reference

**To Modify**:
- `include/dwarfs/reader/internal/domain_metadata_views.h`
- `src/reader/internal/domain_metadata_views.cpp`
- `src/reader/internal/common_metadata_operations.cpp`

**Domain Model Reference**:
- `include/dwarfs/metadata/domain/metadata.h`

**Supporting Files**:
- `include/dwarfs/reader/metadata_types.h`
- `include/dwarfs/reader/internal/metadata_view_interface.h`

---

**Status**: Ready to execute Phase 1
**Next**: Complete `domain_global_metadata::make_dir_entry_view()`
**Time**: Start with 2-3 hour focused session