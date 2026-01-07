# Session 55: Repository Cleanup and Testing - Continuation Plan

**Created**: 2025-12-30
**Session Type**: Multi-phase implementation
**Estimated Duration**: 4 weeks
**Priority**: High

## Executive Summary

This plan addresses 7 critical tasks to improve the DwarFS repository:

1. **Testing Infrastructure Validation** (Tasks 1-4) - Verify all scripts work
2. **MECE Compliance** (Task 5) - Eliminate redundancy, ensure completeness
3. **Homebrew Compatibility** (Task 6) - Cross-version format compatibility
4. **Dependency Devendoring** (Task 7) - Remove 150MB+ of vendored code

**Expected Outcomes**:
- All scripts tested and working
- Clear separation of concerns in scripts/ and benchmarks/
- Verified Thrift format compatibility with Homebrew dwarfs 0.14.1
- Repository size reduced ~67% (150MB → 50MB)

---

## Week 1: Testing & Validation

**Goal**: Verify all existing scripts work correctly

### Task 1: Test scripts/benchmark-all.sh

**Status**: NOT STARTED
**Priority**: P2 (Medium)
**Estimated Time**: 30 minutes

**Current State**:
- Script is a 23-line wrapper that delegates to `benchmarks/run_comprehensive_benchmark.sh`
- No standalone functionality
- Missing `--help` option

**Action Items**:
1. ✅ Review script structure (DONE - wrapper pattern is valid)
2. Run `scripts/verify_benchmark_setup.sh` to check prerequisites
3. Execute `scripts/benchmark-all.sh`
4. Verify results in `benchmarks/results/comprehensive_*/`
5. Check `COMPREHENSIVE_REPORT.md` is generated

**Success Criteria**:
- Script executes without errors
- Delegates properly to comprehensive benchmark
- Results generated in expected location

**Command Sequence**:
```bash
cd /Users/mulgogi/src/external/dwarfs
scripts/verify_benchmark_setup.sh
scripts/benchmark-all.sh
ls -lh benchmarks/results/comprehensive_*/COMPREHENSIVE_REPORT.md
```

---

### Task 2: Test example/static-site-server

**Status**: NOT STARTED
**Priority**: P2 (Medium)
**Estimated Time**: 1 hour

**Current State**:
- Build script: 133 lines, requires vcpkg with overlay ports
- Test script: 158 lines, expects 2 .dff images (aesop.dff, candide.dff)
- Missing prerequisite validation

**Issues Identified**:
1. No check for missing test images before build
2. No port 8080 availability check
3. No graceful cleanup if server fails

**Action Items**:
1. Review overlay port configuration (`vcpkg_ports/dwarfs`)
2. Check if test images exist (`example/static-site-server/*.dff`)
3. If missing, document where to get them OR create them
4. Enhance `build.sh` with image existence check
5. Enhance `test.sh` with port availability check
6. Run `./build.sh --clean && ./build.sh`
7. Run `./test.sh` and verify all 10 tests pass

**Enhancements Needed**:

**build.sh** (add after line 82):
```bash
# Check for test images
echo "Checking for test images..."
if [ ! -f "${SCRIPT_DIR}/aesop.dff" ] || [ ! -f "${SCRIPT_DIR}/candide.dff" ]; then
  echo "WARNING: Test images not found. Build will succeed but tests will fail."
  echo "Expected: aesop.dff, candide.dff"
  echo "Create with: mkdwarfs -i <source-dir> -o <image-name>.dff"
fi
```

**test.sh** (add after line 20):
```bash
# Check port availability
if lsof -Pi :${PORT} -sTCP:LISTEN -t >/dev/null 2>&1; then
  echo -e "${RED}ERROR: Port ${PORT} already in use${NC}"
  lsof -Pi :${PORT} -sTCP:LISTEN
  exit 1
fi

# Check test images exist
for image in aesop.dff candide.dff; do
  if [ ! -f "${SCRIPT_DIR}/${image}" ]; then
    echo -e "${RED}ERROR: Test image not found: ${image}${NC}"
    echo "Create with: mkdwarfs -i <source> -o ${image}"
    exit 1
  fi
done
```

