# DwarFS Benchmarking & Library Refactoring Plan

**Date**: 2025-11-26
**Goal**: Enable FlatBuffers vs Thrift benchmarking and complete library modularization
**Status**: Planning Phase

---

## Executive Summary

**Primary Goals**:
1. **Benchmarking Framework**: Compare FlatBuffers-only vs Thrift-only implementations
2. **Library Refactoring**: Reduce all files to <800 lines through better architecture
3. **CLI Simplification**: Make all CLI tools thin wrappers around libdwarfs

**Current State**:
- ✅ CLI tools already refactored (all <700 lines)
- ⚠️ Library files need modularization (15+ files >800 lines)
- ❌ No benchmarking framework for format comparison

---

## Part 1: Benchmarking Framework (Priority 1)

### Goal

Create comprehensive benchmarking framework to compare:
- **FlatBuffers-only** build vs **Thrift-only** build
- **Extraction performance**: full filesystem vs single file
- **Mount performance**: metadata loading, file access patterns
- **Memory usage**: runtime memory consumption
- **Datasets**: Small (MB), Medium (GB), Large (10+ GB)

### Prerequisites

Current tools to leverage:
- ✅ `dwarfsextract` (280 lines) - already thin
- ✅ `dwarfs` FUSE driver (368 lines) - already refactored
- ✅ `mkdwarfs` (689 lines) - can create test images
- ✅ Existing `benchmarks/` directory structure

### Phase 1.1: Extend dwarfsextract for Benchmarking (2-3 hours)

**Files to Modify**:
1. `tools/src/dwarfsextract_main.cpp` (280 lines)
2. `include/dwarfs/utility/filesystem_extractor.h`
3. `src/utility/filesystem_extractor.cpp` (675 lines)

**Changes Needed**:

**A. Add Performance Metrics APIs to filesystem_extractor**:
```cpp
// Add to include/dwarfs/utility/filesystem_extractor.h
struct extraction_metrics {
  std::chrono::microseconds metadata_load_time;
  std::chrono::microseconds extraction_time;
  size_t bytes_extracted;
  size_t files_extracted;
  size_t blocks_decompressed;
  size_t cache_hits;
  size_t cache_misses;
};

class filesystem_extractor {
public:
  // Add metrics collection
  extraction_metrics const& get_metrics() const;
  void enable_metrics(bool enable = true);
};
```

**B. Add Benchmark Mode to dwarfsextract**:
```bash
# New options
dwarfsextract --benchmark-mode \
  --format=flatbuffers|thrift \
  --repeat=10 \
  --output-json=results.json \
  -i image.dwarfs
```

**New command-line flags**:
- `--benchmark-mode`: Enable detailed timing and metrics
- `--format=FORMAT`: Force specific metadata format
- `--repeat=N`: Run extraction N times for averaging
- `--output-json=FILE`: Export metrics as JSON
- `--single-file=PATH`: Extract only one file for latency testing

### Phase 1.2: Create Benchmark Runner Script (2 hours)

**File**: `benchmarks/run_format_comparison.py`

**Functionality**:
```python
# Compare FlatBuffers vs Thrift across datasets
python benchmarks/run_format_comparison.py \
  --dataset=perl,ci-builds,audio \
  --operations=extract-all,extract-single,mount-read \
  --output=comparison-report.html
```

**What it does**:
1. Creates test images with both formats (`mkdwarfs --format=flatbuffers/thrift`)
2. Runs `dwarfsextract --benchmark-mode` on both formats
3. Tests single-file extraction latency
4. Tests FUSE mount + random file access
5. Collects memory usage via `/usr/bin/time` or similar
6. Generates comparison report with charts

**Metrics to Collect**:
- **Metadata Loading**: Time to parse and validate filesystem metadata
- **Block Decompression**: Time spent decompressing blocks
- **Full Extraction**: Total time to extract all files
- **Single File Latency**: Time to extract one specific file
- **Memory Peak**: Maximum RSS during operation
- **Cache Efficiency**: Hit ratio for block cache

### Phase 1.3: Dataset Preparation (1 hour)

**Use Existing Datasets**:
1. **Small**: `testdata/` (already in repo, ~10MB)
2. **Medium**: Perl installations (via existing benchmarks, ~1GB extracted)
3. **Large**: CI builds (via existing benchmarks, ~10GB extracted)

**Create Both Format Versions**:
```bash
# Script: benchmarks/prepare_format_images.sh
for dataset in small medium large; do
  mkdwarfs -i $dataset -o ${dataset}-fb.dwarfs --format=flatbuffers -l7
  mkdwarfs -i $dataset -o ${dataset}-th.dwarfs --format=thrift -l7
done
```

