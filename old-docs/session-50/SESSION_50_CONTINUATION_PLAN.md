// ... existing code ...
# Session 50: Unified Option Handling with argtable3 & Version Support

**Date**: 2025-12-28+
**Previous Session**: 49 (Monitoring phase)
**Status**: Ready to implement
**Priority**: Medium (feature enhancement)

---

## Executive Summary

Migrate all 4 DwarFS command-line tools from boost::program_options to argtable3 for unified, consistent option handling. Add `--version` support to all tools. This will improve maintainability, reduce code duplication, and provide better user experience through consistent CLI interfaces.

---

## Objectives

### Primary Goals

1. **Add `--version` to all 4 tools** (mkdwarfs, dwarfs, dwarfsck, dwarfsextract)
   - Display version information consistently
   - Include build information (commit, date, compiler)
   - Show library versions (dependencies)

2. **Migrate to argtable3** for unified option parsing
   - Replace boost::program_options across all tools
   - Create reusable option parsing infrastructure
   - Maintain backward compatibility with existing options

3. **Unified option handling architecture**
   - Single option parsing library (argtable3)
   - Consistent error messages across tools
   - Shared option definitions (common options)

### Secondary Goals

1. **Environment variable support**
   - Allow CLI options to be set via environment variables
   - `DWARFS_*` prefix for all environment variables
   - MECE structure: CLI overrides ENV overrides defaults

2. **Improved help system**
   - Consistent formatting across tools
   - Grouped options (basic, advanced, etc.)
   - Better examples in help text

---

## Architecture Design

### Current State

**Tool** | **Option Parser** | **LOC** | **Issues**
---------|-------------------|---------|----------
mkdwarfs | boost::program_options + custom parser | 827 | Complex, duplicated logic
dwarfs | FUSE options + custom parser | 375 | FUSE-specific, inconsistent
dwarfsck | boost::program_options (direct) | 391 | No abstraction
dwarfsextract | boost::program_options (direct) | 427 | No abstraction

**Total**: ~2,020 lines of option parsing code

### Target State

```
┌─────────────────────────────────────────────────────────────┐
│             Common Option Infrastructure                     │
│                                                              │
│  ┌────────────────────────────────────────────────────┐     │
│  │  dwarfs_tool_support (library)                     │     │
│  │  ┌──────────────────────────────────────────────┐  │     │
│  │  │ argtable3_base_parser                       │  │     │
│  │  │  - Common options (--help, --version, etc.) │  │     │
│  │  │  - Logger options                            │  │     │
│  │  │  - Environment variable support              │  │     │
│  │  └──────────────────────────────────────────────┘  │     │
│  └────────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
                              │
         ┌────────────────────┼────────────────────┬────────────┐
         ▼                    ▼                    ▼            ▼
┌──────────────┐    ┌──────────────┐    ┌──────────────┐  ┌──────────────┐
│ mkdwarfs     │    │ dwarfs       │    │ dwarfsck     │  │dwarfsextract │
│ argtable3    │    │ argtable3    │    │ argtable3    │  │ argtable3    │
│ options      │    │ options      │    │ options      │  │ options      │
│ (extends)    │    │ (extends)    │    │ (extends)    │  │ (extends)    │
└──────────────┘    └──────────────┘    └──────────────┘  └──────────────┘
```

### Class Hierarchy (OOP)

```cpp
// Base class for all option parsers
class argtable3_base_parser {
public:
  virtual ~argtable3_base_parser() = default;

  // Parse command line
  virtual int parse(int argc, char** argv) = 0;

  // Common options
  bool help() const { return help_; }
  bool version() const { return version_; }
  bool man() const { return man_; }

  // Environment variable support
  void load_from_environment();

protected:
  // Common option definitions
  void add_common_options();
  void add_logger_options();

private:
  bool help_{false};
  bool version_{false};
  bool man_{false};
  logger_options logopts_;
};

// Tool-specific parsers
class mkdwarfs_argtable3_parser : public argtable3_base_parser { ... };
class dwarfs_argtable3_parser : public argtable3_base_parser { ... };
class dwarfsck_argtable3_parser : public argtable3_base_parser { ... };
class dwarfsextract_argtable3_parser : public argtable3_base_parser { ... };
```

---

## Implementation Plan

### Phase 1: Infrastructure (Days 1-2)

**Tasks**:
1. Add argtable3 dependency to CMake
2. Create `argtable3_base_parser` base class
3. Implement `--version` infrastructure
4. Add environment variable support framework

**Files to Create**:
- `include/dwarfs/tool/argtable3_base_parser.h` (150 lines)
- `src/tool/argtable3_base_parser.cpp` (200 lines)
- `include/dwarfs/tool/version_info.h` (80 lines)
- `src/tool/version_info.cpp` (120 lines)

**Files to Modify**:
- `cmake/tool_support.cmake` - Add argtable3 dependency
- `cmake/vcpkg/argtable3.cmake` - New vcpkg module

