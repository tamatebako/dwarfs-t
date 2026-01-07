# CMake Build System Refactoring Architecture
## MECE + SOLID Principles Applied to CMake

**Date**: 2025-11-16  
**Current Issue**: 1473-line monolithic CMakeLists.txt with tangled dependencies  
**Goal**: Modular, extensible, maintainable build system following MECE and SOLID  
**Status**: Build currently broken due to complex dependency chains

---

## Current Architecture Problems

### Problem 1: Violation of Single Responsibility Principle (SRP)
**Current**: CMakeLists.txt does EVERYTHING
- Dependency detection
- Library definition
- Tool definition
- Test configuration
- Installation rules
- Packaging
- Custom targets
- Property application loops

**Result**: 1453 lines, hard to maintain, fragile

---

### Problem 2: Violation of Open/Closed Principle (OCP)
**Current**: Adding new library/tool requires editing multiple sections
- Add library in cmake/libdwarfs.cmake
- Add to lists in CMakeLists.txt
- Add to install rules
- Add to packaging

**Result**: Changes ripple through entire file

---

### Problem 3: Violation of Dependency Inversion Principle (DIP)
**Current**: High-level modules (tools) depend on low-level details (include paths, compiler flags)
- Lines 882-1027: Big property loop applies to ALL targets equally
- fmt linked as PRIVATE (line 1020-1022) even though it's needed PUBLIC
- Include paths set multiple times in different places

**Result**: Dependency chaos - tools can't find headers

---

### Problem 4: Not MECE (Mutually Exclusive, Collectively Exhaustive)
**Current**: Concerns overlap and scattered
- Include paths set in: cmake/libdwarfs.cmake, cmake/libdwarfs_tool.cmake, CMakeLists.txt (3 places!)
- Dependencies checked in multiple files
- Format configuration split between cmake/metadata_serialization.cmake and cmake/libdwarfs.cmake

**Result**: Redundancy, inconsistency, conflicts

---

## Root Cause Analysis

### Dependency Chain (As-Built)

```
dwarfs_main.cpp (OBJECT library)
  includes: fmt/format.h, dwarfs/tool/main_adapter.h
  ↓
dwarfs_main target
  links PRIVATE: dwarfs_reader (line 452)
  links PRIVATE: dwarfs_tool (line 1094, in loop)
  links PRIVATE: fmt (line 1021, in loop) ← PROBLEM: Should be from dwarfs_common!
  includes: from loop (lines 900-903)
  ↓
dwarfs_tool (OBJECT library)
  links PUBLIC: dwarfs_common (line 53 in libdwarfs_tool.cmake)
  includes PUBLIC: tools/include (line 54)
  ↓
dwarfs_common
  includes PUBLIC: ${CMAKE_SOURCE_DIR}/include, ${CMAKE_BINARY_DIR}/include (our fix)
  links PUBLIC: fmt (our fix in libdwarfs.cmake)
```

### The Bug

**Symptom**: tools/src/dwarfs_main.cpp cannot find `fmt/format.h`

**Root Cause**: Transitive dependency chain broken by big loop

**Details**:
1. dwarfs_common has fmt as PUBLIC (our fix) ✅
2. dwarfs_tool links to dwarfs_common as PUBLIC ✅
3. dwarfs_main links to dwarfs_tool as PRIVATE (line 1094) ✅
4. **BUT**: Big loop (line 1020-1022) links ALL targets to fmt as PRIVATE ❌
   - This OVERRIDES the PUBLIC linkage from dwarfs_common
   - fmt includes don't propagate
   - Compile fails

**The Fix**: Remove lines 1020-1022 (redundant fmt linkage in loop)

---

## Proposed MECE Architecture

### Principle: Separate Concerns, Clear Boundaries

