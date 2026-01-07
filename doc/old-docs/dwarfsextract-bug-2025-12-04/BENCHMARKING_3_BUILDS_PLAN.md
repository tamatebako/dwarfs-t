# Benchmarking Plan: 3-Build Configurations

**Date**: 2025-12-03  
**Status**: 🔄 **READY TO START**  
**Estimated Duration**: 4-6 hours  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`

---

## Objective

Comprehensively benchmark the **3 build configurations** against CLI commands, datasets, image formats, and compression algorithms to validate:
1. **Functional equivalence** across build types
2. **Performance characteristics** of each configuration
3. **Metadata format efficiency** (FlatBuffers vs Thrift)
4. **Production readiness** for v0.16.0 release

---

## Build Configurations to Test

### 1. FlatBuffers-Only Build
```bash
cmake -B build-fb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DCMAKE_BUILD_TYPE=Release
ninja -C build-fb
```

**Purpose**: Default/recommended build, no Thrift dependency  
**Metadata Format**: FlatBuffers only  
**Tag**: `[FB]`

### 2. Thrift-Only Build  
```bash
cmake -B build-tb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DCMAKE_BUILD_TYPE=Release
ninja -C build-tb
```

**Purpose**: Legacy compatibility, Folly/fbthrift required  
**Metadata Format**: Thrift Compact only  
**Tag**: `[TB]`

### 3. Dual-Format Build
```bash
cmake -B build-dual -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DCMAKE_BUILD_TYPE=Release
ninja -C build-dual
```

**Purpose**: Maximum compatibility, can read/write both formats  
**Metadata Format**: FlatBuffers (default), Thrift (optional)  
**Tag**: `[DUAL]`

---

## Test Datasets

### Small Datasets (for quick validation)
1. **Tiny** (11.5 MiB) - From existing benchmarks
   - Path: `benchmark-files/tiny/`
   - Use: Quick smoke tests

### Medium Datasets
2. **Perl** (47.65 GiB → 430.9 MiB with Thrift)
   - Path: `benchmark-files/perl/` or download script
   - Use: Comprehensive deduplication testing
   - Known compression ratio: 110:1

### Large Datasets (if time permits)
3. **Linux Kernel Sources** (multiple versions)
   - Use: Cross-version deduplication
4. **Software Build Trees** (with binaries)
   - Use: Mixed content testing

---

## Compression Algorithms to Test

Test matrix for each dataset/build combination:

### Fast Compression
- `null` - No compression baseline
- `lz4` - Fastest
- `lz4hc:level=9` - High compression LZ4

### Balanced Compression  
- `zstd:level=3` - Default (fast + good ratio)
- `zstd:level=9` - Balanced
- `lzma:level=1` - Fast LZMA

### Maximum Compression
- `zstd:level=22` - Maximum zstd
- `lzma:level=9` - Maximum LZMA
- `brotli:quality=11` - Maximum Brotli

### Specialty Algorithms
- `flac` - For PCM audio (if test data available)
- `ricepp` - For FITS images (if test data available)

---

## CLI Commands to Benchmark

### 1. Creation (`mkdwarfs`)

**Test Matrix**:
```bash
for build in fb tb dual; do
  for dataset in tiny perl; do
    for algo in lz4 zstd:level=3 zstd:level=22 lzma:level=9; do
      ./build-${build}/mkdwarfs \
        -i benchmark-files/${dataset}/ \
        -o results/${build}-${dataset}-${algo}.dwarfs \
        --compression ${algo} \
        --log-level=info \
        2>&1 | tee results/${build}-${dataset}-${algo}.create.log
    done
  done