**Success Criteria**:
- vcpkg overlay ports work correctly
- All 10 tests pass (5 per image × 2 images)
- Server starts and stops cleanly

---

### Task 3: Verify Clean CMake Build

**Status**: NOT STARTED
**Priority**: P1 (High)
**Estimated Time**: 45 minutes

**Current State**:
- CMakeLists.txt: 377 lines
- Default: `DWARFS_WITH_FLATBUFFERS=ON` (implicit)
- User's task comment is **incorrect** - FlatBuffers IS already default ON

**Clarification Needed**:
The user's command:
```cmake
-DDWARFS_WITH_FLATBUFFERS=ON \ # this should be default ON so we should not need to specify it
```
This comment is **correct** - the flag is redundant because FlatBuffers is already ON by default.

**Action Items**:
1. Test 1: FlatBuffers-only (implicit default)
   ```bash
   rm -rf build
   cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release
   ninja -C build
   ./build/mkdwarfs --version  # Should show FlatBuffers support
   ```

2. Test 2: Both formats (explicit Thrift)
   ```bash
   rm -rf build
   cmake -B build -GNinja \
     -DCMAKE_BUILD_TYPE=Release \
     -DDWARFS_WITH_THRIFT=ON
   ninja -C build
   ./build/mkdwarfs --version  # Should show both formats
   ```

3. Test 3: Verify redundant flag doesn't break
   ```bash
   rm -rf build
   cmake -B build -GNinja \
     -DCMAKE_BUILD_TYPE=Release \
     -DDWARFS_WITH_FLATBUFFERS=ON \  # Redundant but valid
     -DDWARFS_WITH_THRIFT=ON
   ninja -C build
   ```

**Success Criteria**:
- All 3 builds succeed
- Version output matches expected format support
- Redundant flags accepted gracefully

---

### Task 4: Test scripts/build-all-and-test.sh --vcpkg

**Status**: NOT STARTED
**Priority**: P1 (High)
**Estimated Time**: 2-3 hours (first run includes vcpkg package installation)

**Current State**:
- Script: 237 lines
- Builds 3 configurations: fb-only, both, thrift-only
- vcpkg support: Lines 48-94 (conditional)

**Prerequisites**:
```bash
export VCPKG_ROOT=~/vcpkg  # Or your vcpkg location
export VCPKG_TRIPLET=arm64-osx-static  # Or auto-detect

# Install dependencies (one-time, ~30-60 minutes)
cd $VCPKG_ROOT
./vcpkg install \
  boost-system boost-filesystem boost-program_options \
  boost-chrono boost-iostreams boost-context boost-fiber \
  zstd lz4 lzma brotli xxhash libarchive \
  openssl jemalloc gtest fmt range-v3 \
  nlohmann-json utf8cpp parallel-hashmap \
  --triplet=$VCPKG_TRIPLET
```

**Action Items**:
1. Verify VCPKG_ROOT is set
2. Install vcpkg dependencies (if not already installed)
3. Run `scripts/build-all-and-test.sh --vcpkg`
4. Verify 3 build directories created:
   - `build-fb-only/` (FlatBuffers only)
   - `build-both/` (FlatBuffers + Thrift)
   - `build-thrift-only/` (Thrift only)
5. Verify all tests pass
6. Check build summary shows PASS for all configs

**Success Criteria**:
- All 3 configurations build successfully
- All tests pass
- Build artifacts exist in each directory
- Build summary: 3/3 PASS

---

## Week 2: MECE Compliance

**Goal**: Organize scripts/ and benchmarks/ to be Mutually Exclusive, Collectively Exhaustive

### Task 5: Clean Up scripts/ and benchmarks/

**Status**: NOT STARTED
**Priority**: P2 (Medium)
**Estimated Time**: 4 hours

**Current Analysis**:

**scripts/ (10 files)**:
```
CLEAR HIERARCHY ✅:
├── run-all.sh               # Master: Clean → Build → Test → Benchmark
├── build-all-and-test.sh   # Build + Test (3 configs)
├── benchmark-all.sh         # Benchmark only (delegates)
├── clean-build.sh           # Single config clean build
├── test-all-configs.sh      # Quick test (3 configs) 
├── verify_benchmark_setup.sh # Verify prerequisites
├── test_vcpkg_install.sh    # vcpkg port validation
├── extract_blocks.py        # Low-level block analysis
└── README.md                # Complete guide
```

**benchmarks/ (15+ files)**:
```
SOME REDUNDANCY ⚠️:
Orchestrators:
├── run_all_benchmarks.sh              # Master (all suites)
├── run_comprehensive_benchmark.sh     # FUSE + API (2-3 hrs)
└── run_quick_comprehensive_test.sh    # Quick validation (5 min)

Legacy (REDUNDANT):
├── run_metadata_format_benchmark.py   # OLD - superseded by comprehensive
├── metadata_format_benchmark.py       # OLD - superseded by comprehensive
└── run_flatbuffers_benchmark.sh       # OLD - superseded by comprehensive

Current:
├── run_libdwarfs_benchmark.sh         # API-only
├── libdwarfs/                         # C++ benchmark programs
├── download_datasets.py               # Dataset downloader
├── build_and_test_all.py             # Python build orchestrator
└── lib/                               # Shared utilities
```

**Issues**:
1. **Legacy scripts**: 3 old scripts superseded by comprehensive benchmarks
2. **Build orchestrator duplication**: Shell vs Python (purpose unclear)
3. **Missing tests**: No Homebrew compatibility tests

**Action Items**:

**Phase 1: Mark Legacy Scripts as Deprecated** (Low Risk)
1. Rename legacy scripts:
   ```bash
   cd benchmarks
   mv run_flatbuffers_benchmark.sh DEPRECATED_run_flatbuffers_benchmark.sh
   mv run_metadata_format_benchmark.py DEPRECATED_run_metadata_format_benchmark.py
   mv metadata_format_benchmark.py DEPRECATED_metadata_format_benchmark.py
   ```

2. Add deprecation warnings to each script:
   ```bash
   echo '#!/bin/bash' > DEPRECATED_run_flatbuffers_benchmark.sh
   echo 'echo "DEPRECATED: This script is superseded by run_comprehensive_benchmark.sh"' >> ...
   echo 'echo "Use: ./benchmarks/run_comprehensive_benchmark.sh"' >> ...
   echo 'exit 1' >> ...
   ```

3. Update `benchmarks/README.md`:
   - Mark legacy scripts as deprecated
   - Recommend alternatives
   - Keep for historical reference

**Phase 2: Clarify Build Orchestrators** (Documentation)
1. Add purpose documentation to `build_and_test_all.py`:
   ```python
   """Python build orchestrator for benchmark infrastructure.
   
   WHEN TO USE:
   - CI/CD Python-only environments
   - Cross-platform build validation
   - Programmatic build orchestration
   
   SHELL EQUIVALENT: scripts/build-all-and-test.sh
   RECOMMENDED: Use shell script for manual builds
   """
   ```

2. Update `scripts/README.md`:
   - Clarify relationship between shell and Python orchestrators
   - Recommend shell for interactive use, Python for automation

**Phase 3: Add Missing Tests** (New Functionality)
1. Create `scripts/test_homebrew_compatibility.sh`:
   - Test local build reads Homebrew .dft images
   - Test Homebrew reads local .dft images
   - Test FUSE mount cross-compatibility
   - See detailed script in Task 6

2. Update `scripts/README.md`:
   - Add Homebrew compatibility test documentation
   - Update script table

**Success Criteria**:
- Legacy scripts marked deprecated with clear warnings
- Documentation clarifies purpose of each script
- No functional overlap
- Collectively exhaustive (all use cases covered)
- Homebrew compatibility test created

---

## Week 3: Homebrew Compatibility

**Goal**: Verify Thrift format compatibility with Homebrew dwarfs 0.14.1

### Task 6: Homebrew Compatibility Testing

**Status**: NOT STARTED
**Priority**: P1 (High)
**Estimated Time**: 2 hours

