# Phase 2: Thrift Header Creation - Continuation Plan

**Date**: 2025-12-04
**Status**: 95% Complete - Final Cleanup Needed
**Branch**: `refactor/dwarfs-mkdwarfs-complete`

---

## Current State

### Completed ✅
1. Created `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h` (236 lines)
2. Updated include paths in `metadata_builder.cpp`
3. Updated include paths in `metadata_builder_factory.cpp`
4. Header properly declares thrift_metadata_builder template class

### Remaining Issue 🔧
**Problem**: Duplicate class definition causing build ambiguity
- Class exists in BOTH header AND anonymous namespace in .cpp
- Compiler error: `reference to 'thrift_metadata_builder' is ambiguous`

**Solution**: Remove duplicate class declaration from .cpp file

---

## Phase 2 Completion Tasks

### Task 1: Remove Duplicate Class Declaration (15 min)
**File**: `src/writer/internal/thrift_metadata_builder.cpp`

**Action**: Delete lines 153-322 (class declaration in anonymous namespace)

**Keep**:
- Lines 1-152: Helper functions and classes
- Lines 327+: Method implementations
- Lines 1246-1250: Template instantiations

**Result**: Class only defined in header, implementations in .cpp

### Task 2: Build Verification (10 min)
```bash
# Thrift-only build
ninja -C build-tb mkdwarfs dwarfsck

# Verify binaries
ls -lh build-tb/{mkdwarfs,dwarfsck}
```

### Task 3: Dual-Format Build (10 min)
```bash
# Configure dual-format
cmake -B build-dual -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=OFF \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=ON \
  -DWITH_FUSE_DRIVER=OFF

# Build
ninja -C build-dual mkdwarfs dwarfsck
```

---

## File Size Refactoring Audit

### Requirement
**All files must be ≤800 lines** (target) or maximum 1000 lines (hard limit)

### Files Requiring Refactoring

#### 1. `src/writer/internal/thrift_metadata_builder.cpp` - 1254 lines ❌
**Current size**: 1254 lines
**Target**: ≤800 lines
**Reduction needed**: 454 lines (36%)

**Refactoring Strategy**:
- Extract `update_inodes()` implementation → `thrift_metadata_inodes.cpp` (~100 lines)
- Extract `update_nlink()` implementation → `thrift_metadata_links.cpp` (~60 lines)
- Extract `update_totals_and_size_cache()` → `thrift_metadata_totals.cpp` (~160 lines)
- Extract `upgrade_from_pre_v2_2()` → `thrift_metadata_upgrade.cpp` (~150 lines)
- Keep core methods in original file (~784 lines)

**Files to create**:
```
src/writer/internal/
├── thrift_metadata_builder.cpp (784 lines) ✓
├── thrift_metadata_inodes.cpp (100 lines) NEW
├── thrift_metadata_links.cpp (60 lines) NEW
├── thrift_metadata_totals.cpp (160 lines) NEW
└── thrift_metadata_upgrade.cpp (150 lines) NEW
```

#### 2. Other Large Files (To Be Audited)

**Action Required**: Run audit to identify all files >800 lines

```bash
find src include -name '*.cpp' -o -name '*.h' | \
  xargs wc -l | \
  awk '$1 > 800 {print $1, $2}' | \
  sort -rn
```

---

## Implementation Phases

### Phase 2A: Complete Thrift Header (30 min) - CURRENT
1. Remove duplicate class definition
2. Verify Thrift-only build
3. Verify dual-format build

### Phase 2B: File Size Audit (30 min)
1. Audit all source files for size
2. Identify files >800 lines
3. Create refactoring plan for each

### Phase 2C: Refactor thrift_metadata_builder.cpp (2-3 hours)
1. Extract update_inodes() → separate file
2. Extract update_nlink() → separate file
3. Extract update_totals_and_size_cache() → separate file
4. Extract upgrade_from_pre_v2_2() → separate file
5. Update CMakeLists.txt to include new files
6. Verify all builds still work

### Phase 2D: Refactor Other Large Files (Variable time)
- Depends on audit results
- Estimate 2-4 hours per file
- Priority: Most critical files first

---

## Success Criteria

### Phase 2A Complete When:
- [x] thrift_metadata_builder_impl.h created
- [ ] No duplicate class definitions
- [ ] Thrift-only build succeeds
- [ ] Dual-format build succeeds
- [ ] All 3 binaries exist (FlatBuffers, Thrift, Dual)

### Phase 2B Complete When:
- [ ] All files audited for size
- [ ] Refactoring plan created for files >800 lines
- [ ] Priority order established

### Phase 2C Complete When:
- [ ] thrift_metadata_builder.cpp ≤800 lines
- [ ] All extracted files created and building
- [ ] All builds verified working
- [ ] No regression in functionality

### Phase 2D Complete When:
- [ ] All files ≤800 lines (target) or ≤1000 lines (maximum)
- [ ] All builds verified
- [ ] Documentation updated

---

## Timeline

**Phase 2A** (Thrift header completion): 30 minutes
**Phase 2B** (Audit): 30 minutes
**Phase 2C** (Refactor thrift_metadata_builder.cpp): 2-3 hours
**Phase 2D** (Refactor other files): Variable (2-8 hours)

**Total estimated time**: 5.5-12 hours

---

## Next Session Start

1. **Read this document**
2. **Execute Phase 2A Task 1**: Remove duplicate class from thrift_metadata_builder.cpp
3. **Execute Phase 2A Tasks 2-3**: Verify builds
4. **Execute Phase 2B**: Run file size audit
5. **Plan Phase 2C**: Detail the refactoring strategy

---

## Documentation Updates Required

### After Phase 2A:
- Update README.md with dual-format build instructions
- Document thrift_metadata_builder_impl.h in architecture docs

### After Phase 2C:
- Document file size policy (800 line target)
- Document refactoring patterns used
- Update architecture docs with new file structure

---

**Last Updated**: 2025-12-04
**Next Review**: After Phase 2A completion