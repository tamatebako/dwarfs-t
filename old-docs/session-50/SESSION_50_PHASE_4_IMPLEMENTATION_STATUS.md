# Session 50: Phase 4 Implementation Status

**Last Updated**: 2025-12-28 17:38 HKT
**Status**: Ready to Start
**Current Phase**: Phase 4 - dwarfs FUSE Driver

---

## Overall Progress

| Phase | Component | Status | Lines | Completion |
|-------|-----------|--------|-------|------------|
| **Phase 1** | mkdwarfs architecture | ✅ DONE | - | 100% |
| **Phase 2** | mkdwarfs argtable3 | ✅ DONE | 1,124 | 100% |
| **Phase 3** | dwarfsck argtable3 | ✅ DONE | 373 | 100% |
| **Phase 3** | dwarfsextract argtable3 | ✅ DONE | 384 | 100% |
| **Phase 4** | dwarfs argtable3 | ⏳ PENDING | ~900 | 0% |
| **Phase 5** | Testing & validation | ⏳ PENDING | - | 0% |
| **Phase 6** | Documentation | ⏳ PENDING | - | 0% |

---

## Phase 4: dwarfs FUSE Driver Migration

### Step 1: Create parsed_options.h (1 hour)
- [ ] File: `include/dwarfs/tool/dwarfs/parsed_options.h`
- [ ] Define ~60 options in struct
- [ ] Group by category (Input, Cache, FUSE, Performance, Logging)
- [ ] Handle platform-specific options with #ifdef

**Expected**: ~200 lines

### Step 2: Create argtable3_options_parser.h (2 hours)
- [ ] File: `include/dwarfs/tool/dwarfs/argtable3_options_parser.h`
- [ ] Extend argtable3_base_parser
- [ ] Declare ~60 arg_* members
- [ ] Override virtual methods

**Expected**: ~220 lines

### Step 3: Create argtable3_options_parser.cpp (6 hours)
- [ ] File: `tools/src/dwarfs/argtable3_options_parser.cpp`
- [ ] Implement parse()
- [ ] Implement define_tool_options() (~60 options)
- [ ] Implement populate_parsed_options()
- [ ] Implement validate_options()
- [ ] Implement load_environment_variables()

**Expected**: ~900 lines

### Step 4: Update dwarfs_main.cpp (3 hours)
- [ ] File: `tools/src/dwarfs_main.cpp`
- [ ] Remove boost::program_options
- [ ] Add argtable3_options_parser
- [ ] Replace option parsing
- [ ] Use opts.get_parsed_options()
- [ ] Maintain FUSE integration

**Expected**: ~250 lines (from 353, -29%)

### Step 5: FUSE Option Bridging (4 hours)
- [ ] Coordinate with mount_handler.cpp
- [ ] Ensure FUSE struct initialization works
- [ ] Test platform-specific FUSE (FUSE-T, WinFsp, etc.)
- [ ] Validate session/mount flow

### Step 6: Update CMake (15 min)
- [ ] Update cmake/tool_support.cmake
- [ ] Add dwarfs/argtable3_options_parser.cpp

### Step 7: Testing (2 hours)
- [ ] Build test
- [ ] --version test
- [ ] --help test
- [ ] --man test
- [ ] Mount test (Linux/macOS/Windows)
- [ ] ENV variable test
- [ ] Platform-specific FUSE test

---

## Success Criteria

### Functional Requirements
- [ ] dwarfs builds without errors
- [ ] `--version` displays version info
- [ ] `--help` shows all options
- [ ] `--man` recognized (if manpage available)
- [ ] Filesystem mounting works identically to before
- [ ] FUSE options passed correctly
- [ ] Environment variables work (DWARFS_DWARFS_*)
- [ ] Platform-specific FUSE works (FUSE-T, WinFsp, etc.)

### Code Quality
- [ ] Follows mkdwarfs/dwarfsck/dwarfsextract pattern
- [ ] Clean separation of concerns
- [ ] MECE option handling (CLI > ENV > defaults)
- [ ] No breaking changes to FUSE integration
- [ ] Code reduction in main.cpp (~29%)

### Testing
- [ ] All existing tests pass
- [ ] New ENV variable tests pass
- [ ] Cross-platform mounting works
- [ ] No regressions

---

## Files Created/Modified

### New Files (3)
1. `include/dwarfs/tool/dwarfs/parsed_options.h` (~200 lines)
2. `include/dwarfs/tool/dwarfs/argtable3_options_parser.h` (~220 lines)
3. `tools/src/dwarfs/argtable3_options_parser.cpp` (~900 lines)

### Modified Files (2)
1. `tools/src/dwarfs_main.cpp` (353 → ~250 lines, -29%)
2. `cmake/tool_support.cmake` (add parser source)

### Total New Code
~1,320 lines of new argtable3 code

---

## Risks & Mitigations

### Risk 1: FUSE Integration Breakage
**Mitigation**: 
- Keep mount_handler.cpp unchanged
- Test on multiple platforms
- Validate FUSE struct initialization

### Risk 2: Platform-Specific Issues
**Mitigation**:
- Use #ifdef for platform-specific options
- Test on Linux, macOS, Windows
- Verify FUSE-T, WinFsp compatibility

### Risk 3: Complex Validation Logic
**Mitigation**:
- Follow mkdwarfs validation pattern
- Separate validation into small functions
- Test edge cases thoroughly

---

## Environment Variables

**Format**: `DWARFS_DWARFS_<OPTION>`

**Examples**:
```bash
export DWARFS_DWARFS_CACHE_SIZE=2g
export DWARFS_DWARFS_NUM_WORKERS=16
export DWARFS_DWARFS_METADATA_CACHE_SIZE=512m
export DWARFS_DWARFS_READONLY=1
export DWARFS_DWARFS_DEBUGLEVEL=info
```

---

## Platform Testing Checklist

### Linux
- [ ] FUSE3 mount/unmount
- [ ] FUSE2 mount/unmount (if enabled)
- [ ] ENV variables
- [ ] Signal handling (SIGINT, SIGTERM)

### macOS
- [ ] FUSE-T mount/unmount (if available)
- [ ] macFUSE mount/unmount (if available)
- [ ] Full Disk Access permission
- [ ] ENV variables

### Windows
- [ ] WinFsp mount/unmount
- [ ] Drive letter assignment
- [ ] ENV variables

### FreeBSD
- [ ] FUSE compat layer mount/unmount
- [ ] ENV variables

---

**Next Session**: Start with Step 1 (parsed_options.h)
**Estimated Completion**: 2 working days from start