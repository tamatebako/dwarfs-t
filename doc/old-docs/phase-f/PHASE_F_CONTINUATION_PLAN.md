# Phase F: Proper Implementation & Automated Benchmarks - Continuation Plan

**Date**: 2025-11-30  
**Current Status**: Phase E Complete (manual benchmarks)  
**Next Phase**: F (Proper Implementation & Automation)  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`

---

## Issues Identified from Phase E

### 1. File Extension System Missing
**Issue**: Both FlatBuffers and Thrift images use `.dwarfs` extension  
**Required**: Distinguish formats by extension:
- `.dff` - DwarFS FlatBuffers format
- `.dft` - DwarFS Thrift format
- `.dwarfs` - Generic/legacy (auto-detect format)

**Impact**: HIGH - Makes format identification and tooling difficult

### 2. Dual-Format Build Broken
**Issue**: `build-dual-fresh/` compilation failed with error:
```
/Users/mulgogi/src/external/dwarfs/src/reader/internal/metadata_types_flatbuffers.cpp:56:18: 
error: no matching constructor for initialization of 'dwarfs::internal::string_table'
```

**Impact**: MEDIUM - Cannot test or benchmark dual-format functionality

### 3. Creation Time Discrepancy
**Issue**: Suspicious timing results:
- FlatBuffers: `real 0.941s, user 0.015s, sys 0.023s`
- Thrift: `real 0.049s, user 0.017s, sys 0.016s`

**Problem**: Real time 20x slower for FlatBuffers despite similar CPU time  
**Likely Cause**: First-run caching, filesystem sync, or measurement error

**Impact**: LOW - Need accurate measurements

### 4. Manual Benchmark Process
**Issue**: Benchmarks run manually with ad-hoc shell commands  
**Required**: 
- Automated benchmark script
- JSON output for data processing
- Report generation from JSON

**Impact**: HIGH - Cannot reproduce or automate benchmarks

---

## Phase F Objectives

1. **Implement File Extension System** (F1)
2. **Fix Dual-Format Build** (F2)
3. **Create Accurate Benchmark Infrastructure** (F3-F5)

---

## F1: File Extension System

### Requirements

**File Extensions**:
- `.dff` = DwarFS FlatBuffers (modern default)
- `.dft` = DwarFS Thrift (legacy)
- `.dwarfs` = Generic (auto-detect, backward compatible)

**Tools Behavior**:
- **mkdwarfs**: Output extension hints format, but always detect from `--format` option
- **dwarfsck/dwarfsextract/dwarfs**: Auto-detect format from magic bytes (extension hint only)

### Implementation Tasks

#### T1.1: Update Format Detection Logic

**File**: `src/reader/filesystem_v2.cpp` (or similar)

**Current**: Detects format via magic bytes only  
**Required**: Add extension hinting for user clarity

```cpp
// Pseudo-code
auto detect_format_from_path(filesystem::path const& p) {
  if (p.extension() == ".dff") return SerializationFormat::FLATBUFFERS;
  if (p.extension() == ".dft") return SerializationFormat::THRIFT_COMPACT;
  // .dwarfs or no extension: auto-detect from magic
  return detect_from_magic_bytes(p);
}
```

#### T1.2: Update mkdwarfs Output Extension

**File**: `tools/src/mkdwarfs/create_handler.cpp` (or main)

**Logic**:
```cpp
// If user specifies .dwarfs, suggest format-specific extension
if (output_path.extension() == ".dwarfs") {
  auto suggested = (format == FLATBUFFERS) ? ".dff" : ".dft";
  // Optionally warn or auto-append correct extension
}
```

#### T1.3: Update Documentation

**Files**: 
- `README.md`
- `doc/mkdwarfs.md`
- `doc/dwarfs-format.md`

**Changes**:
- Document `.dff` and `.dft` extensions
- Explain backward compatibility with `.dwarfs`
- Update all examples

### Estimated Time: 2-3 hours

---

## F2: Fix Dual-Format Build

### Error Analysis

**Error Location**: `src/reader/internal/metadata_types_flatbuffers.cpp:56`

**Error**:
```cpp
return dwarfs::internal::string_table(lgr, "names", st);
```

**Problem**: Constructor signature mismatch

**Expected Signature** (from `include/dwarfs/internal/string_table.h:78`):
```cpp
string_table(logger& lgr, std::string_view name, PackedTableView v);
```

**Passed Type**: `::dwarfs::metadata::domain::string_table` (domain model type)  
**Expected Type**: `PackedTableView` (Thrift Frozen2 view)

### Root Cause

FlatBuffers adapter trying to construct `string_table` with **domain model type** instead of **view type**.

**Solution**: FlatBuffers needs its own adapter that converts domain model to appropriate internal representation.

### Implementation Tasks

#### T2.1: Read Error Context

```bash
# Get full error
cat build-dual-fresh/CMakeFiles/CMakeError.log

