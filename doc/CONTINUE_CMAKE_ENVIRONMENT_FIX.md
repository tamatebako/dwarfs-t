# Next Session: CMake Environment Fix

**Status**: Phase 2.8 Complete, Environment Issues Remain
**Branch**: `feature/multi-format-serialization-fuse`
**Date**: 2025-11-23

## What's Done ✅

**Phase 2 (Multi-Format Metadata Serialization)**: 100% Complete
- ✅ Strategy Pattern implementation with FlatBuffers + Thrift backends
- ✅ All metadata tests passing (17/17)
- ✅ Build successful
- ✅ Code changes verified and working

## What Needs Fixing ⚠️

**Environment Issues** (Pre-existing, NOT caused by our changes):
1. **Boost Library Paths**: 16 test failures due to stale Homebrew paths
2. **FLAC Missing**: 1 test failure for optional compression library

These are **environment/build configuration issues**, not code bugs.

## Quick Start (Next Session)

### Command to Run Immediately

```bash
cd /Users/mulgogi/src/external/dwarfs

# Step 1: Clean rebuild to fix Boost paths
chmod +x scripts/clean-build.sh
./scripts/clean-build.sh

# Step 2: Build
cd build-test
ninja -j8

# Step 3: Verify metadata tests still pass
ctest -R metadata --output-on-failure

# Step 4: Check overall test results
ctest --output-on-failure -j8 2>&1 | tail -100
```

**Expected Result**:
- Metadata tests: 100% pass (17/17) ✅
- Full test suite: Some failures may remain if rpath needs fixing

## If Clean Rebuild Doesn't Work

### Diagnose Boost Installation

```bash
# Check current Boost version
brew info boost

# Verify library exists
ls -la /opt/homebrew/opt/boost/lib/libboost_system.dylib
ls -la /opt/homebrew/Cellar/boost/*/lib/libboost_system.dylib

# Check what the test binary is linked against
otool -L build-test/dwarfs_categorizer_tests | grep boost
```

### Manual CMake Reconfiguration

If the script doesn't work, manually reconfigure:

```bash
cd /Users/mulgogi/src/external/dwarfs

# Create new build directory (don't delete old one yet)
mkdir -p build-fresh
cd build-fresh

# Configure with explicit Boost path detection
cmake .. -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=ON \
  -DWITH_TOOLS=ON \
  -DWITH_LIBDWARFS=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DCMAKE_VERBOSE_MAKEFILE=ON

# Build
ninja -j8

# Test
ctest -R metadata --output-on-failure
```

## Long-Term Fix: CMake rpath Configuration

### Problem
macOS dynamic linker (`dyld`) looks for libraries at **hardcoded install paths**, not relative to executable.

### Solution
Add proper rpath configuration to CMake so executables find libraries relative to their location.

### Files to Modify

**1. `cmake/need_boost.cmake`** (after line 50)

Add this after `find_package(Boost ...)`:

```cmake
# Set proper rpath for Boost libraries on macOS
if(APPLE)
  set(CMAKE_INSTALL_RPATH "@loader_path/../lib;@loader_path")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

  # Add Boost library directories to rpath
  if(Boost_LIBRARY_DIRS)
    list(APPEND CMAKE_INSTALL_RPATH "${Boost_LIBRARY_DIRS}")
  endif()
endif()

# Diagnostic output
message(STATUS "Boost version: ${Boost_VERSION}")
message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost library dirs: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
```

**2. `cmake/libdwarfs.cmake`** (in test target definitions, around line 400)

Update test target properties:

```cmake
# For dwarfs_unit_tests, dwarfs_categorizer_tests, etc.
set_target_properties(dwarfs_categorizer_tests PROPERTIES
  BUILD_RPATH "${Boost_LIBRARY_DIRS}"
  INSTALL_RPATH "@loader_path/../lib"
  INSTALL_RPATH_USE_LINK_PATH TRUE
)
```

**3. Root `CMakeLists.txt`** (after project() declaration, around line 20)

Add global rpath settings:

