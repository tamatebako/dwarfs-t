# Phase H & I: Fix Tests + vcpkg Integration - Continuation Prompt

**Date**: 2025-11-30  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Phase**: H & I - Fix Tests + vcpkg Integration

---

## Session Start

```bash
cd /Users/mulgogi/src/external/dwarfs
git branch --show-current
cat doc/PHASE_H_I_FIX_TESTS_VCPKG_PLAN.md
cat doc/PHASE_H_I_IMPLEMENTATION_STATUS.md
```

---

## Context

### Phases A-G Complete ✅
- FlatBuffers as default format (102.91%-108.63% of Thrift)
- Comprehensive benchmark suite working
- All libraries build successfully

### Current State
- **Test Status**: 1,595/1,613 passing (98.9%)
- **Failing**: 5 tests
- **vcpkg**: No port available yet

### Required Deliverables
1. **100% test pass rate** (all 1,613 tests)
2. **vcpkg port for libdwarfs** (library only)
3. **vcpkg port for dwarfs** (CLI tools + FUSE)

---

## Build Environments

| Build Dir | FlatBuffers | Thrift | Tests | Status |
|-----------|-------------|--------|-------|--------|
| `build-fb/` | ✅ | ❌ | ✅ | Tests enabled, 5 failing |

**Test Command**:
```bash
cd build-fb
./dwarfs_unit_tests  # Run all tests
```

---

## Phase H: Fix Failing Tests

### Failing Tests (5 total)

1. `filesystem_writer.compression_metadata_requirements`
2. `packed_int_vector.signed_int`
3. `time_resolution_converter.error_handling`
4. `utils.size_with_unit`
5. `utils.time_with_unit`

### H1: Analyze Each Test (START HERE)

Run each test individually with verbose output:

```bash
cd build-fb

# Test 1
./dwarfs_unit_tests --gtest_filter="filesystem_writer.compression_metadata_requirements" --gtest_print_time=0

# Test 2
./dwarfs_unit_tests --gtest_filter="packed_int_vector.signed_int" --gtest_print_time=0

# Test 3
./dwarfs_unit_tests --gtest_filter="time_resolution_converter.error_handling" --gtest_print_time=0

# Test 4
./dwarfs_unit_tests --gtest_filter="utils.size_with_unit" --gtest_print_time=0

# Test 5
./dwarfs_unit_tests --gtest_filter="utils.time_with_unit" --gtest_print_time=0
```

**For Each Test**:
1. Capture full output (assertions, expected vs actual)
2. Identify test file location
3. Read test source code
4. Determine if issue is in:
   - Test expectations (outdated after FlatBuffers changes)
   - Implementation (actual bug)
   - Platform-specific behavior (locale, formatting)

### H2-H5: Fix Tests

**Workflow per Test**:
1. Read test file: `test/*_test.cpp`
2. Understand what's being tested
3. Check implementation file if needed
4. Apply fix (test OR implementation)
5. Verify: `./dwarfs_unit_tests --gtest_filter="<test_name>"`
6. Verify no regressions: `./dwarfs_unit_tests` (full suite)

**Common Patterns**:
- **Formatting issues**: Locale-specific (size/time with unit tests)
- **FlatBuffers changes**: Metadata requirements may have changed
- **Signed integers**: Encoding/decoding edge cases

### H6: Final Verification

```bash
cd build-fb
./dwarfs_unit_tests

# Expected output:
# [==========] 1613 tests from 68 test suites ran.
# [  PASSED  ] 1613 tests.
# [  FAILED  ] 0 tests.
```

---

## Phase I: vcpkg Integration

### I1: Create Port Structure

```bash
cd /Users/mulgogi/src/external/dwarfs
mkdir -p ports/libdwarfs ports/dwarfs
```

### I2: libdwarfs Port (Library Only)

**Create**: `ports/libdwarfs/vcpkg.json`

