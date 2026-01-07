# DwarFS v0.16.0 Extended Release Plan

**Created**: 2025-12-08 12:52 HKT
**Status**: Validation Complete, Extended Work Required
**Target Release**: 2025-12-20 (extended from 2025-12-15)

---

## Executive Summary

Based on user feedback, the release scope has been extended to include critical
improvements that will make v0.16.0 a more robust and complete release:

1. ✅ **Quick Validation Complete** - All 3 builds functional
2. ✅ **CI/CD Triggered** - Running in background
3. 🔴 **NEW: Fix 13 Thrift-Conditional Tests** - Tests that are conditionally compiled
4. 🔴 **NEW: Refactor dwarfsck** - Apply modular architecture like mkdwarfs/dwarfs
5. 🔴 **NEW: Update GitHub Actions** - Adapt to multi-format setup
6. ⏳ **Large Image Validation** - Validate fix on real-world data
7. ⏳ **Comprehensive Benchmarks** - Performance validation
8. ⏳ **RC1 → Stable Release** - Final release workflow

**Extended Timeline**: +5 days (now ~12 days total)

---

## Phase A: Fix Thrift-Conditional Tests 🔴 NEW

### Status: Not Started
### Priority: HIGH (Blocking v0.16.0)
### ETA: 4-6 hours

### Problem Analysis

**Current Situation**:
- 13 tests skip when `DWARFS_HAVE_THRIFT` is not defined
- Tests are wrapped in `#ifdef DWARFS_HAVE_THRIFT` guards
- This violates our "format-agnostic" architecture principle
- Both FlatBuffers and Thrift should be tested equally

**Affected Test Files** (from grep analysis):
1. `test/backend_compatibility_test.cpp`
2. `test/block_merger_test.cpp` (Folly dependency)
3. `test/filesystem_test.cpp`
4. `test/global_metadata_test.cpp`
5. `test/metadata_factory_test.cpp`
6. `test/metadata_test.cpp`
7. `test/metadata_view_interface_test.cpp`
8. `test/tool_mkdwarfs_integration_test.cpp`
9. `test/metadata/format_conversion_test.cpp`
10. `test/metadata/serialization_test.cpp`
11. `test/metadata/serialization_benchmark_test.cpp`
12. `test/metadata/converters/round_trip_string_table_test.cpp`
13. `test/metadata/converters/thrift_metadata_converter_test.cpp`

### Solution Strategy

**Approach 1: Parameterized Tests (Recommended)**
Use Google Test's parameterized testing to run same tests with different formats:

```cpp
// Before (Thrift-only):
#ifdef DWARFS_HAVE_THRIFT
TEST(MetadataTest, SerializationRoundTrip) {
  // Test with Thrift
}
#endif

// After (Format-agnostic):
class MetadataFormatTest : public ::testing::TestWithParam<SerializationFormat> {};

TEST_P(MetadataFormatTest, SerializationRoundTrip) {
  auto format = GetParam();
  // Test with format (FlatBuffers OR Thrift)
}

INSTANTIATE_TEST_SUITE_P(
  AllFormats,
  MetadataFormatTest,
  ::testing::Values(
#ifdef DWARFS_HAVE_FLATBUFFERS
    SerializationFormat::FlatBuffers,
#endif
#ifdef DWARFS_HAVE_THRIFT
    SerializationFormat::Thrift
#endif
  )
);
```

**Approach 2: Conditional Skip (Fallback)**
For tests that genuinely require Thrift features:

```cpp
TEST(FollyDependentTest, BlockMerger) {
#ifndef DWARFS_HAVE_THRIFT
  GTEST_SKIP() << "Requires Folly (DWARFS_HAVE_THRIFT)";
#else
  // Folly-specific test
#endif
}
```

### Implementation Tasks

#### A.1 Backend Compatibility Tests
- [ ] Convert `test/backend_compatibility_test.cpp` to parameterized
- [ ] Test both FlatBuffers and Thrift backends
- [ ] Ensure format detection works correctly

#### A.2 Filesystem Tests
- [ ] Convert `test/filesystem_test.cpp` to parameterized
- [ ] Test mounting with both formats
- [ ] Verify read operations work identically

#### A.3 Metadata Tests
- [ ] Convert `test/metadata_test.cpp` to parameterized
- [ ] Test serialization/deserialization for both formats
- [ ] Verify round-trip correctness

#### A.4 Factory Tests
- [ ] Convert `test/metadata_factory_test.cpp` to parameterized
- [ ] Test factory creates correct backend for each format
- [ ] Verify format detection logic

