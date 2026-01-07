# Session 46: Critical Bug Fixes - Manpage & Architecture

**Date**: 2025-12-27+
**Previous Session**: Session 45 (Infrastructure complete)
**Estimated Time**: 3-4 hours
**Status**: Ready to start

---

## Context from Session 45

### ✅ Completed
- Tool_support library builds and installs via vcpkg
- Tools build separately using installed libraries
- Basic functionality validated (mkdwarfs, dwarfsck work)
- Documentation comprehensive

### ❌ Critical Issues Remaining

**Issue 1: Manpage Support Broken**
- `--man` option disabled in separate tool builds
- Stub implementations return empty documents
- Users can't access integrated manpages

**Issue 2: vcpkg Tools Crash (SIGILL)**
- Tools built via vcpkg crash with "Illegal instruction: 4"
- Architecture/optimization mismatch
- Makes vcpkg-built tools unusable

---

## Session 46 Objectives

### Primary Goals
1. ✅ Implement proper manpage support in tool_support library
2. ✅ Fix vcpkg architecture optimization issues
3. ✅ Validate full end-to-end workflow
4. ✅ All tools work correctly in all build modes

---

## Phase 1: Manpage Support Implementation (90 min)

### Root Cause Analysis (15 min)

**Current Architecture**:
```
Full Build:
  cmake/render_manpage.cmake → Python script → *_manpage.cpp (generated)
  tools/CMakeLists.txt → adds to *_main OBJECT library

Separate Build:
  No manpage generation → tools/src/manpage_stubs.cpp (empty)
```

**Problem**: Manpages generated during build, not in library

### Solution Strategy (75 min)

**Option A: Include Generated Manpages in tool_support** (RECOMMENDED)

1. Generate manpages during library build (30 min)
   - Move manpage generation to `cmake/tool_support.cmake`
   - Generate all 4 manpages: mkdwarfs, dwarfs, dwarfsck, dwarfsextract
   - Add generated files to library sources

2. Export manpage headers (15 min)
   - Install generated `*_manpage.cpp` files
   - Or: Convert to headers `*_manpage.h` for cleaner export

3. Update tool builds (15 min)
   - Remove `manpage_stubs.cpp`
   - Link against library-provided manpages
   - Test `--man` option works

4. Validation (15 min)
   - Test in full build: `./mkdwarfs --man | head`
   - Test in separate build: `./build-tools/mkdwarfs --man | head`
   - Test in vcpkg build: verify manpages available

**Option B: Runtime Manpage Loading** (Alternative)

