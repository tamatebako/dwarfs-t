# Session 36: Static Build Infrastructure - Complete Implementation Plan

**Goal**: Enable fully static builds using vcpkg with Tebako-specific dependencies

**Date**: 2025-12-24
**Status**: Ready for Implementation

---

## CRITICAL REQUIREMENTS

### Mandatory Dependencies (ALL REQUIRED)
1. **jemalloc** - MANDATORY (Tebako fork only, NO FALLBACK)
2. **lz4** - MANDATORY (NOT optional)
3. **lzma** - MANDATORY (NOT optional)
4. **brotli** - MANDATORY (NOT optional)
5. **flac** - MANDATORY (NOT optional)

### Tebako-Specific Dependencies (vcpkg overlay ONLY)
- **jemalloc** → `tamatebako/jemalloc.git` (Windows + ARM support)
- **folly** → `tamatebako/folly.git` (mhx fork, for Thrift)
- **fbthrift** → `tamatebako/fbthrift.git` (mhx fork, for Thrift)

### All 6 Triplets Required
- x64-linux-static
- arm64-linux-static
- x64-osx-static
- arm64-osx-static
- x64-windows-static
- arm64-windows-static

---

## Phase 0: Core Infrastructure (3.5 hours)

### Step 1: Create vcpkg.json (10 min)

**File**: `vcpkg.json`

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg/master/scripts/vcpkg.schema.json",
  "name": "dwarfs",
  "version-semver": "0.14.1",
  "description": "Fast high-compression read-only file system (Tebako fork)",
  "homepage": "https://github.com/tamatebako/dwarfs",
  "license": "GPL-3.0-or-later",
  
  "dependencies": [
    {"name": "boost", "version>=": "1.67.0", "default-features": false, "features": ["chrono", "program-options", "filesystem", "process"]},
    {"name": "openssl", "version>=": "3.0.0"},
    {"name": "libarchive", "version>=": "3.6.0", "default-features": false, "features": ["lz4", "zstd", "bzip2", "lzma"]},
    {"name": "xxhash", "version>=": "0.8.1"},
    {"name": "zstd", "version>=": "1.4.8"},
    {"name": "lz4", "version>=": "1.9.3"},
    {"name": "liblzma", "version>=": "5.2.5"},
    {"name": "brotli", "version>=": "1.0.9"},
    {"name": "libflac", "version>=": "1.4.2"},
    {"name": "fmt", "version>=": "10.0"},
    {"name": "range-v3", "version>=": "0.12.0"},
    {"name": "parallel-hashmap", "version>=": "1.3.8"},
    {"name": "nlohmann-json", "version>=": "3.11.0"},
    {"name": "gtest", "version>=": "1.13.0"}
  ],
  
  "builtin-baseline": "latest"
}
```

### Step 2: Create vcpkg-configuration.json (5 min)

**File**: `vcpkg-configuration.json`

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg/master/scripts/vcpkg-configuration.schema.json",
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/microsoft/vcpkg",
    "baseline": "latest"
  },
  "registries": [],
  "overlay-triplets": ["vcpkg_triplets"],
  "overlay-ports": ["vcpkg_ports"]
}
```

### Step 3: Create 6 Triplet Files (20 min)

Create directory: `vcpkg_triplets/`

#### `vcpkg_triplets/x64-linux-static.cmake`

```cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_C_FLAGS "-fPIC")
set(VCPKG_CXX_FLAGS "-fPIC")
```

#### `vcpkg_triplets/arm64-linux-static.cmake`

```cmake
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_C_FLAGS "-fPIC")
set(VCPKG_CXX_FLAGS "-fPIC")
```

#### `vcpkg_triplets/x64-osx-static.cmake`

```cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES x86_64)
set(VCPKG_OSX_DEPLOYMENT_TARGET "10.15")
```

#### `vcpkg_triplets/arm64-osx-static.cmake`

```cmake
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_OSX_DEPLOYMENT_TARGET "11.0")
```

#### `vcpkg_triplets/x64-windows-static.cmake`

```cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Windows)
set(VCPKG_CXX_FLAGS "/MT")
set(VCPKG_C_FLAGS "/MT")
set(VCPKG_CXX_FLAGS_RELEASE "/MT")
set(VCPKG_C_FLAGS_RELEASE "/MT")
set(VCPKG_CXX_FLAGS_DEBUG "/MTd")
set(VCPKG_C_FLAGS_DEBUG "/MTd")
```

#### `vcpkg_triplets/arm64-windows-static.cmake`

```cmake
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Windows)
set(VCPKG_CXX_FLAGS "/MT")
set(VCPKG_C_FLAGS "/MT")
set(VCPKG_CXX_FLAGS_RELEASE "/MT")
set(VCPKG_C_FLAGS_RELEASE "/MT")
set(VCPKG_CXX_FLAGS_DEBUG "/MTd")
set(VCPKG_C_FLAGS_DEBUG "/MTd")
```

