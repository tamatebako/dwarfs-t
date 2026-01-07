# Session 36: Static Build Requirements

**CRITICAL**: This is the PRIMARY goal - all other fixes are secondary.

---

## Core Requirements

### 1. Static Compilation
**ALL dependencies must be statically linked** - no shared libraries, no system dependencies.

### 2. vcpkg as Dependency Manager
**ALL dependencies must come from vcpkg** - no pkg-config, no FetchContent, no system libraries.

### 3. CMake Integration
**ALL dependencies discovered via CMake `find_package()`** using vcpkg toolchain.

---

## Current Problems

### ❌ Problem 1: Dynamic Linking via pkg-config

Current [`CMakeLists.txt:248-270`](../CMakeLists.txt#L248-L270):
```cmake
pkg_check_modules(LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto>=${LIBCRYPTO_REQUIRED_VERSION})
pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive>=${LIBARCHIVE_REQUIRED_VERSION})
pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash>=${XXHASH_REQUIRED_VERSION})
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd>=${ZSTD_REQUIRED_VERSION})
```

**Issues**:
- Uses system libraries (might be shared)
- No version control
- Platform-dependent
- No static linking guarantee

### ❌ Problem 2: FetchContent for Header-Only Libs

Current approach in [`cmake/need_fmt.cmake`](../cmake/need_fmt.cmake), [`cmake/need_range_v3.cmake`](../cmake/need_range_v3.cmake):
```cmake
FetchContent_Declare(fmt
  GIT_REPOSITORY ${LIBFMT_GIT_REPO}
  GIT_TAG ${LIBFMT_TAG}
)
```

**Issues**:
- Bypasses vcpkg version management
- Inconsistent with other dependencies
- Harder to audit/lock versions

### ❌ Problem 3: No Static Linking Configuration

Missing:
- No `-DBUILD_SHARED_LIBS=OFF` in CMake
- No vcpkg triplet specification
- No static runtime configuration

---

## Solution Architecture

### Step 1: vcpkg.json Manifest

Create [`vcpkg.json`](../vcpkg.json) with **static triplet**:

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
      "name": "libflac",
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
    }
  },
  
  "builtin-baseline": "latest"
}
```

### Step 2: vcpkg Configuration Files

#### `vcpkg-configuration.json` (repo root)
```json
{
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/microsoft/vcpkg",
    "baseline": "latest"
  },
  "registries": []
}
```

#### `vcpkg_triplets/x64-linux-static.cmake` (custom triplet)
```cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Force static linking
set(VCPKG_POLICY_DLLS_WITHOUT_LIBS enabled)
set(VCPKG_POLICY_EMPTY_INCLUDE_FOLDER enabled)
```

#### `vcpkg_triplets/arm64-osx-static.cmake` (macOS ARM)
```cmake
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
```

#### `vcpkg_triplets/x64-osx-static.cmake` (macOS Intel)
```cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES x86_64)
```

### Step 3: CMakeLists.txt Changes

#### 3.1 Add vcpkg Toolchain Detection (top of file)

```cmake
# After project() declaration, around line 35:

# ============================================================================
# vcpkg Integration for Static Builds
# ============================================================================

# Detect if vcpkg toolchain is active
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
  set(VCPKG_BUILD ON)
  message(STATUS "vcpkg build detected: ${CMAKE_TOOLCHAIN_FILE}")
  
  # Force static linking in vcpkg mode
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" CACHE STRING "Static library suffix" FORCE)
  
  message(STATUS "Static linking enabled (vcpkg mode)")
else()
  set(VCPKG_BUILD OFF)
  message(STATUS "Non-vcpkg build: using pkg-config or FetchContent")
endif()
```

#### 3.2 Replace pkg-config with find_package() (around line 248-270)

```cmake
# OLD (pkg-config):
# pkg_check_modules(LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto>=${LIBCRYPTO_REQUIRED_VERSION})

