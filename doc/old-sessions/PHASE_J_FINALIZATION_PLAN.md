# Phase J: Finalization & Documentation - PLAN

**Date**: 2025-11-30  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Status**: PLANNED  
**Dependencies**: Phases H & I complete ✅

---

## Overview

Phase J finalizes the DwarFS v0.16.0 release by:
1. Testing vcpkg ports locally
2. Updating official documentation (README.md)
3. Cleaning up temporary documentation
4. Final verification and release preparation

**Estimated Time**: 2-3 hours

---

## Tasks

### J1: Test vcpkg Ports Locally (1 hour)

**Prerequisites**:
```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh
```

**Steps**:

1. **Test libdwarfs installation** (20 min)
   ```bash
   cd /Users/mulgogi/src/external/dwarfs
   ~/vcpkg/vcpkg install libdwarfs --overlay-ports=./ports
   ~/vcpkg/vcpkg list | grep libdwarfs
   ```

2. **Test dwarfs tools installation** (20 min)
   ```bash
   ~/vcpkg/vcpkg install dwarfs --overlay-ports=./ports
   ls ~/vcpkg/installed/*/tools/dwarfs/
   ~/vcpkg/installed/*/tools/dwarfs/mkdwarfs --version
   ```

3. **Test consumer project** (20 min)
   ```bash
   mkdir -p /tmp/dwarfs_test && cd /tmp/dwarfs_test
   # Create CMakeLists.txt and main.cpp (see VCPKG_QUICK_START.md)
   cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build
   ./build/test_app
   ```

**Deliverable**: Verified working vcpkg installations

---

### J2: Update README.md (30 minutes)

**Current**: [`README.md`](../README.md) lacks vcpkg instructions

**Add Sections**:

1. **Installation via vcpkg** (after build instructions)
   ```markdown
   ## Installation via vcpkg
   
   ### Libraries (libdwarfs)
   ```bash
   vcpkg install libdwarfs
   vcpkg install libdwarfs[flac,lz4,lzma,brotli]
   ```
   
   ### Tools (mkdwarfs, dwarfsck, dwarfsextract, dwarfs)
   ```bash
   vcpkg install dwarfs
   vcpkg install dwarfs[fuse]  # Linux only
   ```
   ```

2. **CMake Integration** (in Usage section)
   ```cmake
   find_package(dwarfs CONFIG REQUIRED)
   target_link_libraries(myapp PRIVATE dwarfs::dwarfs_reader)
   ```

3. **Link to documentation**
   - Point to [`doc/VCPKG_QUICK_START.md`](VCPKG_QUICK_START.md)
   - Reference vcpkg features

**Files to Update**:
- [`README.md`](../README.md) - Main documentation
- Consider: [`README.DWARFS.md`](../README.DWARFS.md) if it needs vcpkg info

---

### J3: Clean Up Documentation (30 minutes)

