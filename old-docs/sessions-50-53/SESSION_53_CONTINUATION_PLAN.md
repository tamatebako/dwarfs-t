# Session 53: Environment Variable Testing

**Date**: TBD (after Session 52)
**Previous Session**: 52 (Environment variable documentation - planned)
**Status**: Planned
**Priority**: Medium (ensure feature works correctly)
**Duration**: ~2 hours

---

## Executive Summary

Add automated tests to verify environment variable functionality works correctly across all tools. The infrastructure exists (Session 50), but needs thorough testing to ensure reliability.

---

## Current State

✅ **Infrastructure Complete** (Session 50):
- `argtable3_base_parser::load_environment_variables()`
- `get_env_var()` and `has_env_var()` helper methods
- Pattern: `DWARFS_<TOOL>_<OPTION>`

✅ **Documentation Complete** (Session 52):
- User-facing documentation
- Examples for each tool

❌ **Testing Missing**:
- No automated tests
- Manual testing only
- Priority rules not verified

---

## Objectives

1. Add automated tests for environment variable functionality
2. Verify MECE priority rules (CLI > ENV > defaults)
3. Test all tools with environment variables
4. Add regression tests

---

## Tasks

### Task 1: Test Infrastructure Setup (30 min)

**File**: `test/environment_variables_test.cpp` (NEW)

**Test Framework**: GoogleTest

**Test Structure**:
```cpp
#include <gtest/gtest.h>
#include <cstdlib>
#include "dwarfs/tool/argtable3_base_parser.h"

class EnvironmentVariablesTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Save original environment
    // Clear test variables
  }

  void TearDown() override {
    // Restore original environment
  }

  void setEnv(std::string const& name, std::string const& value) {
    #ifdef _WIN32
      _putenv_s(name.c_str(), value.c_str());
    #else
      setenv(name.c_str(), value.c_str(), 1);
    #endif
  }

  void unsetEnv(std::string const& name) {
    #ifdef _WIN32
      _putenv_s(name.c_str(), "");
    #else
      unsetenv(name.c_str());
    #endif
  }
};
```

### Task 2: Priority Rule Tests (45 min)

**Test MECE Priority**: CLI > ENV > defaults

```cpp
TEST_F(EnvironmentVariablesTest, CliOverridesEnvironment) {
  // Set environment variable
  setEnv("DWARFS_MKDWARFS_COMPRESSION_LEVEL", "5");

  // Parse with CLI argument
  const char* argv[] = {"mkdwarfs", "-l", "7"};
  // ...parse...

  // Verify CLI value (7) used, not ENV value (5)
  EXPECT_EQ(opts.compression_level, 7);
}

TEST_F(EnvironmentVariablesTest, EnvironmentOverridesDefaults) {
  // Set environment variable
  setEnv("DWARFS_MKDWARFS_COMPRESSION_LEVEL", "5");

  // Parse without CLI argument
  const char* argv[] = {"mkdwarfs"};
  // ...parse...

  // Verify ENV value (5) used, not default (7)
  EXPECT_EQ(opts.compression_level, 5);
}

TEST_F(EnvironmentVariablesTest, DefaultsWhenNeitherSet) {
  // Ensure no environment variable
  unsetEnv("DWARFS_MKDWARFS_COMPRESSION_LEVEL");

  // Parse without CLI argument
  const char* argv[] = {"mkdwarfs"};
  // ...parse...

  // Verify default value used
  EXPECT_EQ(opts.compression_level, 7);  // default
}
```

### Task 3: Common Variables Tests (30 min)

**Test variables common to all tools**:

```cpp
TEST_F(EnvironmentVariablesTest, LogLevelEnvironmentVariable) {
  setEnv("DWARFS_LOG_LEVEL", "debug");

  // Parse for any tool
  const char* argv[] = {"tool"};
  // ...parse...

  EXPECT_EQ(parser.get_logger_options().threshold, logger::DEBUG);
}

TEST_F(EnvironmentVariablesTest, VerboseEnvironmentVariable) {
  setEnv("DWARFS_VERBOSE", "1");

  // Parse
  // ...

  EXPECT_EQ(parser.get_logger_options().threshold, logger::INFO);
}
```

### Task 4: Tool-Specific Tests (15 min each = 1 hour)

**mkdwarfs**:
```cpp
TEST_F(EnvironmentVariablesTest, MkdwarfsCompressionLevel) {
  setEnv("DWARFS_MKDWARFS_COMPRESSION_LEVEL", "5");
  // Test...
}

TEST_F(EnvironmentVariablesTest, MkdwarfsNumWorkers) {
  setEnv("DWARFS_MKDWARFS_NUM_WORKERS", "8");
  // Test...
}
```

**dwarfs**:
```cpp
TEST_F(EnvironmentVariablesTest, DwarfsCacheSize) {
  setEnv("DWARFS_DWARFS_CACHE_SIZE", "1g");
  // Test...
}
```

**dwarfsck** and **dwarfsextract**: Similar patterns

### Task 5: Integration Test Script (15 min)

**File**: `test/integration/test_env_vars.sh` (NEW)

```bash
#!/bin/bash
# Integration test for environment variables

set -e

echo "Testing mkdwarfs with environment variables..."
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=5
export DWARFS_MKDWARFS_NUM_WORKERS=8

# Create test image
mkdwarfs -i /tmp/test -o /tmp/test.dff -vv 2>&1 | grep -q "level: 5"
mkdwarfs -i /tmp/test -o /tmp/test.dff -vv 2>&1 | grep -q "workers: 8"

# Test CLI override
mkdwarfs -i /tmp/test -o /tmp/test.dff -l 7 -vv 2>&1 | grep -q "level: 7"

echo "✅ All environment variable tests passed"
```

---

## Files to Create

### New Files (2)
- `test/environment_variables_test.cpp` - Unit tests (~300 lines)
- `test/integration/test_env_vars.sh` - Integration test script (~50 lines)

### Modified Files (1)
- `test/CMakeLists.txt` - Add new test to build

**Total**: 3 files

---

## Success Criteria

✅ All priority rules test correctly (MECE)
✅ Common variables work across all tools
✅ Tool-specific variables work correctly
✅ CLI always overrides environment
✅ Invalid environment values handled gracefully
✅ Integration tests pass
✅ All tests pass in CI/CD

---

## Test Coverage

**Priority Rules**: 3 tests
**Common Variables**: 2 tests
**Tool-Specific**: ~12 tests (3 per tool)
**Integration**: 1 script

**Total**: ~18 tests

---

## Timeline

| Task | Duration | Dependencies |
|------|----------|--------------|
| Test infrastructure | 30 min | None |
| Priority rule tests | 45 min | Infrastructure |
| Common variable tests | 30 min | Infrastructure |
| Tool-specific tests | 1 hour | Infrastructure |
| Integration script | 15 min | All above |
| **Total** | **2 hours** | - |

---

## Next Session

**Session 54**: Enhancement 4 - Archive old planning docs (30 min)

---

**Status**: Planned
**Ready to Start**: After Session 52
**Priority**: Medium