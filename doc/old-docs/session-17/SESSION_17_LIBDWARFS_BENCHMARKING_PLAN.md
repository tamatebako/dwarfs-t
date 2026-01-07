# Session 17: libdwarfs API Benchmarking Plan

**Created**: 2025-12-19
**Prerequisite**: Session 16 Phase 1 Complete (Metadata Format Validation)
**Focus**: Library API Performance Benchmarking
**Priority**: HIGH (Critical for library adoption)
**Estimated Time**: 3-4 hours

---

## Context

### What We've Done (Session 16 Phase 1)

✅ **Metadata Format Comparison**:
- Compared FlatBuffers vs Thrift metadata serialization
- Validated FlatBuffers as production-ready default
- Created automated benchmarking tools
- Verified byte-for-byte identical extracted files

### What We Need Next (Session 17 Phase 2)

🎯 **Library API Performance Benchmarking**:
- Test `libdwarfs` C++ API performance
- Compare against FUSE-based extraction
- Measure memory usage, CPU time, throughput
- Benchmark typical filesystem operations (single file, multiple files, full extraction)
- Provide developers with performance characteristics for library integration

---

## Objectives

### Primary Goals

1. **Benchmark libdwarfs Reader API** (dwarfs_reader library)
   - Single file extraction
   - Multiple file extraction
   - Full filesystem extraction
   - Random access patterns
   - Sequential access patterns

2. **Measure Resource Usage**
   - Memory footprint (RSS, VSZ, heap)
   - CPU time (user, system)
   - I/O throughput (MB/s)
   - Cache effectiveness

3. **Compare Against FUSE**
   - FUSE driver extraction (via mount)
   - dwarfsextract tool (current baseline)
   - Direct library API (new benchmark)

4. **Test Metadata Format Impact**
   - FlatBuffers metadata reading
   - Thrift metadata reading
   - Cache warm vs cold scenarios

### Secondary Goals

1. **Create C++ Benchmark Suite**
   - Reusable benchmark programs
   - Link against libdwarfs
   - Production-quality code

2. **Document API Performance**
   - Performance characteristics
   - Best practices for integration
   - Memory tuning guidelines

3. **Automated Testing**
   - One-command benchmark execution
   - Timestamped reports
   - CI/CD integration ready

---

## Technical Scope

### libdwarfs API to Benchmark

**Primary API** ([`include/dwarfs/reader/filesystem_v2.h`](../include/dwarfs/reader/filesystem_v2.h)):
```cpp
class filesystem_v2 {
  // Directory Operations
  find(path) → entry
  walk(path, callback)

  // File Reading
  open(inode) → file_handle
  read(file_handle, offset, size) → data

  // Metadata
  getattr(entry) → stat_info
  readdir(entry) → vector<entries>
};
```

**Helper APIs**:
- `filesystem_loader` - High-level loading
- `inode_reader_v2` - Low-level chunk reading
- `block_cache` - Cache management

### Benchmark Scenarios

#### 1. Single File Extraction

**Test**: Extract one specific file from DwarFS image

**Metrics**:
- Latency (ms)
- Memory usage (MB)
- Cache impact (warm vs cold)

**Variations**:
- Small file (<1 KB)
- Medium file (1-100 MB)
- Large file (>100 MB)
- Fragmented file (many chunks)
- Contiguous file (few chunks)

#### 2. Multiple File Extraction

**Test**: Extract N files from image

**Metrics**:
- Throughput (MB/s)
- Overhead per file (ms)
- Memory growth
- Cache hit rate

**Variations**:
- 10 files
- 100 files
- 1000 files
- Random distribution
- Directory tree

#### 3. Full Filesystem Extraction

**Test**: Extract entire filesystem to disk

**Metrics**:
- Total time (s)
- Throughput (MB/s)
- Peak memory (MB)
- CPU utilization (%)

**Variations**:
- Sequential extraction
- Parallel extraction (multi-threaded)
- Different compression levels

#### 4. Random Access Patterns

**Test**: Read random offsets from files

**Metrics**:
- Random read latency (ms)
- Block cache efficiency
- Decompression overhead

**Variations**:
- Sequential reads
- Random reads
- Mixed patterns

#### 5. Metadata Operations

**Test**: Filesystem traversal without extraction

**Metrics**:
- readdir() latency
- getattr() latency
- Memory usage

**Variations**:
- FlatBuffers metadata
- Thrift metadata
- Different image sizes

### Comparison Baselines

#### Baseline 1: dwarfsextract (Current Tool)

**How**: Time the existing `dwarfsextract` tool

**Why**: Current production extraction method

**Command**:
```bash
time dwarfsextract -i image.dff -o output/
```

#### Baseline 2: FUSE Mount + cp

**How**: Mount image, copy files via filesystem

