# Phase H & I: Fix Tests + vcpkg Integration - Implementation Plan

**Date**: 2025-11-30  
**Current Status**: Phase G complete, 5 tests failing, no vcpkg port  
**Next Phases**: H (Fix Tests) + I (vcpkg Integration)  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Deadline**: ASAP - Compressed timeline

---

## Problem Statement

**Current State**:
- ✅ Phase G complete: Comprehensive benchmark suite
- ✅ Libraries build successfully
- ⚠️ 5 tests failing (98.9% pass rate, need 100%)
- ❌ No vcpkg port available

**Required State**:
- ✅ 100% test pass rate (all 1,613 tests passing)
- ✅ vcpkg port for libdwarfs
- ✅ vcpkg port for dwarfs (FUSE driver)
- ✅ Production-ready packages

---

## Phase H: Fix Failing Tests (2-4 hours)

### H1: Analyze Failing Tests (30 min)

**Failing Tests** (from test run):
1. `filesystem_writer.compression_metadata_requirements`
2. `packed_int_vector.signed_int`
3. `time_resolution_converter.error_handling`
4. `utils.size_with_unit`
5. `utils.time_with_unit`

**Tasks**:
- [ ] Run each failing test individually with verbose output
- [ ] Identify root causes
- [ ] Categorize by type (logic error, assertion issue, platform-specific)

### H2: Fix filesystem_writer Test (30 min)

**File**: `test/filesystem_writer_test.cpp` or similar

**Likely Issues**:
- Metadata requirements validation
- FlatBuffers-specific checks
- Missing format detection

**Approach**:
- Read test code
- Understand expected behavior
- Fix implementation or test expectations
- Verify fix doesn't break other tests

### H3: Fix packed_int_vector Test (30 min)

**File**: `test/packed_int_vector_test.cpp` or similar

**Likely Issues**:
- Signed integer encoding/decoding
- Edge cases (INT_MIN, INT_MAX)
- Platform-specific behavior

### H4: Fix time_resolution_converter Test (30 min)

**File**: `test/time_resolution_converter_test.cpp` or similar

**Likely Issues**:
- Error handling expectations
- Exception types
- Platform-specific time handling

### H5: Fix utils Tests (1 hour)

**Files**: `test/utils_test.cpp` or similar

**Likely Issues**:
- Locale-specific formatting
- Platform-specific size units
- String parsing edge cases

**Common Pattern**: Likely formatting/parsing issues with:
- `size_with_unit`: "1.5 GB" → bytes conversion
- `time_with_unit`: "1h30m" → seconds conversion

### H6: Verify All Tests Pass (30 min)

**Tasks**:
- [ ] Run full test suite: `./dwarfs_unit_tests`
- [ ] Verify 1,613/1,613 tests pass
- [ ] Run on multiple architectures if available
- [ ] Document any platform-specific fixes

---

## Phase I: vcpkg Integration (4-6 hours)

### I1: vcpkg Port Structure (1 hour)

**Create Port Files**:
```
ports/
├── dwarfs/
│   ├── portfile.cmake
│   ├── vcpkg.json
│   └── usage
└── libdwarfs/
    ├── portfile.cmake
    ├── vcpkg.json
    └── usage
```

**Tasks**:
- [ ] Research vcpkg port requirements
- [ ] Study existing ports (libarchive, zstd, boost)
- [ ] Create port structure
- [ ] Define dependencies

### I2: libdwarfs vcpkg Port (2 hours)

**File**: `ports/libdwarfs/vcpkg.json`

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

**File**: `ports/libdwarfs/portfile.cmake`

```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/dwarfs
    REF v${VERSION}
    SHA512 <calculate>
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

**Tasks**:
- [ ] Create vcpkg.json with dependencies
- [ ] Create portfile.cmake
- [ ] Test local build
- [ ] Verify install files correct

### I3: dwarfs vcpkg Port (2 hours)

**File**: `ports/dwarfs/vcpkg.json`

```json
{
  "name": "dwarfs",
  "version": "0.16.0",
  "description": "DwarFS tools - fast high-compression read-only file system",
  "homepage": "https://github.com/tamatebako/dwarfs",
  "license": "MIT",
  "dependencies": [
    "libdwarfs",
    {
      "name": "fuse3",
      "platform": "linux"
    },
    {
      "name": "fuse-t",
      "platform": "osx"
    },
    {
      "name": "winfsp",
      "platform": "windows"
    }
  ]
}
```

**File**: `ports/dwarfs/portfile.cmake`

```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/dwarfs
    REF v${VERSION}
    SHA512 <calculate>
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWITH_LIBDWARFS=OFF
        -DWITH_TOOLS=ON
        -DWITH_FUSE_DRIVER=ON
        -DWITH_TESTS=OFF
)

