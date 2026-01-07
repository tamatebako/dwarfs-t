# Phase G: Comprehensive Benchmark Suite - Implementation Plan

**Date**: 2025-11-30  
**Current Status**: Phase F complete (basic benchmarks only)  
**Next Phase**: G (Comprehensive testing across all tools and datasets)  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`

---

## Problem Statement

**Current Limitation**: Phase F only tested:
- ❌ One tiny dataset (11 files, 213 bytes)
- ❌ One tool (mkdwarfs only)
- ❌ One operation (creation only)

**Required**: Comprehensive benchmark suite testing:
- ✅ Multiple datasets (small, medium, large)
- ✅ All 4 CLI tools (mkdwarfs, dwarfsck, dwarfsextract, dwarfs)
- ✅ Complete workflows (create → verify → extract → mount → read)
- ✅ Both metadata formats (FlatBuffers vs Thrift)

---

## Available Datasets

From [`benchmarks/download_datasets.py`](../benchmarks/download_datasets.py):

### 1. Perl 5.43.3 (Small-Medium)
- **Files**: 6,802 files
- **Size**: ~95 MB uncompressed
- **Type**: Source code (high redundancy)
- **Use case**: Typical software project

### 2. Raspberry Pi OS Lite ARM64 (Large)
- **Files**: 1 large file
- **Size**: ~2.7 GB compressed image
- **Type**: OS image (low redundancy)
- **Use case**: Large single file

### 3. Custom Test Dataset (Tiny)
- **Files**: 11 files
- **Size**: 213 bytes
- **Type**: Minimal test
- **Use case**: Quick validation

---

## Phase G Objectives

### Unified Benchmark System

**Key Principle**: ONE script runs ALL benchmarks across ALL tools, ALL datasets, and ALL operations.

### G1: Dataset Management
- Download and verify all benchmark datasets
- Create consistent directory structure
- Document dataset characteristics

### G2: Unified Benchmark Runner (SINGLE SCRIPT)
Create **ONE comprehensive script** that:
- Tests ALL 4 CLI tools (mkdwarfs, dwarfsck, dwarfsextract, dwarfs)
- Uses ALL datasets (tiny, perl, raspi)
- Runs ALL operations per tool:
  * **mkdwarfs**: Create with levels 1,5,9 (both formats)
  * **dwarfsck**: Quick check, full validation, JSON export
  * **dwarfsextract**: Extract all, extract one file, convert to tar
  * **dwarfs**: Mount, seq read, random read, directory traversal
- Generates ONE unified JSON with all results
- Orchestrates entire benchmark workflow

### G3: Unified Report Generator (SINGLE SCRIPT)
- Reads ONE comprehensive JSON
- Generates ONE complete report with:
  * All tools analyzed
  * Format comparison across all operations
  * Performance profiles per dataset
  * Recommendations per use case

---

## Implementation Plan

### G1: Dataset Management (1-2 hours)

**Tasks**:
1. Use existing `download_datasets.py` to fetch Perl 5.43.3
2. Document dataset characteristics
3. Create benchmark directory structure
4. Verify checksums

**Deliverable**: All datasets ready in `benchmark-files/`

**Script**: Enhance existing `download_datasets.py`

---

### G2: Unified Benchmark Runner (6-8 hours)

**ONE SCRIPT TO RULE THEM ALL**: `benchmarks/run_all_benchmarks.py`

This single script orchestrates the entire benchmark workflow:

#### Architecture

```python
class BenchmarkRunner:
    def __init__(self, tools, datasets, iterations):
        self.tools = {
            'flatbuffers': {'mkdwarfs', 'dwarfsck', 'dwarfsextract', 'dwarfs'},
            'thrift': {'mkdwarfs', 'dwarfsck', 'dwarfsextract', 'dwarfs'}
        }
        self.datasets = datasets
        self.iterations = iterations
        self.results = {}
    
    def run_all(self):
        """Main orchestrator - runs EVERYTHING"""
        for dataset in self.datasets:
            self.results[dataset] = {}
            
            # 1. CREATE images (mkdwarfs)
            images = self.benchmark_mkdwarfs(dataset)
            
            # 2. VERIFY images (dwarfsck)
            self.benchmark_dwarfsck(images)
            
            # 3. EXTRACT from images (dwarfsextract)
            self.benchmark_dwarfsextract(images)
            
            # 4. MOUNT and ACCESS images (dwarfs)
            if self.fuse_available():
                self.benchmark_dwarfs_fuse(images)
        
        return self.results
