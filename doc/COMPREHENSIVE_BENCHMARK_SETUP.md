# DwarFS Comprehensive Benchmark Setup

**Purpose**: Establish reusable benchmark infrastructure for image formats, compression algorithms, and platform testing

**Version**: v1.0  
**Date**: 2025-12-04

---

## Overview

### Objectives

1. **Format Comparison**: FlatBuffers vs Thrift across all build configurations
2. **Tool vs Library**: Compare CLI tools (mkdwarfs, dwarfsextract) vs libdwarfs API
3. **Comprehensive Operations**: Build, extract (full/single), read throughput, memory
4. **Extensibility**: Add new formats, algorithms, platforms without code changes
5. **Statistical Rigor**: Multiple runs, variance analysis, confidence intervals

### Benchmark Dimensions

```
┌─────────────────────────────────────────────────────────────┐
│                    Benchmark Matrix                         │
├─────────────┬───────────────┬────────────────┬──────────────┤
│ Format      │ Build Config  │ Operation      │ Interface    │
├─────────────┼───────────────┼────────────────┼──────────────┤
│ FlatBuffers │ FB-only       │ Create Image   │ CLI Tool     │
│ Thrift      │ Thrift-only   │ Extract Full   │ libdwarfs    │
│             │ Both          │ Extract Single │              │
│             │               │ Read Throughput│              │
│             │               │ Memory Usage   │              │
└─────────────┴───────────────┴────────────────┴──────────────┘
```

**Total Combinations**: 2 formats × 3 builds × 5 operations × 2 interfaces = **60 test scenarios**

---

## Benchmark Operations

### 1. Image Creation
**Metric**: Time to create DwarFS image from directory tree  
**Tool**: `mkdwarfs` (CLI) or `filesystem_writer` (API)  
**Measures**:
- Wall clock time
- CPU time (user + system)
- Peak memory (RSS)
- Filesystem size (bytes)
- Compression ratio

**Dataset Variations**:
- Small: 100 files, 10 MB total
- Medium: 10,000 files, 1 GB total
- Large: 100,000 files, 10 GB total

### 2. Full Extraction
**Metric**: Time to extract entire filesystem  
**Tool**: `dwarfsextract` (CLI) or `filesystem_extractor` (API)  
**Measures**:
- Extraction time
- Throughput (MB/s)
- Peak memory
- I/O operations
- Cache hits/misses

### 3. Single File Extraction
**Metric**: Time to extract one file from image  
**Tool**: `dwarfsextract --pattern` (CLI) or `filesystem_v2::find()` (API)  
**Measures**:
- Lookup time
- Read time
- Total time
- Metadata access overhead

**File Selection**:
- First file (best case - sequential)
- Middle file (typical case)
- Last file (worst case - full scan)

### 4. Random Read Throughput
**Metric**: MB/s for random file reads  
**Tool**: FUSE mount + `dd` (CLI) or `inode_reader::read()` (API)  
**Measures**:
- Sequential read throughput
- Random read throughput (4KB blocks)
- Random read throughput (1MB blocks)
- Latency (p50, p95, p99)

### 5. Memory Usage
**Metric**: Memory footprint during operations  
**Measures**:
- Metadata memory (string tables, inodes, chunks)
- Block cache memory
- Peak RSS
- Memory over time (leak detection)

---

## Build Configurations

### Config 1: FlatBuffers-only
```cmake
-DDWARFS_WITH_FLATBUFFERS=ON
-DDWARFS_WITH_THRIFT=OFF
```
**Tests**: FlatBuffers images only  
**Expected**: Cannot read Thrift images

### Config 2: Thrift-only
```cmake
-DDWARFS_WITH_FLATBUFFERS=OFF
-DDWARFS_WITH_THRIFT=ON
```
**Tests**: Thrift images only  
**Expected**: Cannot read FlatBuffers images

### Config 3: Both Formats
```cmake
-DDWARFS_WITH_FLATBUFFERS=ON
-DDWARFS_WITH_THRIFT=ON
```
**Tests**: Both image formats  
**Expected**: Auto-detect and read both

---

## Benchmark Datasets

### Dataset 1: Linux Kernel Source
**Characteristics**:
- ~70,000 files
- ~1 GB uncompressed
- Many small text files
- Deep directory trees
- Typical developer use case

**Source**: https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.tar.xz

### Dataset 2: Perl Installations (Available)
**Characteristics**:
- 1,139 Perl installations
- 47.65 GB uncompressed
- Extreme deduplication
- High compression ratio (0.88%)

**Location**: `benchmark_data/perl.tar` (if available)

