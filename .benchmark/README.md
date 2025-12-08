# DwarFS General Benchmark Framework

Comprehensive binary and filesystem benchmarking framework for DwarFS (upstream).

## Overview

This is the upstream benchmark framework that provides broad performance testing capabilities across different DwarFS builds and configurations. It uses a decorator-based registration system for easy benchmark definition and hyperfine for precise timing measurements.

## Quick Start

```bash
# List available benchmarks
.benchmark/benchmark.py --list

# Run all benchmarks for binaries in a directory
.benchmark/benchmark.py \
  --input-dir ./release-binaries \
  --data-dir ./.benchmark/data \
  --output-dir ./results

# Run specific benchmarks only
.benchmark/benchmark.py \
  --input-dir ./release-binaries \
  --data-dir ./.benchmark/data \
  --output-dir ./results \
  --only compress_perl_l7 --only extract_perl_zstd

# Run for specific configurations only
.benchmark/benchmark.py \
  --input-dir ./release-binaries \
  --data-dir ./.benchmark/data \
  --output-dir ./results \
  --config clang --config lto
```

## Architecture

### Decorator-Based Registration

Benchmarks are registered using Python decorators:

```python
@benchmark
@needs_binary("mkdwarfs")
@needs_version("0.9.0")
@without_tag("minimal")
def my_benchmark(env):
    """Benchmark description"""
    # Benchmark implementation
    result = env.mkdwarfs("...")
    env.sample(result)
```

**Available Decorators:**
- `@benchmark` - Register function as a benchmark
- `@needs_version(version)` - Require minimum DwarFS version
- `@needs_binary(name)` - Require specific binary (mkdwarfs, dwarfs, etc.)
- `@needs_files(*files)` - Require specific data files
- `@needs_tag(tag)` - Require configuration tag
- `@without_tag(tag)` - Exclude configurations with tag

### Configuration Discovery

The framework automatically discovers build configurations from filename patterns:

**Standalone Tarballs:**
```
dwarfs-{version}-{commits}-{hash}-Linux-{arch}-{config}.tar.zst
```

**Universal Binaries:**
```
dwarfs-universal-{version}-{commits}-{hash}-Linux-{arch}-{config}
```

**FUSE-Extract Binaries:**
```
dwarfs-fuse-extract-{version}-{commits}-{hash}-Linux-{arch}-{config}
```

**Configuration Tags:**
Extracted from filename, separated by `-`:
- `gcc` / `clang` - Compiler used
- `lto` - Link-time optimization enabled
- `minsize` - Size-optimized build
- `minimal` - Minimal feature set
- `musl` - Built against musl libc
- `mimalloc` - Using mimalloc allocator

### Benchmark Environment

Each benchmark receives an `env` object with:

```python
class BenchmarkEnvironment:
    def tmp(name)           # Get path in temp directory
    def data(name)          # Get path in data directory
    def mkdwarfs(*args)     # Run mkdwarfs with hyperfine
    def dwarfs(*args)       # Run dwarfs with hyperfine
    def dwarfsck(*args)     # Run dwarfsck with hyperfine
    def dwarfsextract(*args)# Run dwarfsextract with hyperfine
    def hyperfine(*cmd)     # Run arbitrary command with hyperfine
    def sample(result)      # Save benchmark result
```

## Available Benchmarks

### Binary Size Benchmarks

- `mkdwarfs_size` - Size of mkdwarfs binary
- `dwarfsck_size` - Size of dwarfsck binary
- `dwarfsextract_size` - Size of dwarfsextract binary
- `dwarfs_size` - Size of dwarfs FUSE binary

### Compression Benchmarks

- `segmenter_perl_l7` - Segmentation at level 7 (no compression)
- `segmenter_perl_l9` - Segmentation at level 9 (no compression)
- `compress_perl_l7` - Compression with zstd:12 at level 7
- `compress_perl_l9` - Compression with lzma:3 at level 9
- `compress_fits` - FITS file compression with categorization
- `compress_pcmaudio` - PCM audio compression with categorization

### Extraction Benchmarks

- `extract_perl_zstd` - Extract Perl dataset to directory
- `extract_perl_zstd_gnutar` - Extract Perl to GNU tar
- `extract_perl_zstd_gnutar_devnull` - Extract Perl to /dev/null
- `extract_fits` - Extract FITS dataset
- `extract_fits_gnutar` - Extract FITS to GNU tar
- `extract_pcmaudio` - Extract PCM audio dataset
- `extract_pcmaudio_gnutar` - Extract PCM audio to GNU tar

### Verification Benchmarks

- `dwarfsck_no_check_perl_zstd` - Quick metadata check (no file verification)
- `check_integrity_perl_zstd` - Full integrity check
- `checksum_files_perl_zstd_sha256` - Checksum all files with SHA256

### FUSE Benchmarks

- `mount_and_run_emacs_l6` - Mount and run Emacs AppImage (level 6)
- `mount_and_run_emacs_l6_mmap` - Same with mmap block allocator
- `mount_and_run_emacs_l6_foreground` - Same in foreground mode
- `mount_and_run_emacs_l9` - Mount and run Emacs AppImage (level 9)
- `mount_and_run_emacs_l9_foreground` - Same in foreground mode
- `mount_and_cat_files` - Mount and read multiple files in parallel
- `mount_and_cat_files_mmap` - Same with mmap block allocator
- `mount_and_cat_files_foreground` - Same in foreground mode

