# FlatBuffers Benchmarking Guide

**Complete guide to running FlatBuffers vs Thrift benchmarks locally**

This guide walks you through the entire process from compilation to generating benchmark reports.

---

## Quick Start (One Command)

**Prerequisites**: CMake, Ninja, Python 3, curl, tar, bc

**Run everything automatically**:
```bash
./benchmarks/run_flatbuffers_benchmark.sh
```

This single command will:
1. ✅ Build both-formats configuration (mkdwarfs + dwarfsextract)
2. ✅ Download Perl 5.43.3 dataset (~27 MB compressed, ~97 MB extracted)
3. ✅ Run compression benchmarks at levels 1, 3, 9
4. ✅ Run extraction benchmarks
5. ✅ Verify extracted files are identical (cryptographic proof)
6. ✅ Generate comprehensive report

**Estimated Time**: ~5-10 minutes (depending on build speed)

---

## Detailed Walkthrough

### Step 1: Clone Repository

```bash
git clone --recurse-submodules https://github.com/tamatebako/dwarfs
cd dwarfs
```

### Step 2: Install Dependencies

**macOS** (via Homebrew):
```bash
brew install cmake ninja boost openssl libevent \
  double-conversion fmt lz4 xz zstd xxhash \
  bzip2 libarchive googletest
```

**Ubuntu/Debian**:
```bash
sudo apt-get install -y ninja-build cmake \
  libboost-all-dev libssl-dev libevent-dev \
  libdouble-conversion-dev libfmt-dev lz4-dev \
  libzstd-dev libxxhash-dev libbz2-dev \
  libarchive-dev libgtest-dev liblzma-dev
```

**Fedora**:
```bash
sudo dnf install ninja-build cmake boost-devel \
  openssl-devel libevent-devel double-conversion-devel \
  fmt-devel lz4-devel libzstd-devel xxhash-devel \
  bzip2-devel libarchive-devel gtest-devel xz-devel
```

### Step 3: Run Benchmark Script

**Full automatic run** (build + download + benchmark):
```bash
./benchmarks/run_flatbuffers_benchmark.sh
```

**Skip build** (if you already have build-both-bench):
```bash
./benchmarks/run_flatbuffers_benchmark.sh --skip-build
```

**Skip download** (if you already have the dataset):
```bash
./benchmarks/run_flatbuffers_benchmark.sh --skip-download
```

**Both skips** (just run benchmarks):
```bash
./benchmarks/run_flatbuffers_benchmark.sh --skip-build --skip-download
```

### Step 4: Review Results

The script outputs:
1. **Console summary** - Real-time progress and final summary
2. **Detailed report** - Saved to `doc/FLATBUFFERS_BENCHMARK_RESULTS_<timestamp>.md`
3. **Benchmark files** - Temporary directory (shown in output, auto-cleaned)

**Example output**:
```
================================================================
  Benchmark Complete!
================================================================

Summary:
========

Compression (Level 3):
  FlatBuffers: 2.999s
  Thrift:      3.617s
  Speedup:     17.1%

Extraction (Level 3):
  FlatBuffers: 2.069s
  Thrift:      1.998s

Verification:
  Status:      ✅ IDENTICAL
  Tree Hash:   3809023dceb2c737f826350c7ca12b87b08f51cef2ec3893f56446f190b00366

Report saved to: doc/FLATBUFFERS_BENCHMARK_RESULTS_20251219_103045.md
```

---

## Manual Step-by-Step (Without Script)

If you prefer to run each step manually:

### 1. Build Both-Formats Configuration

```bash
# Configure
cmake -B build-both-bench -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=ON

# Build
ninja -C build-both-bench mkdwarfs dwarfsextract
```

### 2. Download Dataset

```bash
# Download Perl 5.43.3
mkdir -p benchmark-files
cd benchmark-files
curl -L -o perl-5.43.3.tar.gz \
  https://cpan.metacpan.org/authors/id/P/PE/PEVANS/perl-5.43.3.tar.gz

# Extract
tar xzf perl-5.43.3.tar.gz
rm perl-5.43.3.tar.gz
cd ..
```

### 3. Run Compression Benchmarks