### Dataset 3: DwarFS Source Code
**Characteristics**:
- ~2,000 source files
- Mix: C++, headers, CMake, documentation
- ~15 MB uncompressed
- Real-world C++ project structure

**Location**: Current repository (`/Users/mulgogi/src/external/dwarfs`)

---

## Tool vs Library Comparison

### CLI Tools
**Pros**:
- User-facing interface
- Complete workflow
- Shell integration

**Cons**:
- Process overhead
- Pipe communication
- Less control

**Tools**:
- `mkdwarfs` - Image creation
- `dwarfsextract` - Full extraction
- `dwarfs` (FUSE) - Random access

### libdwarfs API
**Pros**:
- Direct library calls
- No process overhead
- Fine-grained control
- Programmatic access

**Cons**:
- Requires C++ code
- More complex setup

**APIs**:
- `filesystem_writer` - Image creation
- `filesystem_extractor` - Extraction
- `filesystem_v2` - Random access

### Comparison Metrics
1. **Performance Delta**: Tool overhead (seconds)
2. **Memory Delta**: Process memory overhead (MB)
3. **Throughput**: MB/s difference
4. **Latency**: p99 latency difference

---

## Statistical Methodology

### Runs Per Test
- **Minimum**: 3 runs
- **Recommended**: 10 runs
- **Statistical**: 30 runs for distribution analysis

### Metrics Collected
- **Mean**: Average performance
- **Median**: Typical performance
- **StdDev**: Variance
- **Min/Max**: Best/worst case
- **Percentiles**: p50, p90, p95, p99

### Confidence Intervals
- Calculate 95% confidence intervals
- Report if variance >10%
- Flag unstable tests

### Outlier Detection
- Use IQR method (1.5 × IQR)
- Remove outliers (max 2 per dataset)
- Report outlier percentage

---

## Platform Variations

### Primary Platforms
1. **Linux x86_64** (Ubuntu 22.04)
   - Direct FUSE2/FUSE3
   - Server-grade CPU
   - Fast SSD

2. **macOS ARM64** (Apple Silicon)
   - FUSE-T (userspace)
   - Unified memory
   - Fast NVMe

3. **Windows x64**
   - WinFsp
   - NTFS filesystem
   - Various storage

### Secondary Platforms
4. **Linux ARM64** (Raspberry Pi)
   - Low-power CPU
   - SD card storage

5. **FreeBSD x86_64**
   - Linux emulation layer
   - Native FUSE

### Platform-Specific Metrics
- **CPU**: Instructions per second
- **Memory**: Page faults, RSS
- **I/O**: Block operations, cache behavior
- **Filesystem**: Native vs emulated overhead

---

## Benchmark Infrastructure

### Directory Structure
```
benchmarks/
├── extraction_benchmark.py       # Existing extraction tests
├── comprehensive_benchmark.py    # NEW: Full benchmark suite
├── lib/
│   ├── benchmark_executor.py    # Existing
│   ├── dataset_manager.py       # NEW: Dataset download/generation
│   ├── build_manager.py         # NEW: Manage build configs
│   ├── result_collector.py      # NEW: Collect all metrics
│   └── statistics.py            # NEW: Statistical analysis
└── configs/
    ├── datasets.yaml            # Dataset definitions
    ├── operations.yaml          # Operation definitions
    └── platforms.yaml           # Platform configurations
```

### Configuration Files

**datasets.yaml**:
```yaml
datasets:
  linux-kernel:
    name: "Linux Kernel Source"
    url: "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.6.tar.xz"
    size: 1073741824  # 1 GB
    files: 70000
    type: "source-code"
    
  perl-installs:
    name: "Perl Installations"
    path: "benchmark_data/perl.tar"
    size: 51103956992  # 47.65 GB
    files: 1139000
    type: "high-dedup"
```

**operations.yaml**:
```yaml
operations:
  create:
    cli: "mkdwarfs -i {input} -o {output}"
    api: "filesystem_writer.create()"
    metrics: [time, memory, compression_ratio]
    
  extract_full:
    cli: "dwarfsextract -i {input} -o {output}"
    api: "filesystem_extractor.extract()"
    metrics: [time, throughput, memory]
```

---

## Implementation Plan

### Phase 1: Infrastructure (4 hours)
1. **Build Manager** (1h)
   - Automated builds for 3 configurations
   - Dependency management
   - Version tracking

2. **Dataset Manager** (1h)
   - Download external datasets
   - Generate synthetic datasets
   - Validate checksums

3. **Result Collector** (1h)
   - Unified metric collection
   - JSON output format
   - Database integration (optional)