```json
{
  "name": "libdwarfs",
  "version": "0.16.0",
  "description": "DwarFS library - fast high-compression read-only file system",
  "homepage": "https://github.com/tamatebako/dwarfs",
  "license": "MIT",
  "dependencies": [
    {
      "name": "vcpkg-cmake",
      "host": true
    },
    {
      "name": "vcpkg-cmake-config",
      "host": true
    },
    "boost-filesystem",
    "boost-program-options",
    "boost-chrono",
    "boost-context",
    "boost-fiber",
    "zstd",
    "xxhash",
    "openssl",
    "libarchive",
    "flatbuffers",
    "fmt",
    "range-v3",
    "parallel-hashmap"
  ],
  "features": {
    "flac": {
      "description": "FLAC audio compression support",
      "dependencies": ["flac"]
    },
    "lz4": {
      "description": "LZ4 compression support",
      "dependencies": ["lz4"]
    },
    "lzma": {
      "description": "LZMA compression support",
      "dependencies": ["liblzma"]
    },
    "brotli": {
      "description": "Brotli compression support",
      "dependencies": ["brotli"]
    }
  }
}
```

**Create**: `ports/libdwarfs/portfile.cmake`

```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/dwarfs
    REF v${VERSION}
    SHA512 0  # TODO: Calculate actual SHA512
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWITH_LIBDWARFS=ON
        -DWITH_TOOLS=OFF
        -DWITH_FUSE_DRIVER=OFF
        -DWITH_TESTS=OFF
        -DWITH_BENCHMARKS=OFF
        -DDWARFS_WITH_FLATBUFFERS=ON
        -DDWARFS_WITH_THRIFT=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/dwarfs)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.GPL-3.0")
```

**Create**: `ports/libdwarfs/usage`

```
libdwarfs provides CMake targets:

    find_package(dwarfs CONFIG REQUIRED)
    target_link_libraries(main PRIVATE dwarfs::dwarfs_reader dwarfs::dwarfs_writer)

Available targets:
  - dwarfs::dwarfs_common
  - dwarfs::dwarfs_reader
  - dwarfs::dwarfs_writer
  - dwarfs::dwarfs_extractor
```

### I3: dwarfs Port (CLI Tools)

**Create**: `ports/dwarfs/vcpkg.json`

```json
{
  "name": "dwarfs",
  "version": "0.16.0",
  "description": "DwarFS tools - mkdwarfs, dwarfsck, dwarfsextract, dwarfs (FUSE)",
  "homepage": "https://github.com/tamatebako/dwarfs",
  "license": "MIT",
  "dependencies": [
    "libdwarfs"
  ],
  "features": {
    "fuse": {
      "description": "FUSE driver (dwarfs command)",
      "dependencies": [
        {
          "name": "fuse3",
          "platform": "linux"
        },
        {
          "name": "fuse-t",
          "platform": "osx"
        }
      ]
    }
  }
}
```

**Create**: `ports/dwarfs/portfile.cmake`

```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/dwarfs
    REF v${VERSION}
    SHA512 0  # TODO: Calculate actual SHA512
    HEAD_REF main
)

set(FUSE_DRIVER OFF)
if("fuse" IN_LIST FEATURES)
    set(FUSE_DRIVER ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWITH_LIBDWARFS=OFF
        -DWITH_TOOLS=ON
        -DWITH_FUSE_DRIVER=${FUSE_DRIVER}
        -DWITH_TESTS=OFF
        -DDWARFS_WITH_FLATBUFFERS=ON
        -DDWARFS_WITH_THRIFT=OFF
)

vcpkg_cmake_install()

set(TOOLS mkdwarfs dwarfsck dwarfsextract)
if(FUSE_DRIVER)
    list(APPEND TOOLS dwarfs)
endif()

vcpkg_copy_tools(TOOL_NAMES ${TOOLS} AUTO_CLEAN)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.GPL-3.0")
```

**Create**: `ports/dwarfs/usage`

```
dwarfs provides command-line tools:
  - mkdwarfs: Create DwarFS filesystem images
  - dwarfsck: Check and inspect DwarFS images
  - dwarfsextract: Extract files from DwarFS images
  - dwarfs: Mount DwarFS images (with fuse feature)
```

### I4: CMake Export Configuration

**Create**: `cmake/dwarfsConfig.cmake.in`

```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)

# Find dependencies
find_dependency(Boost REQUIRED COMPONENTS filesystem program_options chrono context fiber)
find_dependency(ZSTD REQUIRED)
find_dependency(OpenSSL REQUIRED)
find_dependency(LibArchive REQUIRED)
find_dependency(xxHash REQUIRED)
find_dependency(fmt REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/dwarfsTargets.cmake")

check_required_components(dwarfs)
```

