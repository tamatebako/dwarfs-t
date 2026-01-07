# Session 31EContinuationPlan: Complete Domain Migration

**Date**: 2025-12-22
**Decision**: OPTION 2 - Complete Full Migration
**Estimated Time**: 8-12 hours over 2-3 sessions
**Status**: Ready to execute

## Current State

**Completed in Session 31E (Phase 1)**:
- ✅ Fixed constructor signature (logger parameter added)
- ✅ Fixed friend declaration in metadata_v2.h
- ✅ Created domain view header stubs (150 lines)
- ✅ Created domain view implementation stubs (200 lines)

**Remaining Issues**:
- ❌ 20+ compilation errors
- ❌ Domain view classes incomplete (missing interface methods)
- ❌ Type system mismatch in common_metadata_operations.cpp
- ❌ Method syntax errors (.field vs .field())

## Execution Plan (Compressed Timeline)

### Session 31F: Complete Domain Views (4-6 hours)

**Phase 1: Complete Interface Implementations** (2-3 hours)

Files to modify:
1. `include/dwarfs/reader/internal/domain_metadata_views.h`
2. `src/reader/internal/domain_metadata_views.cpp`

Tasks:
- [ ] Complete `domain_global_metadata` implementation
  - [ ] Implement `make_dir_entry_view()` with proper entry lookup
  - [ ] Implement `self_dir_entry()` with inode→entry mapping
  - [ ] Add helper methods for entry lookup

- [ ] Complete `domain_inode_view_impl`
  - [ ] Already done (verified in stubs)
  - [ ] Add any missing interface methods

- [ ] Complete `domain_dir_entry_view_impl`
  - [ ] Fix `inode()` method to use proper inode lookup
  - [ ] Implement `path()` with proper traversal
  - [ ] Handle v2.2 vs v2.3 format differences

- [ ] Complete `domain_chunk_view`
  - [ ] Already done (verified in stubs)
  - [ ] Add support for large holes

- [ ] Complete `domain_chunk_range_impl`
  - [ ] Add dual-format iterator support
  - [ ] Implement interface methods for dual-format builds

**Phase 2: Fix Type System Integration** (2-3 hours)

Files to modify:
1. `src/reader/internal/common_metadata_operations.cpp`
2. `include/dwarfs/reader/metadata_types.h`

Tasks:
- [ ] Fix method syntax throughout common_metadata_operations.cpp:
  - [ ] Line 396: `chunk.block` → `chunk.block()`
  - [ ] Line 558: `entry.name_index` → `entry.name_index()`
  - [ ] All similar field→method conversions (~8 locations)

- [ ] Fix view construction issues:
  - [ ] Lines 266, 417, 432, 576: Fix dir_entry_view construction
  - [ ] Line 512: Fix inode_view construction
  - [ ] Lines 715, 727, 753: Fix directory iteration
  - [ ] Line 695: Fix directory_view construction
  - [ ] Line 1035: Fix chunk_range construction

- [ ] Fix std::distance lambdas (lines 405-407):
  - [ ] Update lambda signature for correct types

### Session 31G: Integration & Testing (2-3 hours)

**Phase 1: Build Validation** (45 min)

- [ ] FlatBuffers-only build:
  ```bash
  rm -rf build-fb-only
  cmake -B build-fb-only -GNinja -DCMAKE_BUILD_TYPE=Release \
    -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=ON
  ninja -C build-fb-only
  ```

- [ ] Dual-format build:
  ```bash
  rm -rf build-both
  cmake -B build-both -GNinja -DCMAKE_BUILD_TYPE=Release \
    -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON -DWITH_TESTS=ON
  ninja -C build-both
  ```

**Phase 2: Unit Testing** (45 min)

- [ ] Run metadata tests:
  ```bash
  ctest --test-dir build-fb-only --tests-regex "metadata" --output-on-failure
  ctest --test-dir build-both --tests-regex "metadata" --output-on-failure
  ```

