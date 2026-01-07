# Phase 2: Thrift Header Creation - Continuation Prompt

**Date**: 2025-12-04
**Session**: Next Session Start Here
**Current Branch**: `refactor/dwarfs-mkdwarfs-complete`

---

## Context

You are working on **Phase 2** of the DwarFS 3-build benchmarking effort. The goal is to enable building DwarFS with:
1. FlatBuffers-only (modern default) ✅ Working
2. Thrift-only (legacy compatibility) ⏸️ 95% complete
3. Dual-format (both) ⏸️ Pending

Additionally, we need to **refactor all files >1000 lines to ≤800 lines** to maintain code quality.

---

## Current Status: 95% Complete

### What Works ✅
- Created `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h` (236 lines)
- Fixed include paths in `metadata_builder.cpp` and `metadata_builder_factory.cpp`
- FlatBuffers-only build is fully functional
- Header properly declares `thrift_metadata_builder` template class

### What's Broken ❌
**Build Error**: Duplicate class definition causing ambiguity

**File**: `src/writer/internal/thrift_metadata_builder.cpp`
**Error**: `reference to 'thrift_metadata_builder' is ambiguous`
**Cause**: Class is defined in BOTH the header (line 76) AND the .cpp file's anonymous namespace (lines 153-322)

**Compiler Output**:
```
error: reference to 'thrift_metadata_builder' is ambiguous
note: candidate found by name lookup is 'dwarfs::writer::internal::thrift_metadata_builder'
note: candidate found by name lookup is 'dwarfs::writer::internal::(anonymous namespace)::thrift_metadata_builder'
```

---

## Your Mission

Complete **Phase 2A** (30 min), then proceed with **Phase 2B-D** (file size refactoring).

---

## Phase 2A: Fix Duplicate Class (30 minutes)

### Task 1: Remove Duplicate Class Declaration (15 min)

**File**: `src/writer/internal/thrift_metadata_builder.cpp`

**Action**: Delete lines 153-322 (class declaration)

**What to Delete**:
```cpp
// Line 153: Start deletion here
// RENAMED: metadata_builder_ → thrift_metadata_builder
template <typename LoggerPolicy>
class thrift_metadata_builder final : public metadata_builder::impl {
  // ... entire class declaration ...
};
// Line 322: End deletion here

// Line 324: Keep this comment and below
// ... all existing method implementations stay the same ...
```

**What to Keep**:
- Lines 1-152: Helper functions (`get_conversion_factors`, `inode_size_provider`)
- Lines 323+: Method implementations (`build_thrift_internal()`, etc.)
- Lines 1246-1250: Template instantiations at end

**Result After Edit**:
```cpp
} // anonymous namespace  (line 152)

// ============================================================================
// thrift_metadata_builder method implementations
// ============================================================================

// build_thrift_internal implementation (line 327)
template <typename LoggerPolicy>
thrift::metadata::metadata const&
thrift_metadata_builder<LoggerPolicy>::build_thrift_internal() {
  // ... implementation ...
}
// ... rest of implementations ...
```

### Task 2: Verify Thrift-Only Build (10 min)

```bash
cd /Users/mulgogi/src/external/dwarfs

# Build Thrift-only
ninja -C build-tb mkdwarfs dwarfsck

# Verify binaries exist
ls -lh build-tb/{mkdwarfs,dwarfsck}

# Expected output: Two binaries created successfully
```

### Task 3: Verify Dual-Format Build (10 min)

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

# Verify
ls -lh build-dual/{mkdwarfs,dwarfsck}
```

---

## Phase 2B: File Size Audit (1 hour)

### Task 1: Run Audit (15 min)

```bash
cd /Users/mulgogi/src/external/dwarfs

# Find all files >800 lines
find src include -type f \( -name '*.cpp' -o -name '*.h' \) -exec wc -l {} + | \
  awk '$1 > 800 {print $1, $2}' | \
  sort -rn > doc/file-size-audit-results.txt