# Check string_table.h interface
cat include/dwarfs/internal/string_table.h | grep -A 5 "class string_table"
```

#### T2.2: Identify Adapter Pattern

**Current Architecture**:
- Thrift: `ThriftMetadataProvider` → views Frozen2 structures directly
- FlatBuffers: `FlatBuffersMetadataProvider` → needs adapter layer

**Missing**: FlatBuffers-specific adapter for `string_table`

#### T2.3: Implement FlatBuffers String Table Adapter

**Option A**: Extend `string_table` class with FlatBuffers constructor:
```cpp
// In string_table.h
string_table(logger& lgr, std::string_view name, 
             metadata::domain::string_table const& domain_st);
```

**Option B**: Create FlatBuffers-specific wrapper:
```cpp
class flatbuffers_string_table : public string_table {
  flatbuffers_string_table(logger& lgr, std::string_view name,
                           metadata::domain::string_table const& st);
};
```

**Recommendation**: Option A (simpler, consistent with Strategy Pattern)

#### T2.4: Apply Fix and Test

```bash
# Rebuild
ninja -C build-dual-fresh

# Test dual-format functionality
./build-dual-fresh/mkdwarfs -i /tmp/size-test -o /tmp/test-dual-fb.dff --format=flatbuffers
./build-dual-fresh/mkdwarfs -i /tmp/size-test -o /tmp/test-dual-tb.dft --format=thrift
./build-dual-fresh/dwarfsck /tmp/test-dual-fb.dff
./build-dual-fresh/dwarfsck /tmp/test-dual-tb.dft
```

### Estimated Time: 3-4 hours

---

## F3: Investigation - Creation Time Discrepancy

### Hypothesis

**Suspicious Data**:
```
FlatBuffers: real 0.941s, user 0.015s, sys 0.023s  (872ms wall overhead!)
Thrift:      real 0.049s, user 0.017s, sys 0.016s
```

**Analysis**:
- CPU time similar: ~0.04s both
- Real time vastly different: 0.941s vs 0.049s
- **Wall overhead**: 872ms for FlatBuffers

**Possible Causes**:
1. **First-run effect**: FlatBuffers test ran first, triggered caching
2. **Filesystem sync**: FlatBuffers triggered `fsync()` or similar
3. **Measurement error**: Single sample, high variance
4. **Bug**: Actual performance issue

### Investigation Tasks

#### T3.1: Run Multiple Iterations

```bash
# Clear cache
echo 3 | sudo tee /proc/sys/vm/drop_caches  # Linux
purge  # macOS

# Run 10 iterations each
for i in {1..10}; do
  rm -f /tmp/test-fb.dff
  /usr/bin/time -p ./build-fb/mkdwarfs -i /tmp/size-test -o /tmp/test-fb.dff --no-progress 2>&1 | grep real
done

