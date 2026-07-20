# dwarfs-t

> **dwarfs-t** is a permanent, tebako-hardened variant of [DwarFS](https://github.com/mhx/dwarfs),
> maintained by the [tamatebako](https://github.com/tamatebako) project for the
> [tebako](https://github.com/tamatebako/tebako) executable packager.
> Distinguishing features: thrift-free metadata (FlatBuffers-based multi-format
> serialization), static-build-first, macOS FUSE-T support.
> This is **not a tracking fork**: upstream fixes are cherry-picked per release.
> Releases are tagged `tebako-vX.Y.Z`.

[![CI Main](https://github.com/tamatebako/dwarfs-t/actions/workflows/ci-main.yml/badge.svg)](https://github.com/tamatebako/dwarfs-t/actions/workflows/ci-main.yml)
[![PR Validation](https://github.com/tamatebako/dwarfs-t/actions/workflows/pr-validation.yml/badge.svg)](https://github.com/tamatebako/dwarfs-t/actions/workflows/pr-validation.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE.GPL-3.0)

A fast, high-compression, read-only filesystem with excellent metadata serialization flexibility.

dwarfs-t is the tebako variant of DwarFS — see the declaration above. It
provides enhanced portability, extensive CI/CD coverage, and flexible metadata
serialization options. For the original upstream DwarFS, see
https://github.com/mhx/dwarfs.

**CI/CD**: See [.github/workflows/](.github/workflows/) for all workflows and [.github/README.md](.github/README.md) for CI/CD documentation.

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

**Performance Data**: See [`docs/reference/metadata-formats.md`](docs/reference/metadata-formats.md) for detailed performance analysis.

---

## Metadata Serialization Formats

DwarFS supports **three** metadata serialization formats with different stability levels:

### FlatBuffers (Recommended Default) ✅

**Status**: **Stable, required**

- **Size Efficiency**: 102.91% of Legacy Thrift (only +2.91% overhead)
- **Dependencies**: Header-only FlatBuffers library (auto-fetched)
- **Portability**: ⭐⭐⭐⭐⭐ Excellent - works on all platforms, supports static builds
- **Memory**: Zero-copy, memory-mappable access
- **Format**: Self-describing (includes schema)
- **Magic Bytes**: `"DFBF"` (DwarFS FlatBuffers)
- **Build Time**: Fast (header-only)
- **Priority**: 120 (highest)
- **File Extension**: `.dff` (recommended), also supports `.dwarfs` (auto-detect)

**Use for**: All new filesystem images (production-ready)

### Modern Thrift Compact ⚠️ EXPERIMENTAL

**Status**: ⚠️ **Experimental** (fbthrift - floating target, may break)

- **Size Efficiency**: 100% (baseline, smallest possible)
- **Dependencies**: Folly, fbthrift, jemalloc (all via vcpkg v2025.12.29.00+)
- **Portability**: ⭐⭐ Limited - requires complex build toolchain, static builds delicate
- **Memory**: Zero-copy, memory-mappable (CompactProtocol)
- **Format**: CompactProtocol with magic bytes `{0x82, 0x21}`
- **Stability**: ⚠️ **NOT RECOMMENDED** - fbthrift is a moving target
- **Build Time**: Slow (45-60 min first build, full stack compilation)
- **Priority**: 100 (medium-high)
- **File Extension**: `.dftx` (Modern Thrift - distinct from Legacy Thrift)

**Use for**: Minimum size requirement, environments with existing Facebook stack
**⚠️ WARNING**: NOT compatible with Homebrew dwarfs (which uses Legacy Thrift)

### Legacy Thrift (Frozen2) ✅ STABLE

**Status**: **Stable, always available** (Homebrew v0.14.1 compatible)

- **Size Efficiency**: 100% baseline (Frozen2 bit-packed format)
- **Dependencies**: None (hand-coded Frozen2 implementation)
- **Portability**: ⭐⭐⭐⭐⭐ Excellent - works on all platforms, supports static builds
- **Memory**: Sequential parsing (efficient bit-level packing)
- **Format**: Frozen2 bit-packed structures (compatible with dwarfs-rs v0.14.x)
- **Magic Bytes**: None (fallback format)
- **Build Time**: Fast (~30 seconds)
- **Priority**: 50 (fallback)
- **File Extension**: `.dwarfs` (original extension, Homebrew compatible)

**Use for**: Homebrew v0.14.1 compatibility
**Note**: Use `--format=legacy-thrift` to create Homebrew-compatible images

**Format Detection**: Legacy Thrift serves as the **fallback** format when no
magic bytes are detected, ensuring backward compatibility with v0.14.1 images
that don't have format identifiers.

### File Extension Summary

| Extension | Format | Stability | Auto-Detect | Use Case |
|-----------|--------|-----------|-------------|----------|
| `.dff` | FlatBuffers | ✅ Yes | Recommended default |
| `.dwarfs` | Legacy Thrift (fallback) | ✅ Yes (no magic) | Homebrew v0.14.1+ |
| `.dftx` | Modern Thrift | ✅ Yes ({0x82, 0x21}) | Experimental fbthrift |
| `.dft` | *Ambiguous* (Legacy or Modern) | ✅ Yes | Not recommended |

### Size Comparison

Test conducted on Perl 5.43.3 dataset (96.5 MB, 6,816 files):

| Format | Size | Relative | Priority | Stability |
|--------|------|----------|----------|-----------|
| **FlatBuffers** | 27,672,472 bytes | **101.41%** (+385 KB) | 120 | ✅ Stable (recommended) |
| **Modern Thrift** | 27,286,666 bytes | **100%** (baseline) | 100 | ⚠️ Experimental |
| **Legacy Thrift** | ~28-29 MB | **100%** | 50 | ✅ Stable (Homebrew compatible) |

**Key Highlights**:
- **Modern Thrift** (⚠️ Experimental): 100% baseline - smallest but requires full Facebook stack
- **FlatBuffers** (✅ Stable): 1.41% overhead at level 3 - negligible, excellent portability
- **Legacy Thrift** (✅ Stable): Hand-coded, Homebrew v0.14.1 compatible via `--format=legacy-thrift`

The small overhead for FlatBuffers is well worth the portability and simplicity gains.

**Documentation**: See [`docs/reference/metadata-formats.md`](docs/reference/metadata-formats.md) for detailed analysis.

### Multi-Format Architecture (v0.16.0)

DwarFS v0.16.0 implements a clean **Strategy Pattern** for metadata serialization, providing:

**Format-Agnostic Domain Model**:
- Core metadata structure independent of serialization format
- Clean separation between business logic and serialization
- Located in `metadata::domain::metadata`

**Multiple Serialization Strategies**:
- **FlatBuffers Strategy**: Modern default serializer (portable, memory-mappable)
- **Legacy Thrift Strategy**: Hand-coded Thrift Compact (backward compatible, no fbthrift)
- **Thrift Compact Strategy**: Optional modern fbthrift serializer (smallest format)
- Runtime format detection via magic bytes and fallback

**Architecture Benefits**:
- **Extensibility**: Easy to add new formats (e.g., Protocol Buffers, Cap'n Proto)
- **Testability**: Mock interfaces for unit testing
- **Maintainability**: Each format in separate, focused files
- **Flexibility**: Build with one, two, or all three formats
- **Clarity**: No mixing of format-specific code
- **Backward Compatibility**: Legacy Thrift ensures v0.14.1 compatibility

**Comprehensive Documentation**:

The multi-format architecture is thoroughly documented in [`docs/`](docs/):

- **[Documentation Hub](docs/index.adoc)** - Start here for navigation
- **[Metadata Formats Reference](docs/reference/metadata-formats.md)** - Format comparison and usage
- **[Architecture Overview](docs/reference/architecture.md)** - System architecture
- **[Building Guide](docs/guides/building.md)** - Build instructions with vcpkg
- **[Developer Guide](docs/guides/developer-guide.md)** - Development workflow

**Quick Start**:

```bash
# Read the documentation
cd docs/
# Open index.adoc in your AsciiDoc viewer

# Or start with format selection
cat docs/_guides/format-selection.adoc
```

---

## Architecture

### System Overview

DwarFS is structured as a modular C++20 project with five core libraries, multiple command-line tools, a FUSE driver, and comprehensive test infrastructure.

```
┌─────────────────────────────────────────────────────────────────┐
│                     Command-Line Tools                          │
│  mkdwarfs │ dwarfsck │ dwarfsextract │ dwarfs (FUSE driver)     │
└──────┬──────────┬─────────────┬────────────────┬────────────────┘
       │          │             │                │
       ▼          ▼             ▼                ▼
┌──────────┐ ┌──────────┐ ┌──────────┐    ┌──────────┐
│ dwarfs_  │ │ dwarfs_  │ │ dwarfs_  │    │ dwarfs_  │
│ writer   │ │ rewrite  │ │extractor │    │ reader   │
└────┬─────┘ └────┬─────┘ └────┬─────┘    └────┬─────┘
     │            │             │               │
     └────────────┴─────────────┴───────────────┤
                                                 ▼
                                          ┌──────────┐
                                          │ dwarfs_  │
                                          │ common   │
                                          └──────────┘
```

### Core Libraries

1. **dwarfs_common** ([`include/dwarfs/`](include/dwarfs/))
   - Foundation layer: compression, I/O, utilities
   - Block compressor/decompressor interfaces
   - Platform-independent file access
   - Logging and performance monitoring

2. **dwarfs_reader** ([`include/dwarfs/reader/`](include/dwarfs/reader/))
   - Read and interpret DwarFS filesystem images
   - Metadata parsing (FlatBuffers/Thrift)
   - Block cache with prefetching
   - Directory traversal, file lookup

3. **dwarfs_writer** ([`include/dwarfs/writer/`](include/dwarfs/writer/))
   - Create DwarFS filesystem images
   - Multi-threaded scanning
   - Deduplication and segmentation
   - Category-aware compression

4. **dwarfs_extractor** ([`include/dwarfs/utility/filesystem_extractor.h`](include/dwarfs/utility/filesystem_extractor.h))
   - Extract to disk or archive formats
   - Multi-threaded extraction
   - Pattern-based selective extraction

5. **dwarfs_rewrite** ([`include/dwarfs/utility/rewrite_filesystem.h`](include/dwarfs/utility/rewrite_filesystem.h))
   - Recompress or rebuild existing images
   - Change compression algorithms
   - Rebuild metadata with new options

### Metadata Serialization Architecture (v0.16.0)

DwarFS implements a clean **Strategy Pattern** for metadata serialization:

```
┌─────────────────────────────────────────────────────────┐
│       Abstract Interfaces (Format-Agnostic)             │
│                                                         │
│  metadata_builder (writer) │  metadata_provider (reader)│
│  - gather_chunks()         │  - get_chunk()            │
│  - gather_entries()        │  - get_directory()        │
│  - build() → domain        │  - get_inode()            │
└──────────────────┬────────────────────────────────────┘
                   │
         ┌─────────┴─────────┐
         │   implements      │   implements
         ▼                   ▼
┌────────────────┐    ┌────────────────┐
│ FlatBuffers    │    │ Thrift Impl    │
│ Implementation │    │  (optional)    │
│  (required)    │    │                │
│                │    │ Converts       │
│ Works directly │    │ domain ↔       │
│ with domain    │    │ thrift types   │
│ model          │    │                │
└────────────────┘    └────────────────┘
         │                   │
         └───────────────────┤
                             ▼
                  ┌──────────────────┐
                  │   Domain Model   │
                  │ metadata::domain │
                  │   ::metadata     │
                  └──────────────────┘
```

**Key Components**:

- **Domain Model** ([`include/dwarfs/metadata/domain/metadata.h`](include/dwarfs/metadata/domain/metadata.h))
  - Format-agnostic C++ structures
  - Core business logic operates on this

- **Backend Adapter** ([`src/reader/internal/backend_adapter.{h,cpp}`](src/reader/internal/backend_adapter.h))
  - Bridges domain model ↔ backend-specific types
  - Handles all three build configurations (FlatBuffers-only, Thrift-only, both)
  - Thread-local caching for Thrift conversions

- **Format Serializers**
  - FlatBuffers: [`src/metadata/serialization/flatbuffers_serializer.cpp`](src/metadata/serialization/flatbuffers_serializer.cpp)
  - Thrift: [`src/metadata/serialization/thrift_compact_serializer.cpp`](src/metadata/serialization/thrift_compact_serializer.cpp)

### Component Relationships

**Read Path (FUSE Driver)**:
```
dwarfs_main (FUSE callbacks)
    ↓
filesystem_v2 (file/directory lookup)
    ↓
metadata_v2 (inode access) → Format adapters (FlatBuffers/Thrift)
    ↓
inode_reader_v2 (chunk reading) → block_cache (LRU + prefetch)
```

**Write Path (mkdwarfs)**:
```
Scanner (multi-threaded traversal)
    ↓
Categorizer (detect file types)
    ↓
Segmenter (find duplicates)
    ↓
filesystem_writer (build blocks)
    ↓
metadata_builder (metadata finish) → Serializer (FlatBuffers/Thrift)
```

### Design Patterns

**Strategy Pattern** (Metadata Serialization):
- Abstract `metadata_writer_interface` and `metadata_provider`
- FlatBuffers and Thrift implementations
- Runtime format detection

**Adapter Pattern** (Backend Bridge):
- `backend_adapter` translates domain model to backend types
- Handles all three build configurations seamlessly

**Registry Pattern** (Extensibility):
- Compressor registry ([`compressor_registry.h`](include/dwarfs/compressor_registry.h))
- Categorizer registry ([`writer/categorizer_registry.h`](include/dwarfs/writer/categorizer_registry.h))
- Serializer registry ([`serializer_registry.h`](include/dwarfs/metadata/serialization/serializer_registry.h))

For detailed architecture documentation, see [`docs/reference/architecture.md`](docs/reference/architecture.md).

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

Check the [Releases](https://github.com/tamatebako/dwarfs-t/releases) page for pre-built binaries.

#### From Source (Recommended: Unified Build System)

The **unified build system** provides one-step commands for building and testing:

```bash
# Clone repository
git clone --recurse-submodules https://github.com/tamatebako/dwarfs-t
cd dwarfs-t

# One-time vcpkg setup
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# Run all tests (one command!)
./scripts/one-step/test-everything.sh

# Or build all configurations
./scripts/one-step/build-all.sh
```

**Benefits:**
- ✅ Automatic environment detection (vcpkg vs system packages)
- ✅ Tebako jemalloc 5.5.0 auto-verification
- ✅ Tests all configurations (FlatBuffers-only, both-formats)
- ✅ One-step process for developers and release managers

**For detailed workflows**: See [Unified Build System](#unified-build-system) section below.

#### From Source (Traditional CMake)

```bash
# Clone repository
git clone --recurse-submodules https://github.com/tamatebako/dwarfs-t
cd dwarfs-t

# Build FlatBuffers-only (recommended)
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
ninja -C build

# Install
sudo ninja -C build install
```

#### Building with vcpkg (Static Builds)

vcpkg provides fully static builds with all dependencies included.
Perfect for creating portable binaries.

```bash
# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# Build DwarFS
cd dwarfs-t
./scripts/build-all-and-test.sh --vcpkg
```

**Build Time**: First build takes 30-60 minutes (dependencies built from source).
Subsequent builds are much faster (~5 minutes).

See [`docs/guides/building.md`](docs/guides/building.md) for detailed instructions.

**Supported Platforms**:
- Linux: x64, ARM64
- macOS: x64 (Intel), ARM64 (Apple Silicon)
- Windows: x64, ARM64

##### vcpkg Triplet Selection

DwarFS supports **20 standard vcpkg triplets** across all platforms, providing flexibility for both static and dynamic linking:

**Windows (8 triplets)**:
- `x64-windows` (dynamic DLLs - default)
- `x64-windows-static` (standalone executable)
- `x64-mingw-dynamic` (MinGW DLLs)
- `x64-mingw-static` (MinGW static)
- `arm64-windows` (ARM64 DLLs)
- `arm64-windows-static` (ARM64 static)
- `arm64-mingw-dynamic` (ARM64 MinGW DLLs)
- `arm64-mingw-static` (ARM64 MinGW static)

**macOS (4 triplets)**:
- `arm64-osx` (Apple Silicon static - default)
- `arm64-osx-dynamic` (Apple Silicon shared libraries)
- `x64-osx` (Intel static)
- `x64-osx-dynamic` (Intel shared libraries)

**Linux (4 triplets)**:
- `arm64-linux` (ARM64 static - default)
- `arm64-linux-dynamic` (ARM64 shared libraries)
- `x64-linux` (x64 static - default)
- `x64-linux-dynamic` (x64 shared libraries)

**Triplet Selection Guide**:

| Use Case | Recommended Triplet | Why |
|----------|---------------------|-----|
| **Standalone executables** | `*-static` | No runtime dependencies |
| **Library distribution** | `*-dynamic` | Users link against shared libs |
| **macOS deployment** | `arm64-osx` | Native Apple Silicon |
| **Windows deployment** | `x64-windows-static` | Self-contained executable |
| **Linux containers** | `x64-linux` | Static by default |

**Example Builds**:

```bash
# macOS Apple Silicon (static)
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
ninja -C build

# Windows x64 (static executable)
cmake -B build -GNinja ^      ^
  -DCMAKE_BUILD_TYPE=Release ^      ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ^      ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^      ^
  -DDWARFS_WITH_FLATBUFFERS=ON ^      ^
  -DDWARFS_WITH_THRIFT=OFF ^      ^
  -DWITH_TESTS=ON
ninja -C build

# Linux x64 (with Modern Thrift)
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
ninja -C build
```

##### Dependency Management (v0.17.0)

**BZip2 Resolution**: DwarFS v0.17.0 includes an important fix for BZip2 dependency resolution with vcpkg:

**Problem** (pre-v0.17.0):
- boost-iostreams requires BZip2::BZip2 target during configuration
- CMake was finding Boost before BZip2 target existed
- Build failures on all vcpkg triplets

**Solution** (v0.17.0+):
- BZip2 is now found **before** Boost in CMake configuration
- Platform-agnostic solution works on all 20 triplets
- Located in [`cmake/vcpkg/bzip2.cmake`](cmake/vcpkg/bzip2.cmake)

**Impact**:
- ✅ All 20 vcpkg triplets now build successfully
- ✅ No custom triplet files needed
- ✅ Platform-agnostic dependency management

**For detailed build instructions and troubleshooting**: See [`docs/guides/building.md`](docs/guides/building.md)

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

**Complete Guide**: See [`docs/guides/building.md`](docs/guides/building.md) for:
- Detailed installation instructions
- Platform-specific notes
- CMake integration examples
- Troubleshooting tips

#### Building Tools Separately with vcpkg

DwarFS libraries can be built and installed via vcpkg, then tools built separately.
This is useful for:
- Embedding DwarFS in C++ applications
- Building custom tools using DwarFS libraries
- Static linking scenarios
- Cross-platform development

**Workflow**:

```bash
# 1. Install DwarFS libraries via vcpkg
vcpkg install dwarfs

# 2. Build tools using installed libraries
cd tools
cmake -B build -DCMAKE_PREFIX_PATH=$VCPKG_ROOT/installed/<triplet>
cmake --build build

# 3. Use the tools
./build/mkdwarfs -i /path/to/input -o filesystem.dff
./build/dwarfsck filesystem.dff
./build/dwarfsextract -i filesystem.dff -o /path/to/output
```

**Example**: See [`example/static-site-server/`](example/static-site-server/) for a complete working example of embedding DwarFS in a C++ application.

**Note**: Tools built this way have manpage support disabled (use `--help` instead of `--man`).

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

### Environment Variables

DwarFS tools support configuration via environment variables. This is useful for:

- Setting default options in shell profiles
- Configuring tools in containerized environments
- Scripting without repetitive command-line arguments

**Pattern**: `DWARFS_<TOOL>_<OPTION>=value`

**Example**:
```bash
# Set defaults for mkdwarfs
export DWARFS_MKDWARFS_COMPRESSION_LEVEL=7
export DWARFS_MKDWARFS_NUM_WORKERS=8

# Simple invocation uses environment variables
mkdwarfs -i /data -o archive.dff  # Uses level 7, 8 workers

# CLI arguments override environment variables
mkdwarfs -i /data -o archive.dff -l 9  # Uses level 9, 8 workers
```

**Common Variables**:

| Tool | Variable | Description |
|------|----------|-------------|
| **mkdwarfs** | `DWARFS_MKDWARFS_COMPRESSION_LEVEL` | Compression level (0-9) |
|  | `DWARFS_MKDWARFS_NUM_WORKERS` | Worker threads |
|  | `DWARFS_MKDWARFS_MEMORY_LIMIT` | Memory limit (e.g., `4g`) |
| **dwarfs** | `DWARFS_DWARFS_CACHE_SIZE` | Block cache size (e.g., `1g`) |
|  | `DWARFS_DWARFS_NUM_WORKERS` | Decompression workers |
| **dwarfsck** | `DWARFS_DWARFSCK_NUM_WORKERS` | Verification workers |
| **dwarfsextract** | `DWARFS_DWARFSEXTRACT_NUM_WORKERS` | Extraction workers |
| **All tools** | `DWARFS_<TOOL>_LOG_LEVEL` | Logging level |

**Complete Reference**: See [`doc/ENVIRONMENT_VARIABLES.md`](doc/ENVIRONMENT_VARIABLES.md) for all supported variables.

---

## Build Configuration

### Metadata Serialization Options

DwarFS supports **three metadata serialization formats** with flexible build configurations:

```cmake
# FlatBuffers support (modern default, recommended)
-DDWARFS_WITH_FLATBUFFERS=ON  # Default: ON

# Thrift Compact support (legacy/backward compatibility, complex dependencies)
-DDWARFS_WITH_THRIFT=ON  # Default: ON (auto-OFF in Tebako builds)

# Legacy Thrift support (ALWAYS available, no option needed)
# Hand-coded Thrift Compact implementation - always compiled in
```

**Key Points**:
- **Legacy Thrift**: ALWAYS available (no CMake option, always compiled)
- **FlatBuffers**: Optional but recommended (default: ON)
- **Thrift Compact**: Optional (default: ON, requires Folly + fbthrift)

### Valid Build Configurations

| Configuration | FlatBuffers | Legacy Thrift | Thrift Compact | Functionality | Recommendation |
|---------------|-------------|---------------|----------------|---------------|----------------|
| **FlatBuffers-only** | ✅ ON | ✅ Always | ❌ OFF | 100% - All features | ✅ **Recommended** for new deployments |
| **Thrift-only** | ❌ OFF | ✅ Always | ✅ ON | 100% - All features | ✅ Valid for legacy environments |
| **All formats** | ✅ ON | ✅ Always | ✅ ON | 99.9% - See notes | ⚠️ Development/testing only |
| **Minimal** | ❌ OFF | ✅ Always | ❌ OFF | Read-only | ⚠️ Can read but not create images |

#### FlatBuffers-Only Build (Recommended)

**Best for**: New deployments, maximum portability

```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
ninja -C build
```

**Capabilities**:
- ✅ **FlatBuffers**: Create and read `.dff` images (modern default)
- ✅ **Legacy Thrift**: Read v0.14.1 `.dft` images (always available)
- ❌ **Thrift Compact**: Cannot use fbthrift format

**Advantages**:
- ✅ Header-only dependencies (no Folly/fbthrift)
- ✅ Fast compilation
- ✅ Excellent cross-platform support
- ✅ Full sparse file support (`SEEK_DATA`/`SEEK_HOLE`)
- ✅ Backward compatible with Homebrew v0.14.1
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

**Capabilities**:
- ❌ **FlatBuffers**: Cannot create `.dff` images
- ✅ **Legacy Thrift**: Read v0.14.1 `.dft` images (always available)
- ✅ **Thrift Compact**: Create and read fbthrift `.dft` images

**Advantages**:
- ✅ Smallest metadata size (baseline 100%)
- ✅ Zero-copy memory-mapped access (Frozen2)
- ✅ Full sparse file support (`SEEK_DATA`/`SEEK_HOLE`)
- ✅ Creates `.dft` images by default
- ✅ Backward compatible with Homebrew v0.14.1

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
- ✅ **FlatBuffers**: Create and read `.dff` images
- ✅ **Legacy Thrift**: Read v0.14.1 `.dft` images (always available)
- ✅ **Thrift Compact**: Create and read fbthrift `.dft` images
- ✅ Reads all three formats seamlessly
- ✅ Auto-converts FlatBuffers→Thrift internally (lines 72-87 in `metadata_v2_factory.cpp`)
- ✅ All filesystem operations work (create, mount, check, extract)

**Limitations**:
- ⚠️ Sparse file seeking disabled (`SEEK_DATA`/`SEEK_HOLE` returns `ENOTSUP`)
  - Files treated as fully dense (single data extent)
  - Impact: Minimal for 99.9% of use cases
  - Reason: Incompatibility between formats' sparse extent representation
- ⚠️ Slower compilation (includes both Folly and FlatBuffers)
- ⚠️ Larger binary size

**Use case**: Local development when you need to test all formats, reading legacy Thrift images alongside new FlatBuffers images

### Choosing Your Build Configuration

**Quick Decision Guide**:

| If you... | Then build... | Because... |
|-----------|---------------|------------|
| **Are deploying new systems** | FlatBuffers-only | Best portability, Legacy Thrift included for v0.14.1 compat |
| **Need Homebrew v0.14.1 compatibility** | FlatBuffers-only | Legacy Thrift always available, no fbthrift needed |
| **Need maximum compression** | Thrift-only | Smallest metadata with fbthrift (but Legacy Thrift also available) |
| **Have existing Thrift infrastructure** | Thrift-only | Leverage existing Folly deployment |
| **Are developing DwarFS itself** | Both formats | Test cross-format compatibility (all 3 formats) |
| **Need to read old Homebrew images** | FlatBuffers-only | Legacy Thrift handles v0.14.1 images perfectly |
| **Are using Tebako** | FlatBuffers-only | Thrift Compact incompatible with static linking |
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

## Unified Build System

### Overview

DwarFS includes a **production-ready unified build system** designed to simplify development, testing, benchmarking, and release workflows. The system provides:

- **One-step commands** for common workflows (build, test, clean, benchmark)
- **Automatic environment detection** (vcpkg vs system packages)
- **Multi-configuration support** (FlatBuffers-only, both-formats)
- **CI/CD integration** with reusable workflows
- **Tebako jemalloc verification** (critical dependency)

**Status**: ✅ Production-ready, all tests passing

### Quick Start

```bash
# Clone and setup
git clone --recurse-submodules https://github.com/tamatebako/dwarfs-t.git
cd dwarfs-t

# One-time vcpkg setup
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# Run all tests (one command!)
./scripts/one-step/test-everything.sh
```

### One-Step Scripts

| Script | Purpose | Usage |
|--------|---------|-------|
| **test-everything.sh** | Run all tests (auto-detects vcpkg) | `./scripts/one-step/test-everything.sh` |
| **build-all.sh** | Build all configurations | `./scripts/one-step/build-all.sh` |
| **clean.sh** | Clean build artifacts | `./scripts/one-step/clean.sh --all --yes` |
| **benchmark-quick.sh** | Quick benchmark validation | `./scripts/one-step/benchmark-quick.sh` |

### For Developers

**Daily Development Workflow:**

```bash
# 1. Before starting work
git pull origin main
./scripts/one-step/clean.sh --yes

# 2. During development (work with single config)
BUILD_DIR=build-dev ./scripts/clean-build.sh -y
cd build-dev && ninja && ninja test

# 3. Before committing (quick validation)
./scripts/one-step/test-everything.sh --quick

# 4. Before pushing/PR (full validation)
./scripts/one-step/test-everything.sh
```

### For Release Managers

**Pre-Release Validation:**

```bash
# 1. Full test suite
./scripts/one-step/test-everything.sh

# 2. Run release orchestrator (dry-run)
./scripts/orchestrator/release.sh --dry-run

# 3. Verify Homebrew compatibility (if installed)
brew install dwarfs
./build-both-formats/mkdwarfs -i test/ -o test.dft --format=legacy-thrift
/homebrew/bin/dwarfs test.dft ls /
```

### Architecture

The unified build system uses a **layered architecture** with MECE organization:

```
┌─────────────────────────────────────────────────────────┐
│              Developer Interface Layer                  │
│  One-Step Scripts (test-everything.sh, build-all.sh)   │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────┴────────────────────────────────────┐
│              Orchestration Layer                        │
│  build.sh, release.sh (core build/test logic)          │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────┴────────────────────────────────────┐
│              Configuration Layer                        │
│  Build configs (flatbuffers-only, both-formats)        │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────┴────────────────────────────────────┐
│              Platform Layer                             │
│  vcpkg helper, build_env (triplet management)          │
└────────────────────┬────────────────────────────────────┘
                     │
┌────────────────────┴────────────────────────────────────┐
│              CI/CD Layer                                │
│  Reusable workflows, composite actions                  │
└─────────────────────────────────────────────────────────┘
```

**Directory Structure:**

```
scripts/
├── lib/                    # Shared libraries
│   ├── build_env.sh       # Environment detection, colors
│   └── vcpkg_helper.sh    # vcpkg utilities, Tebako jemalloc verification
├── orchestrator/          # Core build/test/release logic
│   ├── build.sh          # Core build logic
│   └── release.sh        # Release orchestration
├── one-step/              # User-facing entry points
│   ├── test-everything.sh
│   ├── build-all.sh
│   ├── clean.sh
│   └── benchmark-quick.sh
└── utils/                 # Utility scripts
    └── clean.sh
```

### CI/CD Matrix Testing

GitHub Actions tests across **multiple platforms** and **configurations**:

**Platform Matrix:**

| Platform | Runner | Architecture | Triplets |
|----------|--------|--------------|----------|
| **macOS** | macos-14 | arm64 | arm64-osx, arm64-osx-dynamic |
| **macOS** | macos-13 | x64 | x64-osx, x64-osx-dynamic |
| **Linux** | ubuntu-24.04-arm64 | arm64 | arm64-linux, arm64-linux-dynamic |
| **Linux** | ubuntu-latest | x64 | x64-linux, x64-linux-dynamic |
| **Windows** | windows-latest | x64 | x64-windows-static/dynamic |

**Configuration Matrix:**

| Configuration | Metadata Formats | File Extensions | Purpose |
|--------------|------------------|-----------------|---------|
| `flatbuffers-only` | FlatBuffers | .dff | Modern default, stable |
| `both-formats` | FlatBuffers + Thrift | .dff, .dft | All formats, max compatibility |

**Total CI Jobs:** 10+ test matrix combinations

### Critical Dependency: Tebako jemalloc

**IMPORTANT**: DwarFS requires **Tebako's fork of jemalloc 5.5.0**, NOT upstream jemalloc!

| Build Mode | jemalloc Source | How to Get It |
|------------|----------------|--------------|
| **vcpkg (Recommended)** | Tebako 5.5.0 | Auto-installed via overlay port |
| **System Packages** | ⚠️ Incompatible | Homebrew has 5.3.0 (won't work) |

**Auto-verification**: All build scripts automatically verify Tebako jemalloc 5.5.0

### vcpkg Triplet Support

DwarFS supports **12+ custom triplets** across platforms:

**macOS:**
- `arm64-osx` (static, recommended for M1/M2/M3)
- `x64-osx` (static, recommended for Intel)
- `arm64-osx-dynamic` (dynamic)
- `x64-osx-dynamic` (dynamic)

**Linux:**
- `arm64-linux` (static)
- `x64-linux` (static)
- `arm64-linux-dynamic` (dynamic)
- `x64-linux-dynamic` (dynamic)

**Windows:**
- `arm64-windows-static/dynamic`
- `x64-windows-static/dynamic`
- Plus MinGW variants

### Documentation

- **[BUILD_SYSTEM_ARCHITECTURE.md](BUILD_SYSTEM_ARCHITECTURE.md)** - Complete system architecture
- **[DEVELOPER_WORKFLOW.md](DEVELOPER_WORKFLOW.md)** - Developer & Release Manager workflows
- **[TESTING.md](TESTING.md)** - Testing guide with CI/CD matrix information
- **[scripts/README.md](scripts/README.md)** - Scripts organization guide
- **[vcpkg_triplets/README.md](vcpkg_triplets/README.md)** - Triplet documentation

### Status

| Component | Status | Notes |
|-----------|--------|-------|
| Unified Build System | ✅ Complete | Orchestrator + libs + one-step scripts |
| vcpkg Triplets | ✅ Complete | 12+ triplets across platforms |
| CI/CD Matrix | ✅ Complete | 10+ test jobs across platforms/configs |
| Tebako jemalloc | ✅ Complete | Overlay port + auto-verification |
| Documentation | ✅ Complete | DEVELOPER_WORKFLOW.md, updated TESTING.md |

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

### Documentation Hub

- **[docs/index.adoc](docs/index.adoc)** - Main documentation index (AsciiDoc)

### Developer Guides

- **[docs/guides/developer-guide.md](docs/guides/developer-guide.md)** - Developer & Release Manager workflows
- **[docs/guides/testing.md](docs/guides/testing.md)** - Comprehensive testing guide with CI/CD matrix
- **[docs/guides/release-checklist.md](docs/guides/release-checklist.md)** - Pre-release checklist
- **[docs/reference/architecture.md](docs/reference/architecture.md)** - System architecture and library overview
- **[docs/guides/building.md](docs/guides/building.md)** - Build system and vcpkg guide

### CI/CD Documentation

- **[.github/README.md](.github/README.md)** - CI/CD overview
- **[.github/MATRIX_INVENTORY.md](.github/MATRIX_INVENTORY.md)** - Build matrix configuration

### Build & Configuration

- **[scripts/README.md](scripts/README.md)** - Scripts organization guide

### Tool Documentation

- **Manual Pages**: [`doc/mkdwarfs.md`](doc/mkdwarfs.md), [`doc/dwarfs.md`](doc/dwarfs.md), [`doc/dwarfsck.md`](doc/dwarfsck.md), [`doc/dwarfsextract.md`](doc/dwarfsextract.md)
- **Format Specification**: [`docs/reference/dwarfs-format.md`](docs/reference/dwarfs-format.md)
- **Environment Variables**: [`docs/reference/dwarfs-env.md`](docs/reference/dwarfs-env.md)

### Reference Documentation

- **[docs/reference/metadata-formats.md](docs/reference/metadata-formats.md)** - Metadata format comparison
- **[docs/reference/tech.md](docs/reference/tech.md)** - Technical implementation details

---

## Testing

DwarFS includes a comprehensive test suite validating all metadata serialization formats.

### Running Tests (Recommended: Unified Build System)

**One-step testing with automatic environment detection:**

```bash
# Run all tests (auto-detects vcpkg or system packages)
./scripts/one-step/test-everything.sh

# Quick validation (skip benchmarks)
./scripts/one-step/test-everything.sh --quick

# Force vcpkg mode
./scripts/one-step/test-everything.sh --vcpkg
```

**What it does:**
1. ✅ Detects build environment (vcpkg vs system packages)
2. ✅ Verifies Tebako jemalloc 5.5.0 overlay port
3. ✅ Tests FlatBuffers-only configuration
4. ✅ Tests both-formats configuration
5. ✅ Runs all unit tests
6. ⏭️  Optionally runs benchmarks (prompts first)

### Running Tests (Traditional CMake)

**Metadata Format Tests** (all passing ✅):
- **frozen_bits_tests**: Bit-packing operations (15 tests)
- **metadata_serializer_tests**: Legacy Thrift serialization (10 tests)
- **legacy_thrift_tests**: Thrift Compact protocol (31 tests)
- **serialization_registry_tests**: Format detection and conversion (10 tests)

**Additional Test Suites**:
- **Unit tests**: Algorithm, data structure, utility tests (60+ files)
- **Integration tests**: Tool workflows, cross-tool compatibility
- **Categorizer tests**: PCM audio, FITS image detection
- **Compressor tests**: All 6 compression algorithms

### Running Tests

```bash
# Build with tests enabled
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_TESTS=ON
ninja -C build

# Run all tests
ctest --test-dir build -j

# Run metadata tests only
ctest --test-dir build --tests-regex "metadata|legacy|frozen"

# Run specific test suite
./build/serialization_registry_tests
./build/frozen_bits_tests
./build/legacy_thrift_tests
```

### Test Coverage

**Metadata Serialization**:
- ✅ FlatBuffers round-trip serialization
- ✅ Legacy Thrift round-trip serialization
- ✅ Thrift Compact round-trip serialization (if enabled)
- ✅ Cross-format conversion (Legacy ↔ FlatBuffers)
- ✅ Format auto-detection (magic bytes + fallback)
- ✅ U64 value preservation (no truncation)

**Backward Compatibility**:
- ✅ Can read Homebrew v0.14.1 images (via Legacy Thrift)
- ✅ Cross-format conversion preserves all metadata
- ✅ Format detection works with all three formats

**Documentation**: See [`test/`](test/) directory for test source code.

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
- **This fork**: https://github.com/tamatebako/dwarfs-t/issues

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

**Repository**: https://github.com/tamatebako/dwarfs-t
**Upstream**: https://github.com/mhx/dwarfs
**Documentation**: [`docs/`](docs/)


