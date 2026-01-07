# Session 41: Unified vcpkg Build & Benchmark - Architectural Design

## Date: 2025-12-27
## Decision: Option A - find_package(dwarfs) Pattern

---

## Architecture Overview

### Two-Layer Build Strategy

```
┌─────────────────────────────────────────────────────────┐
│                scripts/benchmark-all.sh                  │
│                  (Orchestration Layer)                   │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┴────────────┐
        │                         │
        ▼                         ▼
┌──────────────┐         ┌──────────────────┐
│   Layer 1    │         │     Layer 2      │
│              │         │                  │
│   vcpkg      │         │  tools/          │
│   install    │────────▶│  CMakeLists.txt  │
│              │         │                  │
│ (libdwarfs)  │         │ (CLI tools)      │
└──────────────┘         └──────────────────┘
       │                         │
       │                         │
       ▼                         ▼
vcpkg_installed/          build-vcpkg/
└── dwarfs/              ├── mkdwarfs
    ├── lib/             ├── dwarfsck
    ├── include/         ├── dwarfsextract
    └── share/           └── dwarfs
```

### Build Flow

**Step 1: Install libdwarfs via vcpkg**
```bash
vcpkg install dwarfs --overlay-ports=vcpkg_ports \
  --triplet=arm64-osx-static
```
- Triggers `vcpkg_ports/dwarfs/portfile.cmake`
- Builds libdwarfs from local source (../)
- Installs to `vcpkg_installed/arm64-osx-static/`
- Creates CMake package config

**Step 2: Build tools against installed libdwarfs**
```bash
cmake -B build-vcpkg -S tools \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static

cmake --build build-vcpkg --parallel
```
- Uses `tools/CMakeLists.txt` (NEW file)
- Calls `find_package(dwarfs CONFIG REQUIRED)`
- Links tools against installed libraries
- Creates standalone executables

---

## File Structure

### New Files to Create

```
dwarfs/
├── tools/
│   └── CMakeLists.txt              # NEW - Unified build for all tools
│
├── scripts/
│   └── benchmark-all.sh            # NEW - Build + benchmark orchestrator
│
└── doc/
    ├── vcpkg-build-guide.md        # NEW - User documentation
    └── SESSION_41_*.md             # Implementation tracking
```

### Existing Files (Reference Only)

```
dwarfs/
├── vcpkg.json                      # EXISTS - Root manifest
├── vcpkg-configuration.json        # EXISTS - Overlay config
├── vcpkg_ports/
│   └── dwarfs/
│       ├── portfile.cmake          # EXISTS - Builds libdwarfs
│       └── vcpkg.json              # EXISTS - Port manifest
└── example/static-site-server/     # EXISTS - Reference pattern
    ├── CMakeLists.txt              # Pattern to follow
    ├── vcpkg.json
    └── build.sh                    # Pattern to follow
```

---

## tools/CMakeLists.txt Design

### Pattern: Follows example/static-site-server/CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.24)

