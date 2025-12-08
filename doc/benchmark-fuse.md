# FUSE Performance Benchmarking

## Overview

Guide for benchmarking FUSE driver performance in DwarFS, with special focus on comparing different FUSE implementations (FUSE-T vs macFUSE on macOS, libfuse on Linux) and monitoring performance metrics.

## FUSE Implementations

### macOS

DwarFS supports two FUSE implementations on macOS:

#### FUSE-T (Recommended)

- **Type**: User-space FUSE implementation
- **Performance**: Generally faster than macFUSE
- **Stability**: Modern, actively maintained
- **Installation**: `brew install fuse-t`
- **Mount command**: Uses standard `dwarfs` binary

#### macFUSE

- **Type**: Kernel extension-based FUSE
- **Performance**: Mature but slower than FUSE-T
- **Stability**: Long-established, well-tested
- **Installation**: `brew install macfuse`
- **Note**: Requires system extension approval

### Linux

#### libfuse

- **Type**: Standard FUSE implementation
- **Versions**: libfuse2 and libfuse3 supported
- **Performance**: Highly optimized for Linux
- **Installation**: Usually pre-installed or via package manager

## Performance Monitoring

### Using the perfmon Option

DwarFS includes built-in performance monitoring that tracks FUSE operation latencies:

```bash
dwarfs image.dwarfs mountpoint -o perfmon=fuse
```

#### Available Components

- `fuse`: FUSE driver operations
- `filesystem_v2`: Filesystem implementation
- `inode_reader_v2`: Inode reading operations
- `block_cache`: Block cache operations

#### Multiple Components

```bash
dwarfs image.dwarfs mountpoint -o perfmon=fuse+block_cache+filesystem_v2
```

### Extracting Performance Data

While the filesystem is mounted, extract perfmon data using extended attributes:

```bash
# View all perfmon data
xattr -p user.dwarfs.driver.perfmon mountpoint

# Save to file for analysis
xattr -p user.dwarfs.driver.perfmon mountpoint > perfmon_data.txt
```

### Performance Trace

For detailed performance analysis, enable trace output:

```bash
dwarfs image.dwarfs mountpoint \
  -o perfmon=fuse+block_cache \
  -o perfmon_trace=/tmp/perfmon_trace.json
```

The trace file is written when the filesystem is unmounted and contains JSON-formatted timing data for all monitored operations.

## Latency Metrics

### FUSE Operation Types

The perfmon system tracks these FUSE operations:

- **op_init**: Filesystem initialization
- **op_lookup**: Directory entry lookup
- **op_getattr**: Get file/directory attributes
- **op_readlink**: Read symbolic link target
- **op_open**: Open file
- **op_read**: Read file data
- **op_readdir**: Read directory contents
- **op_statfs**: Get filesystem statistics
- **op_getxattr**: Get extended attribute
- **op_listxattr**: List extended attributes

### Latency Statistics

For each operation, the following statistics are tracked:

- **samples**: Number of times the operation was called
- **avg**: Average latency in microseconds
- **p50**: Median latency (50th percentile)
- **p90**: 90th percentile latency
- **p99**: 99th percentile latency

### Example Output

```
op_init: samples=1, avg=199.4us, p50=262.1us, p90=262.1us, p99=262.1us
op_lookup: samples=16, avg=1249us, p50=2097us, p90=4194us, p99=4194us
op_getattr: samples=1, avg=5.786us, p50=8.192us, p90=8.192us, p99=8.192us
op_open: samples=16, avg=7.641us, p50=4.096us, p90=32.77us, p99=32.77us
op_read: samples=45145, avg=71.2us, p50=131.1us, p90=131.1us, p99=262.1us
op_readdir: samples=2, avg=25650us, p50=32.77us, p90=67110us, p99=67110us
```

## Interpreting Latency Metrics

### Understanding Percentiles

- **p50 (Median)**: Half of operations complete faster than this
- **p90**: 90% of operations complete faster than this
- **p99**: 99% of operations complete faster than this

