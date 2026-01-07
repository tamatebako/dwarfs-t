# Thrift-Only Build Refactoring - Continuation Prompt

**Date**: 2025-11-29 11:20 HKT  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Priority**: HIGH  
**Duration**: 6.5 hours estimated

---

## Quick Context

**Current Situation**: Thrift-only build fails with 2 errors after removing legacy includes

**Goal**: Enable all 3 build configurations through OOP refactoring

**Approach**: Apply MECE principles and proper separation of concerns

---

## Start-of-Session Commands

```bash
# 1. Navigate to project
cd /Users/mulgogi/src/external/dwarfs

# 2. Verify current state
git branch --show-current
git log --oneline -3

# 3. Read planning documents (MANDATORY)
cat doc/THRIFT_ONLY_BUILD_REFACTORING_PLAN.md
cat doc/THRIFT_ONLY_BUILD_FIX_STATUS.md

# 4. Verify current errors
cd build-thrift-only 2>/dev/null || \
  (cmake -B build-thrift-only -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DDWARFS_WITH_FLATBUFFERS=OFF \
    -DDWARFS_WITH_THRIFT=ON && \
   ninja -C build-thrift-only mkdwarfs 2>&1 | grep "error:" | head -10)

# Expected: 2 errors
# - time_resolution_handler.h:62 (forward declaration conflict)
# - metadata_v2_thrift.cpp:2426 (method redefinition)

# 5. Create session backup
git branch backup-before-thrift-refactor-$(date +%Y%m%d-%H%M)
```

---

## Implementation Strategy

### Phase 1: Remove Inline Implementations (2h) - START HERE

**Critical**: This phase must be done carefully, testing after EVERY file modification

**File**: [`include/dwarfs/reader/internal/metadata_v2.h`](../include/dwarfs/reader/internal/metadata_v2.h)

**Pattern** (apply to ~20 methods):

1. **Read** the method implementation from header
2. *

*Change** header to declaration only
3. **Move** implementation to .cpp file
4. **Test** FlatBuffers-only and dual-format builds still work
5. **Repeat** for next method

**Example**: `get_chunks()` method

**Step 1: Read** (metadata_v2.h:166-168):
```cpp
chunk_range get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}
```

**Step 2: Change header** to:
```cpp
chunk_range get_chunks(int inode, std::error_code& ec) const;
```

**Step 3: Add to src/reader/internal/metadata_v2.cpp**:
```cpp
chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}
```

**Step 4: Test**:
```bash
ninja -C build-flatbuffers-only mkdwarfs  # Must pass
ninja -C build-benchmark mkdwarfs          # Must pass
```

**WARNING**: If tests fail, revert immediately:
```bash
git checkout HEAD -- include/dwarfs/reader/internal/metadata_v2.h src/reader/internal/metadata_v2.cpp
```

**All Methods to Move** (in order of priority):
1. `get_chunks()` ← **Critical for Error 2**
2. `check_consistency()`
3. `size()`
4. `walk()`
5. `walk_data_order()`
6. `root()`
7-9. `find()` (3 overloads)
10-11. `getattr()` (2 overloads)
12. `opendir()`
13. `readdir()`
14. `dirsize()`
15. `access()`
16. `open()`
17. `seek()`
18. `readlink()`
19. `statvfs()`
20. `block_size()`
21. `has_symlinks()`
22. `has_sparse_files()`
23. `get_inode_info()`
24. `get_block_category()`
25. `get_block_category_metadata()`
26. `get_all_block_categories()`
27. `get_all_uids()`
28. `get_all_gids()`
29. `get_block_numbers_by_category()`
30. `internal_data()`

