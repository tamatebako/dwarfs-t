# Session 50: Phase 3 (dwarfsck & dwarfsextract) - Continuation Plan

**Date Created**: 2025-12-28
**Status**: Ready to Start
**Priority**: HIGH - Compressed timeline
**Estimated Duration**: 1 day

---

## Phase 2 Completion Summary

**Completed** ✅:
- mkdwarfs fully migrated to argtable3 (1,124 lines)
- CMake linking fixed (dwarfs_tool_support integration)
- jemalloc/Folly ABI issue resolved
- All tests passing (version, help, filesystem creation, ENV vars)
- Old boost parser archived

**Lessons Learned**:
1. USE_JEMALLOC=1 must be set globally BEFORE Folly builds
2. jemalloc targets differ: PkgConfig::JEMALLOC (pkg-config) vs jemalloc::jemalloc (vcpkg)
3. OBJECT libraries with PUBLIC linkage don't include .o files
4. Tool options need non-const getter for modification downstream

---

## Phase 3 Scope: dwarfsck & dwarfsextract

These are **simpler tools** with fewer options than mkdwarfs:
- **dwarfsck**: ~30 options (check/inspect filesystem)
- **dwarfsextract**: ~40 options (extract filesystem)

### Estimated Effort

| Tool | Options | Complexity | Time |
|------|---------|------------|------|
| dwarfsck | ~30 | Low | 3 hours |
| dwarfsextract | ~40 | Medium | 4 hours |
| Testing | - | Medium | 1 hour |
| **Total** | **~70** | **Medium** | **8 hours** |

---

## Implementation Steps

### Step 1: dwarfsck Migration (3 hours)

**Create Files** (2 files):
1. `include/dwarfs/tool/dwarfsck/argtable3_options_parser.h` (~120 lines)
2. `tools/src/dwarfsck/argtable3_options_parser.cpp` (~300 lines)

**Pattern** (copy from mkdwarfs):
```cpp
class argtable3_options_parser : public argtable3_base_parser {
  int parse(int argc, char** argv) override;
  parsed_options const& get_parsed_options() const { return opts_; }
  parsed_options& get_parsed_options() { return opts_; }
  void load_environment_variables();  // DWARFS_DWARFSCK_*
protected:
  void define_tool_options() override;
  bool validate_options() override;
  std::string_view get_tool_name() const override { return "dwarfsck"; }
private:
  parsed_options opts_;
  // ~30 arg_* members
};
```

**Integration**:
- Modify `tools/src/dwarfsck_main.cpp`
- Replace boost parser with argtable3 parser
- Test: `--version`, `--help`, filesystem check

### Step 2: dwarfsextract Migration (4 hours)

**Create Files** (2 files):
1. `include/dwarfs/tool/dwarfsextract/argtable3_options_parser.h` (~150 lines)
2. `tools/src/dwarfsextract/argtable3_options_parser.cpp` (~400 lines)

**Pattern** (same as above):
```cpp
class argtable3_options_parser : public argtable3_base_parser {
  // Same structure as dwarfsck
  void load_environment_variables();  // DWARFS_DWARFSEXTRACT_*
  // ~40 arg_* members
};
```

**Integration**:
- Modify `tools/src/dwarfsextract_main.cpp`
- Replace boost parser with argtable3 parser
- Test: `--version`, `--help`, extraction

### Step 3: Update CMake (15 min)

**Update** [`cmake/tool_support.cmake`](../cmake/tool_support.cmake):
```cmake
# dwarfsck parser
${CMAKE_SOURCE_DIR}/tools/src/dwarfsck/argtable3_options_parser.cpp

# dwarfsextract parser
${CMAKE_SOURCE_DIR}/tools/src/dwarfsextract/argtable3_options_parser.cpp
```

### Step 4: Testing (1 hour)