```
cmake/
├── 00_project.cmake              # Project-level configuration
│   ├── Project definition
│   └── Global options
│
├── 01_dependencies/              # Dependency Management (MECE by lifecycle)
│   ├── required.cmake            # Must be present (fail if missing)
│   ├── optional.cmake            # Nice to have (skip if missing)
│   └── fetched.cmake             # Auto-fetch if missing
│
├── 02_platform/                  # Platform Detection (MECE by concern)
│   ├── detection.cmake           # OS, arch, compiler detection
│   ├── compiler_flags.cmake      # Compiler-specific flags
│   └── toolchain.cmake           # Cross-compilation setup
│
├── 03_serialization/             # Metadata Formats (MECE by format)
│   ├── flatbuffers.cmake         # FlatBuffers (required)
│   ├── thrift.cmake              # Thrift (optional)
│   └── registry.cmake            # Format registry init
│
├── 04_libraries/                 # Library Targets (MECE by library, SRP)
│   ├── common.cmake              # dwarfs_common
│   ├── compressor.cmake          # dwarfs_compressor
│   ├── decompressor.cmake        # dwarfs_decompressor
│   ├── reader.cmake              # dwarfs_reader
│   ├── writer.cmake              # dwarfs_writer
│   ├── extractor.cmake           # dwarfs_extractor
│   ├── rewrite.cmake             # dwarfs_rewrite
│   └── tool.cmake                # dwarfs_tool (common functionality)
│
├── 05_binaries/                  # Binary Targets (MECE by binary, SRP)
│   ├── mkdwarfs.cmake            # mkdwarfs tool
│   ├── dwarfsck.cmake            # dwarfsck tool
│   ├── dwarfsextract.cmake       # dwarfsextract tool
│   ├── dwarfs.cmake              # dwarfs FUSE driver
│   └── universal.cmake           # Universal binary
│
├── 06_tests/                     # Test Configuration (MECE by test type)
│   ├── unit_tests.cmake          # Unit tests
│   ├── integration_tests.cmake   # Integration tests
│   ├── test_helpers.cmake        # Common test utilities
│   └── coverage.cmake            # Coverage configuration
│
├── 07_packaging/                 # Installation & Packaging (MECE by phase)
│   ├── installation.cmake        # Install rules
│   ├── packaging.cmake           # CPack configuration
│   └── artifacts.cmake           # CI/CD artifacts
│
├── tebako/                       # Tebako Integration (existing)
│   └── (current modular structure)
│
└── (legacy files for backward compatibility)
```

---

## SOLID Principles Applied

### Single Responsibility Principle (SRP)
**Each CMake file has ONE purpose**:
- `04_libraries/common.cmake`: Only defines dwarfs_common
- `05_binaries/mkdwarfs.cmake`: Only defines mkdwarfs tool
- `01_dependencies/required.cmake`: Only checks required dependencies

**Benefit**: Easy to find, modify, test specific components

---

### Open/Closed Principle (OCP)
**Libraries open for extension, closed for modification**:
- Adding new library: Create new `04_libraries/newlib.cmake`
- Adding new tool: Create new `05_binaries/newtool.cmake`
- Adding new format: Create new `03_serialization/newformat.cmake`

**Benefit**: Add features without touching existing files

---

### Liskov Substitution Principle (LSP)
**All library modules follow same interface**:
```cmake
# Pattern for all 04_libraries/*.cmake files
add_library(dwarfs_XXX ...)
target_sources(dwarfs_XXX ...)
target_include_directories(dwarfs_XXX PUBLIC ...)
target_link_libraries(dwarfs_XXX PUBLIC/PRIVATE ...)
target_compile_definitions(dwarfs_XXX ...)
```

**Benefit**: Consistent, predictable structure

---

### Interface Segregation Principle (ISP)
**Modules only depend on what they use**:
- dwarfs_compressor doesn't care about FUSE
- mkdwarfs doesn't care about extraction
- Tests don't include production-only features

**Benefit**: Minimal coupling, faster builds

---

