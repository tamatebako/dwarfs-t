# DwarFS Compression Algorithm Benchmark Report

**Generated**: 2025-12-01 20:44:25   
**Build Directory**: `build-fb`  
**Build Type**: Release  
**Total Tests**: 24 (all passed ✅)

## Executive Summary

This report presents comprehensive benchmark results for all compression algorithms
supported by DwarFS. All algorithms work in **FlatBuffers-only builds** without
requiring Apache Thrift.

### Key Findings

- **Best Compression**: `ricepp:block_size=128` achieves **31.98%** compression ratio
- **Fastest Compression**: `zstd:level=3` at **2383.8 MB/s**
- **Fastest Decompression**: `zstd:level=5` at **13100.3 MB/s**
- **Best Balance**: `ricepp:block_size=512` with **32.13%** compression at **362.2 MB/s**

### Specialized Algorithms

- **FLAC** (PCM Audio): ✅ Working - 4 configurations tested, **82.98%** avg compression
- **Rice++** (FITS Images): ✅ Working - 3 configurations tested, **31.98%** avg compression

### Build Configuration

- **FlatBuffers**: ✅ Enabled
- **Thrift**: ❌ Disabled
- **All algorithms functional**: ✅ All 6 algorithms working without Thrift dependency



## Algorithm Comparison Matrix

This table shows all tested algorithm configurations with their performance metrics.

| Algorithm | Configuration | Ratio | Comp Speed | Decomp Speed | Dataset | Use Case |
|-----------|--------------|-------|------------|--------------|---------|----------|
| **BROTLI** | `brotli:quality=1` | 99.10% | 321.8 MB/s | 957.8 MB/s | source_code | Web/HTTP compression |
|  | `brotli:quality=5` | 99.25% | 421.0 MB/s | 9539.1 MB/s | source_code |  |
|  | `brotli:quality=9` | 99.35% | 140.4 MB/s | 9090.9 MB/s | source_code |  |
|  | `brotli:quality=11` | 99.21% | 0.5 MB/s | 5168.0 MB/s | source_code |  |
| **FLAC** | `flac:level=0` | 82.98% | 66.2 MB/s | 308.9 MB/s | pcm_audio | PCM audio compression |
|  | `flac:level=3` | 83.31% | 187.8 MB/s | 376.8 MB/s | pcm_audio |  |
|  | `flac:level=5` | 83.31% | 134.4 MB/s | 373.9 MB/s | pcm_audio |  |
|  | `flac:level=8` | 83.31% | 60.7 MB/s | 389.4 MB/s | pcm_audio |  |
| **LZ4** | `lz4` | 88.07% | 623.2 MB/s | 1665.5 MB/s | logs | Maximum speed |
| **LZ4HC** | `lz4hc:level=1` | 89.21% | 1891.8 MB/s | 7570.9 MB/s | logs | Better compression than lz4 |
|  | `lz4hc:level=9` | 95.74% | 35.8 MB/s | 10989.0 MB/s | logs |  |
| **LZMA** | `lzma:level=0` | 46.95% | 18.6 MB/s | 100.0 MB/s | binary | Maximum compression |
|  | `lzma:level=3` | 47.34% | 13.8 MB/s | 102.5 MB/s | binary |  |
|  | `lzma:level=6` | 48.25% | 10.0 MB/s | 105.7 MB/s | binary |  |
|  | `lzma:level=9` | 48.25% | 7.8 MB/s | 105.5 MB/s | binary |  |
| **RICEPP** | `ricepp:block_size=128` | 31.98% | 358.9 MB/s | 379.9 MB/s | fits_image | Astronomical FITS images |
|  | `ricepp:block_size=256` | 32.08% | 343.0 MB/s | 388.1 MB/s | fits_image |  |
|  | `ricepp:block_size=512` | 32.13% | 362.2 MB/s | 391.1 MB/s | fits_image |  |
| **ZSTD** | `zstd:level=1` | 99.25% | 528.5 MB/s | 486.6 MB/s | source_code | General purpose (default) |
|  | `zstd:level=3` | 99.24% | 2383.8 MB/s | 7151.4 MB/s | source_code |  |
|  | `zstd:level=5` | 99.17% | 552.1 MB/s | 13100.3 MB/s | source_code |  |
|  | `zstd:level=9` | 99.29% | 245.7 MB/s | 10371.5 MB/s | source_code |  |
|  | `zstd:level=19` | 99.32% | 0.1 MB/s | 5422.5 MB/s | source_code |  |
|  | `zstd:level=22` | 99.32% | 0.4 MB/s | 10508.0 MB/s | source_code |  |

### Legend

- **Ratio**: Compression ratio (higher = better compression)
- **Comp Speed**: Compression throughput in MB/s (higher = faster)
- **Decomp Speed**: Decompression throughput in MB/s (higher = faster)


## Performance Analysis

### Speed vs Compression Trade-offs

Different algorithms optimize for different goals. Here's how they compare:


#### Maximum Speed

Optimized for fast compression with acceptable compression ratios:

- **zstd:level=3**: 99.24% compression at 2383.8 MB/s
- **lz4hc:level=1**: 89.21% compression at 1891.8 MB/s
- **lz4**: 88.07% compression at 623.2 MB/s
- **zstd:level=5**: 99.17% compression at 552.1 MB/s
- **zstd:level=1**: 99.25% compression at 528.5 MB/s

#### Balanced

Good balance between speed and compression:

- **ricepp:block_size=512**: 32.13% compression at 362.2 MB/s
- **ricepp:block_size=128**: 31.98% compression at 358.9 MB/s
- **ricepp:block_size=256**: 32.08% compression at 343.0 MB/s
- **flac:level=3**: 83.31% compression at 187.8 MB/s
- **flac:level=5**: 83.31% compression at 134.4 MB/s

#### Maximum Compression

Optimized for best compression ratios, slower compression speeds:

- **brotli:quality=5**: 99.25% compression at 421.0 MB/s
- **brotli:quality=1**: 99.10% compression at 321.8 MB/s
- **zstd:level=9**: 99.29% compression at 245.7 MB/s
- **brotli:quality=9**: 99.35% compression at 140.4 MB/s
- **lz4hc:level=9**: 95.74% compression at 35.8 MB/s

### Decompression Performance

Decompression speed is critical for read performance:

- **zstd:level=5**: 13100.3 MB/s (compression: 99.17%)
- **lz4hc:level=9**: 10989.0 MB/s (compression: 95.74%)
- **zstd:level=22**: 10508.0 MB/s (compression: 99.32%)
- **zstd:level=9**: 10371.5 MB/s (compression: 99.29%)
- **brotli:quality=5**: 9539.1 MB/s (compression: 99.25%)


## Use Case Recommendations

Based on benchmark results, here are the recommended algorithms for different scenarios:

### General Purpose

**Recommended**: `zstd:level=3` (default)
- **Compression**: 99.24%
- **Speed**: 2383 MB/s
- **Why**: Excellent balance of speed and compression for most use cases

**Alternative**: `zstd:level=5`
- **Compression**: 99.17%
- **Speed**: 552 MB/s  
- **Why**: Slightly better compression, still very fast

### Maximum Compression

**Recommended**: `lzma:level=9`
- **Compression**: 48.25%
- **Speed**: 7.8 MB/s
- **Why**: Best compression ratio for archival storage where space matters most

**Alternative**: `brotli:quality=9`
- **Compression**: 99.35%
- **Speed**: 140 MB/s
- **Why**: Better compression speed while maintaining excellent ratio

### Maximum Speed

**Recommended**: `lz4hc:level=1`
- **Compression**: 89.21%
- **Speed**: 1892 MB/s
- **Why**: Near-instant compression with decent ratio

**Alternative**: `lz4` (default)
- **Compression**: 88.07%
- **Speed**: 623 MB/s
- **Why**: Slightly faster than lz4hc with minimal compression loss

### Audio Files (PCM)

**Recommended**: `flac:level=3`
- **Compression**: 83.31%
- **Speed**: 188 MB/s
- **Why**: Excellent lossless audio compression, fast enough for real-time

**Alternative**: `flac:level=5`
- **Compression**: 83.31%
- **Speed**: 134 MB/s
- **Why**: Same compression, slightly slower but more thorough analysis

### Astronomical Images (FITS)

**Recommended**: `ricepp:block_size=128`
- **Compression**: 31.98%
- **Speed**: 359 MB/s
- **Why**: Specialized for astronomical data, excellent for FITS images

**Note**: Rice++ compression is now fully functional in **FlatBuffers-only builds**,
making it easier to build and deploy without Apache Thrift dependencies.

### Web/HTTP Compression

**Recommended**: `brotli:quality=5`
- **Compression**: 99.25%
- **Speed**: 421 MB/s
- **Why**: Designed for HTTP compression, excellent browser support

### Critical Performance Considerations

1. **Default recommendations** are based on the "balanced" scenario
2. **Storage-critical** environments should use lzma or brotli:quality=9+
3. **Speed-critical** environments should use lz4 or lz4hc:level=1
4. **Mixed workloads** benefit from zstd's flexibility (levels 1-22)



## Build Configuration Summary

### Current Build

- **Build Type**: Release
- **FlatBuffers**: ✅ Enabled (required)
- **Thrift**: ❌ Disabled (optional)

### Algorithm Availability

All 6 compression algorithms are available in this build:

| Algorithm | Status | FlatBuffers-Only | Dual-Format | Notes |
|-----------|--------|------------------|-------------|-------|
| **zstd** | ✅ Working | ✅ Yes | ✅ Yes | Default algorithm |
| **lzma** | ✅ Working | ✅ Yes | ✅ Yes | Maximum compression |
| **lz4** | ✅ Working | ✅ Yes | ✅ Yes | Maximum speed |
| **lz4hc** | ✅ Working | ✅ Yes | ✅ Yes | Better than lz4 |
| **brotli** | ✅ Working | ✅ Yes | ✅ Yes | Web compression |
| **flac** | ✅ Working | ✅ Yes | ✅ Yes | Audio compression |
| **ricepp** | ✅ Working | ✅ Yes | ✅ Yes | FITS images |

### Key Achievement: Rice++ Thrift Independence

Rice++ compression has been successfully refactored to work without Apache Thrift:

- **Before**: Required Thrift for metadata handling
- **After**: Works in FlatBuffers-only builds
- **Impact**: Easier builds, better portability, fewer dependencies

This makes DwarFS more portable and easier to build on platforms where Thrift/Folly
are difficult to compile (Windows, older macOS, various architectures).

### Build Options

To build with different configurations:

```bash
# FlatBuffers-only (recommended)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF

# Dual-format (backward compatibility)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON

# Thrift-only (NOT SUPPORTED - FlatBuffers required)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON  # FAILS
```



---

**Report Generated**: 2025-12-01 20:50:57   
**Source Data**: `benchmark-results/compression-algorithms.json`  
**Total Tests**: 24 (24 passed, 0 failed)  
**Execution Time**: 6.70 seconds