```

#### Operations per Tool

**mkdwarfs** (Creation):
- Compression levels: 1, 5, 9
- Both formats: FlatBuffers, Thrift
- Metrics: time, size, compression ratio, memory
- Saves images: `{dataset}-{format}-l{level}.{dff|dft}`

**dwarfsck** (Verification):
- Quick check (`--check-integrity`)
- Full validation (default)
- JSON export (`--json`)
- Metrics: time, throughput

**dwarfsextract** (Extraction):
- Extract all files to directory
- Extract single file (largest file in dataset)
- Convert to tar archive
- Metrics: time, throughput, memory

**dwarfs** (FUSE Operations):
- Mount time
- Sequential read (read entire large file)
- Random read (seek to random offsets)
- Directory traversal (`find` command)
- Metadata operations (stat all files)
- Metrics: latency, throughput, ops/sec

#### Usage

```bash
python3 benchmarks/run_all_benchmarks.py \
  --flatbuffers-tools ./build-fb \
  --thrift-tools ./build-tb \
  --datasets tiny,perl \
  --levels 1,5,9 \
  --iterations 5 \
  --output benchmark-results/complete-benchmarks.json
```

#### Output JSON Structure

```json
{
  "metadata": {
    "date": "2025-11-30T...",
    "tools": {
      "flatbuffers": {"mkdwarfs": "path", "dwarfsck": "path", ...},
      "thrift": {...}
    },
    "datasets": ["tiny", "perl"],
    "iterations": 5
  },
  "results": {
    "tiny": {
      "mkdwarfs": {
        "flatbuffers": {"l1": {...}, "l5": {...}, "l9": {...}},
        "thrift": {...}
      },
      "dwarfsck": {
        "images": {
          "tiny-fb-l5.dff": {"quick": {...}, "full": {...}, "json": {...}}
        }
      },
      "dwarfsextract": {
        "images": {
          "tiny-fb-l5.dff": {"extract_all": {...}, "extract_one": {...}, "to_tar": {...}}
        }
      },
      "dwarfs": {
        "images": {
          "tiny-fb-l5.dff": {"mount": {...}, "seq_read": {...}, "rand_read": {...}, "find": {...}, "stat": {...}}
        }
      }
    },
    "perl": {...}
  }
}
```

**Implementation Time**: 6-8 hours for complete unified runner

---

### G3: Unified Report Generator (2-3 hours)

**Enhancement**: Extend `metadata_format_benchmark.py`

**New Features**:
- Support multiple datasets
- Configurable compression levels
- Memory usage tracking
- Compression ratio analysis

**Usage**:
```bash
python3 benchmarks/comprehensive_mkdwarfs_benchmark.py \
  --flatbuffers-mkdwarfs ./build-fb/mkdwarfs \
  --thrift-mkdwarfs ./build-tb/mkdwarfs \
  --datasets tiny,perl,raspi \
  --levels 1,5,9 \
  --iterations 5 \
  --output results/mkdwarfs-benchmarks.json