1. Embed manpage markdown in library (45 min)
   - Read doc/*.md files at compile time
   - Store as string literals in library
   - Render at runtime via mistletoe

2. Simpler but larger binary size

**Decision**: Use Option A (generated .cpp files in library)

---

## Phase 2: vcpkg Architecture Fix (90 min)

### Root Cause Analysis (30 min)

**Symptoms**:
```
./build-tools/mkdwarfs -i input -o output.dff
Illegal instruction: 4
```

**Hypothesis**:
1. vcpkg uses different optimization flags than system build
2. Possible SIMD instruction mismatch (AVX2/AVX512 on ARM64)
3. Library/tool architecture mismatch

**Investigation Steps**:
1. Check compiler flags: `cmake -B build-tools -S tools -LA | grep FLAGS`
2. Check vcpkg triplet: `cat $VCPKG_ROOT/triplets/arm64-osx-static.cmake`
3. Compare with working build: `cmake -B build-test -LA | grep FLAGS`
4. Identify divergence

### Solution Implementation (60 min)

**Step 1: Identify Problematic Flags** (20 min)
```bash
# Dump vcpkg compiler flags
cmake -B build-vcpkg-test -S tools \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_VERBOSE_MAKEFILE=ON \
  --trace-expand 2>&1 | grep "CXX_FLAGS"

# Compare with working build
cmake -B build-system -S tools \
  -DCMAKE_VERBOSE_MAKEFILE=ON \
  --trace-expand 2>&1 | grep "CXX_FLAGS"
```

**Step 2: Fix in tools/CMakeLists.txt** (20 min)
```cmake
# Add after project()
if(APPLE)
  # Ensure compatible architecture flags for ARM64
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=apple-m1")
  # Or more conservative:
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a")
endif()
```

**Step 3: Alternative - Custom Triplet** (20 min)
```cmake
# Create custom triplet: triplets/arm64-osx-dwarfs.cmake
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)

# Conservative flags
set(VCPKG_CXX_FLAGS "-march=armv8-a -O2")
set(VCPKG_C_FLAGS "-march=armv8-a -O2")
```

---

## Phase 3: Comprehensive Validation (60 min)

### Test Matrix

| Build Mode | Manpage | Functional | Expected |
|------------|---------|------------|----------|
| Full build | `--man` | Create/check/extract | ✅ All pass |
| Separate (system libs) | `--man` | Create/check/extract | ✅ All pass |
| vcpkg build | `--man` | Create/check/extract | ✅ All pass |

### Validation Workflow (60 min)

```bash
# Test 1: Full build
cmake -B build-full -DWITH_LIBDWARFS=ON -DWITH_TOOLS=ON
cmake --build build-full -j8
./build-full/mkdwarfs --man | head -20  # Must show manpage
./build-full/mkdwarfs -i /usr/share/dict -o test1.dff -l1
./build-full/dwarfsck test1.dff
./build-full/dwarfsextract -i test1.dff -o test1-out
diff -r /usr/share/dict test1-out  # Exit 0

# Test 2: Separate build (system libraries)
cmake -B build-sep -S tools
cmake --build build-sep
./build-sep/mkdwarfs --man | head -20  # Must show manpage
./build-sep/mkdwarfs -i /usr/share/dict -o test2.dff -l1
./build-sep/dwarfsck test2.dff
./build-sep/dwarfsextract -i test2.dff -o test2-out
diff -r /usr/share/dict test2-out  # Exit 0

# Test 3: vcpkg build
cd example/static-site-server && ./build.sh
cd ../.. && cmake -B build-vcpkg -S tools \
  -DCMAKE_PREFIX_PATH=example/static-site-server/build/vcpkg_installed/arm64-osx-static
cmake --build build-vcpkg
./build-vcpkg/mkdwarfs --man | head -20  # Must show manpage
./build-vcpkg/mkdwarfs -i /usr/share/dict -o test3.dff -l1  # Must NOT crash
./build-vcpkg/dwarfsck test3.dff
./build-vcpkg/dwarfsextract -i test3.dff -o test3-out
diff -r /usr/share/dict test3-out  # Exit 0
```

---

## Phase 4: Update Documentation (30 min)

### Remove "Known Limitations" Sections

Files to update:
1. `doc/TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md` - Remove limitations section
2. `doc/vcpkg-integration.md` - Remove manpage/SIGILL troubleshooting
3. `README.md` - Update "Building Tools Separately" section

### Add Success Stories

- Manpage support works in all build modes
- vcpkg builds fully functional
- No architecture issues

---

## Implementation Checklist

### Phase 1: Manpage Support
- [ ] Move manpage generation to `cmake/tool_support.cmake`
- [ ] Generate 4 manpage .cpp files during library build
- [ ] Add generated files as library sources
- [ ] Install manpage .cpp or .h files
- [ ] Update `tools/CMakeLists.txt` to use library manpages
- [ ] Remove `tools/src/manpage_stubs.cpp`
- [ ] Test `--man` in all build modes

### Phase 2: vcpkg Architecture Fix
- [ ] Analyze compiler flags (vcpkg vs system)
- [ ] Identify SIGILL root cause
- [ ] Implement fix (flags or custom triplet)
- [ ] Test vcpkg-built tools execute correctly
- [ ] Verify no performance regression

### Phase 3: Validation
- [ ] Full build: all tests pass
- [ ] Separate build: all tests pass
- [ ] vcpkg build: all tests pass
- [ ] Cross-validate manpages
- [ ] Cross-validate functionality

### Phase 4: Documentation
- [ ] Remove limitations from all docs
- [ ] Update success criteria
- [ ] Add performance notes
- [ ] Update troubleshooting (different issues)

---

## Success Criteria

**Session 46 Complete When**:
- [x] `--man` option works in all build modes
- [x] vcpkg-built tools execute without SIGILL
- [x] Full end-to-end workflow passes in all modes
- [x] Documentation reflects success (no known limitations)
- [x] Clean builds reproducible

**Definition of Done**:
```bash
# This ENTIRE workflow must succeed WITHOUT errors:
rm -rf build* vcpkg_installed test*.dff test*-out

# vcpkg workflow
cd example/static-site-server && ./build.sh && cd ../..
cmake -B build-vcpkg -S tools \
  -DCMAKE_PREFIX_PATH=$(pwd)/example/static-site-server/build/vcpkg_installed/arm64-osx-static \
  -DPKG_CONFIG_PATH=$(pwd)/example/static-site-server/build/vcpkg_installed/arm64-osx-static/lib/pkgconfig
cmake --build build-vcpkg

# All tests MUST pass
./build-vcpkg/mkdwarfs --man | head -20  # Shows manpage
./build-vcpkg/mkdwarfs -i /usr/share/dict -o test.dff -l1  # No crash
./build-vcpkg/dwarfsck test.dff  # Validates
./build-vcpkg/dwarfsextract -i test.dff -o test-out  # Extracts
diff -r /usr/share/dict test-out  # Exit 0
```

---

## Timeline

| Phase | Task | Est. Time |
|-------|------|-----------|
| 1.1 | Analyze current manpage architecture | 15 min |
| 1.2 | Generate manpages in tool_support | 30 min |
| 1.3 | Integrate into builds | 30 min |
| 1.4 | Test manpage functionality | 15 min |
| 2.1 | Analyze SIGILL root cause | 30 min |
| 2.2 | Implement architecture fix | 30 min |
| 2.3 | Test vcpkg builds | 30 min |
| 3   | Comprehensive validation | 60 min |
| 4   | Update documentation | 30 min |

**Total**: 4 hours

---

## Risk Mitigation

### Risk 1: Manpage Gen Complexity
**Mitigation**: Use existing render_manpage.cmake logic
**Fallback**: Embed markdown directly instead of rendering

### Risk 2: SIGILL Unfixable
**Mitigation**: Document as vcpkg limitation, use system builds
**Escalation**: Report to vcpkg team as triplet issue

### Risk 3: Performance Regression
**Mitigation**: Benchmark before/after
**Acceptance**: 10% slowdown acceptable for correctness

---

**Focus**: Fix the bugs completely, don't document workarounds
**Deadline**: Complete both fixes in single session