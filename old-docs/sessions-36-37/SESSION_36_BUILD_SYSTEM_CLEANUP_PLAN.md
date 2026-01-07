# Session 36: Build System Cleanup Plan

**Date**: 2025-12-24  
**Status**: Planning Phase  
**Goal**: Clean up build/test/benchmark system with vcpkg integration

---

## Issues Identified

### 1. Test Registration Problem ❌

**Issue**: Tests show as "NOT_BUILT" even with `-DWITH_TESTS=ON`

```
Could not find executable dwarfs_filesystem_tests_NOT_BUILT
Could not find executable ricepp_test_NOT_BUILT
```

**Root Cause**: 
- [`cmake/tests.cmake:92-104`](../cmake/tests.cmake#L92-L104) defines `dwarfs_filesystem_tests` conditionally
- [`scripts/build-all-and-test.sh:72`](../scripts/build-all-and-test.sh#L72) only builds tools, not test targets
- Test targets exist but aren't built, so CTest finds placeholder names

**Solution**:
```bash
# Current (WRONG):
cmake --build "$build_dir" --target mkdwarfs dwarfsck dwarfsextract -j"$JOBS"

# Fixed:
if [[ "$WITH_TESTS" == "ON" ]]; then
  cmake --build "$build_dir" -j"$JOBS"  # Build ALL targets including tests
else
  cmake --build "$build_dir" --target mkdwarfs dwarfsck dwarfsextract -j"$JOBS"
fi
```

### 2. Production vs Debug Modes ❌

**Issue**: Scripts always try to run tests, no option for production-only builds

**Solution**: Add `--debug` flag to scripts

```bash
# Usage:
./scripts/run-all.sh              # Production: no tests
./scripts/run-all.sh --debug      # Debug: build + run tests
./scripts/run-all.sh --debug [dataset]  # Debug + benchmark
```

**Implementation**:
- Default: `-DWITH_TESTS=OFF -DWITH_BENCHMARKS=OFF`
- Debug mode: `-DWITH_TESTS=ON -DWITH_BENCHMARKS=ON`
- Skip `ctest` step in production mode

### 3. macOS Compatibility Issues ❌

**Issue**: Benchmark script uses GNU-specific commands

```bash
# Line 80: stat command differs
local image_size=$(stat -f%z "$image" 2>/dev/null || stat -c%s "$image" 2>/dev/null)

# Line 98: du command differs  
local dataset_size=$(du -sb "$DATASET" | cut -f1)  # -b doesn't exist on macOS

# Line 113: numfmt doesn't exist on macOS
echo "  - Image size: $(numfmt --to=iec-i --suffix=B $image_size)"
```

**Solution**: Add compatibility functions

```bash
get_file_size() {
  local file=$1
  if [[ "$OSTYPE" == "darwin"* ]]; then
    stat -f%z "$file"
  else
    stat -c%s "$file"
  fi
}

get_dir_size() {
  local dir=$1
  if [[ "$OSTYPE" == "darwin"* ]]; then
    find "$dir" -type f -exec stat -f%z {} + | awk '{sum+=$1} END {print sum}'
  else
    du -sb "$dir" | cut -f1
  fi
}

format_bytes() {
  local bytes=$1
  if command -v numfmt >/dev/null 2>&1; then
    numfmt --to=iec-i --suffix=B "$bytes"
  else
    # Fallback: simple formatter
    awk -v bytes="$bytes" 'BEGIN {
      units[1]="B"; units[2]="KiB"; units[3]="MiB"; units[4]="GiB"
      i=1; val=bytes
      while(val>=1024 && i<4) { val/=1024; i++ }
      printf "%.1f%s", val, units[i]
    }'
  fi
}
```

### 4. Dependency Management via vcpkg ❌

**Issue**: Currently using mixed dependency sources:
- pkg-config for system libraries (libcrypto, libarchive, etc.)
- FetchContent for header-only libs (range-v3, fmt, googletest)
- Git submodules for Folly/Thrift
- Homebrew on macOS

**Goal**: Migrate to **vcpkg** for ALL dependencies

**Benefits**:
- ✅ Static linking compatible
- ✅ Cross-platform consistency  
- ✅ Version pinning
- ✅ Single dependency manager

**Implementation Plan**:

#### Step 1: Create `vcpkg.json`

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg/master/scripts/vcpkg.schema.json",
  "name": "dwarfs",
  "version-semver": "0.14.1",
  "description": "Fast high-compression read-only file system",
  "homepage": "https://github.com/tamatebako/dwarfs",
  "license": "GPL-3.0-or-later",
  "dependencies": [
    {
      "name": "boost",
      "version>=": "1.67.0",
      "default-features": false,
      "features": ["chrono", "program-options", "filesystem", "process"]
    },
    {
      "name": "openssl",
      "version>=": "3.0.0"
    },
    {
      "name": "libarchive",
      "version>=": "3.6.0",
      "default-features": false,
      "features": ["lz4", "zstd", "bzip2", "lzma"]
    },
    {
      "name": "xxhash",
      "version>=": "0.8.1"
    },
    {
      "name": "zstd",
      "version>=": "1.4.8"
    },
    {
      "name": "lz4",
      "version>=": "1.9.3"
    },
    {
      "name": "liblzma",
      "version>=": "5.2.5"
    },
    {
      "name": "brotli",
      "version>=": "1.0.9"
    },
    {
      "name": "flac",
      "version>=": "1.4.2"
    },
    {
      "name": "fmt",
      "version>=": "10.0"
    },
    {
      "name": "range-v3",
      "version>=": "0.12.0"
    },
    {
      "name": "parallel-hashmap",
      "version>=": "1.3.8"
    },
    {
      "name": "nlohmann-json",
      "version>=": "3.11.0"
    },
    {
      "name": "gtest",
      "version>=": "1.13.0"
    },
    {
      "name": "jemalloc",
      "platform": "!windows & !osx-arm64"
    }
  ],
  "features": {
    "thrift": {
      "description": "Enable Thrift metadata format support",
      "dependencies": [
        {
          "name": "folly",
          "default-features": false,
          "features": ["static"]
        },
        "fbthrift"
      ]
    },
    "fuse": {
      "description": "FUSE driver support",
      "dependencies": [
        {
          "name": "fuse3",
          "platform": "linux"
        },
        {
          "name": "winfsp",
          "platform": "windows"
        }
      ]
    }
  }
}
```

#### Step 2: Update CMakeLists.txt

Replace pkg-config calls with `find_package()` when using vcpkg:

```cmake
# Before:
pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash>=${XXHASH_REQUIRED_VERSION})