### What to Look For

**Good Performance Indicators**:
- Low average latency (< 100µs for most operations)
- Narrow gap between p50 and p99 (consistent performance)
- Read operations in low microsecond range

**Performance Issues**:
- High p99 values indicate occasional slowdowns
- Large gap between avg and p99 suggests variable performance
- Operations taking milliseconds instead of microseconds

### Operation-Specific Guidelines

**op_lookup** (Directory lookups):
- Expected: 100-500µs
- High values may indicate deep directory structures or cache misses

**op_getattr** (File stat operations):
- Expected: 5-20µs
- Very frequent operation, should be highly optimized

**op_read** (File reads):
- Expected: 50-200µs depending on block size
- Varies significantly based on cache state and compression

**op_readdir** (Directory listing):
- Expected: 1000-10000µs
- Depends on directory size and number of entries

## FUSE-T vs macFUSE Comparison

### Performance Characteristics

| Metric | FUSE-T | macFUSE | Notes |
|--------|--------|---------|-------|
| Read throughput | Higher | Lower | FUSE-T typically 20-30% faster |
| Operation latency | Lower | Higher | Especially for metadata operations |
| CPU usage | Lower | Higher | User-space implementation advantage |
| Memory usage | Similar | Similar | Comparable memory footprint |
| Stability | Excellent | Excellent | Both production-ready |

### Benchmark Comparison

To compare FUSE implementations, run identical tests with each:

```bash
# Using FUSE-T
dwarfs image.dwarfs /mnt/test -o perfmon=fuse
# Run your workload
xattr -p user.dwarfs.driver.perfmon /mnt/test > fuse-t-results.txt
umount /mnt/test

# Using macFUSE (if installed)
dwarfs image.dwarfs /mnt/test -o perfmon=fuse
# Run same workload
xattr -p user.dwarfs.driver.perfmon /mnt/test > macfuse-results.txt
umount /mnt/test
```

### Switching Between Implementations

On macOS with both installed, DwarFS automatically detects and uses the available FUSE implementation. To prefer a specific one:

```bash
# Prefer FUSE-T (if available)
export DWARFS_PREFER_FUSE_T=1

# Prefer macFUSE (if available)
unset DWARFS_PREFER_FUSE_T
```

## Running FUSE Benchmarks

### Basic Throughput Test

```bash
# Mount filesystem with perfmon
dwarfs image.dwarfs /mnt/test -o perfmon=fuse

# Sequential read test
time dd if=/mnt/test/largefile of=/dev/null bs=1M

# Random read test
time find /mnt/test -type f -exec cat {} \; > /dev/null

# Get perfmon data
xattr -p user.dwarfs.driver.perfmon /mnt/test

# Unmount
umount /mnt/test
```

### Cache Impact Testing

Compare performance with different cache sizes:

```bash
# Small cache
dwarfs image.dwarfs /mnt/test -o cachesize=64m,perfmon=fuse
# Run workload and measure

# Large cache
dwarfs image.dwarfs /mnt/test -o cachesize=512m,perfmon=fuse
# Run same workload and compare
```

### Worker Thread Scaling

Test impact of parallel decompression:

```bash
# Single worker
dwarfs image.dwarfs /mnt/test -o workers=1,perfmon=fuse
# Measure performance

# Multiple workers
dwarfs image.dwarfs /mnt/test -o workers=4,perfmon=fuse
# Compare performance
```

## Advanced Performance Tuning

### Block Allocator Comparison

```bash
# Using malloc (default)
dwarfs image.dwarfs /mnt/test -o block_allocator=malloc,perfmon=fuse

# Using mmap
dwarfs image.dwarfs /mnt/test -o block_allocator=mmap,perfmon=fuse
```

The `mmap` allocator may perform better with cache tidying enabled:

```bash
dwarfs image.dwarfs /mnt/test \
  -o block_allocator=mmap \
  -o tidy_strategy=time \
  -o tidy_interval=5s \
  -o tidy_max_age=10s \
  -o perfmon=fuse
```