## Output Format

Results are saved as JSON files with comprehensive metadata:

```json
{
  "name": "benchmark_name",
  "type": "standalone|universal|fuse-extract",
  "is_release": true|false,
  "arch": "x86_64|aarch64|...",
  "compiler": "gcc|clang",
  "lto": true|false,
  "minsize": true|false,
  "minimal": true|false,
  "musl": true|false,
  "mimalloc": true|false,
  "processor": "...",
  "cpus": "0-3",
  "hostname": "...",
  "config": "clang-lto",
  "version": "0.9.0",
  "commit": "abc123",
  "commit_time": 1234567890.0,
  "time": 1234567890.0,
  "tags": ["clang", "lto"],

  // Benchmark-specific results
  "wall_time": 1.234,
  "cpu_time": 5.678,
  "mean": 1.234,
  "stddev": 0.012,
  "median": 1.230,
  "user": 4.567,
  "system": 1.111,
  "min": 1.200,
  "max": 1.280,
  "image_size": 12345678,
  "binary_size": 98765
}
```

## Data Files

Benchmarks require specific data files in the `--data-dir`:

- `perl-install-small/` - Small Perl installation (~100MB)
- `perl-install-small-v0.7.5.dwarfs` - Pre-compressed Perl dataset
- `perl-install-1M-zstd.dwarfs` - 1M Perl installations compressed
- `emacs-{arch}-l6.dwarfs` - Emacs AppImage level 6
- `emacs-{arch}-l9.dwarfs` - Emacs AppImage level 9
- `2024-02-07.dwarfs` - FITS dataset (if available)
- `2024-02-07/` - FITS source files
- `pcmaudio.dwarfs` - PCM audio dataset (if available)
- `pcmaudio/` - PCM audio source files

## Integration with Hyperfine

The framework uses [hyperfine](https://github.com/sharkdp/hyperfine) for precise benchmarking:

```python
def hyperfine(command, benchmark_name, **kwargs):
    cmd = ["hyperfine"]
    cmd.extend(["--warmup", kwargs.get("warmup", "2")])

    if "min_runs" in kwargs:
        cmd.extend(["--min-runs", str(kwargs["min_runs"])])

    cmd.extend(["--export-json", output_file])
    cmd.extend(["--command-name", benchmark_name])
    cmd.append(command)

    subprocess.run(cmd, check=True)
```

**Features:**
- Automatic warm-up runs
- Statistical analysis (mean, stddev, median, min, max)
- JSON export for further processing
- Multi-run support with configurable minimum runs

## Query and Analysis

Use `query.py` to analyze results:

```bash
# Query specific benchmark results
.benchmark/query.py --results-dir ./results --benchmark compress_perl_l7

# Compare configurations
.benchmark/query.py --results-dir ./results --compare clang lto minimal
```

Use `history.sh` for historical comparisons:

```bash
# Compare performance across versions
.benchmark/history.sh compress_perl_l7
```

## Platform-Specific Defaults

The framework includes platform-specific defaults for common machines:

```python
defaults = {
    "gandalf": {"cpus": "0-15"},     # 16-core workstation
    "tangerinepi5b": {"cpus": "4-7"}, # Orange Pi 5B (efficiency cores)
    "orangepi": {"cpus": "4-7"},      # Orange Pi
}
```

Override with `--cpus` command-line option.

## Advanced Usage

### Custom Benchmark Development

```python
@benchmark
@needs_binary("mkdwarfs")
@needs_version("0.10.0")
def my_custom_benchmark(env):
    """
    Custom benchmark description

    This benchmark tests ...
    """
    # Create test data
    test_data = env.tmp("test_input")
    os.makedirs(test_data, exist_ok=True)
    # ... populate test data ...

    # Run mkdwarfs
    image = env.tmp("output.dwarfs")
    result = env.mkdwarfs(
        f"-i {test_data} -o {image} --custom-option",
        min_runs=5  # Run 5 times minimum
    )

    # Add custom metrics
    result["custom_metric"] = calculate_metric(image)

    # Save results
    env.sample(result)

    # Cleanup
    os.remove(image)
```

### Filtering Benchmarks

```python
# Only run benchmarks for specific versions
python .benchmark/benchmark.py \
  --input-dir ./binaries \
  --config gcc  # Only gcc builds

# Only run fast benchmarks (exclude slow ones)
python .benchmark/benchmark.py \
  --input-dir ./binaries \
  --only mkdwarfs_size --only dwarfsck_size
```

## Shared Infrastructure

Both frameworks can use shared utilities from `benchmarks/lib/`:

```python
# Add to sys.path
import sys
sys.path.insert(0, "./benchmarks/lib")

from memory_tracker import MemoryTracker
from perfmon_parser import PerfmonParser
from fuse_manager import FUSEManager

# Use in benchmarks...
```

## References

- [Benchmark Suite Overview](../benchmarks/README.md)
- [Metadata Format Benchmarks](../benchmarks/README_METADATA_BENCHMARKS.md)
- [Hyperfine Documentation](https://github.com/sharkdp/hyperfine)