# After (vcpkg-aware):
if(VCPKG_TOOLCHAIN)
  find_package(xxHash ${XXHASH_REQUIRED_VERSION} REQUIRED CONFIG)
  add_library(PkgConfig::XXHASH ALIAS xxHash::xxhash)
else()
  pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash>=${XXHASH_REQUIRED_VERSION})
endif()
```

#### Step 3: Conditional FetchContent

Only use FetchContent as fallback if vcpkg not available:

```cmake
# cmake/need_range_v3.cmake
if(NOT VCPKG_TOOLCHAIN)
  find_package(range-v3 ${RANGE_V3_REQUIRED_VERSION} QUIET CONFIG)
endif()

if(NOT range-v3_FOUND)
  # FetchContent fallback...
endif()
```

### 5. Folly Build Verbosity ⚠️

**Issue**: Folly/Thrift builds produce 1000+ lines of warnings overwhelming the output

**Solution**: Redirect Folly/Thrift CMake output to log file

```bash
# scripts/build-all-and-test.sh

build_config() {
  # ...
  
  # For both/thrift configs, suppress Folly warnings
  if [[ "$thrift" == "ON" ]]; then
    echo -e "${YELLOW}Configuring (Folly output redirected to $build_dir/folly-build.log)...${NC}"
    cmake -B "$build_dir" ... > "$build_dir/folly-build.log" 2>&1
  else
    echo -e "${YELLOW}Configuring...${NC}"
    cmake -B "$build_dir" ...
  fi
}
```

---

## Implementation Priority

### Phase 1: Quick Fixes (1 hour) 🔥
1. ✅ Fix test registration in build scripts
2. ✅ Add production/debug modes  
3. ✅ Fix macOS compatibility in benchmark script

### Phase 2: vcpkg Integration (2-3 hours) 🎯
1. ✅ Create vcpkg.json manifest
2. ✅ Update CMakeLists.txt for vcpkg
3. ✅ Add vcpkg detection logic
4. ✅ Test on macOS + Linux

### Phase 3: Build Optimization (0.5 hour) 🚀
1. ✅ Add Folly output suppression
2. ✅ Add build time tracking
3. ✅ Update documentation

---

## Testing Plan

### Phase 1 Testing
```bash
# Production mode (no tests)
./scripts/run-all.sh example/pg11339-h