### Sequential Access Optimization

Enable sequential access detection:

```bash
dwarfs image.dwarfs /mnt/test \
  -o seq_detector=3 \
  -o readahead=1m \
  -o perfmon=fuse
```

### File Caching Control

Test with and without file caching:

```bash
# With caching (default)
dwarfs image.dwarfs /mnt/test -o cache_files,perfmon=fuse

# Without caching
dwarfs image.dwarfs /mnt/test -o no_cache_files,perfmon=fuse
```

## Automated Benchmarking

### Simple Benchmark Script

```bash
#!/bin/bash
# fuse-benchmark.sh

IMAGE="$1"
MOUNTPOINT="$2"
TESTFILE="$3"

echo "Mounting with perfmon..."
dwarfs "$IMAGE" "$MOUNTPOINT" -o perfmon=fuse

echo "Running sequential read test..."
time cat "$MOUNTPOINT/$TESTFILE" > /dev/null

echo "Running random access test..."
time head -c 1024 "$MOUNTPOINT/$TESTFILE" > /dev/null

echo "Extracting perfmon data..."
xattr -p user.dwarfs.driver.perfmon "$MOUNTPOINT" > perfmon_results.txt

echo "Unmounting..."
umount "$MOUNTPOINT"

echo "Results saved to perfmon_results.txt"
```

Usage:
```bash
chmod +x fuse-benchmark.sh
./fuse-benchmark.sh image.dwarfs /mnt/test path/to/testfile
```

### Comparing Multiple Configurations

```bash
#!/bin/bash
# compare-configs.sh

IMAGE="$1"
MOUNTPOINT="$2"

for cachesize in 64m 128m 256m 512m; do
  echo "Testing cache size: $cachesize"

  dwarfs "$IMAGE" "$MOUNTPOINT" -o cachesize=$cachesize,perfmon=fuse

  # Run your benchmark workload here
  time cat "$MOUNTPOINT"/* > /dev/null

  xattr -p user.dwarfs.driver.perfmon "$MOUNTPOINT" > "perfmon_${cachesize}.txt"

  umount "$MOUNTPOINT"

  sleep 2
done
```

## Troubleshooting Performance Issues

### High Latency

If operation latencies are unexpectedly high:

1. **Check cache size**: Increase with `-o cachesize=`
2. **Monitor cache hit rate**: Use perfmon to track cache efficiency
3. **Verify FUSE implementation**: Try FUSE-T on macOS
4. **Check compression format**: Some formats decompress faster than others

### Low Throughput

If read throughput is lower than expected:

1. **Increase worker threads**: `-o workers=N`
2. **Enable sequential detection**: `-o seq_detector=3`
3. **Adjust readahead**: `-o readahead=1m`
4. **Check block size**: Larger blocks may help sequential reads

### Memory Issues

If memory usage is too high:

1. **Enable cache tidying**: `-o tidy_strategy=time,tidy_interval=5s,tidy_max_age=10s`
2. **Use mmap allocator**: `-o block_allocator=mmap`
3. **Reduce cache size**: `-o cachesize=64m`
4. **Disable file caching**: `-o no_cache_files`

## Performance Monitoring Best Practices

1. **Baseline measurements**: Always compare against a baseline
2. **Warm vs cold cache**: Test both scenarios
3. **Realistic workloads**: Use actual application access patterns
4. **Multiple runs**: Average results over several runs
5. **System idle**: Minimize background activity during benchmarks
6. **Document configuration**: Record all mount options and system details

## See Also

- [FUSE Driver Documentation](dwarfs.md) - Complete dwarfs mount options
- [Metadata Benchmarking](benchmark-metadata.md) - Metadata format comparison
- [DwarFS Format](dwarfs-format.md) - Technical format details
- [Performance Optimization](dwarfs.md#optimizing-performance-and-memory-usage) - Tuning guide

## Author

Written by Marcus Holland-Moritz.

## Copyright

Copyright (C) Marcus Holland-Moritz.