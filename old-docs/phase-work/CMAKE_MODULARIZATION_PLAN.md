# CMake Build System Modularization Plan
## MECE Architecture with Separation of Concerns

**Date**: 2025-11-16  
**Current Issue**: Build fails with missing fmt headers  
**Root Cause**: Include paths incomplete, fmt not linked  
**Strategy**: First fix critical issues, then modularize for maintainability

---

## Critical Issues Identified

### Issue 1: Missing Source Include Directory ⚠️ CRITICAL
**File**: [`cmake/libdwarfs.cmake:237-239`](cmake/libdwarfs.cmake:237)

**Current**:
```cmake
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)
```

**Problem**: `${CMAKE_SOURCE_DIR}/include` is missing!

**Impact**: ALL headers in `include/dwarfs/` are invisible to compiler

**Fix**:
```cmake
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>       # ADD THIS
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
```

---

### Issue 2: fmt Not Linked ⚠️ CRITICAL
**File**: [`cmake/libdwarfs.cmake:340-346`](cmake/libdwarfs.cmake:340)

**Current**:
```cmake
target_link_libraries(
  dwarfs_common
  PUBLIC
  Boost::boost
  Boost::chrono
  dwarfs_fsst
)
```

**Problem**: `${DWARFS_FMT_LIB}` is missing!

**Impact**: fmt headers found but linker fails

**Fix**:
```cmake
target_link_libraries(
  dwarfs_common
  PUBLIC
  Boost::boost
  Boost::chrono
  dwarfs_fsst
  ${DWARFS_FMT_LIB}                                    # ADD THIS
)
```

**Note**: `DWARFS_FMT_LIB` defined in [`cmake/need_fmt.cmake:41-42`](cmake/need_fmt.cmake:41)

---

### Issue 3: Obsolete Format References (CLEANUP)
**File**: [`cmake/libdwarfs.cmake`](cmake/libdwarfs.cmake)

**Lines to Remove**:
- Lines 249-255: CEREAL/BITSERY compile definitions
- Lines 274-280: cereal::cereal / bitsery linkage
- Lines 391-393: bitsery in LIBDWARFS_OBJECT_TARGETS

**Impact**: Clean up removed formats, reduce confusion

---

## Immediate Fix (Phase 1)

### Goal
Make build work NOW with minimal changes

### Changes Required

1. **Add source include directory** (1 line addition)
2. **Add fmt linkage** (1 line addition)
3. **Remove CEREAL/BITSERY references** (15 lines deletion)

### Implementation

**File**: `cmake/libdwarfs.cmake`

**Change 1** (line 237):
```cmake
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>        # NEW
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>                          # NEW
)
```

**Change 2** (after line 345):
```cmake
target_link_libraries(
  dwarfs_common
  PUBLIC
  Boost::boost
  Boost::chrono
  dwarfs_fsst
  ${DWARFS_FMT_LIB}                                     # NEW
)
```

**Change 3** (remove lines 249-255):
```cmake
# DELETE:
if(DWARFS_HAVE_CEREAL)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_CEREAL)
endif()

if(DWARFS_HAVE_BITSERY)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_BITSERY)
endif()
```

**Change 4** (remove lines 274-280):
```cmake
# DELETE:
if(DWARFS_HAVE_CEREAL)
  target_link_libraries(dwarfs_common PRIVATE cereal::cereal)
endif()

if(DWARFS_HAVE_BITSERY)
  target_link_libraries(dwarfs_common PRIVATE bitsery)
endif()
```

**Change 5** (remove lines 391-393):
```cmake
# DELETE:
if(TARGET bitsery)
  list(APPEND LIBDWARFS_OBJECT_TARGETS bitsery)
endif()
```

### Verification

```bash
# Clean build
rm -rf build-test-fix
cmake -B build-test-fix -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DWITH_TESTS=OFF \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=ON

# Should configure without warnings about CEREAL/BITSERY

ninja -C build-test-fix

# Should build successfully
```

### Commit

