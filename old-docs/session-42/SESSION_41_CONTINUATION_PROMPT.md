# Session 41: Unified vcpkg Build & Benchmark - Continuation Prompt

## Context

**Status**: ✅ Architectural design complete, ready for implementation
**Completed**: Phase 1 (vcpkg config verified), Architecture document created
**Next**: Phase 2 - Create tools/CMakeLists.txt and build system

## What You Need to Know

### Architecture Decision ✅

**Approach**: Two-layer build using `find_package(dwarfs)` pattern
1. **Layer 1**: `vcpkg install dwarfs` (builds libdwarfs from source via overlay port)
2. **Layer 2**: `tools/CMakeLists.txt` (builds CLI tools against installed libraries)

**Reference**: See [`doc/SESSION_41_ARCHITECTURAL_DESIGN.md`](SESSION_41_ARCHITECTURAL_DESIGN.md) for complete design

### Current State ✅

```
dwarfs/
├── vcpkg.json                      ✅ EXISTS (all deps listed)
├── vcpkg-configuration.json        ✅ EXISTS (overlay config)
├── vcpkg_ports/dwarfs/             ✅ EXISTS (builds libdwarfs)
├── example/static-site-server/     ✅ REFERENCE PATTERN
├── tools/
│   ├── src/                        ✅ EXISTS (tool sources)
│   └── CMakeLists.txt              ❌ NEEDS CREATION (~150 lines)
└── scripts/
    └── benchmark-all.sh            ❌ NEEDS CREATION (~350 lines)
```

## Your Task: Complete Implementation

### Phase 2: Create tools/CMakeLists.txt (1.5 hours) ⏳ START HERE

**Goal**: Standalone build for all CLI tools using vcpkg-installed libdwarfs

**File to Create**: `tools/CMakeLists.txt` (~150 lines)

**Pattern**: Follow `example/static-site-server/CMakeLists.txt` exactly

**Template** (from architectural design):

```cmake
cmake_minimum_required(VERSION 3.24)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(dwarfs-tools VERSION 0.16.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find DwarFS libraries (installed via vcpkg)
find_package(dwarfs CONFIG REQUIRED)

# Optional: FUSE for dwarfs driver
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(FUSE3 fuse3)
  if(NOT FUSE3_FOUND)
    pkg_check_modules(FUSE fuse)
  endif()
endif()

# Tool executables
add_executable(mkdwarfs src/mkdwarfs_main.cpp src/mkdwarfs.cpp)
target_link_libraries(mkdwarfs PRIVATE dwarfs::dwarfs_common dwarfs::dwarfs_writer)

add_executable(dwarfsck src/dwarfsck_main.cpp src/dwarfsck.cpp)
target_link_libraries(dwarfsck PRIVATE dwarfs::dwarfs_common dwarfs::dwarfs_reader)

add_executable(dwarfsextract src/dwarfsextract_main.cpp src/dwarfsextract.cpp)
target_link_libraries(dwarfsextract PRIVATE
  dwarfs::dwarfs_common dwarfs::dwarfs_reader dwarfs::dwarfs_extractor)

if(FUSE3_FOUND OR FUSE_FOUND)
  add_executable(dwarfs src/dwarfs_main.cpp src/dwarfs.cpp)
  target_link_libraries(dwarfs PRIVATE dwarfs::dwarfs_common dwarfs::dwarfs_reader)
  if(FUSE3_FOUND)
    target_link_libraries(dwarfs PRIVATE PkgConfig::FUSE3)
  else()
    target_link_libraries(dwarfs PRIVATE PkgConfig::FUSE)
  endif()
  message(STATUS "FUSE driver: ENABLED")
else()
  message(STATUS "FUSE driver: DISABLED (FUSE not found)")
endif()

# Compiler flags & install targets (see full template in architecture doc)
```

**Test Build**:
```bash
# 1. Install libdwarfs via vcpkg
vcpkg install dwarfs --overlay-ports=vcpkg_ports --triplet=arm64-osx-static

# 2. Build tools
cmake -B build-vcpkg -S tools \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static

cmake --build build-vcpkg --parallel

# 3. Verify
ls -lh build-vcpkg/mkdwarfs build-vcpkg/dwarfsck build-vcpkg/dwarfsextract
```