**Why**: FUSE overhead comparison

**Commands**:
```bash
dwarfs image.dff mnt/
time cp -r mnt/source output/
fusermount -u mnt/
```

#### Baseline 3: Direct Library API (New)

**How**: C++ program using `filesystem_v2` API

**Why**: Direct API performance without FUSE overhead

**Example**:
```cpp
auto fs = filesystem_v2::open(image_path);
auto entry = fs->find("/path/to/file");
auto data = fs->read(entry);
write_to_disk(data);
```

---

## Implementation Plan

### Phase 2.1: C++ Benchmark Programs (1.5 hours)

#### Task 2.1.1: Create Benchmark Framework

**File**: `benchmarks/libdwarfs/benchmark_framework.h`

**Purpose**: Reusable benchmarking utilities

**Features**:
- Timer utilities (high-resolution)
- Memory tracking (RSS, heap)
- Statistical analysis (mean, median, stddev)
- Result JSON output

**Example**:
```cpp
class BenchmarkRunner {
  void run(string name, function<void()> test);
  void report(ostream& out);

private:
  map<string, BenchmarkResult> results_;
};
```

#### Task 2.1.2: Single File Extraction Benchmark

**File**: `benchmarks/libdwarfs/single_file_bench.cpp`

**Purpose**: Benchmark extracting one file

**Usage**:
```bash
./single_file_bench image.dff /path/to/file [iterations]
```

**Metrics**:
- Extraction time
- Memory usage
- Cache statistics

#### Task 2.1.3: Multiple File Extraction Benchmark

**File**: `benchmarks/libdwarfs/multiple_files_bench.cpp`

**Purpose**: Benchmark extracting N files

**Usage**:
```bash
./multiple_files_bench image.dff file_list.txt [threads]
```

**Features**:
- Single-threaded extraction
- Multi-threaded extraction
- Progress reporting

#### Task 2.1.4: Full Extraction Benchmark

**File**: `benchmarks/libdwarfs/full_extract_bench.cpp`

**Purpose**: Benchmark full filesystem extraction

**Usage**:
```bash
./full_extract_bench image.dff output_dir [threads]
```

**Metrics**:
- Total time
- Throughput (MB/s)
- Peak memory
- CPU utilization

#### Task 2.1.5: Random Access Benchmark

**File**: `benchmarks/libdwarfs/random_access_bench.cpp`

**Purpose**: Benchmark random read patterns

**Usage**:
```bash
./random_access_bench image.dff /path/to/file [pattern]
```

**Patterns**:
- Sequential (baseline)
- Random offsets
- Stride patterns

### Phase 2.2: Automated Test Suite (30 min)

#### Task 2.2.1: Build System Integration

**File**: `benchmarks/libdwarfs/CMakeLists.txt`

**Purpose**: Build all benchmark programs

**Targets**:
- `libdwarfs_benchmarks` - All programs
- Individual benchmark targets

**Example**:
```cmake
add_executable(single_file_bench single_file_bench.cpp)
target_link_libraries(single_file_bench dwarfs_reader)
```

#### Task 2.2.2: Automation Script

**File**: `benchmarks/run_libdwarfs_benchmark.sh`

**Purpose**: Run all benchmarks with one command

**Usage**:
```bash
./benchmarks/run_libdwarfs_benchmark.sh [--skip-build]
```

**Steps**:
1. Build benchmark programs
2. Create test images (if needed)
3. Run all benchmarks
4. Collect results
5. Generate report

**Output**: `doc/LIBDWARFS_BENCHMARK_RESULTS_<timestamp>.md`

### Phase 2.3: Comparison Analysis (1 hour)

#### Task 2.3.1: FUSE Baseline

**Method**: Time FUSE-based extraction

**Metrics**:
- Mount time
- Extraction time (cp -r)
- Unmount time
- Total time

#### Task 2.3.2: dwarfsextract Baseline

**Method**: Time existing tool

**Metrics**:
- Extraction time
- Memory usage
- Throughput

#### Task 2.3.3: libdwarfs API

**Method**: Time direct API usage

**Metrics**:
- Extraction time (comparable)
- Memory overhead
- API call overhead

#### Task 2.3.4: Comparative Report

**Create**: Comparison matrix

**Example**:
```markdown
| Method | Time | Memory | Throughput | Overhead |
|--------|------|--------|------------|----------|
| FUSE + cp | 5.2s | 50 MB | 18 MB/s | Mount overhead |
| dwarfsextract | 2.1s | 100 MB | 45 MB/s | Tool startup |
| libdwarfs API | 2.0s | 80 MB | 47 MB/s | Minimal |
```

### Phase 2.4: Documentation (30 min)

#### Task 2.4.1: Performance Report