```bash
git add cmake/libdwarfs.cmake src/writer/internal/similarity_ordering.cpp
git commit -m "fix(build): resolve include paths and fmt linkage

Critical fixes for build system:
- Add CMAKE_SOURCE_DIR/include to dwarfs_common include paths
- Add fmt library linkage to dwarfs_common
- Remove obsolete CEREAL/BITSERY references from CMake

Build Issues Fixed:
- 'fmt/format.h' not found → fixed (include path + linkage)
- 'dwarfs/tool/*.h' not found → fixed (include path)
- AppleClang 17 lambda capture → fixed (similarity_ordering.cpp:687)

This resolves build failures and enables local testing/benchmarking."
```

---

## Long-Term Modularization (Phase 2)

### Architecture: MECE Principles

**Current Problem**: 1473-line CMakeLists.txt + scattered logic

**Proposed Structure**: Modular, concern-separated

```
cmake/
├── dependencies/                # Dependency Detection (MECE by type)
│   ├── required.cmake           # Boost, OpenSSL, libarchive, xxhash, zstd
│   ├── optional_compression.cmake  # lz4, lzma, brotli, FLAC
│   ├── optional_allocators.cmake   # jemalloc, mimalloc
│   └── header_only.cmake        # fmt, range-v3, phmap, GTest
│
├── serialization/               # Metadata Formats (MECE by format)
│   ├── flatbuffers.cmake        # FlatBuffers detection/config
│   ├── thrift.cmake             # Thrift detection/config
│   └── registry.cmake          # Format registry initialization
│
├── libraries/                   # Library Targets (MECE by library)
│   ├── common.cmake             # dwarfs_common
│   ├── compressor.cmake         # dwarfs_compressor
│   ├── decompressor.cmake       # dwarfs_decompressor
│   ├── reader.cmake             # dwarfs_reader
│   ├── writer.cmake             # dwarfs_writer
│   ├── extractor.cmake          # dwarfs_extractor
│   └── rewrite.cmake            # dwarfs_rewrite
│
├── tools/                       # Tool Targets (if needed)
│   └── definitions.cmake        # mkdwarfs, dwarfs, dwarfsck, dwarfsextract
│
├── tebako/                      # Tebako Integration (existing)
│   └── (current modular files)
│
├── libdwarfs.cmake              # Master library assembly (thin)
├── platform.cmake               # Platform detection
├── compiler.cmake               # Compiler flags
├── need_*.cmake                 # Simple dependency wrappers
└── ...                          # Other config files
```

### Benefits

1. **Clarity**: Each file has ONE clear purpose
2. **Maintainability**: Changes isolated to specific modules
3. **Testability**: Can test individual components
4. **MECE**: No overlaps, all concerns covered
5. **Reusability**: Modules can be included independently

---

## Proposed Execution Order

### Immediate (Today - 30 minutes)

1. **Apply Phase 1 fixes** to `cmake/libdwarfs.cmake`
   - 5 changes total (described above)
   - ~20 lines modified

2. **Test build**
   - Verify compilation succeeds
   - Verify all tools built

3. **Commit fixes**
   - Semantic commit message
   - Push to branch

4. **Run benchmarks** (FlatBuffers only initially)
   - Download Perl dataset
   - Test with FlatBuffers format
   - Generate initial report

---

### Short-Term (Next 1-2 days)

5. **Modularize dependencies** (cmake/dependencies/)
   - Extract required dependencies
   - Extract optional dependencies
   - Extract header-only dependencies
   - Test each module independently

6. **Modularize serialization** (cmake/serialization/)
   - Split metadata_serialization.cmake
   - One file per format
   - Registry in separate file

7. **Modularize libraries** (cmake/libraries/)
   - One file per library target
   - Clear dependency declarations
   - Consistent patterns

8. **Update CMakeLists.txt**
   - Thin master file
   - Include modular cmake files
   - Logical ordering

9. **Test modular build**
   - Verify all configurations work
   - Test Tebako builds
   - Run full test suite

