# Session 36: Static Build Infrastructure - Continuation Prompt

**Start Here**: This is the entry point for implementing Session 36.

**Status**: Ready for implementation (all planning complete)
**Estimated Time**: 3.5 hours
**Priority**: CRITICAL - Blocking all static builds

---

## Quick Start

```bash
# Read these documents first:
cat doc/SESSION_36_STATIC_BUILD_REQUIREMENTS.md  # Full requirements
cat doc/SESSION_36_IMPLEMENTATION_PLAN.md         # Step-by-step plan
cat doc/SESSION_36_IMPLEMENTATION_STATUS.md       # Track progress

# Then start implementation
```

---

## What You Need to Know

### CRITICAL Requirements (DO NOT FORGET)

1. **jemalloc IS MANDATORY** - NOT optional, build FAILS without it
2. **lz4, lzma, brotli, flac ARE ALL MANDATORY** - NOT optional
3. **jemalloc MUST use Tebako fork** - `tamatebako/jemalloc.git` (NO fallback)
4. **folly/fbthrift MUST use Tebako forks** - `tamatebako/*.git` (NO fallback)
5. **All 6 triplets required** - x64/arm64 × linux/darwin/windows

### Architecture

```
Three-Tier Dependency Strategy:

Tier 1 (Standard Libs): vcpkg → pkg-config fallback
Tier 2 (Header-Only):   vcpkg → FetchContent fallback
Tier 3 (Tebako):        vcpkg overlay ONLY (NO fallback)
```

---

## Implementation Order

### Phase 0A: Foundation (30 min)

1. Create `vcpkg.json` (standard deps only, NO jemalloc/folly/fbthrift)
2. Create `vcpkg-configuration.json` (with overlay-ports reference)
3. Create 6 triplet files in `vcpkg_triplets/`

### Phase 0B: Overlay Ports (60 min)

4. Create `vcpkg_ports/jemalloc/` overlay (2 files)
5. Create `vcpkg_ports/folly/` overlay (2 files)
6. Create `vcpkg_ports/fbthrift/` overlay (2 files)

**NOTE**: SHA512 hashes in portfile.cmake will be all zeros initially. After first `vcpkg install`, vcpkg will tell you the correct hash to use.

### Phase 0C: CMake Modules (90 min)

7. Create 15 files in `cmake/vcpkg/`:
   - Standard deps (8): openssl, libarchive, xxhash, zstd, lz4, lzma, brotli, flac
   - Header-only (4): fmt, range_v3, json, phmap
   - Test (1): gtest
   - Tebako (2): jemalloc, folly_thrift

**CRITICAL**: 
- Remove `if(TRY_ENABLE_LZ4)` wrappers - lz4 is MANDATORY
- Remove `if(TRY_ENABLE_LZMA)` wrappers - lzma is MANDATORY  
- Remove `if(TRY_ENABLE_BROTLI)` wrappers - brotli is MANDATORY
- Remove `if(TRY_ENABLE_FLAC)` wrappers - flac is MANDATORY
- Remove `if(USE_JEMALLOC)` wrapper - jemalloc is MANDATORY
- Use `REQUIRED` in all `pkg_check_modules()` calls
- Add `FATAL_ERROR` if not found

### Phase 0D: Integration (20 min)

8. Update `CMakeLists.txt` lines 282-310:
   - Replace pkg_check_modules() block with modular includes
   - Add `include(${CMAKE_SOURCE_DIR}/cmake/vcpkg/*.cmake)` for all 15 modules

9. Update `scripts/build-all-and-test.sh`:
   - Add `--vcpkg` flag support
   - Auto-detect triplet if VCPKG_TRIPLET not set

### Phase 0E: Testing (40 min)

10. Test static build on available platform:
    ```bash
    export VCPKG_ROOT="/path/to/vcpkg"
    export VCPKG_DEFAULT_TRIPLET="arm64-osx-static"
    ./scripts/build-all-and-test.sh --vcpkg
    ```

11. Verify static linking:
    ```bash
    # macOS
    otool -L build-fb-only/mkdwarfs | grep -v "/usr/lib/system"
    
    # Linux
    ldd build-fb-only/mkdwarfs
    ```

---

## File Creation Checklist

Create these 32 new files:

**vcpkg Config** (2):
- [ ] `vcpkg.json`
- [ ] `vcpkg-configuration.json`

**Triplets** (6):
- [ ] `vcpkg_triplets/x64-linux-static.cmake`
- [ ] `vcpkg_triplets/arm64-linux-static.cmake`
- [ ] `vcpkg_triplets/x64-osx-static.cmake`
- [ ] `vcpkg_triplets/arm64-osx-static.cmake`
- [ ] `vcpkg_triplets/x64-windows-static.cmake`
- [ ] `vcpkg_triplets/arm64-windows-static.cmake`