# Preview results
cat doc/file-size-audit-results.txt
```

### Task 2: Analyze Results (15 min)

Review `doc/file-size-audit-results.txt` and identify:
1. Files >1000 lines (CRITICAL - must fix)
2. Files 800-1000 lines (HIGH - should fix)
3. Common patterns (e.g., multiple large metadata files)

### Task 3: Create Refactoring Plan (30 min)

For each file, document:
- Current size
- Target size
- Extraction strategy
- New files to create
- Build system changes needed

**Template** (add to this document):
```markdown
#### File: path/to/large/file.cpp (XXXX lines)
**Target**: ≤800 lines
**Strategy**: Extract X methods into Y new file(s)
**New files**:
- path/to/extracted1.cpp (~XXX lines)
- path/to/extracted2.cpp (~XXX lines)
**Effort**: X hours
```

---

## Phase 2C: Refactor thrift_metadata_builder.cpp (3 hours)

**Current Size**: 1254 lines  
**Target Size**: ≤800 lines  
**Reduction Needed**: 454 lines (36%)

### Extraction Strategy

#### File 1: `thrift_metadata_inodes.cpp` (~100 lines)
**Extract**:
- `template <LoggerPolicy> void thrift_metadata_builder<LoggerPolicy>::update_inodes()`
- All helper logic related to inode updates

#### File 2: `thrift_metadata_links.cpp` (~60 lines)
**Extract**:
- `template <LoggerPolicy> void thrift_metadata_builder<LoggerPolicy>::update_nlink()`

#### File 3: `thrift_metadata_totals.cpp` (~160 lines)
**Extract**:
- `template <LoggerPolicy> void thrift_metadata_builder<LoggerPolicy>::update_totals_and_size_cache()`
- `inode_size_provider` helper class (if not used elsewhere)

#### File 4: `thrift_metadata_upgrade.cpp` (~150 lines)
**Extract**:
- `template <LoggerPolicy> void thrift_metadata_builder<LoggerPolicy>::upgrade_from_pre_v2_2()`
- `template <LoggerPolicy> void thrift_metadata_builder<LoggerPolicy>::upgrade_metadata(...)`

### Implementation Steps

For each extraction:

1. **Create new .cpp file**
   ```cpp
   #ifdef DWARFS_HAVE_THRIFT
   #include <dwarfs/writer/internal/thrift_metadata_builder_impl.h>
   // ... other includes ...
   
   namespace dwarfs::writer::internal {
   
   template <typename LoggerPolicy>
   void thrift_metadata_builder<LoggerPolicy>::method_name(...) {
     // ... implementation ...
   }
   
   // Explicit instantiations
   template void thrift_metadata_builder<debug_logger_policy>::method_name(...);
   template void thrift_metadata_builder<prod_logger_policy>::method_name(...);
   
   } // namespace dwarfs::writer::internal
   #endif // DWARFS_HAVE_THRIFT
   ```

2. **Update CMakeLists.txt**
   ```cmake
   # In cmake/libdwarfs.cmake, add to dwarfs_writer sources
   if(DWARFS_HAVE_THRIFT)
     target_sources(dwarfs_writer PRIVATE
       ${PROJECT_SOURCE_DIR}/src/writer/internal/thrift_metadata_builder.cpp
       ${PROJECT_SOURCE_DIR}/src/writer/internal/thrift_metadata_inodes.cpp
       ${PROJECT_SOURCE_DIR}/src/writer/internal/thrift_metadata_links.cpp
       ${PROJECT_SOURCE_DIR}/src/writer/internal/thrift_metadata_totals.cpp
       ${PROJECT_SOURCE_DIR}/src/writer/internal/thrift_metadata_upgrade.cpp
     )
   endif()
   ```

3. **Verify build after each extraction**
   ```bash
   ninja -C build-tb && ninja -C build-dual
   ```

---

## Phase 2D: Refactor Other Large Files (Variable)

**After Phase 2B audit**, apply similar extraction patterns to other files >800 lines.

**Priority**:
1. Files >1200 lines (CRITICAL)
2. Files 1000-1200 lines (HIGH)
3. Files 800-1000 lines (MEDIUM)

---

## Success Criteria

### Phase 2A Complete ✅
- [ ] No build errors in any configuration
- [ ] `build-fb/mkdwarfs` exists and works
- [ ] `build-tb/mkdwarfs` exists and works
- [ ] `build-dual/mkdwarfs` exists and works

### Phase 2B Complete ✅
- [ ] All files audited
- [ ] Detailed refactoring plan created
- [ ] Priorities established

### Phase 2C Complete ✅
- [ ] `thrift_metadata_builder.cpp` ≤800 lines
- [ ] 4 new extracted files created
- [ ] All builds verified
- [ ] Line count verified: `wc -l src/writer/internal/thrift_metadata_*.cpp`

### Phase 2D Complete ✅
- [ ] All critical files (>1000) refactored
- [ ] All builds verified
- [ ] File size policy documented

---

## Important Files

### Created/Modified
- ✅ `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h` (created)
- ✅ `src/writer/internal/metadata_builder.cpp` (include path fixed)
- ✅ `src/writer/internal/metadata_builder_factory.cpp` (include path fixed)
- ❌ `src/writer/internal/thrift_metadata_builder.cpp` (needs duplicate removed)

### Documentation
- 📄 `doc/PHASE_2_THRIFT_HEADER_CONTINUATION_PLAN.md` (this file)
- 📄 `doc/PHASE_2_THRIFT_HEADER_IMPLEMENTATION_STATUS.md`
- 📄 `.kilocode/rules/memory-bank/context.md` (update after completion)

### Build Directories
- `build-fb/` - FlatBuffers-only (working)
- `build-tb/` - Thrift-only (95% complete)
- `build-dual/` - Dual-format (pending)

---

## Reference Information

### Thrift Metadata Builder Structure

**Header** (`thrift_metadata_builder_impl.h`):
- Template class declaration
- All public/private method declarations
- Member variable declarations
- Documentation

**Implementation** (`thrift_metadata_builder.cpp`):
- Method implementations only
- Template instantiations at end
- `#ifdef DWARFS_HAVE_THRIFT` guards

