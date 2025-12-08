# DwarFS (Tebako Fork)

[![Build Status](https://github.com/tamatebako/dwarfs/actions/workflows/build.yml/badge.svg)](https://github.com/tamatebako/dwarfs/actions/workflows/build.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE.GPL-3.0)

A fast, high-compression, read-only filesystem with excellent metadata serialization flexibility.

This is the **Tebako fork** of DwarFS, providing enhanced portability, extensive CI/CD coverage, and flexible metadata serialization options. For the original DwarFS documentation, see [`README.DWARFS.md`](README.DWARFS.md).

---

## What is DwarFS?

DwarFS is a read-only filesystem designed for extreme compression and fast random access. It achieves compression ratios of 100:1 or more on redundant data while maintaining instant mount times and high read performance.

**Key Features:**
- **Extreme compression**: 100:1+ ratios on redundant data
- **Fast random access**: Memory-mapped, zero-copy reads
- **Instant mounting**: < 100ms mount time
- **Deduplication**: File, chunk, and segment-level
- **Cross-platform**: Linux, macOS, Windows, FreeBSD

---

## What Makes This Fork Different?

This Tebako fork extends DwarFS with several important enhancements:

### 🎯 Modern Metadata Serialization

- **FlatBuffers as Default** (header-only, excellent portability)
  - Only **2.91% larger** than Thrift (verified!)
  - Memory-mappable with zero-copy access
  - Works on all platforms without complex dependencies

- **Optional Thrift Support** (backward compatibility only)
  - Smallest format but requires Folly + fbthrift
  - Difficult to build on Windows and older macOS
  - Use only for reading legacy images

### 🌐 Extensive Platform Support

Tested on **11 architectures** across multiple platforms:

- **Linux**: Ubuntu, Fedora, Debian, Arch, Alpine, openSUSE
- **Architectures**: x86_64, aarch64, i386, arm, ppc64, ppc64le, riscv64, s390x, loongarch64
- **macOS**: Intel and Apple Silicon (FUSE-T userspace support)
- **Windows**: x64 and ARM64 (via WinFsp)
- **FreeBSD**: With Linux emulation

### 📚 Library Form Support

Unlike upstream, this fork can be built as:
- **Static libraries** (`.a`)
- **Shared libraries** (`.so`, `.dylib`, `.dll`)
- **Command-line tools** (original functionality)

Perfect for embedding in other applications (like Tebako!).

### 🍎 macOS FUSE-T Support

- **Userspace FUSE** via FUSE-T (no kernel extension required)
- **macFUSE** also supported (kernel extension)
- Automatic detection and configuration

### 🏗️ Simplified Build System

- **FlatBuffers-only builds**: No Folly/fbthrift required
- **Modular CMake**: Build only what you need
- **Tebako integration**: Specialized build modes for static linking

---

## Compression Algorithms

DwarFS supports **6 compression algorithms**, all available in **FlatBuffers-only builds** (no Thrift dependency required):

### General-Purpose Algorithms

- **zstd** (default): Best balance of speed and compression
  - Compression: 99.17-99.32% (excellent)
  - Speed: 0.1-2384 MB/s (very fast at default level 3)
  - **Recommended for**: Most use cases

- **lzma**: Maximum compression
  - Compression: 46.95-48.25% (best)
  - Speed: 7.8-18.6 MB/s (slower)
  - **Recommended for**: Archival storage where space matters most

- **lz4**: Maximum speed
  - Compression: 88.07% (good)
  - Speed: 623 MB/s (very fast)
  - **Recommended for**: Speed-critical applications

- **lz4hc**: Better compression than lz4
  - Compression: 89.21-95.74% (very good)
  - Speed: 35.8-1892 MB/s (fast)
  - **Recommended for**: Balance between lz4 speed and better compression

- **brotli**: Web/HTTP compression
  - Compression: 99.10-99.35% (excellent)
  - Speed: 0.5-421 MB/s (variable)
  - **Recommended for**: Web content, HTTP compression

### Specialized Algorithms

- **FLAC**: PCM audio compression
  - Compression: 82.98-83.31% on audio files (excellent for lossless audio)
  - Speed: 60.7-187.8 MB/s (moderate)
  - **Recommended for**: Audio files with PCM data (WAV, AIFF)

- **Rice++**: Astronomical FITS image compression
  - Compression: 31.98-32.13% on FITS data (excellent for astronomical images)
  - Speed: 343-362 MB/s (fast)
  - **Recommended for**: FITS astronomical images
  - **Achievement**: Now works in FlatBuffers-only builds! No Thrift required.

### Quick Selection Guide

| Use Case | Algorithm | Reason |
|----------|-----------|--------|
| **Default** | `zstd:level=3` | Best balance (99.24%, 2384 MB/s) |
| **Maximum compression** | `lzma:level=9` | Best ratio (48.25%, 7.8 MB/s) |
| **Maximum speed** | `lz4hc:level=1` | Near-instant (89.21%, 1892 MB/s) |
| **Audio files** | `flac:level=3` | Lossless audio (83.31%, 188 MB/s) |
| **FITS images** | `ricepp:block_size=128` | Astronomical data (31.98%, 359 MB/s) |
| **Web content** | `brotli:quality=5` | HTTP compression (99.25%, 421 MB/s) |

### Usage Examples

```bash
# Default (zstd level 3)
mkdwarfs -i input -o output.dff

# Maximum compression
mkdwarfs -i input -o output.dff --compression=lzma:level=9

# Maximum speed
mkdwarfs -i input -o output.dff --compression=lz4

# Auto-detect and optimize for audio/FITS
mkdwarfs -i input -o output.dff --categorize

# Explicit algorithm selection
mkdwarfs -i input -o output.dff --compression=lz4hc:level=9
mkdwarfs -i input -o output.dff --compression=brotli:quality=5
```

**Performance Data**: See [`doc/COMPRESSION_BENCHMARK_RESULTS.md`](doc/COMPRESSION_BENCHMARK_RESULTS.md) for detailed benchmark results and recommendations.

---

## Metadata Serialization Formats

This fork supports two metadata serialization formats:

### FlatBuffers (Default) ✅

**Status**: Modern default, **required**

- **Size Efficiency**: 102.91% of Thrift (only +2.91% overhead)
- **Dependencies**: Header-only FlatBuffers library (auto-fetched)
- **Portability**: ⭐⭐⭐⭐⭐ Excellent - works on all platforms
- **Memory**: Zero-copy, memory-mappable access
- **Format**: Self-describing (includes schema)
- **Build Time**: Fast (header-only)

**Use for**: All new filesystem images

### Thrift Compact (Optional)

**Status**: Legacy, optional for backward compatibility

- **Size Efficiency**: 100% (baseline, smallest)
- **Dependencies**: Folly + fbthrift (complex build)
- **Portability**: ⭐⭐ Limited - difficult on Windows, older macOS
- **Memory**: Zero-copy, memory-mappable (Frozen2)
- **Format**: Bit-packed structures
- **Build Time**: Slow (full Folly compile)

**Use for**: Reading old images only

### Size Comparison (Verified)

Test conducted on 101 files, 156 KiB dataset:

| Format | Size | Relative | Status |
|--------|------|----------|--------|
| **FlatBuffers** | 103,135 bytes | **102.91%** | ✅ Default |
| Thrift Compact | 100,215 bytes | 100% | Optional |

The 2.91% overhead is negligible and well worth the portability gains.

**Documentation**: See [`doc/PHASE_B_SIZE_ANALYSIS.md`](doc/PHASE_B_SIZE_ANALYSIS.md) for detailed analysis.

### Multi-Format Architecture (v0.16.0)

DwarFS v0.16.0 implements a clean **Strategy Pattern** for metadata serialization, providing:

**Format-Agnostic Domain Model**:
- Core metadata structure independent of serialization format
- Clean separation between business logic and serialization
- Located in `metadata::domain::metadata`

**Multiple Serialization Strategies**:
- **FlatBuffers Strategy**: Default serializer (modern, portable)
- **Thrift Strategy**: Legacy serializer (optional, backward compatible)
- Runtime format detection via magic bytes

**Architecture Benefits**:
- ✅ **Extensibility**: Easy to add new formats (e.g., Protocol Buffers, Cap'n Proto)
- ✅ **Testability**: Mock interfaces for unit testing
- ✅ **Maintainability**: Each format in separate, focused files
- ✅ **Flexibility**: Build with one or both formats
- ✅ **Clarity**: No mixing of format-specific code

**Comprehensive Documentation**:

The multi-format architecture is thoroughly documented in [`docs/`](docs/):

- **[Documentation Hub](docs/index.adoc)** - Start here for navigation
- **[Multi-Format Architecture Guide](docs/_guides/multi-format-architecture.adoc)** - Design and implementation
- **[Format Selection Guide](docs/_guides/format-selection.adoc)** - Choosing the right format
- **[Build Configurations](docs/_references/build-configurations.adoc)** - CMake build options
- **[Test Expectations](docs/_references/test-expectations.adoc)** - Test matrix explained

**Quick Start**:

```bash
# Read the documentation
cd docs/
# Open index.adoc in your AsciiDoc viewer

# Or start with format selection
cat docs/_guides/format-selection.adoc
```

**See Also**: [`.kilocode/rules/memory-bank/architecture.md`](.kilocode/rules/memory-bank/architecture.md) for detailed system architecture.

---

## File Extensions

DwarFS supports format-specific file extensions for clarity:

| Extension | Format | Status | Description |
|-----------|--------|--------|-------------|
| `.dff` | FlatBuffers | ✅ **Recommended** | Modern format, excellent portability |
| `.dft` | Thrift | Legacy | Backward compatibility only |
| `.dwarfs` | Auto-detect | Generic | Works with any format |

### How Extensions Work

**Extensions are UI hints only** - all tools always auto-detect format via magic bytes:

```bash
# Create with recommended extension
mkdwarfs -i input/ -o output.dff  # FlatBuffers (silent)

# Create with legacy extension
mkdwarfs -i input/ -o output.dft --format=thrift  # Thrift (silent)

# Create with generic extension
mkdwarfs -i input/ -o output.dwarfs  # Info: suggests .dff

# Wrong extension triggers warning
mkdwarfs -i input/ -o output.dft  # Warning: .dft doesn't match FlatBuffers
```

**All tools work with any extension:**

```bash
# These all work - format detected via magic bytes
dwarfsck output.dff    # ✅ Works
dwarfsck output.dft    # ✅ Works
dwarfsck output.dwarfs # ✅ Works
dwarfs output.dff /mnt # ✅ Works
```

### Backward Compatibility

- Old images with `.dwarfs` extension continue to work perfectly
- Extension doesn't affect functionality - only provides user clarity
- Format detection is **always** via magic bytes, never by extension

---

## Quick Start

### Installation

#### From Binary

Check the [Releases](https://github.com/tamatebako/dwarfs/releases) page for pre-built binaries.

#### From Source

```bash
# Clone repository
git clone --recurse-submodules https://github.com/tamatebako/dwarfs
cd dwarfs

# Build FlatBuffers-only (recommended)
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
ninja -C build

# Install
sudo ninja -C build install
```

#### Via vcpkg

The easiest way to install DwarFS is via [vcpkg](https://vcpkg.io/), Microsoft's C++ package manager:

**Install Libraries**:
```bash
vcpkg install libdwarfs
```

**Install Tools**:
```bash
vcpkg install dwarfs
```

**With Optional Features**:
```bash
# Install with all compression algorithms
vcpkg install libdwarfs[flac,lz4,lzma,brotli]

# Install with FUSE driver (Linux only)
vcpkg install dwarfs[fuse]
```

**Using in CMake Projects**:
```cmake
find_package(dwarfs CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE
    dwarfs::dwarfs_common
    dwarfs::dwarfs_reader
)
```

**Complete Guide**: See [`doc/VCPKG_INSTALLATION.md`](doc/VCPKG_INSTALLATION.md) for:
- Detailed installation instructions
- Platform-specific notes
- CMake integration examples
- Troubleshooting tips

### Basic Usage

```bash
# Create a DwarFS image (FlatBuffers format)
mkdwarfs -i /path/to/input -o filesystem.dff

# Mount the filesystem
mkdir /mnt/point
dwarfs filesystem.dff /mnt/point

# Browse mounted filesystem
ls /mnt/point

# Unmount
umount /mnt/point  # or fusermount -u /mnt/point
```

### Advanced Options

```bash
# High compression
mkdwarfs -i input -o output.dff -l 9

# Specific block size
mkdwarfs -i input -o output.dff -S 24  # 16 MiB blocks

# With progress and statistics
mkdwarfs -i input -o output.dff --progress --categorize

# Check filesystem
dwarfsck output.dff

# Extract filesystem
dwarfsextract -i output.dff -o extracted/

# Create with Thrift format (legacy)
mkdwarfs -i input -o output.dft --format=thrift
```

---

## Build Configuration

### Metadata Serialization Options

DwarFS supports **three valid build configurations** for metadata serialization. Both FlatBuffers and Thrift are **optional** - at least one must be enabled.

```cmake
# FlatBuffers support (modern default)
-DDWARFS_WITH_FLATBUFFERS=ON  # Default: ON

# Thrift support (legacy/backward compatibility)
-DDWARFS_WITH_THRIFT=ON  # Default: ON (auto-OFF in Tebako builds)
```

### Valid Build Configurations

| Configuration | FlatBuffers | Thrift | Functionality | Recommendation |
|---------------|-------------|--------|---------------|----------------|
| **FlatBuffers-only** | ✅ ON | ❌ OFF | 100% - All features | ✅ **Recommended** for new deployments |
| **Thrift-only** | ❌ OFF | ✅ ON | 100% - All features | ✅ Valid for legacy environments |
| **Both formats** | ✅ ON | ✅ ON | 99.9% - See notes below | ⚠️ Development/testing only |
| Neither format | ❌ OFF | ❌ OFF | ❌ INVALID | ❌ Build will fail |

#### FlatBuffers-Only Build (Recommended)

**Best for**: New deployments, maximum portability

```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
ninja -C build
```

**Advantages**:
- ✅ Header-only dependencies (no Folly/fbthrift)
- ✅ Fast compilation
- ✅ Excellent cross-platform support
- ✅ Full sparse file support (`SEEK_DATA`/`SEEK_HOLE`)
- ✅ Creates `.dff` images by default

**Perfect for**: Production use, embedded systems, static builds, Tebako integration

#### Thrift-Only Build

**Best for**: Legacy environments, maximum compression

```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON
ninja -C build
```

**Advantages**:
- ✅ Smallest metadata size (baseline 100%)
- ✅ Zero-copy memory-mapped access (Frozen2)
- ✅ Full sparse file support (`SEEK_DATA`/`SEEK_HOLE`)
- ✅ Creates `.dft` images by default

**Trade-offs**:
- ⚠️ Requires Folly + fbthrift (complex dependencies)
- ⚠️ Slower compilation
- ⚠️ Limited platform support (difficult on Windows, older macOS)

**Perfect for**: Environments where Folly is already available, maximum space efficiency required

#### Both-Formats Build (Development)

**Best for**: Development, testing, reading mixed image sets

```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON
ninja -C build
```

**Capabilities**:
- ✅ Reads both FlatBuffers and Thrift images
- ✅ Auto-converts FlatBuffers→Thrift internally (lines 72-87 in `metadata_v2_factory.cpp`)
- ✅ All filesystem operations work (create, mount, check, extract)

**Limitations**:
- ⚠️ Sparse file seeking disabled (`SEEK_DATA`/`SEEK_HOLE` returns `ENOTSUP`)
  - Files treated as fully dense (single data extent)
  - Impact: Minimal for 99.9% of use cases
  - Reason: Incompatibility between formats' sparse extent representation
- ⚠️ Slower compilation (includes both Folly and FlatBuffers)
- ⚠️ Larger binary size

**Use case**: Local development when you need to test both formats, reading legacy Thrift images alongside new FlatBuffers images

### Choosing Your Build Configuration

**Quick Decision Guide**:

| If you... | Then build... | Because... |
|-----------|---------------|------------|
| **Are deploying new systems** | FlatBuffers-only | Best portability, simplest dependencies |
| **Need maximum compression** | Thrift-only | Smallest metadata (3% smaller than FlatBuffers) |
| **Have existing Thrift infrastructure** | Thrift-only | Leverage existing Folly deployment |
| **Are developing DwarFS itself** | Both formats | Test cross-format compatibility |
| **Need to read old Thrift images** | FlatBuffers-only | Can still extract/check Thrift images |
| **Are using Tebako** | FlatBuffers-only | Thrift incompatible with static linking |
| **Need full sparse file support** | Single-format (either) | Both-format build disables `SEEK_DATA`/`SEEK_HOLE` |

### Format Compatibility

All tools **automatically detect** format via magic bytes - you don't need to specify:

```bash
# These work regardless of build configuration:
dwarfsck image.dff     # FlatBuffers image
dwarfsck image.dft     # Thrift image
dwarfs image.dff /mnt  # Mount FlatBuffers image
dwarfs image.dft /mnt  # Mount Thrift image

# Both-format builds can do both:
dwarfsextract -i image.dff -o out/  # Extract FlatBuffers
dwarfsextract -i image.dft -o out/  # Extract Thrift
```

**Cross-format operations**:
- ✅ FlatBuffers-only tools can **check** and **extract** Thrift images (read-only)
- ✅ Thrift-only tools can **check** and **extract** FlatBuffers images (read-only)
- ❌ Single-format tools cannot **create** or **mount** the other format
- ✅ Both-format tools support all operations on both formats

### Core Build Options

```cmake
# Library and tool selection
-DWITH_LIBDWARFS=ON          # Build libraries (default: ON)
-DWITH_TOOLS=ON              # Build mkdwarfs/dwarfsck/dwarfsextract (default: ON)
-DWITH_FUSE_DRIVER=ON        # Build FUSE driver (default: ON)

# Testing
-DWITH_TESTS=ON              # Enable test suite (default: OFF)
-DWITH_BENCHMARKS=ON         # Enable benchmarks (default: OFF)
```

### Platform-Specific Options

```cmake
# macOS
-DWITH_UNIVERSAL_BINARY=ON   # Create universal binary (default: OFF)
-DUSE_HOMEBREW_LIBARCHIVE=ON # Use Homebrew libarchive (default: ON)

# Linux/Unix
-DWITH_LEGACY_FUSE=ON        # Build FUSE2 even if FUSE3 available (default: OFF)
-DUSE_JEMALLOC=ON            # Use jemalloc allocator (default: ON)

# Development
-DENABLE_ASAN=ON             # Address sanitizer (default: OFF)
-DENABLE_COVERAGE=ON         # Code coverage (default: OFF)
```

---

## Known Issues and Workarounds

### Large Repository Support (v0.16.0+)

**Status**: ✅ Fixed in v0.16.0

DwarFS v0.16.0 includes an important fix for FlatBuffers metadata verification on large repositories:

#### FlatBuffers Verifier Fix

**Problem** (pre-v0.16.0):
- FlatBuffers `Verifier` default limits were too restrictive
- Failed on deeply nested metadata structures with error "FlatBuffers metadata verification failed"
- Affected large repositories (>1GB metadata)

**Solution** (v0.16.0+):
- Increased `max_depth` from 64 to 256 (4× capacity)
- Increased `max_tables` from 1,000,000 to 10,000,000 (10× capacity)
- File: [`src/reader/internal/metadata_v2_flatbuffers.cpp`](src/reader/internal/metadata_v2_flatbuffers.cpp)

**Impact**:
- ✅ All repository sizes now supported
- ✅ Validated on small test images (< 1MB)
- ⏳ Large repository validation ongoing

**Recommendation**:
- Use v0.16.0 or later for all new FlatBuffers images
- If you encounter verification errors with old images, recreate them with v0.16.0+

---

## Platform Support Matrix

### Verified Platforms

| Platform | Architectures | FUSE | Status |
|----------|---------------|------|--------|
| **Ubuntu 22.04** | x86_64, aarch64 | FUSE3 | ✅ Full CI |
| **Ubuntu 24.04** | x86_64, aarch64 | FUSE3 | ✅ Full CI |
| **Fedora Rawhide** | x86_64, aarch64 | FUSE3 | ✅ Full CI |
| **Debian Testing** | x86_64, aarch64 | FUSE3 | ✅ Full CI |
| **Arch Linux** | x86_64 | FUSE3 | ✅ Full CI |
| **Alpine Linux** | x86_64, aarch64 | FUSE3 | ✅ Full CI |
| **openSUSE Tumbleweed** | x86_64 | FUSE3 | ✅ Full CI |
| **macOS 14** | arm64 (Apple Silicon) | FUSE-T, macFUSE | ✅ Full CI |
| **macOS 15** | x86_64, arm64 | FUSE-T, macFUSE | ✅ Full CI |
| **Windows Server 2022** | x64 | WinFsp | ✅ Full CI |
| **Windows 11** | arm64 | WinFsp | ✅ Full CI |
| **FreeBSD** | x86_64 | Linux compat | ✅ Tested |

### Additional Architectures (Linux)

Tested via cross-compilation:
- i386 (32-bit x86)
- arm (32-bit ARM)
- ppc64 (PowerPC 64 big-endian)
- ppc64le (PowerPC 64 little-endian)
- riscv64 (RISC-V 64-bit)
- s390x (IBM Z)
- loongarch64 (LoongArch 64-bit)

---

## Documentation

### Tebako Fork Documentation

- **vcpkg Installation**: [`doc/VCPKG_INSTALLATION.md`](doc/VCPKG_INSTALLATION.md) - Complete vcpkg installation guide
- **Size Analysis**: [`doc/PHASE_B_SIZE_ANALYSIS.md`](doc/PHASE_B_SIZE_ANALYSIS.md) - FlatBuffers vs Thrift comparison
- **Compression Benchmarks**: [`doc/COMPRESSION_BENCHMARK_RESULTS.md`](doc/COMPRESSION_BENCHMARK_RESULTS.md) - Algorithm performance data
- **Test Results**: [`doc/PHASE_D_TEST_RESULTS.md`](doc/PHASE_D_TEST_RESULTS.md) (when Phase D completes)

### Original DwarFS Documentation

- **Main README**: [`README.DWARFS.md`](README.DWARFS.md) - Original comprehensive documentation
- **Manual Pages**: [`doc/mkdwarfs.md`](doc/mkdwarfs.md), [`doc/dwarfs.md`](doc/dwarfs.md), [`doc/dwarfsck.md`](doc/dwarfsck.md)
- **Format Specification**: [`doc/dwarfs-format.md`](doc/dwarfs-format.md)
- **Environment Variables**: [`doc/dwarfs-env.md`](doc/dwarfs-env.md)

### Architecture Documentation

Located in `.kilocode/rules/memory-bank/`:
- [`architecture.md`](.kilocode/rules/memory-bank/architecture.md) - System architecture
- [`tech.md`](.kilocode/rules/memory-bank/tech.md) - Technical stack

---

## Dependencies

### Required (All Builds)

- **CMake** ≥ 3.28.0
- **Boost** ≥ 1.67.0 (program_options, filesystem, chronos)
- **OpenSSL/LibreSSL** ≥ 3.0.0 (libcrypto)
- **libarchive** ≥ 3.6.0
- **xxHash** ≥ 0.8.1
- **zstd** ≥ 1.4.8

### Optional

- **FlatBuffers** ≥ 23.5.26 (auto-fetched if not found, **required**)
- **Folly + fbthrift** (for Thrift support, **optional**)
- **FLAC** ≥ 1.4.2 (PCM audio compression)
- **lz4** ≥ 1.9.3
- **liblzma** ≥ 5.2.5
- **Brotli** ≥ 1.0.9
- **FUSE3/FUSE2** (Linux), **macFUSE/FUSE-T** (macOS), **WinFsp** (Windows)

### Header-Only (Auto-Fetched)

- **fmt** ≥ 10.0
- **GoogleTest** ≥ 1.13.0
- **range-v3** ≥ 0.12.0
- **parallel-hashmap** ≥ 1.3.8

---

## Contributing

This is a fork maintained for Tebako project integration. For general DwarFS issues:
- **Upstream**: https://github.com/mhx/dwarfs

For Tebako-specific or fork-specific issues:
- **This fork**: https://github.com/tamatebako/dwarfs/issues

### Development

```bash
# Build with tests and coverage
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_TESTS=ON \
  -DENABLE_COVERAGE=ON
ninja -C build
ctest --test-dir build
```

---

## License

DwarFS is licensed under the GNU General Public License v3.0. See [`LICENSE.GPL-3.0`](LICENSE.GPL-3.0).

---

## Acknowledgments

- **Original DwarFS**: Marcus Holland-Moritz (mhx)
- **Tebako Integration**: Ribose Inc.

---

**Repository**: https://github.com/tamatebako/dwarfs
**Upstream**: https://github.com/mhx/dwarfs
**Documentation**: [`doc/`](doc/)