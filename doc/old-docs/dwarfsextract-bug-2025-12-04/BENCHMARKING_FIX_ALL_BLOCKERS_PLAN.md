# Fix All Blockers & Complete 3-Build Benchmarking - Continuation Plan

**Date**: 2025-12-03  
**Status**: 🔴 **ACTIVE - FIX ALL BLOCKERS**  
**Objective**: Fix dwarfsextract crash + create missing Thrift header → Complete full 3-build benchmarking

---

## Critical Blockers Identified

### Blocker #1: dwarfsextract Crash (Segfault)
**Status**: 🔴 **CRITICAL**  
**Impact**: Cannot extract images for verification  
**Root Cause**: Unknown - needs debugging

### Blocker #2: Missing Thrift Header
**Status**: 🔴 **CRITICAL**  
**Impact**: Cannot build Thrift/dual-format configurations  
**File**: `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h`

---

## Fix Strategy

### Phase 1: Debug & Fix dwarfsextract (2-4 hours)

**Objective**: Make extraction work reliably

**Step 1.1: Isolate the Crash**
- [ ] Run dwarfsextract under lldb debugger
- [ ] Capture full stack trace
- [ ] Identify crashing function/line
- [ ] Determine if regression or platform-specific

**Step 1.2: Test Minimal Case**
- [ ] Create simplest possible image
- [ ] Try extraction with minimal options
- [ ] Test different output paths
- [ ] Verify libarchive compatibility

**Step 1.3: Review Recent Changes**
- [ ] Check git log for recent dwarfsextract changes
- [ ] Review filesystem_extractor.cpp modifications
- [ ] Check for Pimpl-related changes that might affect extraction

**Step 1.4: Apply Fix**
- [ ] Implement fix based on root cause
- [ ] Test extraction works for all cases
- [ ] Verify no memory leaks (valgrind if available)
- [ ] Test on multiple platforms if possible

**Deliverables**:
- Working dwarfsextract tool
- Test cases demonstrating extraction works
- Documentation of fix in commit message

---

### Phase 2: Create Missing Thrift Header (1-2 hours)

**Objective**: Enable Thrift/dual-format builds

**Step 2.1: Extract Class Declaration**

Create `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h`:

```cpp
#pragma once

#include <dwarfs/writer/internal/metadata_builder.h>
#include <dwarfs/logger.h>
// ... other necessary includes

namespace dwarfs::writer::internal {

template <typename LoggerPolicy>
class thrift_metadata_builder final : public metadata_builder::impl {
 public:
  using uid_type = file_stat::uid_type;
  using gid_type = file_stat::gid_type;

  // Constructors (declarations only)
  thrift_metadata_builder(logger& lgr, metadata_options const& options);
  
  template <typename T>
    requires(std::same_as<std::decay_t<T>, thrift::metadata::metadata>)
  thrift_metadata_builder(logger& lgr, T&& md,
                         thrift::metadata::fs_options const* orig_fs_options,
                         filesystem_version const& orig_fs_version,
                         metadata_options const& options);

  ~thrift_metadata_builder();

  // Implement metadata_builder::impl interface
  void set_devices(std::vector<uint64_t> devices) override;
  // ... all other interface methods
  
 private:
  // Private members (documented)
  LOG_PROXY_DECL(LoggerPolicy);
  // ... rest of implementation details
};

} // namespace dwarfs::writer::internal
```

**Step 2.2: Update thrift_metadata_builder.cpp**
- [ ] Remove class from anonymous namespace
- [ ] Keep implementation in .cpp
- [ ] Include new header file
- [ ] Ensure template instantiations work

**Step 2.3: Update Dependent Files**
- [ ] Fix `src/writer/internal/metadata_builder.cpp` includes
- [ ] Fix `src/writer/internal/metadata_builder_factory.cpp` includes
- [ ] Verify conditional compilation works

