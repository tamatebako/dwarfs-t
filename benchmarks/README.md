# DwarFS Benchmark Suite

Performance testing tools for DwarFS filesystem operations.

## SYNOPSIS

```bash
# Download test datasets
python3 benchmarks/download_datasets.py --download perl

# Run metadata format benchmarks
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./mkdwarfs \
  --dwarfsextract ./dwarfsextract \
  --dwarfs ./dwarfs \
  --perl-dataset benchmark-files/perl-5.43.3 \
  --output benchmarks/results/metadata_results.json

# Generate report
python3 benchmarks/generate_metadata_report.py \
  benchmarks/results/metadata_results.json \
  METADATA_BENCHMARK_REPORT.md
```

## DESCRIPTION

The DwarFS benchmark suite provides tools for measuring filesystem performance
across metadata serialization formats (FlatBuffers and Thrift). It tests
compression, extraction, and FUSE operations using standardized datasets.

The suite includes:

- **Metadata format benchmarks**: Compare serialization formats across the full
  DwarFS workflow
- **Dataset downloader**: Automated download of test datasets with checksum
  verification
- **Report generator**: Create detailed Markdown reports from benchmark results
- **Shared utilities**: Memory tracking, FUSE management, perfmon parsing,
  progress display

## DATASETS

### Perl 5.43.3 Source Code

Small files workload (6,802 files, ~95 MB extracted). Tests metadata handling
efficiency and file deduplication with many small text files.

**Official Source**: https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz

**SHA-256**: `318651ee5bd94acb6a2d9ab925f3d43fe2192c9c691160d76b65071fad8c9acd`

**Download Size**: ~18 MB

**Extracted Directory**: `benchmark-files/perl-5.43.3`

**Download**:
```bash
python3 benchmarks/download_datasets.py --download perl
```

### Raspberry Pi OS Lite ARM64 Sample

Large single file workload (1 file, ~2.7 GB). Tests large file handling and
binary data compression. The downloader extracts only the image file from the
archive.

**Official Source**: https://downloads.raspberrypi.com/raspios_lite_arm64/images/raspios_lite_arm64-2025-10-02/2025-10-01-raspios-trixie-arm64-lite.img.xz

**SHA-256**: `79146135607ffe8acac94e5ff501de6fc49583117de5ad08c45a32c73ae2a027`

**Download Size**: ~476 MB

**Extracted Directory**: `benchmark-files/raspios_dataset`

**Extracted File**: `raspios_sample.img`

**Download**:
```bash
python3 benchmarks/download_datasets.py --download raspios
```

### Storage Requirements

- Perl: ~18 MB download, ~95 MB extracted
- Raspberry Pi OS: ~476 MB download, ~2.7 GB extracted
- Temporary benchmark files: ~5-15 GB during execution
- **Recommended**: 20 GB free space

## USAGE

### Metadata Format Benchmarks

Compare all metadata formats using a single dataset:

```bash
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./mkdwarfs \
  --dwarfsextract ./dwarfsextract \
  --dwarfs ./dwarfs \
  --perl-dataset benchmark-files/perl-5.43.3 \
  --output benchmarks/results/metadata_results.json
```

Run with both datasets and multiple runs:

```bash
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./mkdwarfs \
  --dwarfsextract ./dwarfsextract \
  --dwarfs ./dwarfs \
  --perl-dataset benchmark-files/perl-5.43.3 \
  --raspios-dataset benchmark-files/raspios_dataset \
  --output benchmarks/results/metadata_results.json \
  --runs 5
```

**Options**:

```
--mkdwarfs PATH          Path to mkdwarfs executable (required)
--dwarfsextract PATH     Path to dwarfsextract executable (required)
--dwarfs PATH            Path to dwarfs FUSE driver (required)
--perl-dataset PATH      Path to Perl dataset directory (required)
--raspios-dataset PATH   Path to RaspOS dataset directory (optional)
--output PATH            Output JSON file path (required)
--work-dir PATH          Working directory (default: /tmp/metadata_bench)
--runs N                 Number of runs per test (default: 3)
```

### Generate Reports

```bash
python3 benchmarks/generate_metadata_report.py \
  benchmarks/results/metadata_results.json \
  METADATA_BENCHMARK_REPORT.md
```

Reports include:

- Executive summary with best performers
- Detailed comparison tables with percentage differences
- FUSE latency percentiles
- Analysis and recommendations

### Download Datasets

List available datasets:

```bash
python3 benchmarks/download_datasets.py --list
```

Download specific dataset:

```bash
python3 benchmarks/download_datasets.py --download perl
```

Download all datasets:

```bash
python3 benchmarks/download_datasets.py --download all
```

**Available Options**:

```
--list              List available datasets with download status
--download DATASET  Download specific dataset (perl, raspios, or all)
--force             Force re-download even if dataset exists
--base-dir PATH     Base directory for datasets (default: benchmark-files)
```

**Dataset Information**:

When listing datasets, you'll see:
- Download status (✓ downloaded, ○ not downloaded)
- Dataset name and description
- Official URL
- Download size
- SHA-256 checksum for verification

