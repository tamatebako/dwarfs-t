# Phase 5: Detailed Execution Plan
## Complete Cereal/Bitsery Cleanup + FlatBuffers Benchmark Suite

**Date**: 2025-11-16  
**Branch**: feature/multi-format-serialization-fuse  
**Goal**: Remove all 777 Cereal/Bitsery references and update benchmark suite for FlatBuffers vs Thrift comparison

---

## Executive Summary

### Current State
- ✅ FlatBuffers integrated (Phase 4 complete)
- ✅ Jemalloc improved (Phase 4 complete)
- ✅ CLI updated (Phase 4 complete)
- ✅ CI/CD running validation
- ❌ 777 Cereal/Bitsery references remain (development-only, never released)
- ⚠️ Benchmark suite 95% ready (needs 2 minor updates)

### Success Criteria
1. **Part 1**: 0 Cereal/Bitsery references in code/tests/CMake (except git history)
2. **Part 2**: Benchmark suite updated for FlatBuffers vs Thrift
3. **Part 3**: Comprehensive benchmark report generated
4. **Final**: Clean codebase with 2 formats only (FlatBuffers default, Thrift legacy)

---

## PART 1: Cereal/Bitsery Complete Cleanup (777 refs → 0)

### 1.1 Delete WIP Documentation Files (~290 refs) ✅ SAFE

**Action**: Delete entire files (no code dependencies)

**Files to Delete**:
```bash
# Priority 1: Planning/WIP docs in doc/
doc/CEREAL_BITSERY_REMOVAL_PLAN.md                    (58 refs)
doc/FLATBUFFERS_FINAL_CEREAL_BITSERY_REMOVED.md       (55 refs)
doc/FLATBUFFERS_CONTINUATION_NEXT_SESSION.md          (41 refs)
doc/THRIFT_OPTIONAL_REFACTORING_PLAN.md               (33 refs)
doc/FLATBUFFERS_WORK_COMPLETE.md                      (25 refs)
doc/FLATBUFFERS_NEXT_PHASE.md                         (24 refs)
doc/FLATBUFFERS_NEXT_STEPS.md
doc/FLATBUFFERS_CONTINUATION.md
doc/FLATBUFFERS_NATIVE_CONTINUATION.md
doc/FLATBUFFERS_PROGRESS.md
doc/FLATBUFFERS_IMPLEMENTATION_COMPLETE.md
doc/FLATBUFFERS_STATUS_2025-11-15_FINAL.md
doc/FLATBUFFERS_BENCHMARK_SETUP.md

# Priority 2: Session continuation docs
doc/SESSION_CONTINUATION_2025-11-14.md
doc/SESSION_CONTINUATION_2025-11-15.md
doc/SESSION_CONTINUATION_2025-11-15_FINAL.md
doc/SESSION_HANDOVER_CONTINUATION.md
doc/PROMPT_FOR_THRIFT_OPTIONAL_REFACTORING.md

# Priority 3: Build planning docs
doc/BUILD_YML_EXTENSION_PLAN.md
doc/PHASE_2_COMPLETE_THRIFT_FOLLY_OPTIONAL.md
doc/PHASE_3_METADATA_ACCESS_LAYER.md
doc/PHASE_4_COMPLETE_FLATBUFFERS_JEMALLOC.md

# Priority 4: Current phase docs (keep PHASE_5_CLEANUP_AND_BENCHMARK.txt)
doc/CEREAL_BITSERY_CLEANUP_PLAN.md
doc/CEREAL_BITSERY_CLEANUP_PROMPT.txt
doc/PHASE_5_CONTINUATION_PROMPT.txt
doc/PHASE_5_SMOKE_TEST_AND_CLEANUP.md

# Priority 5: Design docs with outdated format info
doc/METADATA_STRUCTURES_DESIGN.md                     (if has Cereal/Bitsery)
doc/METADATA_VIEW_INTERFACES.md                       (if has Cereal/Bitsery)

# Priority 6: CI/CD planning
.github/CI-UNIFIED-WORKFLOW.md                        (19 refs)

# Priority 7: Benchmark results
benchmarks/results/test_report.md                      (20 refs)
```