**File**: `doc/LIBDWARFS_API_PERFORMANCE.md`

**Sections**:
- Executive summary
- Benchmark methodology
- Results tables
- Performance characteristics
- Integration guidelines

#### Task 2.4.2: Integration Guide

**File**: `doc/LIBDWARFS_INTEGRATION_GUIDE.md`

**Sections**:
- Quick start example
- API overview
- Performance tuning
- Memory management
- Multi-threading

#### Task 2.4.3: Update Architecture Docs

**File**: `.kilocode/rules/memory-bank/architecture.md`

**Add**: libdwarfs API performance section

---

## Success Criteria

### Must Have ✅

- [ ] C++ benchmark programs compile and run
- [ ] Single file extraction benchmarked
- [ ] Multiple file extraction benchmarked
- [ ] Full extraction benchmarked
- [ ] Comparison against FUSE and dwarfsextract
- [ ] Automated benchmark script
- [ ] Performance report generated

### Should Have 🎯

- [ ] Random access patterns tested
- [ ] Multi-threading performance tested
- [ ] Memory profiling complete
- [ ] Integration guide created

### Nice to Have ⭐

- [ ] Cache effectiveness analysis
- [ ] Different compression level impact
- [ ] Platform comparison (macOS vs Linux)
- [ ] CI/CD integration

---

## Expected Results

### Performance Characteristics

**Single File** (10 MB file):
- Latency: ~20-50 ms (cold cache)
- Latency: ~5-10 ms (warm cache)
- Memory: ~100-200 MB

**Full Extraction** (100 MB filesystem):
- Time: ~2-5 seconds
- Throughput: ~20-50 MB/s
- Memory: ~100-500 MB

**API Overhead**:
- libdwarfs: Minimal (~5% vs tool)
- FUSE: Moderate (~20-30% overhead)

### Use Cases

**Library Integration Good For**:
- Custom extraction logic
- Selective file access
- Direct integration in applications
- Performance-critical scenarios

**FUSE Good For**:
- Standard filesystem access
- Existing tools compatibility
- Read-only mounting
- Development/debugging

---

## Files to Create

### C++ Programs
- `benchmarks/libdwarfs/benchmark_framework.h` (~200 lines)
- `benchmarks/libdwarfs/single_file_bench.cpp` (~150 lines)
- `benchmarks/libdwarfs/multiple_files_bench.cpp` (~200 lines)
- `benchmarks/libdwarfs/full_extract_bench.cpp` (~250 lines)
- `benchmarks/libdwarfs/random_access_bench.cpp` (~150 lines)
- `benchmarks/libdwarfs/CMakeLists.txt` (~50 lines)

### Scripts
- `benchmarks/run_libdwarfs_benchmark.sh` (~300 lines)

### Documentation
- `doc/LIBDWARFS_API_PERFORMANCE.md` (~400 lines)
- `doc/LIBDWARFS_INTEGRATION_GUIDE.md` (~500 lines)

**Total**: ~2,200 lines of new code + documentation

---

## Dependencies

### Build Requirements
- CMake ≥3.28
- C++20 compiler
- libdwarfs (built from same tree)

### Runtime Requirements
- DwarFS images (test dataset)
- Sufficient memory (~1 GB)
- Disk space for extraction

### Optional
- Valgrind (memory profiling)
- perf (CPU profiling)
- Instruments (macOS profiling)

---

## Risks & Mitigation

### Risk 1: API Complexity
**Issue**: filesystem_v2 API may be complex
**Mitigation**: Start with simple examples, iterate

### Risk 2: Memory Profiling Accuracy
**Issue**: Measuring memory accurately is hard
**Mitigation**: Use platform tools (valgrind, instruments)

### Risk 3: Fair Comparison
**Issue**: Comparing library vs FUSE vs tool fairly
**Mitigation**: Measure equivalent operations, document differences

---

## Next Steps

### To Start Session 17

1. Read this plan
2. Review libdwarfs API docs
3. Create benchmark framework
4. Implement single file benchmark
5. Iterate through other benchmarks
6. Run automated suite
7. Generate performance report

### Quick Start Command (Future)

```bash
./benchmarks/run_libdwarfs_benchmark.sh
```

---

## Related Documentation

- **Session 16 Results**: [`doc/DWARFS_METADATA_FORMAT_PERFORMANCE.md`](DWARFS_METADATA_FORMAT_PERFORMANCE.md)
- **Architecture**: [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md)
- **Reader API**: [`include/dwarfs/reader/filesystem_v2.h`](../include/dwarfs/reader/filesystem_v2.h)

---

**Status**: 📋 **READY TO IMPLEMENT**
**Next Session**: Session 17 - libdwarfs API Benchmarking
**Prerequisite**: Session 16 Phase 1 Complete ✅