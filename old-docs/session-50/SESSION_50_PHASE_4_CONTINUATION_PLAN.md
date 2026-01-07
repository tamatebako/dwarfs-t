# Session 50: Phase 4 (dwarfs FUSE Driver) - Continuation Plan

**Date Created**: 2025-12-28
**Status**: Ready to Start
**Priority**: HIGH - Final tool migration
**Estimated Duration**: 1.5 days

---

## Phase 3 Completion Summary

**Completed** ✅:
- dwarfsck fully migrated to argtable3 (373 lines parser, 280 lines main)
- dwarfsextract fully migrated to argtable3 (384 lines parser, 341 lines main)
- Both tools support --version, --help, --man, ENV variables
- CMake integration complete
- Code reduction: dwarfsck -28%, dwarfsextract -20%

**Pattern Established**:
1. Create parsed_options.h with clean struct
2. Create argtable3_options_parser.h extending argtable3_base_parser
3. Create argtable3_options_parser.cpp with full implementation
4. Update tool main.cpp to use parser
5. Add to cmake/tool_support.cmake

---

## Phase 4 Scope: dwarfs FUSE Driver

**Complexity**: HIGHEST - Most complex tool
- **Options**: ~60 options (2x more than mkdwarfs)
- **FUSE Integration**: Special handling for FUSE options
- **Platform-specific**: macOS (FUSE-T), Linux (FUSE2/3), Windows (WinFsp)

### Estimated Effort

| Component | Complexity | Time |
|-----------|------------|------|
| parsed_options.h | High | 1 hour |
| argtable3_options_parser.h | High | 2 hours |
| argtable3_options_parser.cpp | Very High | 6 hours |
| dwarfs_main.cpp integration | High | 3 hours |
| FUSE option bridging | Very High | 4 hours |
| Testing | High | 2 hours |
| **Total** | **Very High** | **18 hours (1.5 days)** |

---

## Implementation Steps

### Step 1: Create parsed_options.h (1 hour)

**File**: `include/dwarfs/tool/dwarfs/parsed_options.h` (~200 lines)

**Options Categories** (from dwarfs_main.cpp analysis):
1. **Input/Output** (3): image, mountpoint, debuglevel
2. **Cache Options** (5): cache-size, num-workers, decompress-ratio, readonly, cache-image
3. **Metadata Options** (4): metadata-cache-size, num-metadata-workers, metadata-threads
4. **FUSE Options** (20+): Various FUSE-specific flags
5. **Performance** (8): seq-detector, prefetch, threads, etc.
6. **Logging** (5): log-level, verbose, quiet, etc.
7. **Tool-specific** (15+): tool-mode, etc.

### Step 2: Create argtable3_options_parser.h (2 hours)

**File**: `include/dwarfs/tool/dwarfs/argtable3_options_parser.h` (~220 lines)

**Pattern** (same as mkdwarfs/dwarfsck/dwarfsextract):
```cpp
class argtable3_options_parser : public argtable3_base_parser {
  int parse(int argc, char** argv) override;
  parsed_options const& get_parsed_options() const;
  parsed_options& get_parsed_options();
  void load_environment_variables();  // DWARFS_DWARFS_*
protected:
  void define_tool_options() override;
  bool validate_options() override;
  std::string_view get_tool_name() const override { return "dwarfs"; }
private:
  void populate_parsed_options();
  parsed_options opts_;
  // ~60 arg_* members
};
```

### Step 3: Create argtable3_options_parser.cpp (6 hours)

**File**: `tools/src/dwarfs/argtable3_options_parser.cpp` (~900 lines)

**Implementation**:
1. Constructor/destructor
2. parse() - Initialize, parse, validate
3. define_tool_options() - Add ~60 options to argtable
4. populate_parsed_options() - Map argtable → parsed_options
5. validate_options() - Tool-specific validation
6. load_environment_variables() - ENV support (DWARFS_DWARFS_*)

**Challenges**:
- Many FUSE-specific options
- Platform-specific options (macOS vs Linux vs Windows)
- Complex validation (mountpoint, image paths, FUSE permissions)

### Step 4: Update dwarfs_main.cpp (3 hours)

**File**: `tools/src/dwarfs_main.cpp`

**Current**: ~353 lines (after Phase 1-6 refactoring)
**Expected**: ~250 lines (-29%)

**Changes**:
1. Remove boost::program_options includes
2. Add argtable3_options_parser include
3. Replace option parsing (lines 100-250)
4. Use opts.get_parsed_options() for all values
5. Keep FUSE integration logic unchanged

### Step 5: FUSE Option Bridging (4 hours)

**Challenge**: Bridge argtable3 options → FUSE struct fuse_arguments

**Current Approach** (in mount_handler.cpp):
```cpp
struct fuse_arguments {
  char const* fsname;
  char const* mountpoint;
  // ... many FUSE options
};
```