#### A.5 Integration Tests
- [ ] Convert `test/tool_mkdwarfs_integration_test.cpp` to parameterized
- [ ] Test mkdwarfs with both formats
- [ ] Verify output images are valid

#### A.6 Converter Tests
- [ ] Keep Thrift-specific converter tests conditional
- [ ] Add FlatBuffers-specific converter tests
- [ ] Add cross-format compatibility tests (if applicable)

#### A.7 Block Merger Tests
- [ ] Keep Folly-dependent tests conditional (requires DWARFS_HAVE_THRIFT)
- [ ] Add note in test explaining Folly dependency
- [ ] Consider alternative implementation without Folly (future work)

### Acceptance Criteria
- ✅ All tests run when `DWARFS_HAVE_FLATBUFFERS=ON`
- ✅ All tests run when `DWARFS_HAVE_THRIFT=ON`
- ✅ Tests work when both formats enabled
- ✅ Test pass rate remains 99%+
- ✅ No test compiles conditionally unless genuinely format-specific

---

## Phase B: Refactor dwarfsck 🔴 NEW

### Status: Not Started
### Priority: MEDIUM (Nice-to-have for v0.16.0)
### ETA: 6-8 hours

### Rationale

**Why This Matters**:
1. **Consistency**: mkdwarfs and dwarfs are now modular, dwarfsck should be too
2. **Reusability**: Checker logic should be in libdwarfs, not tool
3. **Testability**: Library components can be unit-tested independently
4. **Maintainability**: Smaller, focused modules easier to maintain

**Current State** (`tools/src/dwarfsck_main.cpp`):
- Monolithic main file with checker logic embedded
- Direct filesystem access without library abstraction
- Hard to test individual checker components

**Target State**:
- `include/dwarfs/utility/filesystem_checker.h` - Library interface
- `src/utility/filesystem_checker.cpp` - Implementation
- `tools/src/dwarfsck/` - Modular tool components
- Clean separation: library logic vs CLI presentation

### Architecture Design

```
┌────────────────────────────────────────────────┐
│         dwarfsck_main.cpp (200 lines)          │
│  ┌──────────────┐                              │
│  │options_parser│ → parsed_options              │
│  └──────────────┘                              │
│         │                                       │
│         ▼                                       │
│  ┌──────────────┐                              │
│  │check_handler │ → creates library checker    │
│  └──────────────┘                              │
└────────────────────────────────────────────────┘
                    │
         ┌──────────┴──────────┐
         ▼                     ▼
┌──────────────────┐    ┌─────────────────┐
│filesystem_checker│    │ integrity_check │
│   (library)      │    │   (library)     │
│                  │    │                 │
│ - load_image     │    │ - verify_blocks │
│ - check_metadata │    │ - check_chunks  │
│ - verify_blocks  │    │ - validate_hash │
└──────────────────┘    └─────────────────┘
```

### Implementation Tasks

#### B.1 Library Interface Design
- [ ] Create `include/dwarfs/utility/filesystem_checker.h`
- [ ] Define `filesystem_checker` class with check operations
- [ ] Define `check_options` struct for configuration
- [ ] Define `check_result` for output

#### B.2 Library Implementation
- [ ] Create `src/utility/filesystem_checker.cpp`
- [ ] Implement metadata validation
- [ ] Implement block integrity verification
- [ ] Implement chunk consistency checks
- [ ] Implement filesystem structure validation

#### B.3 Tool Modularization
- [ ] Create `tools/include/dwarfs/tool/dwarfsck/options_parser.h`
- [ ] Create `tools/src/dwarfsck/options_parser.cpp`
- [ ] Create `tools/include/dwarfs/tool/dwarfsck/check_handler.h`
- [ ] Create `tools/src/dwarfsck/check_handler.cpp`
- [ ] Refactor `dwarfsck_main.cpp` to minimal orchestration

#### B.4 Testing
- [ ] Write unit tests for `filesystem_checker`
- [ ] Write integration tests for full check workflow
- [ ] Test with corrupted images
- [ ] Test with various check levels

### Acceptance Criteria
- ✅ `dwarfsck_main.cpp` reduced to <300 lines
- ✅ Library components thoroughly tested
- ✅ All existing dwarfsck functionality preserved
- ✅ Performance not degraded
- ✅ New library API documented

---

## Phase C: Update GitHub Actions 🔴 NEW