**Rationale**: These are all work-in-progress planning documents, never part of released documentation. Safe to delete entirely.

**Estimated Impact**: ~290 references removed

---

### 1.2 Update Test Files (~223 refs) ⚠️ CAREFUL

**Strategy**: Remove Cereal/Bitsery test cases, keep only FlatBuffers + Thrift tests

#### File 1: `test/metadata/format_conversion_test.cpp` (67 refs)

**Current Structure** (assumed):
- Tests for Thrift → Cereal conversion
- Tests for Thrift → Bitsery conversion
- Tests for Cereal → Thrift conversion
- Tests for Bitsery → Thrift conversion
- Tests for Cereal → Bitsery conversion

**Target Structure**:
- Tests for Thrift → FlatBuffers conversion
- Tests for FlatBuffers → Thrift conversion

**Action Required**:
1. Read file to identify test cases
2. Remove all Cereal/Bitsery test cases
3. Verify FlatBuffers test cases exist
4. Update test expectations

#### File 2: `test/metadata/serialization_test.cpp` (60 refs)

**Expected Tests**:
- Serialization round-trips for each format
- Format detection tests
- Compatibility tests

**Action Required**:
1. Remove Cereal/Bitsery round-trip tests
2. Remove Cereal/Bitsery format detection tests
3. Keep FlatBuffers + Thrift tests

#### File 3: `test/metadata/serialization_benchmark_test.cpp` (32 refs)

**Expected Tests**:
- Performance benchmarks for each format
- Size comparisons
- Speed comparisons

**Action Required**:
1. Remove Cereal/Bitsery benchmark tests
2. Keep FlatBuffers + Thrift benchmarks

#### File 4: `test/metadata/serialization/serialization_facade_test.cpp` (30 refs)

**Expected Tests**:
- Facade API tests for each format
- Registry tests

**Action Required**:
1. Remove Cereal/Bitsery facade tests
2. Keep FlatBuffers + Thrift facade tests

#### File 5: `test/tool_mkdwarfs_format_conversion_test.cpp` (33 refs)

**Expected Tests**:
- CLI tests for format conversion
- `--metadata-format` option tests

**Action Required**:
1. Remove Cereal/Bitsery CLI tests
2. Update valid format list to ['thrift', 'flatbuffers']

**Verification After Updates**:
```bash
# Should compile successfully
cmake --build build --target dwarfs_unit_tests

# Should pass all tests
ctest --test-dir build --tests-regex "metadata.*serial" --output-on-failure
```

**Estimated Impact**: ~223 references removed

---

### 1.3 Clean CMake Configuration (~20 refs)

#### File 1: `cmake/metadata_serialization.cmake`

**Current Defines** (assumed):
```cmake
add_compile_definitions(DWARFS_HAVE_THRIFT=1)       # Keep
add_compile_definitions(DWARFS_HAVE_FLATBUFFERS=1)  # Keep
add_compile_definitions(DWARFS_HAVE_CEREAL=1)       # REMOVE
add_compile_definitions(DWARFS_HAVE_BITSERY=1)      # REMOVE
```

**Action Required**:
1. Remove CEREAL/BITSERY compile definitions
2. Remove cereal/bitsery dependency checks
3. Keep FlatBuffers (required) + Thrift (optional)

#### File 2: `cmake/libdwarfs.cmake`

**Current Linking** (assumed):
```cmake
target_link_libraries(dwarfs_common
  PUBLIC
    cereal::cereal     # REMOVE
    bitsery            # REMOVE
```

**Action Required**:
1. Remove cereal::cereal linkage
2. Remove bitsery linkage
3. Keep FlatBuffers + Thrift linkage (conditional on DWARFS_WITH_THRIFT)

#### File 3: `CMakeLists.txt`

