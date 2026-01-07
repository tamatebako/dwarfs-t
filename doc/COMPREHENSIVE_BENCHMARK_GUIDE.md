# DwarFS Comprehensive Benchmark Guide

**Purpose**: Compare FUSE vs libdwarfs API performance across all build configurations and metadata formats

**Estimated Runtime**: 2-3 hours (fully automated)

---

## Quick Start

**Single Command**:
```bash
./benchmarks/run_comprehensive_benchmark.sh
```

That's it! The script handles everything automatically.

---

## What It Does

### Phase 1: Build All Configurations (30-60 min)

Builds 3 separate configurations in parallel build directories:

1. **`build-fb-bench/`**: FlatBuffers-only
   - `-DDWARFS_WITH_FLATBUFFERS=ON`
   - `-DDWARFS_WITH_THRIFT=OFF`
   - Can only read/write `.dff` images

2. **`build-thrift-bench/`**: Thrift-only
   - `-DDWARFS_WITH_FLATBUFFERS=OFF`
   - `-DDWARFS_WITH_THRIFT=ON`
   - Can only readwrite `.dft` images

3. **`build-both-bench/`**: Both formats
   - `-DDWARFS_WITH_FLATBUFFERS=ON`
   - `-DDWARFS_WITH_THRIFT=ON`
   - Can read/write both `.dff` and `.dft`

### Phase 2: Create Images

- Creates `test-images/perl-5.43.3.dff` (FlatBuffers format)
- Creates `test-images/perl-5.43.3.dft` (Thrift format)
- Both from same source (Perl 5.43.3, ~96 MB)

### Phase 3: FUSE Benchmarks (30-60 min)

For each build configuration + compatible image:

1. Mount filesystem via FUSE driver
2. Extract all files using `cp -r`
3. Measure time and throughput
4. Unmount and cleanup
5. Save results to JSON

**Combinations tested**:
- FB-only build + .dff image
- Thrift-only build + .dft image
- Both build + .dff image
- Both build + .dft image

### Phase 4: libdwarfs API Benchmarks (30-60 min)

For each build configuration + compatible image:

1. Single file extraction (3 iterations)
2. Full filesystem extraction (3 iterations)
3. Measure time, throughput, memory
4. Save results to JSON

**Same combinations as Phase 3**

### Phase 5: Generate Report

Analyzes all JSON results and creates comprehensive markdown report with:

- Performance comparison tables
- Build configuration analysis
- Format comparison (FB vs Thrift)
- FUSE vs API comparison
- Conclusions and recommendations

---

## Output

### Results Directory Structure

```
results/comprehensive_YYYYMMDD_HHMMSS/
├── COMPREHENSIVE_REPORT.md          # Main report
├── fuse_fb-only_dff.json            # FUSE results
├── fuse_thrift-only_dft.json
├── fuse_both_dff.json
├── fuse_both_dft.json
├── api_single_fb-only_dff.json      # libdwarfs API results
├── api_full_fb-only_dff.json
├── api_single_thrift-only_dft.json
├── api_full_thrift-only_dft.json
├── api_single_both_dff.json
├── api_full_both_dff.json
├── api_single_both_dft.json
└── api_full_both_dft.json
```

### Sample Report

```markdown
# DwarFS Comprehensive Benchmark Report

## Results Summary

### FUSE Extraction Performance

| Build | Format | Duration (s) | Throughput (MB/s) | Cache | Workers |
|-------|--------|--------------|-------------------|-------|---------|
| fb-only | dff | 4.52 | 21.2 | 512 MiB | 4 |
| thrift-only | dft | 4.48 | 21.4 | 512 MiB | 4 |
| both | dff | 4.55 | 21.0 | 512 MiB | 4 |
| both | dft | 4.50 | 21.3 | 512 MiB | 4 |

### libdwarfs API Extraction Performance

| Build | Format | Operation | Time (s) | Throughput (MB/s) | Memory (MB) |
|-------|--------|-----------|----------|-------------------|-------------|
| fb-only | dff | single | 0.05 | 120.5 | 150 |
| fb-only | dff | full | 4.12 | 23.2 | 520 |
...
```

---

## Requirements

### System Requirements

- **Disk Space**: ~5 GB (3 build directories + images)
- **RAM**: 4+ GB recommended
- **Time**: 2-3 hours unattended
- **Platform**: macOS (FUSE-T), Linux (FUSE2/3), FreeBSD

### Software Requirements

- CMake 3.28+
- Ninja build system
- C++20 compiler (GCC 10+, Clang 12+)
- Python 3.6+ (for report generation)
- FUSE driver (macFUSE/FUSE-T/libfuse)

### Optional (for Thrift builds)

- Apache Thrift compiler
- Facebook Folly library

