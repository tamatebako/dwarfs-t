# Metadata Format Benchmarking

## Overview

Comprehensive benchmarking suite for comparing Thrift, Cereal, and Bitsery
metadata serialization formats in DwarFS. This suite evaluates performance
across multiple dimensions including compression efficiency, extraction speed,
and FUSE operation latency.

## Quick Start

```bash
# 1. Download benchmark datasets (if not already present)
cmake --build . --target download-benchmark-datasets

# 2. Run benchmarks on Perl dataset
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./build/mkdwarfs \
  --dwarfsextract ./build/dwarfsextract \
  --dwarfs ./build/dwarfs \
  --perl-dataset benchmark-files/perl-5.42.0 \
  --output benchmarks/results/metadata_format_results.json

# 3. Generate report
ruby benchmarks/generate_metadata_report.rb \
  benchmarks/results/metadata_format_results.json \
  METADATA_FORMAT_BENCHMARK_REPORT.md

# 4. View report
cat METADATA_FORMAT_BENCHMARK_REPORT.md
```

## Requirements

### Build Requirements

- DwarFS built with all three metadata formats (Thrift, Cereal, Bitsery)
- CMake configuration with metadata format support enabled

### Runtime Requirements

- **Python**: 3.8 or later
- **Ruby**: 2.7 or later
- **Benchmark datasets**: Downloaded via CMake target or manually placed

### macOS-Specific

```bash
# System utilities (pre-installed)
/usr/bin/time   # For memory tracking

# FUSE implementation
brew install macfuse  # or fuse-t
```

### Linux-Specific

```bash
# Python and Ruby
sudo apt-get install python3 python3-pip ruby

# Time utility (usually pre-installed)
which /usr/bin/time
```

## Running Benchmarks

### Single Dataset

To benchmark only the Perl dataset:

```bash
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./build/mkdwarfs \
  --dwarfsextract ./build/dwarfsextract \
  --dwarfs ./build/dwarfs \
  --perl-dataset benchmark-files/perl-5.42.0 \
  --output benchmarks/results/perl_results.json
```

### Both Datasets

To benchmark both Perl and RaspOS datasets:

```bash
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./build/mkdwarfs \
  --dwarfsextract ./build/dwarfsextract \
  --dwarfs ./build/dwarfs \
  --perl-dataset benchmark-files/perl-5.42.0 \
  --raspios-dataset benchmark-files/raspios_extracted \
  --output benchmarks/results/metadata_format_results.json \
  --work-dir /tmp/metadata_bench
```

### Command-Line Options

**Python Benchmark Script** ([`benchmarks/run_metadata_format_benchmark.py`](../benchmarks/run_metadata_format_benchmark.py))

```
--mkdwarfs PATH          Path to mkdwarfs executable (required)
--dwarfsextract PATH     Path to dwarfsextract executable (required)
--dwarfs PATH            Path to dwarfs FUSE mount executable (required)
--perl-dataset PATH      Path to Perl dataset directory (required)
--raspios-dataset PATH   Path to RaspOS dataset directory (optional)
--output PATH            Output JSON file path (required)
--work-dir PATH          Working directory for images (default: /tmp/metadata_bench)
```

**Ruby Report Generator** ([`benchmarks/generate_metadata_report.rb`](../benchmarks/generate_metadata_report.rb))

```
Usage: generate_metadata_report.rb <results.json> <output.md>

Arguments:
  results.json    Input JSON file from benchmark run
  output.md       Output Markdown report file
```

## Benchmark Metrics

### Compression Performance

- **Time**: Wall-clock time to create the DwarFS archive
- **Size**: Final archive size in bytes
- **Memory**: Peak resident memory usage during compression

### Extraction Performance

#### Extract All Files

- **Time**: Wall-clock time to extract all files
- **Throughput**: MB/s extraction rate
- **Memory**: Peak memory usage during extraction

#### Extract Single File

- **Time**: Wall-clock time to extract a single test file
- **Throughput**: MB/s extraction rate for the test file
- **Memory**: Peak memory usage
- **Test File**: Same file used across all format tests for consistency

### FUSE Performance

#### Mount Time

- **Mount Time**: Time to mount the filesystem
- **Init Time**: FUSE initialization time
- **Total**: Combined mount + init time

#### Read Operations

