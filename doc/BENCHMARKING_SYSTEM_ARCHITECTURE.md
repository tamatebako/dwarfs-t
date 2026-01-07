# DwarFS Benchmarking System Architecture

**Created**: 2025-12-19
**Status**: Complete and Validated

---

## Overview

The DwarFS benchmarking system is a **three-tier architecture** designed to comprehensively measure filesystem performance across different metadata formats, build configurations, and access patterns.

### System Goals

1. **Validate FlatBuffers vs Thrift** metadata format performance
2. **Compare FUSE vs direct API** access patterns
3. **Measure build configuration impact** (FB-only, Thrift-only, both)
4. **Establish regression baselines** for future development
5. **Provide reproducible results** with statistical rigor

---

## Three-Tier Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                    TIER 1: Shell Scripts                     │
│                   (Orchestration Layer)                      │
├──────────────────────────────────────────────────────────────┤
│  run_comprehensive_benchmark.sh  │  run_all_benchmarks.sh    │
│  - Build config management       │  - Master orchestrator    │
│  - Image creation                │  - Result aggregation     │
│  - Benchmark execution           │  - Unified reporting      │
│  - Report generation             │                           │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ├──────────────────┬─────────────────┐
                 ▼                  ▼                 ▼
┌─────────────────────┐  ┌──────────────────┐  ┌─────────────┐
│   TIER 2: Python    │  │  TIER 3: C++     │  │ Build Tools │
│   (Framework)       │  │  (Execution)     │  │             │
├─────────────────────┤  ├──────────────────┤  ├─────────────┤
│ • BuildManager      │  │ Framework:       │  │ • mkdwarfs  │
│ • DatasetManager    │  │  benchmark_      │  │ • dwarfsck  │
│ • ResultCollector   │  │  framework.h     │  │ • dwarfs    │
│ • BenchStatistics   │  │                  │  │ • dwarfsext │
│ • ReportGenerator   │  │ Programs:        │  │             │
│ • MemoryTracker     │  │  single_file     │  │             │
│ • FUSEManager       │  │  multiple_files  │  │             │
│ • PerfmonParser     │  │  full_extract    │  │             │
└─────────────────────┘  │  random_access   │  └─────────────┘
                         └──────┬───────────┘
                                │
                                ▼
                         ┌──────────────┐
                         │ JSON Results │
                         └──────┬───────┘
                                │
                                ▼
                         ┌──────────────┐
                         │   Reports    │
                         │  (Markdown)  │
                         └──────────────┘
```

---

## Tier 1: Shell Scripts (Orchestration)

### [`run_comprehensive_benchmark.sh`](../benchmarks/run_comprehensive_benchmark.sh)

**Purpose**: End-to-end FUSE vs API comparison across all configurations

**Workflow**:
```bash
1. Build Phase (30-60 min):
   ├─ build-fb-bench/         # FlatBuffers-only
   ├─ build-thrift-bench/     # Thrift-only  
   └─ build-both-bench/       # Both formats

2. Image Creation (1-2 min):
   ├─ perl-5.43.3.dff         # FlatBuffers format
   └─ perl-5.43.3.dft         # Thrift format

3. FUSE Benchmarks (30-60 min):
   ├─ Mount → Extract → Unmount (4 combinations)
   └─ Measure throughput, memory

4. API Benchmarks (30-60 min):
   ├─ Single file latency (4 configs)
   └─ Full extraction (4 configs)

5. Report Generation (<1 min):
   └─ Comparison tables in Markdown