### Status: Not Started
### Priority: HIGH (Blocking CI/CD validation)
### ETA: 3-4 hours

### Problem Analysis

**Current Situation**:
- GitHub Actions assumes Thrift is always available
- No testing of format-specific builds (fb-only, thrift-only)
- No validation of cross-format compatibility
- Missing test for new tool architecture

**What Needs Updating**:
1. Build matrix to test all 3 configurations
2. Test jobs to handle format-specific tests
3. Artifact naming for different formats
4. Documentation generation

### Implementation Tasks

#### C.1 Build Matrix Extension
File: `.github/workflows/build.yml`

Add format configuration dimension:
```yaml
strategy:
  matrix:
    format-config:
      - name: flatbuffers-only
        cmake_args: -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
      - name: thrift-only
        cmake_args: -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
      - name: both-formats
        cmake_args: -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
```

#### C.2 Platform-Specific Configuration
- [ ] Linux (Ubuntu 22.04): All 3 formats
- [ ] Linux (Ubuntu 24.04): All 3 formats
- [ ] macOS (latest): flatbuffers-only, both-formats
- [ ] Windows (latest): flatbuffers-only, both-formats
- [ ] Cross-arch (qemu): flatbuffers-only

#### C.3 Test Job Updates
- [ ] Run full test suite for each format config
- [ ] Report test status by format
- [ ] Upload test results as artifacts
- [ ] Generate coverage reports per format

#### C.4 Artifact Updates
- [ ] Name artifacts with format suffix (e.g., `dwarfs-linux-fb.tar.gz`)
- [ ] Generate checksums for each artifact
- [ ] Create release notes with format compatibility matrix

#### C.5 Documentation Jobs
- [ ] Update doc generation to mention format support
- [ ] Add format configuration examples
- [ ] Document which platforms support which formats

### Acceptance Criteria
- ✅ CI tests all 3 format configurations
- ✅ Format-specific failures clearly reported
- ✅ Artifacts properly labeled
- ✅ Documentation reflects multi-format support
- ✅ No regression in build time

---

## Phase D: Large Image Validation ⏳ CONTINUED

### Status: Partially Complete (small images validated)
### Priority: MEDIUM
### ETA: 2-3 hours

### Tasks

#### D.1 Source Data Preparation
- [ ] Identify large source directory (>1 GB, ideally >10 GB)
- [ ] Options:
  - Use local development source tree
  - Download public dataset (e.g., Linux kernel sources)
  - Use existing benchmark datasets

#### D.2 Image Creation
- [ ] Create large FlatBuffers image with fb-only build
- [ ] Create large Thrift image with thrift-only build
- [ ] Record creation metrics (time, memory, ratio)

#### D.3 Validation Tests
- [ ] Integrity check with `dwarfsck --check-integrity`
- [ ] Full extraction with `dwarfsextract`
- [ ] Content verification with `diff -r`
- [ ] FUSE mount and random access tests
- [ ] Performance profiling

#### D.4 Documentation
- [ ] Document validation results
- [ ] Record any issues found
- [ ] Update validation report

### Commands

```bash
# Prepare large source
mkdir -p /tmp/large-source
# Option 1: Use dwarfs source
cp -r /Users/mulgogi/src/external/dwarfs /tmp/large-source/dwarfs-src

# Create images
./build-fb-bench/mkdwarfs -i /tmp/large-source -o /tmp/large-fb.dff \
  --log-level=info --check-after-write

./build-thrift-bench/mkdwarfs -i /tmp/large-source -o /tmp/large-thrift.dft \
  --format thrift --log-level=info --check-after-write

# Validate
./build-fb-bench/dwarfsck /tmp/large-fb.dff --check-integrity --verbose
./build-thrift-bench/dwarfsck /tmp/large-thrift.dft --check-integrity --verbose

# Extract and verify
./build-fb-bench/dwarfsextract -i /tmp/large-fb.dff -o /tmp/extract-fb
./build-thrift-bench/dwarfsextract -i /tmp/large-thrift.dft -o /tmp/extract-thrift
diff -r /tmp/large-source /tmp/extract-fb
diff -r /tmp/large-source /tmp/extract-thrift
```

### Acceptance Criteria
- ✅ Images >1 GB created successfully
- ✅ All integrity checks pass
- ✅ Full extraction completes without errors
- ✅ Content matches exactly
- ✅ Performance acceptable

---

## Phase E: Comprehensive Benchmarks ⏳ CONTINUED

### Status: Infrastructure Ready, Execution Pending
### Priority: MEDIUM
### ETA: 2-4 hours

