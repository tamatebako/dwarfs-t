# Session 96: v0.17.0 Release Preparation & Validation

**Created**: 2026-01-06
**Prerequisite**: Session 95 complete ✅
**Goal**: Validate all 3 metadata formats, test Homebrew compatibility, fix scripts, and prepare v0.17.0 release

---

## Critical Context

### Three Metadata Formats in Two Builds

**Build 1: Default** (FlatBuffers + Legacy Thrift)
- FlatBuffers: Modern default (header-only)
- Legacy Thrift: Hand-coded, always available (no fbthrift)
- Location: `src/metadata/legacy/` (~2,500 lines)

**Build 2: Thrift-Enabled** (All Three Formats)
- FlatBuffers: Modern default
- Legacy Thrift: Hand-coded, always available
- Modern Thrift: fbthrift v2025.12.29.00 (optional)

### Format Compatibility Requirements

**Legacy Thrift MUST**:
- Read Homebrew dwarfs v0.14.1 images (`.dft` files)
- Write images that Homebrew dwarfs v0.14.1 can read
- Work in BOTH builds (always available)
- Location: `/opt/homebrew/Cellar/dwarfs/0.14.1_3`

**Modern Thrift MUST**:
- Use CompactProtocol with magic bytes `{0x82, 0x21}`
- Work only in Thrift-enabled builds
- Be benchmarkable alongside other formats

---

## Phase 1: Legacy Thrift Homebrew Compatibility (90 min)

### Task 1.1: Create Homebrew Compatibility Test Suite (40 min)

**Objective**: Verify our Legacy Thrift can read/write Homebrew dwarfs v0.14.1 images

**Test File**: `test/metadata/legacy/homebrew_compatibility_test.cpp`

**Tests to Write**:

1. **Read Homebrew Image** - Use Homebrew dwarfs to create image, we read it
   ```cpp
   TEST(HomebrewCompatibility, ReadHomebrewImage) {
     // 1. Create image using /opt/homebrew/bin/mkdwarfs
     // 2. Read with our dwarfs_reader
     // 3. Verify all files/metadata correct
     // 4. Extract and compare content
   }
   ```

2. **Write Image for Homebrew** - We create image, Homebrew dwarfs reads it
   ```cpp
   TEST(HomebrewCompatibility, HomebrewReadsOurImage) {
     // 1. Create image with our mkdwarfs (Legacy Thrift)
     // 2. Mount with /opt/homebrew/bin/dwarfs
     // 3. Verify mount succeeds
     // 4. Compare extracted files
   }
   ```

3. **Format Detection** - Verify format detection
   ```cpp
   TEST(HomebrewCompatibility, FormatDetection) {
     // 1. Read Homebrew image
     // 2. Verify detected as legacy_thrift (no magic bytes)
     // 3. Verify priority 50 (fallback)
   }
   ```

4. **Round-Trip** - Full round-trip compatibility
   ```cpp
   TEST(HomebrewCompatibility, RoundTrip) {
     // 1. Read Homebrew image
     // 2. Recompress with our tools
     // 3. Verify Homebrew can still read it
     // 4. Content identical
   }
   ```

**Integration**: Add to `cmake/metadata_serialization.cmake` test section

**Validation**:
- [ ] 4/4 tests PASS
- [ ] Can read Homebrew v0.14.1 images
- [ ] Homebrew can read our images
- [ ] Format detection correct

---

### Task 1.2: Test All Three Formats End-to-End (30 min)

**Objective**: Create, check, extract images with all formats

**Commands**:
```bash
# FlatBuffers
./build/mkdwarfs -i /usr/share/dict -o test-fb.dff
./build/dwarfsck test-fb.dff
./build/dwarfsextract -i test-fb.dff -o /tmp/extract-fb/

# Modern Thrift (Thrift-enabled build only)
./build/mkdwarfs -i /usr/share/dict -o test-mt.dtc --metadata-format=modern-thrift
./build/dwarfsck test-mt.dtc
./build/dwarfsextract -i test-mt.dtc -o /tmp/extract-mt/

# Legacy Thrift (explicit)
./build/mkdwarfs -i /usr/share/dict -o test-lt.dth --metadata-format=legacy-thrift
./build/dwarfsck test-lt.dth
./build/dwarfsextract -i test-lt.dth -o /tmp/extract-lt/

# Verify content identical
diff -r /tmp/extract-fb /tmp/extract-mt
diff -r /tmp/extract-fb /tmp/extract-lt
```