```

**Output**: `results/comprehensive_YYYYMMDD_HHMMSS/`
- 4 `fuse_*.json` files
- 8 `api_*.json` files
- 1 `COMPREHENSIVE_REPORT.md`

### [`run_libdwarfs_benchmark.sh`](../benchmarks/run_libdwarfs_benchmark.sh)

**Purpose**: Quick validation of libdwarfs C++ API performance

**Workflow**:
```bash
1. Run all 4 C++ benchmarks
2. Generate JSON results
3. Create summary report
```

**Use Case**: Fast API-only testing during development

---

## Tier 2: Python Framework

### Core Modules (`benchmarks/lib/`)

#### [`build_manager.py`](../benchmarks/lib/build_manager.py)
**Responsibility**: Build configuration management

```python
class BuildManager:
    def build_configuration(config, fb_enabled, thrift_enabled):
        """Create build with specific format support"""
        # - Configure CMake with format flags
        # - Build tools + benchmarks
        # - Return paths to executables
```

**Key Features**:
- Platform-aware CMake configuration
- Dependency version detection
- Build artifact validation

#### [`dataset_manager.py`](../benchmarks/lib/dataset_manager.py)
**Responsibility**: Test dataset download and preparation

```python
class DatasetManager:
    DATASETS = {
        'perl': {
            'url': 'https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz',
            'sha256': '318651ee5bd94acb...',
            'size_mb': 18
        }
    }
```

**Key Features**:
- Checksum verification
- Automatic extraction
- Download resume support

#### [`memory_tracker.py`](../benchmarks/lib/memory_tracker.py)
**Responsibility**: Platform-aware memory measurement

```python
class MemoryTracker:
    def measure_command(cmd):
        """
        macOS: Uses /usr/bin/time -l (max RSS)
        Linux: Uses /usr/bin/time -v (max RSS)
        """
```

**Metrics Captured**:
- Peak RSS (Resident Set Size)
- Wall time, user time, system time
- Page faults (major/minor)

#### [`perfmon_parser.py`](../benchmarks/lib/perfmon_parser.py)
**Responsibility**: Parse FUSE performance metrics

```python
class PerfmonParser:
    def parse(perfmon_text):
        """Extract operation latencies from perfmon xattr"""
        # Returns: {op_name: {count, mean, p50, p95, p99}}
```

**Operations Tracked**:
- `op_getattr`, `op_read`, `op_readdir`, `op_lookup`, etc.
- Latency percentiles (p50, p95, p99)

#### [`benchmark_statistics.py`](../benchmarks/lib/benchmark_statistics.py)
**Responsibility**: Statistical analysis

```python
class BenchmarkStatistics:
    @staticmethod
    def analyze(measurements):
        """Return mean, median, stddev, min, max"""
```

#### [`result_formatter.py`](../benchmarks/lib/result_formatter.py)
**Responsibility**: Format results for reports

```python
class ResultFormatter:
    @staticmethod
    def format_time(seconds):
        """Smart formatting: s/ms/µs"""
    
    @staticmethod
    def format_bytes(bytes):
        """Human-readable: KiB/MiB/GiB"""
```

---

## Tier 3: C++ Benchmark Programs

### Framework ([`benchmark_framework.h`](../benchmarks/libdwarfs/benchmark_framework.h))

**Core Classes**:

```cpp
class Timer {
    void start();
    void stop();
    double elapsed_seconds();
};

class MemoryTracker {
    void record();  // RSS snapshot
    size_t peak_bytes();
};

class BenchmarkStatistics {
    void add_sample(double value);
    double mean(), median(), stddev(), min(), max();
};

