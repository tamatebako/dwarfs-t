# DwarFS Benchmark Result Schemas

JSON Schema (v7) documentation for all DwarFS benchmark result formats.

## Overview

The DwarFS benchmark suite produces structured JSON results for programmatic analysis. This directory documents the schemas for each benchmark type.

## Schema Files

### [`fuse_benchmark.json`](fuse_benchmark.json)
**Type**: FUSE extraction benchmark results
**Producer**: `run_comprehensive_benchmark.sh` (FUSE benchmarks)
**Example**: `results/comprehensive_*/fuse_fb-only_dff.json`

Captures FUSE driver extraction performance via mount → cp → unmount workflow.

### [`api_benchmark.json`](api_benchmark.json)
**Type**: libdwarfs C++ API benchmark results
**Producer**: `benchmarks/libdwarfs/*_bench` programs
**Example**: `results/comprehensive_*/api_single_fb-only_dff.json`

Captures direct C++ API performance with statistical analysis.

## Result Naming Convention

### FUSE Benchmarks

**Pattern**: `fuse_{build}_{format}.json`

**Components**:
- `build`: `fb-only` | `thrift-only` | `both`
- `format`: `dff` (FlatBuffers) | `dft` (Thrift)

**Examples**:
- `fuse_fb-only_dff.json` - FlatBuffers-only build with .dff image
- `fuse_both_dft.json` - Both-formats build with .dft image

### API Benchmarks

**Pattern**: `api_{operation}_{build}_{format}.json`

**Components**:
- `operation`: `single` | `full` | `multiple` | `random`
- `build`: `fb-only` | `thrift-only` | `both`
- `format`: `dff` | `dft`

**Examples**:
- `api_single_fb-only_dff.json` - Single file, FB-only build, .dff image
- `api_full_both_dft.json` - Full extraction, both-formats build, .dft image

## Schema Usage

### Validation

Using Python `jsonschema`:

```python
import json
import jsonschema

# Load schema
with open('benchmarks/schemas/fuse_benchmark.json') as f:
    schema = json.load(f)

# Load result
with open('results/fuse_fb-only_dff.json') as f:
    result = json.load(f)

# Validate
jsonschema.validate(result, schema)
print("✓ Valid!")
```

### Parsing

```python
import json

# Parse FUSE result
with open('results/fuse_fb-only_dff.json') as f:
    data = json.load(f)
    
print(f"Build: {data['build']}")
print(f"Format: {data['format']}")
print(f"Throughput: {data['throughput_mb_per_sec']:.2f} MB/s")
```

```python
# Parse API result
with open('results/api_single_fb-only_dff.json') as f:
    data = json.load(f)
    bench_name = list(data.keys())[0]
    results = data[bench_name]
    
print(f"Median time: {results['time']['median']*1000:.2f} ms")
print(f"Peak memory: {results['memory']['max']/1024:.1f} KiB")
print(f"Throughput: {results['throughput_mb_per_sec']:.2f} MB/s")
```

## Result Interpretation

### Time Metrics

All time measurements in **seconds**:
- `mean`: Average across all iterations
- `median`: Middle value (more robust to outliers)
- `stddev`: Variation between runs (lower = more consistent)
- `min`/`max`: Best/worst case

**Use median** for performance comparisons (more stable than mean).

### Memory Metrics

All memory measurements in **bytes**:
- Reported as RSS (Resident Set Size) = actual RAM used
- Peak memory during operation
- Convert to human-readable: `bytes / 1024` = KiB, `/ (1024*1024)` = MiB

### Throughput

Calculated as: `data_size / median_time / (1024*1024)` MB/s

**FUSE benchmarks**: Reflects mount + kernel overhead
**API benchmarks**: Direct library performance (5-10% faster expected)

## Comprehensive Benchmark Results

The comprehensive benchmark produces **12 JSON files** total:

### FUSE Results (4 files)
1. `fuse_fb-only_dff.json` - FB-only build reading .dff
2. `fuse_thrift-only_dft.json` - Thrift-only build reading .dft
3. `fuse_both_dff.json` - Both build reading .dff
4. `fuse_both_dft.json` - Both build reading .dft

### API Results (8 files)
1. `api_single_fb-only_dff.json` - Single file, FB-only, .dff
2. `api_single_thrift-only_dft.json` - Single file, Thrift-only, .dft
3. `api_single_both_dff.json` - Single file, both, .dff
4. `api_single_both_dft.json` - Single file, both, .dft
5. `api_full_fb-only_dff.json` - Full extract, FB-only, .dff
6. `api_full_thrift-only_dft.json` - Full extract, Thrift-only, .dft
7. `api_full_both_dff.json` - Full extract, both, .dff
8. `api_full_both_dft.json` - Full extract, both, .dft

## Comparison Guidelines

### FUSE vs API

**Expected**: API 5-10% faster (no kernel/FUSE overhead)

**Actual causes of differences**:
- FUSE: Kernel context switches, FUSE protocol overhead
- API: Direct memory access, no syscalls for reads

### FlatBuffers vs Thrift

**Performance** (Session 16 results):
- Compression: FB 17-29% faster at levels 1-3
- Extraction: Nearly identical (±3%)
- Size: Thrift 0.07-1.41% smaller

**Portability**:
- FB: Header-only, all platforms
- Thrift: Folly+fbthrift, limited platforms

### Build Configuration Impact

**FB-only**:
- Simplest build
- Best portability
- Can only read .dff images

**Thrift-only**:
- Complex dependencies
- Smallest images
- Can only read .dft images

**Both**:
- Maximum flexibility
- Larger binary (~10-15%)
- Can read/write both formats

## See Also

- [`../README.md`](../README.md) - Benchmark suite overview
- [`../../doc/LIBDWARFS_API_PERFORMANCE.md`](../../doc/LIBDWARFS_API_PERFORMANCE.md) - API performance guide
- [`../../doc/DWARFS_METADATA_FORMAT_PERFORMANCE.md`](../../doc/DWARFS_METADATA_FORMAT_PERFORMANCE.md) - Format comparison
