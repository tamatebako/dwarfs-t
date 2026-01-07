# Session 50: Phase 5 (Testing & Validation) - Continuation Plan

**Date Created**: 2025-12-28
**Status**: Ready After Phase 3 Fixes
**Priority**: HIGH - Complete Session 50
**Estimated Duration**: 0.5 days

---

## Phase 4 Completion Summary

**Completed** ✅:
- dwarfs FUSE driver migrated to argtable3 (622 lines parser)
- All 4 tools now use argtable3: mkdwarfs, dwarfsck, dwarfsextract, **dwarfs**
- Environment variable support across all tools
- Clean architecture with tool_support library

**Blocking Issues** ⚠️ (Pre-existing from Phases 1-3):
1. mkdwarfs headers reference old `options_parser.h` (should be `parsed_options.h`)
2. dwarfsextract perfmon conditional compilation (guards exist but cache issue)
3. fuse_driver.cpp issues when PERFMON disabled (pre-existing)

---

## Phase 5 Scope: Testing & Validation

**Goal**: Verify all 4 tools work correctly with argtable3 and --version support

### Pre-requisites (Must Fix First)

#### Fix 1: mkdwarfs Headers (5 min)

**Files to Update**:
```cpp
// include/dwarfs/tool/mkdwarfs/create_handler.h
// include/dwarfs/tool/mkdwarfs/recompress_handler.h

// Change:
#include <dwarfs/tool/mkdwarfs/options_parser.h>

// To:
#include <dwarfs/tool/mkdwarfs/parsed_options.h>
```

### Testing Matrix

#### Test 1: Build Validation (30 min)

**All Configurations**:
```bash
# FlatBuffers-only (most common)
cmake -B build-fb -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF -DWITH_FUSE_DRIVER=ON
cmake --build build-fb -j8

# Both formats
cmake -B build-both -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON -DWITH_FUSE_DRIVER=ON
cmake --build build-both -j8

# Thrift-only (should work)
cmake -B build-thrift -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON -DWITH_FUSE_DRIVER=ON
cmake --build build-thrift -j8
```

#### Test 2: --version Flag (15 min)

All 4 tools:
```bash
./build-fb/mkdwarfs --version
./build-fb/dwarfsck --version
./build-fb/dwarfsextract --version
./build-fb/dwarfs --version
```

Expected output format:
```
<tool> <version> (<git-rev>, <git-date>)
Built with:
  - <dependencies>
```

#### Test 3: --help Flag (15 min)

All 4 tools:
```bash
./build-fb/mkdwarfs --help
./build-fb/dwarfsck --help
./build-fb/dwarfsextract --version  
./build-fb/dwarfs --help
```

Verify: Clean argtable3 help output, all options listed

#### Test 4: --man Flag (15 min)

All 4 tools (if WITH_MAN_OPTION=ON):
```bash
./build-fb/mkdwarfs --man
./build-fb/dwarfsck --man
./build-fb/dwarfsextract --man
./build-fb/dwarfs --man
```

Verify: Manpage displays correctly

#### Test 5: Environment Variables (30 min)

**mkdwarfs**:
```bash
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=6
./build-fb/mkdwarfs -i /tmp/test -o /tmp/test.dff
# Verify level 6 used
```

**dwarfsck**:
```bash
export DWARFS_DWARFSCK_LOG_LEVEL=debug
./build-fb/dwarfsck /tmp/test.dff
# Verify debug output
```

**dwarfsextract**:
```bash
export DWARFS_DWARFSEXTRACT_NUM_WORKERS=8
./build-fb/dwarfsextract -i /tmp/test.dff -o /tmp/out
# Verify 8 workers used
```

**dwarfs**:
```bash
export DWARFS_DWARFS_CACHE_SIZE=1g
export DWARFS_DWARFS_NUM_WORKERS=8
./build-fb/dwarfs /tmp/test.dff /tmp/mnt
# Verify 1GB cache, 8 workers
umount /tmp/mnt
```

#### Test 6: Full Workflow (30 min)

```bash
# Create image
./build-fb/mkdwarfs -i /usr/local/share/doc -o /tmp/docs.dff -l7

# Check image
./build-fb/dwarfsck /tmp/docs.dff -d all

# Mount image
mkdir -p /tmp/mnt
./build-fb/dwarfs /tmp/docs.dff /tmp/mnt
ls -la /tmp/mnt
umount /tmp/mnt

# Extract image
./build-fb/dwarfsextract -i /tmp/docs.dff -o /tmp/extracted
diff -r /usr/local/share/doc /tmp/extracted
```

#### Test 7: MECE Priority (15 min)

Verify: CLI > ENV > defaults

```bash
# Set ENV
export DWARFS_DWARFS_CACHE_SIZE=512m

# Override with CLI
./build-fb/dwarfs /tmp/test.dff /tmp/mnt --cachesize=1g
# Should use 1g (CLI wins)
```

---

## Success Criteria for Phase 5

### All Tools
- [ ] Build without errors (FlatBuffers-only, both formats)
- [ ] `--version` works and shows correct format
- [ ] `--help` shows all options
- [ ] `--man` displays manpage (if enabled)
- [ ] Environment variables work
- [ ] MECE priority verified (CLI > ENV > defaults)

### Platform Testing
- [ ] macOS (FUSE-T): All tests pass
- [ ] Linux (FUSE3): (CI will verify)
- [ ] Windows (WinFsp): (CI will verify)

---

## Timeline

| Task | Duration |
|------|----------|
| Fix mkdwarfs headers | 5 min |
| Build validation (3 configs) | 30 min |
| --version tests (4 tools) | 15 min |
| --help tests (4 tools) | 15 min |
| --man tests (4 tools) | 15 min |
| ENV variable tests (4 tools) | 30 min |
| Full workflow test | 30 min |
| MECE priority test | 15 min |
| **Total** | **2.5 hours (0.3 days)** |

---

## After Phase 5

### Phase 6: Documentation & Cleanup (0.5 days)
- Update README.adoc
- Create ENVIRONMENT_VARIABLES.md
- Update tool manpages
- Archive planning docs to old-docs/
- Create Session 50 completion summary

---

**Last Updated**: 2025-12-28 19:07 HKT
**Status**: Phase 4 complete, waiting for Phase 3 fixes
**Next**: Fix blocking issues, then proceed with Phase 5 testing