**Level 1 (Fast)**:
```bash
# FlatBuffers
time build-both-bench/mkdwarfs \
  -i benchmark-files/perl-5.43.3/perl-5.43.3 \
  -o /tmp/test_fb_l1.dff \
  --format=flatbuffers -l 1

# Thrift
time build-both-bench/mkdwarfs \
  -i benchmark-files/perl-5.43.3/perl-5.43.3 \
  -o /tmp/test_th_l1.dft \
  --format=thrift -l 1
```

**Level 3 (Default)**:
```bash
# FlatBuffers
time build-both-bench/mkdwarfs \
  -i benchmark-files/perl-5.43.3/perl-5.43.3 \
  -o /tmp/test_fb_l3.dff \
  --format=flatbuffers -l 3

# Thrift
time build-both-bench/mkdwarfs \
  -i benchmark-files/perl-5.43.3/perl-5.43.3 \
  -o /tmp/test_th_l3.dft \
  --format=thrift -l 3
```

**Level 9 (Maximum Compression)**:
```bash
# FlatBuffers
time build-both-bench/mkdwarfs \
  -i benchmark-files/perl-5.43.3/perl-5.43.3 \
  -o /tmp/test_fb_l9.dff \
  --format=flatbuffers -l 9

# Thrift
time build-both-bench/mkdwarfs \
  -i benchmark-files/perl-5.43.3/perl-5.43.3 \
  -o /tmp/test_th_l9.dft \
  --format=thrift -l 9
```

### 4. Run Extraction Benchmarks

```bash
# FlatBuffers
time build-both-bench/dwarfsextract \
  -i /tmp/test_fb_l3.dff \
  -o /tmp/extract_fb

# Thrift
time build-both-bench/dwarfsextract \
  -i /tmp/test_th_l3.dft \
  -o /tmp/extract_th
```

### 5. Verify Extraction Identity

```bash
python3 tools/dirtree_hash.py --compare /tmp/extract_fb /tmp/extract_th
```

**Expected output**:
```
✅ IDENTICAL - Tree hashes match!
Both directories have:
  - Same file structure
  - Same file contents
  - Same file sizes
```

### 6. Check Image Sizes

```bash
ls -lh /tmp/test_*
```

**Expected output** (approximate):
```
-rw-r--r--  36M test_fb_l1.dff
-rw-r--r--  36M test_th_l1.dft
-rw-r--r--  27M test_fb_l3.dff
-rw-r--r--  27M test_th_l3.dft
-rw-r--r--  14M test_fb_l9.dff
-rw-r--r--  14M test_th_l9.dft
```

---

## Understanding the Results

### Compression Time

**What it measures**: Time to create a DwarFS image
**Why it matters**: Faster creation = better for batch operations, CI/CD
**Expected**: FlatBuffers is 17-29% faster at levels 1-3

### Extraction Time

**What it measures**: Time to extract all files from image
**Why it matters**: Indicates read performance
**Expected**: Both formats are virtually identical (±3%)

### Image Size

**What it measures**: Final DwarFS image file size
**Why it matters**: Smaller images = less storage, faster transfers
**Expected**: FlatBuffers is 0.07-1.41% larger (negligible)

### Verification

**What it measures**: Cryptographic proof files are identical
**Why it matters**: Confirms both formats produce same output
**Expected**: Tree hashes match (identical files)

---

## Troubleshooting

### Build Fails with "Thrift not found"

**Problem**: Thrift/Folly dependencies missing or incompatible

**Solution**: Build with FlatBuffers-only (still valid benchmark):
```bash
cmake -B build-fb-bench -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=ON

ninja -C build-fb-bench
```

Then manually test Thrift with a pre-built binary or skip Thrift benchmarks.

### Dataset Download Fails

**Problem**: Network issues or URL changed

**Solution**: Download manually:
```bash
# Visit https://metacpan.org/release/PEVANS/perl-5.43.3
# Download perl-5.43.3.tar.gz
# Extract to benchmark-files/perl-5.43.3/
```

### Script Hangs on Compression

**Problem**: Level 9 compression is very slow (20-30 seconds)

**Solution**: This is normal. Wait for completion or use Ctrl+C and test only levels 1 and 3.

### "Command not found: bc"