### Dependency Inversion Principle (DIP)
**High-level modules depend on abstractions**:
- Tools depend on library interfaces (PUBLIC API)
- Libraries depend on dependency abstractions (targets, not paths)
- Format implementations depend on common domain model

**Benefit**: Loose coupling, easy substitution

---

## Proposed MECE Structure

### Master Orchestrator: CMakeLists.txt (Target: <300 lines)

```cmake
cmake_minimum_required(VERSION 3.28.0)
project(dwarfs VERSION 0.16.0 LANGUAGES CXX C)

# ════════════════════════════════════════════════════
# 00. PROJECT CONFIGURATION
# ════════════════════════════════════════════════════
include(cmake/00_project.cmake)

# ════════════════════════════════════════════════════
# 01. DEPENDENCIES (fail early if missing)
# ════════════════════════════════════════════════════
include(cmake/01_dependencies/required.cmake)
include(cmake/01_dependencies/optional.cmake)
include(cmake/01_dependencies/fetched.cmake)

# ════════════════════════════════════════════════════
# 02. PLATFORM (detect OS, compiler, set flags)
# ════════════════════════════════════════════════════
include(cmake/02_platform/detection.cmake)
include(cmake/02_platform/compiler_flags.cmake)

# ════════════════════════════════════════════════════
# 03. SERIALIZATION (FlatBuffers required, Thrift optional)
# ════════════════════════════════════════════════════
include(cmake/03_serialization/flatbuffers.cmake)
if(DWARFS_WITH_THRIFT)
  include(cmake/03_serialization/thrift.cmake)
endif()
include(cmake/03_serialization/registry.cmake)

# ════════════════════════════════════════════════════
# 04. LIBRARIES (core functionality)
# ════════════════════════════════════════════════════
if(WITH_LIBDWARFS)
  include(cmake/04_libraries/common.cmake)
  include(cmake/04_libraries/compressor.cmake)
  include(cmake/04_libraries/decompressor.cmake)
  include(cmake/04_libraries/reader.cmake)
  include(cmake/04_libraries/writer.cmake)
  include(cmake/04_libraries/extractor.cmake)
  include(cmake/04_libraries/rewrite.cmake)
  include(cmake/04_libraries/tool.cmake)
endif()

# ════════════════════════════════════════════════════
# 05. BINARIES (command-line tools)
# ════════════════════════════════════════════════════
if(WITH_TOOLS)
  include(cmake/05_binaries/mkdwarfs.cmake)
  include(cmake/05_binaries/dwarfsck.cmake)
  include(cmake/05_binaries/dwarfsextract.cmake)
  if(WITH_UNIVERSAL_BINARY)
    include(cmake/05_binaries/universal.cmake)
  endif()
endif()

if(WITH_FUSE_DRIVER)
  include(cmake/05_binaries/dwarfs.cmake)
endif()

# ════════════════════════════════════════════════════
# 06. TESTS (testing infrastructure)
# ════════════════════════════════════════════════════
if(WITH_TESTS)
  enable_testing()
  include(cmake/06_tests/test_helpers.cmake)
  include(cmake/06_tests/unit_tests.cmake)
  include(cmake/06_tests/integration_tests.cmake)
  if(ENABLE_COVERAGE)
    include(cmake/06_tests/coverage.cmake)
  endif()
endif()

# ════════════════════════════════════════════════════
# 07. PACKAGING (installation and distribution)
# ════════════════════════════════════════════════════
include(cmake/07_packaging/installation.cmake)
include(cmake/07_packaging/packaging.cmake)
if(CI_BUILD)
  include(cmake/07_packaging/artifacts.cmake)
endif()

# ════════════════════════════════════════════════════
# 08. CUSTOM TARGETS (format, tidy, strip, etc.)
# ════════════════════════════════════════════════════
include(cmake/08_custom_targets.cmake)
```

---

## Module Specifications

### 00_project.cmake
**Responsibility**: Project-level configuration only
**No Dependencies**: Self-contained
**Exports**: Project options, version variables