**Validation**:
- [ ] All formats create successfully
- [ ] Format detection works (FlatBuffers=DFBF, Modern={0x82,0x21}, Legacy=fallback)
- [ ] Extracted content byte-for-byte identical
- [ ] Size ordering: Modern < FlatBuffers < Legacy

---

### Task 1.3: Verify Default Build Behavior (20 min)

**Objective**: Ensure DWARFS_WITH_FLATBUFFERS=ON is default (no need to specify)

**Test**:
```bash
# Should work WITHOUT specifying FLATBUFFERS (already default ON)
cmake -B build-default \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=ON

# Check CMake output
ninja -C build-default | grep "FlatBuffers serialization.*ENABLED"

# Create image (should default to FlatBuffers)
./build-default/mkdwarfs -i /usr/share/dict -o default.dwarfs
./build-default/dwarfsck default.dwarfs | grep "flatbuffers"
```

**Validation**:
- [ ] FlatBuffers ON by default
- [ ] Legacy Thrift always available
- [ ] Modern Thrift OFF by default
- [ ] Default image uses FlatBuffers

---

## Phase 2: Benchmark System Validation (60 min)

### Task 2.1: Test Benchmark Scripts (30 min)

**Objective**: Ensure benchmark system works with all 3 formats

**Script 1**: `scripts/benchmark-all.sh`
```bash
cd scripts
./benchmark-all.sh

# Should test:
# - FlatBuffers creation/extraction
# - Modern Thrift creation/extraction (if enabled)
# - Legacy Thrift creation/extraction
```

**Script 2**: `benchmarks/build_and_test_all.py`
```bash
python3 benchmarks/build_and_test_all.py --build-all
python3 benchmarks/build_and_test_all.py --test-all
```

**Expected**:
- [ ] scripts/benchmark-all.sh works
- [ ] Benchmarks all 3 formats (if Thrift enabled)
- [ ] Results comparable to Session 19 data
- [ ] No errors or failures

---

### Task 2.2: Clean Up Benchmark/Script Organization (30 min)

**Objective**: Ensure MECE (Mutually Exclusive, Collectively Exhaustive) in benchmarks/scripts

