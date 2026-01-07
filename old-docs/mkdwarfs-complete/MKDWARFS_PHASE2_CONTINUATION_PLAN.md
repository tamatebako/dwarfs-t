# mkdwarfs Refactoring - Phase 2 Continuation Plan

**Date Created**: 2025-11-24 10:58 HKT
**Branch**: `refactor/mkdwarfs-phase1` (will be merged or continued)
**Current Commit**: `c08166b0`
**Phase 1 Status**: ✅ COMPLETE
**Phase 2 Status**: ⏳ READY TO START

---

## Quick Context

Phase 1 successfully extracted option parsing from mkdwarfs_main.cpp (716 lines removed) into a clean options_parser module. Phase 2 will extract filesystem creation logic into a create_handler module.

## Session Start Checklist

When you start the Phase 2 session, **before writing any code**, do these steps:

### 1. Read Memory Bank Files
```
Read ALL files in .kilocode/rules/memory-bank/:
- brief.md (project overview)
- context.md (current status - Phase 1 complete)
- architecture.md (system architecture)
- product.md (project goals)
- tech.md (technical stack)
```

### 2. Read Status Documents
```
Read these documents in order:
1. doc/MKDWARFS_REFACTORING_STATUS.md (Phase 1 completion status)
2. This file (MKDWARFS_PHASE2_CONTINUATION_PLAN.md)
```

### 3. Verify Git State
```bash
cd /Users/mulgogi/src/external/dwarfs
git status
# Should show: On branch refactor/mkdwarfs-phase1
git log --oneline -3
# Should show: c08166b0 docs(mkdwarfs): mark Phase 1 complete...
```

### 4. Review Phase 1 Results
```bash
# View the changes from Phase 1
git diff HEAD~7 --stat

# Expected output:
# - options_parser.h created (159 lines)
# - options_parser.cpp created (619 lines)
# - mkdwarfs_main.cpp reduced by 716 net lines
# - cmake/tools.cmake updated (3 lines)
```

---

## Phase 2 Overview: Extract Create Handler

### Objective
Extract filesystem creation logic from mkdwarfs_main.cpp into a dedicated create_handler module, continuing the separation of concerns.

### What to Extract
From mkdwarfs_main.cpp (lines ~1500-1515 and surrounding setup):
1. Scanner setup and configuration
2. Segmenter factory initialization
3. Entry factory setup
4. Thread pool creation (scanner pool)
5. Writer scanner instantiation
6. Filter attachment
7. Filesystem scan execution

### What to Keep in Main
- Console writer setup (runtime object)
- Progress updater (runtime object)
- Filesystem writer setup (runtime object)
- Output stream handling
- Categorizer manager setup (complex runtime logic)
- Recompress infrastructure (will be Phase 3)

---

## Phase 2 Architecture

### Target Structure
```
tools/
├── include/dwarfs/tool/mkdwarfs/
│   ├── options_parser.h          ✅ Phase 1 - DONE
│   └── create_handler.h          ⏳ Phase 2 - TODO
├── src/mkdwarfs/
│   ├── options_parser.cpp        ✅ Phase 1 - DONE
│   └── create_handler.cpp        ⏳ Phase 2 - TODO
└── src/
    └── mkdwarfs_main.cpp         📝 Phase 2 - SIMPLIFY FURTHER
```

### create_handler.h Design
```cpp
#pragma once

#include <dwarfs/writer/scanner_options.h>
#include <dwarfs/writer/segmenter_factory.h>
#include <dwarfs/tool/mkdwarfs/options_parser.h>

namespace dwarfs::tool::mkdwarfs {

class create_handler {
 public:
  create_handler() = default;

  /**
   * Execute filesystem creation
   *
   * @param opts Parsed command-line options
   * @param iol I/O layer for file access
   * @param lgr Logger
   * @param prog Writer progress tracker
   * @param fsw Filesystem writer
   * @param extra_deps Library dependencies callback
   * @return 0 on success, error code otherwise
   */
  int run(
      parsed_options const& opts,
      iolayer const& iol,
      logger& lgr,
      writer::writer_progress& prog,
      writer::filesystem_writer& fsw,
      std::function<void(library_dependencies&)> const& extra_deps);

 private:
  // Helper methods if needed
};

} // namespace dwarfs::tool::mkdwarfs
```