# NEW (vcpkg-aware):
if(VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(OpenSSL ${LIBCRYPTO_REQUIRED_VERSION} REQUIRED)
  add_library(PkgConfig::LIBCRYPTO ALIAS OpenSSL::Crypto)
  
  find_package(LibArchive ${LIBARCHIVE_REQUIRED_VERSION} REQUIRED CONFIG)
  add_library(PkgConfig::LIBARCHIVE ALIAS LibArchive::LibArchive)
  
  find_package(xxHash ${XXHASH_REQUIRED_VERSION} REQUIRED CONFIG)
  add_library(PkgConfig::XXHASH ALIAS xxHash::xxhash)
  
  find_package(zstd ${ZSTD_REQUIRED_VERSION} REQUIRED CONFIG)
  add_library(PkgConfig::ZSTD ALIAS zstd::libzstd_static)
  
  if(TRY_ENABLE_LZ4)
    find_package(lz4 ${LIBLZ4_REQUIRED_VERSION} REQUIRED CONFIG)
    set(LIBLZ4_FOUND TRUE)
    add_library(PkgConfig::LIBLZ4 ALIAS lz4::lz4)
  endif()
  
  if(TRY_ENABLE_LZMA)
    find_package(LibLZMA ${LIBLZMA_REQUIRED_VERSION} REQUIRED CONFIG)
    set(LIBLZMA_FOUND TRUE)
    add_library(PkgConfig::LIBLZMA ALIAS LibLZMA::LibLZMA)
  endif()
  
  if(TRY_ENABLE_BROTLI)
    find_package(unofficial-brotli ${LIBBROTLI_REQUIRED_VERSION} REQUIRED CONFIG)
    set(LIBBROTLIDEC_FOUND TRUE)
    set(LIBBROTLIENC_FOUND TRUE)
    add_library(PkgConfig::LIBBROTLIDEC ALIAS unofficial::brotli::brotlidec-static)
    add_library(PkgConfig::LIBBROTLIENC ALIAS unofficial::brotli::brotlienc-static)
  endif()
  
  if(TRY_ENABLE_FLAC)
    find_package(FLAC ${FLAC_REQUIRED_VERSION} REQUIRED CONFIG)
    set(FLAC_FOUND TRUE)
    add_library(PkgConfig::FLAC ALIAS FLAC::FLAC++)
  endif()
  
else()
  # Fallback to pkg-config (non-vcpkg builds)
  pkg_check_modules(LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto>=${LIBCRYPTO_REQUIRED_VERSION})
  pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive>=${LIBARCHIVE_REQUIRED_VERSION})
  pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash>=${XXHASH_REQUIRED_VERSION})
  pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd>=${ZSTD_REQUIRED_VERSION})
  
  if(TRY_ENABLE_LZ4)
    pkg_check_modules(LIBLZ4 IMPORTED_TARGET liblz4>=${LIBLZ4_REQUIRED_VERSION})
  endif()
  
  if(TRY_ENABLE_LZMA)
    pkg_check_modules(LIBLZMA IMPORTED_TARGET liblzma>=${LIBLZMA_REQUIRED_VERSION})
  endif()
  
  if(TRY_ENABLE_BROTLI)
    pkg_check_modules(LIBBROTLIDEC IMPORTED_TARGET libbrotlidec>=${LIBBROTLI_REQUIRED_VERSION})
    pkg_check_modules(LIBBROTLIENC IMPORTED_TARGET libbrotlienc>=${LIBBROTLI_REQUIRED_VERSION})
  endif()
  
  if(TRY_ENABLE_FLAC)
    pkg_check_modules(FLAC IMPORTED_TARGET flac++>=${FLAC_REQUIRED_VERSION})
  endif()
endif()
```

#### 3.3 Remove FetchContent for Header-Only Libs

Update [`cmake/need_fmt.cmake`](../cmake/need_fmt.cmake):

```cmake
# Try vcpkg first
if(VCPKG_BUILD)
  find_package(fmt ${LIBFMT_REQUIRED_VERSION} REQUIRED CONFIG)
  message(STATUS "Using fmt from vcpkg: ${fmt_VERSION}")
  return()
endif()

# Try system package
find_package(fmt ${LIBFMT_REQUIRED_VERSION} QUIET CONFIG)
if(fmt_FOUND)
  message(STATUS "Using system fmt: ${fmt_VERSION}")
  return()