done
```

**Metrics to Capture**:
- Wall clock time
- CPU time (user + system)
- Peak memory usage
- Input size
- Output size (image file)
- Compression ratio
- Throughput (MB/s)
- Metadata format used
- Metadata size

### 2. Inspection (`dwarfsck`)

```bash
for image in results/*.dwarfs; do
  build=$(echo $image | cut -d- -f1)
  ./build-${build}/dwarfsck ${image} --json > ${image}.check.json
  ./build-${build}/dwarfsck ${image} --checksum xxhash --verify-integrity
done
```

**Metrics to Capture**:
- Check time
- Metadata format detected
- Integrity verification result
- File count
- Total size
- Metadata size

### 3. Extraction (`dwarfsextract`)

```bash
for image in results/*.dwarfs; do
  build=$(echo $image | cut -d- -f1)
  time ./build-${build}/dwarfsextract \
    -i ${image} \
    -o /tmp/extract-${build}/ \
    --num-workers=8
done
```

**Metrics to Capture**:
- Extraction time
- Throughput (MB/s)
- CPU usage
- Memory usage

### 4. Mounting (`dwarfs`)

```bash
for image in results/*.dwarfs; do
  build=$(echo $image | cut -d- -f1)
  ./build-${build}/dwarfs ${image} /tmp/mnt-${build}/ &
  
  # Random file access test
  time find /tmp/mnt-${build}/ -type f | head -100 | xargs -n1 wc -l
  
  # Sequential read test  
  time tar cf /dev/null /tmp/mnt-${build}/
  
  umount /tmp/mnt-${build}/
done
```

**Metrics to Capture**:
- Mount time
- Random access latency
- Sequential read throughput
- Memory usage during mount
- Cache effectiveness

### 5. Recompression (`mkdwarfs --recompress`)

```bash
# Only test on dual-format build
for src_format in fb tb; do
  for dst_format in fb tb; do
    ./build-dual/mkdwarfs \
      --recompress \
      -i results/dual-perl-zstd.dwarfs \
      -o results/recompress-${src_format}-to-${dst_format}.dwarfs \
      --input-metadata-format=${src_format} \
      --output-metadata-format=${dst_format}
  done
done
```

**Metrics to Capture**:
- Recompression time
- Size change (if any)
- Metadata format conversion success

---

## Image Format Testing

### Test Image Types

1. **Standard Images**
   - Regular filesystem images (default)
   - Test all compression algorithms

2. **Sparse Images** (if test data available)
   - Images with large holes
   - Test hole compression efficiency

3. **Hardlink-Heavy Images**
   - Multiple hardlinks to same inode
   - Test deduplication effectiveness

4. **Symlink-Heavy Images**
   - Many symbolic links
   - Test symlink table packing

---

## Benchmarking Script

Location: [`benchmarks/run_3build_comparison.py`](../benchmarks/run_3build_comparison.py)

### Script Structure

```python
#!/usr/bin/env python3
"""
Comprehensive 3-build configuration benchmarking.
Tests FlatBuffers-only, Thrift-only, and Dual-format builds.
"""

import subprocess
import json
import time
from pathlib import Path
from dataclasses import dataclass
import psutil

@dataclass
class BuildConfig:
    name: str
    build_dir: Path
    metadata_format: str
    tag: str

@dataclass
class BenchmarkResult:
    build: str
    dataset: str
    algorithm: str
    operation: str  # create/check/extract/mount/recompress
    
    # Timing
    wall_time: float
    cpu_time: float
    
    # Sizes
    input_size: int
    output_size: int
    metadata_size: int
    
    # Performance
    throughput_mbs: float
    compression_ratio: float
    
    # Resources
    peak_memory_mb: float
    cpu_percent: float

class BuildBenchmark:
    def __init__(self):
        self.builds = [
            BuildConfig("fb", Path("build-fb"), "flatbuffers", "[FB]"),
            BuildConfig("tb", Path("build-tb"), "thrift", "[TB]"),
            BuildConfig("dual", Path("build-dual"), "both", "[DUAL]"),
        ]
        self.datasets = ["tiny", "perl"]
        self.algorithms = [
            "lz4",
            "zstd:level=3",
            "zstd:level=22",
            "lzma:level=9",
        ]
        
    def benchmark_create(self, build, dataset, algo):
        """Benchmark filesystem creation"""
        # Implementation
        pass
        
    def benchmark_check(self, build, image):
        """Benchmark integrity checking"""
        pass
        
    def benchmark_extract(self, build, image):
        """Benchmark extraction"""
        pass
        
    def benchmark_mount(self, build, image):
        """Benchmark FUSE mounting"""
        pass
        
    def benchmark_recompress(self, src_fmt, dst_fmt):
        """Benchmark metadata recompression"""
        pass
        
    def run_all(self):
        """Execute full benchmark suite"""
        results = []
        
        # Creation benchmarks
        for build in self.builds:
            for dataset in self.datasets:
                for algo in self.algorithms:
                    result = self.benchmark_create(build, dataset, algo)
                    results.append(result)
        
        # Check benchmarks
        # Extract benchmarks
        # Mount benchmarks
        # Recompress benchmarks (dual-format only)
        
        return results
        
    def generate_report(self, results):
        """Generate comprehensive comparison report"""
        # Markdown report with tables and charts
        pass

if __name__ == "__main__":
    benchmark = BuildBenchmark()
    results = benchmark.run_all()
    benchmark.generate_report(results)
```

---

## Report Generation

### Output Files

1. **Raw Results** (JSON)
   - `results/3build-benchmark-raw.json`
   - All measurements in structured format

2. **Summary Report** (Markdown)
   - `results/3BUILD_COMPARISON_REPORT.md`
   - Tables comparing all metrics across builds

3. **Detailed Analysis** (Markdown)
   - `results/3BUILD_DETAILED_ANALYSIS.md`
   - Per-algorithm, per-dataset deep dive

4. **Visualizations** (PNG)
   - `results/charts/*.png`
   - Bar charts, spider charts for key metrics

### Report Sections

#### 1. Executive Summary
- High-level findings
- Recommendation: Which build to use when
- Performance highlights

#### 2. Build Configuration Comparison
- Dependencies required
- Build time
- Binary sizes
- Feature matrix

#### 3. Creation Performance
| Build | Dataset | Algorithm | Time (s) | Size (MB) | Ratio | Throughput (MB/s) | Memory (MB) |
|-------|---------|-----------|----------|-----------|-------|-------------------|-------------|
| [FB]  | tiny    | zstd:3    | 1.2      | 2.5       | 4.6x  | 9.5               | 120         |
| [TB]  | tiny    | zstd:3    | 1.3      | 2.4       | 4.8x  | 8.8               | 125         |
| [DUAL]| tiny    | zstd:3    | 1.2      | 2.5       | 4.6x  | 9.5               | 122         |

#### 4. Metadata Format Analysis
- **Size Comparison**: FlatBuffers vs Thrift
- **Access Performance**: Zero-copy vs packed
- **Compatibility**: Cross-format reading

#### 5. Algorithm Performance Matrix
- Heatmap of compression ratio by algorithm/dataset/build
- Speed vs ratio tradeoff charts

#### 6. CLI Command Performance
- Per-command timing analysis
- Resource usage patterns

#### 7. Recommendations
- **Default**: FlatBuffers-only (best portability)
- **Legacy**: Thrift-only (backward compatibility)
- **Migration**: Dual-format (transition period)

---

## Success Criteria

### Functional Requirements
- ✅ All 3 builds produce valid images
- ✅ All images pass integrity checks
- ✅ All images mount successfully
- ✅ Extracted content matches original
- ✅ Dual-format can read both FlatBuffers and Thrift images
- ✅ Recompression works in both directions

### Performance Requirements
- ✅ FlatBuffers within 5% of Thrift performance
- ✅ FlatBuffers metadata ≤ 110% of Thrift size (target met: 102-109%)
- ✅ No degradation in compression ratios
- ✅ Memory usage within acceptable bounds

### Quality Requirements
- ✅ Comprehensive test coverage
- ✅ Reproducible results
- ✅ Clear documentation
- ✅ CI/CD integration ready

---

## Timeline

### Phase 1: Setup (30 minutes)
- Build all 3 configurations
- Download/prepare test datasets
- Create benchmark script skeleton

### Phase 2: Creation Benchmarks (1.5 hours)
- Test all build/dataset/algorithm combinations
- Capture all metrics
- Generate intermediate results

### Phase 3: CLI Command Benchmarks (1.5 hours)
- Check, extract, mount tests
- Recompression tests (dual-format)
- Performance profiling

### Phase 4: Analysis & Reporting (1-1.5 hours)
- Generate comparison tables
- Create visualizations
- Write detailed analysis

### Phase 5: Documentation (30 minutes)
- Update README with findings
- Document recommended configurations
- Add troubleshooting guide

**Total Estimated Time**: 4-6 hours

---

## Potential Issues

### 1. Build Failures
- **Mitigation**: Already validated all 3 builds compile successfully
- **Fallback**: Use existing binaries if rebuild fails

### 2. Dataset Availability
- **Mitigation**: Start with tiny dataset, Perl if available
- **Fallback**: Generate synthetic test data

### 3. Long Benchmark Times
- **Mitigation**: Run in parallel where possible
- **Timeout**: Set 2-hour max per test

### 4. Disk Space
- **Requirement**: ~100 GB for Perl dataset + results
- **Mitigation**: Clean up intermediate files

---

## Deliverables

1. ✅ [`benchmarks/run_3build_comparison.py`](../benchmarks/run_3build_comparison.py) - Benchmark script
2. ✅ `results/3build-benchmark-raw.json` - Raw data
3. ✅ `results/3BUILD_COMPARISON_REPORT.md` - Summary report
4. ✅ `results/3BUILD_DETAILED_ANALYSIS.md` - Deep dive
5. ✅ `results/charts/*.png` - Visualizations
6. ✅ `README.md` updates - Integration guide
7. ✅ CI/CD job - Automated benchmarking

---

## Next Steps

1. Review this plan
2. Approve benchmark parameters
3. Execute Phase 1 (Setup)
4. Begin systematic benchmarking
5. Generate reports
6. Update documentation
7. Prepare for v0.16.0 release

---

**Status**: 📋 **READY FOR EXECUTION**  
**Estimated Completion**: 2025-12-03 EOD or 2025-12-04  
**Priority**: HIGH - Release blocker validation