# Use new Boost CMake config (CMP0167)
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(dwarfs-tools VERSION 0.16.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Find Dependencies
# ============================================================================

# DwarFS libraries (installed via vcpkg)
find_package(dwarfs CONFIG REQUIRED)

# Optional: FUSE for dwarfs driver
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(FUSE3 fuse3)
  if(NOT FUSE3_FOUND)
    pkg_check_modules(FUSE fuse)
  endif()
endif()

# ============================================================================
# Tool Executables
# ============================================================================

# mkdwarfs - Filesystem creator
add_executable(mkdwarfs
  src/mkdwarfs_main.cpp
  src/mkdwarfs.cpp
)
target_link_libraries(mkdwarfs PRIVATE
  dwarfs::dwarfs_common
  dwarfs::dwarfs_writer
)

# dwarfsck - Filesystem checker
add_executable(dwarfsck
  src/dwarfsck_main.cpp
  src/dwarfsck.cpp
)
target_link_libraries(dwarfsck PRIVATE
  dwarfs::dwarfs_common
  dwarfs::dwarfs_reader
)

# dwarfsextract - Filesystem extractor
add_executable(dwarfsextract
  src/dwarfsextract_main.cpp
  src/dwarfsextract.cpp
)
target_link_libraries(dwarfsextract PRIVATE
  dwarfs::dwarfs_common
  dwarfs::dwarfs_reader
  dwarfs::dwarfs_extractor
)

# dwarfs - FUSE driver (conditional)
if(FUSE3_FOUND OR FUSE_FOUND)
  add_executable(dwarfs
    src/dwarfs_main.cpp
    src/dwarfs.cpp
  )
  target_link_libraries(dwarfs PRIVATE
    dwarfs::dwarfs_common
    dwarfs::dwarfs_reader
  )
  
  if(FUSE3_FOUND)
    target_link_libraries(dwarfs PRIVATE PkgConfig::FUSE3)
  else()
    target_link_libraries(dwarfs PRIVATE PkgConfig::FUSE)
  endif()
  
  message(STATUS "FUSE driver: ENABLED")
else()
  message(STATUS "FUSE driver: DISABLED (FUSE not found)")
endif()

# ============================================================================
# Compiler Flags
# ============================================================================

foreach(target mkdwarfs dwarfsck dwarfsextract)
  target_compile_options(${target} PRIVATE
    -Wall -Wextra
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-g -O0>
  )
endforeach()

if(TARGET dwarfs)
  target_compile_options(dwarfs PRIVATE
    -Wall -Wextra
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-g -O0>
  )
endif()

# ============================================================================
# Install Targets
# ============================================================================

install(TARGETS mkdwarfs dwarfsck dwarfsextract
  RUNTIME DESTINATION bin
)

if(TARGET dwarfs)
  install(TARGETS dwarfs
    RUNTIME DESTINATION bin
  )
endif()

# ============================================================================
# Configuration Summary
# ============================================================================

message(STATUS "DwarFS Tools Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tools: mkdwarfs, dwarfsck, dwarfsextract${FUSE3_FOUND OR FUSE_FOUND ? ", dwarfs" : ""}")
```

**Key Design Decisions:**

1. **Clean Separation**: Uses `find_package(dwarfs)` - no direct source deps
2. **Minimal**: ~150 lines vs 374 lines in main CMakeLists.txt
3. **Self-Documenting**: Clear structure, comments
4. **FUSE Conditional**: Gracefully handles missing FUSE
5. **Pattern Match**: Mirrors example/static-site-server/CMakeLists.txt

---

## scripts/benchmark-all.sh Design

### Pattern: Enhanced version of example/static-site-server/build.sh

```bash
#!/bin/bash
# DwarFS Tools Build & Benchmark Script
#
# Builds all CLI tools via vcpkg and runs performance benchmarks
#
# Prerequisites:
#   - vcpkg installed and VCPKG_ROOT set
#   - Benchmark dataset (auto-downloaded if missing)
#
# Usage:
#   ./scripts/benchmark-all.sh            # Build + benchmark
#   ./scripts/benchmark-all.sh --clean    # Clean only
#   ./scripts/benchmark-all.sh --rebuild  # Clean + build
#   ./scripts/benchmark-all.sh --help     # Show usage

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CLEAN_ONLY=false
REBUILD=false
SKIP_BENCHMARKS=false

# ============================================================================
# Configuration
# ============================================================================

VCPKG_ROOT="${VCPKG_ROOT:-/Users/mulgogi/src/external/vcpkg}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${PROJECT_ROOT}/build-vcpkg"
DATASET_PATH="${PROJECT_ROOT}/benchmarks/datasets/perl-5.43.3"

# Platform detection
detect_platform() {
  if [ "$(uname)" = "Darwin" ]; then
    if [ "$(uname -m)" = "arm64" ]; then
      echo "arm64-osx-static"
    else
      echo "x64-osx-static"
    fi
  elif [ "$(uname)" = "Linux" ]; then
    if [ "$(uname -m)" = "x86_64" ]; then
      echo "x64-linux-static"
    else
      echo "arm64-linux-static"
    fi
  else
    echo "x64-windows-static"
  fi
}

TRIPLET="${TRIPLET:-$(detect_platform)}"

# ============================================================================
# Phase 1: Environment Check
# ============================================================================

check_environment() {
  echo "=== Environment Check ==="
  
  if [ ! -x "${VCPKG_ROOT}/vcpkg" ]; then
    echo "ERROR: vcpkg not found at: ${VCPKG_ROOT}"
    echo "Please install vcpkg or set VCPKG_ROOT environment variable"
    exit 1
  fi
  
  echo "✓ vcpkg: ${VCPKG_ROOT}"
  echo "✓ Triplet: ${TRIPLET}"
  echo "✓ Build type: ${BUILD_TYPE}"
  echo ""
}

# ============================================================================
# Phase 2: Install libdwarfs via vcpkg
# ============================================================================

install_libdwarfs() {
  echo "=== Installing libdwarfs via vcpkg ==="
  echo "This builds libdwarfs from local source using overlay port..."
  echo ""
  
  cd "${PROJECT_ROOT}"
  "${VCPKG_ROOT}/vcpkg" install dwarfs \
    --overlay-ports=vcpkg_ports \
    --triplet="${TRIPLET}" \
    --recurse
    
  echo ""
  echo "✓ libdwarfs installed to vcpkg_installed/${TRIPLET}/"
  echo ""
}

# ============================================================================
# Phase 3: Build Tools
# ============================================================================

build_tools() {
  echo "=== Building Tools ==="
  echo ""
  
  cmake -B "${BUILD_DIR}" -S "${PROJECT_ROOT}/tools" \
    -GNinja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET="${TRIPLET}"
  
  cmake --build "${BUILD_DIR}" --parallel
  
  echo ""
  echo "=== Build Complete ==="
  ls -lh "${BUILD_DIR}/mkdwarfs"
  ls -lh "${BUILD_DIR}/dwarfsck"
  ls -lh "${BUILD_DIR}/dwarfsextract"
  [ -f "${BUILD_DIR}/dwarfs" ] && ls -lh "${BUILD_DIR}/dwarfs"
  echo ""
}

# ============================================================================
# Phase 4: Run Benchmarks
# ============================================================================

run_benchmarks() {
  echo "=== Running Benchmarks ==="
  
  # Check dataset
  if [ ! -d "${DATASET_PATH}" ]; then
    echo "WARNING: Benchmark dataset not found: ${DATASET_PATH}"
    echo "Skipping benchmarks (run benchmarks/download_datasets.py first)"
    return
  fi
  
  TMP_IMAGE="/tmp/benchmark-vcpkg.dff"
  TMP_OUTPUT="/tmp/benchmark-vcpkg-output"
  
  # Benchmark 1: mkdwarfs compression
  echo ""
  echo "Benchmark 1/4: mkdwarfs (compression)"
  time "${BUILD_DIR}/mkdwarfs" \
    -i "${DATASET_PATH}" \
    -o "${TMP_IMAGE}" \
    --compression-level 3 \
    --progress
  
  echo ""
  ls -lh "${TMP_IMAGE}"
  
  # Benchmark 2: dwarfsck verification
  echo ""
  echo "Benchmark 2/4: dwarfsck (verification)"
  time "${BUILD_DIR}/dwarfsck" "${TMP_IMAGE}"
  
  # Benchmark 3: dwarfsextract extraction
  echo ""
  echo "Benchmark 3/4: dwarfsextract (extraction)"
  rm -rf "${TMP_OUTPUT}"
  time "${BUILD_DIR}/dwarfsextract" \
    -i "${TMP_IMAGE}" \
    -o "${TMP_OUTPUT}"
  
  # Benchmark 4: dwarfs FUSE (if available)
  if [ -f "${BUILD_DIR}/dwarfs" ]; then
    echo ""
    echo "Benchmark 4/4: dwarfs (mount/unmount)"
    TMP_MOUNT="/tmp/benchmark-vcpkg-mount"
    mkdir -p "${TMP_MOUNT}"
    
    "${BUILD_DIR}/dwarfs" "${TMP_IMAGE}" "${TMP_MOUNT}" &
    FUSE_PID=$!
    sleep 2
    
    echo "Mounted filesystem:"
    ls -lh "${TMP_MOUNT}" | head -20
    
    kill ${FUSE_PID} 2>/dev/null || true
    wait ${FUSE_PID} 2>/dev/null || true
    rmdir "${TMP_MOUNT}"
  fi
  
  # Cleanup
  rm -f "${TMP_IMAGE}"
  rm -rf "${TMP_OUTPUT}"
  
  echo ""
  echo "=== Benchmarks Complete ==="
}

# ============================================================================
# Main Execution
# ============================================================================

main() {
  check_environment
  
  if [ "$CLEAN_ONLY" = false ]; then
    install_libdwarfs
    build_tools
    
    if [ "$SKIP_BENCHMARKS" = false ]; then
      run_benchmarks
    fi
  fi
  
  echo ""
  echo "=== Done ==="
}

main "$@"
```

**Key Features:**

1. **Two-Phase Build**: Install libdwarfs → Build tools
2. **Platform Detection**: Auto-selects triplet
3. **Comprehensive Benchmarks**: All 4 tools
4. **Error Handling**: Graceful degradation
5. **User-Friendly**: Clean output, helpful messages

---

## Dependency Flow

```
vcpkg.json (root)
    │
    ├─▶ jemalloc (overlay port → tamatebako/jemalloc)
    ├─▶ boost-* (vcpkg registry)
    ├─▶ openssl (vcpkg registry)
    ├─▶ libarchive (vcpkg registry)
    ├─▶ zstd (vcpkg registry)
    ├─▶ xxhash (vcpkg registry)
    ├─▶ lz4 (vcpkg registry)
    ├─▶ liblzma (vcpkg registry)
    ├─▶ brotli (vcpkg registry)
    ├─▶ libflac (vcpkg registry)
    ├─▶ fmt (vcpkg registry)
    └─▶ ... (other deps)

vcpkg install dwarfs
    │
    └─▶ vcpkg_ports/dwarfs/portfile.cmake
            │
            ├─▶ Builds from local source (../../)
            ├─▶ Uses main CMakeLists.txt
            ├─▶ Installs libdwarfs_* libraries
            └─▶ Creates CMake package config

find_package(dwarfs CONFIG)
    │
    └─▶ Finds dwarfs-config.cmake in vcpkg_installed/
            │
            └─▶ Exports targets:
                ├─▶ dwarfs::dwarfs_common
                ├─▶ dwarfs::dwarfs_reader
                ├─▶ dwarfs::dwarfs_writer
                ├─▶ dwarfs::dwarfs_extractor
                └─▶ dwarfs::dwarfs_rewrite (if Thrift)
```

---

## Comparison with Main Build

| Aspect | Main CMake Build | vcpkg Tools Build |
|--------|------------------|-------------------|
| **Purpose** | Library development | Tool usage |
| **Entry Point** | `CMakeLists.txt` (root) | `tools/CMakeLists.txt` |
| **Dependencies** | pkg-config, FetchContent | vcpkg only |
| **Scope** | Libraries + tools + tests | Tools only |
| **Complexity** | 374 lines, many options | ~150 lines, simple |
| **Use Case** | Developers | Users, benchmarking |

---

## Success Criteria

### Phase 1: vcpkg Configuration
- [x] Root vcpkg.json exists with all deps
- [x] Root vcpkg-configuration.json configured
- [ ] `vcpkg install` from root succeeds

### Phase 2: Tools Build
- [ ] `tools/CMakeLists.txt` created (~150 lines)
- [ ] All 4 tools build successfully
- [ ] `find_package(dwarfs)` works
- [ ] FUSE conditional works

### Phase 3: Benchmark Script
- [ ] `scripts/benchmark-all.sh` created (~350 lines)
- [ ] Platform detection works
- [ ] Two-phase build works
- [ ] All benchmarks run

### Phase 4: Integration
- [ ] Clean build works: `rm -rf build-vcpkg && ./scripts/benchmark-all.sh`
- [ ] Main build unaffected
- [ ] Documented in README.md

---

## Timeline

- **Phase 1**: 30 min (config already exists, just test)
- **Phase 2**: 1.5 hrs (create tools/CMakeLists.txt, test build)
- **Phase 3**: 2 hrs (create benchmark script, test)
- **Phase 4**: 1 hr (integration testing)
- **Phase 5**: 1 hr (documentation)

**Total**: ~6 hours

---

## Next Steps

1. Test vcpkg install: `vcpkg install dwarfs`
2. Create `tools/CMakeLists.txt`
3. Test tool build
4. Create `scripts/benchmark-all.sh`
5. Run full workflow
6. Document

---

**Status**: Design complete, ready for implementation
**Date**: 2025-12-27