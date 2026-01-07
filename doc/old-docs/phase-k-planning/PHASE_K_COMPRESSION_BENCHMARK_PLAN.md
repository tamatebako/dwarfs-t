# Phase K: Comprehensive Compression Algorithm Benchmarking

**Date**: 2025-12-01  
**Status**: PLANNED  
**Priority**: HIGH  
**Estimated Time**: 8-12 hours

---

## Overview

Extend the benchmark suite to comprehensively test performance across:
- **All CLI tools**: mkdwarfs, dwarfsck, dwarfsextract, dwarfs (FUSE)
- **All compression algorithms**: zstd, lzma, lz4, brotli, flac, ricepp
- **All build configurations**: FlatBuffers-only, Thrift-only, Dual-format
- **Format comparison**: FlatBuffers metadata vs Thrift metadata performance
- **Integration**: Google Test via libdwarfs API

---

## Goals

### 1. Algorithm Performance Matrix
Create comprehensive benchmarks comparing:
- Compression ratio (%) across algorithms
- Compression speed (MB/s) across algorithms
- Decompression speed (MB/s) across algorithms
- Memory usage during compression/decompression
- Metadata overhead per algorithm

### 2. Format Performance Comparison
Test same data with different metadata formats:
- FlatBuffers metadata + each algorithm
- Thrift metadata + each algorithm
- Size difference between formats
- Speed difference between formats

### 3. Build Configuration Testing
Verify all algorithms work correctly in:
- **FB-only build**: FlatBuffers metadata, all algorithms
- **TB-only build**: Thrift metadata, all algorithms (if possible)
- **Dual build**: Both formats, all algorithms

### 4. CLI Tool Performance
Benchmark end-to-end workflows:
- `mkdwarfs`: Creation with each algorithm
- `dwarfsck`: Verification speed with each algorithm
- `dwarfsextract`: Extraction speed with each algorithm
- `dwarfs`: FUSE mount performance with each algorithm

---

## Implementation Plan

### K1: Google Test Framework Integration (2 hours)

**Create** `test/compression_algorithm_benchmark.cpp`:
```cpp
// Benchmark all compression algorithms via libdwarfs API
class CompressionAlgorithmBenchmark : public ::testing::Test {
protected:
  void SetUp() override;
  void TearDown() override;
  
  // Test datasets
  std::vector<TestDataset> datasets_;
  
  // Compression algorithms to test
  std::vector<CompressionAlgorithm> algorithms_ = {
    {"zstd", {1, 3, 5, 9, 19, 22}},    // levels
    {"lzma", {0, 3, 6, 9}},
    {"lz4", {1, 9}},
    {"lz4hc", {1, 9}},
    {"brotli", {1, 5, 9, 11}},
    {"flac", {0, 3, 5, 8}},           // requires PCM data
    {"ricepp", {128, 256, 512}}       // block sizes, requires FITS
  };
  
  // Metadata formats
  enum MetadataFormat { FlatBuffers, Thrift };
};

TEST_F(CompressionAlgorithmBenchmark, CompressionRatio) {
  // For each algorithm, for each level, measure compression ratio
}

TEST_F(CompressionAlgorithmBenchmark, CompressionSpeed) {
  // For each algorithm, for each level, measure MB/s
}

TEST_F(CompressionAlgorithmBenchmark, DecompressionSpeed) {
  // For each algorithm, measure decompression MB/s
}

TEST_F(CompressionAlgorithmBenchmark, MemoryUsage) {
  // Track peak memory during compression/decompression
}

TEST_F(CompressionAlgorithmBenchmark, MetadataOverhead) {
  // Measure metadata size for FlatBuffers vs Thrift
}

TEST_F(CompressionAlgorithmBenchmark, CrossFormatCompatibility) {
  // Create with one format, read with another
}
```

**Files**:
- `test/compression_algorithm_benchmark.cpp` (NEW)
- Update `cmake/tests.cmake` to include benchmark

**Dependencies**:
- libdwarfs API (writer, reader, compressor, decompressor)
- Google Benchmark integration (optional)
- Memory tracking utilities

---

### K2: Test Dataset Preparation (1 hour)

**Create test datasets** optimized for different algorithms:

1. **Generic Data** (zstd, lzma, lz4, brotli):
   - Text files (source code, logs)
   - Binary files (executables, archives)
   - Mixed data

2. **PCM Audio Data** (flac):
   - 16-bit stereo WAV files
   - 24-bit mono WAV files
   - Various sample rates (44.1kHz, 48kHz, 96kHz)

3. **FITS Images** (ricepp):
   - 16-bit astronomical images
   - Big-endian and little-endian variants

**Location**: `benchmark-files/compression-test/`

**Structure**:
```
benchmark-files/compression-test/
├── generic/
│   ├── source-code.tar     (1 MB, repetitive)
│   ├── logs.tar            (1 MB, highly repetitive)
│   ├── binary.tar          (1 MB, mixed)
│   └── random.bin          (1 MB, incompressible)
├── audio/
│   ├── pcm-16bit-stereo.wav   (10 MB)
│   ├── pcm-24bit-mono.wav     (10 MB)
│   └── pcm-96khz.wav          (10 MB)
└── fits/
    ├── image-16bit-be.fits    (5 MB)
    └── image-16bit-le.fits    (5 MB)
```

---

### K3: Python Benchmark Runner (3 hours)

**Create** `benchmarks/compression_algorithm_benchmark.py`:

```python
#!/usr/bin/env python3
"""
Comprehensive compression algorithm benchmark.

Tests all algorithms across all formats and builds.
"""

import json
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import List, Dict

@dataclass
class Algorithm:
    name: str
    levels: List[int]
    requires_metadata: bool = False

@dataclass
class TestCase:
    dataset: str
    algorithm: Algorithm
    level: int
    metadata_format: str  # "flatbuffers" or "thrift"

class CompressionBenchmark:
    def __init__(self, build_dirs: List[Path]):
        self.build_dirs = build_dirs
        self.algorithms = [
            Algorithm("zstd", [1, 3, 5, 9, 19, 22]),
            Algorithm("lzma", [0, 3, 6, 9]),
            Algorithm("lz4", [1, 9]),
            Algorithm("lz4hc", [1, 9]),
            Algorithm("brotli", [1, 5, 9, 11]),
            Algorithm("flac", [0, 3, 5, 8], requires_metadata=True),
            Algorithm("ricepp", [128, 256, 512], requires_metadata=True),
        ]
    
    def run_benchmark(self, test_case: TestCase) -> Dict:
        """Run single benchmark case."""
        # Create DwarFS image with specific algorithm
        # Measure: compression time, ratio, memory
        # Extract: decompression time, memory
        # Check: filesystem integrity
        pass
    
    def run_all_benchmarks(self) -> Dict:
        """Run all benchmark combinations."""
        results = {
            "flatbuffers_only": {},
            "thrift_only": {},
            "dual_format": {}
        }
        
        for build_name, build_dir in self.build_dirs.items():
            for dataset in self.get_datasets():
                for algorithm in self.algorithms:
                    for level in algorithm.levels:
                        # Test with FlatBuffers
                        test_fb = TestCase(dataset, algorithm, level, "flatbuffers")
                        results[build_name][f"{dataset}_{algorithm.name}_{level}_fb"] = \
                            self.run_benchmark(test_fb)
                        
                        # Test with Thrift (if available)
                        if self.supports_thrift(build_dir):
                            test_th = TestCase(dataset, algorithm, level, "thrift")
                            results[build_name][f"{dataset}_{algorithm.name}_{level}_th"] = \
                                self.run_benchmark(test_th)
        
        return results
    
    def generate_report(self, results: Dict) -> str:
        """Generate comprehensive markdown report."""
        pass

if __name__ == "__main__":
    builds = {
        "flatbuffers_only": Path("build-fb"),
        "dual_format": Path("build-dual"),
    }
    
    benchmark = CompressionBenchmark(builds)
    results = benchmark.run_all_benchmarks()
    report = benchmark.generate_report(results)
    
    Path("benchmark-results/compression-algorithms.json").write_text(
        json.dumps(results, indent=2)
    )
    Path("benchmark-results/COMPRESSION_ALGORITHMS_REPORT.md").write_text(report)
```

---

### K4: Report Generation (2 hours)

