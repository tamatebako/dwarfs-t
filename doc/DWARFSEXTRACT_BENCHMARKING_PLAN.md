# DwarFS Extract Benchmarking Plan

**Date**: 2025-12-04  
**Status**: 📋 **Ready to Execute**  
**Version**: v0.16.0-dev  
**Purpose**: Verify dwarfsextract fix has no performance regression

---

## Objective

Benchmark the fixed dwarfsextract to:
1. Verify no performance regression from the `size()` method addition
2. Document baseline performance metrics for FlatBuffers extraction
3. Compare with Thrift extraction performance (if available)
4. Establish performance benchmarks for future releases

---

## Benchmarking Strategy

### Phase 1: Micro-Benchmarks (String Table Access)
**Duration**: 30 minutes  
**Focus**: Verify `size()` vs `unpacked_size()` performance

**Tests**:
1. String table creation time (various sizes)
2. `size()` access time (O(1) verification)
3. `unpacked_size()` access time (O(n) for unpacked)
4. Memory usage comparison

**Datasets**:
- Small: 10 entries
- Medium: 1,000 entries
- Large: 100,000 entries

### Phase 2: Tool-Level Benchmarks (dwarfsextract)
**Duration**: 1 hour  
**Focus**: End-to-end extraction performance

**Metrics**:
1. Extraction time (wall clock)
2. Extraction throughput (MB/s)
3. Peak memory usage
4. CPU utilization
5. I/O operations

**Datasets**:
- Small: Linux kernel source (~1 GB, 70K files)
- Medium: Perl installations (47 GB → 431 MB compressed)
- Large: Full distribution (>100 GB)

### Phase 3: Comparison Benchmarks
**Duration**: 30 minutes  
**Focus**: FlatBuffers vs Thrift (if both available)

**Comparison Points**:
1. Extraction speed
2. Memory usage
3. Metadata loading time
4. Random access performance

### Phase 4: Regression Testing
**Duration**: 30 minutes  
**Focus**: Ensure no performance degradation

**Tests**:
1. Compare against benchmark baselines (from previous benchmarks)
2. Verify O(1) complexity of new `size()` method
3. Check for memory leaks (valgrind/ASAN)

---

## Benchmark Datasets

### Dataset 1: Linux Kernel Source
**Purpose**: Typical developer use case  
**Characteristics**:
- ~70,000 files
- ~1 GB uncompressed
- Many small text files
- Deep directory trees

**Creation**:
```bash
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.tar.xz
tar xf linux-6.6.tar.xz
./mkdwarfs -i linux-6.6 -o linux-kernel.dwarfs
```

### Dataset 2: Perl Installations (Existing)
**Purpose**: High compression ratio, many duplicates  
**Characteristics**:
- 1,139 Perl installations
- 47.65 GB → 431 MB (0.88%)
- Extreme deduplication

**Location**: `benchmark_data/perl-flatbuffers.dwarfs` (if exists)

### Dataset 3: Mixed Workload
**Purpose**: Realistic mixed content  
**Characteristics**:
- Mix of text, binary, compressed files
- Various file sizes (1 byte - 100 MB)
- Moderate compression ratio

**Creation**:
```bash
# Create synthetic dataset
mkdir -p /tmp/mixed-workload
# Script to create varied content
```

---

## Benchmark Metrics

### Primary Metrics
1. **Extraction Time**: Wall clock time for full extraction
2. **Throughput**: MB/s of extracted data
3. **Memory Usage**: Peak RSS (Resident Set Size)
4. **CPU Time**: User + System time

### Secondary Metrics
1. **Metadata Load Time**: Time to load filesystem metadata
2. **Block Decompression Time**: Average per block
3. **Cache Hit Rate**: Percentage of cache hits
4. **I/O Operations**: Read/Write counts

### String Table Specific Metrics
1. **size() Call Time**: Average time per call (should be <1ns)
2. **unpacked_size() Call Time**: Average time per call
3. **Memory Overhead**: Bytes per entry

---

## Benchmark Execution Plan

### Setup
```bash
# Build optimized release
cmake -B build-bench -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_BENCHMARKS=ON

ninja -C build-bench

# Prepare datasets
./prepare_benchmark_datasets.sh
```

### Run Benchmarks
```bash
# Phase 1: Micro-benchmarks (if we create them)
./build-bench/string_table_benchmark

# Phase 2: Tool-level benchmarks
for dataset in linux-kernel perl mixed-workload; do
  echo "=== Benchmarking $dataset ==="
  
  # Clear cache
  sync && echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true
  
  # Run extraction with metrics
  /usr/bin/time -v ./build-bench/dwarfsextract \
    -i benchmark_data/${dataset}.dwarfs \
    -o /tmp/extract-${dataset} \
    2>&1 | tee benchmark-results/${dataset}-extract.log
  
  # Cleanup
  rm -rf /tmp/extract-${dataset}
done

# Phase 3: Comparison (if Thrift build available)
if [ -f build-thrift/dwarfsextract ]; then
  # Compare FlatBuffers vs Thrift
  ./compare_formats.sh
fi

# Phase 4: Regression check
./check_regressions.sh
```

