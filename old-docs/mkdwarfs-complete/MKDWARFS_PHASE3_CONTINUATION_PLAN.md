# mkdwarfs Refactoring - Phase 3: Recompress Handler Extraction

**Date Created**: 2025-11-25 09:03 HKT  
**Prerequisites**: Phase 1 ✅ Complete, Phase 2 ✅ Complete  
**Branch**: `refactor/mkdwarfs-phase1`  
**Current Commit**: `f5ec0de0`  
**Working Directory**: `/Users/mulgogi/src/external/dwarfs`

---

## Executive Summary

Phase 3 aims to extract recompress functionality into a dedicated handler to enable building mkdwarfs without Thrift support. **Critical discovery**: The actual recompress implementation already exists in `dwarfs/utility/rewrite_filesystem.h` (a library utility), but mkdwarfs_main.cpp currently has NO recompress execution code - only option parsing and validation.

**Key Finding**: The recompress validation logic is in `options_parser.cpp` (guards against using recompress without Thrift), but there's NO recompress execution path in the current 686-line mkdwarfs_main.cpp.

**Phase 3 Status**: The work may already be partially done! We need to verify if:
1. Recompress was removed during refactoring (intentionally or accidentally)
2. Recompress execution was never in mkdwarfs_main.cpp
3. A separate execution path exists elsewhere

---

## Current State Analysis

### What Exists Now

**In `options_parser.cpp` (✅ Already Has Thrift Guards)**:
```cpp
void options_parser::validate_recompress_requirements(parsed_options const& opts) {
#ifndef DWARFS_HAVE_THRIFT
  if (opts.is_recompress || opts.rebuild_metadata || opts.change_block_size) {
    throw std::runtime_error(
        "recompress functionality requires Thrift support\n"
        "This build was compiled without Thrift (DWARFS_WITH_THRIFT=OFF)\n"
        ...
        "To use recompress features, rebuild with DWARFS_WITH_THRIFT=ON");
  }
#endif
}
```

**In `mkdwarfs_main.cpp` (Current 686 lines)**:
- Lines 85-88: Thrift-guarded includes for rewrite_filesystem
- Lines 442-445: Variable declarations for recompress options
- Lines 514-517: Mapping recompress options from parsed_options
- **NO EXECUTION CODE** for recompress

### What's Missing

**Expected but NOT Found**:
- Code that actually calls `utility::rewrite_filesystem()`
- Code that handles recompress mode vs create mode branching
- Integration between recompress options and actual recompression

**Possible Locations** (need to check):
1. Original mkdwarfs_main.cpp before Phase 1 (commit before `ed54f53c`)
2. Removed during Phase 1/2 refactoring
3. Never existed (separate tool?)

---

## Investigation Required

### Step 1: Find Original Recompress Code

```bash
# Check the very first commit before refactoring started
cd /Users/mulgogi/src/external/dwarfs
git log --all --oneline --reverse | head -20

# Find when recompress was added
git log --all --oneline --grep="recompress" --grep="rewrite" | tail -10

# Check the file at the earliest refactoring commit
git show ed54f53c^:tools/src/mkdwarfs_main.cpp | grep -A 50 "rewrite_filesystem\|recompress_opts"
```

### Step 2: Determine Recompress Execution Path

**Hypothesis A**: Recompress was in original mkdwarfs_main.cpp
- If true: Need to extract it to recompress_handler
- Lines to extract: ~100-200 (estimated)
- Guards needed: `#ifdef DWARFS_HAVE_THRIFT`

**Hypothesis B**: Recompress was removed during refactoring
- If true: Need to restore and extract to handler
- Reference: Original working implementation

**Hypothesis C**: Recompress never existed in mkdwarfs
- If true: Different tool handles it (dwarfsck? separate command?)
- Action: Document and skip Phase 3

### Step 3: Verify Build Behaviors

```bash
# Test current build without Thrift
cmake -B build-no-thrift -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=OFF \
  -DWITH_FUSE_DRIVER=OFF
ninja -C build-no-thrift mkdwarfs
# Expected: Should compile successfully

# Test recompress option
./build-no-thrift/mkdwarfs --recompress=all \
  -I test.dwarfs -O test2.dwarfs 2>&1
# Expected: Error message from options_parser validation
```

---

## Phase 3 Strategy (Depends on Investigation)

### Scenario A: Recompress Code Exists in History

