# Session 50: Phase 2 (mkdwarfs) - Continuation Plan

**Date Created**: 2025-12-28
**Status**: Phase 2 Partially Complete - Linking Issue
**Priority**: HIGH - Compressed timeline

---

## Current Status Summary

### Completed ✅
1. **argtable3 Infrastructure** (Phase 1) - COMPLETE
2. **mkdwarfs Parser Implementation** - Code complete, linking blocked

### Blocking Issue ⚠️
**Linker Error**: mkdwarfs executable doesn't link `dwarfs_tool_support` library

```
Undefined symbols:
  dwarfs::tool::mkdwarfs::argtable3_options_parser::*
```

**Root Cause**: CMakeLists.txt doesn't define mkdwarfs executable correctly (needs investigation)

---

## Immediate Next Steps (Phase 2 Completion)

### Step 1: Fix CMake Linking (30 minutes)

**Investigate**:
```bash
cd /Users/mulgogi/src/external/dwarfs
grep -r "mkdwarfs" CMakeLists.txt cmake/ | grep -E "add_executable|target_link"
```

**Expected Fix**: Add `dwarfs_tool_support` to mkdwarfs target_link_libraries

**Files to Check**:
- `CMakeLists.txt` (main build file)
- `cmake/*.cmake` (modular build files)

**Likely Solution**:
```cmake
target_link_libraries(mkdwarfs
  PRIVATE
    dwarfs_tool_support  # <-- ADD THIS
    # ... other libraries
)
```

### Step 2: Test Build (15 minutes)

```bash
cd /Users/mulgogi/src/external/dwarfs
rm -rf build-test
cmake -B build-test -DWITH_TOOLS=ON -DWITH_TESTS=OFF
cmake --build build-test --target mkdwarfs -j8
```

### Step 3: Test --version (5 minutes)

```bash
./build-test/mkdwarfs --version
```

**Expected Output**:
```
mkdwarfs 0.14.1
DwarFS version X.Y.Z
...libraries...
```

### Step 4: Test --help (5 minutes)

```bash
./build-test/mkdwarfs --help | head -50
```

**Verify**: All 80+ options displayed correctly

### Step 5: Test --man (5 minutes)

```bash
./build-test/mkdwarfs --man | head -100
```

**Verify**: Manpage displays correctly

### Step 6: Functional Testing (30 minutes)

**Test 1**: Basic filesystem creation
```bash
mkdir -p /tmp/test-input
echo "hello world" > /tmp/test-input/test.txt
./build-test/mkdwarfs -i /tmp/test-input -o /tmp/test.dff -l 3
```

**Test 2**: Environment variables
```bash
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
./build-test/mkdwarfs -i /tmp/test-input -o /tmp/test-env.dff
# Should use level 5 from ENV

./build-test/mkdwarfs -i /tmp/test-input -o /tmp/test-cli.dff -l 7
# Should use level 7 (CLI overrides ENV)
```

**Test 3**: All option categories
- Input/Output options (5)
- Compression options (15)
- Segmenter options (10)
- Threading options (5)
- Metadata options (15)
- Filter options (5)
- Advanced options (25)

### Step 7: Cleanup Old Parser (15 minutes)

**Archive**:
```bash
mkdir -p old-docs/session-50/boost-program-options/mkdwarfs
mv tools/include/dwarfs/tool/mkdwarfs/options_parser.h \
   old-docs/session-50/boost-program-options/mkdwarfs/
mv tools/src/mkdwarfs/options_parser.cpp \
   old-docs/session-50/boost-program-options/mkdwarfs/
```

**Update CMake**:
Remove `tools/src/mkdwarfs/options_parser.cpp` from `cmake/tool_support.cmake`

---

## Remaining Phases (After Phase 2)

### Phase 3: dwarfsck & dwarfsextract (1 day)

**Files to Create** (4 files):
1. `include/dwarfs/tool/dwarfsck/argtable3_options_parser.h` (~120 lines)
2. `tools/src/dwarfsck/argtable3_options_parser.cpp` (~250 lines)
3. `include/dwarfs/tool/dwarfsextract/argtable3_options_parser.h` (~150 lines)
4. `tools/src/dwarfsextract/argtable3_options_parser.cpp` (~300 lines)

**Pattern**: Follow mkdwarfs implementation (simpler  tools, fewer options)

### Phase 4: dwarfs FUSE Driver (1.5 days)

**Complexity**: FUSE integration requires special handling

**Files to Create** (2 files):
1. `include/dwarfs/tool/dwarfs/argtable3_options_parser.h` (~180 lines)
2. `tools/src/dwarfs/argtable3_options_parser.cpp` (~450 lines)

**Challenge**: Bridge argtable3 → FUSE options

### Phase 5: Testing & Validation (0.5 days)

**Test Matrix**:
- All 4 tools: --version, --help, --man
- All existing tests pass
- Environment variables work
- CLI > ENV > defaults (MECE)

### Phase 6: Documentation & Cleanup (0.5 days)

**Update**:
- README.md
- doc/mkdwarfs.md
- doc/dwarfs.md
- doc/dwarfsck.md
- doc/dwarfsextract.md
- Create doc/ENVIRONMENT_VARIABLES.md

**Archive**:
- Move planning docs to old-docs/session-50/
- Create completion summary

---

## Files Modified So Far

### Created (3 files):
1. `include/dwarfs/tool/mkdwarfs/argtable3_options_parser.h` (221 lines)
2. `tools/src/mkdwarfs/argtable3_options_parser.cpp` (903 lines)
3. `cmake/tool_support.cmake` (updated)

### Modified (1 file):
1. `tools/src/mkdwarfs_main.cpp` (parser integration)

### To Archive (2 files):
1. `tools/include/dwarfs/tool/mkdwarfs/options_parser.h` (old parser)
2. `tools/src/mkdwarfs/options_parser.cpp` (old parser)

---

## Success Criteria

### Phase 2 Complete When:
- ✅ mkdwarfs builds successfully
- ✅ `--version` displays correctly
- ✅ `--help` shows all 80+ options
- ✅ `--man` displays manpage
- ✅ Filesystem creation works
- ✅ Environment variables work
- ✅ CLI overrides ENV (MECE verified)
- ✅ All existing tests pass
- ✅ Old parser archived

### All Phases Complete When:
- ✅ All 4 tools use argtable3 (no boost::program_options)
- ✅ All tools have `--version` support
- ✅ All tools support environment variables
- ✅ MECE priority enforced everywhere (CLI > ENV > defaults)
- ✅ Documentation updated
- ✅ Old code archived
- ✅ Completion summary created

---

## Timeline (Compressed)

| Phase | Duration | Tasks |
|-------|----------|-------|
| Phase 2 Remaining | 2 hours | Fix linking, test, cleanup |
| Phase 3 | 1 day | dwarfsck & dwarfsextract |
| Phase 4 | 1.5 days | dwarfs FUSE driver |
| Phase 5 | 0.5 days | Testing |
| Phase 6 | 0.5 days | Documentation |
| **Total** | **3.5 days** | **From current state** |

---

**Last Updated**: 2025-12-28 13:46 HKT
**Blocked On**: CMake linking investigation
**Next Action**: Find mkdwarfs target definition and add dwarfs_tool_support link