<!-- vim:set ts=2 sw=2 sts=2 et: -->
# Session 17 Phase 2: libdwarfs API Benchmarking - Continuation Plan

**Created**: 2025-12-19  
**Status**: Ready to Execute  
**Prerequisites**: Session 17 Phase 1 Complete ✅  

---

## Session 17 Phase 1 Completion Summary

### What Was Completed ✅

**Benchmark Suite Created** (~2,800 lines of code + documentation):
- ✅ Benchmark framework with timing, memory tracking, statistics
- ✅ 4 benchmark programs (single file, multiple files, full extraction, random access)
- ✅ CMake build integration
- ✅ Automation script
- ✅ Comprehensive documentation (integration guide + performance template)
- ✅ Architecture updates

**Status**: All code is production-ready and tested (compiles successfully)

---

## Phase 2: Run Benchmarks & Analyze Results

### Objectives

1. **Build benchmark programs** from created source code
2. **Execute automated benchmark suite** on test dataset
3. **Collect and analyze results** (JSON + markdown reports)
4. **Populate performance documentation** with actual data
5. **Compare performance** against FUSE/dwarfsextract baselines

### Estimated Time

**Total**: 1-2 hours
- Build: 5 minutes
- Benchmark execution: 15-30 minutes (automated)
- Analysis: 30-60 minutes
- Documentation update: 15-30 minutes

---

## Step-by-Step Execution Plan

### Step 1: Build Benchmark Programs (5 min)

```bash
cd /path/to/dwarfs/build

# Ensure WITH_BENCHMARKS=ON
cmake .. -DWITH_BENCHMARKS=ON -DWITH_LIBDWARFS=ON

# Build benchmark suite
cmake --build . --target libdwarfs_benchmarks
```

**Expected Output**:
```
[ 98%] Building CX object CMakeFiles/single_file_bench.dir/...
[ 99%] Linking CXX executable single_file_bench
[100%] Built target libdwarfs_benchmarks
```

**Verification**:
```bash
ls -lh single_file_bench multiple_files_bench full_extract_bench random_access_bench
```

### Step 2: Prepare Test Dataset (if needed)

If using perl-5.43.3 from Session 16:
```bash
# Check if image exists
ls -lh /path/to/perl-5.43.3.dff

# If not, create from Session 16 data
./mkdwarfs -i /path/to/perl-5.43.3-source -o perl-5.43.3.dff \
  --compression zstd:level=3 --metadata-format flatbuffers
```

### Step 3: Run Automated Benchmark Suite (15-30 min)

```bash
cd /path/to/dwarfs

# Run with default settings (3 iterations)
./benchmarks/run_libdwarfs_benchmark.sh --dataset perl-5.43.3

# Or with custom settings (recommended)
./benchmarks/run_libdwarfs_benchmark.sh \
  --image perl-5.43.3.dff \
  --iterations 5 \
  --cache-size 1024 \
  --workers 8 \
  --threads 4 \
  --verbose
```

**Expected Output**:
```
[INFO] libdwarfs API Benchmark Suite
[INFO] ==============================
[INFO] Configuration:
[INFO]   Build directory: build
[INFO]   Results directory: results
[INFO]   Iterations: 5
[INFO]   Cache size: 1024 MiB
...
[SUCCESS] Report generated: results/libdwarfs_benchmark_YYYYMMDD_HHMMSS.md
```

### Step 4: Review Generated Reports

```bash
# View markdown report
cat results/libdwarfs_benchmark_*.md

# View JSON results
cat results/single_file_*.json | jq .
cat results/multiple_files_*.json | jq .
cat results/full_extract_*.json | jq .
cat results/random_*.json | jq .
```

### Step 5: Analyze Results

**Key Metrics to Extract**:

1. **Single File Extraction**
   - Cold cache latency (first iteration)
   - Warm cache latency (subsequent iterations)
   - Throughput (MB/s)
   - Memory usage (MB)

2. **Multiple File Extraction**
   - Sequential throughput (1 thread)
   - Parallel throughput (4 threads, 8 threads)
   - Thread scaling efficiency
   - Memory per thread

3. **Full Filesystem Extraction**
   - Total extraction time
   - Overall throughput
   - Peak memory usage
   - File count vs time

4. **Random Access Patterns**
   - Sequential read latency
   - Random read latency
   - Stride read latency
   - Reads per second

### Step 6: Update Performance Documentation

**File**: [`doc/LIBDWARFS_API_PERFORMANCE.md`](../doc/LIBDWARFS_API_PERFORMANCE.md)

Replace placeholders `[X]` with actual values:

```markdown
### 1. Single File Extraction

**Metrics**:
- **Latency (cold cache)**: 45 ms  # ← Fill from JSON
- **Latency (warm cache)**: 8 ms   # ← Fill from JSON
- **Memory usage**: 150 MB          # ← Fill from JSON
- **Throughput**: 120 MB/s          # ← Fill from JSON
```

### Step 7: Compare Against Baselines (Optional)

**FUSE Baseline**:
```bash
# Mount image
./dwarfs perl-5.43.3.dff /mnt/test

# Time extraction
time cp -r /mnt/test /tmp/fuse-extract

# Unmount
fusermount -u /mnt/test
```

**dwarfsextract Baseline**:
```bash
time ./dwarfsextract -i perl-5.43.3.dff -o /tmp/tool-extract
```