**Step 2.4: Test Thrift Build**
```bash
cmake -B build-tb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=OFF

ninja -C build-tb mkdwarfs dwarfsck dwarfsextract
```

**Deliverables**:
- `thrift_metadata_builder_impl.h` header file
- Working Thrift-only build
- Working dual-format build

---

### Phase 3: Build All 3 Configurations (30 minutes)

**Step 3.1: Clean Builds**
```bash
# FlatBuffers-only (already exists)
ls build-fb/mkdwarfs build-fb/dwarfsck build-fb/dwarfsextract

# Thrift-only (rebuild)
rm -rf build-tb
cmake -B build-tb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=OFF \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=ON \
  -DWITH_FUSE_DRIVER=OFF

ninja -C build-tb mkdwarfs dwarfsck dwarfsextract

# Dual-format (new)
rm -rf build-dual
cmake -B build-dual -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=OFF \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=ON \
  -DWITH_FUSE_DRIVER=OFF

ninja -C build-dual mkdwarfs dwarfsck dwarfsextract
```

**Step 3.2: Verify All Builds**
- [ ] All 3 builds produce working binaries
- [ ] Tool versions match expected
- [ ] No missing library errors

**Deliverables**:
- 3 complete build directories
- Binary size comparison table
- Build configuration documentation

---

### Phase 4: Quick Validation Test (15 minutes)

**Objective**: Verify all 3 builds work end-to-end

**Step 4.1: Create Test Data**
```bash
mkdir -p /tmp/test-data
dd if=/dev/urandom of=/tmp/test-data/file1.bin bs=1M count=10
dd if=/dev/urandom of=/tmp/test-data/file2.bin bs=1M count=10
cp /tmp/test-data/file1.bin /tmp/test-data/file3.bin
```

**Step 4.2: Test Each Build**
```bash
# FlatBuffers-only
./build-fb/mkdwarfs -i /tmp/test-data -o /tmp/fb-test.dwarfs
./build-fb/dwarfsck /tmp/fb-test.dwarfs
./build-fb/dwarfsextract -i /tmp/fb-test.dwarfs -o /tmp/extract-fb
diff -r /tmp/test-data /tmp/extract-fb

# Thrift-only  
./build-tb/mkdwarfs -i /tmp/test-data -o /tmp/tb-test.dwarfs
./build-tb/dwarfsck /tmp/tb-test.dwarfs
./build-tb/dwarfsextract -i /tmp/tb-test.dwarfs -o /tmp/extract-tb
diff -r /tmp/test-data /tmp/extract-tb

# Dual-format (FlatBuffers default)
./build-dual/mkdwarfs -i /tmp/test-data -o /tmp/dual-test.dwarfs
./build-dual/dwarfsck /tmp/dual-test.dwarfs
./build-dual/dwarfsextract -i /tmp/dual-test.dwarfs -o /tmp/extract-dual
diff -r /tmp/test-data /tmp/extract-dual
```

**Step 4.3: Cross-Format Testing**
```bash
# Dual build should read both formats
./build-dual/dwarfsck /tmp/fb-test.dwarfs   # FlatBuffers image
./build-dual/dwarfsck /tmp/tb-test.dwarfs   # Thrift image

# FlatBuffers-only should reject Thrift
./build-fb/dwarfsck /tmp/tb-test.dwarfs     # Expected: error
```

**Validation Checklist**:
- [ ] All 3 builds create valid images
- [ ] All images pass integrity checks  
- [ ] Extraction produces identical content
- [ ] Image sizes within expected ranges
- [ ] Dual build reads both formats
- [ ] Format detection works correctly

---

### Phase 5: Implement Benchmark Script (1-2 hours)

**Objective**: Automate comprehensive benchmarking

**Step 5.1: Create Benchmark Runner**

File: `benchmarks/run_3build_comparison.py`

