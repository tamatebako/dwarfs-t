# DwarFS Comprehensive Benchmark Guide

**Version**: 0.16.0  
**Last Updated**: 2025-12-05

---

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Quick Start](#quick-start)
4. [Detailed Guide](#detailed-guide)
5. [Understanding Results](#understanding-results)
6. [Advanced Usage](#advanced-usage)
7. [Troubleshooting](#troubleshooting)

---

## Overview

The DwarFS comprehensive benchmark suite validates performance across:
- **Build configurations**: fb-only, thrift-only, both
- **Metadata formats**: FlatBuffers, Thrift
- **Operations**: create, extract_full, extract_single
- **Datasets**: dwarfs-source, linux-kernel, custom

### Key Features

- ✅ Clean rebuild from scratch (ensures code consistency)
- ✅ Multiple runs for statistical significance
- ✅ Comprehensive metrics collection
- ✅ Statistical analysis (mean, median, stdev, percentiles)
- ✅ Regression detection
- ✅ JSON output for automation

---

## Prerequisites

### System Requirements

- **OS**: macOS, Linux, FreeBSD, Windows
- **Python**: 3.8 or later
- **CMake**: 3.28.0 or later
- **Ninja**: Recommended for faster builds
- **Disk Space**: 20 GB free (for builds and test data)
- **Memory**: 8 GB minimum, 16 GB recommended

### Dependencies

All dependencies are managed automatically by CMake. See main README for details.

---

## Quick Start

### Step 1: Clean Rebuild and Run

```bash
cd /path/to/dwarfs

# Clean rebuild and run comprehensive benchmark
./benchmarks/clean_rebuild_and_benchmark.sh
```

This will:
1. Delete all previous benchmark builds
2. Rebuild all configurations from scratch
3. Run comprehensive benchmarks
4. Save results with timestamp

### Step 2: View Results

```bash
# Find latest results
ls -lt benchmark-results/comprehensive/ | head -5

# View summary
cat benchmark-results/comprehensive/YYYYMMDD-HHMMSS/summary.json
```

**That's it!** For most users, this is sufficient.

---

## Detailed Guide

### Manual Step-by-Step Execution

For more control, run steps individually:

#### Step 1: Clean Previous Builds

**CRITICAL**: Always start with a clean slate.

```bash
cd /path/to/dwarfs

# Delete ALL benchmark builds
rm -rf build-fb-bench build-thrift-bench build-both-bench

# Clean benchmark data
rm -rf benchmark-data/images/*
```

**Why?** Pre-built directories may contain outdated code. Benchmarks MUST use fresh builds to ensure accuracy.

#### Step 2: Rebuild All Configurations

```bash
# Rebuild all from scratch
python3 benchmarks/lib/build_manager.py --workspace . --build-all
```

**Expected Output**:
```
Building: FlatBuffers Only (fb-only)
✓ Build successful

Building: Thrift Only (thrift-only)  
✓ Build successful (or ✗ if Thrift unavailable - this is OK)

Building: Both Formats (both)
✓ Build successful (or ✗ if build error - see troubleshooting)
```

#### Step 3: Run Benchmarks

**Simple Test** (3 runs, ~5 minutes):
```bash
python3 benchmarks/comprehensive_benchmark.py \
  --builds fb-only \
  --datasets dwarfs-source \
  --operations create,extract_full \
  --runs 3
```

**Full Validation** (10 runs, ~30 minutes):
```bash
python3 benchmarks/comprehensive_benchmark.py \
  --builds fb-only,both \
  --datasets dwarfs-source,linux-kernel \
  --operations create,extract_full \
  --runs 10 \
  --output-dir benchmark-results/comprehensive/validation-$(date +%Y%m%d)
```

**Format Comparison** (comparing FlatBuffers vs Thrift):
```bash
python3 benchmarks/comprehensive_benchmark.py \
  --builds both \
  --datasets linux-kernel \
  --operations create,extract_full \
  --formats flatbuffers,thrift \
  --runs 5 \
  --output-dir benchmark-results/comprehensive/format-comparison-$(date +%Y%m%d)
```

#### Step 4: Analyze Results

```bash
# View detailed results
python3 -m json.tool benchmark-results/comprehensive/latest/summary.json

# Check for regressions (if you have baseline)
python3 benchmarks/comprehensive_benchmark.py \
  --builds both \
  --datasets linux-kernel \
  --operations all \
  --runs 5 \
  --regression-check \
  --baseline benchmark-results/comprehensive/v0.15.0/summary.json
```

---

## Understanding Results

### Output Files

Each benchmark run creates three JSON files:

#### 1. results.json (Complete Data)

Contains every single run with full metrics:

```json
{
  "metadata": {
    "version": "0.16.0",
    "start_time": "2025-12-05T...",
    "platform": {
      "system": "Darwin",
      "machine": "arm64",
      "cpu_count": 10
    }
  },
  "total_runs": 30,
  "successful_runs": 30,
  "runs": [
    {
      "operation": "create",
      "build_config": "fb-only",
      "dataset": "linux-kernel",
      "format": "flatbuffers",
      "run_number": 1,
      "success": true,
      "metrics": {
        "time_wall": 45.234,
        "size_input": 1073741824,
        "size_output": 107374182,
        "compression_ratio": 10.0,
        "throughput_mb_s": 23.75
      }
    }
  ]
}
```

#### 2. detailed.json (Grouped Results)

Groups runs by test configuration:

```json
{
  "groups": {
    "create_fb-only_linux-kernel_flatbuffers": {
      "count": 5,
      "successful": 5,
      "failed": 0,
      "runs": [...]
    }
  }
}
```

#### 3. summary.json (Statistical Analysis)

High-level statistics for quick comparison:

```json
{
  "version": "0.16.0",
  "operations": {
    "create": {
      "count": 15,
      "time_stats": {
        "mean": 45.67,
        "median": 45.23,
        "stdev": 2.34,
        "p50": 45.23,
        "p90": 48.12,
        "p95": 49.01,
        "p99": 50.23
      }
    }
  },
  "formats": {
    "flatbuffers": {
      "count": 15,
      "time_stats": {...},
      "avg_size_mb": 102.4
    },
    "thrift": {
      "count": 15,
      "time_stats": {...},
      "avg_size_mb": 97.3  
    }
  }
}
```

### Key Metrics

| Metric | Description | Unit |
|--------|-------------|------|
| **time_wall** | Total elapsed time | seconds |
| **size_input** | Input data size | bytes |
| **size_output** | Output image size | bytes |
| **compression_ratio** | Input/Output ratio | ratio |
| **throughput_mb_s** | Processing speed | MB/s |
| **memory_peak_mb** | Peak memory usage | MB |

### Statistical Measures

| Measure | Meaning |
|---------|---------|
| **mean** | Average value |
| **median** | Middle value (50th percentile) |
| **stdev** | Standard deviation (variability) |
| **p90** | 90% of runs were faster than this |
| **p95** | 95% of runs were faster than this |
| **p99** | 99% of runs were faster than this |
| **cv_percent** | Coefficient of variation (lower = more stable) |

---

## Advanced Usage

### Custom Datasets

#### Using Local Directory

```bash
# Create dataset info (if not already defined)
# Edit benchmarks/lib/dataset_manager.py and add:

'my-dataset': DatasetInfo(
    name='My Custom Dataset',
    slug='my-dataset',
    description='My data description',
    url=None,
    sha256=None,
    size_mb=500,
    file_count=10000,
    source_type='local',
)

# Place data in benchmark-data/datasets/my-dataset/
# Then run benchmarks
python3 benchmarks/comprehensive_benchmark.py \
  --datasets my-dataset \
  --operations create \
  --runs 3
```

#### Download Large Dataset

The benchmark suite can download and cache datasets:

```bash
# Linux kernel will be downloaded automatically
python3 benchmarks/comprehensive_benchmark.py \
  --datasets linux-kernel \
  --operations create \
  --runs 5
```

### Compression Algorithm Comparison

```bash
# Compare zstd levels
for level in 1 3 5 7 9; do
  python3 benchmarks/comprehensive_benchmark.py \
    --builds fb-only \
    --datasets dwarfs-source \
    --operations create \
    --runs 3 \
    --output-dir benchmark-results/compression/zstd-level-${level}
  
  # Edit dataset_manager.py to use different compression level
  # OR create images manually with different settings
done
```

### Regression Detection

```bash
# 1. Create baseline (current version)
python3 benchmarks/comprehensive_benchmark.py \
  --builds both \
  --datasets linux-kernel \
  --operations all \
  --runs 10 \
  --output-dir benchmark-results/baselines/v0.16.0

# 2. After code changes, compare
python3 benchmarks/comprehensive_benchmark.py \
  --builds both \
  --datasets linux-kernel \
  --operations all \
  --runs 10 \
  --regression-check \
  --baseline benchmark-results/baselines/v0.16.0/summary.json \
  --threshold 0.05  # 5% regression threshold
```

If regressions detected:
```json
{
  "has_regressions": true,
  "regressions": [
    {
      "operation": "create",
      "format": "flatbuffers",
      "current": 46.2,
      "baseline": 44.0,
      "delta_percent": 5.0
    }
  ]
}
```

---

## Troubleshooting

### Common Issues

#### Issue 1: "both" Build Fails

**Symptom**: Build error in `metadata_v2_thrift.cpp`

**Cause**: Thrift + FlatBuffers compilation issue (codebase bug)

**Workaround**:
```bash
# Use separate builds
python3 benchmarks/comprehensive_benchmark.py \
  --builds fb-only \
  --formats flatbuffers \
  ...
```

#### Issue 2: Extraction Tests Fail

**Symptom**: `FlatBuffers metadata verification failed`

**Cause**: String table bug in fresh builds (fixed in main build)

**Solution**:
```bash
# Delete corrupted images
rm -f benchmark-data/images/*

# Rebuild with fix
rm -rf build-fb-bench
python3 benchmarks/lib/build_manager.py --build fb-only

# Re-run benchmarks
python3 benchmarks/comprehensive_benchmark.py ...
```

#### Issue 3: Out of Disk Space

**Symptom**: Build or benchmark fails with "No space left on device"

**Solution**:
```bash
# Clean old builds
rm -rf build-*

# Clean old benchmark results
rm -rf benchmark-results/comprehensive/202412*  # old dates

# Clean downloaded datasets (if not needed)
rm -rf benchmark-data/datasets/linux-kernel
```

#### Issue 4: Thrift-only Build Fails

**Expected**: Thrift-only builds may fail if Thrift is not available

**Solution**: This is OK - FlatBuffers-only testing is sufficient. If you need Thrift:
```bash
# Check if Thrift is available
cmake . -DDWARFS_WITH_THRIFT=ON

# If not, install Folly + fbthrift (platform-specific)
```

### Debug Output

Enable verbose mode for troubleshooting:

```bash
python3 benchmarks/comprehensive_benchmark.py \
  --builds fb-only \
  --datasets dwarfs-source \
  --operations create \
  --runs 1 \
  --verbose
```

---

## Benchmark Best Practices

### DO ✅

1. **Always clean and rebuild** before benchmarks
2. **Run multiple iterations** (minimum 3, preferably 10)
3. **Use consistent hardware** for comparisons
4. **Document system state** (other processes, I/O load)
5. **Save results with timestamps**
6. **Compare like-with-like** (same dataset, same build type)

### DON'T ❌

1. **Never use pre-built directories** (`build-fb/`, `build-debug/`, etc.)
2. **Don't run on busy systems** (other I/O loads skew results)
3. **Don't compare different datasets**
4. **Don't use single runs** (insufficient statistical significance)
5. **Don't modify code between runs** (invalidates comparison)

---

## Performance Expectations

### Typical Results (macOS ARM64, M1 Pro)

| Operation | Dataset | Time (mean) | Throughput |
|-----------|---------|-------------|------------|
| create | dwarfs-source (20 MB) | 2-5s | 4-10 MB/s |
| create | linux-kernel (1 GB) | 30-60s | 17-33 MB/s |
| extract_full | dwarfs-source | 0.5-1s | 20-40 MB/s |
| extract_full | linux-kernel | 10-20s | 50-100 MB/s |

### Format Comparison

| Metric | FlatBuffers | Thrift | Notes |
|--------|-------------|--------|-------|
| **Size** | 105-108% | 100% (baseline) | Thrift ~3-5% smaller |
| **Speed** | Similar | Similar | Both use zero-copy |
| **Memory** | Similar | Similar | Both memory-mappable |
| **Portability** | Excellent | Limited | FlatBuffers header-only |

---

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Benchmark

on:
  push:
    branches: [main]
  pull_request:

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y ninja-build cmake ...
      
      - name: Run benchmarks
        run: |
          ./benchmarks/clean_rebuild_and_benchmark.sh
      
      - name: Upload results
        uses: actions/upload-artifact@v3
        with:
          name: benchmark-results
          path: benchmark-results/comprehensive/
```

---

## FAQ

### Q: How long does a full benchmark take?

**A**: Depends on configuration:
- Quick test (3 runs, dwarfs-source): ~5-10 minutes
- Full validation (10 runs, linux-kernel): ~30-60 minutes
- Comprehensive suite (all configs, multiple datasets): 2-4 hours

### Q: Can I run benchmarks in parallel?

**A**: Not recommended - parallel builds compete for resources and skew results. Run sequentially for accurate measurements.

### Q: What's the minimum number of runs?

**A**: Minimum 3 for basic validation, 5-10 for reliable statistics, 20+ for publication-quality data.

### Q: How do I compare FlatBuffers vs Thrift?

**A**: Use the `both` build configuration with `--formats flatbuffers,thrift`:

```bash
python3 benchmarks/comprehensive_benchmark.py \
  --builds both \
  --formats flatbuffers,thrift \
  --datasets linux-kernel \
  --runs 10
```

### Q: Can I benchmark on Windows?

**A**: Yes, but you need to adapt the clean_rebuild script:

```powershell
# Clean
Remove-Item build-*-bench -Recurse -Force

# Rebuild  
python benchmarks/lib/build_manager.py --build-all

# Run
python benchmarks/comprehensive_benchmark.py ...
```

---

## Support

- **Issues**: https://github.com/tamatebako/dwarfs/issues
- **Discussions**: https://github.com/tamatebako/dwarfs/discussions
- **Documentation**: See `doc/` directory

---

**Last Updated**: 2025-12-05
**Version**: 0.16.0

---

## Appendix A: FUSE Performance Benchmarking

### Overview

Guide for benchmarking FUSE driver performance in DwarFS, with special focus on comparing different FUSE implementations (FUSE-T vs macFUSE on macOS, libfuse on Linux) and monitoring performance metrics.

### FUSE Implementations

#### macOS

DwarFS supports two FUSE implementations on macOS:

**FUSE-T (Recommended)**
- **Type**: User-space FUSE implementation
- **Performance**: Generally faster than macFUSE
- **Stability**: Modern, actively maintained
- **Installation**: `brew install fuse-t`
- **Mount command**: Uses standard `dwarfs` binary

**macFUSE**
- **Type**: Kernel extension-based FUSE
- **Performance**: Mature but slower than FUSE-T
- **Stability**: Long-established, well-tested
- **Installation**: `brew install macfuse`
- **Note**: Requires system extension approval

#### Linux

**libfuse**
- **Type**: Standard FUSE implementation
- **Versions**: libfuse2 and libfuse3 supported
- **Performance**: Highly optimized for Linux
- **Installation**: Usually pre-installed or via package manager

### Performance Monitoring

#### Using the perfmon Option

DwarFS includes built-in performance monitoring that tracks FUSE operation latencies:

```bash
dwarfs image.dwarfs mountpoint -o perfmon=fuse
```

**Available Components**:
- `fuse`: FUSE driver operations
- `filesystem_v2`: Filesystem implementation
- `inode_reader_v2`: Inode reading operations
- `block_cache`: Block cache operations

#### Extracting Performance Data

While the filesystem is mounted, extract perfmon data using extended attributes:

```bash
# View all perfmon data
xattr -p user.dwarfs.driver.perfmon mountpoint

# Save to file for analysis
xattr -p user.dwarfs.driver.perfmon mountpoint > perfmon_data.txt
```

### Latency Metrics

#### FUSE Operation Types

The perfmon system tracks these FUSE operations:

- **op_init**: Filesystem initialization
- **op_lookup**: Directory entry lookup
- **op_getattr**: Get file/directory attributes
- **op_readlink**: Read symbolic link target
- **op_open**: Open file
- **op_read**: Read file data
- **op_readdir**: Read directory contents
- **op_statfs**: Get filesystem statistics
- **op_getxattr**: Get extended attribute
- **op_listxattr**: List extended attributes

#### Example Output

```
op_init: samples=1, avg=199.4us, p50=262.1us, p90=262.1us, p99=262.1us
op_lookup: samples=16, avg=1249us, p50=2097us, p90=4194us, p99=4194us
op_getattr: samples=1, avg=5.786us, p50=8.192us, p90=8.192us, p99=8.192us
op_open: samples=16, avg=7.641us, p50=4.096us, p90=32.77us, p99=32.77us
op_read: samples=45145, avg=71.2us, p50=131.1us, p90=131.1us, p99=262.1us
op_readdir: samples=2, avg=25650us, p50=32.77us, p90=67110us, p99=67110us
```

### FUSE-T vs macFUSE Comparison

| Metric | FUSE-T | macFUSE | Notes |
|--------|--------|---------|-------|
| Read throughput | Higher | Lower | FUSE-T typically 20-30% faster |
| Operation latency | Lower | Higher | Especially for metadata operations |
| CPU usage | Lower | Higher | User-space implementation advantage |
| Stability | Excellent | Excellent | Both production-ready |

### Running FUSE Benchmarks

#### Basic Throughput Test

```bash
# Mount filesystem with perfmon
dwarfs image.dwarfs /mnt/test -o perfmon=fuse

# Sequential read test
time dd if=/mnt/test/largefile of=/dev/null bs=1M

# Random read test
time find /mnt/test -type f -exec cat {} \; > /dev/null

# Get perfmon data
xattr -p user.dwarfs.driver.perfmon /mnt/test

# Unmount
umount /mnt/test
```

#### Cache Impact Testing

Compare performance with different cache sizes:

```bash
# Small cache
dwarfs image.dwarfs /mnt/test -o cachesize=64m,perfmon=fuse
# Run workload and measure

# Large cache
dwarfs image.dwarfs /mnt/test -o cachesize=512m,perfmon=fuse
# Run same workload and compare
```

### Troubleshooting Performance Issues

**High Latency**:
1. **Check cache size**: Increase with `-o cachesize=`
2. **Monitor cache hit rate**: Use perfmon to track cache efficiency
3. **Verify FUSE implementation**: Try FUSE-T on macOS
4. **Check compression format**: Some formats decompress faster than others

**Low Throughput**:
1. **Increase worker threads**: `-o workers=N`
2. **Enable sequential detection**: `-o seq_detector=3`
3. **Adjust readahead**: `-o readahead=1m`
4. **Check block size**: Larger blocks may help sequential reads

---

## Appendix B: Metadata Format Benchmarking

### Overview

Comprehensive benchmarking suite for comparing Thrift, FlatBuffers, and other metadata serialization formats in DwarFS. Evaluates performance across compression efficiency, extraction speed, and FUSE operation latency.

### Quick Start

```bash
# 1. Download benchmark datasets (if not already present)
cmake --build . --target download-benchmark-datasets

# 2. Run benchmarks on Perl dataset
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./build/mkdwarfs \
  --dwarfsextract ./build/dwarfsextract \
  --dwarfs ./build/dwarfs \
  --perl-dataset benchmark-files/perl-5.42.0 \
  --output benchmarks/results/metadata_format_results.json

# 3. Generate report
ruby benchmarks/generate_metadata_report.rb \
  benchmarks/results/metadata_format_results.json \
  METADATA_FORMAT_BENCHMARK_REPORT.md
```

### Benchmark Metrics

#### Compression Performance

- **Time**: Wall-clock time to create the DwarFS archive
- **Size**: Final archive size in bytes
- **Memory**: Peak resident memory usage during compression

#### Extraction Performance

- **Time**: Wall-clock time to extract files
- **Throughput**: MB/s extraction rate
- **Memory**: Peak memory usage during extraction

#### FUSE Performance

- **Mount Time**: Time to mount the filesystem
- **Read Operations**: Time and throughput for reading files
- **Operation Latency**: Low-level FUSE operation timings in microseconds

### Test File Selection

The benchmark uses specific test files for single-file operations:

- **Perl**: `doio.c` (106KB) - Representative C source file
- **RaspOS**: `bin/bash` (~1MB) - Common system binary

### Interpreting Results

The report uses Thrift as the baseline and shows:

- `~`: No significant difference (<0.5%)
- `-X%`: Improvement (lower is better for time/latency)
- `+X%`: Improvement for throughput, regression for time/size

### Integration with CI/CD

```yaml
name: Metadata Format Benchmarks

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Build DwarFS
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build -j$(nproc)

      - name: Download Datasets
        run: cmake --build build --target download-benchmark-datasets

      - name: Run Benchmarks
        run: |
          python3 benchmarks/run_metadata_format_benchmark.py \
            --mkdwarfs ./build/mkdwarfs \
            --dwarfsextract ./build/dwarfsextract \
            --dwarfs ./build/dwarfs \
            --perl-dataset benchmark-files/perl-5.42.0 \
            --output benchmarks/results/metadata_results.json

      - name: Generate Report
        run: |
          ruby benchmarks/generate_metadata_report.rb \
            benchmarks/results/metadata_results.json \
            METADATA_BENCHMARK_REPORT.md
```

---

## Appendix C: FlatBuffers Performance Report

**Date**: 2025-12-19
**Session**: 16 - FlatBuffers Benchmarking & Performance Enhancement
**Dataset**: Perl 5.43.3 (96.51 MiB source, 6,816 files)
**Platform**: macOS ARM64 (M-series), 128GB RAM
**Build**: build-both-bench (Both formats enabled)

### Executive Summary

**FlatBuffers is production-ready** with excellent performance across all compression levels.

#### Key Findings

- **Compression Speed**: FlatBuffers is **17-29% faster** at compression levels 1-3
- **Extraction Speed**: **Equivalent** (within 3.4% margin)
- **Image Size**: **Minimal overhead** (0.07-1.41% larger than Thrift)
- **File Extensions**: Use `.dff` for FlatBuffers, `.dft` for Thrift

### Detailed Benchmark Results

#### Compression Performance

| Level | Format | Time (s) | Size (MB) | Speed vs Thrift | Size vs Thrift |
|-------|--------|----------|-----------|-----------------|----------------|
| **1** | FlatBuffers | **1.489** | 35.07 | **-28.9%** | +0.93% |
| **1** | Thrift | 2.095 | 34.75 | baseline | baseline |
| **3** | FlatBuffers | **2.999** | 26.39 | **-17.1%** | +1.41% |
| **3** | Thrift | 3.617 | 26.02 | baseline | baseline |
| **9** | FlatBuffers | 27.043 | 13.35 | +1.6% | +0.07% |
| **9** | Thrift | **26.606** | 13.35 | baseline | baseline |

#### Extraction Performance (Level 3)

| Format | Time (s) | Delta |
|--------|----------|-------|
| Thrift | **1.998** | baseline |
| FlatBuffers | 2.069 | +3.4% |

**Critical**: Both formats produce **byte-for-byte identical** extracted files.

### File Extension Convention

- **FlatBuffers**: Use `.dff` (DwarFS FlatBuffers)
- **Thrift**: Use `.dft` (DwarFS Thrift)
- **Generic**: `.dwarfs` (discouraged, format ambiguous)

**Example**:
```bash
# CORRECT
mkdwarfs -i src -o archive.dff --format=flatbuffers
mkdwarfs -i src -o archive.dft --format=thrift

# DISCOURAGED (works but unclear)
mkdwarfs -i src -o archive.dwarfs --format=flatbuffers
```

### Recommendations

**For Users**:
1. **Default to FlatBuffers** (.dff)
   - Faster image creation
   - Better portability
   - Minimal size overhead
   - Future-proof format

2. **Use Thrift (.dft) only for:**
   - Reading legacy images
   - Absolute minimum size requirement
   - Compatibility with older tools

**For Developers**:
1. **Build Configuration**
   - **FlatBuffers**: Always enabled (required)
   - **Thrift**: Optional (for backward compatibility)
   - Test both formats in CI/CD

---

## See Also

- [Building DwarFS](building.md) - Build instructions
- [Developer Guide](developer-guide.md) - Development workflow
- [Testing](testing.md) - Test suite documentation
- [DwarFS Format Reference](../reference/architecture.md) - Technical format details