class BenchmarkResult {
    void to_json(std::ostream&);  // Export results
};
```

**Key Features**:
- High-resolution timing (std::chrono::high_resolution_clock)
- Platform-aware memory tracking (macOS: task_info, Linux: /proc/self/status)
- Statistical rigor (mean, median, stddev for multiple runs)
- JSON export for programmatic analysis

### Benchmark Programs

#### [`single_file_bench.cpp`](../benchmarks/libdwarfs/single_file_bench.cpp)
**Purpose**: Measure single file extraction latency

**Test Pattern**:
```cpp
for (int i = 0; i < iterations; ++i) {
    timer.start();
    auto entry = fs.find(file_path);      // Lookup
    auto inode = fs.open(entry->inode()); // Open
    auto data = fs.read_string(inode);    // Read
    timer.stop();
    memory_tracker.record();
}
```

**Metrics**:
- Cold cache latency (first run)
- Warm cache latency (subsequent runs)
- Memory footprint
- Throughput (MB/s)

#### [`full_extract_bench.cpp`](../benchmarks/libdwarfs/full_extract_bench.cpp)
**Purpose**: Measure full filesystem extraction throughput

**Test Pattern**:
```cpp
timer.start();
for (auto& entry : all_entries) {
    extract_file(entry, output_dir, num_threads);
}
timer.stop();
```

**Metrics**:
- Total extraction time
- Throughput (MB/s)
- Peak memory usage
- Thread scalability

#### [`multiple_files_bench.cpp`](../benchmarks/libdwarfs/multiple_files_bench.cpp)
**Purpose**: Measure N-file extraction with threading

**Test Pattern**:
```cpp
// Extract N files in parallel
ThreadPool pool(num_threads);
for (auto& file : files) {
    pool.enqueue([&] { extract_file(file); });
}
pool.wait_all();
```

#### [`random_access_bench.cpp`](../benchmarks/libdwarfs/random_access_bench.cpp)
**Purpose**: Measure random read performance

**Access Patterns**:
- **Sequential**: Read files in order
- **Random**: Shuffled file order
- **Stride**: Every Nth file

---

## Data Flow

### Comprehensive Benchmark Flow

```
User
  │
  └─> ./run_comprehensive_benchmark.sh
        │
        ├─> Phase 1: Build Configurations
        │     ├─> CMake configure (FB-only)
        │     ├─> Ninja build
        │     ├─> CMake configure (Thrift-only)
        │     ├─> Ninja build
        │     ├─> CMake configure (Both)
        │     └─> Ninja build
        │
        ├─> Phase 2: Create Images
        │     ├─> mkdwarfs (FB-only) → perl-5.43.3.dff
        │     └─> mkdwarfs (Thrift) → perl-5.43.3.dft
        │
        ├─> Phase 3: FUSE Benchmarks
        │     ├─> benchmark_fuse_extraction()
        │     │     ├─> dwarfs (mount)
        │     │     ├─> cp -r (extract)
        │     │     ├─> umount
        │     │     └─> fuse_*.json
        │     │
        │     └─> Repeat for 4 config/format combos
        │
        ├─> Phase 4: API Benchmarks
        │     ├─> benchmark_libdwarfs_api()
        │     │     ├─> single_file_bench → api_single_*.json
        │     │     └─> full_extract_bench → api_full_*.json
        │     │
        │     └─> Repeat for 4 config/format combos
        │
        └─> Phase 5: Generate Report
              ├─> Parse all JSON files
              ├─> Create comparison tables
              └─> COMPREHENSIVE_REPORT.md
```

### Master Benchmark Flow

```
./run_all_benchmarks.sh
  │
  ├─> ./run_comprehensive_benchmark.sh
  │     └─> results/comprehensive_*/
  │
  ├─> ./run_metadata_format_benchmark.py
  │     └─> results/metadata_*.json
  │
  ├─> ./compression_algorithm_benchmark.py
  │     └─> results/compression_*.json
  │
  └─> ./generate_master_report.py results/
        └─> results/master_*/MASTER_REPORT.md