for i in {1..10}; do
  rm -f /tmp/test-tb.dft  
  /usr/bin/time -p ./build-tb/mkdwarfs -i /tmp/size-test -o /tmp/test-tb.dft --no-progress 2>&1 | grep real
done
```

#### T3.2: Profile with Instruments/perf

```bash
# macOS
sudo instruments -t "Time Profiler" -D /tmp/profile.trace ./build-fb/mkdwarfs -i /tmp/size-test -o /tmp/test.dff

# Linux
perf record -g ./build-fb/mkdwarfs -i /tmp/size-test -o /tmp/test.dff
perf report
```

#### T3.3: Check for Serialization Differences

```bash
# Add verbose logging
./build-fb/mkdwarfs -i /tmp/size-test -o /tmp/test.dff --log-level=verbose
./build-tb/mkdwarfs -i /tmp/size-test -o /tmp/test.dft --log-level=verbose
```

### Estimated Time: 1-2 hours

---

## F4: Automated Benchmark Script

### Requirements

**Script Name**: `benchmarks/metadata_format_benchmark.py`

**Functionality**:
1. Accept build paths for FlatBuffers and Thrift mkdwarfs
2. Accept test dataset path
3. Run configurable number of iterations
4. Measure:
   - Creation time (wall, user, sys)
   - Image size
   - Verification time
   - Memory usage
5. Output JSON with all metrics

**JSON Schema**:
```json
{
  "metadata": {
    "date": "2025-11-30T18:45:00Z",
    "dataset": {
      "path": "/tmp/size-test",
      "files": 11,
      "size_bytes": 232
    },
    "iterations": 10,
    "builds": {
      "flatbuffers": "./build-fb/mkdwarfs",
      "thrift": "./build-tb/mkdwarfs"
    }
  },
  "results": {
    "flatbuffers": {
      "creation": {
        "real_mean": 0.052,
        "real_stddev": 0.003,
        "user_mean": 0.015,
        "sys_mean": 0.023,
        "samples": [0.049, 0.051, 0.054, ...]
      },
      "image_size_bytes": 1347,
      "verification": {
        "time_mean": 0.021,
        "time_stddev": 0.001
      }
    },
    "thrift": {
      "creation": { ... },
      "image_size_bytes": 1240,
      "verification": { ... }
    }
  }
}
```

### Implementation

**File**: `benchmarks/metadata_format_benchmark.py`

```python
#!/usr/bin/env python3
"""
DwarFS Metadata Format Benchmark Script

Compares FlatBuffers and Thrift metadata formats across:
- Creation time
- Image size
- Verification time
- Memory usage
"""

import argparse
import json
import subprocess
import tempfile
import time
import statistics
from pathlib import Path
from typing import Dict, List

def run_benchmark(mkdwarfs_path: Path, dataset_path: Path, 
                  output_path: Path, iterations: int) -> Dict:
    """Run benchmark for one format."""
    
    samples = []
    for i in range(iterations):
        # Clear cache if possible
        subprocess.run(['sync'], check=False)
        
        # Time creation
        start = time.perf_counter()
        result = subprocess.run(
            [str(mkdwarfs_path), '-i', str(dataset_path), 
             '-o', str(output_path), '--no-progress'],
            capture_output=True, text=True, timeout=60
        )
        elapsed = time.perf_counter() - start
        
        if result.returncode != 0:
            raise RuntimeError(f"mkdwarfs failed: {result.stderr}")
        
        samples.append(elapsed)
        
        # Remove output for next iteration
        output_path.unlink()
    
    return {
        'real_mean': statistics.mean(samples),
        'real_stddev': statistics.stdev(samples) if len(samples) > 1 else 0,
        'samples': samples
    }

