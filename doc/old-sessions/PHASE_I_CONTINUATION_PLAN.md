# Phase I: vcpkg Integration - Continuation Plan

**Created**: 2025-12-01 22:43 HKT  
**Status**: Ready to Start  
**Priority**: HIGH - Only blocking phase before production release  
**Estimated Time**: 4-6 hours

---

## Overview

Phase I implements vcpkg integration to enable easy distribution and installation of DwarFS libraries and tools via the vcpkg package manager. This is the **final blocking phase** before production release v0.16.0.

## Prerequisites - ALL MET ✅

- ✅ Phase A-F: Metadata serialization complete
- ✅ Phase G: Comprehensive benchmark suite complete
- ✅ Phase H: 100% test pass rate achieved
- ✅ Phase K: Compression benchmarking complete
- ✅ FlatBuffers as default format (Thrift optional)
- ✅ Rice++ Thrift independence achieved
- ✅ Clean architecture with 5 modular libraries

## Goals

### Primary Goals
1. Create vcpkg ports for libdwarfs (libraries)
2. Create vcpkg ports for dwarfs (tools)
3. Implement CMake package config for downstream projects
4. Test installation and usage workflows
5. Document vcpkg installation process

### Success Criteria
- [ ] vcpkg ports created and tested
- [ ] Libraries installable via `vcpkg install libdwarfs`
- [ ] Tools installable via `vcpkg install dwarfs`
- [ ] CMake `find_package(dwarfs)` works
- [ ] Usage examples documented
- [ ] CI/CD tests vcpkg installation

---

## Implementation Plan

### Task I.1: Port Structure Setup (30 min)

**Objective**: Create vcpkg port directory structure

**Actions**:
```bash
# Create port directories
mkdir -p ports/libdwarfs
mkdir -p ports/dwarfs

# Create required files
touch ports/libdwarfs/{portfile.cmake,vcpkg.json,usage}
touch ports/dwarfs/{portfile.cmake,vcpkg.json,usage}
```

**Deliverables**:
- [ ] `ports/libdwarfs/` directory created
- [ ] `ports/dwarfs/` directory created
- [ ] Placeholder files created

---

### Task I.2: libdwarfs Port Implementation (2 hours)

**Objective**: Implement vcpkg port for DwarFS libraries

#### I.2.1: vcpkg.json Manifest

**File**: `ports/libdwarfs/vcpkg.json`