---

### Phase 3: Create scripts/benchmark-all.sh (2 hours)

**Goal**: Single script that builds and benchmarks all tools

**File to Create**: `scripts/benchmark-all.sh` (~350 lines)

**Pattern**: Enhanced version of `example/static-site-server/build.sh`

**Structure**:
```bash
#!/bin/bash
set -e

# Configuration
VCPKG_ROOT="${VCPKG_ROOT:-/Users/mulgogi/src/external/vcpkg}"
TRIPLET="${TRIPLET:-$(detect_platform)}"
BUILD_DIR="build-vcpkg"

# Phase 1: Environment check
check_environment() { ... }

# Phase 2: Install libdwarfs
install_libdwarfs() {
  vcpkg install dwarfs --overlay-ports=vcpkg_ports --triplet="$TRIPLET"
}

# Phase 3: Build tools
build_tools() {
  cmake -B "$BUILD_DIR" -S tools -GNinja \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET="$TRIPLET"
  cmake --build "$BUILD_DIR" --parallel
}

# Phase 4: Run benchmarks
run_benchmarks() {
  # mkdwarfs, dwarfsck, dwarfsextract, dwarfs tests
}

main "$@"
```

**See**: Full template in `doc/SESSION_41_ARCHITECTURAL_DESIGN.md` lines 240-420

---

### Phase 4: Integration Testing (1 hour)

```bash
# Clean build test
rm -rf build-vcpkg vcpkg_installed
./scripts/benchmark-all.sh

# Verify main build still works
cmake -B build -GNinja
cmake --build build
```

---

### Phase 5: Documentation (1 hour)

**Update**:
1. `README.md` - Add "Building with vcpkg" section
2. Create `doc/vcpkg-build-guide.md` - Full user guide
3. Update `.gitignore` - Add `build-vcpkg/` and `vcpkg_installed/`

---

## Quick Start Commands

```bash
# 1. Read architectural design
cat doc/SESSION_41_ARCHITECTURAL_DESIGN.md

# 2. Read implementation status
cat doc/SESSION_41_IMPLEMENTATION_STATUS.md

# 3. Create tools/CMakeLists.txt
# (Use template from architecture doc)

# 4. Test build
vcpkg install dwarfs --overlay-ports=vcpkg_ports --triplet=arm64-osx-static
cmake -B build-vcpkg -S tools \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static
cmake --build build-vcpkg

# 5. Create scripts/benchmark-all.sh
# (Use template from architecture doc)

# 6. Test full workflow
./scripts/benchmark-all.sh
```

---

## Key Files

**Read These First**:
- [`doc/SESSION_41_ARCHITECTURAL_DESIGN.md`](SESSION_41_ARCHITECTURAL_DESIGN.md) - Complete architecture
- [`doc/SESSION_41_IMPLEMENTATION_STATUS.md`](SESSION_41_IMPLEMENTATION_STATUS.md) - Progress tracker
- [`example/static-site-server/CMakeLists.txt`](../example/static-site-server/CMakeLists.txt) - Reference pattern
- [`example/static-site-server/build.sh`](../example/static-site-server/build.sh) - Script pattern

**Create These**:
- `tools/CMakeLists.txt` - Tool build system
- `scripts/benchmark-all.sh` - Build & benchmark orchestrator

---

## Success Criteria

- [ ] `tools/CMakeLists.txt` created and builds all 4 tools
- [ ] `scripts/benchmark-all.sh` runs complete workflow
- [ ] All benchmarks execute successfully
- [ ] Main build system unaffected
- [ ] Documentation updated

---

## Timeline

- Phase 2: 1.5 hours (tools build)
- Phase 3: 2 hours (benchmark script)
- Phase 4: 1 hour (testing)
- Phase 5: 1 hour (documentation)

**Total**: ~5.5 hours remaining

---

**Ready to implement. Start with Phase 2: Create tools/CMakeLists.txt**