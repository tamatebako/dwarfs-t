# Session 19: Comprehensive Benchmark System Integration & Validation

**Created**: 2025-12-19
**Priority**: HIGH (Complete benchmarking infrastructure)
**Estimated Time**: 4-6 hours
**Deadline**: ASAP

---

## Context

Session 17 created **libdwarfs C++ API benchmarks** but discovered there's already a **complete comprehensive benchmarking system** that needs integration and validation.

### What Exists

**Comprehensive System** (`benchmarks/`):
- ✅ **Shell orchestrator**: [`run_comprehensive_benchmark.sh`](../benchmarks/run_comprehensive_benchmark.sh) (505 lines)
- ✅ **Python orchestrator**: [`comprehensive_benchmark.py`](../benchmarks/comprehensive_benchmark.py) (499 lines)
- ✅ **Build manager**: Handles fb-only, thrift-only, both configs
- ✅ **Dataset manager**: Downloads and prepares test data
- ✅ **Result collector**: Collects and aggregates results
- ✅ **Report generators**: Creates comprehensive markdown reports
- ✅ **Utility libraries**: 13 modules in [`benchmarks/lib/`](../benchmarks/lib/)

**libdwarfs Benchmarks** (Session 17):
- ✅ **C++ framework**: [`benchmark_framework.h`](../benchmarks/libdwarfs/benchmark_framework.h) (395 lines)
- ✅ **4 C++ programs**: single_file, multiple_files, full_extract, random_access
- ✅ **Shell runner**: [`run_libdwarfs_benchmark.sh`](../benchmarks/run_libdwarfs_benchmark.sh) (470 lines)
- ⚠️ **Bug fixed**: map::at on moved-from string (commit 352a3c2a)

### What's Missing

**Integration**:
- [ ] libdwarfs API benchmarks not called by comprehensive script
- [ ] No unified JSON schema documented
- [ ] Compression algorithm benchmarks not integrated
- [ ] No single "run all" command

**Validation**:
- [ ] Comprehensive benchmark never fully tested end-to-end
- [ ] Report generation not validated
- [ ] Result schema not documented
- [ ] No regression baseline established

---

## Objectives

### 1. Document Existing Infrastructure

Create comprehensive documentation of the benchmark system architecture:

**File**: `doc/BENCHMARKING_SYSTEM_ARCHITECTURE.md`

**Contents**:
- Complete system diagram
- Component responsibilities
- Data flow through the system
- JSON schemas for all result types
- Integration points

### 2. Validate Comprehensive Benchmark Script

**Test**: [`run_comprehensive_benchmark.sh`](../benchmarks/run_comprehensive_benchmark.sh)

**Requirements**:
- Builds all 3 configurations (fb-only, thrift-only, both)
- Creates both .dff and .dft images
- Runs FUSE benchmarks for each config/format combo
- Runs libdwarfs API benchmarks for each config/format combo
- Generates comprehensive report with all comparisons

**Expected Runtime**: 2-3 hours
**Expected Output**: `results/comprehensive_YYYYMMDD_HHMMSS/COMPREHENSIVE_REPORT.md`

### 3. Integrate libdwarfs Benchmarks

**Modify**: [`run_comprehensive_benchmark.sh`](../benchmarks/run_comprehensive_benchmark.sh:236-273)

The `benchmark_libdwarfs_api()` function at line 235 correctly calls the C++ programs, but needs fix for file path issue (missing "/" prefix).

**Fix Required**:
```bash
# Line 244: Add "/" prefix to test file path
local test_file="/$(dwarfsck"$image" -l 2>/dev/null | grep -E '\.(pm|pl|pod)$' | head -n 1 || echo "")"
```

### 4. Create Unified JSON Schema

**File**: `benchmarks/schemas/benchmark_results.json`

Document the schema for:
- **FUSE extraction results** (from shell script)
- **libdwarfs API results** (from C++ programs)
- **Compression benchmarks** (from Python)
- **Metadata format benchmarks** (from Python)

### 5. Create Master Script

**File**: `benchmarks/run_all_benchmarks.sh`

Single command to run ALL benchmarks:

```bash
#!/usr/bin/env bash
# Master benchmark orchestrator

./benchmarks/run_comprehensive_benchmark.sh   # FUSE vs API, all formats
./benchmarks/run_metadata_format_benchmark.py  # Metadata formats only
./benchmarks/compression_algorithm_benchmark.py # Compression only

# Combine all results
python3 benchmarks/generate_master_report.py results/
```