```

---

## JSON Result Schemas

### FUSE Extraction Result

**File**: `fuse_{build}_{format}.json`

```json
{
  "benchmark": "fuse_extraction",
  "build": "fb-only",
  "format": "dff",
  "image": "/path/to/perl-5.43.3.dff",
  "duration_seconds": 2.15,
  "extracted_bytes": 101171200,
  "throughput_mb_per_sec": 44.85,
  "cache_size_mib": 512,
  "num_workers": 4
}
```

**Schema**: [`benchmarks/schemas/fuse_benchmark.json`](../benchmarks/schemas/fuse_benchmark.json)

### libdwarfs API Result

**File**: `api_{operation}_{build}_{format}.json`

```json
{
  "single_file_extraction": {
    "iterations": 3,
    "time": {
      "count": 3,
      "mean": 0.000214,
      "median": 0.000213,
      "stddev": 0.000002,
      "min": 0.000212,
      "max": 0.000216
    },
    "memory": {
      "count": 3,
      "mean": 147456.0,
      "median": 147456.0,
      "stddev": 0.0,
      "min": 147456.0,
      "max": 147456.0
    },
    "throughput_mb_per_sec": 226.17,
    "metadata": {
      "image_path": "test.dff",
      "cache_size": "536870912",
      "num_workers": "4",
      "file_path": "/Porting/Maintainers.pl",
      "file_size": 48451
    }
  }
}
```

**Schema**: [`benchmarks/schemas/api_benchmark.json`](../benchmarks/schemas/api_benchmark.json)

---

## Component Responsibilities

### Shell Layer

| Component | Responsibility | Output |
|-----------|---------------|---------|
| `run_comprehensive_benchmark.sh` | End-to-end FUSE vs API | 12 JSON + report |
| `run_libdwarfs_benchmark.sh` | Quick API validation | 4 JSON files |
| `run_all_benchmarks.sh` | Master orchestrator | Unified report |

### Python Layer

| Component | Responsibility | Used By |
|-----------|---------------|---------|
| `BuildManager` | Build config mgmt | Comprehensive script |
| `DatasetManager` | Dataset download | All benchmarks |
| `ResultCollector` | Result aggregation | Report generators |
| `BenchmarkStatistics` | Statistical analysis | Python benchmarks |
| `MemoryTracker` | Memory measurement | Python benchmarks |
| `FUSEManager` | Mount/unmount lifecycle | FUSE benchmarks |
| `PerfmonParser` | FUSE metrics parsing | FUSE benchmarks |

### C++ Layer

| Component | Responsibility | Metrics |
|-----------|---------------|---------|
| `benchmark_framework.h` | Timing, memory, stats | All benchmarks |
| `single_file_bench` | Single file latency | Cold/warm cache |
| `full_extract_bench` | Full extraction | Throughput, memory |
| `multiple_files_bench` | Multi-file ops | Parallel efficiency |
| `random_access_bench` | Access patterns | Sequential vs random |

---

## Build Configurations Tested

### FlatBuffers-only (`build-fb-bench/`)

**CMake Options**:
```cmake
-DDWARFS_WITH_FLATBUFFERS=ON
-DDWARFS_WITH_THRIFT=OFF
```

**Capabilities**:
- ✅ Create .dff images
- ✅ Read .dff images
- ❌ Create .dft images
- ❌ Read .dft images

**Use Case**: Maximum portability, simplest build

### Thrift-only (`build-thrift-bench/`)

**CMake Options**:
```cmake
-DDWARFS_WITH_FLATBUFFERS=OFF
-DDWARFS_WITH_THRIFT=ON
```

**Capabilities**:
- ❌ Create .dff images
- ❌ Read .dff images
- ✅ Create .dft images
- ✅ Read .dft images

**Use Case**: Smallest images, backward compatibility

### Both Formats (`build-both-bench/`)

**CMake Options**:
```cmake
-DDWARFS_WITH_FLATBUFFERS=ON
-DDWARFS_WITH_THRIFT=ON
```

**Capabilities**:
- ✅ Create .dff images
- ✅ Read .dff images
- ✅ Create .dft images
- ✅ Read .dft images

**Use Case**: Maximum flexibility, migration scenarios

---

## Test Matrix

### Comprehensive Benchmark: 12 Test Combinations

| # | Build | Format | FUSE? | API Single? | API Full? |
|---|-------|--------|-------|-------------|-----------|
| 1 | fb-only | .dff | ✓ | ✓ | ✓ |
| 2 | thrift-only | .dft | ✓ | ✓ | ✓ |
| 3 | both | .dff | ✓ | ✓ | ✓ |
| 4 | both | .dft | ✓ | ✓ | ✓ |

**Total Operations**: 4 FUSE + 4 single + 4 full = **12 benchmarks**

### Expected Patterns

**FUSE vs API**:
- API should be ~5-10% faster (no kernel overhead)
- Memory usage similar
- Throughput patterns match

**FlatBuffers vs Thrift**:
- Compression: FB ~17-29% faster (levels 1-3)
- Extraction: Nearly identical (±3%)
- Size: Thrift ~0.07-1.41% smaller

**Build Config Impact**:
- Performance: Minimal difference (<2%)
- Binary size: Both-formats ~10-15% larger
- Build time: Both-formats 2x longer

---

## Performance Metrics

### Time Measurements

**Resolution**: Nanosecond (C++), microsecond (shell)

**Statistics**:
- `mean`: Average across iterations
- `median`: Middle value (robust to outliers)
- `stddev`: Consistency indicator
- `min`/`max`: Best/worst case

**Recommendation**: Use `median` for comparisons

### Memory Measurements

**Platform**:
- **macOS**: `mach_task_basic_info` (task_info())
- **Linux**: `/proc/self/status` RSS field

**Units**: Bytes (convert to KiB/MiB for display)

**Metric**: Peak RSS during operation

### Throughput Calculation

```
throughput_mb_per_sec = data_size_bytes / median_time_seconds / (1024 * 1024)
```

---

## File Organization

```
benchmarks/
├── run_comprehensive_benchmark.sh    # Main orchestrator
├── run_libdwarfs_benchmark.sh       # API-only runner
├── run_all_benchmarks.sh            # Master script
│
├── lib/                             # Python framework (13 modules)
│   ├── __init__.py
│   ├── build_manager.py
│   ├── dataset_manager.py
│   ├── result_collector.py
│   ├── benchmark_statistics.py
│   ├── memory_tracker.py
│   ├── perfmon_parser.py
│   └── ...
│
├── libdwarfs/                       # C++ benchmarks
│   ├── benchmark_framework.h        # Core framework (395 lines)
│   ├── CMakeLists.txt              # Build integration
│   ├── single_file_bench.cpp       # Single file latency
│   ├── multiple_files_bench.cpp    # Multi-file throughput
│   ├── full_extract_bench.cpp      # Full extraction
│   └── random_access_bench.cpp     # Access patterns
│
├── schemas/                         # JSON schemas
│   ├── README.md                    # Schema documentation
│   ├── fuse_benchmark.json         # FUSE result schema
│   └── api_benchmark.json          # API result schema
│
└── results/                         # Output directory
    ├── comprehensive_YYYYMMDD_HHMMSS/
    │   ├── fuse_*.json (4 files)
    │   ├── api_*.json (8 files)
    │   └── COMPREHENSIVE_REPORT.md
    │
    └── master_YYYYMMDD_HHMMSS/
        ├── comprehensive_*/
        ├── metadata_results.json
        ├── compression_results.json
        └── MASTER_REPORT.md