10. **Document architecture**
    - Update memory bank
    - Create CMAKE_ARCHITECTURE.md
    - Document patterns

---

## Expected File Structure After Phase 2

### cmake/dependencies/required.cmake (NEW)
```cmake
# Required dependencies that must be present for ANY build
message(STATUS "Checking required dependencies...")

find_package(Boost REQUIRED COMPONENTS program_options filesystem chrono)
pkg_check_modules(LIBCRYPTO REQUIRED libcrypto>=3.0.0)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive>=3.6.0)
pkg_check_modules(XXHASH REQUIRED libxxhash>=0.8.1)
pkg_check_modules(ZSTD REQUIRED libzstd>=1.4.8)

message(STATUS "Required dependencies found")
```

### cmake/dependencies/header_only.cmake (NEW)
```cmake
# Header-only libraries with FetchContent fallback
message(STATUS "Configuring header-only dependencies...")

# fmt (required)
include(${CMAKE_SOURCE_DIR}/cmake/need_fmt.cmake)

# range-v3 (required)
find_package(range-v3 ${RANGE_V3_REQUIRED} CONFIG QUIET)
if(NOT range-v3_FOUND)
  FetchContent_Declare(range-v3 GIT_REPOSITORY ${RANGE_V3_GIT_REPO} GIT_TAG ${RANGE_V3_VERSION})
  FetchContent_MakeAvailable(range-v3)
endif()

# parallel-hashmap (required)
find_package(phmap ${PHMAP_REQUIRED} CONFIG QUIET)
if(NOT phmap_FOUND)
  FetchContent_Declare(phmap GIT_REPOSITORY ${PHMAP_GIT_REPO} GIT_TAG ${PHMAP_VERSION})
  FetchContent_MakeAvailable(phmap)
endif()

message(STATUS "Header-only dependencies configured")
```

### cmake/serialization/flatbuffers.cmake (NEW)
```cmake
# FlatBuffers metadata format configuration (REQUIRED)
message(STATUS "Configuring FlatBuffers serialization...")

find_package(flatbuffers ${FLATBUFFERS_REQUIRED} CONFIG QUIET)

if(NOT flatbuffers_FOUND)
  message(STATUS "FlatBuffers not found, fetching via FetchContent...")
  FetchContent_Declare(
    flatbuffers
    GIT_REPOSITORY https://github.com/google/flatbuffers.git
    GIT_TAG v${FLATBUFFERS_VERSION}
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(flatbuffers)
endif()

# Compile schema
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/include/dwarfs/gen-flatbuffers/metadata_generated.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include/dwarfs/gen-flatbuffers
  COMMAND flatbuffers::flatc --cpp --scoped-enums --cpp-std c++17
          -o ${CMAKE_BINARY_DIR}/include/dwarfs/gen-flatbuffers
          ${CMAKE_SOURCE_DIR}/flatbuffers/metadata.fbs
  DEPENDS flatbuffers/metadata.fbs flatbuffers::flatc
  COMMENT "Generating FlatBuffers metadata headers"
)

add_custom_target(dwarfs_metadata_flatbuffers_generate
  DEPENDS ${CMAKE_BINARY_DIR}/include/dwarfs/gen-flatbuffers/metadata_generated.h
)

add_library(dwarfs_metadata_flatbuffers INTERFACE)
target_include_directories(dwarfs_metadata_flatbuffers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
)
add_dependencies(dwarfs_metadata_flatbuffers dwarfs_metadata_flatbuffers_generate)

# Add sources to serialization registry
list(APPEND DWARFS_SERIALIZATION_SOURCES
  src/metadata/serialization/flatbuffers_serializer.cpp
  src/reader/internal/metadata_v2_flatbuffers.cpp
  src/reader/internal/metadata_types_flatbuffers.cpp
  src/reader/internal/flatbuffer_metadata_views.cpp
)

set(DWARFS_HAVE_FLATBUFFERS ON PARENT_SCOPE)
message(STATUS "FlatBuffers configured: ENABLED (required)")
```