```cmake
# Project options (all cmake -D flags)
option(WITH_LIBDWARFS ...)
option(WITH_TOOLS ...)
# ... all options from current CMakeLists.txt lines 39-80

# Version detection (from cmake/version.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/version.cmake)

# Tebako integration (if needed)
if(TEBAKO_BUILD)
  include(${CMAKE_SOURCE_DIR}/cmake/tebako.cmake)
endif()

# Export standard variables
set(DWARFS_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules")
```

---

### 01_dependencies/required.cmake
**Responsibility**: Check REQUIRED dependencies only
**Dependencies**: PkgConfig
**Exports**: Imported targets (PkgConfig::XXX)
**Principle**: Fail fast if requirements not met

```cmake
find_package(PkgConfig REQUIRED)

message(STATUS "═══ Checking Required Dependencies ═══")

find_package(Boost ${BOOST_REQUIRED_VERSION} REQUIRED CONFIG
  COMPONENTS chrono program_options
  OPTIONAL_COMPONENTS process
)

pkg_check_modules(LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto>=${LIBCRYPTO_REQUIRED_VERSION})
pkg_check_modules(LIBARCHIVE REQUIRED IMPORTED_TARGET libarchive>=${LIBARCHIVE_REQUIRED_VERSION})
pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash>=${XXHASH_REQUIRED_VERSION})
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd>=${ZSTD_REQUIRED_VERSION})

message(STATUS "✓ All required dependencies found")
```

---

### 01_dependencies/optional.cmake  
**Responsibility**: Check OPTIONAL dependencies only
**Dependencies**: PkgConfig
**Exports**: Imported targets (if found)
**Principle**: Graceful degradation if missing

```cmake
message(STATUS "═══ Checking Optional Dependencies ═══")

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

if(USE_JEMALLOC)
  include(${CMAKE_SOURCE_DIR}/cmake/need_jemalloc.cmake)
endif()

if(USE_MIMALLOC)
  find_package(mimalloc ${MIMALLOC_REQUIRED_VERSION} REQUIRED CONFIG)
endif()

message(STATUS "✓ Optional dependencies checked")
```

---

### 01_dependencies/fetched.cmake
**Responsibility**: Header-only libs with FetchContent fallback
**Dependencies**: FetchContent
**Exports**: Targets (fmt::fmt, range-v3::range-v3, phmap, gtest)
**Principle**: Always succeed (fetch if not found)

```cmake
include(FetchContent)

message(STATUS "═══ Configuring Auto-Fetched Dependencies ═══")

# fmt (required for all)
include(${CMAKE_SOURCE_DIR}/cmake/need_fmt.cmake)

# range-v3 (required for string utilities)
include(${CMAKE_SOURCE_DIR}/cmake/need_range_v3.cmake)

# parallel-hashmap (required for hash tables)
include(${CMAKE_SOURCE_DIR}/cmake/need_phmap.cmake)

# GoogleTest (required if tests enabled)
if(WITH_TESTS)
  include(${CMAKE_SOURCE_DIR}/cmake/need_gtest.cmake)
endif()

message(STATUS "✓ Auto-fetched dependencies configured")
```

---

### 04_libraries/common.cmake
**Responsibility**: Define dwarfs_common library ONLY
**Dependencies**: Dependencies from 01_*, serialization from 03_*
**Exports**: dwarfs_common target with PUBLIC interface
**Principle**: Single library, clear interface

