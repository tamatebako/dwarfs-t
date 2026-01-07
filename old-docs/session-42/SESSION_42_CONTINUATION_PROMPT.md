# Session 42: Tools Build & Benchmark - Continuation Prompt

## Quick Context

**Previous Session**: Session 41 - Created tools build infrastructure
**Status**: Phase 2 complete (infrastructure), binary cache blocking verification
**Time Spent**: ~90 minutes
**Remaining**: ~2 hours (compressed schedule)

---

## What Was Accomplished (Session 41)

### ✅ Infrastructure Created

**Files Created** (3):
- `tools/CMakeLists.txt` (153 lines) - Standalone build using find_package()
- `tools/vcpkg.json` (11 lines) - Declares dwarfs dependency
- `tools/vcpkg-configuration.json` (9 lines) - Overlay configuration

**Files Fixed** (3):
- `cmake/libdwarfs.cmake` - Use `share/dwarfs` (vcpkg convention)
- `cmake/dwarfs-config.cmake.in` - Add LibArchive::LibArchive alias
- `vcpkg_ports/dwarfs/portfile.cmake` - Remove config fixup

**Result**: Two-layer build architecture ready:
1. Layer 1: `vcpkg install dwarfs` (builds libdwarfs) ✅
2. Layer 2: `cmake -S tools` (builds CLI tools) ⏸️ (blocked by cache)

---

## Current Blocker: vcpkg Binary Cache

**Problem**: vcpkg binary cache contains old dwarfs-config.cmake without LibArchive alias

**Solution**: Clear cache and force rebuild

---

## Your Task: Complete Implementation

### IMMEDIATE: Clear Cache & Verify Build (15 min) ⏳ START HERE

**Step 1**: Clear ALL vcpkg caches
```bash
cd /Users/mulgogi/src/external/dwarfs

# Clear vcpkg binary cache
rm -rf ~/.cache/vcpkg ~/Library/Caches/vcpkg
rm -rf /Users/mulgogi/src/external/vcpkg/packages/dwarfs_*
rm -rf /Users/mulgogi/src/external/vcpkg/buildtrees/dwarfs

# Clear local build
rm -rf build-vcpkg vcpkg_installed
```

**Step 2**: Rebuild from scratch
```bash
cmake -B build-vcpkg -S tools -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/Users/mulgogi/src/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static
```

**Step 3**: Build tools
```bash
cmake --build build-vcpkg --parallel
```

**Step 4**: Verify all 4 tools
```bash
ls -lh build-vcpkg/{mkdwarfs,dwarfsck,dwarfsextract,dwarfs}
build-vcpkg/mkdwarfs --help | head -5
```

**Success Criteria**: All 4 tools compile successfully

---

### Phase 3: Create Benchmark Script (45 min)

**Goal**: Single script that builds and benchmarks all tools

**File**: `scripts/benchmark-all.sh` (~300 lines)

**Template** (from `doc/SESSION_41_ARCHITECTURAL_DESIGN.md` lines 240-458):

```bash
#!/bin/bash
# DwarFS Tools Build & Benchmark Script
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VCPKG_ROOT="${VCPKG_ROOT:-/Users/mulgogi/src/external/vcpkg}"
BUILD_DIR="${PROJECT_ROOT}/build-vcpkg"
DATASET="${PROJECT_ROOT}/benchmarks/datasets/perl-5.43.3"

# Platform detection
detect_platform() {
  if [ "$(uname)" = "Darwin" ]; then
    [ "$(uname -m)" = "arm64" ] && echo "arm64-osx-static" || echo "x64-osx-static"
  elif [ "$(uname)" = "Linux" ]; then
    [ "$(uname -m)" = "x86_64" ] && echo "x64-linux-static" || echo "arm64-linux-static"
  else
    echo "x64-windows-static"
  fi
}

TRIPLET="${TRIPLET:-$(detect_platform)}"

# Phase 1: Environment check
check_environment() {
  echo "=== Environment Check ==="
  [ ! -x "${VCPKG_ROOT}/vcpkg" ] && { echo "ERROR: vcpkg not found"; exit 1; }
  echo "✓ vcpkg: ${VCPKG_ROOT}"
  echo "✓ Triplet: ${TRIPLET}"
}

# Phase 2: Build tools
build_tools() {
  echo "=== Building Tools ==="
  cmake -B "${BUILD_DIR}" -S "${PROJECT_ROOT}/tools" -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET="${TRIPLET}"
  cmake --build "${BUILD_DIR}" --parallel
  echo "✓ Built: mkdwarfs, dwarfsck, dwarfsextract, dwarfs"
}

# Phase 3: Run benchmarks
run_benchmarks() {
  echo "=== Running Benchmarks ==="
  [ ! -d "${DATASET}" ] && { echo "Dataset missing, skipping"; return; }
  
  TMP_IMAGE="/tmp/bench.dff"
  TMP_OUT="/tmp/bench-out"
  
  # Benchmark 1: mkdwarfs
  echo "1/4: mkdwarfs compression"
  time "${BUILD_DIR}/mkdwarfs" -i "${DATASET}" -o "${TMP_IMAGE}" -l3 --progress
  ls -lh "${TMP_IMAGE}"
  
  # Benchmark 2: dwarfsck
  echo "2/4: dwarfsck verification"
  time "${BUILD_DIR}/dwarfsck" "${TMP_IMAGE}"
  
  # Benchmark 3: dwarfsextract
  echo "3/4: dwarfsextract extraction"
  time "${BUILD_DIR}/dwarfsextract" -i "${TMP_IMAGE}" -o "${TMP_OUT}"
  
  # Benchmark 4: dwarfs FUSE (if available)
  if [ -f "${BUILD_DIR}/dwarfs" ]; then
    echo "4/4: dwarfs mount/unmount"
    TMP_MNT="/tmp/bench-mount"
    mkdir -p "${TMP_MNT}"
    "${BUILD_DIR}/dwarfs" "${TMP_IMAGE}" "${TMP_MNT}" &
    FUSE_PID=$!
    sleep 2
    ls "${TMP_MNT}" | head -10
    kill ${FUSE_PID} 2>/dev/null || true
    wait ${FUSE_PID} 2>/dev/null || true
    rmdir "${TMP_MNT}"
  fi
  
  # Cleanup
  rm -f "${TMP_IMAGE}"
  rm -rf "${TMP_OUT}"
}

# Main
main() {
  check_environment
  build_tools
  run_benchmarks
  echo "=== Done ==="
}

main "$@"
```