**Overlay Ports** (6):
- [ ] `vcpkg_ports/jemalloc/vcpkg.json`
- [ ] `vcpkg_ports/jemalloc/portfile.cmake`
- [ ] `vcpkg_ports/folly/vcpkg.json`
- [ ] `vcpkg_ports/folly/portfile.cmake`
- [ ] `vcpkg_ports/fbthrift/vcpkg.json`
- [ ] `vcpkg_ports/fbthrift/portfile.cmake`

**CMake Modules** (15):
- [ ] `cmake/vcpkg/openssl.cmake`
- [ ] `cmake/vcpkg/libarchive.cmake`
- [ ] `cmake/vcpkg/xxhash.cmake`
- [ ] `cmake/vcpkg/zstd.cmake`
- [ ] `cmake/vcpkg/lz4.cmake`
- [ ] `cmake/vcpkg/lzma.cmake`
- [ ] `cmake/vcpkg/brotli.cmake`
- [ ] `cmake/vcpkg/flac.cmake`
- [ ] `cmake/vcpkg/fmt.cmake`
- [ ] `cmake/vcpkg/range_v3.cmake`
- [ ] `cmake/vcpkg/json.cmake`
- [ ] `cmake/vcpkg/phmap.cmake`
- [ ] `cmake/vcpkg/gtest.cmake`
- [ ] `cmake/vcpkg/jemalloc.cmake`
- [ ] `cmake/vcpkg/folly_thrift.cmake`

**Test Scripts** (2):
- [ ] `scripts/test-static-build.sh`
- [ ] `scripts/test-all-static-builds.sh`

**Modified Files** (2):
- [ ] `CMakeLists.txt` (lines 282-310)
- [ ] `scripts/build-all-and-test.sh` (add --vcpkg flag)

---

## Common Issues & Solutions

### Issue 1: "jemalloc not found"

**Cause**: Overlay port not configured or vcpkg not used

**Solution**:
```bash
# Verify overlay port exists
ls vcpkg_ports/jemalloc/

# Install manually
vcpkg install jemalloc:arm64-osx-static

# Check CMake configuration
cat cmake/vcpkg/jemalloc.cmake  # Should have FATAL_ERROR if not found
```

### Issue 2: "lz4 is optional but not found"

**Cause**: Old code with `if(TRY_ENABLE_LZ4)` wrapper

**Solution**: Remove the wrapper - lz4 is MANDATORY, not optional.

### Issue 3: SHA512 hash mismatch in portfile.cmake

**Expected**: First install will fail with hash mismatch

**Solution**:
```bash
# vcpkg will show correct hash:
vcpkg install jemalloc:arm64-osx-static
# Error: ... expected hash XXXX

# Copy hash to portfile.cmake
vim vcpkg_ports/jemalloc/portfile.cmake
# Replace 0000... with actual hash
```

---

## Success Criteria

✅ **All 32 files created**
✅ **vcpkg installs jemalloc from tamatebako/jemalloc overlay**
✅ **Build completes with all MANDATORY deps (jemalloc, lz4, lzma, brotli, flac)**
✅ **ldd/otool shows ONLY system libraries**
✅ **Binary size reasonable (<25 MB for mkdwarfs static)**

---

## Time Budget

- Phase 0A (Foundation): 30 min
- Phase 0B (Overlays): 60 min
- Phase 0C (CMake): 90 min
- Phase 0D (Integration): 20 min
- Phase 0E (Testing): 40 min

**Total**: 3.5 hours

---

## Next Session After Completion

After Session 36 completes successfully:

1. Update `doc/SESSION_36_IMPLEMENTATION_STATUS.md` (mark all complete)
2. Test on at least 2 platforms (macOS + Linux or Windows)
3. Document any platform-specific issues
4. Move temporary docs to `old-docs/`
5. Update README.adoc with vcpkg build instructions

---

## References

- **Full Requirements**: `doc/SESSION_36_STATIC_BUILD_REQUIREMENTS.md`
- **Implementation Plan**: `doc/SESSION_36_IMPLEMENTATION_PLAN.md`
- **Status Tracker**: `doc/SESSION_36_IMPLEMENTATION_STATUS.md`
- **Memory Bank Rules**: `.kilocode/rules/memory-bank/`

---

**Created**: 2025-12-24 13:40 HKT
**Status**: Ready for implementation
**Next**: Switch to Code mode and start Phase 0A