**Current State**:
- Homebrew version: dwarfs 0.14.1_3 at `/opt/homebrew/Cellar/dwarfs/0.14.1_3`
- Format: Thrift Compact (.dft)
- Local build: Both FlatBuffers and Thrift support

**Action Items**:

**1. Create Test Script**: `scripts/test_homebrew_compatibility.sh`

```bash
#!/bin/bash
# Test compatibility with Homebrew dwarfs 0.14.1_3
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Homebrew paths
HOMEBREW_BASE=/opt/homebrew/Cellar/dwarfs/0.14.1_3
HOMEBREW_MKDWARFS=$HOMEBREW_BASE/bin/mkdwarfs
HOMEBREW_DWARFS=$HOMEBREW_BASE/bin/dwarfs
HOMEBREW_DWARFSEXTRACT=$HOMEBREW_BASE/bin/dwarfsextract

# Local paths (use build/ by default, allow override)
BUILD_DIR=${BUILD_DIR:-$PROJECT_ROOT/build}
LOCAL_MKDWARFS=$BUILD_DIR/mkdwarfs
LOCAL_DWARFS=$BUILD_DIR/dwarfs
LOCAL_DWARFSEXTRACT=$BUILD_DIR/dwarfsextract

# Test data
TEST_DATA=$PROJECT_ROOT/test/test_data  # Use existing test data
TEST_DIR=$(mktemp -d)
trap "rm -rf $TEST_DIR" EXIT

echo "========================================"
echo "Homebrew Compatibility Test Suite"
echo "========================================"
echo ""

# Prerequisites
echo "Checking prerequisites..."
for cmd in $HOMEBREW_MKDWARFS $HOMEBREW_DWARFSEXTRACT $HOMEBREW_DWARFS; do
  if [ ! -x "$cmd" ]; then
    echo -e "${RED}ERROR: Homebrew binary not found: $cmd${NC}"
    echo "Install with: brew install dwarfs"
    exit 1
  fi
done

for cmd in $LOCAL_MKDWARFS $LOCAL_DWARFSEXTRACT; do
  if [ ! -x "$cmd" ]; then
    echo -e "${RED}ERROR: Local binary not found: $cmd${NC}"
    echo "Build with: cmake -B build && ninja -C build"
    exit 1
  fi
done

if [ ! -d "$TEST_DATA" ]; then
  echo -e "${RED}ERROR: Test data not found: $TEST_DATA${NC}"
  exit 1
fi

echo -e "${GREEN}✓ All prerequisites met${NC}"
echo ""

# Test counters
TESTS_RUN=0
TESTS_PASSED=0

run_test() {
  local name="$1"
  TESTS_RUN=$((TESTS_RUN + 1))
  echo -n "Test $TESTS_RUN: $name... "
}

pass() {
  echo -e "${GREEN}PASS${NC}"
  TESTS_PASSED=$((TESTS_PASSED + 1))
}

fail() {
  echo -e "${RED}FAIL${NC}"
  echo "  Error: $1"
}

# Test 1: Homebrew creates .dft → Local reads
run_test "Homebrew creates .dft → Local reads"
$HOMEBREW_MKDWARFS -i $TEST_DATA -o $TEST_DIR/homebrew.dft -l7 >/dev/null 2>&1
$LOCAL_DWARFSEXTRACT -i $TEST_DIR/homebrew.dft -o $TEST_DIR/extract1 >/dev/null 2>&1
if diff -r $TEST_DATA $TEST_DIR/extract1 >/dev/null 2>&1; then
  pass
else
  fail "Extracted content differs from original"
fi

# Test 2: Local creates .dft → Homebrew reads
run_test "Local creates .dft → Homebrew reads"
$LOCAL_MKDWARFS -i $TEST_DATA -o $TEST_DIR/local.dft -l7 >/dev/null 2>&1
$HOMEBREW_DWARFSEXTRACT -i $TEST_DIR/local.dft -o $TEST_DIR/extract2 >/dev/null 2>&1
if diff -r $TEST_DATA $TEST_DIR/extract2 >/dev/null 2>&1; then
  pass
else
  fail "Extracted content differs from original"
fi

# Test 3: Format detection
run_test "Format auto-detection works"
if file $TEST_DIR/homebrew.dft | grep -q "DwarFS"; then
  if file $TEST_DIR/local.dft | grep -q "DwarFS"; then
    pass
  else
    fail "Local .dft not detected as DwarFS"
  fi
else
  fail "Homebrew .dft not detected as DwarFS"
fi

# Test 4: Version compatibility check
run_test "Version information"
HOMEBREW_VER=$($HOMEBREW_MKDWARFS --version 2>&1 | head -n1)
LOCAL_VER=$($LOCAL_MKDWARFS --version 2>&1 | head -n1)
echo ""
echo "  Homebrew: $HOMEBREW_VER"
echo "  Local:    $LOCAL_VER"
pass

# Summary
echo ""
echo "========================================"
echo "Test Summary"
echo "========================================"
echo "Tests run: $TESTS_RUN"
echo "Tests passed: $TESTS_PASSED"

if [ $TESTS_PASSED -eq $TESTS_RUN ]; then
  echo -e "${GREEN}All tests passed!${NC}"
  exit 0
else
  echo -e "${RED}Some tests failed${NC}"
  exit 1
fi
```