- **Single File Read**: Time and throughput for reading one file
- **All Files Read**: Time and throughput for reading all files
- **Memory**: Peak memory usage during read operations

#### Operation Latency

Low-level FUSE operation timings (in microseconds):

- `op_init`: Filesystem initialization
- `op_lookup`: Name lookup operations
- `op_getattr`: Get file attributes
- `op_readlink`: Read symbolic link
- `op_open`: Open file
- `op_read`: Read file data
- `op_readdir`: Read directory contents
- `op_statfs`: Get filesystem statistics
- `op_getxattr`: Get extended attributes
- `op_listxattr`: List extended attributes

## Test File Selection

The benchmark uses specific test files for single-file operations:

- **Perl**: [`doio.c`](../benchmark-files/perl-5.42.0/doio.c) (106KB)
  - Representative C source file from Perl distribution
  - Medium-sized for meaningful measurements
  - Consistently available across format tests

- **RaspOS**: `bin/bash` (recommended, ~1MB)
  - Common system binary
  - Larger file for throughput testing
  - Present in all RaspOS images

These files are selected to:
- Represent real-world access patterns
- Provide meaningful performance measurements
- Remain consistent across all format variations

## Interpreting Results

### JSON Output Format

See [`benchmarks/results/metadata_format_results_example.json`](../benchmarks/results/metadata_format_results_example.json) for complete structure.

**Key sections**:

- `metadata`: System information, tool versions, timestamps
- `datasets`: Dataset details (size, file count, test file)
- `results`: Array of format results with all metrics

### Markdown Report Sections

1. **Executive Summary**: Best performers for each metric category
2. **Dataset Results**: Detailed performance breakdown by dataset
   - Compression performance
   - Extraction performance (all files + single file)
   - FUSE performance (mount, read, latency)
3. **Analysis**: Interpretation of results and patterns
4. **Recommendations**: Use case-specific guidance

### Comparison Symbols

The report uses Thrift as the baseline and shows:

- `~`: No significant difference (<0.5%)
- `✅ -X%`: Improvement (lower is better for time/latency)
- `⬆️ +X%`: Improvement for throughput, regression for time/size

**For throughput metrics** (MB/s): Higher percentages indicate better performance

**For time/latency metrics**: Lower values (negative percentages) indicate better performance

**For size metrics**: Smaller is usually better (depends on context)

## Implementation Details

### Memory Tracking

The benchmark uses platform-specific time utilities for accurate memory measurement:

**macOS**:
```bash
/usr/bin/time -l <command>
# Parses "maximum resident set size" in bytes
```

**Linux**:
```bash
/usr/bin/time -v <command>
# Parses "Maximum resident set size (kbytes)"
```

### FUSE Management

Critical sequence for reliable FUSE benchmarking:

1. Start FUSE mount in background process
2. Wait for [`os.path.ismount()`](../benchmarks/run_metadata_format_benchmark.py) confirmation
3. Run benchmark tests
4. Extract perfmon xattr **before** unmounting
5. Unmount cleanly
6. Verify process terminated

### Perfmon Parsing

Latency metrics are extracted from the `user.dwarfs.driver.perfmon` extended attribute:

```bash
xattr -p user.dwarfs.driver.perfmon <mount_point>
```

**Format**:
```
op_name: samples=N, avg=X.Xus, p50=X.Xus, p90=X.Xus, p99=X.Xus
```

The parser extracts:
- Sample count
- Average latency
- Median (p50)
- 90th percentile (p90)
- 99th percentile (p99)

## Performance Notes

### Benchmark Duration

Approximate execution times:

- **Per format/dataset**: 30-60 seconds
- **3 formats × 1 dataset**: 2-5 minutes
- **3 formats × 2 datasets**: 5-10 minutes

Times vary based on:
- Dataset size
- System performance
- Compression settings
- Available CPU cores

### Disk Space Requirements

Working directory space needed:

- **Per format/dataset**: ~2-3× dataset size
- **Perl dataset (100MB)**: ~600MB total working space
- **RaspOS dataset (3.5GB)**: ~21GB total working space

**Cleanup**:
```bash
rm -rf /tmp/metadata_bench
```

### System Load Considerations

For accurate benchmarks:

- Run on idle system
- Avoid other disk-intensive operations
- Disable background services if possible
- Consider running multiple iterations for statistical significance

## Troubleshooting

### FUSE Mount Fails