**After Phase 1**: Commit
```bash
git add include/dwarfs/reader/internal/metadata_v2.h src/reader/internal/metadata_v2.cpp
git commit -m "refactor(metadata): Move all metadata_v2 methods to .cpp

Removed inline implementations from metadata_v2.h to enable
proper OOP separation of concerns.

This fixes method redefinition errors in Thrift-only builds by
ensuring the public API doesn't expose backend types.

All ~30 methods now properly separated:
- Header: declarations only
- Implementation: src/reader/internal/metadata_v2.cpp

Tested:
- FlatBuffers-only: ✓ PASS
- Dual-format: ✓ PASS
- Thrift-only: 1 error remaining (forward declaration)"
```

### Phase 2: Fix Forward Declarations (1h)

**File**: [`include/dwarfs/reader/internal/time_resolution_handler.h`](../include/dwarfs/reader/internal/time_resolution_handler.h)

**Current** (line 62):
```cpp
class inode_view_impl;  // WRONG - conflicts with type alias
```

**Replacement** (use apply_diff):
```cpp
// Forward declaration: namespace-qualified for single-format builds
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: interface type (no forward declaration needed, already included)
  
#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: forward declare backend namespace
  namespace thrift_backend {
    class inode_view_impl;
  }
  
#elif defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
  // FlatBuffers-only: forward declare backend namespace
  namespace flatbuffers_backend {
    class inode_view_impl;
  }
  
#else
  #error "At least one metadata format must be enabled"
#endif
```

**Search for other forward declarations**:
```bash
grep -rn "class inode_view_impl;" include/ --exclude-dir=gen-*
grep -rn "class dir_entry_view_impl;" include/ --exclude-dir=gen-*
grep -rn "class chunk_range;" include/ --exclude-dir=gen-*
grep -rn "class global_metadata;" include/ --exclude-dir=gen-*
```

**Apply pattern to ALL findings**

**Test After Phase 2**:
```bash
ninja -C build-thrift-only mkdwarfs  # Should now compile!
ninja -C build-flatbuffers-only mkdwarfs  # Must still pass
ninja -C build-benchmark mkdwarfs  # Must still pass
```

**Commit**:
```bash
git add include/dwarfs/reader/internal/time_resolution_handler.h
git commit -m "refactor(metadata): Fix forward declarations for single-format builds

Replaced class forward declarations with namespace-qualified
forward declarations to avoid conflicts with type aliases.

Pattern applied:
- Dual-format: uses interface types (no forward decl needed)
- Single-format: forward declares backend namespace types

Fixes: Forward declaration conflict in time_resolution_handler.h

Tested:
- FlatBuffers-only: ✓ PASS
- Dual-format: ✓ PASS
- Thrift-only: ✓ PASS (0 errors!)"
```

### Phase 3: Test All Configurations (1h)

Create test script `test-all-configs.sh`:
```bash
#!/bin/bash
set -e

echo "=== Testing All Build Configurations ==="

# FlatBuffers-only
echo "--- FlatBuffers-only ---"
rm -rf build-fb-only
cmake -B build-fb-only -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
ninja -C build-fb-only mkdwarfs
echo "✓ FlatBuffers-only: SUCCESS"

# Thrift-only
echo "--- Thrift-only ---"
rm -rf build-tb-only
cmake -B build-tb-only -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
ninja -C build-tb-only mkdwarfs
echo "✓ Thrift-only: SUCCESS"

# Dual-format
echo "--- Dual-format ---"
rm -rf build-dual
cmake -B build-dual -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
ninja -C build-dual mkdwarfs
echo "✓ Dual-format: SUCCESS"

echo "=== ALL CONFIGURATIONS PASSED ==="
```

**Run**:
```bash
chmod +x test-all-configs.sh
./test-all-configs.sh
```

### Phase 4: Runtime Testing (1h)

```bash
# Create test data
mkdir -p /tmp/test-refactor
echo "Build test" > /tmp/test-refactor/file.txt

# Test each build
./build-fb-only/mkdwarfs -i /tmp/test-refactor -o /tmp/test-fb-refactor.dwarfs
./build-tb-only/mkdwarfs -i /tmp/test-refactor -o /tmp/test-tb-refactor.dwarfs
./build-dual/mkdwarfs -i /tmp/test-refactor -o /tmp/test-dual-refactor.dwarfs

# Verify all images are valid
ls -lh /tmp/test-*-refactor.dwarfs
```

