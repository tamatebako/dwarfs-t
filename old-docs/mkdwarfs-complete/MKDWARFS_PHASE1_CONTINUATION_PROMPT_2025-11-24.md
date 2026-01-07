# mkdwarfs Refactoring - Phase 1 Continuation Prompt

**Date Created**: 2025-11-24 10:30 HKT
**Branch**: `refactor/mkdwarfs-phase1`
**Current Commit**: `c2f90729`
**Status**: Phase 1 at 50% completion - Option definitions extracted, ready for processing logic

---

## Quick Context

The mkdwarfs_main.cpp file (1578 lines) is being refactored to separate concerns and fix linking issues when building without Thrift support. Phase 1 extracts option parsing into a separate options_parser module.

### What Was Completed (First 50%)

✅ Created architecture files:
- `tools/include/dwarfs/tool/mkdwarfs/options_parser.h` (159 lines) - Complete API
- `tools/src/mkdwarfs/options_parser.cpp` (380 lines) - Partial implementation

✅ Extracted from mkdwarfs_main.cpp:
- All option definitions (lines 502-733) → options_parser::parse()
- Validation helper methods
- Format detection logic
- Fixed typo: `validate_recompress_requirements`

✅ Git commit: `c2f90729` on branch `refactor/mkdwarfs-phase1`

### What Remains (Second 50%)

⏳ Extract option processing logic (lines 867-1400 from mkdwarfs_main.cpp)
⏳ Update mkdwarfs_main.cpp to use options_parser
⏳ Add options_parser.cpp to CMake build system
⏳ Test compilation
⏳ Update status documents
⏳ Final commit for Phase 1

---

## Session Start Checklist

When you start the next session, **before writing any code**, do these steps:

### 1. Read Memory Bank Files
```
Read ALL files in .kilocode/rules/memory-bank/:
- brief.md
- context.md
- architecture.md
- product.md
- tech.md
```

### 2. Read Status Documents
```
Read these documents in order:
1. doc/MKDWARFS_REFACTORING_STATUS.md (overall status)
2. doc/MKDWARFS_REFACTORING_CONTINUATION_PLAN.md (architecture plan)
3. This file (MKDWARFS_PHASE1_CONTINUATION_PROMPT_2025-11-24.md)
```

### 3. Verify Git State
```bash
cd /Users/mulgogi/src/external/dwarfs
git status
# Should show: On branch refactor/mkdwarfs-phase1
git log --oneline -3
# Should show: c2f90729 feat(mkdwarfs): extract option definitions...
```

### 4. Review Created Files
```
Read these files to understand current state:
- tools/include/dwarfs/tool/mkdwarfs/options_parser.h
- tools/src/mkdwarfs/options_parser.cpp
```

---

## Remaining Tasks for Phase 1

### Task 1: Extract Option Processing Logic (~30 min)

**Objective**: Extract option processing/validation from mkdwarfs_main.cpp (lines 867-1400)

**Source Sections** (from mkdwarfs_main.cpp):
1. Lines 867-886: Default value processing
2. Lines 888-1092: Option processing (paths, metadata, etc.)
3. Lines 1093-1400: Complex processing (categorizers, etc.)

**Target**: Add to `options_parser::parse()` in `tools/src/mkdwarfs/options_parser.cpp`

**Strategy**:
1. Read lines 867-1400 from mkdwarfs_main.cpp
2. Identify which logic belongs in parse()
3. Extract in 3 sub-commits:
   - Part A: Default values (867-886)
   - Part B: Simple processing (888-1092)
   - Part C: Complex processing (1093-1400)
4. Test compilation after each sub-commit

**IMPORTANT**:
- Keep ALL logic intact - this is pure code movement
- Don't refactor or optimize - just move
- Preserve variable names and logic flow
- Use `edit_file` tool for safe editing

### Task 2: Update mkdwarfs_main.cpp (~15 min)

**Objective**: Simplify mkdwarfs_main.cpp to use the new options_parser

**Changes to make**:
```cpp
// At top of file, add:
#include <dwarfs/tool/mkdwarfs/options_parser.h>

// In mkdwarfs_main(), replace option parsing code (lines 429-1400) with:
mkdwarfs::options_parser parser;
mkdwarfs::parsed_options opts;

if (auto rc = parser.parse(argc, argv, iol, opts)) {
  return rc;
}

// Then use opts.* instead of local variables throughout
```

**Steps**:
1. Read mkdwarfs_main.cpp to identify all variable usages
2. Create mapping: local_var → opts.field
3. Use `edit_file` to update mkdwarfs_main.cpp
4. Verify logic flow unchanged

### Task 3: Add to CMake Build System (~10 min)

**File to modify**: `CMakeLists.txt` or create `cmake/tools.cmake`

**Changes needed**:
```cmake
# Add mkdwarfs_lib target if it doesn't exist
add_library(mkdwarfs_lib OBJECT
    tools/src/mkdwarfs/options_parser.cpp
)

target_link_libraries(mkdwarfs_lib PRIVATE
    dwarfs_common
    dwarfs_writer
    Boost::program_options
)

# Update mkdwarfs target to link mkdwarfs_lib
target_link_libraries(mkdwarfs PRIVATE
    mkdwarfs_lib
    dwarfs_writer
    dwarfs_reader
)
```

### Task 4: Test Compilation (~5 min)

```bash
cd /Users/mulgogi/src/external/dwarfs
mkdir -p build && cd build
cmake .. -GNinja -DWITH_TESTS=OFF
ninja mkdwarfs
```

**Expected Result**: Clean compilation