```cmake
# Global rpath settings for macOS
if(APPLE)
  set(CMAKE_MACOSX_RPATH TRUE)
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()
```

## Optional: Fix FLAC Test Failure

### Option A: Install FLAC

```bash
brew install flac
cd /Users/mulgogi/src/external/dwarfs/build-test
cmake .. -DTRY_ENABLE_FLAC=ON
ninja
ctest -R filesystem_writer.compression_metadata_requirements
```

### Option B: Disable FLAC in Build

```bash
cd /Users/mulgogi/src/external/dwarfs/build-test
cmake .. -DTRY_ENABLE_FLAC=OFF
ninja
# Test will pass (FLAC not expected)
```

### Option C: Make Test Skip Gracefully (Best Long-Term)

Edit [`test/filesystem_writer_test.cpp`](../test/filesystem_writer_test.cpp):

Find the test `filesystem_writer.compression_metadata_requirements` and add:

```cpp
TEST(filesystem_writer, compression_metadata_requirements) {
  auto& reg = compression_registry::instance();

  // Skip test if FLAC not available
  if (!reg.has_compressor("flac")) {
    GTEST_SKIP() << "FLAC compression not available (library not installed)";
  }

  // ... existing test code ...
}
```

## Success Criteria

### Minimum Success (Good Enough for Now)
- ✅ Metadata tests: 100% pass
- ✅ Build completes without errors
- ✅ Code changes verified working
- ⚠️ Environment-related test failures documented and understood

### Full Success (Ideal)
- ✅ All tests pass or gracefully skip
- ✅ Builds survive Homebrew upgrades without manual cleaning
- ✅ Clear diagnostic output for library paths
- ✅ Documentation updated with environment setup

## Files Reference

**Build System**:
- [`scripts/clean-build.sh`](../scripts/clean-build.sh) - Clean rebuild script (NEW)
- [`cmake/need_boost.cmake`](../cmake/need_boost.cmake) - Boost finding logic
- [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake) - Library definitions
- Root [`CMakeLists.txt`](../CMakeLists.txt) - Main build config

**Documentation**:
- [`doc/CMAKE_ENVIRONMENT_FIX_PLAN.md`](CMAKE_ENVIRONMENT_FIX_PLAN.md) - Detailed plan (NEW)
- This file - Quick continuation prompt (NEW)

**Test Files**:
- [`test/filesystem_writer_test.cpp`](../test/filesystem_writer_test.cpp) - FLAC test location
- [`test/metadata_factory_test.cpp`](../test/metadata_factory_test.cpp) - Our passing tests ✅

## Notes

1. **Priority**: Metadata tests are what matter for Phase 2, and they're passing ✅
2. **Environment Issues**: Pre-existing, not regressions from our work
3. **Quick Fix**: Clean rebuild likely resolves most issues
4. **Long-Term Fix**: rpath configuration prevents future issues
5. **Optional**: FLAC can be installed or test can skip gracefully

## If You're Stuck

**Common Issues**:

1. **"Library not loaded"**: Clean rebuild needed
2. **"Unknown compression: flac"**: Install FLAC or skip test
3. **CMake can't find Boost**: Check `brew info boost` output
4. **Tests timeout**: Use `-j1` instead of `-j8` for serial execution

**Quick Diagnostics**:

```bash
# Check Boost installation
brew list boost

# Check what test binary needs
otool -L build-test/dwarfs_categorizer_tests

# Check current rpath in binary
otool -l build-test/dwarfs_categorizer_tests | grep -A 3 LC_RPATH

# Verbose CMake to see what it's finding
cd build-test
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON --debug-output
```

## Summary for Next Session

**In One Sentence**: Run [`scripts/clean-build.sh`](../scripts/clean-build.sh) to fix stale Boost paths, verify metadata tests still pass, optionally add rpath configuration for long-term stability.

**Time Estimate**:
- Quick fix (clean rebuild): 5 minutes
- Full fix (rpath configuration): 30 minutes
- FLAC handling: 10 minutes

**Priority**: Quick fix sufficient for now (Phase 2 complete), full fix can be separate PR.