[Previously documented in V0_16_0_RELEASE_CONTINUATION_PLAN.md]

---

## Phase F: CI/CD Validation & Release ⏳ CONTINUED

[Previously documented in V0_16_0_RELEASE_CONTINUATION_PLAN.md]

---

## Updated Timeline

| Phase | Task | Original ETA | New ETA | Status |
|-------|------|--------------|---------|--------|
| 0 | Critical Fixes | - | ✅ 2025-12-07 | COMPLETE |
| 1 | Documentation | - | ✅ 2025-12-08 | COMPLETE |
| 2 | Quick Validation | - | ✅ 2025-12-08 | COMPLETE |
| **A** | **Fix Thrift Tests** | **NEW** | **2025-12-09** | **PENDING** |
| **B** | **Refactor dwarfsck** | **NEW** | **2025-12-10** | **PENDING** |
| **C** | **Update GHA** | **NEW** | **2025-12-10** | **PENDING** |
| D | Large Validation | 2025-12-09 | 2025-12-11 | PENDING |
| E | Benchmarks | 2025-12-09-10 | 2025-12-11-12 | PENDING |
| F | CI/CD Review | 2025-12-10 | 2025-12-12 | PENDING |
| G | RC1 Tag | 2025-12-10 | 2025-12-13 | PENDING |
| H | Platform Testing | 2025-12-11-13 | 2025-12-14-16 | PENDING |
| I | **Stable Release** | ~~2025-12-15~~ | **2025-12-20** | **TARGET** |

**Schedule Impact**: +5 days (quality improvements worth the delay)

---

## Priority Order for Next Session

### Immediate (Start Now)
1. **Phase A**: Fix Thrift-conditional tests
   - Highest impact on test coverage
   - Unblocks format-agnostic testing
   - Required for CI/CD validation

### Short Term (1-2 days)
2. **Phase C**: Update GitHub Actions
   - Required for proper CI/CD validation
   - Tests new multi-format architecture
   - Validates all platforms

3. **Phase B**: Refactor dwarfsck
   - Completes tool modularization
   - Improves code quality
   - Nice-to-have for v0.16.0

### Medium Term (3-4 days)
4. **Phase D**: Large image validation
5. **Phase E**: Comprehensive benchmarks
6. **Phase F**: CI/CD review and RC1

---

## Success Criteria Updates

### For v0.16.0 Release

**Minimum Requirements** (was: 4 items, now: 7 items):
- [x] FlatBuffers fix validated
- [x] Documentation updated
- [x] Quick validation passing
- [ ] **NEW**: 100% test pass rate (fix 13 conditional tests)
- [ ] **NEW**: All tools modular (mkdwarfs ✅, dwarfs ✅, dwarfsck ⏳)
- [ ] **NEW**: GHA validates all format configurations
- [ ] CI/CD passing on all platforms

**Recommended** (unchanged):
- [ ] Large image validation
- [ ] Comprehensive benchmarks
- [ ] Platform testing complete

### Rationale for Extended Scope

1. **Quality Over Speed**: Fix fundamental issues now vs. technical debt later
2. **Architecture Consistency**: All tools should follow same patterns
3. **Test Coverage**: 100% pass rate shows polish and completeness
4. **CI/CD Robustness**: Properly validate multi-format architecture

---

## Files to Create/Update

### New Documentation
- [x] `doc/V0_16_0_EXTENDED_CONTINUATION_PLAN.md` - This file
- [ ] `doc/V0_16_0_IMPLEMENTATION_STATUS.md` - Detailed progress tracker
- [ ] `doc/DWARFSCK_REFACTORING_PLAN.md` - Detailed dwarfsck design
- [ ] `doc/TEST_COVERAGE_IMPROVEMENT_PLAN.md` - Test fixing strategy

### Update Existing
- [ ] `doc/V0_16_0_RELEASE_STATUS.md` - Update timeline and metrics
- [ ] `.github/workflows/build.yml` - Multi-format testing
- [ ] `README.md` - Update timeline if needed

### Archive Old Docs
- [ ] Move Phase H/I/K completion docs to `doc/old-docs/v0.15-work/`
- [ ] Move benchmark planning docs to `doc/old-docs/benchmarking/`

---

**Last Updated**: 2025-12-08 12:52 HKT
**Next Review**: After Phase A completion
**Owner**: Release Engineering Team
**Target**: v0.16.0 stable by 2025-12-20
// ... existing code ...