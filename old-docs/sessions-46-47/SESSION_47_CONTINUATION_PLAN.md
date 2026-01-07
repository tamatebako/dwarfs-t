# Session 47: Complete Critical Bug Fixes - Testing & vcpkg SIGILL

**Date**: 2025-12-27+
**Previous Session**: Session 46 (Manpage implementation complete)
**Estimated Time**: 3-4 hours
**Status**: Ready to start
**Priority**: CRITICAL (1 of 2 bugs fixed, 1 remaining)

---

## Context from Session 46

### ✅ Completed in Session 46
- **Manpage Support Implementation**: COMPLETE
  - Manpages generate during library build
  - 3 manpage .cpp files: mkdwarfs, dwarfs, dwarfsextract
  - FUSE compilation issues fixed
  - need_fuse.cmake duplicate target fixed
  - tools/CMakeLists.txt updated

### ❌ Critical Issues Remaining

**Issue 1: Manpage Testing Incomplete**
- Manpages generated but not tested end-to-end
- Separate tool builds not validated
- `--man` option functionality unverified

**Issue 2: vcpkg Tools Crash (SIGILL)** 🔴 CRITICAL
- Tools built via vcpkg crash with "Illegal instruction: 4"
- Architecture/optimization mismatch on ARM64
- Makes vcpkg-built tools completely unusable

---

## Session 47 Objectives

### Primary Goals
1. ✅ Complete manpage testing and validation
2. ✅ Fix vcpkg SIGILL crashes completely
3. ✅ Validate full end-to-end workflows
4. ✅ Update all documentation
5. ✅ Remove "Known Limitations" sections

---

## Phase 1: Complete Manpage Validation (45 min)

### 1.1 Test Full Build (15 min)

```bash
# Build with manpages
cmake -B build-full -DWITH_LIBDWARFS=ON -DWITH_TOOLS=ON \
  -DWITH_MAN_OPTION=ON -DWITH_FUSE_DRIVER=ON
cmake --build build-full -j8

# Test manpage display
./build-full/mkdwarfs --man | head -50
./build-full/dwarfs --man | head -50
./build-full/dwarfsck --man | head -50
./build-full/dwarfsextract --man | head -50

# Verify content is not empty
test $(./build-full/mkdwarfs --man | wc -l) -gt 100
```