**Current Test Linking** (assumed):
```cmake
target_link_libraries(dwarfs_unit_tests
  cereal::cereal     # REMOVE
  bitsery            # REMOVE
```

**Action Required**:
1. Remove test linking for cereal/bitsery
2. Keep FlatBuffers + Thrift test linkage

**Verification**:
```bash
# Clean build
rm -rf build
cmake -B build -GNinja -DWITH_TESTS=ON

# Should succeed with only FlatBuffers + Thrift
ninja -C build
```

**Estimated Impact**: ~20 references removed

---

### 1.4 Update Source Code (~28 refs)

#### File: `src/history.cpp` (28 refs)

**Investigation Needed**: Check if these are:
- Old serialized history strings (likely, historical data)
- Active code paths (unlikely)

**Action Options**:
1. **If historical strings**: Leave as-is (just old data)
2. **If active code**: Update to remove format references
3. **If test fixtures**: Update test data

**Verification**:
```bash
grep -n "cereal\|bitsery" src/history.cpp
# Read context to determine nature of references
```

**Estimated Impact**: 0-28 references (depending on investigation)

---

### 1.5 Update Memory Bank Files (~15 refs)

#### File 1: `.kilocode/rules/memory-bank/tech.md`

**Sections to Update**:
```markdown
## Serialization Technologies

**Metadata Serialization** (2 formats supported):  # Update from 3→2
1. **FlatBuffers** (modern default):               # NEW
   - ...
2. **Apache Thrift Compact** (legacy):
   - ...

# DELETE entirely:
2. **Cereal Binary** (modern default):
3. **Bitsery** (performance):
```

**Action Required**:
1. Update format count: 3 → 2
2. Update default: Cereal → FlatBuffers
3. Remove Cereal/Bitsery sections
4. Update examples to use `--metadata-format=flatbuffers`

#### File 2: `.kilocode/rules/memory-bank/context.md`

**Section: Recent Major Changes**:
```markdown
### FlatBuffers as Modern Default ✅ COMPLETE (November 2025)

**Removed Formats** (November 2025):
- Cereal Binary (backed up, no longer supported)    # Keep this line for history
- Bitsery (backed up, no longer supported)          # Keep this line for history
```

**Action Required**:
1. Update "Current Work Focus" section
2. Update "Next Steps" to reflect completion
3. Keep historical note about removed formats

#### File 3: `.kilocode/rules/memory-bank/architecture.md`

**Sections to Update**:
- Metadata Serialization Architecture (lines ~200-250)
- Format Selection Rationale table
- Critical Code Locations (serializer_registry)

**Action Required**:
1. Update 3-format design → 2-format design
2. Update format comparison table
3. Remove Cereal/Bitsery implementation details

**Estimated Impact**: ~15 references removed

---

### 1.6 Update User-Facing Documentation

#### File 1: `doc/dwarfs-format.md`

**Section: Metadata Serialization Formats**:

**Current** (assumed):
```markdown
DwarFS supports three metadata serialization formats:
- Thrift Compact (legacy)
- Cereal Binary (default)
- Bitsery (performance)
```

**Target**:
```markdown
DwarFS supports two metadata serialization formats:
- FlatBuffers (modern default) - Memory-mappable, zero-copy, excellent portability
- Thrift Compact (legacy) - Optional, for backward compatibility only
```

**Action Required**:
1. Update format descriptions
2. Update `--metadata-format` option documentation
3. Update examples

#### File 2: `doc/mkdwarfs.md`

**Section: --metadata-format Option**:

**Current** (assumed):
```
--metadata-format=FORMAT
    Choose metadata format: thrift, cereal, bitsery
    Default: cereal
```

**Target**:
```
--metadata-format=FORMAT
    Choose metadata format: thrift, flatbuffers
    Default: flatbuffers
```

**Action Required**:
1. Update valid format list
2. Update default format
3. Update examples

#### File 3: `README.md` (if applicable)

