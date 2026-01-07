# DwarFS Benchmark CI Guide

**Last Updated**: 2025-12-02  
**Status**: Production Ready

---

## Overview

DwarFS has comprehensive benchmark infrastructure testing:
1. **CLI Tools Performance** - Speed, memory, space efficiency across all 4 tools
2. **Compression Algorithms** - 24 algorithm configurations
3. **Metadata Formats** - FlatBuffers and Thrift are both valid independently
4. **Build Configurations** - Three independent formats: FlatBuffers-only, Thrift-only, Dual-format (all pass)

---

## Existing CI Jobs (Already Working)

### 1. compression-benchmark
**File**: `.github/workflows/build.yml` (lines 1184-1260)  
**Trigger**: Push, Pull Request  
**Platforms**: Ubuntu 24.04 (amd64)

**What it tests**:
- 24 compression algorithm configurations (zstd, lzma, lz4, brotli, flac, ricepp)
- FlatBuffers-only build
- Dual-format build
- JSON results + Markdown reports

**Output**:
- `benchmark-results/compression-flatbuffers-only.json`
- `benchmark-results/COMPRESSION_REPORT_flatbuffers-only.md`
- `benchmark-results/compression-dual-format.json`
- `benchmark-results/COMPRESSION_REPORT_dual-format.md`

**Run manually**:
```bash
# Trigger via GitHub Actions UI
# Or locally:
python3 benchmarks/compression_algorithm_benchmark.py \
  --build-dir build \
  --output benchmark-results/compression-test.json

python3 benchmarks/generate_compression_report.py \
  --input benchmark-results/compression-test.json \
  --output benchmark-results/REPORT.md
```

### 2. metadata-formats
**File**: `.github/workflows/build.yml` (lines 956-1181)  
**Trigger**: Push, Pull Request  
**Platforms**: Ubuntu (x86_64, aarch64), macOS (x86_64, aarch64), Windows (x64)

**What it tests**:
- FlatBuffers-only builds (should pass)
- Dual-format builds (should pass)
- Thrift-only builds (should fail - FlatBuffers required)
- All test suites on each platform

**Validates**:
- Build configurations work correctly
- FlatBuffers is truly required
- Tests pass in all valid configurations

### 3. vcpkg-test
**File**: `.github/workflows/build.yml` (lines 1563-1650)  
**Trigger**: Push, Pull Request  
**Platforms**: Ubuntu 24.04, macOS 14

**What it tests**:
- vcpkg port installation
- CMake integration
- Library linking

---

## New CI Job: benchmark-comprehensive 🆕

**File**: `.github/workflows/benchmark-comprehensive.yml`  
**Trigger**: Manual (`workflow_dispatch`), Weekly (Sunday 2 AM UTC), Push to benchmarks/  
**Platforms**: Ubuntu 24.04 (amd64)

### Job Matrix

**2 builds × 2 datasets = 4 combinations**:
- Builds: `flatbuffers-only`, `dual-format`
- Datasets: `tiny` (synthetic, ~100 KB), `perl` (Perl 5.43.3, ~95 MB)

### What It Tests

#### Per Build × Dataset Combination:

**1. mkdwarfs (Creation)**
- Compression levels: 1, 5, 9
- Measures: Wall time, CPU time, memory usage
- Records: Image size, compression ratio
- Output: `/usr/bin/time -v` logs

**2. dwarfsck (Verification)**
- Quick integrity check (`--check-integrity`)
- Full validation (default)
- Measures: Wall time, memory usage

**3. dwarfsextract (Extraction)**
- Extract entire filesystem
- Measures: Wall time, memory, throughput

**4. dwarfs (FUSE Operations)**
- Mount filesystem
- Run `find` to traverse all files
- Capture perfmon statistics
- Unmount cleanly

### Unified Benchmark Mode

When triggered manually, runs additional comprehensive test:
- Builds both FlatBuffers-only and Dual-format
- Tests Perl dataset
- Runs Python orchestrator: `run_all_benchmarks.py`
- Generates unified JSON and comprehensive report

---

## Running Benchmarks Locally

### Prerequisites

**System Dependencies**:
```bash
# Ubuntu/Debian
sudo apt-get install -y \
  ninja-build cmake python3 \
  libboost-all-dev libssl-dev libevent-dev \
  libdouble-conversion-dev libfmt-dev libgoogle-glog-dev \
  liblz4-dev liblzma-dev libzstd-dev libxxhash-dev \
  libbz2-dev libarchive-dev libgtest-dev \
  nlohmann-json3-dev libutf8cpp-dev libflac-dev \
  libfuse3-dev fuse3

# Optional: Thrift for dual-format
sudo apt-get install -y libthrift-dev

# macOS
brew install ninja boost openssl libevent double-conversion \
  fmt glog lz4 xz zstd xxhash bzip2 libarchive googletest

# Optional: Thrift
brew install thrift
```