```cmake
message(STATUS "Configuring dwarfs_common library...")

# ════════════════════════════════════════════════════
# SOURCES
# ════════════════════════════════════════════════════
add_library(dwarfs_common
  src/byte_buffer.cpp
  src/checksum.cpp
  src/compression_registry.cpp
  # ... all current sources from cmake/libdwarfs.cmake lines 28-97
)

# ════════════════════════════════════════════════════
# PUBLIC INTERFACE (propagates to dependents)
# ════════════════════════════════════════════════════
target_include_directories(dwarfs_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(dwarfs_common PUBLIC
  Boost::boost
  Boost::chrono
  dwarfs_fsst
  ${DWARFS_FMT_LIB}          # PUBLIC so fmt propagates to tools!
)

# ════════════════════════════════════════════════════
# PRIVATE DEPENDENCIES (don't propagate)
# ════════════════════════════════════════════════════
target_link_libraries(dwarfs_common PRIVATE
  PkgConfig::LIBCRYPTO
  PkgConfig::XXHASH
  PkgConfig::ZSTD
)

# ════════════════════════════════════════════════════
# CONDITIONAL DEPENDENCIES (based on what was found)
# ════════════════════════════════════════════════════
if(LIBLZ4_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBLZ4)
endif()

if(LIBLZMA_FOUND)
  target_link_libraries(dwarfs_common PRIVATE PkgConfig::LIBLZMA)
endif()

# ... etc for all optional deps

# ════════════════════════════════════════════════════
# FORMAT-SPECIFIC CONFIGURATION
# ════════════════════════════════════════════════════
if(DWARFS_HAVE_FLATBUFFERS)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_FLATBUFFERS)
  target_link_libraries(dwarfs_common PUBLIC dwarfs_metadata_flatbuffers)
  add_dependencies(dwarfs_common dwarfs_metadata_flatbuffers_generate)
endif()

if(DWARFS_HAVE_THRIFT)
  target_compile_definitions(dwarfs_common PUBLIC DWARFS_HAVE_THRIFT)
  target_link_libraries(dwarfs_common PUBLIC
    folly
    dwarfs_compression_thrift
    dwarfs_metadata_thrift
    dwarfs_history_thrift
    dwarfs_features_thrift
  )
  target_link_libraries(dwarfs_common PRIVATE
    dwarfs_folly_lite
    dwarfs_thrift_lite
  )
endif()

# ════════════════════════════════════════════════════
# COMPILER DEFINITIONS
# ════════════════════════════════════════════════════
target_compile_definitions(dwarfs_common PRIVATE
  DWARFS_SYSTEM_ID="${CMAKE_SYSTEM_NAME} [${CMAKE_SYSTEM_PROCESSOR}]"
  DWARFS_COMPILER_ID="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)

# ════════════════════════════════════════════════════
# C++ STANDARD (via common function)
# ════════════════════════════════════════════════════
set_target_properties(dwarfs_common PROPERTIES
  CXX_STANDARD ${DWARFS_CXX_STANDARD}
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  EXPORT_COMPILE_COMMANDS ON
)

message(STATUS "✓ dwarfs_common configured")
```

---

### 05_binaries/mkdwarfs.cmake
**Responsibility**: Define mkdwarfs tool ONLY
**Dependencies**: dwarfs_writer, dwarfs_reader, dwarfs_rewrite
**Exports**: mkdwarfs executable
**Principle**: One tool per file

```cmake
message(STATUS "Configuring mkdwarfs tool...")

# Main implementation (OBJECT library for reuse)
add_library(mkdwarfs_main OBJECT tools/src/mkdwarfs_main.cpp)

# Executable wrapper
add_executable(mkdwarfs tools/src/mkdwarfs.cpp)

# Dependencies (inherit everything from libraries via PUBLIC linkage)
target_link_libraries(mkdwarfs_main PRIVATE
  dwarfs_writer         # Also brings dwarfs_common (PUBLIC)
  dwarfs_reader         # Also brings dwarfs_common (PUBLIC)
  dwarfs_rewrite
)

target_link_libraries(mkdwarfs PRIVATE
  mkdwarfs_main
  dwarfs_tool          # Common tool utilities
)

# Manpage integration (if enabled)
if(WITH_MAN_OPTION)
  include(${CMAKE_SOURCE_DIR}/cmake/manpage_integration.cmake)
  add_tool_manpage(mkdwarfs mkdwarfs_main)
endif()

# Installation
install(TARGETS mkdwarfs RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(NOT WIN32)
  install(FILES doc/completions/bash/mkdwarfs
          DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${BASH_INSTALL_PATH})
  install(FILES doc/completions/zsh/_mkdwarfs
          DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${ZSH_INSTALL_PATH})
endif()

message(STATUS "✓ mkdwarfs configured")
```