**Note**: Script will gracefully skip Thrift builds if dependencies unavailable

---

## Advanced Usage

### Custom Parameters

Edit script variables:
```bash
ITERATIONS=5              # More iterations for accuracy
CACHE_SIZE=1024          # Larger cache (MiB)
NUM_WORKERS=8            # More workers
EXTRACT_THREADS=4        # Parallel extraction
```

### Skip Phases

Comment out phases in `main()` function:
```bash
# build_configuration "thrift-only" "$BUILD_THRIFT" OFF ON  # Skip Thrift
```

### Rerun Specific Benchmarks

Builds persist, so you can rerun benchmarks:
```bash
# Rebuild phase comments out
./benchmarks/run_comprehensive_benchmark.sh
```

---

## Troubleshooting

### Build Failures

**Thrift/Folly missing**:
- Expected on some platforms
- Script continues with FB-only and Both builds
- Thrift benchmarks skipped

**Out of disk space**:
- Each build ~1-2 GB
- Clean old builds: `rm -rf build-*-bench`

### FUSE Mount Failures

**Permission denied**:
- macOS: Enable Full Disk Access for Terminal
- Linux: Check `/dev/fuse` permissions

**Mount point busy**:
- Script cleanup failed
- Manual unmount: `umount /tmp/dwarfs_bench_*`

### Benchmark Crashes

**Out of memory**:
- Reduce `CACHE_SIZE`
- Reduce `NUM_WORKERS`

**Filesystem errors**:
- Check image integrity: `dwarfsck --check <image>`

---

## Interpreting Results

### FUSE vs API Performance

**Expected**:
- API should be 5-10% faster (no kernel overhead)
- Similar throughput patterns

**If FUSE is faster**:
- Kernel caching helping
- API cache too small

### Format Comparison

**FlatBuffers (.dff)**:
- Image: ~1-5% larger
- Serialization: Faster
- Portability: Excellent
- **Use when**: Portability matters, Thrift unavailable

**Thrift (.dft)**:
- Image: Smallest
- Serialization: Slower (complex encoding)
- Portability: Limited (Folly/Thrift required)
- **Use when**: Space critical, Thrift available

### Build Configuration Impact

**FlatBuffers-only**:
- Binary: Smallest (~10% smaller)
- Build: Fastest
- Portability: Best
- **Use when**: Production deployments

**Thrift-only**:
- Binary: Medium
- Build: Slowest (Folly compilation)
- Portability: Limited
- **Use when**: Legacy compatibility only

**Both formats**:
- Binary: Largest
- Build: Slowest
- Portability: Flexible
- **Use when**: Development, format migration

---

## Example Session

```bash
$ ./benchmarks/run_comprehensive_benchmark.sh

[INFO] DwarFS Comprehensive Benchmark Suite
[INFO] This will take 2-3 hours to complete
[INFO] Results will be saved to: results/comprehensive_20251219_143052

═══════════════════════════════════════════════════════════════
  Phase 1: Building Configurations (30-60 minutes)
═══════════════════════════════════════════════════════════════

[INFO] Building Configuration: flatbuffers-only
[INFO] Build directory: build-fb-bench
...
[SUCCESS] Build complete: flatbuffers-only

[INFO] Building Configuration: thrift-only
...

═══════════════════════════════════════════════════════════════
  Creating DwarFS Images
═══════════════════════════════════════════════════════════════

[INFO] Creating FlatBuffers image...
[SUCCESS] Created: perl-5.43.3.dff

...

═══════════════════════════════════════════════════════════════
  Benchmark Complete!
═══════════════════════════════════════════════════════════════

[SUCCESS] All benchmarks completed successfully

[INFO] Results directory: results/comprehensive_20251219_143052
[INFO] Comprehensive report: results/comprehensive_20251219_143052/COMPREHENSIVE_REPORT.md

$ cat results/comprehensive_20251219_143052/COMPREHENSIVE_REPORT.md
...
```

---

## Related Documentation

- **libdwarfs API Guide**: [`LIBDWARFS_INTEGRATION_GUIDE.md`](LIBDWARFS_INTEGRATION_GUIDE.md)
- **Session 17 Plan**: [`SESSION_17_LIBDWARFS_BENCHMARKING_PLAN.md`](SESSION_17_LIBDWARFS_BENCHMARKING_PLAN.md)
- **FlatBuffers Performance**: [`FLATBUFFERS_PERFORMANCE_REPORT.md`](FLATBUFFERS_PERFORMANCE_REPORT.md)

---

**Last Updated**: 2025-12-19  
**Script**: [`benchmarks/run_comprehensive_benchmark.sh`](../benchmarks/run_comprehensive_benchmark.sh)  
**Status**: Production Ready