- [ ] Run reader tests:
  ```bash
  ctest --test-dir build-fb-only --tests-regex "reader" --output-on-failure
  ctest --test-dir build-both --tests-regex "reader" --output-on-failure
  ```

**Phase 3: Integration Testing** (45 min)

- [ ] Create test image (FlatBuffers):
  ```bash
  ./build-fb-only/mkdwarfs -i /usr/bin -o test-fb.dff \
    --compression=zstd:level=3
  ```

- [ ] Create test image (Thrift, if dual-format):
  ```bash
  ./build-both/mkdwarfs -i /usr/bin -o test-thrift.dwarfs \
    --metadata-format=thrift --compression=zstd:level=3
  ```

- [ ] Mount and verify:
  ```bash
  mkdir -p /tmp/test-mount
  ./build-fb-only/dwarfs test-fb.dff /tmp/test-mount
  ls -la /tmp/test-mount/ls
  md5sum /usr/bin/ls /tmp/test-mount/ls  # Must match
  umount /tmp/test-mount
  ```

- [ ] Extract and verify:
  ```bash
  ./build-fb-only/dwarfsextract -i test-fb.dff -o extracted/
  diff -r /usr/bin extracted/
  ```

**Phase 4: Cleanup** (30 min)

- [ ] Delete old backend files (7,288 lines):
  ```bash
  git rm src/reader/internal/metadata_v2_thrift.cpp \
         src/reader/internal/metadata_v2_flatbuffers.cpp \
         src/reader/internal/metadata_types_thrift.cpp \
         src/reader/internal/metadata_types_flatbuffers.cpp
  ```

- [ ] Update cmake/libdwarfs.cmake to remove old files

- [ ] Git commit with comprehensive message

## Risk Mitigation

**Risk 1: Type System Complexity**
- **Mitigation**: Work incrementally, test each fix
- **Fallback**: Keep old backends until fully verified

**Risk 2: Runtime Bugs**
- **Mitigation**: Comprehensive integration testing
- **Fallback**: Add debug logging to catch issues

**Risk 3: Performance Regression**
- **Mitigation**: Run benchmarks before/after
- **Fallback**: Profile and optimize hot paths

## Success Criteria

- ✅ All builds compile (FlatBuffers-only, Thrift-only, dual-format)
- ✅ All unit tests pass
- ✅ Integration tests succeed (create/mount/extract)
- ✅ Old backend files deleted (7,288 lines)
- ✅ Net code reduction: -6,661 lines (-85%)
- ✅ No performance regression (within 5%)

## Files Modified Summary

**Created/Modified (Session 31E)**:
1. `src/reader/internal/flatbuffers_metadata_adapter.cpp` - Logger fix
2. `include/dwarfs/reader/internal/metadata_v2.h` - Friend declaration
3. `include/dwarfs/reader/internal/domain_metadata_views.h` - Stubs (150 lines)
4. `src/reader/internal/domain_metadata_views.cpp` - Stubs (200 lines)

**To Complete (Session 31F)**:
1. Complete domain_metadata_views.h (add ~50 lines)
2. Complete domain_metadata_views.cpp (add ~300 lines)
3. Fix common_metadata_operations.cpp (~50 fixes)
4. Update metadata_types.h (conditional includes)

**To Delete (Session 31G)**:
1. metadata_v2_thrift.cpp (2,470 lines)
2. metadata_v2_flatbuffers.cpp (2,516 lines)
3. metadata_types_thrift.cpp (1,151 lines)
4. metadata_types_flatbuffers.cpp (1,151 lines)

## Timeline Estimate

| Session | Duration | Focus |
|---------|----------|-------|
| 31F-Part1 | 2-3 hours | Complete domain views |
| 31F-Part2 | 2-3 hours | Fix type system |
| 31G | 2-3 hours | Test & cleanup |
| **Total** | **6-9 hours** | **Compressed from 8-12** |

---

**Last Updated**: 2025-12-22
**Status**: Ready for Session 31F execution
**Next**: Begin Phase 1 - Complete domain view implementations