```python
#!/usr/bin/env python3
"""
3-Build Configuration Benchmark Runner

Tests FlatBuffers-only, Thrift-only, and Dual-format builds across:
- Multiple datasets (tiny, perl)
- Multiple algorithms (lz4, zstd:3, zstd:22, lzma:9)
- All operations (create, check, extract)
"""

import subprocess
import json
import time
import argparse
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
    exit_code: int
    error_message: str = ""

class BuildBenchmark:
    def __init__(self, results_dir="benchmark-results"):
        self.results_dir = Path(results_dir)
        self.results_dir.mkdir(exist_ok=True)
        
        self.builds = {
            "fb": Path("build-fb"),
            "tb": Path("build-tb"),
            "dual": Path("build-dual"),
        }
        
        self.results = []
        
    def run_mkdwarfs(self, build, input_dir, output_file, algo):
        """Run mkdwarfs and capture metrics"""
        mkdwarfs = self.builds[build] / "mkdwarfs"
        
        start_time = time.time()
        proc = psutil.Popen([
            str(mkdwarfs),
            "-i", str(input_dir),
            "-o", str(output_file),
            "--compression", algo,
            "--log-level", "error"
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        peak_memory = 0
        while proc.poll() is None:
            try:
                peak_memory = max(peak_memory, proc.memory_info().rss / 1024 / 1024)
            except:
                pass
            time.sleep(0.1)
        
        stdout, stderr = proc.communicate()
        wall_time = time.time() - start_time
        
        input_size = sum(f.stat().st_size for f in Path(input_dir).rglob('*') if f.is_file())
        output_size = Path(output_file).stat().st_size if Path(output_file).exists() else 0
        
        return BenchmarkResult(
            build=build,
            dataset=Path(input_dir).name,
            algorithm=algo,
            operation="create",
            wall_time=wall_time,
            input_size=input_size,
            output_size=output_size,
            metadata_size=0,
            throughput_mbs=input_size / wall_time / 1024 / 1024 if wall_time > 0 else 0,
            peak_memory_mb=peak_memory,
            exit_code=proc.returncode,
            error_message=stderr.decode() if proc.returncode != 0 else ""
        )
        
    def run_dwarfsck(self, build, image_file):
        """Run dwarfsck and capture metrics"""
        dwarfsck = self.builds[build] / "dwarfsck"
        
        start_time = time.time()
        result = subprocess.run([
            str(dwarfsck),
            str(image_file),
            "--json"
        ], capture_output=True)
        wall_time = time.time() - start_time
        
        metadata_size = 0
        if result.returncode == 0:
            try:
                data = json.loads(result.stdout)
                # Extract metadata size if available
            except:
                pass
        
        return BenchmarkResult(
            build=build,
            dataset="",
            algorithm="",
            operation="check",
            wall_time=wall_time,
            input_size=Path(image_file).stat().st_size,
            output_size=0,
            metadata_size=metadata_size,
            throughput_mbs=0,
            peak_memory_mb=0,
            exit_code=result.returncode,
            error_message=result.stderr.decode() if result.returncode != 0 else ""
        )
        
    def run_dwarfsextract(self, build, image_file, output_dir):
        """Run dwarfsextract and capture metrics"""
        dwarfsextract = self.builds[build] / "dwarfsextract"
        
        start_time = time.time()
        proc = psutil.Popen([
            str(dwarfsextract),
            "-i", str(image_file),
            "-o", str(output_dir)
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        peak_memory = 0
        while proc.poll() is None:
            try:
                peak_memory = max(peak_memory, proc.memory_info().rss / 1024 / 1024)
            except:
                pass
            time.sleep(0.1)
        
        stdout, stderr = proc.communicate()
        wall_time = time.time() - start_time
        
        output_size = sum(f.stat().st_size for f in Path(output_dir).rglob('*') if f.is_file()) if Path(output_dir).exists() else 0
        
        return BenchmarkResult(
            build=build,
            dataset="",
            algorithm="",
            operation="extract",
            wall_time=wall_time,
            input_size=Path(image_file).stat().st_size,
            output_size=output_size,
            metadata_size=0,
            throughput_mbs=output_size / wall_time / 1024 / 1024 if wall_time > 0 else 0,
            peak_memory_mb=peak_memory,
            exit_code=proc.returncode,
            error_message=stderr.decode() if proc.returncode != 0 else ""
        )
        
    def run_benchmark(self, builds, datasets, algorithms):
        """Run complete benchmark matrix"""
        for build in builds:
            for dataset in datasets:
                for algo in algorithms:
                    print(f"Testing {build}/{dataset}/{algo}...")
                    
                    # Create image
                    output_file = self.results_dir / f"{build}-{dataset}-{algo.replace(':', '-')}.dwarfs"
                    result = self.run_mkdwarfs(build, dataset, output_file, algo)
                    self.results.append(result)
                    
                    if result.exit_code == 0:
                        # Check image
                        result = self.run_dwarfsck(build, output_file)
                        self.results.append(result)
                        
                        # Extract image
                        extract_dir = self.results_dir / f"extract-{build}-{dataset}-{algo.replace(':', '-')}"
                        result = self.run_dwarfsextract(build, output_file, extract_dir)
                        self.results.append(result)
                        
    def save_results(self, output_file):
        """Save results as JSON"""
        with open(output_file, 'w') as f:
            json.dump([asdict(r) for r in self.results], f, indent=2)
            
def main():
    parser = argparse.ArgumentParser(description='3-Build Benchmark Runner')
    parser.add_argument('--builds', default='fb,tb,dual', help='Comma-separated build names')
    parser.add_argument('--datasets', default='tiny', help='Comma-separated dataset paths')
    parser.add_argument('--algorithms', default='lz4,zstd:level=3', help='Comma-separated algorithms')
    parser.add_argument('--output', required=True, help='Output JSON file')
    args = parser.parse_args()
    
    benchmark = BuildBenchmark()
    benchmark.run_benchmark(
        args.builds.split(','),
        args.datasets.split(','),
        args.algorithms.split(',')
    )
    benchmark.save_results(args.output)
    print(f"Results saved to {args.output}")

if __name__ == '__main__':
    main()
```

