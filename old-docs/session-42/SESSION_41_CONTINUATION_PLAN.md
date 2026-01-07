# Session 41 Continuation Plan - Tools Build & Benchmark

## Current Status: Phase 2 Complete (Infrastructure), Binary Cache Issue

**Date**: 2025-12-27
**Time Spent**: ~90 minutes
**Completion**: 40% (2/5 phases)

---

## ✅ Completed Work

### Phase 1: vcpkg Configuration ✅
- Root vcpkg.json verified with all dependencies
- Root vcpkg-configuration.json configured
- Overlay ports and triplets validated

### Phase 2: Tools Build System ✅ (Infrastructure Complete)
- **Created**: `tools/CMakeLists.txt` (153 lines) - Standalone build
- **Created**: `tools/vcpkg.json` - Manifest mode configuration  
- **Created**: `tools/vcpkg-configuration.json` - Overlay configuration
- **Fixed**: `cmake/libdwarfs.cmake` - Use vcpkg convention (share/dwarfs)
- **Fixed**: `cmake/dwarfs-config.cmake.in` - Add LibArchive::LibArchive alias
- **Fixed**: `vcpkg_ports/dwarfs/portfile.cmake` - Remove config fixup

**Blocking Issue**: vcpkg binary cache contains old dwarfs-config.cmake

---

## ⏭️ Remaining Work (Compressed Schedule)

### Immediate: Clear Binary Cache & Verify Build (15 min)

**Commands**:
```bash
# Clear ALL vcpkg caches
rm -rf ~/.cache/vcpkg ~/Library/Caches/vcpkg
rm -rf /Users/mulgogi/src/external/vcpkg/packages/dwarfs_*
rm -rf /Users/mulgogi/src/external/vcpkg/buildtrees/dwarfs
rm -rf build-vcpkg vcpkg_installed

# Rebuild with --no-binarycaching
cmake -B build-vcpkg -S tools -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/Users/mulgogi/src/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static

cmake --build build-vcpkg

# Verify all 4 tools built
ls -lh build-vcpkg/{mkdwarfs,dwarfsck,dwarfsextract,dwarfs}
```

**Success Criteria**: All 4 tools compile and link successfully

---

### Phase 3: Benchmark Script (45 min)

**Create**: `scripts/benchmark-all.sh` (~300 lines)

**Structure** (from architectural design):
```bash
#!/bin/bash
set -e

# Config
VCPKG_ROOT="${VCPKG_ROOT:-/Users/mulgogi/src/external/vcpkg}"
TRIPLET="arm64-osx-static"
BUILD_DIR="build-vcpkg"
DATASET="benchmarks/datasets/perl-5.43.3"

# Phase 1: Environment check
check_environment() { ... }

# Phase 2: Build tools (if needed)
build_tools() {
  if [ ! -f "$BUILD_DIR/mkdwarfs" ]; then
    cmake -B "$BUILD_DIR" -S tools ...
    cmake --build "$BUILD_DIR"
  fi
}

# Phase 3: Run benchmarks
run_benchmarks() {
  # mkdwarfs compression
  # dwarfsck verification  
  # dwarfsextract extraction
  # dwarfs mount/unmount (if FUSE available)
}

main "$@"
```

**Reference**: `doc/SESSION_41_ARCHITECTURAL_DESIGN.md` lines 240-420

**Test**:
```bash
chmod +x scripts/benchmark-all.sh
./scripts/benchmark-all.sh
```

---

### Phase 4: Integration Testing (30 min)

1. **Clean Build Test**:
   ```bash
   rm -rf build-vcpkg vcpkg_installed
   ./scripts/benchmark-all.sh
   ```

2. **Main Build Compatibility**:
   ```bash
   cmake -B build -GNinja
   cmake --build build
   ```

3. **example/static-site-server Still Works**:
   ```bash
   cd example/static-site-server
   ./build.sh
   ```

**Success Criteria**: All builds work, no regressions

---

### Phase 5: Documentation (30 min)

#### 5.1 Update README.md

Add section after "Building" section:

```markdown
## Building with vcpkg (Tools Only)

For users who want to use DwarFS tools without building libraries:

```bash
# Install tools via vcpkg
scripts/benchmark-all.sh

# Or manually:
cmake -B build-vcpkg -S tools \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=<your-triplet>
cmake --build build-vcpkg
```

Tools will be in `build-vcpkg/`.

See [`doc/vcpkg-build-guide.md`](doc/vcpkg-build-guide.md) for details.
```

#### 5.2 Update .gitignore

```
build-vcpkg/
vcpkg_installed/
```

#### 5.3 Move Temporary Docs to old-docs/

```bash
mkdir -p old-docs/session-41
mv doc/SESSION_41_*.md old-docs/session-41/
```

---

## Timeline (Compressed)

| Phase | Task | Time | Total |
|-------|------|------|-------|
| **2-final** | Clear cache + verify | 15 min | 0:15 |
| **3** | Benchmark script | 45 min | 1:00 |
| **4** | Integration tests | 30 min | 1:30 |
| **5** | Documentation | 30 min | 2:00 |

**Total Remaining**: **2 hours** (compressed from original 5.5 hours)

---

## Success Metrics

- [x] Phase 1: vcpkg config verified
- [x] Phase 2: Tools build infrastructure created
- [ ] Phase 2-final: Tools actually build
- [ ] Phase 3: Benchmark script functional
- [ ] Phase 4: All builds tested
- [ ] Phase 5: Documentation updated

---

## Key Files

**Reference Documents**:
- [`doc/SESSION_41_ARCHITECTURAL_DESIGN.md`](SESSION_41_ARCHITECTURAL_DESIGN.md) - Complete architecture
- [`doc/SESSION_41_IMPLEMENTATION_STATUS.md`](SESSION_41_IMPLEMENTATION_STATUS.md) - Progress tracker
- [`doc/SESSION_41_PHASE2_COMPLETION_STATUS.md`](SESSION_41_PHASE2_COMPLETION_STATUS.md) - Phase 2 details

**Created Files**:
- `tools/CMakeLists.txt` (153 lines)
- `tools/vcpkg.json` (11 lines)
- `tools/vcpkg-configuration.json` (9 lines)

**Modified Files**:
- `cmake/libdwarfs.cmake` (DWARFS_CMAKE_INSTALL_DIR)
- `cmake/dwarfs-config.cmake.in` (LibArchive alias)
- `vcpkg_ports/dwarfs/portfile.cmake` (removed fixup)

---

## Notes

- Architecture is **correct** - two-layer build via find_package()
- All fixes follow **vcpkg best practices**
- No hacks or workarounds
- Binary cache issue is **operational**, not architectural

---

**Last Updated**: 2025-12-27 17:04 HKT