**Create comprehensive reports** showing:

1. **Algorithm Comparison Matrix**:
```markdown
| Algorithm | Level | Ratio (%) | Speed (MB/s) | Memory (MB) |
|-----------|-------|-----------|--------------|-------------|
| zstd      | 1     | 35.2      | 450          | 150         |
| zstd      | 3     | 32.1      | 380          | 180         |
| ...       | ...   | ...       | ...          | ...         |
| flac      | 5     | 42.0      | 85           | 90          |
| ricepp    | 128   | 55.0      | 120          | 60          |
```

2. **Format Overhead Comparison**:
```markdown
| Dataset | Algorithm | FB Size | TH Size | Overhead | Winner |
|---------|-----------|---------|---------|----------|--------|
| source  | zstd:5    | 1.2 MB  | 1.1 MB  | +8.3%    | Thrift |
| audio   | flac:5    | 4.5 MB  | 4.4 MB  | +2.3%    | Thrift |
| fits    | ricepp    | 2.8 MB  | 2.7 MB  | +3.7%    | Thrift |
```

3. **Build Configuration Results**:
```markdown
### FlatBuffers-Only Build
- ✅ All 6 algorithms functional
- ✅ FLAC categorizer works
- ✅ Rice++ categorizer works
- Metadata overhead: +5-10% vs Thrift

### Dual-Format Build
- ✅ Can read both formats
- ✅ Can write both formats
- Flexibility: Maximum
```

4. **CLI Tool Performance**:
```markdown
| Tool          | Dataset | Algorithm | Time (s) | Throughput |
|---------------|---------|-----------|----------|------------|
| mkdwarfs      | 100MB   | zstd:5    | 2.5      | 40 MB/s    |
| dwarfsck      | 100MB   | zstd:5    | 0.8      | 125 MB/s   |
| dwarfsextract | 100MB   | zstd:5    | 1.2      | 83 MB/s    |
| dwarfs (read) | 100MB   | zstd:5    | 0.3      | 333 MB/s   |
```

---

### K5: CI/CD Integration (1 hour)

**Add to** `.github/workflows/build.yml`:

```yaml
compression-benchmark:
  name: Compression Algorithm Benchmark
  runs-on: ubuntu-24.04
  
  steps:
    - uses: actions/checkout@v4
    
    # Build FlatBuffers-only
    - name: Build FB-only
      run: |
        cmake -B build-fb -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
        cmake --build build-fb
    
    # Build Dual-format
    - name: Build Dual
      run: |
        cmake -B build-dual -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
        cmake --build build-dual
    
    # Run benchmarks
    - name: Run Compression Benchmarks
      run: |
        python3 benchmarks/compression_algorithm_benchmark.py
    
    # Upload results
    - uses: actions/upload-artifact@v4
      with:
        name: compression-benchmark-results
        path: benchmark-results/compression-algorithms.*
```

---

## Success Criteria

Phase K will be complete when:

✅ **Test Suite**:
- Google Test framework benchmarks all 6 algorithms
- Tests run on FlatBuffers-only, Thrift-only, and Dual builds
- Memory tracking integrated
- Performance metrics collected

✅ **Python Runner**:
- Automates benchmark execution across all combinations
- Generates JSON results
- Creates markdown reports
- Handles errors gracefully

✅ **Results**:
- Algorithm comparison matrix complete
- Format overhead measured
- CLI tool performance documented
- CI/CD integration verified

✅ **Documentation**:
- Results published to `benchmark-results/`
- Interpretation guide included
- Recommendations for algorithm selection

---

## Timeline

| Task | Duration | Dependencies |
|------|----------|--------------|
| K1: Google Test Integration | 2h | - |
| K2: Dataset Preparation | 1h | - |
| K3: Python Runner | 3h | K1, K2 |
| K4: Report Generation | 2h | K3 |
| K5: CI/CD Integration | 1h | K3, K4 |

**Total**: 9 hours (compressed timeline)

---

## Next Steps

After Phase K completion, proceed to:
1. Archive Phase H/I/J/K documentation
2. Update README.md with vcpkg instructions
3. Final verification and release preparation

---

**Last Updated**: 2025-12-01 16:24 HKT  
**Status**: Ready to begin  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`