**Success Criteria**:
- ✅ All 4 tools show formatted manpages
- ✅ Content matches doc/*.md files
- ✅ No empty output

### 1.2 Test Separate Tool Build (15 min)

```bash
# Install libraries to temp location
cmake --install build-full --prefix /tmp/dwarfs-test

# Build tools separately
export PKG_CONFIG_PATH=/tmp/dwarfs-test/lib/pkgconfig
cmake -B build-tools -S tools -DCMAKE_PREFIX_PATH=/tmp/dwarfs-test
cmake --build build-tools

# Test manpages
./build-tools/mkdwarfs --man | head -50
./build-tools/dwarfsck --man | head -50
./build-tools/dwarfsextract --man | head -50
```

**Success Criteria**:
- ✅ Tools build successfully
- ✅ Manpages display correctly
- ✅ No link errors

### 1.3 Test vcpkg Workflow (15 min)

```bash
# Build library via vcpkg
cd example/static-site-server && ./build.sh && cd ../..

# Build tools using vcpkg libraries
cmake -B build-vcpkg-tools -S tools \
  -DCMAKE_PREFIX_PATH=$(pwd)/example/static-site-server/build/vcpkg_installed/arm64-osx-static \
  -DPKG_CONFIG_PATH=$(pwd)/example/static-site-server/build/vcpkg_installed/arm64-osx-static/lib/pkgconfig

cmake --build build-vcpkg-tools

# Test manpages
./build-vcpkg-tools/mkdwarfs --man | head -50
```

**Success Criteria**:
- ✅ Tools build via vcpkg libraries
- ✅ Manpages display
- ✅ (May still have SIGILL - fix in Phase 2)

---

## Phase 2: Fix vcpkg SIGILL Crashes (90 min)

### 2.1 Root Cause Analysis (30 min)

**Investigation Steps**:

1. **Check compiler flags divergence**
```bash
# vcpkg build flags
cmake -B build-vcpkg-debug -S tools \
  -DCMAKE_PREFIX_PATH=... \
  -DCM

AKE_VERBOSE_MAKEFILE=ON \
  2>&1 | tee vcpkg-flags.log

# System build flags
cmake -B build-sys-debug -DWITH_TOOLS=ON \
  -DCMAKE_VERBOSE_MAKEFILE=ON \
  2>&1 | tee system-flags.log

# Compare
diff vcpkg-flags.log system-flags.log | grep -E "(march|mcpu|mtune)"
```

2. **Check binary architecture**
```bash
otool -l build-vcpkg-tools/mkdwarfs | grep -A 5 LC_BUILD_VERSION
file build-vcpkg-tools/mkdwarfs
```

3. **Run with debugging**
```bash
lldb build-vcpkg-tools/mkdwarfs
(lldb) run --help
# Check where SIGILL occurs
```

**Expected Findings**:
- vcpkg may use `-march=native` or `-mcpu=apple-m2` incompatible flags
- OR: vcpkg enables AVX512/SIMD instructions not on ARM64
- OR: Library compiled with different flags than tools

### 2.2 Implement Fix (30 min)

**Solution A: Conservative Flags in tools/CMakeLists.txt**

```cmake
# Add after project() in tools/CMakeLists.txt

if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
  # Use baseline ARM64 instructions only
  add_compile_options(-march=armv8-a -mtune=generic)
  add_link_options(-march=armv8-a)
  message(STATUS "Using conservative ARM64 flags for compatibility")
endif()
```

**Solution B: Custom vcpkg Triplet**

Create `vcpkg_ports/dwarfs/triplets/arm64-osx-dwarfs-static.cmake`:
```cmake
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)

# Conservative optimization for compatibility
set(VCPKG_CXX_FLAGS "-O2 -march=armv8-a -mtune=generic")
set(VCPKG_C_FLAGS "-O2 -march=armv8-a -mtune=generic")
```

**Solution C: Disable Problematic Optimizations**

```cmake
# If SIMD instructions causing issue
if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
  add_compile_options(-mno-outline-atomics)
endif()
```

**Decision**: Implement Solution A first (simplest), fallback to B if needed

### 2.3 Test & Validate (30 min)

```bash
# Rebuild with fix
rm -rf build-vcpkg-tools
cmake -B build-vcpkg-tools -S tools -DCMAKE_PREFIX_PATH=...
cmake --build build-vcpkg-tools

# Test execution (critical!)
./build-vcpkg-tools/mkdwarfs --help
echo "Exit code: $?"  # MUST be 0, not 132

# Test functionality
./build-vcpkg-tools/mkdwarfs -i /usr/share/dict -o test.dff -l1
./build-vcpkg-tools/dwarfsck test.dff
./build-vcpkg-tools/dwarfsextract -i test.dff -o test-out
diff -r /usr/share/dict test-out
```

**Success Criteria**:
- ✅ Tools execute without SIGILL
- ✅ All operations complete successfully
- ✅ Output files valid
- ✅ Performance acceptable (within 10% of system build)

---

## Phase 3: Comprehensive Validation (60 min)

### 3.1 Full Test Matrix (40 min)

| Build Mode | Manpage | Create | Check | Extract | Status |
|------------|---------|--------|-------|---------|--------|
| Full build | ✅ | ✅ | ✅ | ✅ | ⏳ Test |
| Separate (system libs) | ✅ | ✅ | ✅ | ✅ | ⏳ Test |
| vcpkg build | ✅ | ✅ | ✅ | ✅ | ⏳ Test |

**Test Script**:
```bash
#!/bin/bash
set -e

test_workflow() {
  local build_dir=$1
  local name=$2

  echo "=== Testing $name ==="

  # 1. Manpage
  echo "Testing manpage..."
  $build_dir/mkdwarfs --man | head -20 || return 1

  # 2. Create filesystem
  echo "Creating filesystem..."
  $build_dir/mkdwarfs -i /usr/share/dict -o test-$name.dff -l1 || return 1

  # 3. Check filesystem
  echo "Checking filesystem..."
  $build_dir/dwarfsck test-$name.dff || return 1

  # 4. Extract filesystem
  echo "Extracting filesystem..."
  $build_dir/dwarfsextract -i test-$name.dff -o test-$name-out || return 1

  # 5. Verify content
  echo "Verifying content..."
  diff -r /usr/share/dict test-$name-out || return 1

  echo "✅ $name: ALL TESTS PASSED"
  rm -rf test-$name.dff test-$name-out
}

# Run all tests
test_workflow "build-full" "full-build"
test_workflow "build-tools" "separate-build"
test_workflow "build-vcpkg-tools" "vcpkg-build"

echo ""
echo "🎉 ALL WORKFLOWS PASSED!"
```

### 3.2 Performance Validation (20 min)

```bash
# Benchmark vcpkg vs system build
time build-full/mkdwarfs -i /usr/share/dict -o test-sys.dff -l3
time build-vcpkg-tools/mkdwarfs -i /usr/share/dict -o test-vcpkg.dff -l3

# Compare sizes
ls -lh test-sys.dff test-vcpkg.dff

# Acceptable: vcpkg within 10% of system build time
```

---

## Phase 4: Documentation Updates (45 min)

### 4.1 Remove "Known Limitations" (15 min)

**Files to Update**:

1. **`doc/TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md`**
   - Remove lines 213-226 ("Known Limitations" section)
   - Add "Session 46-47 Fixes" section documenting solutions

2. **`doc/vcpkg-integration.md`**
   - Remove manpage troubleshooting
   - Remove SIGILL troubleshooting
   - Update with success stories

3. **`README.md`**
   - Remove notes about `--man` disabled
   - Update "Building Tools Separately" section

### 4.2 Update Success Documentation (20 min)

**Add to `doc/TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md`**:

```markdown
## Session 46-47 Fixes

### Fixed: Manpage Support (Session 46)
- **Issue**: `--man` option disabled in separate tool builds
- **Solution**: Generate manpages during library build
- **Implementation**: `cmake/tool_support.cmake` lines 123-177
- **Status**: ✅ FIXED - All build modes support `--man`

### Fixed: vcpkg SIGILL Crashes (Session 47)
- **Issue**: Tools crash with "Illegal instruction: 4" on ARM64
- **Root Cause**: Architecture optimization mismatch
- **Solution**: Conservative ARM64 flags in tools/CMakeLists.txt
- **Status**: ✅ FIXED - vcpkg builds fully functional
```

### 4.3 Create Session Summary (10 min)

Create `doc/SESSION_46_47_COMPLETION_SUMMARY.md` documenting:
- Problems solved
- Solutions implemented
- Files modified
- Test results
- Performance metrics

---

## Implementation Checklist

### Phase 1: Manpage Validation
- [ ] Test full build manpages
- [ ] Test separate build manpages
- [ ] Test vcpkg build manpages
- [ ] Verify content correctness
- [ ] Document results

### Phase 2: vcpkg SIGILL Fix
- [ ] Analyze compiler flags
- [ ] Identify root cause
- [ ] Implement fix (Solution A/B/C)
- [ ] Test execution (no crashes)
- [ ] Validate functionality
- [ ] Benchmark performance

### Phase 3: Comprehensive Validation
- [ ] Run full test matrix
- [ ] All 3 build modes pass
- [ ] Performance acceptable
- [ ] Document results

### Phase 4: Documentation
- [ ] Remove limitations sections
- [ ] Update success documentation
- [ ] Create completion summary
- [ ] Move outdated docs to old-docs/

---

## Success Criteria

**Session 47 Complete When**:
- ✅ Manpages work in all 3 build modes
- ✅ vcpkg tools execute without SIGILL
- ✅ Full end-to-end workflow passes in all modes
- ✅ Performance within 10% of system build
- ✅ Documentation updated (no "Known Limitations")
- ✅ Completion summary created

**Definition of Done**:
```bash
# This ENTIRE workflow must succeed WITHOUT errors:
#!/bin/bash
set -e

# Clean start
rm -rf build-* test*.dff test*-out

# Test 1: Full build
cmake -B build-full -DWITH_LIBDWARFS=ON -DWITH_TOOLS=ON -DWITH_MAN_OPTION=ON
cmake --build build-full -j8
./build-full/mkdwarfs --man | head -20
./build-full/mkdwarfs -i /usr/share/dict -o test1.dff -l1
./build-full/dwarfsck test1.dff
./build-full/dwarfsextract -i test1.dff -o test1-out
diff -r /usr/share/dict test1-out

# Test 2: Separate build
cmake --install build-full --prefix /tmp/dwarfs-test
export PKG_CONFIG_PATH=/tmp/dwarfs-test/lib/pkgconfig
cmake -B build-tools -S tools -DCMAKE_PREFIX_PATH=/tmp/dwarfs-test
cmake --build build-tools
./build-tools/mkdwarfs --man | head -20
./build-tools/mkdwarfs -i /usr/share/dict -o test2.dff -l1
./build-tools/dwarfsck test2.dff
./build-tools/dwarfsextract -i test2.dff -o test2-out
diff -r /usr/share/dict test2-out

# Test 3: vcpkg build
cd example/static-site-server && ./build.sh && cd ../..
cmake -B build-vcpkg-tools -S tools \
  -DCMAKE_PREFIX_PATH=$(pwd)/example/static-site-server/build/vcpkg_installed/arm64-osx-static \
  -DPKG_CONFIG_PATH=$(pwd)/example/static-site-server/build/vcpkg_installed/arm64-osx-static/lib/pkgconfig
cmake --build build-vcpkg-tools
./build-vcpkg-tools/mkdwarfs --man | head -20
./build-vcpkg-tools/mkdwarfs -i /usr/share/dict -o test3.dff -l1  # MUST NOT CRASH
./build-vcpkg-tools/dwarfsck test3.dff
./build-vcpkg-tools/dwarfsextract -i test3.dff -o test3-out
diff -r /usr/share/dict test3-out

echo "🎉 ALL TESTS PASSED - SESSION 47 COMPLETE!"
```

---

## Timeline

| Phase | Task | Est. Time |
|-------|------|-----------|
| 1.1 | Test full build manpages | 15 min |
| 1.2 | Test separate build manpages | 15 min |
| 1.3 | Test vcpkg build manpages | 15 min |
| 2.1 | Analyze SIGILL root cause | 30 min |
| 2.2 | Implement architecture fix | 30 min |
| 2.3 | Test and validate fix | 30 min |
| 3.1 | Full test matrix | 40 min |
| 3.2 | Performance validation | 20 min |
| 4.1 | Remove limitations | 15 min |
| 4.2 | Update documentation | 20 min |
| 4.3 | Create summary | 10 min |

**Total**: 4 hours

---

## Risk Mitigation

### Risk 1: SIGILL Unfixable with Flags
**Mitigation**: Try all 3 solutions (A→B→C) progressively
**Fallback**: Document as vcpkg limitation, recommend system builds
**Escalation**: Report to vcpkg as arm64-osx triplet issue

### Risk 2: Performance Regression
**Mitigation**: Benchmark before/after with conservative flags
**Acceptance**: 10% slowdown acceptable for correctness
**Alternative**: Offer optimized vs compatible builds

### Risk 3: Manpage Content Issues
**Mitigation**: Verify against source doc/*.md files
**Fallback**: Fix render_manpage.py if needed
**Testing**: Visual inspection + automated checks

---

**Focus**: Complete both critical bugs in single session - no half measures!
**Deadline**: All fixes functional by end of session