## EXTENDING

### Shared Libraries

The benchmark suite provides reusable utilities in `benchmarks/lib/`:

#### Memory Tracker

Track memory usage using `/usr/bin/time`:

```python
from benchmarks.lib import MemoryTracker

tracker = MemoryTracker()
result = tracker.measure_command("mkdwarfs -i input -o output.dwarfs")

print(f"Peak memory: {result['memory_mb']:.1f} MB")
print(f"Wall time: {result['wall_time']:.2f}s")
```

Automatically detects platform (macOS: `time -l`, Linux: `time -v`).

#### FUSE Manager

Manage FUSE mount/unmount lifecycle:

```python
from benchmarks.lib import FUSEManager
import tempfile

manager = FUSEManager("/path/to/dwarfs")

with tempfile.TemporaryDirectory() as mount_point:
    proc = manager.mount("image.dwarfs", mount_point, perfmon=True)

    # Perform tests...

    perfmon = manager.get_perfmon_xattr(mount_point)
    manager.unmount(mount_point, proc)
```

#### Perfmon Parser

Extract FUSE performance metrics:

```python
from benchmarks.lib import PerfmonParser

metrics = PerfmonParser.parse(perfmon_text)
for op, metric in metrics.items():
    print(f"{op}: p99={metric.p99_us}µs")
```

#### Report Generator

Generate Markdown reports from JSON results:

```python
from benchmarks.lib.report_generator import MetadataFormatReport

report = MetadataFormatReport("results.json")
markdown = report.generate()

with open("REPORT.md", "w") as f:
    f.write(markdown)
```

### Custom Benchmarks

Create custom benchmarks using the shared infrastructure:

```python
#!/usr/bin/env python3
from benchmarks.lib import MemoryTracker, FUSEManager, PerfmonParser
import tempfile
import json

def custom_benchmark(image_path, dwarfs_path):
    """Custom benchmark measuring file count operation."""
    tracker = MemoryTracker()
    manager = FUSEManager(dwarfs_path)

    with tempfile.TemporaryDirectory() as mount_point:
        proc = manager.mount(image_path, mount_point, perfmon=True)

        result = tracker.measure_command(
            f"find {mount_point} -type f | wc -l"
        )

        perfmon = manager.get_perfmon_xattr(mount_point)
        latency = PerfmonParser.parse(perfmon)

        manager.unmount(mount_point, proc)

        return {
            'file_count': int(result['stdout'].strip()),
            'memory_mb': result['memory_mb'],
            'time_seconds': result['wall_time'],
            'latency_p99_us': latency.get('op_read', {}).get('p99_us', 0)
        }

if __name__ == '__main__':
    result = custom_benchmark("test.dwarfs", "./dwarfs")
    print(json.dumps(result, indent=2))
```

## TROUBLESHOOTING

### FUSE Mount Fails

Check FUSE installation:

```bash
# macOS (FUSE-T)
mount_fuse-t --version

# Linux
fusermount --version
```

Check for stale mounts:

```bash
mount | grep dwarfs

# Clean up stale mounts
umount /path/to/mount        # macOS
fusermount -u /path/to/mount # Linux
```

### Memory Metrics Missing

Verify `/usr/bin/time` is used (not shell builtin):

```bash
# macOS
/usr/bin/time -l echo test

# Linux
/usr/bin/time -v echo test
```

### Perfmon Data Not Available

Requirements:

1. Mount with `-o perfmon=fuse` option
2. Extended attribute support on mount point
3. Read xattr **before** unmounting

Check xattr support:

```bash
# macOS
xattr -l <mount_point>

# Linux
getfattr -d <mount_point>
```

### Dataset Download Fails

Check connectivity:

```bash
curl -I https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz
curl -I https://downloads.raspberrypi.com/raspios_lite_arm64/images/raspios_lite_arm64-2025-10-02/2025-10-01-raspios-trixie-arm64-lite.img.xz
```

Manual download and verification:

```bash
cd benchmark-files

# Perl dataset
curl -O https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz
tar xzf perl-5.43.3.tar.gz
shasum -a 256 perl-5.43.3.tar.gz
# Expected: 318651ee5bd94acb6a2d9ab925f3d43fe2192c9c691160d76b65071fad8c9acd

# RaspOS dataset
curl -O https://downloads.raspberrypi.com/raspios_lite_arm64/images/raspios_lite_arm64-2025-10-02/2025-10-01-raspios-trixie-arm64-lite.img.xz
mkdir -p raspios_dataset
xz -dc 2025-10-01-raspios-trixie-arm64-lite.img.xz > raspios_dataset/raspios_sample.img
shasum -a 256 2025-10-01-raspios-trixie-arm64-lite.img.xz
# Expected: 79146135607ffe8acac94e5ff501de6fc49583117de5ad08c45a32c73ae2a027
```

## SEE ALSO

[mkdwarfs(1)](../doc/mkdwarfs.md), [dwarfs(1)](../doc/dwarfs.md),
[dwarfsextract(1)](../doc/dwarfsextract.md)