### create_handler.cpp Implementation Plan
```cpp
#include <dwarfs/tool/mkdwarfs/create_handler.h>

// All necessary includes...

namespace dwarfs::tool::mkdwarfs {

int create_handler::run(
    parsed_options const& opts,
    iolayer const& iol,
    logger& lgr,
    writer::writer_progress& prog,
    writer::filesystem_writer& fsw,
    std::function<void(library_dependencies&)> const& extra_deps) {

  // 1. Create segmenter factory
  writer::segmenter_factory sf(lgr, prog, opts.scanner_opts.inode.categorizer_mgr,
                               opts.segmenter_config);

  // 2. Create entry factory
  writer::entry_factory ef;

  // 3. Create scanner thread pool
  thread_pool scanner_pool(lgr, *iol.os, "scanner", opts.num_scanner_workers);

  // 4. Create scanner
  writer::scanner s(lgr, scanner_pool, sf, ef, *iol.os, opts.scanner_opts);

  // 5. Add filters if present (handled in main via rule_filter)
  // This will be passed in as a parameter

  // 6. Execute scan
  s.scan(fsw, opts.input_path, prog, opts.input_list, iol.file, extra_deps);

  return 0;
}

} // namespace dwarfs::tool::mkdwarfs
```

---

## Phase 2 Implementation Steps

### Step 1: Create create_handler.h (15 min)
```bash
# Create the header file
touch tools/include/dwarfs/tool/mkdwarfs/create_handler.h

# Implement the class definition
# - Add necessary includes
# - Define create_handler class
# - Define run() method signature
# - Add documentation
```

