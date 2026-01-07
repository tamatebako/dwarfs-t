# CMake Build System Refactoring Plan
## MECE Architecture with Separation of Concerns

**Date**: 2025-11-16  
**Purpose**: Fix build system, modularize CMake, establish clean architecture  
**Principle**: MECE (Mutually Exclusive, Collectively Exhaustive) + Separation of Concerns

---

## Current Problems Identified

### 1. Missing Include Paths (CRITICAL)
**File**: `cmake/libdwarfs.cmake` lines 237-239

**Current**:
```cmake
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)
```

**Problem**: Missing source include directory!

**Must Be**:
```cmake
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>        # Source headers
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>        # Generated headers
  $<INSTALL_INTERFACE:include>
)
```

---

### 2. Missing fmt Linkage (CRITICAL)
**File**: `cmake/libdwarfs.cmake`

**Problem**: `src/history.cpp` uses fmt, but no fmt linkage!

**Missing**:
```cmake
target_link_libraries(dwarfs_common 
  PRIVATE ${DWARFS_FMT_LIB}  # Or PUBLIC if headers use fmt
)
```

**Note**: `DWARFS_FMT_LIB` is set in `cmake/need_fmt.cmake` line 41-42

---

### 3. Obsolete Format References (CLEANUP)
**File**: `cmake/libdwarfs.cmake` lines 249-255, 274-280

**Problem**: Still references CEREAL and BITSERY (removed formats)

**Lines to Remove**:
```cmake
# Lines 249-255: Remove entirely
if(DWARFS_HAVE_CEREAL)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_CEREAL)
endif()

if(DWARFS_HAVE_BITSERY)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_BITSERY)
endif()

# Lines 274-280: Remove entirely  
if(DWARFS_HAVE_CEREAL)
  target_link_libraries(dwarfs_common PRIVATE cereal::cereal)
endif()

if(DWARFS_HAVE_BITSERY)
  target_link_libraries(dwarfs_common PRIVATE bitsery)
endif()
```

---

### 4. Inconsistent Include Directory Handling
**File**: `cmake/libdwarfs.cmake`

**Current Issues**:
- Line 237-239: Only binary dir for dwarfs_common
- Line 302: Private binary dir for dwarfs_reader  
- Line 335: SYSTEM PRIVATE for fsst

**Problem**: Inconsistent, redundant, scattered

---

## Proposed MECE Architecture

### Principle: One Concern Per Module

```
cmake/
├── dependencies/              # NEW: Dependency detection
│   ├── required.cmake         # Required: Boost, OpenSSL, libarchive, xxhash, zstd
│   ├── optional.cmake         # Optional: lz4, lzma, brotli, FLAC
│   ├── allocators.cmake       # jemalloc, mimalloc
│   └── header_only.cmake      # fmt, range-v3, parallel-hashmap, GoogleTest
│
├── serialization/             # Metadata format configuration
│   ├── flatbuffers.cmake      # NEW: Extract from metadata_serialization.cmake
│   ├── thrift.cmake           # NEW: Extract from metadata_serialization.cmake
│   └── registry.cmake         # Format registry, auto-detection
│
├── libraries/                 # NEW: Library definitions
│   ├── common.cmake           # dwarfs_common
│   ├── compressor.cmake       # dwarfs_compressor
│   ├── decompressor.cmake     # dwarfs_decompressor
│   ├── reader.cmake           # dwarfs_reader
│   ├── writer.cmake           # dwarfs_writer
│   ├── extractor.cmake        # dwarfs_extractor
│   └── rewrite.cmake          # dwarfs_rewrite
│
├── tebako/                    # Tebako-specific  
│   └── (existing files)
│
├── need_*.cmake               # Simple dependency wrappers
└── *.cmake                    # Top-level configuration files
```

---

## Refactoring Strategy

### Phase 1: Fix Critical Issues (Immediate - 30 min)

**Goal**: Make build work NOW

**Actions**:

1. **Fix include paths** in `cmake/libdwarfs.cmake`:
   ```cmake
   # Line 237: UPDATE
   target_include_directories(dwarfs_common PUBLIC
     $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
     $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
     $<INSTALL_INTERFACE:include>
   )
   ```

2. **Add fmt linkage** in `cmake/libdwarfs.cmake`:
   ```cmake
   # After line 345 (after Boost linkage)
   target_link_libraries(dwarfs_common PRIVATE ${DWARFS_FMT_LIB})
   ```

3. **Remove CEREAL/BITSERY** (lines 249-255, 274-280, 391-393):
   - Delete compile definitions for CEREAL/BITSERY
   - Delete linkage for cereal::cereal and bitsery
   - Delete bitsery from LIBDWARFS_OBJECT_TARGETS

4. **Test Build**:
   ```bash
   rm -rf build-bench-test
   cmake -B build-bench-test -GNinja -DCMAKE_BUILD_TYPE=Release -DDWARFS_WITH_FLATBUFFERS=ON
   ninja -C build-bench-test
   ```

**Expected**: Build succeeds

---

### Phase 2: Modularize (Long-term - 2-3 hours)

**Goal**: Clean, maintainable, MECE architecture