**Action Required**:
1. Check for Cereal/Bitsery mentions
2. Update to FlatBuffers + Thrift only

---

### 1.7 Verification Steps

**Step 1: Reference Count**:
```bash
# Should return 0 or very minimal (only in git history/changelogs)
grep -r "cereal\|bitsery" \
  --include="*.cpp" \
  --include="*.h" \
  --include="*.cmake" \
  --include="*.md" \
  --exclude-dir=build* \
  --exclude-dir=.git \
  . | wc -l
```

**Step 2: Build Verification**:
```bash
# Clean build
rm -rf build
cmake -B build -GNinja \
  -DWITH_TESTS=ON \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=ON \
  -DCMAKE_BUILD_TYPE=Release

ninja -C build

# Should succeed with only FlatBuffers + Thrift
```

**Step 3: Test Verification**:
```bash
# All tests should pass
ctest --test-dir build -j

# Metadata tests specifically
ctest --test-dir build --tests-regex "metadata" --output-on-failure
```

**Step 4: Format Detection**:
```bash
# Create test images
./build/mkdwarfs -i /tmp/test-source -o /tmp/test-fb.dwarfs --metadata-format=flatbuffers
./build/mkdwarfs -i /tmp/test-source -o /tmp/test-thrift.dwarfs --metadata-format=thrift

# Verify format detection
./build/dwarfsck /tmp/test-fb.dwarfs --json | jq '.metadata.serialization_format'
# Expected: "flatbuffers"

./build/dwarfsck /tmp/test-thrift.dwarfs --json | jq '.metadata.serialization_format'
# Expected: "thrift"
```

---

### 1.8 Commit Strategy

**Commit After Part 1 Completion**:
```bash
git add -A
git commit -m "chore: remove development-only format references

- Delete WIP documentation files (~290 refs)
- Remove Cereal/Bitsery test cases from test suite (~223 refs)
- Clean CMake configuration (~20 refs)
- Update memory bank files (~15 refs)
- Update user-facing documentation (doc/*.md)

Note: Cereal and Bitsery were development-only formats never
released to users. All functionality now provided by:
- FlatBuffers (modern default, required)
- Thrift (legacy, optional for backward compatibility)

Total: 777 references → 0 (or minimal in changelogs/history)"
```

---

## PART 2: Benchmark Suite Update (95% Complete!)

### Assessment

**Good News**: The benchmark infrastructure is already configured for FlatBuffers!

**Files Already Correct**:
- ✅ `benchmarks/run_metadata_format_benchmark.py` (line 117: `FORMATS = ['thrift', 'flatbuffers']`)
- ✅ `benchmarks/generate_metadata_report.py` (thin wrapper, no format references)
- ✅ All library files in `benchmarks/lib/` (format-agnostic)

**Files Needing Updates**:
- ❌ `benchmarks/lib/report_generator.py` (line 75)
- ❌ `benchmarks/README.md` (descriptions)

---

### 2.1 Update `benchmarks/lib/report_generator.py`

**File**: `benchmarks/lib/report_generator.py`  
**Line**: 75  
**Current**:
```python
FORMATS = ['thrift', 'cereal', 'bitsery']
```

**Target**:
```python
FORMATS = ['thrift', 'flatbuffers']
```

**Impact**:
- Report generator will recognize FlatBuffers format
- Comparison tables will show Thrift vs FlatBuffers
- Executive summary will compare correct formats

**Verification**:
```bash
# After update, test report generation with sample data
python3 benchmarks/lib/report_generator.py \
  sample_results.json \
  test_report.md

# Check output for correct format names
grep -i "cereal\|bitsery" test_report.md
# Should return 0 results
```

---

### 2.2 Update `benchmarks/README.md`

**Current** (line 28-29):
```markdown
The DwarFS benchmark suite provides tools for measuring filesystem performance
across different metadata serialization formats (Thrift, Cereal, Bitsery).
```

**Target**:
```markdown
The DwarFS benchmark suite provides tools for measuring filesystem performance
across metadata serialization formats (FlatBuffers and Thrift).
```

