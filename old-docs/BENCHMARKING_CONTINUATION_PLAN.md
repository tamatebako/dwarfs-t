# DwarFS Benchmarking Framework - Continuation Plan

**Date**: 2025-11-27
**Status**: Phase 1.1 Complete, Ready for Phase 1.2
**Branch**: feature/benchmark-framework

---

## Executive Summary

Phase 1.1 successfully added benchmark metrics APIs to dwarfsextract. The tool can now collect and export detailed performance metrics including metadata load time, extraction time, file counts, and errors. Next phases will create automated benchmarking infrastructure and compare FlatBuffers vs Thrift formats.

---

## Completed: Phase 1.1 ✅

### Implementation Summary
- **Files Modified**: 3
  - `include/dwarfs/utility/filesystem_extractor.h` - Added metrics API
  - `src/utility/filesystem_extractor.cpp` - Instrumented extraction
  - `tools/src/dwarfsextract_main.cpp` - Added CLI benchmark mode
- **New Functionality**:
  - `extraction_metrics` structure with 11 metrics
  - `--benchmark-mode` flag
  - `--output-json=FILE` for JSON export
  - `--repeat=N` for averaging
- **Build Status**: ✅ Compiles successfully on macOS ARM64
- **Time Spent**: 1.5 hours (vs 3h estimated)

---

## Phase 1.2: Create Benchmark Runner Script

**Estimated Time**: 2 hours
**Priority**: High

### Objective
Create Python script to automate FlatBuffers vs Thrift format comparison across multiple datasets.

### Implementation Steps

#### Step 1: Create Base Infrastructure (30 min)
```bash
benchmarks/lib/
├── __init__.py
├── benchmark_executor.py  # Run dwarfsextract with metrics
├── dataset_manager.py     # Manage test datasets
├── result_formatter.py    # JSON → Markdown/HTML
└── comparison_engine.py   # Compare FlatBuffers vs Thrift
```

#### Step 2: Implement Benchmark Executor (30 min)
**File**: `benchmarks/lib/benchmark_executor.py`

```python
class BenchmarkExecutor:
    def run_extraction(self, image_path, output_dir, repeat=3):
        """Run dwarfsextract with metrics collection"""
        # Execute: dwarfsextract --benchmark-mode --repeat=N
        # Parse JSON output
        # Return metrics dict
    
    def run_comparison(self, flatbuffers_image, thrift_image):
        """Compare two images"""
        # Run extraction for both
        # Calculate deltas
        # Return comparison results
```

#### Step 3: Create Main Runner Script (30 min)
**File**: `benchmarks/run_format_comparison.py`

```python
#!/usr/bin/env python3
"""
Compare FlatBuffers vs Thrift metadata formats.

Usage:
    python benchmarks/run_format_comparison.py \\
        --dataset small,medium,large \\
        --repeat 5 \\
        --output comparison-report.md
"""
```

**Features**:
- Accept dataset names or image paths
- Run benchmarks with configurable repeat count
- Generate comparison report (Markdown/HTML)
- Export raw data to JSON

#### Step 4: Add Result Formatter (30 min)
**File**: `benchmarks/lib/result_formatter.py`

```python
class ResultFormatter:
    def format_markdown(self, results):
        """Generate Markdown comparison report"""
    
    def format_html(self, results):
        """Generate HTML with charts (optional)"""
    
    def format_json(self, results):
        """Export structured JSON"""
```

**Output Format**:
```markdown
# Format Comparison Report

## Summary
- Dataset: perl-5.38 (47.5 GiB extracted)
- Repeat Count: 5 runs each

## Results

### FlatBuffers
- Image Size: 430.9 MiB
- Avg Metadata Load: 8.2 ms
- Avg Extraction: 180.5 sec
- Memory Peak: 256 MiB

### Thrift Compact
- Image Size: 410.3 MiB (-4.8%)
- Avg Metadata Load: 12.5 ms (+52%)
- Avg Extraction: 182.1 sec (+0.9%)
- Memory Peak: 248 MiB (-3.1%)

## Conclusion
FlatBuffers loads 52% faster but produces 4.8% larger images.
```

---

## Phase 1.3: Prepare Test Datasets

**Estimated Time**: 1 hour
**Priority**: High

### Objective
Create test images in both formats for benchmarking.

### Datasets

#### Small Dataset (~10 MB)
- **Source**: `testdata/` directory
- **Purpose**: Quick iteration, CI testing

#### Medium Dataset (~1 GB)
- **Source**: Generate from Perl installations or use existing benchmark data
- **Purpose**: Realistic comparison

#### Large Dataset (~10 GB)
- **Source**: CI build artifacts or download sample
- **Purpose**: Stress testing

### Implementation

**File**: `benchmarks/prepare_datasets.sh`