#### 2.1 Extract Required Dependencies

**New File**: `cmake/dependencies/required.cmake`

**Content**:
```cmake
# Required dependencies for all builds

message(STATUS "Configuring required dependencies...")

# Boost
find_package(Boost ${BOOST_MIN_VERSION} REQUIRED
  COMPONENTS
    program_options
    filesystem
    chrono
    ${BOOST_OPTIONAL_COMPONENTS}
)

# OpenSSL/LibreSSL
pkg_check_modules(LIBCRYPTO REQUIRED libcrypto>=3.0.0)

# libarchive
pkg_check_modules(LIBARCHIVE REQUIRED libarchive>=3.6.0)

# xxHash  
pkg_check_modules(XXHASH REQUIRED libxxhash>=0.8.1)

# zstd
pkg_check_modules(ZSTD REQUIRED libzstd>=1.4.8)

message(STATUS "Required dependencies configured")
```

**Include in CMakeLists.txt** (line ~200):
```cmake
include(cmake/dependencies/required.cmake)
```

---

#### 2.2 Extract Optional Dependencies

**New File**: `cmake/dependencies/optional.cmake`

**Content**:
```cmake
# Optional compression libraries

message(STATUS "Checking optional dependencies...")

# lz4
if(TRY_ENABLE_LZ4)
  pkg_check_modules(LIBLZ4 liblz4>=1.9.3)
endif()

# lzma
if(TRY_ENABLE_LZMA)
  pkg_check_modules(LIBLZMA liblzma>=5.2.5)
endif()

# brotli
if(TRY_ENABLE_BROTLI)
  pkg_check_modules(LIBBROTLIDEC libbrotlidec>=1.0.9)
  pkg_check_modules(LIBBROTLIENC libbrotlienc>=1.0.9)
endif()

# FLAC
if(TRY_ENABLE_FLAC)
  pkg_check_modules(FLAC flac++>=1.4.2)
endif()

message(STATUS "Optional dependencies checked")
```

---

#### 2.3 Extract Header-Only Dependencies

**New File**: `cmake/dependencies/header_only.cmake`

**Content**:
```cmake
# Header-only libraries with FetchContent fallback

message(STATUS "Configuring header-only dependencies...")

# fmt (move from need_fmt.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/need_fmt.cmake)

# range-v3
find_package(range-v3 QUIET)
if(NOT range-v3_FOUND)
  FetchContent_Declare(range-v3 ...)
  FetchContent_MakeAvailable(range-v3)
endif()

# parallel-hashmap
find_package(phmap QUIET)
if(NOT phmap_FOUND)
  FetchContent_Declare(phmap ...)
  FetchContent_MakeAvailable(phmap)
endif()

# GoogleTest (if tests enabled)
if(WITH_TESTS)
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    FetchContent_Declare(googletest ...)
    FetchContent_MakeAvailable(googletest)
  endif()
endif()

message(STATUS "Header-only dependencies configured")
```

---

#### 2.4 Split Serialization Configuration

**New File**: `cmake/serialization/flatbuffers.cmake`

**Content**: Extract FlatBuffers handling from `cmake/metadata_serialization.cmake`

**New File**: `cmake/serialization/thrift.cmake`

**Content**: Extract Thrift handling from `cmake/metadata_serialization.cmake`

**New File**: `cmake/serialization/registry.cmake`

**Content**: Format registry init, auto-detection setup

---

#### 2.5 Split Library Definitions

**Example**: `cmake/libraries/common.cmake`

```cmake
# dwarfs_common library definition

add_library(dwarfs_common
  src/byte_buffer.cpp
  src/checksum.cpp
  # ... all sources
)

# Include directories
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Required dependencies
target_link_libraries(dwarfs_common
  PUBLIC
    Boost::boost
    Boost::chrono
    dwarfs_fsst
  PRIVATE
    PkgConfig::LIBCRYPTO
    PkgConfig::XXHASH
    PkgConfig::ZSTD
    ${DWARFS_FMT_LIB}
)

# Optional dependencies
if(LIBLZ4_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBLZ4)
endif()

# Format-specific linkage
if(DWARFS_HAVE_FLATBUFFERS)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_FLATBUFFERS)
  target_link_libraries(dwarfs_common PUBLIC dwarfs_metadata_flatbuffers)
  add_dependencies(dwarfs_common dwarfs_metadata_flatbuffers_generate)
endif()

if(DWARFS_HAVE_THRIFT)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_THRIFT)
  target_link_libraries(dwarfs_common 
    PUBLIC folly dwarfs_compression_thrift dwarfs_metadata_thrift dwarfs_history_thrift dwarfs_features_thrift
    PRIVATE dwarfs_folly_lite dwarfs_thrift_lite
  )
endif()
```

---

## Immediate Fix (Phase 1) - Detailed Steps

### Step 1: Fix Include Paths

**File**: `cmake/libdwarfs.cmake`  
**Line**: 237-239

<apply_diff>
<args>
<file>
<path>cmake/libdwarfs.cmake</path>
<diff>
<content>
<<<<<<< SEARCH
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)
=======
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
>>>>>>> REPLACE