**Key Points**:
- Use `parsed_options` from options_parser
- Take runtime objects as parameters (don't create them)
- Return int for error codes
- Keep interface clean and simple

### Step 2: Create create_handler.cpp (30 min)
```bash
# Create the implementation file
touch tools/src/mkdwarfs/create_handler.cpp

# Implement run() method
# Extract from mkdwarfs_main.cpp lines ~1500-1515
```

**What to Extract**:
1. Segmenter factory creation (~line 1501)
2. Entry factory creation (~line 1504)
3. Scanner pool creation (~line 1506)
4. Scanner instantiation (~line 1508)
5. Filter attachment (~line 1510-1512)
6. Scan execution (~line 1514)

**What NOT to Extract** (stays in main):
- rule_filter creation (complex setup, line 1021-1039)
- console_writer setup (line 1014)
- filesystem_writer setup (line 1420-1483)
- progress object (line 1200)

### Step 3: Update mkdwarfs_main.cpp (20 min)
```cpp
// Add include at top
#include <dwarfs/tool/mkdwarfs/create_handler.h>

// In mkdwarfs_main(), after filesystem_writer setup:
create_handler handler;
return handler.run(opts, iol, lgr, prog, *fsw, extra_deps);
```

**Changes**:
- Add include for create_handler
- Replace lines 1500-1515 with handler.run() call
- Pass necessary runtime objects as parameters
- Expected reduction: ~100-150 lines

### Step 4: Add to CMake (5 min)
```cmake
# In cmake/tools.cmake, after options_parser.cpp:
target_sources(mkdwarfs_main PRIVATE
  tools/src/mkdwarfs/options_parser.cpp
  tools/src/mkdwarfs/create_handler.cpp
)
```

### Step 5: Test Compilation (10 min)
```bash
cd build
cmake .. -GNinja -DWITH_TESTS=OFF -DWITH_FUSE_DRIVER=OFF
ninja mkdwarfs

# Note: May still hit metadata_v2_flatbuffers.cpp errors (pre-existing)
# Our code should compile successfully before that
```

### Step 6: Update Documentation (10 min)
1. Update `doc/MKDWARFS_REFACTORING_STATUS.md`:
   - Mark Phase 2 complete
   - Document lines removed
   - Add file list

2. Update `.kilocode/rules/memory-bank/context.md`:
   - Update current work status
   - Mark Phase 2 complete
   - Add next steps for Phase 3

### Step 7: Commit Changes (5 min)
```bash
git add -A
git commit -m "feat(mkdwarfs): extract create_handler (Phase 2 complete)

- Create create_handler.h and create_handler.cpp
- Extract filesystem creation logic from mkdwarfs_main
- Reduce mkdwarfs_main.cpp by ~100-150 lines
- Add create_handler to CMake build system
- Phase 2 of 7 complete"
```

---

## Critical Guidelines

### DO:
1. ✅ Read ALL memory bank files first
2. ✅ Make small, incremental commits
3. ✅ Test compilation after each major change
4. ✅ Keep extracted code identical to source
5. ✅ Update documentation as you go

### DON'T:
1. ❌ Extract runtime object creation (console_writer, progress, etc.)
2. ❌ Extract categorizer manager setup (too complex for Phase 2)
3. ❌ Extract recompress logic (that's Phase 3)
4. ❌ Refactor or optimize during extraction
5. ❌ Make changes without testing compilation

---

## Key Code Locations

### Files to Read
1. `tools/src/mkdwarfs_main.cpp` - Lines 1500-1515 (scan execution)
2. `tools/include/dwarfs/tool/mkdwarfs/options_parser.h` - For parsed_options
3. `include/dwarfs/writer/scanner.h` - Scanner API
4. `include/dwarfs/writer/segmenter_factory.h` - Segmenter API

### Files to Create
1. `tools/include/dwarfs/tool/mkdwarfs/create_handler.h` (NEW)
2. `tools/src/mkdwarfs/create_handler.cpp` (NEW)

### Files to Modify
1. `tools/src/mkdwarfs_main.cpp` (simplify further)
2. `cmake/tools.cmake` (add create_handler.cpp)
3. `doc/MKDWARFS_REFACTORING_STATUS.md` (update status)
4. `.kilocode/rules/memory-bank/context.md` (update context)

---

## Expected Results

After Phase 2 completion:

### Line Count Changes
```
mkdwarfs_main.cpp:  862 lines → ~712-762 lines (-100 to -150 lines)
create_handler.h:   NEW → ~80-100 lines
create_handler.cpp: NEW → ~150-200 lines
```

### Total Progress
```
Phase 1: Options Parser     [██████████] 100% ✅ COMPLETE
Phase 2: Create Handler     [██████████] 100% ✅ COMPLETE (target)
Phase 3: Recompress Handler [░░░░░░░░░░]   0%
Phase 4: Handler Factory    [░░░░░░░░░░]   0%
Phase 5: Simplify Main      [░░░░░░░░░░]   0%
Phase 6: Update Build       [░░░░░░░░░░]   0%
Phase 7: Test & Document    [░░░░░░░░░░]   0%
```

### Git State
- Branch: `refactor/mkdwarfs-phase1` (or `refactor/mkdwarfs-phase2`)
- Commits: 8-10 total (7 from Phase 1 + 1-3 from Phase 2)
- Ready for Phase 3

---

## Troubleshooting

### If Compilation Fails in create_handler.cpp
1. Check includes - make sure all necessary headers are included
2. Check namespace - should be `dwarfs::tool::mkdwarfs`
3. Check parameter types - match exactly with mkdwarfs_main.cpp
4. Check that you're not creating objects that should be passed in

### If Compilation Fails in mkdwarfs_main.cpp
1. Verify include for create_handler.h is present
2. Check that handler.run() call matches signature
3. Verify all parameters are available in scope
4. Check that removed code wasn't used elsewhere

### If CMake Fails
1. Verify create_handler.cpp path is correct in cmake/tools.cmake
2. Run `cmake .. -GNinja` again to regenerate
3. Check that both options_parser and create_handler are in target_sources

---

## Success Criteria

Phase 2 is complete when:

1. ✅ create_handler.h created with clean API (~80-100 lines)
2. ✅ create_handler.cpp created with scan logic (~150-200 lines)
3. ✅ mkdwarfs_main.cpp simplified by ~100-150 lines
4. ✅ CMake build system updated
5. ✅ Code compiles (or fails in pre-existing metadata_v2_flatbuffers.cpp)
6. ✅ Documentation updated (status docs and memory bank)
7. ✅ Changes committed to git (1-3 commits)
8. ✅ Ready for Phase 3

---

## Phase 3 Preview

After Phase 2, Phase 3 will:
- Extract recompress logic into `recompress_handler.h/cpp`
- Separate Thrift-dependent code
- Make it possible to build without Thrift
- Estimated time: 2-3 hours

---

**Last Updated**: 2025-11-24 10:58 HKT
**Estimated Time for Phase 2**: 1.5-2 hours
**Dependencies**: Phase 1 complete (options_parser)
**Blockers**: None (pre-existing build issues unrelated)