**Step 5.2: Create Report Generator**

File: `benchmarks/generate_3build_report.py`

```python
#!/usr/bin/env python3
"""Generate comparison report from benchmark results"""

import json
import sys
from pathlib import Path

def generate_report(results_file, output_file):
    with open(results_file) as f:
        results = json.load(f)
    
    with open(output_file, 'w') as f:
        f.write("# 3-Build Configuration Comparison Report\n\n")
        
        # Executive summary
        f.write("## Executive Summary\n\n")
        
        # Performance comparison
        f.write("## Creation Performance\n\n")
        
        # Size comparison
        f.write("## Image Size Comparison\n\n")
        
        # Extract performance
        f.write("## Extraction Performance\n\n")
        
        # Memory usage
        f.write("## Memory Usage\n\n")
        
        # Recommendations
        f.write("## Recommendations\n\n")

if __name__ == '__main__':
    generate_report(sys.argv[1], sys.argv[2])
```

---

### Phase 6: Run Comprehensive Benchmarks (2-3 hours)

**Step 6.1: Prepare Datasets**
```bash
# Tiny dataset (quick test)
mkdir -p benchmark-files/tiny
# ... create test files

# Perl dataset (comprehensive)
# Use existing perl dataset if available
```

**Step 6.2: Run Benchmark Matrix**
```bash
python3 benchmarks/run_3build_comparison.py \
  --builds fb,tb,dual \
  --datasets benchmark-files/tiny,benchmark-files/perl \
  --algorithms lz4,zstd:level=3,zstd:level=22,lzma:level=9 \
  --output benchmark-results/3build-comparison-$(date +%Y%m%d).json
```