### Build Configuration

**FlatBuffers-only**:
```bash
cmake -B build-fb -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
```

**Thrift-only**:
```bash
cmake -B build-tb -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
```

**Dual-format**:
```bash
cmake -B build-dual -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
```

---

## Known Issues

### Issue: dwarfsextract Segfault (DEFERRED)
**File**: `src/utility/filesystem_extractor.cpp`
**Status**: Pre-existing bug, not related to this work
**Workaround**: Use mount+copy instead
**Ref**: `doc/DWARFSEXTRACT_BUG_ANALYSIS.md`

---

## Time Budget

| Phase | Tasks | Estimated Time | Priority |
|-------|-------|---------------|----------|
| 2A | Complete Thrift header | 30 min | CRITICAL |
| 2B | File size audit | 1 hour | HIGH |
| 2C | Refactor thrift_metadata_builder.cpp | 3 hours | HIGH |
| 2D | Refactor other files | Variable | MEDIUM |
| **Total** | | **4.5-12.5 hours** | |

---

## Next Session Checklist

1. [ ] Read this document completely
2<br/> [ ] Read `doc/PHASE_2_THRIFT_HEADER_IMPLEMENTATION_STATUS.md`
3. [ ] Review memory bank: `.kilocode/rules/memory-bank/context.md`
4. [ ] Execute Phase 2A Task 1 (remove duplicate class)
5. [ ] Execute Phase 2A Task 2 (verify Thrift build)
6. [ ] Execute Phase 2A Task 3 (verify dual build)
7. [ ] Execute Phase 2B (file size audit)
8. [ ] Plan Phase 2C execution
9. [ ] Update status document as you progress

---

**Last Updated**: 2025-12-04 10:42 HKT  
**Status**: 🟡 95% Complete - Ready for Final Push  
**Next Milestone**: Phase 2A completion (30 minutes)  
**Final Goal**: All files ≤800 lines, all 3 builds working

---

## Quick Start Commands

```bash
# Navigate to project
cd /Users/mulgogi/src/external/dwarfs

# Check current status
git status
git log -1 --oneline

# Remove duplicate class (use your editor)
# Delete lines 153-322 from src/writer/internal/thrift_metadata_builder.cpp

# Test builds
ninja -C build-tb mkdwarfs dwarfsck
ninja -C build-dual mkdwarfs dwarfsck

# Run audit
find src include -type f \( -name '*.cpp' -o -name '*.h' \) -exec wc -l {} + | \
  awk '$1 > 800' | sort -rn
```

Good luck! 🚀