---

## Dependency Flow (Corrected)

### Proper PUBLIC Linkage Chain

```
┌─────────────────────────────────────────────┐
│ dwarfs_common (04_libraries/common.cmake)   │
│   PUBLIC: Boost, fmt, fsst, FlatBuffers     │
│   PRIVATE: crypto, xxhash, zstd              │
│   INCLUDES: include/, build/include/         │
└──────────────────┬──────────────────────────┘
                   │ PUBLIC linkage
         ┌─────────┴─────────┐
         ▼                   ▼
┌──────────────────┐  ┌──────────────────┐
│ dwarfs_reader    │  │ dwarfs_writer    │
│  PUBLIC: common  │  │  PUBLIC: common  │
│  Inherits: fmt,  │  │  Inherits: fmt,  │
│  includes        │  │  includes        │
└────────┬─────────┘  └────────┬─────────┘
         │                     │
         │ PUBLIC linkage      │ PUBLIC linkage
         ▼                     ▼
┌──────────────────┐  ┌──────────────────┐
│ mkdwarfs_main    │  │ dwarfs_main      │
│  PRIVATE: writer,│  │  PRIVATE: reader │
│  reader, rewr    │  │  Inherits: fmt,  │
│  Inherits: fmt,  │  │  includes        │
│  includes ✓      │  │  ✓               │
└──────────────────┘  └──────────────────┘
```

**Key**: PUBLIC linkage at library level ensures transitive propagation

---

## Implementation Plan

### Phase 1: Create Modular Structure (2-3 hours)

**Step 1.1: Create Directory Structure**
```bash
mkdir -p cmake/00_project
mkdir -p cmake/01_dependencies
mkdir -p cmake/02_platform
mkdir -p cmake/03_serialization
mkdir -p cmake/04_libraries
mkdir -p cmake/05_binaries
mkdir -p cmake/06_tests
mkdir -p cmake/07_packaging
mkdir -p cmake/08_custom_targets
```

**Step 1.2: Extract Project Configuration**
- Move options (lines 39-97) → `cmake/00_project.cmake`
- Move version setup → keep cmake/version.cmake
- Keep Tebako integration reference

**Step 1.3: Extract Dependencies**
- Required deps (lines 221-249) → `cmake/01_dependencies/required.cmake`
- Optional deps (lines 250-272) → `cmake/01_dependencies/optional.cmake`
- Keep need_*.cmake wrappers, reference from fetched.cmake

**Step 1.4: Extract Libraries**
- dwarfs_common (from cmake/libdwarfs.cmake) → `cmake/04_libraries/common.cmake`
- dwarfs_reader (from cmake/libdwarfs.cmake) → `cmake/04_libraries/reader.cmake`
- dwarfs_writer (from cmake/libdwarfs.cmake) → `cmake/04_libraries/writer.cmake`
- dwarfs_tool (from cmake/libdwarfs_tool.cmake) → `cmake/04_libraries/tool.cmake`
- ... etc for all 7 libraries

**Step  1.5: Extract Tools**
- mkdwarfs (lines 328-347) → `cmake/05_binaries/mkdwarfs.cmake`
- dwarfsck (lines 328-347) → `cmake/05_binaries/dwarfsck.cmake`
- dwarfsextract (lines 328-347) → `cmake/05_binaries/dwarfsextract.cmake`
- dwarfs (lines 410-491) → `cmake/05_binaries/dwarfs.cmake`

**Step 1.6: Extract Tests**
- Test helpers (lines 527-556) → `cmake/06_tests/test_helpers.cmake`
- Unit tests (lines 566-647) → `cmake/06_tests/unit_tests.cmake`
- Tool tests (lines 649-682) → `cmake/06_tests/integration_tests.cmake`