**If Errors**: Review error messages, fix incrementally

### Task 5: Update Status Documents (~5 min)

**Files to update**:
1. `doc/MKDWARFS_REFACTORING_STATUS.md`
   - Mark Phase 1 as 100% complete
   - Update completion date
   - Document final file sizes

2. `.kilocode/rules/memory-bank/context.md`
   - Update "Current Work" section
   - Mark Phase 1 complete
   - Note next phase

### Task 6: Final Commit

```bash
cd /Users/mulgogi/src/external/dwarfs
git add -A
git commit -m "feat(mkdwarfs): complete Phase 1 - options parser extraction

- Extract all option definitions to options_parser.h/cpp
- Extract option processing logic
- Update mkdwarfs_main.cpp to use options_parser
- Add options_parser to CMake build system
- ~337 lines removed from mkdwarfs_main.cpp
- Phase 1 of 7 complete"
```

---

## Critical Safety Rules

### DO NOT:
1. ❌ Edit mkdwarfs_main.cpp before options_parser is complete
2. ❌ Make changes without testing compilation
3. ❌ Combine multiple logical changes in one commit
4. ❌ Remove or modify existing logic during extraction
5. ❌ Refactor or optimize - just move code

### DO:
1. ✅ Read ALL context documents before starting
2. ✅ Make small, incremental commits
3. ✅ Test compilation after each major change
4. ✅ Keep extracted code identical to source
5. ✅ Update MKDWARFS_REFACTORING_STATUS.md as you progress

---

## Recommended Session Flow

```
Session Start (5 min)
├─ Read memory bank files
├─ Read status documents
├─ Verify git state
└─ Review created files

Task 1: Extract Option Processing (30 min)
├─ Part A: Default values (867-886)
│  ├─ Extract and test
│  └─ Git commit
├─ Part B: Simple processing (888-1092)
│  ├─ Extract and test
│  └─ Git commit
└─ Part C: Complex processing (1093-1400)
   ├─ Extract and test
   └─ Git commit

Task 2: Update mkdwarfs_main (15 min)
├─ Replace option parsing with parser.parse()
├─ Update variable references
├─ Test compilation
└─ Git commit

Task 3: Add to CMake (10 min)
├─ Update CMakeLists.txt
├─ Test compilation
└─ Git commit

Task 4: Test Compilation (5 min)
├─ Build mkdwarfs
├─ Test --help
└─ Test --long-help

Task 5: Update Status (5 min)
├─ Update MKDWARFS_REFACTORING_STATUS.md
├─ Update memory bank context.md
└─ Mark Phase 1 complete in TODO

Task 6: Final Commit (5 min)
└─ Commit Phase 1 completion

Session End (5 min)
├─ Verify all tests pass
├─ Update status to Phase 1 Complete
└─ Ready to start Phase 2
```

**Total Time**: ~80 minutes

---

## Expected End State

After completing Phase 1:

```
✅ options_parser.h - Complete with all fields (159 lines)
✅ options_parser.cpp - Complete implementation (~700 lines)
✅ mkdwarfs_main.cpp - Simplified by ~337 lines (1578 → ~1241 lines)
✅ CMakeLists.txt - Updated with mkdwarfs_lib target
✅ Compilation - Clean build
✅ Tests - --help and --long-help working
✅ Git - 4-5 commits on refactor/mkdwarfs-phase1
✅ Status - Phase 1 marked complete, ready for Phase 2
```

---

## Error Recovery

If compilation fails:

```bash
# Step 1: Review the error
ninja mkdwarfs 2>&1 | tee build-error.log

# Step 2: If unfixable, revert last commit
git log --oneline -5  # Review recent commits
git reset --hard HEAD~1  # Revert one commit

# Step 3: Try again with smaller changes
# Make smaller, more focused commits
```

---

## Next Phase Preview

After Phase 1 completes, Phase 2 will:
- Create `create_handler.h` and `create_handler.cpp`
- Extract filesystem creation logic (lines 1500-1515)
- Move scanner setup code (~100 lines)
- Keep recompress code in mkdwarfs_main.cpp for now

---

## Key Files Reference

**Created Files**:
- `tools/include/dwarfs/tool/mkdwarfs/options_parser.h` (159 lines)
- `tools/src/mkdwarfs/options_parser.cpp` (380 → ~700 lines after completion)

**To Modify**:
- `tools/src/mkdwarfs_main.cpp` (1578 → ~1241 lines after completion)
- `CMakeLists.txt` or `cmake/tools.cmake` (add mkdwarfs_lib target)

**Status Docs**:
- `doc/MKDWARFS_REFACTORING_STATUS.md` (update completion status)
- `doc/MKDWARFS_REFACTORING_CONTINUATION_PLAN.md` (original plan)
- `.kilocode/rules/memory-bank/context.md` (update current work)

---

## Success Criteria

Phase 1 is complete when:

1. ✅ options_parser.cpp has full implementation (~700 lines)
2. ✅ mkdwarfs_main.cpp uses options_parser (~1241 lines)
3. ✅ `ninja mkdwarfs` builds successfully
4. ✅ `./mkdwarfs --help` works
5. ✅ `./mkdwarfs --long-help` works
6. ✅ All changes committed to git (4-5 commits)
7. ✅ Status documents updated
8. ✅ Ready to start Phase 2

---

**Last Updated**: 2025-11-24 10:30 HKT
**Estimated Time to Complete Phase 1**: 80 minutes
**Current Progress**: 50% → targeting 100%
**Current Commit**: `c2f90729`
**Next Commit**: Option processing logic extraction