**Content Structure**:
```json
{
  "name": "libdwarfs",
  "version-string": "0.16.0",
  "description": "Fast, high-compression read-only file system - Libraries",
  "homepage": "https://github.com/tamatebako/dwarfs",
  "dependencies": [
    "boost-filesystem",
    "boost-program-options",
    "boost-iostreams",
    "boost-context",
    "openssl",
    "libarchive",
    "xxhash",
    "zstd",
    "fmt",
    "gtest",
    "range-v3",
    "parallel-hashmap"
  ],
  "features": {
    "flac": {
      "description": "FLAC compression support",
      "dependencies": ["libflac"]
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

**Implementation**:
1. Define base dependencies (required)
2. Define optional features (compression algorithms)
3. Specify version and metadata
4. Test JSON validity

#### I.2.2: portfile.cmake Build Script

**File**: `ports/libdwarfs/portfile.cmake`

**Content Structure**:
```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/dwarfs
    REF v${VERSION}
    SHA512 <hash-will-be-computed>
    HEAD_REF main
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        flac        TRY_ENABLE_FLAC
        lz4         TRY_ENABLE_LZ4
        lzma        TRY_ENABLE_LZMA
        brotli      TRY_ENABLE_BROTLI
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWITH_LIBDWARFS=ON
        -DWITH_TOOLS=OFF
        -DWITH_FUSE_DRIVER=OFF
        -DWITH_TESTS=OFF
        -DDWARFS_WITH_FLATBUFFERS=ON
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME dwarfs CONFIG_PATH lib/cmake/dwarfs)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(INSTALL "${SOURCE_PATH}/LICENSE.GPL-3.0" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
```

**Implementation Steps**:
1. Configure GitHub source fetching
2. Map vcpkg features to CMake options
3. Configure CMake with library-only build
4. Install and fix up CMake config files
5. Clean up debug headers
6. Install license

#### I.2.3: Usage Documentation

**File**: `ports/libdwarfs/usage`

**Content**:
```
libdwarfs provides CMake targets:

    find_package(dwarfs CONFIG REQUIRED)
    target_link_libraries(main PRIVATE 
        dwarfs::dwarfs_common
        dwarfs::dwarfs_reader
        dwarfs::dwarfs_writer
        dwarfs::dwarfs_extractor
        dwarfs::dwarfs_rewrite
    )

Available libraries:
- dwarfs_common: Core utilities and compression
- dwarfs_reader: Read DwarFS images
- dwarfs_writer: Create DwarFS images
- dwarfs_extractor: Extract from DwarFS images
- dwarfs_rewrite: Recompress DwarFS images
```

---

### Task I.3: dwarfs Port Implementation (1 hour)

**Objective**: Implement vcpkg port for DwarFS tools

#### I.3.1: vcpkg.json Manifest

**File**: `ports/dwarfs/vcpkg.json`

**Content Structure**:
```json
{
  "name": "dwarfs",
  "version-string": "0.16.0",
  "description": "Fast, high-compression read-only file system - Tools",
  "homepage": "https://github.com/tamatebako/dwarfs",
  "dependencies": [
    "libdwarfs"
  ],
  "features": {
    "fuse": {
      "description": "FUSE driver support",
      "dependencies": [
        {
          "name": "fuse3",
          "platform": "linux"
        }
      ]
    }
  }
}
```

**Implementation**:
1. Define dependency on libdwarfs
2. Add FUSE feature (platform-specific)
3. Specify version and metadata

#### I.3.2: portfile.cmake Build Script

**File**: `ports/dwarfs/portfile.cmake`

**Content Structure**:
```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/dwarfs
    REF v${VERSION}
    SHA512 <hash-will-be-computed>
    HEAD_REF main
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        fuse        WITH_FUSE_DRIVER
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DWITH_LIBDWARFS=OFF
        -DWITH_TOOLS=ON
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_copy_tools(TOOL_NAMES mkdwarfs dwarfsck dwarfsextract AUTO_CLEAN)

if("fuse" IN_LIST FEATURES)
    vcpkg_copy_tools(TOOL_NAMES dwarfs AUTO_CLEAN)
endif()

file(INSTALL "${SOURCE_PATH}/LICENSE.GPL-3.0" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
```

**Implementation Steps**:
1. Configure GitHub source fetching
2. Map FUSE feature to CMake option
3. Configure CMake with tools-only build
4. Install tools to bin/
5. Conditionally install FUSE driver
6. Install license

#### I.3.3: Usage Documentation

**File**: `ports/dwarfs/usage`

**Content**:
```
dwarfs provides command-line tools:

Tools installed:
- mkdwarfs: Create DwarFS filesystem images
- dwarfsck: Check and inspect DwarFS images
- dwarfsextract: Extract files from DwarFS images
- dwarfs: Mount DwarFS images (if FUSE feature enabled)

Usage examples:
    mkdwarfs -i /path/to/source -o output.dwarfs
    dwarfsck output.dwarfs
    dwarfsextract -i output.dwarfs -o /path/to/destination
    dwarfs output.dwarfs /mnt/point  # requires FUSE
```

---

### Task I.4: CMake Package Config (1 hour)

**Objective**: Create CMake package config for downstream projects

#### I.4.1: dwarfsConfig.cmake.in Template

**File**: `cmake/dwarfsConfig.cmake.in`

**Content Structure**:
```cmake
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)

# Required dependencies
find_dependency(Boost REQUIRED COMPONENTS filesystem program_options iostreams)
find_dependency(OpenSSL REQUIRED)
find_dependency(LibArchive REQUIRED)
find_dependency(xxHash REQUIRED)
find_dependency(zstd REQUIRED)
find_dependency(fmt REQUIRED)

# Optional dependencies
if(@TRY_ENABLE_FLAC@)
    find_dependency(FLAC)
endif()

if(@TRY_ENABLE_LZ4@)
    find_dependency(lz4)
endif()

if(@TRY_ENABLE_LZMA@)
    find_dependency(LibLZMA)
endif()

if(@TRY_ENABLE_BROTLI@)
    find_dependency(brotli)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/dwarfsTargets.cmake")

check_required_components(dwarfs)
```

**Implementation**:
1. Define package initialization
2. Find all required dependencies
3. Conditionally find optional dependencies
4. Include exported targets
5. Validate components

#### I.4.2: Update CMakeLists.txt

**File**: `CMakeLists.txt`

**Modifications Required**:
```cmake
# Add near end of file, before tests

# Export targets for find_package()
install(EXPORT dwarfsTargets
    FILE dwarfsTargets-cmake
    NAMESPACE dwarfs::
    DESTINATION lib/cmake/dwarfs
)

# Generate package config
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/dwarfsConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfsConfig.cmake
    INSTALL_DESTINATION lib/cmake/dwarfs
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfsConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfsConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/dwarfsConfigVersion.cmake
    DESTINATION lib/cmake/dwarfs
)
```

**Implementation Steps**:
1. Add export directives to library targets
2. Install export files
3. Generate package config from template
4. Generate version file
5. Install config files

---

### Task I.5: Testing & Validation (1 hour)

**Objective**: Test vcpkg installation and usage

#### I.5.1: Local Testing

**Test Script**: `scripts/test_vcpkg_install.sh`

```bash
#!/bin/bash
set -e

