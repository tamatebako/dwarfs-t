# Benchmarking 3-Build Configurations - Continuation Prompt

**Date**: 2025-12-03  
**Status**: 🔄 **READY TO START**  
**Context**: Post-Pimpl fix, all builds compile successfully  
**Goal**: Validate functional equivalence and performance of FlatBuffers, Thrift, and Dual-format builds

---

## Quick Start

To begin benchmarking the 3 build configurations, follow this systematic approach:

### Prerequisites Check

1. **Verify all 3 builds exist and work**:
```bash
cd /Users/mulgogi/src/external/dwarfs

# Check FlatBuffers-only build
ls -lh build-fb/mkdwarfs build-fb/dwarfsck build-fb/dwarfsextract

# Check Thrift-only build (if exists)
ls -lh build-tb/mkdwarfs build-tb/dwarfsck build-tb/dwarfsextract

# Check Dual-format build (if exists)
ls -lh build-dual/mkdwarfs build-dual/dwarfsck build-dual/dwarfsextract
```

2. **If builds don't exist, create them**:
```bash
# Thrift-only build
cmake -B build-tb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=OFF
ninja -C build-tb mkdwarfs dwarfsck dwarfsextract

# Dual-format build
cmake -B build-dual -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=OFF
ninja -C build-dual mkdwarfs dwarfsck dwarfsextract
```

### Phase 1: Quick Validation Test (15 minutes)

**Objective**: Verify all 3 builds produce working images

```bash
# Create test directory
mkdir -p benchmark-results/quick-test

# Create small test data
mkdir -p /tmp/test-data
dd if=/dev/urandom of=/tmp/test-data/file1.bin bs=1M count=10
dd if=/dev/urandom of=/tmp/test-data/file2.bin bs=1M count=10
cp /tmp/test-data/file1.bin /tmp/test-data/file3.bin  # Duplicate for dedup test

# Test FlatBuffers-only build
./build-fb/mkdwarfs -i /tmp/test-data -o benchmark-results/quick-test/fb-test.dwarfs
./build-fb/dwarfsck benchmark-results/quick-test/fb-test.dwarfs
./build-fb/dwarfsextract -i benchmark-results/quick-test/fb-test.dwarfs -o /tmp/extract-fb

# Test Thrift-only build (if available)
./build-tb/mkdwarfs -i /tmp/test-data -o benchmark-results/quick-test/tb-test.dwarfs
./build-tb/dwarfsck benchmark-results/quick-test/tb-test.dwarfs
./build-tb/dwarfsextract -i benchmark-results/quick-test/tb-test.dwarfs -o /tmp/extract-tb

# Test Dual-format build (if available)
./build-dual/mkdwarfs -i /tmp/test-data -o benchmark-results/quick-test/dual-test.dwarfs
./build-dual/dwarfsck benchmark-results/quick-test/dual-test.dwarfs
./build-dual/dwarfsextract -i benchmark-results/quick-test/dual-test.dwarfs -o /tmp/extract-dual

# Compare extracted content
diff -r /tmp/test-data /tmp/extract-fb
diff -r /tmp/test-data /tmp/extract-tb
diff -r /tmp/test-data /tmp/extract-dual

# Compare image sizes
ls -lh benchmark-results/quick-test/*.dwarfs
```

**Expected Results**:
- ✅ All 3 builds create valid images
- ✅ All images pass integrity checks
- ✅ Extracted content matches original
- ✅ Image sizes are within 5-10% of each other

### Phase 2: Implement Benchmark Script (1 hour)

**Location**: Create `benchmarks/run_3build_comparison.py`

**Key Implementation Steps**:

1. **Start with basic structure**:
```python
#!/usr/bin/env python3
import subprocess
import json
import time
from pathlib import Path
from dataclasses import dataclass, asdict
import psutil

@dataclass
class BenchmarkResult:
    build: str
    dataset: str
    algorithm: str
    operation: str
    wall_time: float
    input_size: int
    output_size: int
    metadata_size: int
    throughput_mbs: float
    peak_memory_mb: float

class BuildBenchmark:
    def __init__(self, results_dir="benchmark-results"):
        self.results_dir = Path(results_dir)
        self.results_dir.mkdir(exist_ok=True)
        
        self.builds = {
            "fb": Path("build-fb"),
            "tb": Path("build-tb"),
            "dual": Path("build-dual"),
        }
        
    def run_mkdwarfs(self, build, input_dir, output_file, algo):
        """Run mkdwarfs and capture metrics"""
        mkdwarfs = self.builds[build] / "mkdwarfs"
        
        start_time = time.time()
        proc = psutil.Popen([
            str(mkdwarfs),
            "-i", str(input_dir),
            "-o", str(output_file),
            "--compression", algo,
            "--log-level", "info"
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        # Monitor memory usage
        peak_memory = 0
        while proc.poll() is None:
            try:
                peak_memory = max(peak_memory, proc.memory_info().rss / 1024 / 1024)
            except:
                pass
            time.sleep(0.1)
        
        stdout, stderr = proc.communicate()
        wall_time = time.time() - start_time
        
        # Get sizes
        input_size = sum(f.stat().st_size for f in Path(input_dir).rglob('*') if f.is_file())
        output_size = Path(output_file).stat().st_size
        
        return BenchmarkResult(
            build=build,
            dataset=Path(input_dir).name,
            algorithm=algo,
            operation="create",
            wall_time=wall_time,
            input_size=input_size,
            output_size=output_size,
            metadata_size=0,  # Extract from dwarfsck
            throughput_mbs=input_size / wall_time / 1024 / 1024,
            peak_memory_mb=peak_memory
        )
```

2. **Add methods for each operation**: check, extract, mount
3. **Implement test matrix iteration**
4. **Add JSON output and Markdown report generation**

### Phase 3: Run Comprehensive Benchmarks (2-3 hours)

**Test Matrix**:

```bash
# Run the benchmark script
python3 benchmarks/run_3build_comparison.py \
  --builds fb,tb,dual \
  --datasets tiny,perl \
  --algorithms lz4,zstd:level=3,zstd:level=22,lzma:level=9 \
  --operations create,check,extract \
  --output benchmark-results/3build-comparison-$(date +%Y%m%d).json
```

**Monitor Progress**:
- Watch `benchmark-results/` for intermediate results
- Check logs for errors
- Verify disk space (need ~100GB for full Perl benchmark)

### Phase 4: Generate Reports (30 minutes)

**Create comparison report**:

```bash
python3 benchmarks/generate_3build_report.py \
  --input benchmark-results/3build-comparison-*.json \
  --output benchmark-results/3BUILD_COMPARISON_REPORT.md
```

**Report should include**:
1. Executive summary with recommendations
2. Build configuration comparison table
3. Creation performance metrics
4. Metadata format size comparison
5. CLI command performance
6. Algorithm performance matrix
7. Visualizations (if matplotlib available)

### Phase 5: Validate Results (30 minutes)

**Validation Checklist**:

- [ ] All 3 builds produced valid images for all test cases
- [ ] Image sizes are within expected ranges (FlatBuffers ~102-109% of Thrift)
- [ ] All images pass integrity checks
- [ ] Performance is within 5% across builds (for same algorithm)
- [ ] Memory usage is reasonable (< 8GB for Perl dataset)
- [ ] Extracted content matches original for all cases
- [ ] Dual-format build can read both FlatBuffers and Thrift images
- [ ] Recompression works correctly (if tested)

---

## Troubleshooting

### Build Issues

**Problem**: Thrift-only build fails  
**Solution**: Verify Folly and fbthrift are available:
```bash
pkg-config --exists folly && echo "Folly OK" || echo "Folly missing"
```

**Problem**: Dual-format build fails  
**Solution**: Ensure both FlatBuffers and Thrift dependencies are available

### Benchmark Issues

**Problem**: Out of memory during large dataset test  
**Solution**: 
- Reduce `--memory-limit` in mkdwarfs
- Test with smaller dataset first
- Monitor with `htop` or Activity Monitor