**Additional Updates**:
- Update any examples showing format selection
- Update descriptions to use FlatBuffers instead of Cereal/Bitsery

---

### 2.3 Verification

**Step 1: Code Consistency**:
```bash
# Verify all benchmark files use correct formats
grep -r "cereal\|bitsery" benchmarks/ \
  --include="*.py" \
  --include="*.md"
# Should return 0 results
```

**Step 2: Syntax Check**:
```bash
# Python syntax check
python3 -m py_compile benchmarks/run_metadata_format_benchmark.py
python3 -m py_compile benchmarks/lib/report_generator.py

# Should succeed with no errors
```

**Step 3: Dry Run** (optional, if test data available):
```bash
# Create minimal test setup
mkdwarfs -i /tmp/small-test -o /tmp/test.dwarfs --metadata-format=flatbuffers

# Run benchmark script (will fail without full setup, but tests arg parsing)
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./build/mkdwarfs \
  --dwarfsextract ./build/dwarfsextract \
  --dwarfs ./build/dwarfs \
  --perl-dataset /tmp/small-test \
  --output /tmp/bench-results.json \
  --runs 1
```

---

### 2.4 Commit Strategy

**Commit After Part 2 Completion**:
```bash
git add benchmarks/
git commit -m "feat(benchmarks): update suite for FlatBuffers vs Thrift

- Update report_generator.py format list (line 75)
- Update README.md descriptions to reflect current formats
- Remove all Cereal/Bitsery references from benchmark suite

The benchmark suite now tests the two supported formats:
- FlatBuffers (modern default)
- Thrift (legacy)

This prepares for comprehensive performance comparison between
the two formats across multiple dimensions:
- Compression (time, size, memory)
- Extraction (all files, single file)
- FUSE operations (mount, read, latency)"
```

---

## PART 3: Run Comprehensive Benchmarks

### Prerequisites

**Requirements**:
1. ✅ CI/CD successful build OR local build working
2. ✅ Working binaries with BOTH formats (FlatBuffers + Thrift)
3. ✅ Benchmark suite updated (Part 2 complete)
4. ✅ Test dataset downloaded (Perl 5.43.3)

**Platform Recommendation**:
- Linux x86_64 (most stable, best performance)
- macOS x86_64 (if Linux unavailable)
- Minimum 8 GB free disk space
- Minimum 8 GB RAM

---

### 3.1 Setup

**Step 1: Download Test Dataset**:
```bash
# Download Perl 5.43.3 source code
python3 benchmarks/download_datasets.py --download perl

# Verify download
ls -lh benchmark-files/perl-5.43.3
# Expected: ~6,802 files, ~95 MB
```

**Step 2: Prepare Work Directory**:
```bash
# Create work directory
mkdir -p /tmp/metadata_bench

# Clean any previous runs
rm -rf /tmp/metadata_bench/*
```

**Step 3: Verify Tools**:
```bash
# Check tool versions
./build/mkdwarfs --help | grep "mkdwarfs (v"
./build/dwarfsck --help | grep "dwarfsck (v"
./build/dwarfsextract --help | grep "dwarfsextract (v"
./build/dwarfs --help | grep "dwarfs (v"

# All should show v0.16.0 or similar
```

---

### 3.2 Run Benchmarks

**Full Benchmark Run** (est. 30-60 minutes):
```bash
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./build/mkdwarfs \
  --dwarfsextract ./build/dwarfsextract \
  --dwarfs ./build/dwarfs \
  --perl-dataset benchmark-files/perl-5.43.3 \
  --output benchmarks/results/flatbuffers-vs-thrift-$(date +%Y%m%d).json \
  --work-dir /tmp/metadata_bench \
  --runs 5

# 5 runs: 1 warm-up + 4 measured runs
# Dataset: Perl 5.43.3 (~95 MB, 6,802 files)
# Formats: FlatBuffers, Thrift
# Operations: Compress, extract, FUSE mount/read
```