---

## Implementation Plan

### Phase 1: Documentation (1 hour)

**1.1 Create System Architecture Document**

**File**: `doc/BENCHMARKING_SYSTEM_ARCHITECTURE.md`

```markdown
# DwarFS Benchmarking System Architecture

## Overview

Three-tier benchmark system:
1. **Shell Scripts**: Quick FUSE/API comparison
2. **Python System**: Comprehensive multi-config testing
3. **C++ Programs**: Low-level API performance

## Components

### Tier 1: Shell Scripts

**run_comprehensive_benchmark.sh**:
- Builds 3 configs
- Creates 2 image formats
- Runs FUSE benchmarks
- Runs API benchmarks
- Generates unified report

**run_libdwarfs_benchmark.sh**:
- Runs C++ API benchmarks only
- Quick validation of API performance

### Tier 2: Python Infrastructure

**Core Classes**:
- `BuildManager`: Build configuration management
- `DatasetManager`: Dataset download/preparation
- `ResultCollector`: Result aggregation
- `BenchmarkStatistics`: Statistical analysis
- `ReportGenerator`: Markdown report generation

**Utilities**:
- `MemoryTracker`: Platform-aware memory measurement
- `FUSEManager`: Mount/unmount lifecycle
- `PerfmonParser`: FUSE performance metrics

### Tier 3: C++ Programs

**Framework**: `benchmark_framework.h`
- High-resolution timing
- Memory tracking
- Statistical analysis
- JSON export

**Programs**:
- `single_file_bench`: Single file latency
- `multiple_files_bench`: Multi-file throughput
- `full_extract_bench`: Full extraction
- `random_access_bench`: Random access patterns

## Data Flow

```
┌─────────────────┐
│ Master Script   │
└────────┬────────┘
         │
    ┌────┴────┐
    │         │
    ▼         ▼
┌────────┐ ┌──────────┐
│ Shell  │ │  Python  │
│Scripts │ │Framework │
└───┬────┘ └────┬─────┘
    │           │
    ▼           ▼
┌────────────────────┐
│  C++ Benchmarks    │
│  + Build Tools     │
└──────────┬─────────┘
           │
           ▼
    ┌─────────────┐
    │JSON Results │
    └──────┬──────┘
           │
           ▼
    ┌──────────────┐
    │ Report Gen   │
    └──────────────┘
```

## JSON Schemas

### FUSE Benchmark Result
```json
{
  "benchmark": "fuse_extraction",
  "build": "fb-only|thrift-only|both",
  "format": "dff|dft",
  "image": "path/to/image",
  "duration_seconds": 1.234,
  "extracted_bytes": 12345678,
  "throughput_mb_per_sec": 98.76,
  "cache_size_mib": 512,
  "num_workers": 4
}
```

### libdwarfs API Result
```json
{
  "benchmark_name": {
    "iterations": 3,
    "time": {
      "count": 3,
      "mean": 1.234,
      "median": 1.230,
      "stddev": 0.005,
      "min": 1.229,
      "max": 1.240
    },
    "memory": {
      "count": 3,
      "mean": 8388608.0,
      "median": 8388608.0,
      "stddev": 0.0,
      "min": 8388608.0,
      "max": 8388608.0
    },
    "throughput_mb_per_sec": 78.90,
    "metadata": {
      "image_path": "path/to/image",
      "cache_size": "536870912",
      "num_workers": "4"
    }
  }
}
```
```

**1.2 Document JSON Schemas**

**File**: `benchmarks/schemas/README.md`

Full documentation of all JSON result schemas with examples.

### Phase 2: Fix Path Issue in Comprehensive Script (30 min)

**2.1 Fix benchmark_libdwarfs_api() Function**

**File**: [`benchmarks/run_comprehensive_benchmark.sh`](../benchmarks/run_comprehensive_benchmark.sh:244)

```bash
# Current (line 244):
local test_file=$("$build_dir/dwarfsck" "$image" -l 2>/dev/null | grep -E '\.(pm|pl|pod)$' | head -n 1 || echo "")

# Fix to:
local test_file="/$("$build_dir/dwarfsck" "$image" -l 2>/dev/null | grep -E '\.(pm|pl|pod)$' | head -n 1 || echo "")"
```

Add validation:
```bash
if [[ -z "$test_file" ]] || [[ "$test_file" == "/" ]]; then
  warn "No suitable test file found, skipping single/random benchmarks"
  test_file=""  # Clear it
  return 0  # Not a failure
fi
```