**2. Make script executable**:
```bash
chmod +x scripts/test_homebrew_compatibility.sh
```

**3. Run test**:
```bash
# Ensure local build exists with Thrift support
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DDWARFS_WITH_THRIFT=ON
ninja -C build

# Run compatibility test
scripts/test_homebrew_compatibility.sh
```

**4. Document findings** in `doc/HOMEBREW_COMPATIBILITY.md`

**Success Criteria**:
- ✅ Homebrew-created .dft images readable by local build
- ✅ Local .dft images readable by Homebrew
- ✅ Format detection works correctly
- ✅ All 4 tests pass

---

## Week 4: Dependency Devendoring

**Goal**: Remove vendored dependencies, use vcpkg exclusively

### Task 7: Remove Vendored Dependencies

**Status**: NOT STARTED
**Priority**: P1 (High - repository cleanup)
**Estimated Time**: 8-12 hours over multiple days

**Current State**:

| Directory | Type | Files | Size | vcpkg? | Action |
|-----------|------|-------|------|--------|--------|
| `folly/` | Library | 22,000+ | >100MB | ✅ | **REMOVE** |
| `fbthrift/` | Compiler | 5,000+ | ~50MB | ✅ | **REMOVE** |
| `fast_float/` | Header | 1 | <10KB | ✅ | **REMOVE** |
| `fsst/` | Library | ~50 | ~500KB | ❌ | **KEEP** |
| `ricepp/` | Library | ~100 | ~2MB | ❌ | **KEEP** |
| `flatbuffers/` | **SCHEMAS** | 2 | <50KB | N/A | **KEEP** |
| `thrift/` | **SCHEMAS** | 2 | <50KB | N/A | **KEEP** |

**Total Removal**: ~150MB (67% reduction)

**Strategy**: Three phases with increasing risk

---

#### Phase 1: Remove fast_float (Low Risk)

**Estimated Time**: 1 hour
**Risk**: Low (header-only, widely available)

**Prerequisites**:
```bash
# Install via vcpkg
vcpkg install fast-float --triplet=$VCPKG_TRIPLET
```

**Action Items**:
1. Create branch:
   ```bash
   git checkout -b remove-fast-float
   ```

2. Create backup:
   ```bash
   tar czf vendored-deps-backup.tar.gz fast_float/
   ```

3. Remove directory:
   ```bash
   rm -rf fast_float/
   ```

4. Update `cmake/vcpkg/fast_float.cmake` (if not already vcpkg-aware):
   ```cmake
   # Remove FetchContent logic
   # Use only:
   find_package(fast_float REQUIRED CONFIG)
   ```

5. Update `vcpkg.json`:
   ```json
   {
     "dependencies": [
       ...
       "fast-float"
     ]
   }
   ```