**Move to old-docs/**:

Phase documentation (completed work):
- `doc/PHASE_H_I_FIX_TESTS_VCPKG_PLAN.md` → `doc/old-docs/phase-h-i/`
- `doc/PHASE_H_I_CONTINUATION_PROMPT.md` → `doc/old-docs/phase-h-i/`

Phase G documentation (already archived):
- Verify `doc/old-docs/phase-g/` is complete

**Keep in doc/**:
- `doc/PHASE_H_I_COMPLETION_SUMMARY.md` (final reference)
- `doc/PHASE_H_I_IMPLEMENTATION_STATUS.md` (final status)
- `doc/VCPKG_QUICK_START.md` (user guide)
- `doc/PHASE_G_IMPLEMENTATION_STATUS.md` (benchmark completion)

**Archive Structure**:
```
doc/old-docs/
├── phase-g/          (Benchmark suite - completed)
├── phase-h-i/        (Tests + vcpkg - completed)
├── dual-format-completion/  (Metadata formats - completed)
├── phase-b-size-optimization/  (Size optimization - completed)
├── phase-c-completed/  (Documentation - completed)
├── phase-d-sessions/   (Testing validation - completed)
├── phase-e/           (Benchmarks - completed)
├── phase-f/           (Extensions - completed)
└── thrift-only-refactoring/  (Thrift isolation - completed)
```

---

### J4: Calculate SHA512 for Portfiles (15 minutes)

**When**: After v0.16.0 tag created

**Steps**:
```bash
cd /tmp
git clone https://github.com/tamatebako/dwarfs.git
cd dwarfs
git checkout v0.16.0
git archive --format=tar.gz --prefix=dwarfs-0.16.0/ v0.16.0 > ../dwarfs-0.16.0.tar.gz
shasum -a 512 ../dwarfs-0.16.0.tar.gz
```

**Update**:
- `ports/libdwarfs/portfile.cmake:5` - Replace SHA512
- `ports/dwarfs/portfile.cmake:5` - Replace SHA512

---

### J5: Final Verification (30 minutes)

**Build Tests**:
```bash
# Clean build
rm -rf build-fb
cmake -B build-fb -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=ON \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=ON \
  -DWITH_FUSE_DRIVER=OFF \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
ninja -C build-fb
build-fb/dwarfs_unit_tests
```

**Tool Tests**:
```bash
# Create test image
mkdir /tmp/test_source
echo "Hello World" > /tmp/test_source/test.txt
build-fb/mkdwarfs -i /tmp/test_source -o /tmp/test.dwarfs

# Check image
build-fb/dwarfsck /tmp/test.dwarfs

# Extract
build-fb/dwarfsextract -i /tmp/test.dwarfs -o /tmp/test_extract
cat /tmp/test_extract/test.txt
```

**Expected**: All tests pass, tools work correctly

---

### J6: Commit Changes (15 minutes)

**Semantic Commit Message**:
```
fix(tests,vcpkg): fix 5 failing tests, add vcpkg distribution ports

Phase H: Fix Failing Tests
- Fix critical signed integer bug in packed_int_vector (sign extension)
- Improve time/size formatting (remove unnecessary decimals)
- Update test expectations for cleaner output
- Add graceful FLAC test skipping when unavailable
- Result: 1,613/1,613 tests passing (100%)

Phase I: vcpkg Integration
- Create libdwarfs vcpkg port with optional features (flac,lz4,lzma,brotli)
- Create dwarfs vcpkg port with optional FUSE support
- Add comprehensive documentation and usage guides
- Verify existing CMake export mechanism

Files changed:
- 4 test files (filesystem_writer, time_resolution_converter, metadata, utils)
- 1 header (folly_compat.h - critical bug fix + formatting)
- 7 vcpkg port files (libdwarfs, dwarfs manifests and build scripts)
- 3 documentation files (completion summary, quick start, port README)

Closes #XXX (if issue exists)
```

**Commands**:
```bash
git status
git add -A
git commit -m "fix(tests,vcpkg): ..."
git push origin refactor/dwarfs-mkdwarfs-complete
```

---

## Success Criteria

Phase J will be considered complete when:
- ✅ vcpkg ports successfully install locally
- ✅ Consumer project builds and links
- ✅ README.md includes vcpkg instructions
- ✅ Temporary documentation archived
- ✅ SHA512 calculated and updated (when tag available)
- ✅ Changes committed to branch
- ✅ Ready for PR/merge to main

---

## Deliverables

1. **Working vcpkg ports** - Locally tested, ready for submission
2. **Updated README.md** - vcpkg installation instructions
3. **Clean documentation** - Temporary files archived
4. **Commit ready** - All changes staged with semantic message
5. **Release prep** - v0.16.0 ready for tagging

---

## Timeline

| Task | Duration | Start After |
|------|----------|-------------|
| J1: Test vcpkg | 1h | Immediate |
| J2: Update README | 30min | J1 complete |
| J3: Clean docs | 30min | J2 complete |
| J4: SHA512 | 15min | v0.16.0 tagged |
| J5: Verification | 30min | J3 complete |
| J6: Commit | 15min | J5 complete |

**Total**: 2.5-3 hours (compressed timeline)

---

## Notes

- SHA512 calculation requires v0.16.0 tag (defer if not tagged)
- vcpkg testing can proceed with placeholder SHA512
- All critical work (tests, ports) complete
- J2-J6 are administrative/finalization tasks

---

**Last Updated**: 2025-11-30 21:34 HKT  
**Status**: Ready to begin  
**Next**: Start J1 (Test vcpkg ports)