### cmake/libraries/common.cmake (NEW)
```cmake
# dwarfs_common library - core utilities and abstractions

add_library(dwarfs_common
  # Core
  src/byte_buffer.cpp
  src/checksum.cpp
  src/compression_registry.cpp
  src/error.cpp
  src/logger.cpp
  # ... (all current sources)
)

# Include directories - PUBLIC so they propagate
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Required public dependencies
target_link_libraries(dwarfs_common
  PUBLIC
    Boost::boost
    Boost::chrono
    dwarfs_fsst
    ${DWARFS_FMT_LIB}
)

# Required private dependencies
target_link_libraries(dwarfs_common
  PRIVATE
    PkgConfig::LIBCRYPTO
    PkgConfig::XXHASH
    PkgConfig::ZSTD
)

# Format-specific configurations
if(DWARFS_HAVE_FLATBUFFERS)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_FLATBUFFERS)
  target_link_libraries(dwarfs_common PUBLIC dwarfs_metadata_flatbuffers)
  add_dependencies(dwarfs_common dwarfs_metadata_flatbuffers_generate)
endif()

if(DWARFS_HAVE_THRIFT)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_THRIFT)
  target_link_libraries(dwarfs_common 
    PUBLIC 
      folly
      dwarfs_compression_thrift
      dwarfs_metadata_thrift
      dwarfs_history_thrift
      dwarfs_features_thrift
    PRIVATE
      dwarfs_folly_lite
      dwarfs_thrift_lite
  )
endif()

# Optional compression libraries
if(LIBLZ4_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBLZ4)
endif()

if(LIBLZMA_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBLZMA)
endif()

if(LIBBROTLIDEC_FOUND AND LIBBROTLIENC_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBBROTLIDEC PkgConfig::LIBBROTLIENC)
endif()

if(FLAC_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::FLAC)
endif()

# Platform-specific
if(WIN32)
  target_link_libraries(dwarfs_common PRIVATE bcrypt.lib)
endif()

if(ENABLE_STACKTRACE)
  target_link_libraries(dwarfs_common PUBLIC cpptrace::cpptrace)
endif()

# Compiler definitions
target_compile_definitions(dwarfs_common PRIVATE
  DWARFS_SYSTEM_ID="${CMAKE_SYSTEM_NAME} [${CMAKE_SYSTEM_PROCESSOR}]"
  DWARFS_COMPILER_ID="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)
```

---

## Master Assembly Pattern

### New cmake/libdwarfs.cmake (Thin Orchestrator)

```cmake
# DwarFS Library Assembly
# Orchestrates modular library definitions

message(STATUS "Assembling DwarFS libraries...")

# Include individual library definitions
include(cmake/libraries/common.cmake)
include(cmake/libraries/compressor.cmake)
include(cmake/libraries/decompressor.cmake)
include(cmake/libraries/reader.cmake)
include(cmake/libraries/writer.cmake)
include(cmake/libraries/extractor.cmake)
include(cmake/libraries/rewrite.cmake)

# Inter-library dependencies
target_link_libraries(dwarfs_reader PUBLIC dwarfs_common dwarfs_decompressor)
target_link_libraries(dwarfs_writer PUBLIC dwarfs_common dwarfs_compressor dwarfs_decompressor)
target_link_libraries(dwarfs_extractor PUBLIC dwarfs_reader)
target_link_libraries(dwarfs_rewrite PUBLIC dwarfs_reader dwarfs_writer)

# Export targets list
set(LIBDWARFS_TARGETS
  dwarfs_common
  dwarfs_compressor
  dwarfs_decompressor
  dwarfs_reader
  dwarfs_writer
  dwarfs_extractor
  dwarfs_rewrite
  PARENT_SCOPE
)

message(STATUS "DwarFS libraries assembled")
```

---

## CMakeLists.txt Reorganization