**Problem**: Benchmark takes too long  
**Solution**:
- Start with tiny dataset only
- Reduce algorithm count
- Run in parallel (separate terminals)

**Problem**: Images fail integrity check  
**Solution**:
- Check build configuration
- Verify source data isn't corrupted
- Try different compression algorithm

---

## Expected Outcomes

### Performance Expectations

**Creation Time** (for Perl 47GB dataset):
- All builds: ~5-15 minutes (zstd:level=3)
- All builds: ~20-60 minutes (lzma:level=9)
- Variance between builds: < 5%

**Image Sizes**:
- FlatBuffers: 430-470 MiB (102-109% of Thrift baseline)
- Thrift: 431 MiB (baseline)
- Dual-format: Same as FlatBuffers (defaults to FlatBuffers)

**Memory Usage**:
- FlatBuffers: ~2-6 GB peak
- Thrift: ~2-6 GB peak
- Difference: < 10%

**Throughput**:
- All builds: 50-200 MB/s (depends on algorithm and CPU)
- CPU-bound for fast algorithms (lz4)
- I/O-bound for slower algorithms (lzma:9)

### Quality Metrics

**Functional**:
- ✅ 100% test cases pass integrity checks
- ✅ Bit-identical extraction across builds
- ✅ No crashes or hangs
- ✅ Proper error handling

**Non-Functional**:
- ✅ Reproducible results (±2%)
- ✅ Stable memory usage
- ✅ Clean logs (no warnings/errors)
- ✅ Cross-platform results (if applicable)

---

## Deliverables Checklist

- [ ] `benchmarks/run_3build_comparison.py` - Main benchmark script
- [ ] `benchmarks/generate_3build_report.py` - Report generator
- [ ] `benchmark-results/3build-raw-*.json` - Raw benchmark data
- [ ] `benchmark-results/3BUILD_COMPARISON_REPORT.md` - Summary report
- [ ] `benchmark-results/3BUILD_DETAILED_ANALYSIS.md` - Deep dive analysis
- [ ] `benchmark-results/charts/` - Visualization PNGs (if generated)
- [ ] Updated `README.md` or `README.DWARFS.md` with findings
- [ ] CI/CD job configuration (optional, for automation)

---

## Success Criteria

### Must Have ✅
1. All 3 builds create functionally equivalent images
2. FlatBuffers metadata ≤ 110% of Thrift size
3. Performance within 5% across builds
4. All images pass integrity checks
5. Comprehensive report generated

### Should Have 🎯
1. Tests with Perl dataset (47GB)
2. Multiple compression algorithms tested
3. CLI command benchmarks complete
4. Visualizations generated
5. CI/CD automation ready

### Nice to Have 💡
1. Sparse file handling tests
2. Hardlink/symlink-heavy tests
3. Cross-format recompression validation
4. Performance profiling with perfmon
5. Comparison with SquashFS or other filesystems

---

## Timeline

| Phase | Task | Duration | Status |
|-------|------|----------|--------|
| 0 | Build all configurations | 15 min | ✅ (build-fb done) |
| 1 | Quick validation test | 15 min | ⏸️ Pending |
| 2 | Implement benchmark script | 1 hour | ⏸️ Pending |
| 3 | Run comprehensive benchmarks | 2-3 hours | ⏸️ Pending |
| 4 | Generate reports | 30 min | ⏸️ Pending |
| 5 | Validate results | 30 min | ⏸️ Pending |

**Total Estimated**: 4.5-6 hours  
**Target Completion**: 2025-12-04

---

## Next Actions

1. **Review this continuation plan** ✅ 
2. **Approve benchmark parameters** ⏸️
3. **Build missing configurations** (Thrift-only, Dual-format)
4. **Execute Phase 1: Quick validation**
5. **Execute Phase 2: Implement script**
6. **Execute Phase 3: Run benchmarks**
7. **Execute Phase 4: Generate reports**
8. **Execute Phase 5: Validate & document**

---

**Status**: 📋 **READY FOR EXECUTION**  
**Priority**: HIGH - Release validation  
**Contact**: Ready to proceed with Phase 1