**Deliverables**:
- ✅ argtable3 integrated into build
- ✅ Base parser class working
- ✅ Version info infrastructure complete

### Phase 2: mkdwarfs Migration (Days 3-4)

**Tasks**:
1. Create `mkdwarfs_argtable3_parser` class
2. Port all boost::program_options options to argtable3
3. Maintain backward compatibility
4. Add `--version` support
5. Update tests

**Files to Create**:
- `include/dwarfs/tool/mkdwarfs/argtable3_options_parser.h` (200 lines)
- `src/mkdwarfs/argtable3_options_parser.cpp` (600 lines)

**Files to Modify**:
- `tools/src/mkdwarfs_main.cpp` - Switch to argtable3 parser
- Delete: `tools/src/mkdwarfs/options_parser.cpp` (827 lines → 0)
- Delete: `tools/include/dwarfs/tool/mkdwarfs/options_parser.h`

**Tests**:
- All existing mkdwarfs tests pass
- New `--version` test
- Environment variable tests

### Phase 3: dwarfsck & dwarfsextract Migration (Days 5-6)

**Tasks**:
1. Create `dwarfsck_argtable3_parser` class
2. Create `dwarfsextract_argtable3_parser` class
3. Port all options to argtable3
4. Add `--version` support
5. Update tests

**Files to Create**:
- `include/dwarfs/tool/dwarfsck/argtable3_options_parser.h` (120 lines)
- `src/dwarfsck/argtable3_options_parser.cpp` (250 lines)
- `include/dwarfs/tool/dwarfsextract/argtable3_options_parser.h` (150 lines)
- `src/dwarfsextract/argtable3_options_parser.cpp` (300 lines)

**Files to Modify**:
- `tools/src/dwarfsck_main.cpp` - Switch to argtable3
- `tools/src/dwarfsextract_main.cpp` - Switch to argtable3

**Tests**:
- All existing tests pass
- New `--version` tests

### Phase 4: dwarfs FUSE Driver Migration (Days 7-8)

**Tasks**:
1. Create `dwarfs_argtable3_parser` class (FUSE-aware)
2. Integrate with FUSE option system
3. Port custom FUSE options to argtable3
4. Add `--version` support
5. Update tests

**Files to Create**:
- `include/dwarfs/tool/dwarfs/argtable3_options_parser.h` (180 lines)
- `src/dwarfs/argtable3_options_parser.cpp` (400 lines)

**Files to Modify**:
- `tools/src/dwarfs_main.cpp` - Switch to argtable3
- Delete: `tools/src/dwarfs/options_parser.cpp` (375 lines → 0)
- Delete: `tools/include/dwarfs/tool/dwarfs/options_parser.h`

**Challenges**:
- FUSE has its own option system (`fuse_opt_parse`)
- Need to bridge argtable3 and FUSE options
- Maintain FUSE compatibility

### Phase 5: Testing & Validation (Day 9)

**Tasks**:
1. Comprehensive testing across all tools
2. Backward compatibility verification
3. Environment variable testing
4. Cross-platform testing (Linux, macOS, Windows)
5. Performance validation

**Test Matrix**:
- ✅ All 4 tools display correct version
- ✅ All existing options work identically
- ✅ Environment variables override defaults
- ✅ CLI arguments override environment variables
- ✅ Help text is clear and consistent
- ✅ Error messages are clear and consistent

### Phase 6: Documentation & Cleanup (Day 10)

**Tasks**:
1. Update official documentation
2. Add environment variable documentation
3. Update man pages
4. Add migration guide for users
5. Archive old boost::program_options code
6. Clean up dead code

**Documentation to Update**:
- `README.md` - Add environment variable section
- `doc/mkdwarfs.md` - Document all env vars
- `doc/dwarfs.md` - Document all env vars
- `doc/dwarfsck.md` - Document all env vars
- `doc/dwarfsextract.md` - Document all env vars
- New: `doc/ENVIRONMENT_VARIABLES.md` - Complete reference

**Files to Archive**:
- `old-docs/session-50/boost_program_options_code/` (old parsers)

---

## Environment Variable Design

### Naming Convention

**Pattern**: `DWARFS_<TOOL>_<OPTION>`

Examples:
```bash
# mkdwarfs
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=7
export DWARFS_MKDWARFS_NUM_WORKERS=8
export DWARFS_MKDWARFS_BLOCK_SIZE_BITS=24

# dwarfs
export DWARFS_DWARFS_CACHE_SIZE=1g
export DWARFS_DWARFS_NUM_WORKERS=4

# Common (all tools)
export DWARFS_LOG_LEVEL=info
export DWARFS_VERBOSE=1
```

### Priority Order (MECE)

1. **CLI arguments** (highest priority)
2. **Environment variables**
3. **Configuration file** (future)
4. **Defaults** (lowest priority)

Example:
```bash
# Default: level=7
# ENV: DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
# CLI: mkdwarfs -l 9

# Result: level=9 (CLI wins)
```

---

## argtable3 Integration