**2.2 Fix File List Generation**

**File**: [`benchmarks/run_comprehensive_benchmark.sh`](../benchmarks/run_comprehensive_benchmark.sh:252-258)

The single_file_bench call needs proper  path with "/" prefix.

### Phase 3: Test Comprehensive System (2-3 hours runtime)

**3.1 Download Dataset**

```bash
python3 benchmarks/download_datasets.py --download perl
```

**3.2 Run Comprehensive Benchmark**

```bash
./benchmarks/run_comprehensive_benchmark.sh
```

**Expected**:
- Builds: fb-only, thrift-only (fail expected), both
- Images: perl-5.43.3.dff, perl-5.43.3.dft
- FUSE benchmarks: 4 combinations
- API benchmarks: 4 combinations
- Report: `results/comprehensive_*/COMPREHENSIVE_REPORT.md`

**3.3 Validate Results**

Check generated files:
```bash
ls -lh results/comprehensive_*/
# Should contain:
# - fuse_*.json (4 files)
# - api_*.json (8 files: 4 single + 4 full)
# - COMPREHENSIVE_REPORT.md
```

### Phase 4: Create Master Orchestrator (1 hour)

**4.1 Create Master Script**

**File**: `benchmarks/run_all_benchmarks.sh`

```bash
#!/usr/bin/env bash
# Master benchmark orchestrator - runs ALL benchmarking systems

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="$PROJECT_ROOT/results/master_${TIMESTAMP}"

mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo "DwarFS MASTER Benchmark Suite"
echo "=========================================="
echo ""
echo "This runs ALL benchmarking systems:"
echo "1. Comprehensive (FUSE vs API, all formats)"
echo "2. Metadata format comparison"
echo "3. Compression algorithm comparison"
echo ""
echo "Expected runtime: 3-4 hours"
echo "Results: $RESULTS_DIR"
echo ""
read -p "Press Enter to continue or Ctrl+C to cancel..."

# 1. Comprehensive benchmarks (FUSE vs API)
echo ""
echo "=" *70
echo "SUITE 1: Comprehensive (FUSE vs API, all formats)"
echo "="*70
./benchmarks/run_comprehensive_benchmark.sh

# Copy results
cp -r results/comprehensive_* "$RESULTS_DIR/"

# 2. Metadata format benchmarks
echo ""
echo "="*70
echo "SUITE 2: Metadata Format Comparison"
echo "="*70
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs build/mkdwarfs \
  --dwarfsextract build/dwarfsextract \
  --dwarfs build/dwarfs \
  --perl-dataset benchmark-files/perl-5.43.3 \
  --output "$RESULTS_DIR/metadata_results.json" \
  --runs 5

# 3. Compression algorithm benchmarks
echo ""
echo "="*70
echo "SUITE 3: Compression Algorithm Comparison"
echo "="*70
python3 benchmarks/compression_algorithm_benchmark.py \
  --mkdwarfs build/mkdwarfs \
  --dataset benchmark-files/perl-5.43.3 \
  --output "$RESULTS_DIR/compression_results.json"

# 4. Generate master report
echo ""
echo "="*70
echo "GENERATING MASTER REPORT"
echo "="*70
python3 benchmarks/generate_master_report.py "$RESULTS_DIR"

echo ""
echo "=========================================="
echo "ALL BENCHMARKS COMPLETE!"
echo "=========================================="
echo ""
echo "Results: $RESULTS_DIR"
echo "Master Report: $RESULTS_DIR/MASTER_REPORT.md"
echo ""
```

**4.2 Create Master Report Generator**

**File**: `benchmarks/generate_master_report.py`

Combines ALL benchmark results into single cohesive report with:
- Executive summary
- FUSE vs API comparison
- FlatBuffers vs Thrift comparison
- Compression algorithm comparison
- Build configuration impact
- Recommendations

### Phase 5: JSON Schema Documentation (1 hour)

**5.1 Create Schema Directory**

```bash
mkdir -p benchmarks/schemas
```

**5.2 Document All Schemas**

**Files**:
- `benchmarks/schemas/fuse_benchmark.json` - FUSE extraction schema
- `benchmarks/schemas/api_benchmark.json` - libdwarfs API schema
- `benchmarks/schemas/compression_benchmark.json` - Compression schema
- `benchmarks/schemas/metadata_benchmark.json` - Metadata schema
- `benchmarks/schemas/README.md` - Schema overview

---

## Expected Deliverables

### 1. Working Comprehensive System