**Step 1.7: Create Thin Master**
- Rewrite CMakeLists.txt as orchestrator only
- Include modular files in logical order
- <300 lines total

---

### Phase 2: Apply Consistent Patterns (1 hour)

**Step 2.1: Create Common Functions**
File: `cmake/functions/library_common.cmake`

```cmake
# Common function for setting up library targets
function(setup_library_target target_name)
  set_target_properties(${target_name} PROPERTIES
    CXX_STANDARD ${DWARFS_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    EXPORT_COMPILE_COMMANDS ON
  )
  
  target_link_libraries(${target_name} PUBLIC Boost::boost)
  
  target_include_directories(${target_name} SYSTEM PUBLIC
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:range-v3::range-v3,INTERFACE_INCLUDE_DIRECTORIES>>
  )
  
  if(ENABLE_ASAN)
    target_compile_options(${target_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${target_name} PRIVATE -fsanitize=address)
  endif()
  
  # ... other common flags
endfunction()
```

**Step 2.2: Apply to All Libraries**
```cmake
# In each 04_libraries/*.cmake
add_library(dwarfs_XXX ...)
# ... target configuration ...
setup_library_target(dwarfs_XXX)
```

---

### Phase 3: Fix Current Build Issue (30 min)

**Immediate Fix** (before refactoring):

**File**: CMakeLists.txt line 1020-1022

**Current**:
```cmake
  if(DWARFS_FMT_LIB)
    target_link_libraries(${tgt} PRIVATE ${DWARFS_FMT_LIB})
  endif()
```

**Problem**: This PRIVATE linkage overrides

 PUBLIC linkage from dwarfs_common

**Fix**: REMOVE these lines entirely (redundant, already in dwarfs_common as PUBLIC)

**Rationale**: 
- dwarfs_common has fmt as PUBLIC
- Libraries link to dwarfs_common
- Tools link to libraries
- Transitive dependency should work

---

## Migration Strategy

### Option A: Refactor First (Recommended)
1. Create modular structure
2. Extract into modules
3. Test each module
4. Update master CMakeLists.txt
5. Verify all configurations work
6. Fix any remaining issues
7. Document architecture

**Benefits**: Clean slate, proper architecture
**Time**: 3-4 hours
**Risk**: Low (can rollback)

---

### Option B: Fix Then Refactor
1. Remove problematic lines 1020-1022 (redundant fmt linkage)
2. Test build
3. Commit working build
4. Then refactor (separate PR)

**Benefits**: Get build working faster
**Time**: 30 min for fix, 3-4 hours for refactor
**Risk**: Low

---

## Recommendation

**Immediate** (next 30 min):
1. Remove CMakeLists.txt lines 1020-1022 (redundant fmt PRIVATE linkage)
2. Test build - should work now
3. Commit: "fix(build): remove redundant fmt linkage that breaks tools"

**Short-term** (next session, 3-4 hours):
4. Refactor into modular MECE structure
5. Apply SOLID principles throughout
6. Test all configurations
7. Commit: "refactor(cmake): modularize build system with MECE architecture"
8. Update memory bank with new architecture

**Then**: Continue with benchmarks (Phase 5 Part 3)

---

## Success Criteria

### Immediate Fix Success
- [ ] Build completes without errors
- [ ] All tools created (mkdwarfs, dwarfsck, dwarfsextract, dwarfs)
- [ ] Can run: `./build-bench-test/mkdwarfs --help`

### Refactoring Success
- [ ] CMakeLists.txt < 300 lines (from 1453)
- [ ] Each module < 150 lines
- [ ] Clear separation of concerns (MECE)
- [ ] Consistent patterns (SOLID)
- [ ] All build configurations work
- [ ] Tests pass
- [ ] Documentation complete

---

**Priority**: Fix build NOW, refactor NEXT