```bash
# Check FUSE implementation
mount_fuse-t --version  # macOS with FUSE-T
fusermount --version    # Linux

# Ensure mount point is empty
ls -la /tmp/mount_point

# Check for stale mounts
mount | grep dwarfs

# Clean up stale mounts
umount /tmp/mount_point  # or fusermount -u
```

### Memory Metrics Missing

```bash
# Verify time utility exists
which /usr/bin/time

# Test time utility
/usr/bin/time -l echo test  # macOS
/usr/bin/time -v echo test  # Linux

# Check permissions
ls -l /usr/bin/time
```

### Perfmon Data Not Available

```bash
# Verify FUSE mount includes perfmon option
# Must include: -o perfmon=fuse

# Check xattr support on mount point
xattr -l <mount_point>

# Verify perfmon attribute exists
xattr -p user.dwarfs.driver.perfmon <mount_point>
```

### Python Script Errors

```bash
# Check Python version
python3 --version  # Should be 3.8+

# Verify tool paths
./build/mkdwarfs --version
./build/dwarfsextract --version
./build/dwarfs --version

# Check dataset paths
ls -la benchmark-files/perl-5.42.0
```

### Ruby Script Errors

```bash
# Check Ruby version
ruby --version  # Should be 2.7+

# Verify JSON file exists and is valid
cat benchmarks/results/metadata_format_results.json | jq .

# Run with error output
ruby benchmarks/generate_metadata_report.rb input.json output.md 2>&1
```

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Metadata Format Benchmarks

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  benchmark:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v3

      - name: Build DwarFS
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build -j$(nproc)

      - name: Download Datasets
        run: cmake --build build --target download-benchmark-datasets

      - name: Run Benchmarks
        run: |
          python3 benchmarks/run_metadata_format_benchmark.py \
            --mkdwarfs ./build/mkdwarfs \
            --dwarfsextract ./build/dwarfsextract \
            --dwarfs ./build/dwarfs \
            --perl-dataset benchmark-files/perl-5.42.0 \
            --output benchmarks/results/metadata_results.json

      - name: Generate Report
        run: |
          ruby benchmarks/generate_metadata_report.rb \
            benchmarks/results/metadata_results.json \
            METADATA_BENCHMARK_REPORT.md

      - name: Upload Report
        uses: actions/upload-artifact@v3
        with:
          name: benchmark-report
          path: METADATA_BENCHMARK_REPORT.md

      - name: Comment PR
        if: github.event_name == 'pull_request'
        uses: actions/github-script@v6
        with:
          script: |
            const fs = require('fs');
            const report = fs.readFileSync('METADATA_BENCHMARK_REPORT.md', 'utf8');
            github.rest.issues.createComment({
              issue_number: context.issue.number,
              owner: context.repo.owner,
              repo: context.repo.repo,
              body: '## Benchmark Results\n\n' + report
            });
```

## Example Output

See:
- [`benchmarks/results/metadata_format_results_example.json`](../benchmarks/results/metadata_format_results_example.json) - Example JSON output
- [`METADATA_FORMAT_BENCHMARK_REPORT_EXAMPLE.md`](../METADATA_FORMAT_BENCHMARK_REPORT_EXAMPLE.md) - Example generated report

## Contributing

When adding new metrics:

1. **Update Python script** ([`benchmarks/run_metadata_format_benchmark.py`](../benchmarks/run_metadata_format_benchmark.py))
   - Add metric to appropriate dataclass
   - Implement measurement method
   - Update JSON serialization

2. **Update Ruby generator** ([`benchmarks/generate_metadata_report.rb`](../benchmarks/generate_metadata_report.rb))
   - Add metric to result model
   - Create table generation method
   - Update summary/analysis sections

3. **Update example files**
   - Add metric to example JSON
   - Regenerate example report
   - Update documentation

## See Also

- [Metadata Format Overview](dwarfs-format.md#metadata-serialization) - Technical details on format implementation
- [Benchmarking Design](METADATA_FORMAT_BENCHMARK_PLAN.md) - Original design document
- [DwarFS Format Specification](dwarfs-format.md) - Complete format documentation
- [mkdwarfs Manual](mkdwarfs.md) - Compression tool documentation

## Author

Written by Marcus Holland-Moritz.

## Copyright

Copyright (C) Marcus Holland-Moritz.