**Solution**:
1. In argtable3_options_parser, populate parsed_options with all FUSE-relevant values
2. In mount_handler, read from parsed_options → fuse_arguments
3. Maintain existing FUSE initialization flow

**Files to Coordinate**:
- `tools/src/dwarfs/argtable3_options_parser.cpp` (populate FUSE options)
- `tools/src/dwarfs/mount_handler.cpp` (consume FUSE options)
- Ensure no breaking changes to FUSE integration

### Step 6: Update CMake (15 min)

**Update** [`cmake/tool_support.cmake`](../cmake/tool_support.cmake):
```cmake
# dwarfs FUSE driver handlers - only if FUSE driver enabled
$<$<BOOL:${WITH_FUSE_DRIVER}>:${CMAKE_SOURCE_DIR}/tools/src/dwarfs/argtable3_options_parser.cpp>
$<$<BOOL:${WITH_FUSE_DRIVER}>:${CMAKE_SOURCE_DIR}/tools/src/dwarfs/mount_handler.cpp>
```

**Note**: mount_handler.cpp already in cmake, only add argtable3_options_parser.cpp

### Step 7: Testing (2 hours)

**dwarfs tests**:
```bash
./build/dwarfs --version
./build/dwarfs --help
./build/dwarfs --man

# Basic mount test
./build/mkdwarfs -i /tmp/test -o /tmp/test.dff
./build/dwarfs /tmp/test.dff /tmp/mnt
ls /tmp/mnt
fusermount -u /tmp/mnt  # Linux
umount /tmp/mnt         # macOS

# ENV variable test
export DWARFS_DWARFS_CACHE_SIZE=1g
export DWARFS_DWARFS_NUM_WORKERS=8
./build/dwarfs /tmp/test.dff /tmp/mnt
```

**Platform-specific**:
- Linux: Test FUSE2 and FUSE3
- macOS: Test FUSE-T (if available)
- Windows: Test WinFsp (if available)

---

## Success Criteria for Phase 4

### dwarfs FUSE Driver
- [ ] Builds without errors
- [ ] `--version` works
- [ ] `--help` shows all options
- [ ] `--man` recognized
- [ ] Filesystem mounting works identically
- [ ] FUSE options passed correctly
- [ ] ENV variables work (DWARFS_DWARFS_*)
- [ ] Platform-specific FUSE works (FUSE-T, WinFsp, etc.)
- [ ] Old parser archived (if exists)

---

## Next Phases After Phase 4

### Phase 5: Testing & Validation (0.5 days)
- All 4 tools: --version, --help, --man
- All existing tests pass
- Environment variables work
- MECE priority verified (CLI > ENV > defaults)
- Cross-platform testing

### Phase 6: Documentation & Cleanup (0.5 days)
- Update README.md
- Create ENVIRONMENT_VARIABLES.md
- Update tool manpages (if needed)
- Archive planning docs
- Create completion summary

---

## Key Implementation Notes

**Reuse Pattern from mkdwarfs/dwarfsck/dwarfsextract**:
1. Extend `argtable3_base_parser`
2. Call `add_common_options()` and `add_logger_options()` in `define_tool_options()`
3. Implement `populate_parsed_options()` to map argtable → parsed_options
4. Implement `validate_options()` for tool-specific validation
5. Implement `load_environment_variables()` with tool prefix
6. Provide both const and non-const `get_parsed_options()`

**Special Considerations for dwarfs**:
- FUSE options are platform-specific (handle with #ifdef)
- FUSE struct initialization must remain unchanged
- Session/mount lifecycle must remain unchanged
- Performance-critical paths (cache, workers) need careful validation

**Avoid These Issues**:
- ❌ Don't break FUSE integration
- ❌ Don't change mount/unmount flow
- ❌ Don't modify signal handling
- ❌ Don't skip platform-specific testing

---

## FUSE Platform Matrix

| Platform | FUSE Type | Version | Status |
|----------|-----------|---------|--------|
| Linux | FUSE3 | ≥3.0 | ✅ Primary |
| Linux | FUSE2 | ≥2.9 | ✅ Legacy |
| macOS | FUSE-T | ≥1.0 | ✅ Userspace |
| macOS | macFUSE | ≥4.0 | ✅ Kernel |
| Windows | WinFsp | ≥1.9 | ✅ Supported |
| FreeBSD | FUSE | ≥2.9 | ✅ Compat Layer |

---

## Timeline

| Day | Tasks |
|-----|-------|
| Day 1 AM | Create parsed_options.h + argtable3_options_parser.h (3 hours) |
| Day 1 PM | Start argtable3_options_parser.cpp (4 hours) |
| Day 2 AM | Finish argtable3_options_parser.cpp + dwarfs_main.cpp (5 hours) |
| Day 2 PM | FUSE bridging + CMake + testing (5 hours) |

**Total**: 18 hours (1.5 working days)

---

**Last Updated**: 2025-12-28 17:34 HKT
**Status**: Phase 3 complete, Phase 4 ready to start
**Next**: Implement dwarfs argtable3 parser