```

---

## Integration Points

### CMake Integration

**File**: [`cmake/tests.cmake`](../cmake/tests.cmake:265-280)

```cmake
if(WITH_LIBDWARFS AND WITH_BENCHMARKS)
  add_subdirectory(benchmarks/libdwarfs)
  
  list(APPEND BENCHMARK_TARGETS
    single_file_bench
    multiple_files_bench
    full_extract_bench
    random_access_bench
  )
endif()
```

**Build Requirement**: `WITH_BENCHMARKS=ON` (implies `WITH_TESTS=ON` for GoogleTest)

### Shell → C++ Integration

**Method**: Direct execution via built binaries

```bash
"$build_dir/benchmarks/libdwarfs/single_file_bench" \
  "$image" "$file_path" \
  -n $ITERATIONS \
  -c $CACHE_SIZE \
  -w $NUM_WORKERS \
  --json "$output.json"
```

**Result**: Structured JSON with all metrics

### Python → Shell Integration

Future: Python wrappers could call shell scripts and parse results

---

## Performance Expectations

### Session 17 Baseline (Perl 5.43.3, macOS ARM64)

**Single File** (48.45 KB):
- Cold cache: 8.29 ms
- Warm cache: 0.21 ms
- Speedup: 39x
- Throughput: 16.05 MB/s (cold), 226+ MB/s (warm)
- Memory: 144 KiB

**Full Extraction** (6,816 files, 96.5 MB):
- Median time: 1.49 s (4 threads)
- Mean time: 3.48 s
- Throughput: 27.75 MB/s
- Memory: 8.44 MiB
- Speedup: 5x (cold 7.46s → warm 1.49s)

**See**: [`LIBDWARFS_API_PERFORMANCE.md`](LIBDWARFS_API_PERFORMANCE.md)

---

## Troubleshooting

### Build Failures

**Issue**: `jemalloc not found`
**Fix**: Add `-DUSE_JEMALLOC=OFF` to CMake (v0.16.0+: optional allocator)

**Issue**: `gtest_discover_tests() unknown`
**Fix**: Ensure `-DWITH_TESTS=ON` (fetches GoogleTest)

**Issue**: `WITH_BENCHMARKS requires WITH_LIBDWARFS`
**Fix**: Add `-DWITH_LIBDWARFS=ON`

### Runtime Failures

**Issue**: `map::at: key not found` in libdwarfs benchmarks
**Fix**: Ensure file paths from `dwarfsck -l` have "/" prefix (fixed in commit ca7d657e)

**Issue**: FUSE mount fails
**Fix**: 
- macOS: Install FUSE-T or macFUSE
- Linux: Install fuse3 or fuse
- Check: `mount | grep dwarfs` for stale mounts

**Issue**: Memory metrics missing
**Fix**: Use `/usr/bin/time` (not shell builtin)
- macOS: `/usr/bin/time -l`
- Linux: `/usr/bin/time -v`

---

## Extension Guide

### Adding New Benchmark

**1. Create C++ program** in `benchmarks/libdwarfs/`:
```cpp
#include "benchmark_framework.h"