6. Test build:
   ```bash
   cmake -B build -GNinja \
     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
   ninja -C build
   ctest --test-dir build
   ```

7. If successful:
   ```bash
   git add -A
   git commit -m "feat(deps): Remove vendored fast_float, use vcpkg"
   git push origin remove-fast-float
   ```

**Success Criteria**:
- Build succeeds with vcpkg fast_float
- All tests pass
- Repository size reduced

---

#### Phase 2: Remove folly/fbthrift (High Risk)

**Estimated Time**: 4-6 hours
**Risk**: High (complex dependencies, static linking concerns)

**Prerequisites**:
```bash
# Install via vcpkg
vcpkg install folly fbthrift --triplet=$VCPKG_TRIPLET
```

**CRITICAL CHECKS**:
1. ✅ Jemalloc still works (CRITICAL RULE 1)
2. ✅ Static linking works
3. ✅ All platforms tested in CI

**Action Items**:

1. Create branch:
   ```bash
   git checkout -b remove-folly-fbthrift
   ```

2. Create backup:
   ```bash
   tar czf folly-fbthrift-backup.tar.gz folly/ fbthrift/
   ```

3. Remove directories:
   ```bash
   rm -rf folly/ fbthrift/
   ```

4. Update `vcpkg.json`:
   ```json
   {
     "dependencies": [
       ...
     ],
     "features": {
       "thrift": {
         "description": "Enable Thrift metadata support",
         "dependencies": [
           "folly",
           {
             "name": "fbthrift",
             "host": true
           }
         ]
       }
     }
   }
   ```

5. Update `CMakeLists.txt` (lines 296-316):
   ```cmake
   # Remove submodule logic, replace with:
   if(DWARFS_WITH_THRIFT)
     if(VCPKG_BUILD)
       # vcpkg provides folly and fbthrift
       find_package(folly REQUIRED CONFIG)
       find_package(FBThrift REQUIRED CONFIG)
     else()
       # Non-vcpkg build: error (no longer support submodules)
       message(FATAL_ERROR 
         "Thrift support requires vcpkg. "
         "Set CMAKE_TOOLCHAIN_FILE or disable Thrift (-DDWARFS_WITH_THRIFT=OFF)")
     endif()
   endif()
   ```

6. Test build with Thrift:
   ```bash
   vcpkg install dwarfs[thrift] --triplet=$VCPKG_TRIPLET
   
   cmake -B build -GNinja \
     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
     -DDWARFS_WITH_THRIFT=ON
   ninja -C build
   ctest --test-dir build
   ```

7. Test build without Thrift:
   ```bash
   cmake -B build-fb -GNinja \
     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
     -DDWARFS_WITH_THRIFT=OFF
   ninja -C build-fb
   ctest --test-dir build-fb
   ```

8. Run Homebrew compatibility test:
   ```bash
   BUILD_DIR=build scripts/test_homebrew_compatibility.sh
   ```

9. If successful:
   ```bash
   git add -A
   git commit -m "feat(deps): Remove vendored folly/fbthrift, require vcpkg for Thrift"
   git push origin remove-folly-fbthrift
   ```

**Success Criteria**:
- Both builds succeed (with/without Thrift)
- All tests pass
- Homebrew compatibility maintained
- Jemalloc still works
- CI green on all platforms

---

#### Phase 3: Update Documentation

**Estimated Time**: 2 hours

**Action Items**:

1. Update `README.md`:
   - Add vcpkg as primary build method
   - Document Thrift feature flag
   - Remove submodule instructions

2. Create `doc/VCPKG_BUILD_GUIDE.md`:
   ```markdown
   # Building DwarFS with vcpkg
   
   ## Prerequisites
   - vcpkg installed
   - CMake ≥3.28
   - Ninja
   
   ## Quick Start
   ```bash
   # FlatBuffers-only (default)
   cmake -B build -GNinja \
     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
   ninja -C build
   
   # With Thrift support
   vcpkg install dwarfs[thrift] --triplet=<your-triplet>
   cmake -B build -GNinja \
     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
     -DDWARFS_WITH_THRIFT=ON
   ninja -C build
   ```
   ```