**Quick Benchmark Run** (est. 10 minutes):
```bash
# Faster run with fewer iterations
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./build/mkdwarfs \
  --dwarfsextract ./build/dwarfsextract \
  --dwarfs ./build/dwarfs \
  --perl-dataset benchmark-files/perl-5.43.3 \
  --output benchmarks/results/flatbuffers-vs-thrift-quick.json \
  --work-dir /tmp/metadata_bench \
  --runs 3

# 3 runs: 1 warm-up + 2 measured runs
```

---

### 3.3 Generate Report

**Step 1: Generate Markdown Report**:
```bash
python3 benchmarks/generate_metadata_report.py \
  benchmarks/results/flatbuffers-vs-thrift-$(date +%Y%m%d).json \
  doc/benchmark-flatbuffers-vs-thrift.md
```

**Step 2: Review Report**:
```bash
# Check report structure
head -50 doc/benchmark-flatbuffers-vs-thrift.md

# Verify format names
grep -i "cereal\|bitsery" doc/benchmark-flatbuffers-vs-thrift.md
# Should return 0 results

# Check for FlatBuffers/Thrift
grep -i "flatbuffers\|thrift" doc/benchmark-flatbuffers-vs-thrift.md | head -20
```

**Step 3: Analyze Results**:

**Expected Findings**:

| Metric | FlatBuffers | Thrift | Winner |
|--------|-------------|--------|--------|
| Archive Size | ~5-10% larger | Smaller (bit-packed) | Thrift |
| Compression Time | Similar | Similar | ~Tie~ |
| Mount Time | Faster (zero-copy) | Slightly slower | FlatBufffers |
| Read Throughput | Faster (zero-copy) | Good | FlatBuffers |
| Memory Usage | Similar | Similar | ~Tie~ |
| Build Portability | Excellent | Limited | FlatBuffers |

**Key Insights**:
- FlatBuffers: Better portability, faster runtime, slightly larger
- Thrift: Better compression, complex dependencies, legacy

---

### 3.4 Commit Results

**Commit After Part 3 Completion**:
```bash
git add benchmarks/results/
git add doc/benchmark-flatbuffers-vs-thrift.md

git commit -m "perf(benchmarks): FlatBuffers vs Thrift comparison results

Comprehensive performance comparison using Perl 5.43.3 dataset (95MB, 6,802 files).

Key Findings:
- FlatBuffers: ~5-10% larger archives, faster mounts, better portability
- Thrift: Slightly smaller archives, good performance, complex dependencies

Measured Metrics:
- Compression: time, size, memory
- Extraction: throughput, latency
- FUSE: mount time, read performance, operation latency

Recommendation: FlatBuffers as default (excellent portability + performance),
Thrift as legacy (backward compatibility).

Results: doc/benchmark-flatbuffers-vs-thrift.md
Raw data: benchmarks/results/flatbuffers-vs-thrift-*.json"
```

---

## FINAL VERIFICATION

### V.1 Zero Cereal/Bitsery References

**Test**:
```bash
# Code/Tests/CMake
grep -r "cereal\|bitsery" \
  --include="*.cpp" \
  --include="*.h" \
  --include="*.cmake" \
  --exclude-dir=build* \
  --exclude-dir=.git \
  . | wc -l

# Expected: 0

# Documentation (excluding git history)
grep -r "cereal\|bitsery" \
  --include="*.md" \
  --exclude="CHANGES.md" \
  --exclude-dir=.git \
  . | wc -l

# Expected: 0 (or only historical mentions in CHANGES.md)
```

---

### V.2 Benchmark Suite Functional

**Test**:
```bash
# Python syntax
python3 -m py_compile benchmarks/run_metadata_format_benchmark.py
python3 -m py_compile benchmarks/lib/report_generator.py

# Format list check
grep "FORMATS = " benchmarks/run_metadata_format_benchmark.py
# Expected: FORMATS = ['thrift', 'flatbuffers']

grep "FORMATS = " benchmarks/lib/report_generator.py
# Expected: FORMATS = ['thrift', 'flatbuffers']
```