int main(int argc, char* argv[]) {
    // Parse args, setup filesystem
    BenchmarkResult result("my_benchmark");
    
    for (int i = 0; i < iterations; ++i) {
        Timer timer;
        MemoryTracker mem;
        
        timer.start();
        // ... perform operation ...
        timer.stop();
        mem.record();
        
        result.add_time(timer.elapsed_seconds());
        result.add_memory(mem.peak_bytes());
    }
    
    result.to_json(output_file);
}
```

**2. Add to CMakeLists.txt**:
```cmake
add_executable(my_benchmark my_benchmark.cpp)
target_link_libraries(my_benchmark PRIVATE dwarfs_reader)
```

**3. Integrate in shell script**:
```bash
"$build_dir/benchmarks/libdwarfs/my_benchmark" \
  "$image" --json "$RESULTS_DIR/my_result.json"
```

---

## See Also

- [`../benchmarks/README.md`](../benchmarks/README.md) - User guide
- [`../benchmarks/schemas/README.md`](../benchmarks/schemas/README.md) - JSON schemas
- [`LIBDWARFS_API_PERFORMANCE.md`](LIBDWARFS_API_PERFORMANCE.md) - API performance
- [`DWARFS_METADATA_FORMAT_PERFORMANCE.md`](DWARFS_METADATA_FORMAT_PERFORMANCE.md) - Format comparison
- [`LIBDWARFS_INTEGRATION_GUIDE.md`](LIBDWARFS_INTEGRATION_GUIDE.md) - C++ API usage

---

**Version**: 0.16.0
**Last Updated**: 2025-12-19