**Create**:
```bash
# Create script
# (Use template above)

# Make executable
chmod +x scripts/benchmark-all.sh

# Test
./scripts/benchmark-all.sh
```

---

### Phase 4: Integration Testing (30 min)

**Test 1**: Clean build workflow
```bash
rm -rf build-vcpkg vcpkg_installed
./scripts/benchmark-all.sh
```

**Test 2**: Main build still works
```bash
cmake -B build -GNinja && cmake --build build
```

**Test 3**: example/static-site-server still works
```bash
cd example/static-site-server && ./build.sh
```

---

### Phase 5: Documentation (30 min)

**5.1**: Update `README.md`
- Add "Building with vcpkg (Tools Only)" section after main "Building" section
- Link to vcpkg-build-guide.md

**5.2**: Create `doc/vcpkg-build-guide.md`
- Full user guide for vcpkg-based workflow
- Prerequisites, usage, troubleshooting

**5.3**: Update `.gitignore`
- Add `build-vcpkg/`
- Add `vcpkg_installed/` (already there, verify)

**5.4**: Move temporary docs
```bash
mkdir -p old-docs/session-41
mv doc/SESSION_41_*.md old-docs/session-41/
# Keep SESSION_42_* in doc/ as active
```

---

## Quick Reference Commands

**Build Tools**:
```bash
cmake -B build-vcpkg -S tools \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/Users/mulgogi/src/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static

cmake --build build-vcpkg
```

**Verify**:
```bash
ls -lh build-vcpkg/{mkdwarfs,dwarfsck,dwarfsextract,dwarfs}
```

**Clean**:
```bash
rm -rf build-vcpkg vcpkg_installed \
  ~/.cache/vcpkg ~/Library/Caches/vcpkg \
  /Users/mulgogi/src/external/vcpkg/packages/dwarfs_* \
  /Users/mulgogi/src/external/vcpkg/buildtrees/dwarfs
```

---

## Key Reference Documents

**Read These First**:
- [`doc/SESSION_41_ARCHITECTURAL_DESIGN.md`](SESSION_41_ARCHITECTURAL_DESIGN.md) - Complete architecture
- [`doc/SESSION_41_CONTINUATION_PLAN.md`](SESSION_41_CONTINUATION_PLAN.md) - This plan
- [`doc/SESSION_41_PHASE2_COMPLETION_STATUS.md`](SESSION_41_PHASE2_COMPLETION_STATUS.md) - Phase 2 details
- [`example/static-site-server/build.sh`](../example/static-site-server/build.sh) - Script pattern

**Created Files**:
- `tools/CMakeLists.txt` - Tool build system
- `tools/vcpkg.json` - Tool manifest
- `tools/vcpkg-configuration.json` - Overlay config

**Modified Files**:
- `cmake/libdwarfs.cmake` - Install path fix
- `cmake/dwarfs-config.cmake.in` - Target alias fix  
- `vcpkg_ports/dwarfs/portfile.cmake` - Removed fixup

---

## Timeline (Compressed)

- ⏰ Cache clear + verify: 15 min
- ⏰ Benchmark script: 45 min
- ⏰ Integration tests: 30 min  
- ⏰ Documentation: 30 min

**Total**: 2 hours

---

## Success Criteria

- [ ] All 4 tools build via vcpkg
- [ ] Benchmark script runs complete workflow
- [ ] Main build unaffected
- [ ] Documentation updated
- [ ] Temporary docs moved to old-docs/

---

**Ready to start. Begin with cache clearing.**
**Date**: 2025-12-27