### Phase 1.4: Expected Results Format (reference)

```json
{
  "dataset": "perl-installations",
  "dataset_size_gb": 47.49,
  "results": {
    "flatbuffers": {
      "image_size_mb": 430.9,
      "metadata_load_ms": 8.2,
      "extract_all_sec": 180.5,
      "extract_single_ms": 2.1,
      "memory_peak_mb": 256,
      "cache_hit_ratio": 0.85
    },
    "thrift": {
      "image_size_mb": 410.3,
      "metadata_load_ms": 12.5,
      "extract_all_sec": 182.1,
      "extract_single_ms": 2.3,
      "memory_peak_mb": 248,
      "cache_hit_ratio": 0.84
    }
  }
}
```

---

## Part 2: Library File Refactoring (Priority 2)

### Problem

15+ library files exceed 800 lines, violating maintainability guidelines.

### Files Requiring Refactoring (>800 lines)

| File | Lines | Category | Refactoring Strategy |
|------|-------|----------|---------------------|
| unicode_case_folding.cpp | 3036 | Data | ✅ Skip (generated data tables) |
| metadata_v2_thrift_upstream.cpp | 2590 | Legacy | Split into logical units |
| metadata_v2_flatbuffers.cpp | 2386 | Reader | Split by metadata type |
| metadata_v2_thrift.cpp | 2365 | Reader | Split by metadata type |
| segmenter.cpp | 1998 | Writer | Extract strategies |
| filesystem_v2.cpp | 1682 | Reader | Split into components |
| metadata_builder.cpp | 1308 | Writer | Split by build phase |
| thrift_metadata_builder.cpp | 1285 | Writer | Split by metadata type |
| filesystem_writer.cpp | 1245 | Writer | Extract phases |
| flatbuffers_metadata_builder.cpp | 1242 | Writer | Split by metadata type |
| pcmaudio_categorizer.cpp | 1228 | Writer | Extract format parsers |
| fuse_driver.cpp | 1227 | Reader | ✅ Already extracted (in v0.16.0) |
| metadata_types_thrift.cpp | 1226 | Reader | Split view implementations |
| scanner.cpp | 1015 | Writer | Extract scan strategies |
| inode_manager.cpp | 912 | Writer | Split responsibilities |
| folly_compat.h | 909 | Util | ✅ Skip (compatibility shims) |
| block_cache.cpp | 835 | Reader | Extract cache strategies |
| cpp_thrift_converter.cpp | 803 | Metadata | Split by type family |

### Refactoring Priorities

**Phase 2.1: Reader Library (4-6 hours)**

**Target**: `src/reader/filesystem_v2.cpp` (1682 lines)

**Current Monolithic Structure**:
- File operations (open, read, seek, stat)
- Directory traversal
- Inode management
- Block cache integration
- Metadata access

**Proposed Split**:
```
src/reader/filesystem_v2.cpp (1682 lines)
  ↓ SPLIT INTO ↓
src/reader/filesystem_v2.cpp (600 lines) - Core orchestration
src/reader/internal/file_operations.cpp (400 lines) - File I/O
src/reader/internal/directory_walker.cpp (350 lines) - Dir traversal
src/reader/internal/inode_accessor.cpp (350 lines) - Inode ops
```

**Strategy**:
1. Extract file operation logic → `file_operations.{h,cpp}`
2. Extract directory traversal → `directory_walker.{h,cpp}`
3. Extract inode access → `inode_accessor.{h,cpp}`
4. Keep core coordination in `filesystem_v2.cpp`

**Phase 2.2: Writer Library (6-8 hours)**

**Target A**: `src/writer/segmenter.cpp` (1998 lines)

**Current Structure**:
- Rolling hash computation
- Bloom filter management
- Segment matching algorithms
- Multi-threading coordination

**Proposed Split**:
```
src/writer/segmenter.cpp (1998 lines)
  ↓ SPLIT INTO ↓
src/writer/segmenter.cpp (500 lines) - Main orchestration
src/writer/internal/rolling_hash.cpp (400 lines) - Hash algorithms
src/writer/internal/bloom_filter_manager.cpp (350 lines) - Bloom ops
src/writer/internal/segment_matcher.cpp (450 lines) - Matching logic
src/writer/internal/segmenter_worker.cpp (300 lines) - Thread pool
```

**Target B**: `src/writer/filesystem_writer.cpp` (1245 lines)

**Proposed Split**:
```
src/writer/filesystem_writer.cpp (1245 lines)
  ↓ SPLIT INTO ↓
src/writer/filesystem_writer.cpp (550 lines) - Core writer
src/writer/internal/block_builder.cpp (350 lines) - Block assembly
src/writer/internal/compression_scheduler.cpp (350 lines) - Compression
```