vcpkg_cmake_install()
vcpkg_copy_tools(TOOL_NAMES mkdwarfs dwarfsck dwarfsextract dwarfs AUTO_CLEAN)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.GPL-3.0")
```

**Tasks**:
- [ ] Create vcpkg.json with dependencies
- [ ] Create portfile.cmake
- [ ] Test local build
- [ ] Verify tools installed correctly

### I4: CMake Export Configuration (1 hour)

**Create Config Files for vcpkg**:

**File**: `cmake/dwarfsConfig.cmake.in`

```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)
find_dependency(Boost REQUIRED COMPONENTS filesystem program_options chrono)
find_dependency(ZSTD REQUIRED)
find_dependency(OpenSSL REQUIRED)
find_dependency(LibArchive REQUIRED)
find_dependency(fmt REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/dwarfsTargets.cmake")
check_required_components(dwarfs)
```

**Update CMakeLists.txt**:

```cmake
# Add near end of file
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

install(EXPORT dwarfsTargets
    FILE dwarfsTargets.cmake
    NAMESPACE dwarfs::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dwarfs
)
```

**Tasks**:
- [ ] Create CMake config file
- [ ] Update CMakeLists.txt with export
- [ ] Test find_package(dwarfs) works
- [ ] Verify all targets exported

### I5: Test vcpkg Ports (1 hour)

**Local Testing**:

```bash
# Test libdwarfs
cd vcpkg
./vcpkg install libdwarfs --overlay-ports=../dwarfs/ports

# Test dwarfs
./vcpkg install dwarfs --overlay-ports=../dwarfs/ports

# Test features
./vcpkg install libdwarfs[flac,lz4] --overlay-ports=../dwarfs/ports
```

**Integration Testing**:

```cpp
// test_consumer/CMakeLists.txt
find_package(dwarfs CONFIG REQUIRED)
add_executable(test_app main.cpp)
target_link_libraries(test_app PRIVATE dwarfs::dwarfs_reader)
```

**Tasks**:
- [ ] Test local install
- [ ] Test consuming project
- [ ] Verify headers installed
- [ ] Verify libraries linkable

### I6: Submit to vcpkg Registry (Optional, 1 hour)

**If ready for public release**:

**Tasks**:
- [ ] Fork vcpkg repository
- [ ] Add ports to fork
- [ ] Test CI (vcpkg bot)
- [ ] Submit pull request
- [ ] Address review feedback

---

## Implementation Order

### Day 1 (6-8 hours)

**Morning** (3-4h):
1. H1: Analyze all 5 failing tests
2. H2-H5: Fix tests one by one
3. H6: Verify all tests pass

**Afternoon** (3-4h):
4. I1: Create vcpkg port structure
5. I2: Implement libdwarfs port
6. I3: Start dwarfs port

### Day 2 (2-3 hours)

7. I3: Complete dwarfs port
8. I4: CMake export configuration
9. I5: Test ports locally

---

## Success Criteria

### Phase H: Fix Tests
- [ ] All 1,613 tests passing (100% pass rate)
- [ ] No skipped tests in FlatBuffers-only build
- [ ] Tests stable across multiple runs
- [ ] Fixes don't introduce regressions

### Phase I: vcpkg Integration
- [ ] libdwarfs installs via vcpkg
- [ ] dwarfs tools install via vcpkg
- [ ] Consumer projects can link libdwarfs
- [ ] All dependencies resolved correctly
- [ ] Works on Linux, macOS, Windows

---

## Files to Create/Modify

### Phase H
- [ ] Various test files (identify during H1)
- [ ] Possibly fix implementation files

### Phase I
- [ ] `ports/libdwarfs/portfile.cmake`
- [ ] `ports/libdwarfs/vcpkg.json`
- [ ] `ports/libdwarfs/usage`
- [ ] `ports/dwarfs/portfile.cmake`
- [ ] `ports/dwarfs/vcpkg.json`
- [ ] `ports/dwarfs/usage`
- [ ] `cmake/dwarfsConfig.cmake.in`
- [ ] Update `CMakeLists.txt` (export targets)

---

## Risk Mitigation

### Risk: Tests require Thrift backend
**Impact**: Can't achieve 100% pass rate with FlatBuffers-only  
**Mitigation**: 
- Analyze if tests are backend-specific
- Port tests to work with both backends
- Document any backend-specific limitations

### Risk: vcpkg dependency conflicts
**Impact**: Port won't build in vcpkg environment  
**Mitigation**:
- Test with clean vcpkg installation
- Use vcpkg's dependency resolution
- Avoid specifying exact versions

### Risk: Platform-specific FUSE dependencies
**Impact**: dwarfs port won't work on all platforms  
**Mitigation**:
- Use platform-specific dependencies in vcpkg.json
- Make FUSE optional via feature
- Document platform requirements

---

## Documentation Updates (After Completion)

1. **README.md**:
   - Add "Installation via vcpkg" section
   - Update build instructions
   - Add CMake integration example

2. **doc/BUILDING.md** (create):
   - Comprehensive build guide
   - vcpkg installation
   - Manual build instructions
   - Troubleshooting

3. **Archive old docs**:
   - Move Phase G planning docs to `doc/old-docs/phase-g/`
   - Keep only active implementation status

---

**Estimated Total Time**: 8-12 hours over 1-2 days  
**Priority**: HIGH - Required for production readiness  
**Next Step**: Begin Phase H (Analyze failing tests)