### Dependency Addition

```cmake
# cmake/vcpkg/argtable3.cmake
include_guard(GLOBAL)

# Try vcpkg first
find_package(argtable3 CONFIG QUIET)

if(argtable3_FOUND)
  message(STATUS "Using argtable3 from vcpkg")
  add_library(argtable3::argtable3 ALIAS argtable3)
else()
  # Try pkg-config
  pkg_check_modules(argtable3 argtable3)

  if(argtable3_FOUND)
    message(STATUS "Using system argtable3 (pkg-config)")
    add_library(argtable3::argtable3 INTERFACE IMPORTED)
    set_target_properties(argtable3::argtable3 PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${argtable3_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${argtable3_LIBRARIES}")
  else()
    # FetchContent as fallback
    include(FetchContent)
    FetchContent_Declare(
      argtable3
      GIT_REPOSITORY https://github.com/argtable/argtable3.git
      GIT_TAG v3.2.2)
    FetchContent_MakeAvailable(argtable3)
  endif()
endif()
```

### vcpkg Port

```json
// vcpkg.json
{
  "dependencies": [
    ...
    "argtable3"
  ]
}
```

---

## Version Information Design

### Output Format

```bash
$ mkdwarfs --version
mkdwarfs 0.16.0 (commit: a1b2c3d, built: 2025-12-28 11:23:45 UTC)
Compiler: clang 17.0.1
Platform: macOS ARM64
Features: FlatBuffers, FLAC, LZ4, LZMA, Brotli, zstd
Libraries:
  - Boost 1.84.0
  - OpenSSL 3.2.1
  - libarchive 3.7.2
  - zstd 1.5.6
  - xxHash 0.8.2
```

### Implementation

```cpp
// include/dwarfs/tool/version_info.h
namespace dwarfs::tool {

struct version_info {
  std::string version;       // "0.16.0"
  std::string commit_hash;   // "a1b2c3d"
  std::string build_date;    // "2025-12-28 11:23:45 UTC"
  std::string compiler;      // "clang 17.0.1"
  std::string platform;      // "macOS ARM64"
  std::vector<std::string> features;
  std::map<std::string, std::string> libraries;

  static version_info get();
  std::string to_string(std::string_view tool_name) const;
};

} // namespace dwarfs::tool
```

---

## Backward Compatibility

### Guarantees

1. **All existing options work identically**
   - Same short/long option names
   - Same default values
   - Same validation rules

2. **Error messages remain clear**
   - Improved clarity where possible
   - Maintain familiar phrasing

3. **Exit codes unchanged**
   - 0 = success
   - 1 = error
   - 2 = validation error

### Migration Path

**For users**: No changes required. All existing commands work identically.

**For developers**: Old boost::program_options code archived but available for reference.

---

## Metrics

**Before**:
- Total option parsing LOC: ~2,020
- Option parsing libraries: 2 (boost::program_options + FUSE options)
- Consistency: Low (different patterns per tool)
- Version support: 0/4 tools
- Environment variable support: 0/4 tools

**After (Target)**:
- Total option parsing LOC: ~1,500 (-25%)
- Option parsing libraries: 1 (argtable3)
- Consistency: High (unified base class)
- Version support: 4/4 tools ✅
- Environment variable support: 4/4 tools ✅
- Code reuse: High (shared base class)

---

## Risks & Mitigation

### Risk 1: argtable3 Limitations

**Risk**: argtable3 may not support all boost::program_options features

**Mitigation**:
- Thorough feature comparison before starting
- Fallback to custom parsing for complex options
- Document any limitations clearly

### Risk 2: FUSE Integration Complexity

**Risk**: FUSE has its own option system that may conflict

**Mitigation**:
- Keep FUSE options separate where needed
- Bridge argtable3 → FUSE options as needed
- Maintain existing FUSE behavior exactly

### Risk 3: Breaking Changes

**Risk**: Subtle behavior changes may break user scripts

**Mitigation**:
- Comprehensive backward compatibility testing
- Beta release period for testing
- Clear release notes documenting any changes

---

## Success Criteria

Session 50 is successful if:

1. ✅ All 4 tools have `--version` support
2. ✅ All tools use argtable3 for option parsing
3. ✅ All existing options work identically
4. ✅ Environment variable support works across all tools
5. ✅ All tests pass (including new tests)
6. ✅ Code is cleaner and more maintainable
7. ✅ Documentation is complete and accurate

---

## Timeline

**Estimated Duration**: 10 working days (2 weeks)

**Milestones**:
- Day 2: Infrastructure complete
- Day 4: mkdwarfs migrated
- Day 6: dwarfsck & dwarfsextract migrated
- Day 8: dwarfs migrated
- Day 9: Testing complete
- Day 10: Documentation complete

---

**Status**: Ready to implement
**Next Session**: Session 50 Implementation
**Document**: [`doc/SESSION_50_IMPLEMENTATION_STATUS.md`](SESSION_50_IMPLEMENTATION_STATUS.md)