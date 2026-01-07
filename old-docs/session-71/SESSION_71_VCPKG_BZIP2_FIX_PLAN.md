# Session 71: vcpkg BZip2 Fix - Systematic Approach

**Created**: 2026-01-03
**Status**: 🔴 **PLANNED** - Ready to execute
**Goal**: Enable Modern Thrift builds via vcpkg by resolving BZip2 dependency conflict

---

## Problem Analysis

### Root Cause
```
FBThrift → mvfst → Boost → boost-iostreams → BZip2
                                               ↓
                             vcpkg toolchain sets CMAKE_DISABLE_FIND_PACKAGE_BZip2=ON
                                               ↓
                                    Configuration fails
```

### Why Current Approaches Failed
1. ❌ Adding `bzip2` to `vcpkg.json` - Toolchain override persists
2. ❌ Overlay port for `boost-iostreams` - Library still not linked
3. ❌ Manual CMake flags - Toolchain ignores them
4. ❌ Feature manipulation - Toolchain enforces locks

### Core Issue
vcpkg's toolchain (`scripts/buildsystems/vcpkg.cmake`) globally disables BZip2 package finding to prevent conflicts, but boost-iostreams requires it as a dependency.

---

## Solution Strategy: Custom Triplet

**Approach**: Create a custom vcpkg triplet that allows BZip2 while maintaining vcpkg's dependency management.

### Why This Works
1. ✅ Triplets override toolchain behavior
2. ✅ Can selectively enable packages
3. ✅ Maintains vcpkg dependency tracking
4. ✅ Portable across platforms

---

## Implementation Plan

### Phase 1: Create Custom Triplet (15 min)

**File**: `triplets/community/arm64-osx-dwarfs.cmake`

```cmake
# Base triplet
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)

# CRITICAL: Allow BZip2 for boost-iostreams
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_DISABLE_FIND_PACKAGE_BZip2=OFF"
)

# Ensure BZip2 is available
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/chainloads/osx-bzip2.cmake")
```

**Chainload File**: `triplets/chainloads/osx-bzip2.cmake`

```cmake
# Enable BZip2 explicitly
set(CMAKE_REQUIRE_FIND_PACKAGE_BZip2 TRUE)
set(CMAKE_DISABLE_FIND_PACKAGE_BZip2 OFF CACHE BOOL "" FORCE)

# Ensure BZip2 paths are visible
if(EXISTS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include/bzlib.h")
    set(BZIP2_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include" CACHE PATH "")
    set(BZIP2_LIBRARIES "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/libbz2.a" CACHE FILEPATH "")
    set(BZIP2_FOUND TRUE CACHE BOOL "")
endif()
```

### Phase 2: Update vcpkg.json (5 min)

Add BZip2 as explicit dependency:

```json
{
  "name": "dwarfs",
  "dependencies": [
    "boost-iostreams",
    "bzip2",
    "folly",
    "fbthrift",
    ...
  ]
}
```

### Phase 3: Update CMakeLists.txt (10 min)

Add triplet selection logic:

```cmake
# Near top of CMakeLists.txt (after project())
if(DWARFS_WITH_THRIFT AND VCPKG_TARGET_TRIPLET)
    # Ensure BZip2 is available for boost-iostreams
    if(VCPKG_TARGET_TRIPLET MATCHES "osx-dwarfs$")
        message(STATUS "Using custom dwarfs triplet with BZip2 support")
    else()
        message(WARNING "Consider using *-osx-dwarfs triplet for Modern Thrift builds")
    endif()
endif()

# In metadata_serialization.cmake
if(DWARFS_WITH_THRIFT)
    # Find BZip2 if using vcpkg
    if(VCPKG_TARGET_TRIPLET)
        find_package(BZip2 REQUIRED)
        message(STATUS "BZip2 found: ${BZIP2_LIBRARIES}")
    endif()
endif()
```

### Phase 4: Test Build (30 min)

```bash
# 1. Install vcpkg dependencies with custom triplet
export VCPKG_DEFAULT_TRIPLET=arm64-osx-dwarfs
vcpkg install --triplet arm64-osx-dwarfs

# 2. Configure DwarFS
cmake -B build-modern-thrift -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-dwarfs \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON

# 3. Build
ninja -C build-modern-thrift

# 4. Test
ctest --test-dir build-modern-thrift --tests-regex "metadata"
```

### Phase 5: Validation (15 min)

```bash
# Test Modern Thrift metadata creation
./build-modern-thrift/mkdwarfs \
  -i test/data/small \
  -o test-modern.dft \
  --format=modern-thrift

# Verify format
./build-modern-thrift/dwarfsck test-modern.dft --json | jq '.metadata.format'
# Expected: "modern_thrift_compact"

# Test detection
./build-modern-thrift/dwarfsck test-modern.dft
# Should detect Modern Thrift via magic bytes {0x82, 0x21}
```

---

## Alternative: Platform-Specific Approach

If custom triplet fails, use platform-specific workarounds:

### macOS (Homebrew BZip2)

```cmake
# cmake/metadata_serialization.cmake
if(APPLE AND DWARFS_WITH_THRIFT)
    # Use Homebrew BZip2 instead of vcpkg
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(BZIP2 REQUIRED bzip2)
    
    # Override vcpkg's lock
    set(CMAKE_DISABLE_FIND_PACKAGE_BZip2 OFF CACHE BOOL "" FORCE)
    set(BZIP2_FOUND TRUE)
endif()
```

### Linux (System BZip2)

Similar approach using system package manager.

---

## Success Criteria

1. ✅ `cmake` configuration succeeds with Modern Thrift enabled
2. ✅ BZip2 found and linked to boost-iostreams
3. ✅ All 66/66 metadata tests pass
4. ✅ Can create Modern Thrift images (`.dft` with `{0x82, 0x21}` magic)
5. ✅ Can read Modern Thrift images
6. ✅ Format detection works correctly

---

## Rollback Plan

If all approaches fail:
1. Document Modern Thrift as "manual build only"
2. Release v0.17.0 with FlatBuffers + Legacy Thrift
3. Defer Modern Thrift vcpkg to v0.17.1
4. File issue with vcpkg/boost-iostreams maintainers

---

## Time Estimate

| Phase | Time | Cumulative |
|-------|------|------------|
| 1. Custom triplet | 15 min | 15 min |
| 2. Update vcpkg.json | 5 min | 20 min |
| 3. Update CMake | 10 min | 30 min |
| 4. Test build | 30 min | 60 min |
| 5. Validation | 15 min | 75 min |
| **Total** | **75 min** | **1h 15m** |

---

## Next Session Start

```bash
# Read this plan
cat doc/SESSION_71_VCPKG_BZIP2_FIX_PLAN.md

# Start with Phase 1: Create custom triplet
mkdir -p triplets/community
mkdir -p triplets/chainloads
```

---

**References**:
- vcpkg triplets: https://learn.microsoft.com/en-us/vcpkg/users/triplets
- vcpkg chainloading: https://learn.microsoft.com/en-us/vcpkg/users/triplets#chainload-toolchains
- boost-iostreams: https://github.com/microsoft/vcpkg/tree/master/ports/boost-iostreams

**Created**: 2026-01-03 15:15 HKT
**Status**: Ready to execute