def main():
    parser = argparse.ArgumentParser(description='Benchmark metadata formats')
    parser.add_argument('--flatbuffers-mkdwarfs', required=True, type=Path)
    parser.add_argument('--thrift-mkdwarfs', required=True, type=Path)
    parser.add_argument('--dataset', required=True, type=Path)
    parser.add_argument('--iterations', type=int, default=10)
    parser.add_argument('--output', type=Path, required=True)
    
    args = parser.parse_args()
    
    # Run benchmarks
    with tempfile.TemporaryDirectory() as tmpdir:
        fb_output = Path(tmpdir) / 'test-fb.dff'
        tb_output = Path(tmpdir) / 'test-tb.dft'
        
        print("Running FlatBuffers benchmark...")
        fb_results = run_benchmark(args.flatbuffers_mkdwarfs, 
                                   args.dataset, fb_output, args.iterations)
        
        print("Running Thrift benchmark...")
        tb_results = run_benchmark(args.thrift_mkdwarfs,
                                   args.dataset, tb_output, args.iterations)
    
    # Collect results
    results = {
        'metadata': {
            'date': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
            'dataset': str(args.dataset),
            'iterations': args.iterations
        },
        'results': {
            'flatbuffers': fb_results,
            'thrift': tb_results
        }
    }
    
    # Write JSON
    with open(args.output, 'w') as f:
        json.dump(results, f, indent=2)
    
    print(f"Results written to {args.output}")

if __name__ == '__main__':
    main()
```

### Estimated Time: 2-3 hours

---

## F5: Benchmark Report Generation

### Requirements

**Script Name**: `benchmarks/generate_report.py`

**Functionality**:
1. Read JSON from F4
2. Generate Markdown report with:
   - Summary table
   - Statistical analysis
   - Size comparison
   - Performance comparison
   - Recommendations

**Report Template**: `doc/PHASE_F_BENCHMARK_REPORT.md`

### Implementation

**File**: `benchmarks/generate_report.py`

```python
#!/usr/bin/env python3
"""Generate benchmark report from JSON data."""

import argparse
import json
from pathlib import Path

def generate_report(data: dict) -> str:
    """Generate Markdown report from benchmark data."""
    
    md = []
    md.append("# DwarFS Metadata Format Benchmark Report")
    md.append("")
    md.append(f"**Date**: {data['metadata']['date']}")
    md.append(f"**Dataset**: {data['metadata']['dataset']}")
    md.append(f"**Iterations**: {data['metadata']['iterations']}")
    md.append("")
    
    fb = data['results']['flatbuffers']
    tb = data['results']['thrift']
    
    # Summary table
    md.append("## Summary")
    md.append("")
    md.append("| Metric | FlatBuffers | Thrift | Ratio |")
    md.append("|--------|-------------|--------|-------|")
    md.append(f"| Creation Time (mean) | {fb['real_mean']:.3f}s | "
              f"{tb['real_mean']:.3f}s | "
              f"{fb['real_mean']/tb['real_mean']:.2f}x |")
    md.append(f"| Image Size | {fb['image_size_bytes']} bytes | "
              f"{tb['image_size_bytes']} bytes | "
              f"{fb['image_size_bytes']/tb['image_size_bytes']:.4f}x |")
    
    return "\n".join(md)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=Path, help='JSON input file')
    parser.add_argument('output', type=Path, help='Markdown output file')
    
    args = parser.parse_args()
    
    with open(args.input) as f:
        data = json.load(f)
    
    report = generate_report(data)
    
    with open(args.output, 'w') as f:
        f.write(report)
    
    print(f"Report written to {args.output}")

if __name__ == '__main__':
    main()