---

### V.3 CI/CD Status

**Check**:
```bash
# Check CI/CD status via GitHub CLI (if available)
gh run list --branch feature/multi-format-serialization-fuse --limit 1

# Or check manually at:
# https://github.com/tamatebako/dwarfs/actions
```

**Expected**: All CI/CD jobs passing (or only pre-existing failures)

---

### V.4 Final Review Checklist

- [ ] Part 1: All 777 Cereal/Bitsery references removed
- [ ] Part 2: Benchmark suite updated for FlatBuffers
- [ ] Part 3: Comprehensive benchmark report generated
- [ ] V.1: grep returns 0 Cereal/Bitsery in code
- [ ] V.2: Benchmark suite Python files compile
- [ ] V.3: CI/CD passing (or pre-existing issues only)
- [ ] V.4: All commits pushed with semantic messages
- [ ] Final: Memory bank updated with current state

---

## Success Criteria Summary

### Deliverables

1. **Clean Codebase**: 0 Cereal/Bitsery references in active code
2. **Updated Tests**: Test suite covers FlatBuffers + Thrift only
3. **Updated Docs**: User docs mention FlatBuffers (default) + Thrift (legacy)
4. **Benchmark Suite**: Updated for FlatBuffers vs Thrift comparison
5. **Benchmark Report**: Comprehensive performance analysis
6. **Memory Bank**: Updated to reflect current state

### Metrics

- **References Removed**: 777 → 0 (or minimal in changelogs)
- **Files Deleted**: ~20 WIP documentation files
- **Files Updated**: ~15 source/test/doc files
- **Build Status**: Clean build with FlatBuffers + Thrift
- **Test Status**: All tests passing
- **CI/CD Status**: Pipeline passing

---

## Timeline Estimate

| Phase | Description | Time | Dependencies |
|-------|-------------|------|--------------|
| Part 1.1 | Delete WIP docs | 10 min | None |
| Part 1.2 | Update test files | 45 min | None |
| Part 1.3 | Clean CMake | 15 min | None |
| Part 1.4 | Check history.cpp | 10 min | None |
| Part 1.5 | Update memory bank | 20 min | None |
| Part 1.6 | Update user docs | 20 min | None |
| Part 1.7 | Verification | 15 min | Build working |
| Part 1.8 | Commit Part 1 | 5 min | All above |
| Part 2.1-2.4 | Update benchmarks | 10 min | None |
| Part 2.5 | Commit Part 2 | 5 min | All above |
| Part 3.1 | Download dataset | 5 min | Internet |
| Part 3.2 | Run benchmarks | 30-60 min | CI/CD success OR local build |
| Part 3.3 | Generate report | 5 min | Benchmark data |
| Part 3.4 | Commit Part 3 | 5 min | All above |
| **TOTAL** | | **~3-4 hours** | |

---

## Next Steps After Completion

1. **Create PR**: Merge feature branch to main
2. **Update CHANGES.md**: Document v0.16.0 changes
3. **Tag Release**: Create v0.16.0 release tag
4. **Update Documentation**: Ensure all docs reflect FlatBuffers as default
5. **Announce**: Communicate FlatBuffers as modern default format

---

## Emergency Rollback Plan

If issues arise during execution:

1. **Git Reset**: `git reset --hard <last-good-commit>`
2. **Restore Backup**: If needed, restore files from `/tmp/dwarfs-cereal-bitsery-backup/`
3. **Review Logs**: Check build logs and test output for specific failures
4. **Incremental Fix**: Fix issues one file at a time, commit after each success

---

## Contact & Support

- **GitHub Issues**: https://github.com/tamatebako/dwarfs/issues
- **Documentation**: `doc/*.md`
- **Memory Bank**: `.kilocode/rules/memory-bank/`

---

**End of Detailed Execution Plan**