### Current Order (Problematic)
```cmake
# Mixed: options, dependencies, libraries, tools all interleaved
```

### Proposed Order (MECE)
```cmake
cmake_minimum_required(VERSION 3.28.0)
project(dwarfs VERSION 0.16.0 LANGUAGES CXX C)

# ============================================================
# 1. PROJECT CONFIGURATION
# ============================================================
include(cmake/platform.cmake)          # Platform detection
include(cmake/compiler.cmake)          # Compiler flags
include(cmake/version.cmake)           # Version detection
include(cmake/options.cmake)           # Build options

# ============================================================
# 2. REQUIRED DEPENDENCIES (fail if missing)
# ============================================================
include(cmake/dependencies/required.cmake)

# ============================================================
# 3. OPTIONAL DEPENDENCIES (skip if missing)
# ============================================================
include(cmake/dependencies/optional_compression.cmake)
include(cmake/dependencies/optional_allocators.cmake)

# ============================================================
# 4. HEADER-ONLY DEPENDENCIES (fetch if missing)
# ============================================================
include(cmake/dependencies/header_only.cmake)

# ============================================================
# 5. SERIALIZATION FORMATS (FlatBuffers required, Thrift optional)
# ============================================================
include(cmake/serialization/flatbuffers.cmake)  # Always
if(DWARFS_WITH_THRIFT)
  include(cmake/serialization/thrift.cmake)      # Conditional
endif()
include(cmake/serialization/registry.cmake)     # Initialize

# ============================================================
# 6. LIBRARIES (all dependencies resolved above)
# ============================================================
if(WITH_LIBDWARFS)
  include(cmake/libdwarfs.cmake)                # Thin orchestrator
endif()

# ============================================================
# 7. TOOLS (depend on libraries)
# ============================================================
if(WITH_TOOLS)
  add_subdirectory(tools)
endif()

# ============================================================
# 8. TESTS (depend on libraries and tools)
# ============================================================
if(WITH_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# ============================================================
# 9. PACKAGING & INSTALLATION
# ============================================================
include(cmake/packaging.cmake)
```

---

## Implementation Timeline

### Phase 1 (Immediate - 30 min)
- [x] Diagnose issues
- [ ] Apply 5 fixes to cmake/libdwarfs.cmake
- [ ] Fix AppleClang 17 lambda (already done)
- [ ] Test build
- [ ] Commit

### Phase 2 (Short-term - 2-3 hours)
- [ ] Create cmake/dependencies/ modules
- [ ] Create cmake/serialization/ modules  
- [ ] Create cmake/libraries/ modules
- [ ] Update CMakeLists.txt (thin master)
- [ ] Test all build configurations
- [ ] Update documentation
- [ ] Commit modularization

### Phase 3 (After builds work)
- [ ] Run benchmarks
- [ ] Generate reports
- [ ] Final documentation

---

## Success Criteria

### Phase 1 Success
- [ ] Build completes without errors
- [ ] All tools created (mkdwarfs, dwarfsextract, dwarfs)
- [ ] Tests compile (if enabled)
- [ ] Both FlatBuffers architecture working

### Phase 2 Success
- [ ] CMakeLists.txt < 500 lines (from 1473)
- [ ] Each cmake module < 150 lines
- [ ] Clear separation of concerns
- [ ] All build configurations work
- [ ] Documentation updated

---

## Risk Mitigation

### Backup Strategy
- Git commit after Phase 1 (working build)
- Git commit after each Phase 2 module
- Can rollback to any working state

### Testing Strategy
- Test after each change
- Multiple build configurations:
  - FlatBuffers only
  - FlatBuffers + Thrift
  - Tebako MKD scope
  - Tebako ALL scope
  - With/without tests

---

## Next Steps

1. **Review this plan**
2. **Approve Phase 1 immediate fixes**
3. **Apply changes to cmake/libdwarfs.cmake**
4. **Test build**
5. **Commit and continue**

---

**Priority**: HIGHEST - Blocks all development, testing, CI/CD, benchmarking