```

### Estimated Time: 1-2 hours

---

## Phase F Timeline

| Task | Estimated Time | Priority | Dependencies |
|------|----------------|----------|--------------|
| **F1: File Extensions** | 2-3 hours | HIGH | None |
| **F2: Fix Dual-Format** | 3-4 hours | MEDIUM | None |
| **F3: Time Investigation** | 1-2 hours | LOW | None |
| **F4: Benchmark Script** | 2-3 hours | HIGH | F1 (for extensions) |
| **F5: Report Generator** | 1-2 hours | HIGH | F4 |

**Total Estimated Time**: 9-15 hours

**Recommended Order**:
1. F1 (file extensions) - foundation for everything
2. F4 + F5 (automated benchmarks) - validation
3. F2 (dual-format build) - completeness
4. F3 (time investigation) - optional deep dive

---

## Success Criteria

### Phase F Complete When:

- [ ] File extension system implemented (`.dff`, `.dft`, `.dwarfs`)
- [ ] All tools honor extension conventions
- [ ] Dual-format build compiles and works
- [ ] Automated benchmark script produces JSON
- [ ] Report generator creates proper Markdown
- [ ] Benchmarks run with ≥10 iterations
- [ ] Results show consistent timing (low variance)
- [ ] Documentation updated with new extensions

---

## Testing Plan

### T1: File Extension Tests

```bash
# Test FlatBuffers with .dff
./build-fb/mkdwarfs -i /tmp/test -o /tmp/test.dff
./build-fb/dwarfsck /tmp/test.dff
file /tmp/test.dff  # Should mention FlatBuffers

# Test Thrift with .dft
./build-tb/mkdwarfs -i /tmp/test -o /tmp/test.dft
./build-tb/dwarfsck /tmp/test.dft
file /tmp/test.dft  # Should mention Thrift

# Test backward compat with .dwarfs
./build-fb/mkdwarfs -i /tmp/test -o /tmp/test.dwarfs
./build-fb/dwarfsck /tmp/test.dwarfs  # Auto-detect
```

### T2: Automated Benchmark Test

```bash
python3 benchmarks/metadata_format_benchmark.py \
  --flatbuffers-mkdwarfs ./build-fb/mkdwarfs \
  --thrift-mkdwarfs ./build-tb/mkdwarfs \
  --dataset /tmp/size-test \
  --iterations 10 \
  --output results.json

python3 benchmarks/generate_report.py \
  results.json \
  doc/PHASE_F_BENCHMARK_REPORT.md

# Verify report
cat doc/PHASE_F_BENCHMARK_REPORT.md
```

### T3: Dual-Format Build Test

```bash
./build-dual-fresh/mkdwarfs -i /tmp/test -o /tmp/test-dual-fb.dff --format=flatbuffers
./build-dual-fresh/mkdwarfs -i /tmp/test -o /tmp/test-dual-tb.dft --format=thrift
./build-dual-fresh/dwarfsck /tmp/test-dual-fb.dff
./build-dual-fresh/dwarfsck /tmp/test-dual-tb.dft
```

---

## Documentation Updates

### Files to Update

1. **README.md**:
   - Document `.dff` and `.dft` extensions
   - Update all examples
   - Add extension comparison table

2. **doc/mkdwarfs.md**:
   - Add `--format` option details
   - Document extension behavior
   - Add examples with `.dff` and `.dft`

3. **doc/dwarfs-format.md**:
   - Add file extension section
   - Document magic bytes for each format
   - Add format detection algorithm

4. **doc/PHASE_F_BENCHMARK_REPORT.md**:
   - Generated from automated benchmarks
   - Replace Phase E manual results

### Files to Archive

Move to `doc/old-docs/`:
- `doc/PHASE_E_BENCHMARK_RESULTS.md` (manual benchmarks superseded)

---

## Rollback Plan

If issues arise:

```bash
# Revert file extension changes
git checkout HEAD -- src/reader/filesystem_v2.cpp tools/src/mkdwarfs/

# Use old builds
rm -rf build-dual-fresh
# Keep build-fb and build-tb which work

# Fall back to manual benchmarks
# Phase E results still valid, just not automated
```

---

**Status**: Ready to execute  
**Next Step**: F1 - Implement file extension system  
**Command**: Read `doc/PHASE_F_CONTINUATION_PROMPT.md` to continue