**Target C**: `src/writer/scanner.cpp` (1015 lines)

**Proposed Split**:
```
src/writer/scanner.cpp (1015 lines)
  ↓ SPLIT INTO ↓
src/writer/scanner.cpp (550 lines) - Main scanner
src/writer/internal/scan_strategy.cpp (250 lines) - Strategy interface
src/writer/internal/filesystem_scanner.cpp (220 lines) - FS scan impl
```

**Phase 2.3: Metadata Library (6-8 hours)**

**Target A**: `src/reader/internal/metadata_v2_flatbuffers.cpp` (2386 lines)
**Target B**: `src/reader/internal/metadata_v2_thrift.cpp` (2365 lines)

**Strategy**: These are format-specific implementations. Split by metadata component:

```
metadata_v2_flatbuffers.cpp (2386 lines)
  ↓ SPLIT INTO ↓
metadata_v2_flatbuffers.cpp (600 lines) - Main interface
internal/fb_inode_reader.cpp (500 lines) - Inode handling
internal/fb_directory_reader.cpp (450 lines) - Directory handling
internal/fb_chunk_reader.cpp (450 lines) - Chunk handling
internal/fb_metadata_validator.cpp (400 lines) - Validation
```

Similar split for Thrift variant.

**Target B**: Metadata Builders (all >800 lines)

```
metadata_builder.cpp (1308 lines)
  ↓ SPLIT INTO ↓
metadata_builder.cpp (650 lines) - Core builder
internal/inode_metadata_builder.cpp (350 lines) - Inode building
internal/directory_metadata_builder.cpp (310 lines) - Dir building
```

### Refactoring Principles

1. **Single Responsibility**: Each file handles one aspect
2. **Strategy Pattern**: Use interfaces for pluggable algorithms
3. **Clear Boundaries**: Well-defined APIs between components
4. **No Circular Dependencies**: Strict layering
5. **Test Coverage**: Maintain or improve coverage during refactoring

---

## Part 3: CLI Tool Library Extraction (Priority 3)

### Current CLI Status

All CLI tools already under 800 lines:
- ✅ `mkdwarfs_main.cpp`: 689 lines (refactored in v0.16.0)
- ✅ `dwarfs_main.cpp`: 368 lines (refactored in v0.16.0)
- ✅ `dwarfsck_main.cpp`: 391 lines
- ✅ `dwarfsextract_main.cpp`: 280 lines

### Potential Further Refactoring

**dwarfsck_main.cpp** (391 lines) - Already acceptable but could extract:
- Filesystem checking logic → `libdwarfs/utility/filesystem_checker.{h,cpp}`
- JSON export logic → `libdwarfs/utility/metadata_exporter.{h,cpp}`

**Target**: Reduce to ~200 lines with library extraction

**Files to Create**:
1. `include/dwarfs/utility/filesystem_checker.h`
2. `src/utility/filesystem_checker.cpp`
3. `tools/src/dwarfsck/options_parser.cpp`
4. `tools/src/dwarfsck/check_handler.cpp`

---

## Part 4: Implementation Timeline

### Phase 1: Benchmarking (Priority 1, Week 1)

| Task | Time | Output |
|------|------|--------|
| 1.1: Extend dwarfsextract metrics | 3h | New APIs in filesystem_extractor |
| 1.2: Create benchmark runner | 2h | `run_format_comparison.py` |
| 1.3: Prepare datasets | 1h | Test images for both formats |
| 1.4: Run initial benchmarks | 2h | Baseline comparison results |
| 1.5: Document findings | 1h | Benchmark report |
| **Total** | **9h** | **Working benchmark framework** |

### Phase 2: Reader Library Refactoring (Week 2)

| Task | Time | Output |
|------|------|--------|
| 2.1: Extract file_operations | 2h | <800 lines per file |
| 2.2: Extract directory_walker | 2h | <800 lines per file |
| 2.3: Extract inode_accessor | 2h | <800 lines per file |
| 2.4: Test and validate | 2h | All tests passing |
| **Total** | **8h** | **Modular reader library** |

### Phase 3: Writer Library Refactoring (Week 3)

| Task | Time | Output |
|------|------|--------|
| 3.1: Refactor segmenter | 4h | 5 files <800 lines each |
| 3.2: Refactor filesystem_writer | 3h | 3 files <800 lines each |
| 3.3: Refactor scanner | 2h | 3 files <800 lines each |
| 3.4: Test and validate | 3h | All tests passing |
| **Total** | **12h** | **Modular writer library** |

### Phase 4: Metadata Library Refactoring (Week 4)