**Step 6.3: Generate Reports**
```bash
python3 benchmarks/generate_3build_report.py \
  benchmark-results/3build-comparison-*.json \
  benchmark-results/3BUILD_COMPARISON_REPORT.md
```

**Expected Metrics**:
- Creation time per build/algorithm
- Image sizes (FlatBuffers vs Thrift)  
- Extraction time
- Memory usage
- Throughput rates

---

### Phase 7: Validate & Document (1 hour)

**Step 7.1: Validation Checklist**
- [ ] All 3 builds produce valid images
- [ ] Image sizes: FlatBuffers ~102-109% of Thrift
- [ ] Performance within 5% across builds
- [ ] All images pass integrity checks
- [ ] Extraction verified for all cases
- [ ] Dual build reads both formats
- [ ] No crashes or hangs

**Step 7.2: Update Documentation**

Update `README.md` or `README.DWARFS.md`:
```markdown
## Metadata Formats

DwarFS supports two metadata serialization formats:

- **FlatBuffers** (default): Modern format (v24.3.25)
  - Memory-mappable, zero-copy access
  - Excellent portability
  - ~102-109% size of Thrift
  
- **Thrift Compact** (legacy): For backward compatibility
  - Smallest format (bit-packed)
  - Requires Folly + fbthrift
  - Legacy images only

### Build Configurations

- **FlatBuffers-only**: Default, works on all platforms
- **Dual-format**: Supports both formats (backward compatible)
- **Thrift-only**: Not recommended (FlatBuffers preferred)
```

**Step 7.3: Move Old Documentation**
```bash
mkdir -p doc/old-docs/benchmarking-2025-12
mv doc/BENCHMARKING_3_BUILDS_CONTINUATION.md doc/old-docs/benchmarking-2025-12/
mv doc/BENCHMARKING_3_BUILDS_STATUS.md doc/old-docs/benchmarking-2025-12/
# Keep BENCHMARKING_FINDINGS_2025-12-03.md as reference
```

---

## Implementation Status Tracker

See [`doc/BENCHMARKING_FIX_ALL_IMPLEMENTATION_STATUS.md`](BENCHMARKING_FIX_ALL_IMPLEMENTATION_STATUS.md)

---

## Timeline

| Phase | Task | Duration | Priority |
|-------|------|----------|----------|
| 1 | Fix dwarfsextract crash | 2-4h | P0 |
| 2 | Create Thrift header | 1-2h | P0 |
| 3 | Build all configurations | 0.5h | P1 |
| 4 | Quick validation | 0.25h | P1 |
| 5 | Implement benchmark script | 1-2h | P1 |
| 6 | Run benchmarks | 2-3h | P1 |
| 7 | Validate & document | 1h | P1 |

**Total Estimated**: 7.75-12.75 hours  
**Target Completion**: 2025-12-04 evening

---

## Success Criteria

### Must Have ✅
1. dwarfsextract works reliably
2. Missing Thrift header created
3. All 3 builds working
4. Complete benchmark results
5. Comparison report generated

### Should Have 🎯
1. Perl dataset benchmarks
2. Multiple algorithm tests
3. Cross-format compatibility validated
4. Documentation updated

### Nice to Have 💡
1. Visualization charts
2. CI/CD integration
3. Performance profiling

---

## Deliverables

- [ ] Working dwarfsextract
- [ ] `thrift_metadata_builder_impl.h` header
- [ ] 3 build directories (fb, tb, dual)
- [ ] `benchmarks/run_3build_comparison.py`
- [ ] `benchmarks/generate_3build_report.py` 
- [ ] `benchmark-results/3build-raw-*.json`
- [ ] `benchmark-results/3BUILD_COMPARISON_REPORT.md`
- [ ] Updated README
- [ ] Archived old docs

---

**Status**: 📋 **READY TO EXECUTE**  
**Next Action**: Start Phase 1 - Debug dwarfsextract crash