# mkdwarfs Refactoring - Phase 1 Continuation Prompt

**Date Created**: 2025-11-23
**Branch**: `refactor/mkdwarfs-phase1`
**Current Commit**: `9a42296b`
**Status**: Phase 1 at 30% completion - Architecture established, ready for full extraction

---

## Quick Context

The mkdwarfs_main.cpp file (1587 lines) is being refactored to fix architectural issues:
- **Problem**: Monolithic file violates Single Responsibility Principle
- **Problem**: Recompress functionality requires Thrift, breaking non-Thrift builds
- **Solution**: Extract into modular components using Strategy Pattern

### What Was Done Last Session

✅ Created options parser architecture:
- Directory: `tools/include/dwarfs/tool/mkdwarfs/`
- Header: `options_parser.h` with `parsed_options` struct and `options_parser` class
- Implementation: `options_parser.cpp` with skeleton and validation methods
- Status doc: `doc/MKDWARFS_REFACTORING_STATUS.md`
- Git branch: `refactor/mkdwarfs-phase1` (commit `9a42296b`)

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
1. doc/MKDWARFS_REFACTORING_STATUS.md (current status)
2. doc/MKDWARFS_REFACTORING_CONTINUATION_PLAN.md (original plan)
3. This file (MKDWARFS_PHASE1_CONTINUATION_PROMPT.md)
```

### 3. Verify Git State
```bash
cd /Users/mulgogi/src/external/dwarfs
git status
# Should show: On branch refactor/mkdwarfs-phase1
git log --oneline -1
# Should show: 9a42296b feat(mkdwarfs): add options parser architecture
```

### 4. Review Created Files
```
Read these files that were created:
- tools/include/dwarfs/tool/mkdwarfs/options_parser.h
- tools/src/mkdwarfs/options_parser.cpp
```

---

## Phase 1 Completion Tasks

### Task 1: Fix Typo in options_parser.cpp (5 min)

**File**: `tools/src/mkdwarfs/options_parser.cpp`
**Line**: 146
**Change**: `validate_re compress_requirements` → `validate_recompress_requirements`

### Task 2: Complete Option Definitions (15 min)

**Objective**: Extract ALL option definitions from mkdwarfs_main.cpp into options_parser::parse()

**Source Range**: Lines 502-733 in `tools/src/mkdwarfs_main.cpp`

**What to Extract**:
- Basic options (502-525)
- Advanced options (527-588)
- Filesystem options (590-620)
- Segmenter options (622-644)
- Compressor options (646-665)
- Filter options (667-679)
- Metadata options (681-721)

**Method**: Use read_file to get the exact content, then add to options_parser::parse()

**IMPORTANT**: Keep ALL logic intact - this is pure code movement, not refactoring yet

### Task 3: Extract Option Processing Logic (20 min)

**Objective**: Extract option processing/validation from mkdwarfs_main.cpp

**Source Ranges**:
- Lines 867-886: Default value processing
- Lines 888-1092: Option processing (paths, metadata, etc.)
- Lines 1093-1400: Complex processing (categorizers, etc.)

**Strategy**:
1. Extract in 3 separate commits for safety
2. Test compilation after each extraction
3. Keep validation logic in helper methods

### Task 4: Update mkdwarfs_main.cpp (15 min)

**Objective**: Simplify mkdwarfs_main.cpp to use the new options_parser

**Changes**:
```cpp
// In mkdwarfs_main(), replace option parsing code with:
#include <dwarfs/tool/mkdwarfs/options_parser.h>

// ...
mkdwarfs::options_parser parser;
mkdwarfs::parsed_options opts;

if (auto rc = parser.parse(argc, argv, iol, opts)) {
  return rc;
}

// Then use opts.* instead of local variables
```

### Task 5: Test Compilation (5 min)

```bash
cd /Users/mulgogi/src/external/dwarfs
mkdir -p build && cd build
cmake .. -GNinja -DWITH_TESTS=ON
ninja mkdwarfs
```

**Expected Result**: Clean compilation

**If Errors**: Review error messages, fix incrementally

---

## Critical Safety Rules

### DO NOT:
1. ❌ Edit mkdwarfs_main.cpp directly until options_parser is complete
2. ❌ Make changes without testing compilation
3. ❌ Combine multiple logical changes in one commit
4. ❌ Remove or modify existing logic during extraction

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

Task 1: Fix Typo (5 min)
├─ Fix validate_recompress_requirements typo
├─ Test compilation
└─ Git commit

Task 2: Extract Option Definitions (15 min)
├─ Read lines 502-733 from mkdwarfs_main.cpp
├─ Add to options_parser::parse()
├─ Test compilation
└─ Git commit

Task 3: Extract Option Processing (20 min)
├─ Part A: Default values (867-886)
│  ├─ Extract and test
│  └─ Git commit
├─ Part B: Simple processing (888-1092)
│  ├─ Extract and test
│  └─ Git commit
└─ Part C: Complex processing (1093-1400)
   ├─ Extract and test
   └─ Git commit

Task 4: Update mkdwarfs_main (15 min)
├─ Replace option parsing with parser.parse()
├─ Update variable references
├─ Test compilation
└─ Git commit

Task 5: Final Testing (5 min)
├─ Build mkdwarfs
├─ Test --help
├─ Test --long-help
└─ Update status to Phase 1 Complete

Session End (5 min)
├─ Update MKDWARFS_REFACTORING_STATUS.md
├─ Update memory bank context.md
└─ Mark Phase 1 complete in TODO
```

**Total Time**: ~70 minutes

---

## Expected End State

After completing Phase 1:

```
✅ options_parser.h - Complete with all fields
✅ options_parser.cpp - Complete implementation (~400 lines)
✅ mkdwarfs_main.cpp - Simplified by ~300 lines
✅ Compilation - Clean build
✅ Tests - --help and --long-help working
✅ Git - 5-6 commits on refactor/mkdwarfs-phase1
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
- `tools/include/dwarfs/tool/mkdwarfs/options_parser.h`
- `tools/src/mkdwarfs/options_parser.cpp`

**To Modify**:
- `tools/src/mkdwarfs_main.cpp` (will simplify from 1587 → ~1250 lines)

**Status Docs**:
- `doc/MKDWARFS_REFACTORING_STATUS.md` (this session's detailed status)
- `doc/MKDWARFS_REFACTORING_CONTINUATION_PLAN.md` (original plan)

**Memory Bank**:
- `.kilocode/rules/memory-bank/context.md` (updated with current work)

---

## Success Criteria

Phase 1 is complete when:

1. ✅ options_parser.cpp has full implementation
2. ✅ mkdwarfs_main.cpp uses options_parser
3. ✅ `ninja mkdwarfs` builds successfully
4. ✅ `./mkdwarfs --help` works
5. ✅ `./mkdwarfs --long-help` works
6. ✅ All changes committed to git
7. ✅ Status documents updated
8. ✅ Ready to start Phase 2

---

**Last Updated**: 2025-11-23 23:00 HKT
**Estimated Time to Complete Phase 1**: 40-70 minutes
**Current Progress**: 30% → targeting 100%
// ... existing code ...