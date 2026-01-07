// ... existing code ...
# Session 50: Quick Start Prompt

**Read This First**: [`doc/SESSION_50_CONTINUATION_PLAN.md`](SESSION_50_CONTINUATION_PLAN.md)

---

## Context

Session 49 completed the monitoring phase. All manpage functionality is working correctly. User has requested two new features:

1. **Add `--version` to all 4 tools** (mkdwarfs, dwarfs, dwarfsck, dwarfsextract)
2. **Migrate to argtable3** for unified option handling across all tools

---

## Your Task

Implement unified option handling with argtable3 and add `--version` support to all DwarFS tools following object-oriented principles.

**Duration**: ~10 days
**Complexity**: High (architecture change)
**Priority**: Medium (feature enhancement)

---

## Quick Reference

### Current State

- **4 tools** use **2 different option parsing libraries**:
  - mkdwarfs: boost::program_options (custom parser, 827 lines)
  - dwarfs: FUSE options + custom (375 lines)
  - dwarfsck: boost::program_options (inline, 391 lines)
  - dwarfsextract: boost::program_options (inline, 427 lines)
- **Total**: ~2,020 lines of option parsing code
- **Version support**: 0/4 tools
- **Environment variables**: Not supported

### Target State

- **All 4 tools** use **1 option parsing library** (argtable3)
- **Unified base class**: `argtable3_base_parser` (OOP architecture)
- **Total**: ~1,500 lines (-25% reduction)
- **Version support**: 4/4 tools ✅
- **Environment variables**: Full support with MECE priority (CLI > ENV > defaults)

---

## Implementation Phases

### Phase 1: Infrastructure (Days 1-2) → **START HERE**

**Goal**: Create foundation for argtable3 integration

**Tasks**:
1. Add argtable3 dependency to CMake [`cmake/vcpkg/argtable3.cmake`](../cmake/vcpkg/argtable3.cmake)
2. Create base parser class [`include/dwarfs/tool/argtable3_base_parser.h`](../include/dwarfs/tool/argtable3_base_parser.h)
3. Implement version info [`include/dwarfs/tool/version_info.h`](../include/dwarfs/tool/version_info.h)
4. Add environment variable framework

**Files to Create** (6 files, ~630 lines):
- `cmake/vcpkg/argtable3.cmake` (80 lines)
- `include/dwarfs/tool/argtable3_base_parser.h` (150 lines)
- `src/tool/argtable3_base_parser.cpp` (200 lines)
- `include/dwarfs/tool/version_info.h` (80 lines)
- `src/tool/version_info.cpp` (120 lines)
- `test/argtable3_base_parser_test.cpp` (150 lines - if tests enabled)

**Verification**:
```bash
# Build with new infrastructure
cmake -B build -DWITH_TESTS=ON
cmake --build build

# Run base parser tests
./build/argtable3_base_parser_test
```

### Phase 2: mkdwarfs Migration (Days 3-4)

**Goal**: Migrate mkdwarfs (most complex tool) to argtable3

**Key Challenge**: 80+ command-line options to port

**Files to Create**:
- `include/dwarfs/tool/mkdwarfs/argtable3_options_parser.h`
- `src/mkdwarfs/argtable3_options_parser.cpp`

**Files to Delete**:
- `tools/src/mkdwarfs/options_parser.cpp` (827 lines)
- `tools/include/dwarfs/tool/mkdwarfs/options_parser.h`

**Verification**:
```bash
# Test all existing functionality
./build/mkdwarfs --version
./build/mkdwarfs --help
./build/mkdwarfs -i test-data -o test.dff -l 3
```

### Phase 3: dwarfsck & dwarfsextract (Days 5-6)

**Goal**: Migrate simpler tools

**Less complex** than mkdwarfs but still need careful porting.

### Phase 4: dwarfs FUSE Driver (Days 7-8)

**Goal**: Migrate FUSE driver (special case)

**Key Challenge**: Integration with FUSE's own option system

### Phase 5: Testing (Day 9)

**Goal**: Comprehensive cross-platform validation

### Phase 6: Documentation (Day 10)

**Goal**: Update all docs, create env var guide

---

## Architecture Principles

### OOP Design