# Ensure vcpkg is installed
if [ ! -d ~/vcpkg ]; then
    echo "Installing vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
    cd ~/vcpkg && ./bootstrap-vcpkg.sh
fi

# Test libdwarfs port
echo "Testing libdwarfs port..."
~/vcpkg/vcpkg install libdwarfs \
    --overlay-ports=./ports \
    --triplet=x64-linux

# Test dwarfs port
echo "Testing dwarfs port..."
~/vcpkg/vcpkg install dwarfs \
    --overlay-ports=./ports \
    --triplet=x64-linux

# Test CMake integration
echo "Testing CMake find_package..."
mkdir -p /tmp/dwarfs-test && cd /tmp/dwarfs-test
cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(dwarfs_test)
find_package(dwarfs CONFIG REQUIRED)
add_executable(test main.cpp)
target_link_libraries(test PRIVATE dwarfs::dwarfs_common)
EOF

cat > main.cpp <<'EOF'
#include <dwarfs/logger.h>
int main() {
    auto lgr = dwarfs::stream_logger::create(std::cerr);
    return 0;
}
EOF

cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build

echo "✅ All vcpkg tests passed!"
```

**Implementation**:
1. Create test script
2. Test libdwarfs installation
3. Test dwarfs installation
4. Test CMake find_package()
5. Build minimal test program
6. Verify linking works

#### I.5.2: CI/CD Integration

**File**: `.github/workflows/build.yml`

**Add New Job**:
```yaml
vcpkg-test:
  name: Test vcpkg Installation
  runs-on: ubuntu-24.04
  steps:
    - uses: actions/checkout@v4
    
    - name: Install vcpkg
      run: |
        git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
        cd ~/vcpkg && ./bootstrap-vcpkg.sh
    
    - name: Test libdwarfs port
      run: |
        ~/vcpkg/vcpkg install libdwarfs \
          --overlay-ports=./ports \
          --triplet=x64-linux
    
    - name: Test dwarfs port
      run: |
        ~/vcpkg/vcpkg install dwarfs \
          --overlay-ports=./ports \
          --triplet=x64-linux
    
    - name: Test CMake integration
      run: ./scripts/test_vcpkg_install.sh
```

---

### Task I.6: Documentation (30 min)

**Objective**: Document vcpkg installation and usage

#### I.6.1: Update README.md

**Section to Add**: "Installation via vcpkg"

```markdown
## Installation via vcpkg

### Installing Libraries

```bash
vcpkg install libdwarfs
```

Optional features:
```bash
vcpkg install libdwarfs[flac,lz4,lzma,brotli]
```

### Installing Tools

```bash
vcpkg install dwarfs
```

