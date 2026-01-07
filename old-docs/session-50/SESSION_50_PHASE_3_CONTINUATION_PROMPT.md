# Session 50: Phase 3 Quick Start

**Read First**: [`doc/SESSION_50_PHASE_3_CONTINUATION_PLAN.md`](SESSION_50_PHASE_3_CONTINUATION_PLAN.md)

---

## Current State: Phase 2 Complete ✅

### What's Done
- mkdwarfs fully migrated to argtable3 (1,124 lines)
- All tests passing (version, help, creation, ENV vars)
- jemalloc/Folly ABI issue resolved
- Pattern established for tools 2-4

---

## Your Task: Migrate dwarfsck & dwarfsextract

### Quick Start (8 hours total)

**Step 1**: dwarfsck migration (3 hours)

```bash
# Copy mkdwarfs parser as template
cd /Users/mulgogi/src/external/dwarfs
mkdir -p include/dwarfs/tool/dwarfsck
mkdir -p tools/src/dwarfsck

# Reference implementation
cat tools/src/mkdwarfs/argtable3_options_parser.cpp
cat include/dwarfs/tool/mkdwarfs/argtable3_options_parser.h
```

**Create these files**:
1. `include/dwarfs/tool/dwarfsck/argtable3_options_parser.h` (~120 lines)
   - Copy mkdwarfs header
   - Change namespace to `dwarfsck`
   - Update option members (~30 options)
   - Change `get_tool_name()` to return "dwarfsck"

2. `tools/src/dwarfsck/argtable3_options_parser.cpp` (~300 lines)
   - Copy mkdwarfs implementation
   - Update namespace
   - Implement ~30 options in `define_tool_options()`
   - Implement `populate_parsed_options()`
   - Implement `load_environment_variables()` with "DWARFSCK" prefix

3. Modify `tools/src/dwarfsck_main.cpp`:
   - Replace: `#include <dwarfs/tool/dwarfsck/options_parser.h>`
   - With: `#include <dwarfs/tool/dwarfsck/argtable3_options_parser.h>`
   - Replace boost parser instantiation with argtable3 version
   - Use `auto& opts = opt_parser.get_parsed_options()`

4. Update `cmake/tool_support.cmake`:
   - Add: `${CMAKE_SOURCE_DIR}/tools/src/dwarfsck/argtable3_options_parser.cpp`

**Step 2**: dwarfsextract migration (4 hours)
- Same pattern as dwarfsck
- ~40 options instead of ~30

**Step 3**: Test & cleanup (1 hour)

```bash
# Build
cmake --build build-test --target dwarfsck dwarfsextract -j8

# Test
./build-test/dwarfsck --version
./build-test/dwarfsck --help
./build-test/dwarfsck /tmp/test-SUCCESS.dff

./build-test/dwarfsextract --version
./build-test/dwarfsextract --help
./build-test/dwarfsextract -i /tmp/test-SUCCESS.dff -o /tmp/extracted/

# Archive old parsers
mv tools/include/dwarfs/tool/{dwarfsck,dwarfsextract}/options_parser.h old-docs/session-50/boost-program-options/
mv tools/src/{dwarfsck,dwarfsextract}/options_parser.cpp old-docs/session-50/boost-program-options/
```

---

## Key Points

**Reuse mkdwarfs Pattern** ✅:
- Extend `argtable3_base_parser`
- Call `add_common_options()` + `add_logger_options()`
- Implement `define_tool_options()`, `populate_parsed_options()`, `validate_options()`
- Provide both const and non-const `get_parsed_options()`

**Avoid Issues**:
- ❌ Don't define tools-specific USE_JEMALLOC (already global in folly.cmake)
- ✅ Do handle both PkgConfig::JEMALLOC and jemalloc::jemalloc in cmake
- ✅ Do test with actual filesystem operations
- ✅ Do verify ENV variables work

---

## Success Criteria

- [ ] dwarfsck builds & runs correctly
- [ ] dwarfsextract builds & runs correctly
- [ ] All tests pass (version, help, functionality, ENV)
- [ ] Old parsers archived
- [ ] Ready for Phase 4 (dwarfs FUSE driver)

**Estimated Time**: 8 hours (1 working day)

---

**Status**: Ready to start
**Priority**: HIGH
**Next**: Implement dwarfsck argtable3 parser