**Update**: `CMakeLists.txt` (add near end, before packaging section)

```cmake
# CMake package configuration for installation
include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/dwarfsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfsConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dwarfs
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfsConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dwarfs
)

# Export targets for find_package()
install(EXPORT dwarfsTargets
    FILE dwarfsTargets.cmake
    NAMESPACE dwarfs::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dwarfs
)
```

**Also update library targets to use EXPORT**:

```cmake
# For each library (dwarfs_common, dwarfs_reader, etc.)
install(TARGETS dwarfs_common
    EXPORT dwarfsTargets  # Add this
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
```

### I5: Test vcpkg Ports

```bash
# Install vcpkg if not already
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# Test libdwarfs
cd /Users/mulgogi/src/external/dwarfs
~/vcpkg/vcpkg install libdwarfs --overlay-ports=./ports

# Test dwarfs
~/vcpkg/vcpkg install dwarfs --overlay-ports=./ports

# Test with features
~/vcpkg/vcpkg install libdwarfs[flac,lz4] --overlay-ports=./ports

# Verify installation
~/vcpkg/vcpkg list | grep dwarfs
```

**Create Test Consumer**:

```bash
mkdir -p test_consumer
cd test_consumer

# CMakeLists.txt
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.28)
project(dwarfs_test)

find_package(dwarfs CONFIG REQUIRED)

add_executable(test_reader main.cpp)
target_link_libraries(test_reader PRIVATE dwarfs::dwarfs_reader)
EOF

# main.cpp
cat > main.cpp << 'EOF'
#include <dwarfs/reader/filesystem_loader.h>
#include <iostream>

int main() {
    std::cout << "DwarFS library linked successfully!" << std::endl;
    return 0;
}
EOF

# Build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/test_reader
```

---

## Common Issues

### Test Fixes

**Issue**: Locale-specific formatting  
**Solution**: Use "C" locale in tests or accept multiple formats

**Issue**: FlatBuffers metadata changes  
**Solution**: Update test expectations to match new format

### vcpkg Issues

**Issue**: SHA512 calculation  
**Solution**: 
```bash
cd /tmp
git clone https://github.com/tamatebako/dwarfs.git
cd dwarfs
git archive --format=tar.gz --prefix=dwarfs/ HEAD > ../dwarfs.tar.gz
shasum -a 512 ../dwarfs.tar.gz
```

**Issue**: Missing dependencies  
**Solution**: Add to vcpkg.json dependencies array

**Issue**: Tools not found  
**Solution**: Check `vcpkg_copy_tools()` parameters

---

## Validation Checklist

### Phase H Complete When:
- [ ] All 5 failing tests now pass
- [ ] No new test failures introduced
- [ ] `./dwarfs_unit_tests` shows 1613/1613 passing
- [ ] Tests documented in implementation status

### Phase I Complete When:
- [ ] `vcpkg install libdwarfs` succeeds
- [ ] `vcpkg install dwarfs` succeeds
- [ ] Test consumer project builds and links
- [ ] Headers installed to correct location
- [ ] Libraries findable via CMake
- [ ] Tools in PATH after vcpkg install

---

## Documentation Updates (After Completion)

After both phases complete:

1. Update `README.md`:
   - Add vcpkg installation instructions
   - Add CMake integration example
   - List all passing tests

2. Archive Phase G docs:
   ```bash
   mv doc/PHASE_G_*.md doc/old-docs/phase-g/
   ```

3. Update memory bank:
   ```
   .kilocode/rules/memory-bank/context.md
   ```

---

**Start with**: H1 - Analyze all 5 failing tests  
**Timeline**: 8-12 hours total  
**Priority**: HIGH - Required for production release

See full details in:
- [`doc/PHASE_H_I_FIX_TESTS_VCPKG_PLAN.md`](PHASE_H_I_FIX_TESTS_VCPKG_PLAN.md)
- [`doc/PHASE_H_I_IMPLEMENTATION_STATUS.md`](PHASE_H_I_IMPLEMENTATION_STATUS.md)