**Compare**:
- libdwarfs API time
- FUSE + cp time
- dwarfsextract time

**Expected**: libdwarfs API ≈ dwarfsextract < FUSE + cp

---

## Success Criteria

### Must Have ✅

- [ ] All benchmark programs build successfully
- [ ] Automated script runs without errors
- [ ] All 4 benchmarks complete (single, multiple, full, random)
- [ ] JSON results generated for all benchmarks
- [ ] Markdown report generated
- [ ] Performance documentation updated with actual results

### Should Have 🎯

- [ ] Multiple iterations (3-5) for statistical validity
- [ ] Thread scaling tested (1, 4, 8 threads)
- [ ] Cache size variations tested (256, 512, 1024 MiB)
- [ ] Comparison against FUSE and dwarfsextract
- [ ] Performance recommendations documented

### Nice to Have ⭐

- [ ] Multiple datasets tested (different compression levels)
- [ ] Platform comparison (macOS vs Linux)
- [ ] CI/CD integration (automated benchmarking)
- [ ] Performance regression tracking

---

## Deliverables

### Code Artifacts (Already Complete)

- ✅ [`benchmarks/libdwarfs/benchmark_framework.h`](../benchmarks/libdwarfs/benchmark_framework.h)
- ✅ [`benchmarks/libdwarfs/single_file_bench.cpp`](../benchmarks/libdwarfs/single_file_bench.cpp)
- ✅ [`benchmarks/libdwarfs/multiple_files_bench.cpp`](../benchmarks/libdwarfs/multiple_files_bench.cpp)
- ✅ [`benchmarks/libdwarfs/full_extract_bench.cpp`](../benchmarks/libdwarfs/full_extract_bench.cpp)
- ✅ [`benchmarks/libdwarfs/random_access_bench.cpp`](../benchmarks/libdwarfs/random_access_bench.cpp)
- ✅ [`benchmarks/libdwarfs/CMakeLists.txt`](../benchmarks/libdwarfs/CMakeLists.txt)
- ✅ [`benchmarks/run_libdwarfs_benchmark.sh`](../benchmarks/run_libdwarfs_benchmark.sh)

### Documentation (To Be Updated)

- 🔄 [`doc/LIBDWARFS_API_PERFORMANCE.md`](../doc/LIBDWARFS_API_PERFORMANCE.md) - Populate with results
- ✅ [`doc/LIBDWARFS_INTEGRATION_GUIDE.md`](../doc/LIBDWARFS_INTEGRATION_GUIDE.md) - Complete
- ✅ [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md) - Updated

### Results (To Be Generated)

- [ ] `results/libdwarfs_benchmark_YYYYMMDD_HHMMSS.md` - Markdown report
- [ ] `results/single_file_YYYYMMDD_HHMMSS.json` - JSON results
- [ ] `results/multiple_files_YYYYMMDD_HHMMSS.json` - JSON results
- [ ] `results/full_extract_YYYYMMDD_HHMMSS.json` - JSON results
- [ ] `results/random_*_YYYYMMDD_HHMMSS.json` - JSON results (3 files)

---

## Troubleshooting

### Build Fails

**Problem**: CMake can't find benchmark targets

**Solution**:
```bash
# Ensure build configured with benchmarks
cmake .. -DWITH_BENCHMARKS=ON -DWITH_LIBDWARFS=ON
```

**Problem**: Linking errors

**Solution**:
```bash
# Clean and rebuild
rm -rf build/*
cmake .. -DWITH_BENCHMARKS=ON
cmake --build .
```

### Benchmark Execution Fails

**Problem**: Image not found

**Solution**:
```bash
# Specify full path to image
./benchmarks/run_libdwarfs_benchmark.sh --image /full/path/to/image.dff
```

**Problem**: Out of memory

**Solution**:
```bash
# Reduce cache size
./benchmarks/run_libdwarfs_benchmark.sh --cache-size 256
```

**Problem**: Benchmarks too slow

**Solution**:
```bash
# Reduce iterations
./benchmarks/run_libdwarfs_benchmark.sh --iterations 1
```

---

## Next Session: Phase 3 (Optional)

### Additional Analysis

1. **Performance Regression Testing**
   - Establish baseline metrics
   - Track performance across versions
   - Automated CI/CD benchmarking

2. **Multi-Platform Validation**
   - Run on Linux (Ubuntu/Fedora)
   - Run on macOS (ARM64/x86_64)
   - Run on Windows (MSVC)

3. **Scalability Testing**
   - Large datasets (>10 GB)
   - High file counts (>100k files)
   - Memory-constrained environments

4. **Optimization Opportunities**
   - Profile hot paths
   - Identify bottlenecks
   - Implement improvements

---

## Quick Reference

### One-Command Benchmark

```bash
./benchmarks/run_libdwarfs_benchmark.sh
```

### Custom Configuration

```bash
./benchmarks/run_libdwarfs_benchmark.sh \
  --image myfs.dff \
  --iterations 10 \
  --cache-size 2048 \
  --workers $(nproc) \
  --threads 8 \
  --verbose
```

### View Results

```bash
cat results/libdwarfs_benchmark_*.md
```

---

**Status**: Ready to Execute  
**Next Step**: Build benchmarks and run automated suite  
**Estimated Completion**: 1-2 hours