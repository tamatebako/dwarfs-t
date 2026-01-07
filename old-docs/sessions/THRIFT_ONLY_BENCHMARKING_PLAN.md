# Thrift-Only Build Benchmarking & Verification Plan

**Date**: 2025-12-02  
**Status**: Ready to Execute  
**Priority**: HIGH - Critical for v0.16.0 release

---

## Overview

This plan outlines comprehensive benchmarking to verify that the Thrift-only build fix:
1. Compiles successfully without FlatBuffers
2. Passes all applicable tests
3. Performs equivalently to FlatBuffers builds
4. Has acceptable binary sizes

---

## Phase 1: Build Verification (Est: 30 min)

### 1.1 Clean Build Test

**Objective**: Verify all three build configurations compile successfully

**Test Configurations**:

```bash
# Configuration 1: FlatBuffers-Only
rm -rf build-fb && mkdir build-fb && cd build-fb
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
time ninja 2>&1 | tee build-fb.log

# Configuration 2: Thrift-Only
rm -rf build-tb && mkdir build-tb && cd build-tb
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
time ninja 2>&1 | tee build-tb.log

# Configuration 3: Dual-Format
rm -rf build-dual && mkdir build-dual && cd build-dual
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
time ninja 2>&1 | tee build-dual.log
```

**Success Criteria**:
- All builds complete without errors
- No "undefined reference" linker errors
- No segmentation faults during build

**Metrics to Collect**:
- Build time (wall clock)
- Number of compilation units
- Any warnings or errors

### 1.2 Binary Size Analysis

**Objective**: Compare binary sizes across configurations

```bash
# Collect binary sizes
for build in build-fb build-tb build-dual; do
  cd $build
  ls -lh mkdwarfs dwarfsck dwarfsextract dwarfs-bin dwarfs_unit_tests
  size mkdwarfs dwarfsck dwarfsextract
done > binary_sizes.txt
```

**Success Criteria**:
- Thrift-only binaries smaller than dual-format
- FlatBuffers-only binaries smaller than dual-format
- No unexpected binary size increases

---

## Phase 2: Test Suite Verification (Est: 1 hour)

### 2.1 Unit Tests

**Objective**: Verify all applicable tests pass

```bash
# FlatBuffers-only tests
cd build-fb
./dwarfs_unit_tests --gtest_output=json:test-results-fb.json 2>&1 | tee test-fb.log

# Thrift-only tests
cd build-tb
./dwarfs_unit_tests --gtest_output=json:test-results-tb.json 2>&1 | tee test-tb.log

# Dual-format tests
cd build-dual
./dwarfs_unit_tests --gtest_output=json:test-results-dual.json 2>&1 | tee test-dual.log
```

**Success Criteria**:
- **FlatBuffers-only**: 1,600/1,613 tests pass (13 skipped = Thrift tests)
- **Thrift-only**: Similar pass rate (some FlatBuffers tests will be skipped)
- **Dual-format**: All 1,613 tests pass
- **NO SEGFAULTS** in any configuration

**Metrics to Collect**:
- Total tests run
- Tests passed
- Tests failed (with failure details)
- Tests skipped
- Execution time

### 2.2 Test Analysis

Create comparison report:

```python
import json

configs = ['fb', 'tb', 'dual']
results = {}

for config in configs:
    with open(f'test-results-{config}.json') as f:
        results[config] = json.load(f)
    
    print(f"\n{config.upper()} Configuration:")
    print(f"  Total: {results[config]['tests']}")
    print(f"  Passed: {results[config]['passes']}")
    print(f"  Failed: {results[config]['failures']}")
    print(f"  Skipped: {results[config]['disabled']}")
```

---

## Phase 3: Functional Verification (Est: 45 min)

### 3.1 Tool Functionality Tests

**Objective**: Verify all tools work correctly in each configuration

**Test Matrix**:

| Tool | Test | FlatBuffers | Thrift | Dual |
|------|------|-------------|--------|------|
| mkdwarfs | Create image | ✓ | ✓ | ✓ |
| dwarfsck | Check image | ✓ | ✓ | ✓ |
| dwarfsextract | Extract image | ✓ | ✓ | ✓ |
| dwarfs | Mount image | ✓ | ✓ | ✓ |

**Test Script**:

```bash
#!/bin/bash
set -e

for config in fb tb dual; do
  cd build-$config
  
  # Create test filesystem
  ./mkdwarfs -i ../benchmark-files/tiny -o test-${config}.dwarfs \
    --log-level=error
  
  # Check filesystem
  ./dwarfsck -i test-${config}.dwarfs
  
  # Extract filesystem
  mkdir -p extract-${config}
  ./dwarfsextract -i test-${config}.dwarfs -o extract-${config}
  
  # Verify extraction
  diff -r ../benchmark-files/tiny extract-${config}
  
  # Test mounting (if FUSE available)
  if [ -f dwarfs-bin ]; then
    mkdir -p mnt-${config}
    ./dwarfs-bin test-${config}.dwarfs mnt-${config} &
    MOUNT_PID=$!
    sleep 2
    ls -la mnt-${config}
    kill $MOUNT_PID
    rmdir mnt-${config}
  fi
  
  echo "$config: ALL TOOLS WORKING"
done
```

**Success Criteria**:
- All tools execute without errors
- Images created with each config can be read by others
- Extracted content matches source
- Mounting works (if applicable)

---

## Phase 4: Performance Benchmarking (Est: 2 hours)

### 4.1 Compression Performance

**Objective**: Compare compression speed and ratios

```bash
#!/bin/bash
DATASET="benchmark-files/perl"

for config in fb tb dual; do
  cd build-$config
  
  echo "=== $config Configuration ==="
  
  # Compression benchmark
  /usr/bin/time -v ./mkdwarfs \
    -i ../$DATASET \
    -o ${config}-perl.dwarfs \
    --log-level=warn \
    2>&1 | tee compress-${config}.log
  
  # Record metrics
  SIZE=$(stat -f%z ${config}-perl.dwarfs 2>/dev/null || stat -c%s ${config}-perl.dwarfs)
  echo "Image size: $SIZE bytes"
done
```

**Metrics to Collect**:
- Compression time (wall clock, user, system)
- Peak memory usage
- Image size
- Compression ratio

### 4.2 Read Performance

**Objective**: Compare random access and sequential read performance

```bash
#!/bin/bash

for config in fb tb dual; do
  cd build-$config
  
  # Mount filesystem
  mkdir -p mnt
  ./dwarfs-bin ${config}-perl.dwarfs mnt &
  MOUNT_PID=$!
  sleep 2
  
  # Sequential read test
  echo "Sequential read test:"
  time tar -cf /dev/null mnt/
  
  # Random access test
  echo "Random access test:"
  time find mnt/ -type f -exec md5sum {} \; > /dev/null
  
  # Cleanup
  kill $MOUNT_PID
  rmdir mnt
done
```

**Metrics to Collect**:
- Sequential read throughput (MB/s)
- Random access latency (ms)
- Total read time for full extraction

---

## Phase 5: Report Generation (Est: 1 hour)

### 5.1 Generate Comprehensive Report

Create markdown report with:

1. **Executive Summary**
   - Overall success/failure
   - Key findings
   - Recommendations

2. **Build Verification Results**
   - Build times
   - Binary sizes
   - Warnings/errors

3. **Test Results**
   - Pass/fail rates
   - Failed test details
   - Configuration-specific behaviors

4. **Performance Comparison**
   - Compression performance
   - Read performance
   - Memory usage

5. **Recommendations**
   - Production readiness
   - Known issues
   - Future work

### 5.2 Update Official Documentation

Update these files:
- `README.md` - Add Thrift-only build instructions
- `doc/THRIFT_ONLY_BUILD_VERIFICATION_PLAN.md` - Mark as complete
- `doc/FINAL_STATUS_DECEMBER_2_2025.md` - Update with results

Move to `old-docs/`:
- `doc/THRIFT_ONLY_NEXT_SESSION_PLAN.md`
- `doc/THRIFT_ONLY_NEXT_SESSION_STATUS.md`
- `doc/THRIFT_ONLY_BUILD_CONTINUATION_PROMPT.md`

---

## Success Criteria Summary

✅ **Build**: All three configurations compile successfully  
✅ **Tests**: All applicable tests pass without segfaults  
✅ **Tools**: All CLI tools work in each configuration  
✅ **Performance**: Thrift-only performs within 10% of FlatBuffers  
✅ **Binary Size**: Thrift-only binaries are smaller than dual-format  

**Release Decision**: If all criteria met → Ready for v0.16.0

---

## Timeline Estimate

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Build Verification | 30 min | None |
| Test Verification | 1 hour | Phase 1 complete |
| Functional Tests | 45 min | Phase 1 complete |
| Performance Benchmarks | 2 hours | Phase 1 complete |
| Report Generation | 1 hour | All phases complete |
| **Total** | **5 hours 15 min** | - |

---

## Risk Assessment

**Low Risk**:
- FlatBuffers-only build (already working)
- Dual-format build (already working)

**Medium Risk**:
- Thrift-only test pass rate (unknown)

**Mitigation**:
- Document any Thrift-specific test failures
- Create follow-up issues for any problems
- Do not block v0.16.0 release for minor issues

---

**Next Step**: Execute Phase 1 (Build Verification)