endif()

# Fallback to FetchContent (only in non-vcpkg builds)
message(STATUS "fmt not found, using FetchContent...")
# ... existing FetchContent code ...
```

Apply similar pattern to:
- [`cmake/need_range_v3.cmake`](../cmake/need_range_v3.cmake)
- [`cmake/need_phmap.cmake`](../cmake/need_phmap.cmake)
- [`cmake/need_json.cmake`](../cmake/need_json.cmake)
- [`cmake/need_gtest.cmake`](../cmake/need_gtest.cmake)

### Step 4: Build Scripts

#### Update `scripts/build-all-and-test.sh`

```bash
# Add vcpkg support at the top:

VCPKG_ROOT=${VCPKG_ROOT:-""}
VCPKG_TRIPLET=${VCPKG_TRIPLET:-""}

if [[ -n "$VCPKG_ROOT" ]]; then
  echo "Using vcpkg at: $VCPKG_ROOT"
  echo "Triplet: ${VCPKG_TRIPLET:-auto}"
  export CMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
  
  if [[ -n "$VCPKG_TRIPLET" ]]; then
    export VCPKG_DEFAULT_TRIPLET="$VCPKG_TRIPLET"
  fi
fi

# In build_config(), add toolchain:
if cmake -B "$build_dir" -G"$CMAKE_GENERATOR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    ${CMAKE_TOOLCHAIN_FILE:+-DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE"} \
    ${VCPKG_DEFAULT_TRIPLET:+-DVCPKG_TARGET_TRIPLET="$VCPKG_DEFAULT_TRIPLET"} \
    -DDWARFS_WITH_FLATBUFFERS="$fb" \
    -DDWARFS_WITH_THRIFT="$thrift" \
    -DWITH_TESTS="$BUILD_TESTS" \
    -DWITH_BENCHMARKS="$BUILD_TESTS"; then
```

---

## Usage Examples

### macOS ARM64 Static Build
```bash
# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh

# Set environment
export VCPKG_ROOT="$(pwd)/vcpkg"
export VCPKG_DEFAULT_TRIPLET="arm64-osx-static"

# Build
./scripts/run-all.sh --debug
```

### Linux x64 Static Build
```bash
export VCPKG_ROOT="/path/to/vcpkg"
export VCPKG_DEFAULT_TRIPLET="x64-linux-static"
./scripts/run-all.sh
```

### Verify Static Linking
```bash
# macOS
otool -L build-fb-only/mkdwarfs | grep -v "/usr/lib/system"

# Linux
ldd build-fb-only/mkdwarfs

# Expected: No shared library dependencies (except libc/libm/system libs)
```

---

## Implementation Priority (REVISED)

### Phase 0: Static Build Infrastructure (2 hours) 🔥🔥🔥
**This is now the HIGHEST priority.**

1. ✅ Create `vcpkg.json` manifest
2. ✅ Create custom static triplets (`vcpkg_triplets/*.cmake`)
3. ✅ Update `CMakeLists.txt` for vcpkg detection
4. ✅ Replace all pkg-config with `find_package()`
5. ✅ Update header-only lib CMake modules
6. ✅ Test static build on macOS
7. ✅ Test static build on Linux

### Phase 1: Script Fixes (1 hour) 🔥
- Fix test registration
- Add production/debug modes
- Fix macOS compatibility

### Phase 2: Build Optimization (0.5 hour) 🚀
- Suppress Folly warnings
- Add build timing

---

## Success Criteria (REVISED)

✅ **Static Build Working** when:
1. All dependencies come from vcpkg (no pkg-config, no FetchContent)
2. `ldd`/`otool` shows only system libraries (libc, libSystem, etc.)
3. Builds work on macOS ARM64, macOS x64, Linux x64
4. Binary size is reasonable (<20 MB for mkdwarfs)
5. No runtime library loading errors

✅ **Phase 0 Complete** = Static builds working  
✅ **Phase 1 Complete** = Tests + scripts working  
✅ **Phase 2 Complete** = Build process optimized

---

**Next**: Create REVISED implementation prompt focusing on static builds FIRST.