**Command**: `./benchmarks/run_comprehensive_benchmark.sh`

**Output**: `results/comprehensive_*/`
- **FUSE benchmarks**: 4 JSON files
- **API benchmarks**: 8 JSON files (single + full for each config/format)
- **Report**: `COMPREHENSIVE_REPORT.md` with comparison tables

### 2. Master Benchmark System

**Command**: `./benchmarks/run_all_benchmarks.sh`

**Output**: `results/master_*/`
- All comprehensive results
- Metadata format results
- Compression algorithm results
- **Master Report**: Unified analysis

### 3. Complete Documentation

**Files**:
- `doc/BENCHMARKING_SYSTEM_ARCHITECTURE.md` - System overview
- `benchmarks/schemas/README.md` - JSON schemas
- `doc/COMPREHENSIVE_BENCHMARK_GUIDE.md` (exists, validate)
- Updated `benchmarks/README.md` with full system info

### 4. Regression Baseline

**File**: `benchmarks/baselines/v0.16.0.json`

Established baseline for future regression detection.

---

## Success Criteria

### Must Have ✅
- [ ] Comprehensive benchmark runs end-to-end without errors
- [ ] All 12 benchmark combinations complete (3 builds × 2 formats × 2 ops)
- [ ] Report generated with real data
- [ ] JSON schemas documented
- [ ] libdwarfs API integrated into comprehensive script

### Should Have 🎯
- [ ] Master script runs all 3 benchmark suites
- [ ] Unified master report generated
- [ ] Regression baseline established
- [ ] All documentation updated

### Nice to Have ⭐
- [ ] CI/CD integration plan
- [ ] Performance graphs
- [ ] Historical trend tracking

---

## Known Issues

### 1. Path Prefix Issue (Critical)
**File**: `run_comprehensive_benchmark.sh:244`
**Issue**: File paths from `dwarfsck -l` missing "/" prefix
**Fix**: Add "/" prefix when calling benchmarks
**Impact**: Causes "map::at: key not found" in libdwarfs benchmarks

### 2. Build Dependency
**Issue**: Comprehensive script assumes all builds succeed
**Fix**: Handle expected failures (thrift-only currently expected to fail per docs)
**Impact**: May abort entire suite prematurely

### 3. Dataset Availability
**Issue**: Script doesn't check if Perl dataset downloaded
**Fix**: Add pre-flight check or auto-download
**Impact**: Fails early if dataset missing

---

## Quick Start (After Phase 2 Fix)

```bash
# 1. Download dataset
python3 benchmarks/download_datasets.py --download perl

# 2. Run comprehensive benchmark (2-3 hours)
./benchmarks/run_comprehensive_benchmark.sh

# 3. View results
cat results/comprehensive_*/COMPREHENSIVE_REPORT.md
```

---

## File Structure

```
benchmarks/
├── run_comprehensive_benchmark.sh    # Main orchestrator (NEEDS FIX)
├── comprehensive_benchmark.py        # Python alternative
├── run_all_benchmarks.py            # Run all suites (TO CREATE)
├── run_metadata_format_benchmark.py # Metadata formats
├── compression_algorithm_benchmark.py # Compression
├── generate_master_report.py        # Unified report (TO CREATE)
├── lib/                             # Shared utilities (13 modules)
│   ├── build_manager.py
│   ├── dataset_manager.py
│   ├── result_collector.py
│   ├── benchmark_statistics.py
│   └── ...
├── libdwarfs/                       # C++ benchmarks
│   ├── benchmark_framework.h        # Framework (FIXED)
│   ├── single_file_bench.cpp
│   ├── full_extract_bench.cpp
│   └── ...
├── schemas/                         # JSON schemas (TO CREATE)
│   ├── README.md
│   ├── fuse_benchmark.json
│   ├── api_benchmark.json
│   └── ...
└── results/                         # Output directory
    ├── comprehensive_*/
    ├── master_*/
    └── *.json
```

---

## Next Session Start Checklist

- [ ] Read this plan
- [ ] Read `benchmarks/README.md`
- [ ] Read `run_comprehensive_benchmark.sh` (understand structure)
- [ ] Check `benchmarks/lib/` modules (understand Python infrastructure)
- [ ] Verify dataset downloaded (`benchmark-files/perl-5.43.3/`)
- [ ] Start with Phase 2 (fix path issue)

---

**Status**: Plan Complete, Ready for Implementation
**Next**: Fix path issue, run comprehensive benchmark, validate results