```

**Metrics**:
- Creation time (real, user, sys)
- Image size (bytes, compression ratio)
- Memory usage (peak RSS)
- Throughput (MB/s)

**Output**: JSON with per-dataset, per-format, per-level results

---

### G3: dwarfsck Benchmarks (1-2 hours)

**New Script**: `benchmarks/benchmark_dwarfsck.py`

**Operations to Test**:
1. **Quick Check** (`--check-integrity`)
   - Verify file structure only
   - Measure: time

2. **Full Validation** (default)
   - Verify all chunks
   - Measure: time, throughput (MB/s)

3. **JSON Export** (`--json`)
   - Export metadata
   - Measure: time, JSON size

**Usage**:
```bash
python3 benchmarks/benchmark_dwarfsck.py \
  --dwarfsck ./build-fb/dwarfsck \
  --images results/mkdwarfs-benchmarks/*.dff \
  --operations quick,full,json \
  --iterations 3 \
  --output results/dwarfsck-benchmarks.json
```

**Metrics**:
- Verification time
- Throughput (MB/s for full validation)
- JSON export size

---

### G4: dwarfsextract Benchmarks (2-3 hours)

**New Script**: `benchmarks/benchmark_dwarfsextract.py`

**Operations to Test**:
1. **Full Extraction** (to directory)
   - Extract entire filesystem
   - Measure: time, throughput, memory

2. **Selective Extraction** (specific paths)
   - Extract single file
   - Extract directory subtree
   - Measure: time, throughput

3. **Archive Conversion** (to tar/cpio)
   - Convert to tar format
   - Measure: time, throughput

**Usage**:
```bash
python3 benchmarks/benchmark_dwarfsextract.py \
  --dwarfsextract ./build-fb/dwarfsextract \
  --images results/mkdwarfs-benchmarks/*.dff \
  --operations full,selective,tar \
  --iterations 3 \
  --output results/dwarfsextract-benchmarks.json
```

**Metrics**:
- Extraction time
- Throughput (MB/s)
- Memory usage
- File count/size

---

### G5: dwarfs FUSE Benchmarks (3-4 hours)

**New Script**: `benchmarks/benchmark_dwarfs_fuse.py`

**Operations to Test**:
1. **Mount Time**
   - Time to mount
   - Measure: mount latency

2. **Sequential Read**
   - Read entire large file
   - Measure: throughput (MB/s)

3. **Random Read**
   - Read random offsets in files
   - Measure: latency, IOPS

4. **Directory Traversal**
   - `find` entire tree
   - `ls -lR` entire tree
   - Measure: time, operations/sec

5. **Metadata Operations**
   - stat() on many files
   - Measure: operations/sec

**Usage**:
```bash
python3 benchmarks/benchmark_dwarfs_fuse.py \
  --dwarfs ./build-fb/dwarfs \
  --images results/mkdwarfs-benchmarks/*.dff \
  --operations mount,seq-read,rand-read,find,stat \
  --iterations 3 \
  --output results/dwarfs-fuse-benchmarks.json
```

**Metrics**:
- Mount time
- Read throughput (MB/s)
- Read latency (ms)
- Metadata ops/sec
- Directory traversal time

**Challenges**:
- Requires FUSE support (macFUSE/FUSE-T on macOS)
- Needs mount point management
- Proper cleanup after tests

---

### G6: Unified Report Generation (2-3 hours)

**Enhancement**: Extend `generate_metadata_report.py`

**New Script**: `benchmarks/generate_comprehensive_report.py`

**Features**:
- Aggregate data from all benchmark JSONs
- Format comparison across all tools
- Per-dataset performance profiles
- Bottleneck analysis
- Recommendations per use case

**Usage**:
```bash
python3 benchmarks/generate_comprehensive_report.py \
  --mkdwarfs-results results/mkdwarfs-benchmarks.json \
  --dwarfsck-results results/dwarfsck-benchmarks.json \
  --dwarfsextract-results results/dwarfsextract-benchmarks.json \
  --dwarfs-fuse-results results/dwarfs-fuse-benchmarks.json \
  --output doc/COMPREHENSIVE_BENCHMARK_REPORT.md
```

**Report Sections**:
1. **Executive Summary**
   - Key findings
   - Format recommendation
   - Performance highlights

2. **mkdwarfs Analysis**
   - Creation performance per dataset
   - Compression ratios
   - Format comparison

3. **dwarfsck Analysis**
   - Verification performance
   - Validation thoroughness

4. **dwarfsextract Analysis**
   - Extraction performance
   - Format conversion speed

5. **dwarfs FUSE Analysis**
   - Mount performance
   - Read performance
   - Metadata operations

6. **Format Comparison Matrix**
   - FlatBuffers vs Thrift across all tools
   - Use case recommendations

7. **Performance Profiles**
   - Small files (Perl dataset)
   - Large files (RasPi dataset)
   - Mixed workloads

8. **Recommendations**
   - When to use FlatBuffers
   - When to keep Thrift support
   - Performance tuning tips

---

## Implementation Order

### Simplified Workflow (2 days)

**Day 1** (7-10 hours):
1. **G1**: Dataset management (1-2h)
2. **G2**: Unified benchmark runner - `run_all_benchmarks.py` (6-8h)
   - ONE script that runs ALL tools
   - Tests ALL operations on ALL datasets
   - Generates ONE comprehensive JSON with everything

**Day 2** (2-3 hours):
3. **G3**: Unified report generator - `generate_complete_report.py` (2-3h)
   - Reads the comprehensive JSON
   - Generates ONE complete report
4. Documentation updates
5. Final validation

**Total Estimate**: 9-13 hours over 2 days

**Key Benefits**:
- Unified approach is MORE EFFICIENT
- ENSURES CONSISTENCY across all tests
- ONE command runs EVERYTHING
- Easier to reproduce and automate

---

## Directory Structure

```
benchmarks/
├── download_datasets.py              # Existing
├── metadata_format_benchmark.py      # Phase F (basic)
├── generate_metadata_report.py       # Phase F (basic)
├── comprehensive_mkdwarfs_benchmark.py   # G2 - NEW
├── benchmark_dwarfsck.py                 # G3 - NEW
├── benchmark_dwarfsextract.py            # G4 - NEW
├── benchmark_dwarfs_fuse.py              # G5 - NEW
├── generate_comprehensive_report.py      # G6 - NEW
└── lib/                                  # Shared utilities
    ├── dataset_manager.py                # Dataset handling
    ├── memory_tracker.py                 # Memory monitoring
    ├── fuse_manager.py                   # FUSE mount/unmount
    └── report_utils.py                   # Common report functions

benchmark-files/
├── tiny/                    # 11 files, 213 bytes
├── perl-5.43.3/            # 6,802 files, ~95 MB
└── raspi-os-lite-arm64/    # 1 file, ~2.7 GB

benchmark-results/
├── mkdwarfs-benchmarks.json
├── dwarfsck-benchmarks.json
├── dwarfsextract-benchmarks.json
├── dwarfs-fuse-benchmarks.json
└── images/                    # Generated test images
    ├── tiny-fb-l1.dff
    ├── tiny-tb-l1.dft
    ├── perl-fb-l5.dff
    ├── perl-tb-l5.dft
    ├── raspi-fb-l9.dff
    └── raspi-tb-l9.dft

doc/
├── COMPREHENSIVE_BENCHMARK_REPORT.md   # Final unified report
└── PHASE_G_IMPLEMENTATION_STATUS.md    # Progress tracking
```

---

## Success Criteria

Phase G is complete when:

- [ ] All 3 datasets downloaded and verified
- [ ] mkdwarfs benchmarked on all datasets with both formats
- [ ] dwarfsck benchmarked on all generated images
- [ ] dwarfsextract benchmarked with full/selective/tar operations
- [ ] dwarfs FUSE benchmarked with mount/read/metadata operations
- [ ] Comprehensive report generated with all data
- [ ] Format recommendations documented
- [ ] Performance profiles per use case documented
- [ ] All benchmark data reproducible

---

## Risk Mitigation

### Risk: FUSE availability
**Impact**: Cannot test dwarfs tool  
**Mitigation**: 
- Detect FUSE support at runtime
- Skip FUSE tests if unavailable
- Document platform requirements

### Risk: Large dataset download time
**Impact**: Extended setup time  
**Mitigation**:
- Cache datasets between runs
- Verify checksums before download
- Provide skip option if datasets exist

### Risk: Benchmark duration
**Impact**: Long test runs  
**Mitigation**:
- Configurable iteration counts
- Quick mode (1 iteration) for validation
- Full mode (10+ iterations) for accuracy

---

## Documentation Updates

After Phase G, update:

1. **README.md**:
   - Add "Benchmarking" section
   - Link to comprehensive report
   - Include usage examples

2. **doc/COMPREHENSIVE_BENCHMARK_REPORT.md**:
   - NEW: Complete benchmark results
   - Format comparison across all tools
   - Performance recommendations

3. **benchmarks/README.md**:
   - NEW: Benchmark suite documentation
   - Usage instructions for each script
   - Dataset descriptions

4. **Archive old docs**:
   - Move Phase F completion docs to `doc/old-docs/phase-f/`
   - Keep only relevant, up-to-date documentation

---

## Next Steps

1. Read this plan
2. Read [`doc/PHASE_G_IMPLEMENTATION_STATUS.md`](PHASE_G_IMPLEMENTATION_STATUS.md)
3. Read [`doc/PHASE_G_CONTINUATION_PROMPT.md`](PHASE_G_CONTINUATION_PROMPT.md)
4. Begin with G1 (dataset management)

---

**Last Updated**: 2025-11-30  
**Status**: Planning complete, ready for implementation  
**Estimated Time**: 11-16 hours over 2-3 days