4. **Statistical Analysis** (1h)
   - Confidence intervals
   - Outlier detection
   - Regression analysis

### Phase 2: Benchmarks (6 hours)
1. **Image Creation** (1h)
   - CLI vs API comparison
   - Format comparison
   - Compression algorithm variants

2. **Full Extraction** (1h)
   - Sequential vs parallel
   - Cache effects
   - Memory profiling

3. **Single File** (1h)
   - Lookup performance
   - Metadata overhead
   - Cache warmup effects

4. **Random Read** (2h)
   - FUSE mount setup
   - dd/fio benchmarking
   - Latency distribution

5. **Memory Profiling** (1h)
   - String table analysis
   - Block cache behavior
   - Leak detection

### Phase 3: Validation (2 hours)
1. **Cross-Platform** (1h)
   - Run on Linux, macOS, Windows
   - Compare results
   - Document platform differences

2. **Regression Testing** (1h)
   - Compare against baselines
   - Flag significant changes
   - Update baselines

---

## Success Criteria

### Functional
- ✅ All 60 test scenarios execute successfully
- ✅ Both CLI and API interfaces tested
- ✅ All image formats readable by dual-format build
- ✅ Format detection works correctly

### Performance
- ✅ No regression >5% from baseline
- ✅ Tool overhead <10% vs API
- ✅ Throughput >100 MB/s for typical datasets
- ✅ Memory usage <100 MB for metadata

### Statistical
- ✅ Variance <20% across runs
- ✅ 95% confidence intervals computed
- ✅ Outliers <10% of runs
- ✅ Results reproducible across platforms

---

## Deliverables

### Code
1. `benchmarks/comprehensive_benchmark.py` - Main suite
2. `benchmarks/lib/dataset_manager.py` - Dataset handling
3. `benchmarks/lib/build_manager.py` - Build automation
4. `benchmarks/lib/statistics.py` - Statistical analysis

### Configuration
5. `benchmarks/configs/datasets.yaml`
6. `benchmarks/configs/operations.yaml`
7. `benchmarks/configs/platforms.yaml`

### Documentation
8. `doc/BENCHMARK_METHODOLOGY.md` - Detailed methodology
9. `doc/BENCHMARK_RESULTS_{VERSION}.md` - Results per version
10. `doc/PLATFORM_COMPARISON.md` - Cross-platform analysis

### Results
11. `benchmark-results/comprehensive/{VERSION}/` - All raw data
12. `benchmark-results/comprehensive/{VERSION}/summary.json` - Summary
13. `benchmark-results/comprehensive/{VERSION}/regression.json` - Regression report

---

## Usage Examples

### Run Full Suite
```bash
python3 benchmarks/comprehensive_benchmark.py \
  --builds fb-only,thrift-only,both \
  --datasets linux-kernel,perl-installs \
  --operations all \
  --runs 10
```

### Compare Formats Only
```bash
python3 benchmarks/comprehensive_benchmark.py \
  --builds both \
  --datasets linux-kernel \
  --operations create,extract_full \
  --compare-formats
```

### Memory Profiling
```bash
python3 benchmarks/comprehensive_benchmark.py \
  --builds fb-only \
  --datasets mixed-workload \
  --operations memory \
  --memory-profile
```

### Regression Check
```bash
python3 benchmarks/comprehensive_benchmark.py \
  --builds both \
  --datasets all \
  --operations all \
  --regression-check \
  --baseline v0.15.0
```

---

## Extensibility

### Adding New Image Format
1. Add format to `datasets.yaml`
2. Add build config to `build_manager.py`
3. No code changes needed - automatic discovery

### Adding New Compression Algorithm
1. Add algorithm to `operations.yaml` under `create`
2. Specify compression options
3. Run: `--operations create --compression lz4,zstd,brotli`

### Adding New Platform
1. Add platform to `platforms.yaml`
2. Set platform-specific paths/tools
3. CI/CD runs automatically

### Adding New Operation
1. Define in `operations.yaml`
2. Implement in `comprehensive_benchmark.py`
3. Add metrics to `result_collector.py`

---

## Timeline

### MVP (12 hours)
- Phase 1: Infrastructure (4h)
- Phase 2: Core benchmarks (6h)  
- Phase 3: Validation (2h)

### Complete (20 hours)
- MVP (12h)
- Platform testing (4h)
- Documentation (2h)
- Regression baselines (2h)

---

## Status

**Current**: Planning complete  
**Next**: Implement infrastructure (Phase 1)  
**ETA**: 12-20 hours for full suite