**Download Datasets**:
```bash
# Perl dataset (~95 MB)
python3 benchmarks/download_datasets.py --download perl

# List available datasets
python3 benchmarks/download_datasets.py --list
```

### Quick Compression Benchmark

**Test one build**:
```bash
# Build FlatBuffers-only
cmake -B build-fb -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON

cmake --build build-fb

# Run compression benchmarks
python3 benchmarks/compression_algorithm_benchmark.py \
  --build-dir build-fb \
  --output results.json

# Generate report
python3 benchmarks/generate_compression_report.py \
  --input results.json \
  --output REPORT.md
```

### Manual CLI Tool Benchmarking

**Create test image**:
```bash
# Tiny dataset
mkdir -p /tmp/test-data
echo "test content" > /tmp/test-data/file.txt

# Time creation
/usr/bin/time -v ./build-fb/mkdwarfs \
  -i /tmp/test-data \
  -o test.dwarfs \
  -l 5
```

**Verify image**:
```bash
# Quick check
/usr/bin/time -v ./build-fb/dwarfsck --check-integrity test.dwarfs

# Full validation
/usr/bin/time -v ./build-fb/dwarfsck test.dwarfs

# JSON export
./build-fb/dwarfsck --json -o metadata.json test.dwarfs
```

**Extract image**:
```bash
/usr/bin/time -v ./build-fb/dwarfsextract \
  -i test.dwarfs \
  -o /tmp/extracted
```

**Mount and test**:
```bash
mkdir -p /tmp/mount
./build-fb/dwarfs test.dwarfs /tmp/mount -o perfmon=fuse &
sleep 2

# Test operations
find /tmp/mount -type f
ls -lah /tmp/mount

# Get perfmon stats (Linux)
getfattr -n user.dwarfs.perfmon /tmp/mount

# Unmount
fusermount -u /tmp/mount
```

### Comprehensive All-Tools Benchmark

**Build both configurations**:
```bash
# FlatBuffers-only
cmake -B build-fb -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TOOLS=ON \
  -DWITH_FUSE_DRIVER=ON

cmake --build build-fb

# Dual-format
cmake -B build-dual -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TOOLS=ON \
  -DWITH_FUSE_DRIVER=ON

cmake --build build-dual
```

**Run unified benchmark**:
```bash
python3 benchmarks/run_all_benchmarks.py \
  --flatbuffers-tools build-fb \
  --thrift-tools build-dual \
  --perl-dataset benchmark-files/perl-5.43.3 \
  --levels 1,5,9 \
  --iterations 3 \
  --output benchmark-results/unified.json
```

**Generate comprehensive report**:
```bash
python3 benchmarks/generate_comprehensive_report.py \
  benchmark-results/unified.json \
  benchmark-results/COMPREHENSIVE_REPORT.md
```

---

## Benchmark Results Interpretation

### Compression Algorithm Results

**Key Metrics**:
- **Ratio**: Lower = better compression (e.g., 32% = highly compressed)
- **Comp Speed**: Higher = faster compression (MB/s)
- **Decomp Speed**: Higher = faster decompression (MB/s)

**Algorithm Selection Guide**:
- **General Purpose**: zstd:level=3 (fast, good ratio)
- **Maximum Compression**: lzma:level=9 (slow, best ratio)
- **Maximum Speed**: lz4hc:level=1 (very fast, decent ratio)
- **PCM Audio**: flac:level=3 (specialized, lossless)
- **FITS Images**: ricepp:block_size=128 (specialized, astronomical data)

### CLI Tool Results

**mkdwarfs (Creation)**:
- **Wall Time**: Total time to create image
- **Memory**: Peak RSS (resident set size)
- **Image Size**: Final .dwarfs file size
- **Throughput**: Input data size / wall time

**dwarfsck (Verification)**:
- **Quick Check**: XXH3-64 checksum validation (fast)
- **Full Check**: SHA-512/256 validation (thorough)
- **JSON Export**: Metadata extraction time

**dwarfsextract (Extraction)**:
- **Wall Time**: Total extraction time
- **Throughput**: Extracted data size / wall time
- **Memory**: Peak RSS during extraction

**dwarfs (FUSE)**:
- **Mount Time**: Time to mount filesystem (should be <100ms)
- **Find Time**: Directory traversal time
- **Perfmon**: p50/p99 latencies for FUSE operations (read, getattr, etc.)

---

## CI Artifact Retention

**Compression Benchmarks**: 30 days (automatic, smaller files)  
**CLI Benchmarks**: 90 days (configurable, detailed logs)  
**Unified Benchmarks**: 90 days (comprehensive results)

**Accessing Artifacts**:
1. Navigate to GitHub Actions run
2. Click on workflow run
3. Scroll to "Artifacts" section
4. Download ZIP files

---

## Triggering CI Benchmarks