# Debug mode (with tests)
./scripts/run-all.sh --debug example/pg11339-h

# Verify all tools built
ls -lh build-*/mkdwarfs build-*/dwarfsck build-*/dwarfsextract

# Verify benchmarks complete
cat benchmarks/results/*/benchmark-report.md
```

### Phase 2 Testing
```bash
# Clean build with vcpkg
rm -rf build-* vcpkg_installed/
cmake -B build-vcpkg-test -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
cmake --build build-vcpkg-test -j8

# Verify no FetchContent used
grep "FetchContent" build-vcpkg-test/CMakeCache.txt || echo "✓ No FetchContent"
```

### Phase 3 Testing
```bash
# Time builds
time ./scripts/build-all-and-test.sh

# Check log size
wc -l build-both/folly-build.log  # Should be 1000+ lines
grep -c "warning:" build-both/folly-build.log || echo "0 warnings in console"
```

---

## Files to Modify

### Critical Files
- ✅ [`scripts/build-all-and-test.sh`](../scripts/build-all-and-test.sh) - Add test building, debug mode
- ✅ [`scripts/run-all.sh`](../scripts/run-all.sh) - Add debug mode flag
- ✅ [`scripts/benchmark-all.sh`](../scripts/benchmark-all.sh) - Fix macOS compatibility
- ✅ `vcpkg.json` - NEW: Dependency manifest
- ✅ [`CMakeLists.txt`](../CMakeLists.txt) - Add vcpkg detection

### Optional Files  
- 📝 [`cmake/need_fmt.cmake`](../cmake/need_fmt.cmake) - Add vcpkg support
- 📝 [`cmake/need_range_v3.cmake`](../cmake/need_range_v3.cmake) - Add vcpkg support
- 📝 [`cmake/need_phmap.cmake`](../cmake/need_phmap.cmake) - Add vcpkg support
- 📝 [`cmake/need_gtest.cmake`](../cmake/need_gtest.cmake) - Add vcpkg support

---

## Success Criteria

✅ **Phase 1 Complete** when:
- Production mode runs WITHOUT building tests
- Debug mode builds AND runs tests successfully
- Benchmark script completes on macOS with "✨ All done!"
- No "NOT_BUILT" test errors

✅ **Phase 2 Complete** when:
- vcpkg.json successfully describes all dependencies
- CMake finds ALL dependencies via vcpkg (no FetchContent)
- Builds work on macOS and Linux with vcpkg
- Static linking works with vcpkg

✅ **Phase 3 Complete** when:
- Folly warnings don't spam console (redirected to log)
- Build times are tracked and reported
- Documentation updated with vcpkg instructions

---

## Risk Assessment

### Low Risk ✅
- Test registration fix (isolated to scripts)
- macOS compatibility (backward compatible)
- Production/debug modes (additive change)

### Medium Risk ⚠️
- Folly output redirection (might break error detection)
- vcpkg integration (requires testing on multiple platforms)

### High Risk 🔴  
- Removing FetchContent entirely (breaking change for non-vcpkg users)

**Mitigation**: Keep FetchContent as fallback for non-vcpkg builds

---

## Next Steps

1. Review this plan with user
2. Get confirmation on scope (Phase 1 only? or all phases?)
3. Start with Phase 1 (quick wins)
4. Test thoroughly before Phase 2
5. Consider Phase 2 as separate session if time-consuming

---

**Status**: Ready for implementation  
**Estimated Time**: 
- Phase 1: 1 hour
- Phase 2: 2-3 hours  
- Phase 3: 0.5 hour
- **Total**: 3.5-4.5 hours