### Phase 5: Update Benchmarks (1h)

Re-run benchmarks with working Thrift-only:
```bash
# Now we can benchmark all THREE configs!
cd /tmp && rm -rf test-data && mkdir test-data
for i in {1..100}; do echo "Test $i" > test-data/file$i.txt; done

# Test all three
echo "=== FlatBuffers-only ===" 
/usr/bin/time -l ~/src/external/dwarfs/build-fb-only/mkdwarfs \
  -i /tmp/test-data -o /tmp/bench-fb.dwarfs --no-history 2>&1 | grep -E "(real|maximum|Size)"

echo "=== Thrift-only ===" 
/usr/bin/time -l ~/src/external/dwarfs/build-tb-only/mkdwarfs \
  -i /tmp/test-data -o /tmp/bench-tb.dwarfs --no-history 2>&1 | grep -E "(real|maximum|Size)"

echo "=== Dual-format ===" 
/usr/bin/time -l ~/src/external/dwarfs/build-dual/mkdwarfs \
  -i /tmp/test-data -o /tmp/bench-dual.dwarfs --no-history 2>&1 | grep -E "(real|maximum|Size)"
```

Update [`doc/METADATA_FORMAT_BENCHMARK_RESULTS.md`](../doc/METADATA_FORMAT_BENCHMARK_RESULTS.md) with Thrift-only results

---

## Validation Checklist

After completing all phases:

- [ ] FlatBuffers-only: Compiles, mkdwarfs works
- [ ] Thrift-only: Compiles, mkdwarfs works
- [ ] Dual-format: Compiles, mkdwarfs works
- [ ] All images created are valid
- [ ] Dual-format can read all 3 image types
- [ ] Benchmarks updated with Thrift-only results
- [ ] Zero inline implementations in metadata_v2.h
- [ ] All forward declarations properly namespace-qualified
- [ ] Documentation updated

---

## Common Pitfalls

### Pitfall 1: Forgetting to Test After Each Change
**Symptom**: Multiple methods broken, hard to debug  
**Solution**: Test FlatBuffers-only and dual-format after EVERY method move

### Pitfall 2: Wrong Namespace in Forward Declaration
**Symptom**: "use of undeclared identifier"  
**Solution**: Ensure forward declaration matches backend namespace exactly

### Pitfall 3: Missing Method Implementation
**Symptom**: "undefined reference to metadata_v2::method"  
**Solution**: Verify ALL methods moved to .cpp have implementations

---

## Emergency Recovery

If something breaks badly:

```bash
# Restore from backup
git reset --hard backup-before-thrift-refactor-YYYYMMDD-HHMM

# Or restore specific files
git checkout HEAD~1 -- path/to/file
```

---

## Success Criteria

ALL must be true:

- ✅ `ninja -C build-fb-only mkdwarfs` → SUCCESS
- ✅ `ninja -C build-tb-only mkdwarfs` → SUCCESS
- ✅ `ninja -C build-dual mkdwarfs` → SUCCESS
- ✅ All three mkdwarfs create valid images
- ✅ Zero compiler errors
- ✅ Zero compiler warnings in metadata files
- ✅ Benchmarks show Thrift-only performance

---

## Timeline

| Phase | Task | Time |
|-------|------|------|
| 1 | Remove inline implementations | 2h |
| 2 | Fix forward declarations | 1h |
| 3 | Test all configurations | 1h |
| 4 | Runtime testing | 1h |
| 5 | Update benchmarks | 1h |
| Documentation | Final docs update | 30min |

**Total**: 6.5 hours

---

**Document Version**: 1.0  
**Created**: 2025-11-29 11:20 HKT  
**For**: Thrift-only refactoring implementation  
**Read First**: doc/THRIFT_ONLY_BUILD_REFACTORING_PLAN.md