| Task | Time | Output |
|------|------|--------|
| 4.1: Split FlatBuffers metadata | 4h | 5 files <800 lines each |
| 4.2: Split Thrift metadata | 4h | 5 files <800 lines each |
| 4.3: Split metadata builders | 4h | Multiple files <800 lines |
| 4.4: Test and validate | 4h | All tests passing |
| **Total** | **16h** | **Modular metadata library** |

### Phase 5: dwarfsck Refactoring (Optional, Week 5)

| Task | Time | Output |
|------|------|--------|
| 5.1: Extract filesystem_checker | 2h | New library component |
| 5.2: Extract options_parser | 1h | Consistent with other tools |
| 5.3: Create check_handler | 1h | Handler pattern |
| 5.4: Update dwarfsck_main | 1h | Thin CLI wrapper |
| **Total** | **5h** | **Refactored dwarfsck** |

**Grand Total**: 50 hours (5-6 weeks part-time)

---

## Part 5: Success Criteria

### Benchmarking Success

- ✅ Automated benchmark runs on 3+ datasets
- ✅ Clear performance comparison: FlatBuffers vs Thrift
- ✅ Metrics exported to JSON/HTML report
- ✅ Single-file extraction latency measured
- ✅ Memory usage profiled
- ✅ Cache efficiency analyzed

### Refactoring Success

- ✅ All source files <800 lines
- ✅ No circular dependencies introduced
- ✅ All existing tests pass
- ✅ Code coverage maintained or improved
- ✅ Clear module boundaries with documented APIs
- ✅ Build times maintained or improved

### Library Quality

- ✅ Each module has single, clear responsibility
- ✅ Interfaces well-documented with examples
- ✅ Can build tools independently
- ✅ Easy to add new functionality without touching core

---

## Part 6: Testing Strategy

### Unit Tests

For each extracted module:
- Test individual functions
- Mock dependencies
- Test error conditions
- Verify performance characteristics

### Integration Tests

- Full extraction workflows
- Single-file extraction
- Filesystem creation with various options
- Format conversion (Thrift ↔ FlatBuffers)

### Benchmark Tests

- Regression testing for performance
- Memory usage validation
- Cache efficiency verification

---

## Part 7: Documentation Updates

### Files to Update

1. **`README.md`**: Add benchmarking section
2. **`doc/dwarfsextract.md`**: Document new benchmark options
3. **`.kilocode/rules/memory-bank/architecture.md`**: Update module structure
4. **`.kilocode/rules/memory-bank/tech.md`**: Document new library organization

### New Documentation

1. **`doc/BENCHMARKING.md`**: Comprehensive benchmarking guide
2. **`doc/LIBRARY_ARCHITECTURE.md`**: Module dependency diagram
3. **`doc/REFACTORING_LOG.md`**: Track all refactoring decisions

---

## Part 8: Risk Assessment

### Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| Breaking existing functionality | High | Comprehensive test suite, incremental changes |
| Performance regression | Medium | Benchmark each change, profile critical paths |
| Increased build time | Low | Monitor compile times, use forward declarations |
| Complex merge conflicts | Medium | Work in feature branches, frequent rebases |
| API instability | Medium | Version interfaces, maintain backward compatibility |

---

## Part 9: Quick Start Guide

### For Benchmarking (Next Session)

```bash
# 1. Checkout refactor branch
cd /Users/mulgogi/src/external/dwarfs
git checkout refactor/dwarfs-mkdwarfs-complete

# 2. Build with both formats
cmake -B build -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
ninja -C build

# 3. Create test images
./build/mkdwarfs -i testdata -o test-fb.dwarfs --format=flatbuffers -l7
./build/mkdwarfs -i testdata -o test-th.dwarfs --format=thrift -l7

# 4. Add benchmark mode to dwarfsextract
# (Follow Phase 1.1 instructions)

# 5. Run initial benchmarks
python benchmarks/run_format_comparison.py
```

### For Library Refactoring (Later)

```bash
# 1. Start with filesystem_v2.cpp
# 2. Create new files in src/reader/internal/
# 3. Extract one component at a time
# 4. Build and test after each extraction
# 5. Commit incrementally
```

---

## Part 10: Continuation Checklist

Before starting next session:

- [ ] Read this document completely
- [ ] Verify current `refactor/dwarfs-mkdwarfs-complete` branch status
- [ ] Check CI/CD results for current branch
- [ ] Review memory bank context
- [ ] Choose: Benchmarking (Priority 1) or Refactoring (Priority 2)

**Recommended Start**: Phase 1.1 (Benchmarking Framework)
**Estimated First Session**: 3-4 hours
**Expected Output**: Working benchmark for format comparison

---

**Created**: 2025-11-26 23:22 HKT
**Next Update**: After Phase 1 completion
**Status**: Ready for implementation