With FUSE driver (Linux only):
```bash
vcpkg install dwarfs[fuse]
```

### Using in CMake Projects

```cmake
find_package(dwarfs CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE dwarfs::dwarfs_reader)
```

See [`ports/libdwarfs/usage`](ports/libdwarfs/usage) for complete API reference.
```

#### I.6.2: Create vcpkg Guide

**File**: `doc/VCPKG_INSTALLATION.md`

**Content**: Complete installation guide including:
- Prerequisites
- Installation steps
- Feature selection
- Troubleshooting
- Examples

---

## Validation Checklist

### Port Files
- [ ] `ports/libdwarfs/vcpkg.json` valid JSON
- [ ] `ports/libdwarfs/portfile.cmake` valid CMake
- [ ] `ports/libdwarfs/usage` clear and complete
- [ ] `ports/dwarfs/vcpkg.json` valid JSON
- [ ] `ports/dwarfs/portfile.cmake` valid CMake
- [ ] `ports/dwarfs/usage` clear and complete

### CMake Integration
- [ ] `cmake/dwarfsConfig.cmake.in` created
- [ ] CMakeLists.txt exports targets
- [ ] CMakeLists.txt installs config files
- [ ] find_package(dwarfs) works

### Testing
- [ ] libdwarfs installs successfully
- [ ] dwarfs installs successfully
- [ ] Tools are accessible
- [ ] CMake integration works
- [ ] Test program compiles and links

### Documentation
- [ ] README.md updated
- [ ] vcpkg installation guide created
- [ ] Usage examples provided
- [ ] CI/CD integrated

---

## Dependencies & Constraints

### vcpkg Requirements
- vcpkg installed and bootstrapped
- Git access to GitHub repositories
- CMake ≥3.20
- Compiler: GCC ≥10, Clang ≥12, MSVC ≥19.29

### Platform Support
- **Linux**: Full support (all features)
- **macOS**: Full support (FUSE via FUSE-T)
- **Windows**: Full support (FUSE via WinFsp)
- **FreeBSD**: Via Linux emulation

### Build Configurations
- **Static linking**: Supported
- **Shared libraries**: Supported (default)
- **Feature selection**: Via vcpkg features

---

## Risk Assessment

### Low Risk
- Port structure is straightforward
- CMake export is well-defined
- Dependencies are all in vcpkg

### Medium Risk
- SHA512 hash needs computing for each version
- Platform-specific features (FUSE) may need testing
- Downstream CMake projects may have edge cases

### Mitigation Strategies
1. Test on all platforms in CI/CD
2. Provide clear error messages
3. Document troubleshooting steps
4. Test with real downstream projects

---

## Timeline

| Task | Duration | Dependencies |
|------|----------|--------------|
| **I.1** Port Structure | 30 min | None |
| **I.2** libdwarfs Port | 2 hours | I.1 |
| **I.3** dwarfs Port | 1 hour | I.2 |
| **I.4** CMake Config | 1 hour | I.2 |
| **I.5** Testing | 1 hour | I.2, I.3, I.4 |
| **I.6** Documentation | 30 min | All above |
| **Total** | **6 hours** | Sequential |

**Optimistic**: 4 hours (if ports work first try)  
**Realistic**: 6 hours (with debugging)  
**Pessimistic**: 8 hours (if major issues)

---

## Success Metrics

### Functional Metrics
- ✅ vcpkg install completes without errors
- ✅ All tools accessible after installation
- ✅ CMake find_package() succeeds
- ✅ Test programs compile and link
- ✅ CI/CD tests pass

### Quality Metrics
- ✅ Documentation is clear and complete
- ✅ Error messages are helpful
- ✅ Installation is smooth and intuitive
- ✅ Platform support is comprehensive

---

## Post-Phase I Actions

After Phase I completion:
1. Merge Phase I branch to main
2. Tag release v0.16.0
3. Submit ports to vcpkg upstream repository
4. Announce vcpkg availability
5. (Optional) Complete Phase J cleanup

---

**Status**: Ready to implement  
**Next Session**: Start with Task I.1 (Port Structure Setup)  
**Estimated Completion**: 4-6 hours from start