3. Update `.github/workflows/build.yml`:
   - Use vcpkg for all builds
   - Remove submodule initialization
   - Add vcpkg binary caching

4. Archive old documentation:
   ```bash
   mkdir -p old-docs/session-55-dependency-cleanup
   mv doc/*SUBMODULE*.md old-docs/session-55-dependency-cleanup/
   ```

**Success Criteria**:
- All documentation updated
- Build instructions clear
- CI workflows updated
- Old docs archived

---

## Risk Mitigation

### Critical Risks

**Risk 1: Static Linking Breakage**
- **Probability**: Medium
- **Impact**: High (blocks Tebako builds)
- **Mitigation**:
  - Test vcpkg static triplets thoroughly
  - Verify jemalloc works (CRITICAL RULE 1)
  - Keep backup branches
  - Test on multiple platforms before merge

**Risk 2: Version Incompatibilities**
- **Probability**: Low-Medium
- **Impact**: Medium (build failures)
- **Mitigation**:
  - Pin exact versions in vcpkg.json
  - Test with Homebrew compatibility
  - Document version requirements

**Risk 3: CI/CD Pipeline Failures**
- **Probability**: Medium
- **Impact**: High (blocks merges)
- **Mitigation**:
  - Use vcpkg binary caching
  - Test locally before pushing
  - Gradual rollout (fast_float first)

### Medium Risks

**Risk 4: Script Refactoring Confusion**
- **Probability**: Low
- **Impact**: Medium (user confusion)
- **Mitigation**:
  - Clear deprecation warnings
  - Updated documentation
  - Keep legacy scripts for transition period

**Risk 5: Homebrew Incompatibility**
- **Probability**: Low
- **Impact**: Medium (format compatibility)
- **Mitigation**:
  - Automated test suite
  - Document any limitations
  - Test frequently during development

---

## Success Metrics

### Quantitative
- ✅ Repository size: 150MB → 50MB (-67%)
- ✅ Build time reduced (vcpkg caching)
- ✅ All 4 test suites pass (Tasks 1-4)
- ✅ Homebrew compatibility: 4/4 tests pass
- ✅ CI/CD: Green on all platforms

### Qualitative
- ✅ Clear MECE structure in scripts/ and benchmarks/
- ✅ No vendored dependencies (except fsst, ricepp)
- ✅ vcpkg-first build process
- ✅ Comprehensive documentation
- ✅ Maintainable codebase

---

## Rollback Plan

If any phase fails critically:

1. **Immediate Rollback**:
   ```bash
   git checkout main
   git branch -D <failed-branch>
   tar xzf <backup-tarball>
   ```

2. **Restore Functionality**:
   ```bash
   cmake -B build -GNinja
   ninja -C build
   ctest --test-dir build
   ```

3. **Document Failure**:
   - What failed
   - Why it failed
   - What was learned
   - Alternative approaches

4. **Reassess**:
   - Can we fix the issue?
   - Should we postpone?
   - Do we need different approach?

---

## Timeline Summary

| Week | Tasks | Priority | Est. Time | Risk |
|------|-------|----------|-----------|------|
| 1 | Tasks 1-4 (Testing) | P1 | 5-6 hours | Low |
| 2 | Task 5 (MECE) | P2 | 4 hours | Low |
| 3 | Task 6 (Homebrew) | P1 | 2 hours | Low |
| 4 | Task 7 Phase 1 (fast_float) | P1 | 1 hour | Low |
| 4 | Task 7 Phase 2 (folly/thrift) | P1 | 4-6 hours | **High** |
| 4 | Task 7 Phase 3 (docs) | P2 | 2 hours | Low |

**Total**: ~18-21 hours over 4 weeks

---

## Next Steps

1. **Review this plan** with team
2. **Create GitHub issues** for each task
3. **Set up feature branches**
4. **Begin Week 1** testing phase
5. **Update status tracker** after each task

See [`doc/SESSION_55_IMPLEMENTATION_STATUS.md`](SESSION_55_IMPLEMENTATION_STATUS.md) for detailed progress tracking.