**Review**:
1. **scripts/** - What's here?
   - benchmark-all.sh
   - build-all-and-test.sh
   - Others?

2. **benchmarks/** - What's here?
   - build_and_test_all.py
   - libdwarfs/
   - Others?

**MECE Principles**:
- No duplicate functionality between scripts/ and benchmarks/
- Clear separation: scripts/ = build/test automation, benchmarks/ = performance measurement
- Each script has single responsibility
- All use cases covered (collectively exhaustive)

**Actions**:
- [ ] Audit all scripts in scripts/ and benchmarks/
- [ ] Remove duplicates
- [ ] Document purpose of each
- [ ] Update README in each directory
- [ ] Ensure all work with 3 formats

---

## Phase 3: Integration Testing (60 min)

### Task 3.1: Example Application Test (20 min)

**Objective**: Verify static-site-server example works

**Commands**:
```bash
cd example/static-site-server

# Build
./build.sh

# Test
./test.sh
```

**Validation**:
- [ ] build.sh completes
- [ ] test.sh passes
- [ ] Can load FlatBuffers images
- [ ] Can load Modern Thrift images (if built)
- [ ] Can load Legacy Thrift images

---

### Task 3.2: vcpkg Build Test (20 min)

**Objective**: Ensure vcpkg build script works end-to-end

**Commands**:
```bash
# Clean slate
rm -rf build-vcpkg/

# Run official build script
./scripts/build-all-and-test.sh --vcpkg

# Should:
# - Build with vcpkg toolchain
# - Run all tests
# - Validate all formats
```

**Validation**:
- [ ] Build completes
- [ ] All tests pass
- [ ] Modern Thrift library built (if DWARFS_WITH_THRIFT=ON)
- [ ] Script returns 0

---

### Task 3.3: Default Build Test (20 min)

**Objective**: Verify clean default build works

**Commands**:
```bash
# Default build (should enable FlatBuffers + Legacy automatically)
cmake -B build-default \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=ON
# NOTE: No DWARFS_WITH_FLATBUFFERS needed (default ON)

ninja -C build-default
ctest --test-dir build-default -j

# Check included formats
./build-default/dwarfsck --help | grep "metadata-format"
```

**Validation**:
- [ ] FlatBuffers enabled by default
- [ ] Legacy Thrift always available
- [ ] Modern Thrift NOT included (OFF by default)
- [ ] All tests pass

---

## Phase 4: Format Compatibility Matrix (60 min)

### Task 4.1: Cross-Build Format Reading (30 min)

**Objective**: Verify format reading across builds

**Test Matrix**:

| Build Type | Can Read FB? | Can Read MT? | Can Read LT? |
|------------|--------------|--------------|--------------|
| Default (FB+LT) | ✅ Yes | ❓ Test | ✅ Yes |
| Thrift (FB+LT+MT) | ✅ Yes | ✅ Yes | ✅ Yes |

**Tests**:
```bash
# Create images with Thrift-enabled build
./build-modern/mkdwarfs -i /usr/share/dict -o fb.dff
./build-modern/mkdwarfs -i /usr/share/dict -o mt.dtc --metadata-format=modern-thrift
./build-modern/mkdwarfs -i /usr/share/dict -o lt.dth --metadata-format=legacy-thrift

# Test with default build (FB + LT only)
./build-default/dwarfsck fb.dff  # Should work ✅
./build-default/dwarfsck mt.dtc  # Should fail gracefully ❌
./build-default/dwarfsck lt.dth  # Should work ✅

# Test with Thrift-enabled build (all formats)
./build-modern/dwarfsck fb.dff  # Should work ✅
./build-modern/dwarfsck mt.dtc  # Should work ✅
./build-modern/dwarfsck lt.dth  # Should work ✅
```

**Validation**:
- [ ] Default build reads FB + LT correctly
- [ ] Default build gracefully handles MT (error message)
- [ ] Thrift build reads all 3 formats
- [ ] Error messages clear and helpful

---

### Task 4.2: Homebrew Interoperability (30 min)

**Objective**: Verify our Legacy Thrift works with Homebrew dwarfs v0.14.1

**Setup**:
```bash
# Verify Homebrew dwarfs available
/opt/homebrew/bin/mkdwarfs --version  # Should show 0.14.1
/opt/homebrew/bin/dwarfs --version
```

**Test 1: Read Homebrew Image**
```bash
# Create with Homebrew
/opt/homebrew/bin/mkdwarfs -i /usr/share/dict -o homebrew.dft
hexdump -C homebrew.dft | head -1  # Check: NO magic bytes

# Read with our tools
./build-default/dwarfsck homebrew.dft
# Should show: "metadata format: legacy_thrift"

./build-default/dwarfsextract -i homebrew.dft -o /tmp/extract-hb/
diff -r /usr/share/dict /tmp/extract-hb/  # Should match
```

**Test 2: Homebrew Reads Our Image**
```bash
# Create with our tools
./build-default/mkdwarfs -i /usr/share/dict -o ours.dth --metadata-format=legacy-thrift
hexdump -C ours.dth | head -1  # Check: NO magic bytes (fallback)

# Read with Homebrew
/opt/homebrew/bin/dwarfsck ours.dth
# Should succeed

# Extract with Homebrew
mkdir -p /tmp/hb-mount
/opt/homebrew/bin/dwarfs ours.dth /tmp/hb-mount
ls /tmp/hb-mount/  # Should show dict files
umount /tmp/hb-mount
```

**Validation**:
- [ ] Can read Homebrew v0.14.1 images
- [ ] Homebrew can read our Legacy Thrift images
- [ ] No magic bytes in Legacy Thrift images
- [ ] Content identical via diff

---

## Phase 5: Benchmark System Fix (60 min)

### Task 5.1: Fix benchmark-all.sh (30 min)

**Objective**: Ensure scripts/benchmark-all.sh works with all 3 formats

**Location**: `scripts/benchmark-all.sh`

**Requirements**:
- [ ] Tests FlatBuffers creation/extraction
- [ ] Tests Modern Thrift creation/extraction (if DWARFS_WITH_THRIFT=ON)
- [ ] Tests Legacy Thrift creation/extraction
- [ ] Produces comparison table
- [ ] Works in both build configurations

**Expected Output**:
```
Benchmarking DwarFS Metadata Formats
=====================================

Creation Time:
- FlatBuffers:    2.5s
- Modern Thrift:  3.0s (if enabled)
- Legacy Thrift:  2.8s

Extraction Time:
- FlatBuffers:    2.0s
- Modern Thrift:  1.9s (if enabled)
- Legacy Thrift:  2.1s

Image Size:
- FlatBuffers:    27.6 MB
- Modern Thrift:  27.3 MB (if enabled) ← smallest
- Legacy Thrift:  28.5 MB
```

---

### Task 5.2: Clean Up benchmarks/ and scripts/ (30 min)

**Objective**: Apply MECE principles to benchmark/script organization

**Audit**:

1. **scripts/** - Build/test automation
   - `benchmark-all.sh` → Metadata format benchmarking
   - `build-all-and-test.sh` → Full build + test automation
   - Others? Document or consolidate

2. **benchmarks/** - Performance measurement
   - `lib/` → Framework (build_manager.py, etc.)
   - `libdwarfs/` → API performance tests
   - `build_and_test_all.py` → Duplicate of scripts/build-all-and-test.sh?

**MECE Cleanup**:
- [ ] Remove duplicates (choose one location)
- [ ] Document purpose of each script/directory
- [ ] Create benchmarks/README.md
- [ ] Create scripts/README.md
- [ ] Ensure all work with 3 formats

**Recommendation**:
- `scripts/` = **Build automation** (build, test, release)
- `benchmarks/` = **Performance measurement** (timing, comparison)

---

## Phase 6: Integration Script Validation (45 min)

### Task 6.1: Test example/static-site-server (20 min)

**Commands**:
```bash
cd example/static-site-server

# Build
./build.sh
# Should use find_package(dwarfs) and link libraries

# Test
./test.sh
# Should start server, test access, shutdown
```

**Validation**:
- [ ] build.sh completes
- [ ] Finds dwarfs libraries
- [ ] Compiles successfully
- [ ] test.sh passes all checks
- [ ] Can load .dff images
- [ ] Can load .dth images

---

### Task 6.2: Test build-all-and-test.sh (25 min)

**Commands**:
```bash
# Clean build
rm -rf build/

# Run full automation
./scripts/build-all-and-test.sh --vcpkg

# Should:
# - Setup vcpkg
# - Build all libraries
# - Build all tools
# - Run all tests
# - Report results
```

**Validation**:
- [ ] Script completes without errors
- [ ] All tests pass
- [ ] Both build configs work (default + Thrift-enabled)
- [ ] Exit code 0

---

## Phase 7: Documentation & Release (45 min)

### Task 7.1: Move Remaining Temporary Docs (10 min)

**Files to Move** (to `doc/old-docs/sessions/`):
- [ ] Any SESSION_*_CONTINUATION_PLAN.md still in doc/
- [ ] Any SESSION_*_IMPLEMENTATION_STATUS.md still in doc/
- [ ] SESSION_95_COMPLETION_SUMMARY.md (keep Session 95 summary in main doc/)

**Files to Keep** (current work):
- SESSION_95_COMPLETION_SUMMARY.md (recent work)
- SESSION_95_IMPLEMENTATION_STATUS.md (recent work)
- SESSION_96_* files (this session)

---

### Task 7.2: Create Release Notes (20 min)

**Create**: `RELEASE_NOTES_v0.17.0.md`

```markdown
# DwarFS v0.17.0 Release Notes

## Overview
Major release adding Modern Thrift metadata format and comprehensive vcpkg integration.

## New Features

### Modern Thrift Metadata Format
- CompactProtocol serialization via fbthrift v2025.12.29.00
- Magic bytes: {0x82, 0x21}
- File extension: .dtc
- 0.07-1.41% smaller than FlatBuffers
- Test coverage: 15/15 tests PASSED

### Three Metadata Formats
- **FlatBuffers** (default): Best portability
- **Modern Thrift** (optional): Smallest size
- **Legacy Thrift** (always): v0.14.1 compatibility

### vcpkg Integration
- Complete overlay ports for Folly/fbthrift/jemalloc
- 20 standard triplets supported
- Static builds with all dependencies

## Build Configurations

**Default**: FlatBuffers + Legacy Thrift
**Thrift-Enabled**: FlatBuffers + Legacy Thrift + Modern Thrift

## Migration Guide
[Content here]

## Known Issues
[None currently]

## Acknowledgments
[Team credits]
```

---

### Task 7.3: Tag Release Candidate (15 min)

**Commands**:
```bash
# Final checks
git status
git log --oneline -10

# Tag RC1
git tag -a v0.17.0-rc1 -m "Release Candidate 1 for v0.17.0

Features:
- Modern Thrift metadata format (CompactProtocol)
- Legacy Thrift Homebrew v0.14.1 compatibility
- vcpkg overlay ports for Folly/fbthrift/jemalloc
- Comprehensive documentation (800+ lines)

Test Status: 15/15 Modern Thrift tests PASSED
Build Status: All integration complete
Documentation: Complete

Ready for cross-platform validation."

# Push
git push origin v0.17.0-rc1
```

**Validation**:
- [ ] Tag created
- [ ] CI/CD triggered
- [ ] All workflows queued

---

## Success Criteria

### Must Achieve
- [ ] Legacy Thrift Homebrew compatibility verified (read & write)
- [ ] All 3 formats work end-to-end
- [ ] Benchmark scripts functional
- [ ] Example app works
- [ ] v0.17.0-rc1 tagged

### Should Achieve
- [ ] Default build behavior correct (FlatBuffers ON by default)
- [ ] Scripts/benchmarks organized (MECE)
- [ ] Release notes complete

### Nice to Have
- [ ] Performance benchmarks re-run
- [ ] All CI/CD passes
- [ ] Community feedback

---

## Time Budget

**Phase 1: Legacy Thrift** (90 min)
- Task 1.1: Homebrew test suite (40 min)
- Task 1.2: All formats end-to-end (30 min)
- Task 1.3: Default build (20 min)

**Phase 2: Benchmarks** (60 min)
- Task 2.1: Test scripts (30 min)
- Task 2.2: MECE cleanup (30 min)

**Phase 3: Integration** (45 min)
- Task 3.1: Example app (20 min)
- Task 3.2: Build script (25 min)

**Phase 4: Release** (45 min)
- Task 4.1: Move docs (10 min)
- Task 4.2: Release notes (20 min)
- Task 4.3: Tag RC1 (15 min)

**Total**: ~4 hours

---

## Critical Requirements

1. **Homebrew Compatibility** - Top priority
   - Must read v0.14.1 images
   - Must write compatible images
   - Test with actual Homebrew binaries

2. **Three Format Support** - All tools
   - mkdwarfs creates all 3
   - dwarfsck reads all 3
   - dwarfsextract extracts all 3

3. **Benchmark System** - Functional
   - scripts/benchmark-all.sh works
   - Tests all available formats
   - Produces meaningful results

4. **Build Scripts** - All work
   - build-all-and-test.sh --vcpkg
   - example/static-site-server/build.sh
   - Default build (no explicit FB flag)

---

**Priority**: CRITICAL - v0.17.0 blocker
**Dependencies**: Session 95 ✅
**Blocks**: v0.17.0 final release