### Collect Results
```bash
# Generate comprehensive report
python3 benchmarks/generate_extraction_report.py \
  --results benchmark-results/ \
  --output doc/DWARFSEXTRACT_BENCHMARK_RESULTS.md
```

---

## Expected Results

### String Table Performance
```
Operation          | Before Fix | After Fix | Change
-------------------|------------|-----------|--------
size() call        | N/A        | <1 ns     | N/A
unpacked_size()    | O(n)       | O(n)      | No change
Memory per entry   | N/A        | +0 bytes  | No overhead
```

### Extraction Performance (Linux Kernel)
```
Metric                | Expected
----------------------|---------
Extraction Time       | <5 seconds
Throughput            | >200 MB/s
Peak Memory           | <100 MB
CPU Utilization       | 80-95%
```

### Comparison (FlatBuffers vs Thrift)
```
Metric                | FlatBuffers | Thrift | Ratio
----------------------|-------------|--------|-------
Metadata Load Time    | <50 ms      | <50 ms | ~1.0x
Extraction Time       | TBD         | TBD    | ~1.0x
Memory Usage          | TBD         | TBD    | ~1.0x
```

---

## Success Criteria

### Must Pass (Blocking)
1. ✅ No performance regression >5% compared to baseline
2. ✅ `size()` method is O(1) (verified via profiling)
3. ✅ No memory leaks (verified via valgrind/ASAN)
4. ✅ All extractions complete successfully

### Should Pass (Warning)
1. ⚠️ Memory usage within 10% of baseline
2. ⚠️ CPU utilization within expected range (80-100%)
3. ⚠️ Cache hit rate >90% for sequential access

### Nice to Have
1. 💡 Extraction throughput >200 MB/s on modern hardware
2. 💡 Metadata load time <100ms for typical datasets
3. 💡 Performance parity with Thrift implementation

---

## Benchmark Report Format

The final report should include:

### Executive Summary
- Overall performance assessment
- Key findings
- Recommendations

### Detailed Results
- Raw benchmark data (tables)
- Performance graphs (if generated)
- Statistical analysis (mean, median, stddev)

### Comparison Analysis
- Before/after comparison (if applicable)
- FlatBuffers vs Thrift (if applicable)
- Platform-specific results

### Recommendations
- Optimization opportunities
- Configuration tuning suggestions
- Future work

---

## Tools & Infrastructure

### Required Tools
- `/usr/bin/time` - Resource usage measurement
- `perf` (Linux) - CPU profiling
- `valgrind` - Memory leak detection
- `hyperfine` - Statistical benchmarking (optional)

### Optional Tools
- `flamegraph` - Visualization
- `heaptrack` - Memory profiling
- `cachegrind` - Cache analysis

### Scripts to Create
1. `prepare_benchmark_datasets.sh` - Download/create datasets
2. `run_extraction_benchmarks.sh` - Execute all benchmarks
3. `compare_formats.sh` - FlatBuffers vs Thrift comparison
4. `check_regressions.sh` - Verify no performance loss
5. `generate_extraction_report.py` - Create markdown report

---

## Timeline

| Phase | Duration | Dependencies | Output |
|-------|----------|--------------|--------|
| Setup | 15 min | Datasets | Benchmark environment ready |
| Phase 1 | 30 min | - | Micro-benchmark results |
| Phase 2 | 1 hour | Datasets | Tool-level benchmarks |
| Phase 3 | 30 min | Thrift build | Comparison data |
| Phase 4 | 30 min | Baseline data | Regression report |
| Report | 30 min | All phases | Final documentation |

**Total**: ~3 hours

---

## Deliverables

1. **Benchmark Results**: `doc/DWARFSEXTRACT_BENCHMARK_RESULTS.md`
2. **Raw Data**: `benchmark-results/*.json` or `*.csv`
3. **Regression Report**: `doc/DWARFSEXTRACT_REGRESSION_ANALYSIS.md`
4. **Performance Graphs**: `doc/perf/*.svg` (optional)

---

## Notes

### Why This Matters
1. **Confidence**: Proves fix doesn't degrade performance
2. **Baseline**: Establishes metrics for future releases
3. **Documentation**: Provides performance expectations for users
4. **Validation**: Confirms O(1) complexity of new method

### Future Work
1. Add micro-benchmarks to CI/CD pipeline
2. Track performance over time (performance regression testing)
3. Create automated performance alerts
4. Benchmark on various platforms (ARM, x86, RISC-V)

---

## Status: Ready to Execute

All prerequisites met:
- ✅ Fix implemented and tested
- ✅ Production build available
- ✅ Test datasets available or easy to create
- ✅ Benchmarking infrastructure exists (from Phase G)

**Next Step**: Execute Phase 1 (Micro-Benchmarks)