```cpp
// Base class (abstract)
class argtable3_base_parser {
protected:
  virtual void define_tool_options() = 0;
  void add_common_options();  // --help, --version, --man
  void add_logger_options();   // --log-level, --verbose, etc.

public:
  virtual int parse(int argc, char** argv) = 0;
  void load_from_environment(); // ENV support
};

// Tool-specific (concrete)
class mkdwarfs_argtable3_parser : public argtable3_base_parser {
protected:
  void define_tool_options() override;

public:
  int parse(int argc, char** argv) override;
};
```

### MECE Priority

```
CLI arguments (highest)
   ↓
Environment variables
   ↓
Configuration file (future)
   ↓
Defaults (lowest)
```

### Environment Variable Naming

```bash
DWARFS_<TOOL>_<OPTION>

# Examples:
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=7
export DWARFS_DWARFS_CACHE_SIZE=1g
export DWARFS_LOG_LEVEL=info  # Common option
```

---

## Key Implementation Files

### Phase 1 Critical Files

1. **[`cmake/vcpkg/argtable3.cmake`](../cmake/vcpkg/argtable3.cmake)** - Dependency integration
   - Try vcpkg first
   - Fall back to pkg-config
   - FetchContent as last resort

2. **[`include/dwarfs/tool/argtable3_base_parser.h`](../include/dwarfs/tool/argtable3_base_parser.h)** - Base class
   - Pure virtual `define_tool_options()`
   - Pure virtual `parse()`
   - Protected helpers for common patterns

3. **[`include/dwarfs/tool/version_info.h`](../include/dwarfs/tool/version_info.h)** - Version infrastructure
   - Struct with version, commit, build date
   - Compiler, platform info
   - Library versions

---

## Testing Strategy

### Unit Tests

```cpp
// Test base parser
TEST(Argtable3BaseParser, ParsesCommonOptions) {
  // Test --help, --version, --man
}

TEST(Argtable3BaseParser, LoadsEnvironmentVariables) {
  // Test ENV loading
}

TEST(Argtable3BaseParser, CLIOverridesEnvironment) {
  // Test MECE priority
}
```

### Integration Tests

```bash
# Test each tool
./build/mkdwarfs --version
./build/dwarfs --version
./build/dwarfsck --version
./build/dwarfsextract --version

# Test environment variables
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
./build/mkdwarfs -i test -o test.dff  # Should use level 5
./build/mkdwarfs -i test -o test.dff -l 7  # Should use level 7 (CLI wins)
```

---

## Documentation Requirements

**Must Update**:
- `README.md` - Add environment variable section
- `doc/mkdwarfs.md` - Document all env vars
- `doc/dwarfs.md` - Document all env vars
- `doc/dwarfsck.md` - Document all env vars
- `doc/dwarfsextract.md` - Document all env vars

**Must Create**:
- `doc/ENVIRONMENT_VARIABLES.md` - Complete reference
- `doc/ARGTABLE3_MIGRATION_GUIDE.md` - For developers

**Must Archive**:
- `old-docs/session-50/` - Old boost::program_options code

---

## Success Criteria

✅ All 4 tools display version with `--version`
✅ All tools use argtable3 (no boost::program_options)
✅ All existing options work identically
✅ Environment variables work for all tools
✅ CLI overrides ENV (MECE enforced)
✅ All tests pass
✅ Code is cleaner (-25% LOC)
✅ Documentation complete

---

## Quick Commands

```bash
# Read full plan
cat doc/SESSION_50_CONTINUATION_PLAN.md

# Track progress
cat doc/SESSION_50_IMPLEMENTATION_STATUS.md

# Start Phase 1
# 1. Create cmake/vcpkg/argtable3.cmake
# 2. Create include/dwarfs/tool/argtable3_base_parser.h
# 3. Create src/tool/argtable3_base_parser.cpp
# 4. Create include/dwarfs/tool/version_info.h
# 5. Create src/tool/version_info.cpp
# 6. Update cmake/tool_support.cmake (link argtable3)
```

---

**Status**: Ready to implement
**Next**: Start Phase 1 - Infrastructure
**Track**: [`doc/SESSION_50_IMPLEMENTATION_STATUS.md`](SESSION_50_IMPLEMENTATION_STATUS.md)