```bash
#!/bin/bash
# Prepare test datasets in both formats

MKDWARFS="./build/mkdwarfs"
DATASETS_DIR="./benchmark-datasets"

# Small dataset
$MKDWARFS -i testdata -o "$DATASETS_DIR/small-fb.dwarfs" \\
    --metadata-format=flatbuffers -l7

$MKDWARFS -i testdata -o "$DATASETS_DIR/small-th.dwarfs" \\
    --metadata-format=thrift -l7

# Medium dataset (if available)
if [ -d "benchmark_data/perl" ]; then
    $MKDWARFS -i benchmark_data/perl -o "$DATASETS_DIR/medium-fb.dwarfs" \\
        --metadata-format=flatbuffers -l7
    
    $MKDWARFS -i benchmark_data/perl -o "$DATASETS_DIR/medium-th.dwarfs" \\
        --metadata-format=thrift -l7
fi
```

**Note**: May need to fix mkdwarfs assertion issue first. Alternative: Use pre-built images if available.

---

## Phase 1.4: Run Initial Benchmarks

**Estimated Time**: 2 hours
**Priority**: Medium

### Objective
Execute benchmarks and collect initial data.

### Steps

#### Step 1: Build Tools with Both Formats (15 min)
```bash
cd /Users/mulgogi/src/external/dwarfs
mkdir -p build-bench-all
cd build-bench-all

cmake .. -GNinja \\
    -DCMAKE_BUILD_TYPE=Release \\
    -DDWARFS_WITH_FLATBUFFERS=ON \\
    -DDWARFS_WITH_THRIFT=ON \\
    -DWITH_TOOLS=ON \\
    -DWITH_TESTS=OFF

ninja mkdwarfs dwarfsextract
```

#### Step 2: Create Test Images (30 min)
```bash
# Run prepare_datasets.sh
bash benchmarks/prepare_datasets.sh
```

#### Step 3: Run Benchmarks (1 hour)
```bash
# Small dataset (quick validation)
python benchmarks/run_format_comparison.py \\
    --flatbuffers benchmark-datasets/small-fb.dwarfs \\
    --thrift benchmark-datasets/small-th.dwarfs \\
    --repeat 10 \\
    --output benchmark-results/small-comparison.md

# Medium dataset (realistic test)
python benchmarks/run_format_comparison.py \\
    --flatbuffers benchmark-datasets/medium-fb.dwarfs \\
    --thrift benchmark-datasets/medium-th.dwarfs \\
    --repeat 5 \\
    --output benchmark-results/medium-comparison.md
```

#### Step 4: Collect System Info (15 min)
- CPU model, core count
- RAM amount
- Storage type (SSD/HDD)
- OS version
- Library versions

---

## Phase 1.5: Document Findings

**Estimated Time**: 1 hour
**Priority**: High

### Objective
Create comprehensive benchmark report with conclusions.

### Deliverables

#### 1. Benchmark Report
**File**: `doc/BENCHMARK_FLATBUFFERS_VS_THRIFT.md`

**Sections**:
- Executive Summary
- Test Methodology
- System Configuration
- Results by Dataset
- Performance Analysis
- Memory Usage Comparison
- Conclusions & Recommendations

#### 2. Update README
**File**: `README.md`

Add benchmarking section:
```markdown
## Benchmarking

DwarFS includes a benchmarking framework to compare metadata formats:

```bash
# Extract with metrics
dwarfsextract --benchmark-mode \\
    --output-json=metrics.json \\
    -i image.dwarfs -o output/

# Compare formats
python benchmarks/run_format_comparison.py \\
    --dataset small,medium \\
    --output comparison.md
```

See [Benchmark Results](doc/BENCHMARK_FLATBUFFERS_VS_THRIFT.md) for detailed analysis.
```

#### 3. Update Memory Bank
**File**: `.kilocode/rules/memory-bank/context.md`

Update with benchmark findings and next priorities.

---

## Alternative: Compressed Timeline (Next Session)

If time is constrained, compress phases 1.2-1.5 into single session:

### Quick Path (3-4 hours total)

1. **Create Simple Runner** (1 hour)
   - Minimal Python script
   - Runs dwarfsextract on existing images
   - Outputs comparison table

2. **Use Existing Images** (30 min)
   - Skip dataset creation
   - Use test suite images (even if old format)
   - Focus on demonstrating capability

3. **Document Framework** (1 hour)
   - Write usage guide
   - Document architecture
   - Create example outputs

4. **Update Memory Bank** (30 min)
   - Record decisions made
   - Document next steps

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| mkdwarfs assertion failure | High | Use existing images or fix assertion first |
| Thrift images incompatible | Medium | Rebuild images or test with both formats |
| Insufficient test data | Low | Use test suite data, even if small |
| Python dependencies | Low | Use stdlib only, minimal external deps |

---

## Success Criteria

- ✅ Automated benchmark script works
- ✅ Can compare FlatBuffers vs Thrift
- ✅ Results exported to Markdown/JSON
- ✅ Documentation complete
- ✅ Findings inform future decisions

---

## Next Session Quick Start

```bash
# 1. Navigate to project
cd /Users/mulgogi/src/external/dwarfs

# 2. Read continuation plan
cat doc/BENCHMARKING_CONTINUATION_PLAN.md

# 3. Start with Phase 1.2
# Create benchmarks/lib/benchmark_executor.py

# 4. Or use Quick Path for compressed timeline
```

---

**Created**: 2025-11-27 08:16 HKT
**Last Updated**: 2025-11-27 08:16 HKT
**Status**: Ready for Phase 1.2