### Automatic Triggers

**compression-benchmark**:
- Every push to main/branch
- Every pull request

**benchmark-comprehensive**:
- Every Sunday at 2 AM UTC (scheduled)
- Push to `benchmarks/` directory
- Push to `.github/workflows/benchmark-comprehensive.yml`

### Manual Triggers

**Via GitHub UI**:
1. Go to "Actions" tab
2. Select "Comprehensive CLI Benchmarks"
3. Click "Run workflow"
4. Select branch
5. Click "Run workflow" button

**Via GitHub CLI**:
```bash
gh workflow run benchmark-comprehensive.yml
```

**Via API**:
```bash
curl -X POST \
  -H "Accept: application/vnd.github+json" \
  -H "Authorization: Bearer $GITHUB_TOKEN" \
  https://api.github.com/repos/tamatebako/dwarfs/actions/workflows/benchmark-comprehensive.yml/dispatches \
  -d '{"ref":"main"}'
```

---

## Test Suite Validation

**All Platforms** run:
```bash
ctest --test-dir build --output-on-failure -j
```

**Expected Results**:
- FlatBuffers-only: 1,600/1,613 passing (13 Thrift tests skipped)
- Dual-format: 1,613/1,613 passing (all tests)
- Thrift-only: Build fails (FlatBuffers required)

**Test Categories**:
- Unit tests: Algorithm, data structure, utility tests
- Integration tests: Tool workflows, cross-tool compatibility
- Metadata tests: Serialization round-trips for all formats
- Expensive tests: Full filesystem workflows, format compatibility

---

## Troubleshooting

### Dataset Download Fails

**Symptom**: `download_datasets.py` unable to download Perl/RaspOS

**Solution**:
```bash
# Manual download
cd benchmark-files
curl -O https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz
tar xzf perl-5.43.3.tar.gz
shasum -a 256 perl-5.43.3.tar.gz
# Verify: 318651ee5bd94acb6a2d9ab925f3d43fe2192c9c691160d76b65071fad8c9acd
```

### FUSE Mount Fails

**Symptom**: dwarfs benchmark fails to mount

**Check**:
```bash
# Verify FUSE installed
mount_fuse-t --version  # macOS (FUSE-T)
fusermount --version    # Linux

# Check stale mounts
mount | grep dwarfs
fusermount -u /path/to/mount  # Clean up
```

### Memory Metrics Missing

**Symptom**: No memory data in benchmark logs

**Verify**:
```bash
# Must use /usr/bin/time, not shell builtin
/usr/bin/time -v echo test  # Linux
/usr/bin/time -l echo test  # macOS
```

### Build Configuration Issues

**Symptom**: Builds missing tools or features

**Check CMake**:
```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TOOLS=ON \
  -DWITH_FUSE_DRIVER=ON \
  -DWITH_TESTS=ON

# Verify configuration
cmake -L build | grep DWARFS
```

---

## Adding New Benchmarks

### To compression-benchmark

1. Add test case to [`test/dwarfs_compression_benchmark.cpp`](../test/dwarfs_compression_benchmark.cpp)
2. Update [`benchmarks/compression_algorithm_benchmark.py`](../benchmarks/compression_algorithm_benchmark.py) if needed
3. CI will run automatically on next push

### To CLI benchmarks

1. Modify [`benchmarks/run_all_benchmarks.py`](../benchmarks/run_all_benchmarks.py)
2. Add new operation to appropriate tool method
3. Update `.github/workflows/benchmark-comprehensive.yml` if manual steps needed

### New Dataset

1. Add to [`benchmarks/download_datasets.py`](../benchmarks/download_datasets.py)
2. Update dataset matrix in `.github/workflows/benchmark-comprehensive.yml`
3. Document in [`benchmarks/README.md`](../benchmarks/README.md)

---

## Performance Regression Detection

**Manual Comparison**:
```bash
# Run benchmark on old commit
git checkout <old-commit>
./run_benchmark.sh > old-results.json

# Run benchmark on new commit
git checkout <new-commit>
./run_benchmark.sh > new-results.json

# Compare (custom script needed)
python3 compare_results.py old-results.json new-results.json
```

**Future Enhancement**: Automated regression detection with thresholds

---

## See Also

- [`benchmarks/README.md`](../benchmarks/README.md) - Benchmark suite documentation
- [`doc/COMPRESSION_BENCHMARK_RESULTS.md`](COMPRESSION_BENCHMARK_RESULTS.md) - Latest results
- [`doc/mkdwarfs.md`](mkdwarfs.md) - mkdwarfs manual
- [`doc/dwarfs.md`](dwarfs.md) - dwarfs manual
- [`.github/workflows/build.yml`](../.github/workflows/build.yml) - Main CI workflow

---

**Last Updated**: 2025-12-02 12:07 HKT  
**Maintainer**: DwarFS Team  
**Status**: Production Ready