**Tasks**:
1. Locate recompress execution code in git history
2. Create `recompress_handler.h` and `recompress_handler.cpp`
3. Extract recompress logic with proper Thrift guards
4. Integrate handler into mkdwarfs_main.cpp
5. Update CMake build system
6. Test with/without Thrift

**Expected Outcome**:
- mkdwarfs builds with `-DDWARFS_WITH_THRIFT=OFF`
- Recompress works with `-DDWARFS_WITH_THRIFT=ON`
- Clear error when recompress used without Thrift
- ~100-200 lines extracted, mkdwarfs_main.cpp → ~500-600 lines

### Scenario B: Recompress Was Removed

**Tasks**:
1. Restore recompress code from git history
2. Create `recompress_handler.h` and `recompress_handler.cpp`
3. Immediately extract to handler (don't add back to main)
4. Integrate handler + guards
5. Test thoroughly

**Expected Outcome**: Same as Scenario A

### Scenario C: Recompress Never in mkdwarfs

**Tasks**:
1. Document that recompress is handled by utility library only
2. Verify options_parser guards are sufficient
3. Update documentation
4. **Skip to Phase 4** (handler factory pattern)

**Expected Outcome**:
- Phase 3 marked as N/A
- Current Thrift guards in options_parser sufficient
- No code extraction needed

---

## Proposed Architecture (If Code Found)

### File Structure
```
tools/
├── include/dwarfs/tool/mkdwarfs/
│   ├── options_parser.h          ✅ Exists (Phase 1)
│   ├── create_handler.h          ✅ Exists (Phase 2)
│   └── recompress_handler.h      🆕 Phase 3
├── src/mkdwarfs/
│   ├── options_parser.cpp        ✅ Exists (Phase 1)
│   ├── create_handler.cpp        ✅ Exists (Phase 2)
│   └── recompress_handler.cpp    🆕 Phase 3
└── src/
    └── mkdwarfs_main.cpp         📝 To be simplified further
```

### Handler Interface (Draft)
```cpp
// tools/include/dwarfs/tool/mkdwarfs/recompress_handler.h
#pragma once

#ifdef DWARFS_HAVE_THRIFT

namespace dwarfs {
namespace tool {
class iolayer;
namespace mkdwarfs {

class parsed_options;

class recompress_handler {
public:
  recompress_handler() = default;
  
  /**
   * Execute recompress operation
   * 
   * @param opts Parsed command-line options
   * @param iol I/O layer for file access
   * @return 0 on success, error code otherwise
   */
  int run(parsed_options const& opts, iolayer const& iol);
  
private:
  // Implementation details
};

} // namespace mkdwarfs
} // namespace tool
} // namespace dwarfs

#endif // DWARFS_HAVE_THRIFT
```

### Integration in mkdwarfs_main.cpp (Draft)
```cpp
int mkdwarfs_main(int argc, sys_char** argv, iolayer const& iol) {
  // ... options parsing ...
  
  if (opts.is_recompress) {
#ifdef DWARFS_HAVE_THRIFT
    mkdwarfs::recompress_handler handler;
    return handler.run(opts, iol);
#else
    // Should never reach here due to options_parser validation
    iol.err << "error: recompress requires Thrift support\n";
    return 1;
#endif
  }
  
  // ... normal create path (already extracted to create_handler) ...
  mkdwarfs::create_handler handler;
  return handler.run(opts, iol, console, prog, fsw, entry_filter.get(), extra_deps);
}
```

---

## Testing Strategy

### Functional Tests

#### Test 1: Build Without Thrift
```bash
cmake -B build-no-thrift -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=OFF
ninja -C build-no-thrift mkdwarfs
# Expected: ✅ Compiles successfully
```

#### Test 2: Build With Thrift
```bash
cmake -B build-with-thrift -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=OFF
ninja -C build-with-thrift mkdwarfs
# Expected: ✅ Compiles successfully
```

#### Test 3: Recompress Without Thrift
```bash
./build-no-thrift/mkdwarfs --recompress=all \
  -I test.dwarfs -O test2.dwarfs 2>&1
# Expected: Clear error message from options_parser
```

#### Test 4: Recompress With Thrift
```bash
# Create test image
./build-with-thrift/mkdwarfs -i /tmp/test -o /tmp/test.dwarfs -l 3

# Recompress
./build-with-thrift/mkdwarfs --recompress=all \
  -I /tmp/test.dwarfs -O /tmp/test2.dwarfs
# Expected: ✅ Successfully recompresses
```

#### Test 5: Normal Create Without Thrift
```bash
./build-no-thrift/mkdwarfs -i /tmp/test -o /tmp/test.dwarfs -l 3
# Expected: ✅ Works normally (no Thrift needed for create)
```

---

## Success Criteria

### For Phase 3 Completion (Scenario A/B)

1. ✅ `recompress_handler.h` and `.cpp` created
2. ✅ Recompress logic extracted from mkdwarfs_main.cpp
3. ✅ mkdwarfs builds successfully with `-DDWARFS_WITH_THRIFT=OFF`
4. ✅ Recompress works correctly with `-DDWARFS_WITH_THRIFT=ON`
5. ✅ Clear error message when recompress used without Thrift
6. ✅ mkdwarfs_main.cpp reduced to ~500-600 lines
7. ✅ All tests pass (create, recompress, with/without Thrift)
8. ✅ CMake updated to include handler
9. ✅ Documentation updated

### For Phase 3 Skip (Scenario C)

1. ✅ Document that recompress not in mkdwarfs_main.cpp
2. ✅ Verify options_parser guards sufficient
3. ✅ Update Status doc: Phase 3 = N/A
4. ✅ Proceed to Phase 4

---

## Next Session Start Instructions

### Step 1: Verify Current Commit

```bash
cd /Users/mulgogi/src/external/dwarfs
git status
# Should show: On branch refactor/mkdwarfs-phase1

git log --oneline -3
# Should show f5ec0de0 at top

# Verify build works
cd build && ninja mkdwarfs && ./mkdwarfs --help | head -10
```

### Step 2: Investigate Recompress History

```bash
# Find earliest commit with recompress
git log --all --reverse --oneline --grep="recompress" | head -5

# Check original mkdwarfs before refactoring
git log --all --oneline | grep -B 5 "extract Part A"
# Note the commit BEFORE "extract Part A"

# View that original file
git show <COMMIT>:tools/src/mkdwarfs_main.cpp | wc -l
git show <COMMIT>:tools/src/mkdwarfs_main.cpp | grep -n "rewrite_filesystem" | head -20
```

### Step 3: Determine Action

Based on investigation results:
- **If recompress code found**: Proceed with Scenario A
- **If code was removed**: Proceed with Scenario B  
- **If no code ever existed**: Skip to Phase 4

---

## Quick Reference

### Current State
| File | Lines | Status |
|------|-------|--------|
| mkdwarfs_main.cpp | 686 | ✅ Phases 1&2 complete |
| options_parser.cpp | 766 | ✅ Phase 1 complete |
| create_handler.cpp | 69 | ✅ Phase 2 complete |
| **recompress_handler.cpp** | **0** | **⏳ Phase 3 pending** |

### Build Commands

```bash
# No Thrift build
cmake -B build -GNinja -DDWARFS_WITH_THRIFT=OFF
ninja -C build mkdwarfs

# With Thrift build  
cmake -B build -GNinja -DDWARFS_WITH_THRIFT=ON
ninja -C build mkdwarfs
```

### Git Commands

```bash
git log --all --oneline --grep="recompress"
git show <commit>:tools/src/mkdwarfs_main.cpp | grep -A 20 "rewrite_filesystem"
git diff <before> <after> tools/src/mkdwarfs_main.cpp
```

---

## Risk Assessment

### Low Risk
- ✅ Options parsing already has Thrift guards
- ✅ Build system already supports conditional compilation
- ✅ Phases 1 & 2 provide clean extraction pattern

### Medium Risk
- ⚠️ Recompress code location unknown
- ⚠️ May need restoration from history
- ⚠️ Integration complexity if code scattered

### High Risk
- ❌ None identified

---

## Timeline Estimate

### Investigation Phase
- **Time**: 30-60 minutes
- **Tasks**: Find recompress code, determine scenario
- **Outcome**: Scenario A, B, or C confirmed

### If Scenario A/B (Code Exists/Restore)
- **Time**: 2-3 hours
- **Breakdown**:
  - Code location/restoration: 30 min
  - Handler creation: 45 min
  - Integration + guards: 45 min
  - Testing: 45 min
  - Documentation: 15 min

### If Scenario C (Skip)
- **Time**: 15 minutes
- **Tasks**: Document findings, update status

---

**Next Update**: After investigation phase complete  
**Decision Point**: Which scenario applies  
**Estimated Completion**: 2025-11-25 (Investigation + Scenario A/B if needed)