**Problem**: Missing `bc` calculator utility

**Solution**: Install bc:
```bash
# macOS
brew install bc

# Ubuntu/Debian
sudo apt-get install bc

# Fedora
sudo dnf install bc
```

---

## Customization

### Test Different Datasets

Edit the script's `DATASET` variable:
```bash
DATASET="path/to/your/dataset"
```

Or create your own benchmark:
```bash
# Use any directory as dataset
YOUR_DATA="/path/to/test/data"

# Benchmark compression
time build-both-bench/mkdwarfs -i "$YOUR_DATA" -o test_fb.dff --format=flatbuffers -l 3
time build-both-bench/mkdwarfs -i "$YOUR_DATA" -o test_th.dft --format=thrift -l 3

# Compare sizes
ls -lh test_*.d{ff,ft}
```

### Test Additional Compression Levels

Modify the script's loop:
```bash
for level in 1 2 3 4 5 6 7 8 9; do
  # ... benchmark code ...
done
```

### Save Benchmark Images

The script auto-cleans `/tmp/dwarfs_flatbuffers_benchmark_*`. To preserve:

1. Comment out the cleanup trap in the script:
   ```bash
   # trap cleanup EXIT  # Comment this out
   ```

2. Or manually save before script exits:
   ```bash
   cp -r /tmp/dwarfs_flatbuffers_benchmark_* ~/saved_benchmarks/
   ```

---

## Performance Baselines

### Expected Results (Perl 5.43.3 on Modern Hardware)

**Compression** (typical desktop: 8 cores, SSD):
- Level 1: ~1.5-2.5 seconds (both formats)
- Level 3: ~3-4 seconds (both formats)
- Level 9: ~25-30 seconds (both formats)

**Extraction**:
- ~2 seconds (both formats)

**Sizes** (Perl 5.43.3):
- Level 1: ~35-37 MB
- Level 3: ~26-28 MB
- Level 9: ~13-14 MB

### Interpreting Differences

**<5% difference**: Within measurement noise (insignificant)
**5-15% difference**: Noticeable but acceptable
**>15% difference**: Significant advantage for one format

---

## CI/CD Integration

### GitHub Actions Example

```yaml
name: FlatBuffers Benchmark

on:
  push:
    paths:
      - 'src/metadata/serialization/**'
      - 'flatbuffers/**'

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y ninja-build cmake \
            libboost-all-dev libssl-dev libevent-dev \
            libfmt-dev lz4-dev libzstd-dev libxxhash-dev \
            libarchive-dev

      - name: Run benchmark
        run: ./benchmarks/run_flatbuffers_benchmark.sh

      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-results
          path: doc/FLATBUFFERS_BENCHMARK_RESULTS_*.md
```

---

## Related Documentation

- **Full Performance Report**: [`doc/DWARFS_METADATA_FORMAT_PERFORMANCE.md`](DWARFS_METADATA_FORMAT_PERFORMANCE.md)
- **Directory Tree Hash Utility**: [`doc/DIRTREE_HASH_UTILITY.md`](DIRTREE_HASH_UTILITY.md)
- **Architecture**: [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md)

---

## FAQ

**Q: Why Perl 5.43.3 as the dataset?**
A: It's a real-world dataset with diverse file types (C, Perl, documentation), good size (~97 MB), and publicly available.

**Q: Can I use my own dataset?**
A: Yes! Edit `DATASET` variable in the script or run manual benchmarks with any directory.

**Q: Why do compression times vary between runs?**
A: CPU load, disk caching, background processes. Run multiple iterations and average for accuracy.

**Q: Is FlatBuffers always faster?**
A: At typical compression levels (1-3), yes. At maximum compression (9), they're equivalent.

**Q: Why is the script so verbose?**
A: To provide clear progress feedback and help debug issues. You can redirect output if needed.

**Q: Can I benchmark on Windows?**
A: Yes, but you'll need WSL or adapt the script to PowerShell. The manual steps work on Windows with appropriate path adjustments.

---

**Created**: 2025-12-19
**Script Location**: [`benchmarks/run_flatbuffers_benchmark.sh`](../benchmarks/run_flatbuffers_benchmark.sh)
**Contact**: DwarFS Team