**dwarfsck tests**:
```bash
./build/dwarfsck --version
./build/dwarfsck --help
./build/dwarfsck /tmp/test-SUCCESS.dff
export DWARFS_DWARFSCK_LOG_LEVEL=debug
./build/dwarfsck /tmp/test-SUCCESS.dff
```

**dwarfsextract tests**:
```bash
./build/dwarfsextract --version
./build/dwarfsextract --help
./build/dwarfsextract -i /tmp/test-SUCCESS.dff -o /tmp/extracted/
export DWARFS_DWARFSEXTRACT_NUM_WORKERS=4
./build/dwarfsextract -i /tmp/test-SUCCESS.dff -o /tmp/extracted-env/
```

### Step 5: Cleanup (30 min)

**Archive old parsers**:
```bash
mkdir -p old-docs/session-50/boost-program-options/{dwarfsck,dwarfsextract}
mv tools/include/dwarfs/tool/dwarfsck/options_parser.h old-docs/session-50/boost-program-options/dwarfsck/
mv tools/src/dwarfsck/options_parser.cpp old-docs/session-50/boost-program-options/dwarfsck/
mv tools/include/dwarfs/tool/dwarfsextract/options_parser.h old-docs/session-50/boost-program-options/dwarfsextract/
mv tools/src/dwarfsextract/options_parser.cpp old-docs/session-50/boost-program-options/dwarfsextract/
```

**Update CMake**:
- Remove old parser references

---

## Success Criteria for Phase 3

### dwarfsck
- [ ] Builds without errors
- [ ] `--version` works
- [ ] `--help` shows all options
- [ ] `--man` recognized
- [ ] Filesystem check works identically
- [ ] ENV variables work (DWARFS_DWARFSCK_*)
- [ ] Old parser archived

### dwarfsextract
- [ ] Builds without errors
- [ ] `--version` works
- [ ] `--help` shows all options
- [ ] `--man` recognized
- [ ] Filesystem extraction works identically
- [ ] ENV variables work (DWARFS_DWARFSEXTRACT_*)
- [ ] Old parser archived

---

## Next Phases After Phase 3

### Phase 4: dwarfs FUSE Driver (1.5 days)
- Most complex tool (~60 options)
- FUSE integration requires special handling
- Bridge argtable3 → FUSE options

### Phase 5: Testing & Validation (0.5 days)
- All 4 tools: --version, --help, --man
- All existing tests pass
- Environment variables work
- MECE priority verified (CLI > ENV > defaults)

### Phase 6: Documentation & Cleanup (0.5 days)
- Update README.md
- Update tool manpages
- Create ENVIRONMENT_VARIABLES.md
- Archive planning docs
- Create completion summary

---

## Key Implementation Notes

**Reuse Pattern from mkdwarfs**:
1. Extend `argtable3_base_parser`
2. Call `add_common_options()` and `add_logger_options()` in `define_tool_options()`
3. Implement `populate_parsed_options()` to map argtable → parsed_options
4. Implement `validate_options()` for tool-specific validation
5. Implement `load_environment_variables()` with tool prefix
6. Provide both const and non-const `get_parsed_options()`

**Avoid These Issues**:
- ❌ Don't set USE_JEMALLOC conditionally (must be global)
- ❌ Don't use PRIVATE linkage for jemalloc in executables
- ❌ Don't forget to handle both PkgConfig::JEMALLOC and jemalloc::jemalloc
- ❌ Don't return const reference from get_parsed_options() if caller needs to modify

---

## Timeline

| Day | Tasks |
|-----|-------|
| Day 1 AM | dwarfsck migration (3 hours) |
| Day 1 PM | dwarfsextract migration (4 hours) |
| Day 1 EOD | Testing & cleanup (1 hour) |

**Total**: 8 hours (1 working day)

---

**Last Updated**: 2025-12-28 16:46 HKT
**Status**: Phase 2 complete, Phase 3 ready to start
**Next**: Implement dwarfsck argtable3 parser