### Step 4: Create Overlay Ports (60 min)

Create directory: `vcpkg_ports/`

#### Overlay Port 1: jemalloc

Create `vcpkg_ports/jemalloc/vcpkg.json`:

```json
{
  "name": "jemalloc",
  "version": "5.3.0",
  "port-version": 1,
  "description": "jemalloc memory allocator (Tebako fork with Windows & ARM support)",
  "homepage": "https://github.com/tamatebako/jemalloc",
  "license": "BSD-2-Clause",
  "supports": "!uwp",
  "dependencies": []
}
```

Create `vcpkg_ports/jemalloc/portfile.cmake`:

```cmake
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/jemalloc
    REF v5.3.0-tebako
    SHA512 0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DJEMALLOC_BUILD_TESTS=OFF
        -DJEMALLOC_BUILD_DOC=OFF
        -DBUILD_SHARED_LIBS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/jemalloc)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
```

#### Overlay Port 2: folly

Create `vcpkg_ports/folly/vcpkg.json`:

```json
{
  "name": "folly",
  "version": "2024.01.15.00",
  "port-version": 1,
  "description": "Facebook's C++ library (Tebako/mhx fork)",
  "homepage": "https://github.com/tamatebako/folly",
  "license": "Apache-2.0",
  "supports": "!uwp",
  "dependencies": [
    {"name": "boost", "default-features": false, "features": ["context", "filesystem", "program-options", "regex", "system", "thread"]},
    "double-conversion",
    "fmt",
    "glog",
    {"name": "libevent", "default-features": false},
    "lz4",
    "snappy",
    "zlib",
    "zstd"
  ]
}
```

Create `vcpkg_ports/folly/portfile.cmake`:

```cmake
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/folly
    REF v2024.01.15.00-tebako
    SHA512 0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DFOLLY_USE_JEMALLOC=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/folly)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
```

#### Overlay Port 3: fbthrift

Create `vcpkg_ports/fbthrift/vcpkg.json`:

```json
{
  "name": "fbthrift",
  "version": "2024.01.15.00",
  "port-version": 1,
  "description": "Facebook's Thrift implementation (Tebako/mhx fork)",
  "homepage": "https://github.com/tamatebako/fbthrift",
  "license": "Apache-2.0",
  "supports": "!uwp",
  "dependencies": [
    "folly",
    "fizz",
    "wangle",
    "zstd",
    "libsodium"
  ]
}
```

Create `vcpkg_ports/fbthrift/portfile.cmake`:

```cmake
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/fbthrift
    REF v2024.01.15.00-tebako
    SHA512 0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -Dthriftpy=OFF
        -Dthriftpy3=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/fbthrift)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
```

**NOTE**: Update SHA512 hashes after first vcpkg install attempt.

### Step 5: Create Modular CMake Files (90 min)

Create directory: `cmake/vcpkg/`

#### `cmake/vcpkg/openssl.cmake` (CONTINUE BELOW...)

(See full file contents in SESSION_36_STATIC_BUILD_REQUIREMENTS.md)

---

## Quick Execution Script

```bash
#!/usr/bin/env bash
# Execute Session 36 implementation

set -e

echo "Session 36: Static Build Infrastructure Setup"
echo

# 1. Create vcpkg.json
echo "Creating vcpkg.json..."
cat > vcpkg.json << 'EOF'
{... content from Step 1 ...}
EOF

# 2. Create vcpkg-configuration.json
echo "Creating vcpkg-configuration.json..."
cat > vcpkg-configuration.json << 'EOF'
{... content from Step 2 ...}
EOF

# 3. Create triplets
echo "Creating triplet files..."
mkdir -p vcpkg_triplets
# (Create all 6 triplet files)

# 4. Create overlay ports
echo "Creating overlay ports..."
mkdir -p vcpkg_ports/{jemalloc,folly,fbthrift}
# (Create all overlay port files)

# 5. Create cmake/vcpkg modules
echo "Creating cmake/vcpkg modules..."
mkdir -p cmake/vcpkg
# (Create all 15 cmake files)

# 6. Test build
echo "Testing static build..."
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
export VCPKG_DEFAULT_TRIPLET="$(uname -m)-$(uname -s | tr '[:upper:]' '[:lower:]')-static"
./scripts/build-all-and-test.sh --vcpkg

echo "✓ Session 36 implementation complete!"
```

---

## Success Criteria

✅ All 40 files created
✅ vcpkg installs all dependencies
✅ Static build succeeds
✅ `ldd`/`otool` shows only  system libraries
✅ Binary size reasonable (<25 MB)

---

**SEE ALSO**:
- Full file contents: `doc/SESSION_36_STATIC_BUILD_REQUIREMENTS.md`
- Implementation status: `doc/SESSION_36_IMPLEMENTATION_STATUS.md`
- Continuation prompt: `doc/SESSION_36_CONTINUATION_PROMPT.md`