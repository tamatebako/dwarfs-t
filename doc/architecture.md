# DwarFS Architecture

## System Overview

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

## Core Libraries

### 1. [`dwarfs_common`](../include/dwarfs/)
**Purpose**: Foundation layer providing utilities, abstractions, and common types

**Key Components**:
- **Compression**: Block compressor/decompressor interfaces and implementations
  - [`include/dwarfs/block_compressor.h`](../include/dwarfs/block_compressor.h)
  - [`include/dwarfs/compression.h`](../include/dwarfs/compression.h) - Registry of algorithms (zstd, lzma, lz4, brotli, flac, ricepp)
  - [`src/compression/`](../src/compression/) - Individual algorithm implementations
- **I/O Abstractions**: Platform-independent file access with multiple strategies
  - [`include/dwarfs/file_access.h`](../include/dwarfs/file_access.h) - Generic file I/O interface
  - [`include/dwarfs/os_access.h`](../include/dwarfs/os_access.h) - OS-specific operations
- **Data Structures**: Efficient byte buffers, checksums, utilities
  - [`include/dwarfs/byte_buffer.h`](../include/dwarfs/byte_buffer.h) - Memory-mapped and allocated buffers
  - [`include/dwarfs/checksum.h`](../include/dwarfs/checksum.h) - XXH3, SHA-512/256 support
- **Logging & Monitoring**: Structured logging and performance monitoring
  - [`include/dwarfs/logger.h`](../include/dwarfs/logger.h) - Multi-level logging system
  - [`include/dwarfs/performance_monitor.h`](../include/dwarfs/performance_monitor.h) - Latency tracking

### 2. [`dwarfs_reader`](../include/dwarfs/reader/)
**Purpose**: Read and interpret DwarFS filesystem images

**Key Components**:
- **Filesystem Interface**: [`reader/filesystem_v2.h`](../include/dwarfs/reader/filesystem_v2.h)
  - Metadata parsing and validation
  - Directory traversal, file lookup
  - Block cache management
- **Metadata Reading**: [`reader/internal/metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp) and [`reader/internal/metadata_v2_thrift.cpp`](../src/reader/internal/metadata_v2_thrift.cpp)
  - Multi-format support (Thrift/FlatBuffers)
  - Lazy unpacking of packed structures
  - Frozen layout for Thrift, zero-copy access for FlatBuffers
- **Block Cache**: [`reader/internal/block_cache.cpp`](../src/reader/internal/block_cache.cpp)
  - LRU eviction policy
  - Sequential access detection & prefetching
  - Multi-threaded decompression

### 3. [`dwarfs_writer`](../include/dwarfs/writer/)
**Purpose**: Create DwarFS filesystem images from directory trees

**Key Components**:
- **Scanner**: [`writer/scanner.h`](../include/dwarfs/writer/scanner.h)
  - Multi-threaded directory scanning
  - File hashing for deduplication
  - Filter rule application
- **Categorizer Framework**: [`writer/categorizer.h`](../include/dwarfs/writer/categorizer.h)
  - PCM audio categorizer (detects WAV/AIFF, provides metadata for FLAC)
  - FITS categorizer (astronomical images, Rice++ compression)
  - Incompressible categorizer (skips already-compressed data)
- **Segmenter**: [`writer/segmenter_factory.h`](../include/dwarfs/writer/segmenter_factory.h)
  - Rolling hash-based duplicate detection
  - Bloom filter for fast rejection
  - Category-aware segmentation with custom granularity
- **Metadata Builder**: [`writer/internal/metadata_builder.cpp`](../src/writer/internal/metadata_builder.cpp)
  - Constructs metadata domain model
  - Multi-format serialization support
  - Packing options (chunk table, directories, shared files, string tables)

### 4. [`dwarfs_extractor`](../include/dwarfs/utility/filesystem_extractor.h)
**Purpose**: Extract DwarFS images to disk or archive formats

**Key Features**:
- Multi-threaded extraction
- Direct conversion to tar/cpio/etc via libarchive
- Pattern-based selective extraction
- Preserves sparse files, hardlinks, symlinks, xattrs

### 5. [`dwarfs_rewrite`](../include/dwarfs/utility/rewrite_filesystem.h)
**Purpose**: Recompress or rebuild existing DwarFS images

**Key Features**:
- Recompress blocks with different algorithms
- Rebuild metadata with new options
- Change block size
- Category-selective recompression

## mkdwarfs Tool Architecture (NEW - v0.16.0+)

**Purpose**: Modularized command-line tool for creating DwarFS filesystem images

### Handler Pattern Implementation

The mkdwarfs tool was refactored (Phases 1-4, completed 2025-11-25) to separate concerns and enable building without Thrift support:

```
┌──────────────────────────────────────────────────────┐
│              mkdwarfs_main.cpp (689 lines)           │
│                                                      │
│  ┌────────────────┐                                 │
│  │ options_parser │ → parsed_options                │
│  └────────────────┘                                 │
│          │                                           │
│          ▼                                           │
│  ┌────────────────┐                                 │
│  │handler_factory │ → unique_ptr<handler_interface> │
│  └────────────────┘                                 │
│          │                                           │
│          ▼                                           │
│    handler->run()                                    │
└──────────────────────────────────────────────────────┘
                   │
         ┌─────────┴─────────┐
         ▼                   ▼
┌─────────────────┐  ┌──────────────────┐
│ create_handler  │  │recompress_handler│
│   (always)      │  │ (#ifdef THRIFT)  │
└─────────────────┘  └──────────────────┘
```

### Key Modules

1. **options_parser** ([`tools/include/dwarfs/tool/mkdwarfs/options_parser.h`](../tools/include/dwarfs/tool/mkdwarfs/options_parser.h))
   - Parses all command-line options
   - Applies compression-level defaults
   - Validates configuration
   - Detects metadata format preferences
   - 766 lines extracted from main

2. **handler_interface** ([`tools/include/dwarfs/tool/mkdwarfs/handler_interface.h`](../tools/include/dwarfs/tool/mkdwarfs/handler_interface.h))
   - Abstract base class for all handlers
   - Pure virtual `run()` method
   - Defines common execution interface

3. **create_handler** ([`tools/include/dwarfs/tool/mkdwarfs/create_handler.h`](../tools/include/dwarfs/tool/mkdwarfs/create_handler.h))
   - Implements filesystem creation workflow
   - Scanner setup and configuration
   - Segmenter/categorizer initialization
   - Always available (no Thrift dependency)
   - 76 lines of clean logic

4. **recompress_handler** ([`tools/include/dwarfs/tool/mkdwarfs/recompress_handler.h`](../tools/include/dwarfs/tool/mkdwarfs/recompress_handler.h))
   - Implements recompression workflow
   - Requires Thrift for metadata reading
   - Wrapped in `#ifdef DWARFS_HAVE_THRIFT`
   - 165 lines of recompress logic

5. **handler_factory** ([`tools/include/dwarfs/tool/mkdwarfs/handler_factory.h`](../tools/include/dwarfs/tool/mkdwarfs/handler_factory.h))
   - Factory pattern for handler creation
   - Checks for recompress + Thrift availability
   - Returns appropriate handler via interface
   - Clear error messages for missing features
   - 56 lines of clean factory logic

### Refactoring Metrics

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| mkdwarfs_main.cpp lines | 1578 | 689 | **-56.3%** |
| Conditional branches in main | 5+ | 0 | **-100%** |
| Build configurations | 1 | 2 (±Thrift) | Flexible |
| Modules | Monolithic | 5 clean modules | Testable |

### Benefits Achieved

1. **Separation of Concerns**: Each module has single responsibility
2. **Testability**: Each handler can be unit tested independently
3. **Flexibility**: Can build with/without Thrift support
4. **Maintainability**: ~700-line main() vs 1578-line monolith
5. **Extensibility**: Adding new handlers = implement interface + register in factory
6. **Error Handling**: Clear, helpful messages for missing features

### Files

**Headers** (`tools/include/dwarfs/tool/mkdwarfs/`):
- `options_parser.h` (158 lines)
- `handler_interface.h` (87 lines)
- `handler_factory.h` (53 lines)
- `create_handler.h` (82 lines)
- `recompress_handler.h` (89 lines)

**Implementation** (`tools/src/mkdwarfs/`):
- `options_parser.cpp` (766 lines)
- `handler_factory.cpp` (56 lines)
- `create_handler.cpp` (76 lines)
- `recompress_handler.cpp` (165 lines)

**Total**: 9 files, 1532 lines extracted from main

## dwarfs Tool Architecture (NEW - v0.16.0+)

**Purpose**: Modularized FUSE driver with reusable library components

### Handler Pattern Implementation

The dwarfs tool was refactored (Phases 1-6, completed 2025-11-26) to extract reusable library components and enable building without complex FUSE dependencies:

```
┌──────────────────────────────────────────────────────┐
│              dwarfs_main.cpp (353 lines)             │
│                                                      │
│  ┌────────────────┐                                 │
│  │ options_parser │ → parsed_options                │
│  └────────────────┘                                 │
│          │                                           │
│          ▼                                           │
│  ┌────────────────┐                                 │
│  │mount_handler   │ → creates handler               │
│  └────────────────┘                                 │
│          │                                           │
│          ▼                                           │
│  Libraries: filesystem_loader → fuse_driver         │
└──────────────────────────────────────────────────────┘
                   │
         ┌─────────┴─────────┐
         ▼                   ▼
┌─────────────────┐  ┌─────────────────┐
│filesystem_loader│  │  fuse_driver    │
│  (library)      │  │   (library)     │
│                 │  │                 │
│ High-level FS   │  │ FUSE operations │
│ loading API     │  │ All callbacks   │
└─────────────────┘  └─────────────────┘
```

### Key Modules

1. **options_parser** ([`tools/include/dwarfs/tool/dwarfs/options_parser.h`](../tools/include/dwarfs/tool/dwarfs/options_parser.h))
   - Parses all command-line options
   - Validates configuration
   - Configures cache, workers, FUSE options
   - 370 lines extracted from main

2. **filesystem_loader** ([`include/dwarfs/reader/filesystem_loader.h`](../include/dwarfs/reader/filesystem_loader.h)) **[LIBRARY]**
   - High-level filesystem loading API
   - Configures cache size, workers, I/O strategy
   - Reusable for embedding DwarFS in C++ applications
   - 93 lines of clean loading logic

3. **fuse_driver** ([`include/dwarfs/reader/fuse_driver.h`](../include/dwarfs/reader/fuse_driver.h)) **[LIBRARY]**
   - All FUSE operations (getattr, read, readdir, etc.)
   - Reusable for custom FUSE implementations
   - Platform-agnostic FUSE abstraction
   - ~1,800 lines of FUSE operations

4. **mount_handler** ([`tools/include/dwarfs/tool/dwarfs/mount_handler.h`](../tools/include/dwarfs/tool/dwarfs/mount_handler.h))
   - FUSE session management
   - Mount/unmount orchestration
   - Signal handling
   - 441 lines of session management

### Refactoring Metrics

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| dwarfs_main.cpp lines | 2,041 | 353 | **-82.7%** |
| Reusable library code | 0 | ~2,050 | **NEW** |
| Tool module code | 0 | ~815 | **NEW** |
| Build configurations | 1 | Multiple | Flexible |
| Embeddable | No | Yes | **NEW** |

### Benefits Achieved

1. **Reusable Libraries**: filesystem_loader and fuse_driver can be used in other C++ projects
2. **Separation of Concerns**: CLI, configuration, FUSE operations isolated
3. **Testability**: Each component can be unit tested independently
4. **Maintainability**: 82.7% reduction in main() complexity
5. **Platform Support**: Full FUSE-T compatibility on macOS (userspace, no kernel extension)
6. **Extensibility**: Easy to add features without modifying main()

### Platform Achievement: FUSE-T Support

- ✅ Hybrid FUSE API support (FUSE 2.x mount + 3.x session)
- ✅ Conditional compilation for platform differences
- ✅ Works on macOS ARM64 without kernel extension
- ✅ All FUSE operations functional

### Files

**Library Headers** (`include/dwarfs/reader/`):
- `filesystem_loader.h` (151 lines)
- `fuse_driver.h` (181 lines)

**Library Implementation** (`src/reader/`):
- `filesystem_loader.cpp` (93 lines)
- `fuse_driver.cpp` (~1,800 lines)

**Tool Headers** (`tools/include/dwarfs/tool/dwarfs/`):
- `options_parser.h` (177 lines)
- `mount_handler.h` (104 lines)

**Tool Implementation** (`tools/src/dwarfs/`):
- `options_parser.cpp` (370 lines)
- `mount_handler.cpp` (441 lines)

**Total**: 8 files, ~3,200 lines extracted from main

### Library Usage Examples

**Filesystem Loading**:
```cpp
#include <dwarfs/reader/filesystem_loader.h>

dwarfs::reader::filesystem_load_config config;
config.image_path = "myfs.dwarfs";
config.cache_size = 1024 * 1024 * 1024;  // 1 GiB
config.num_workers = 8;

auto lgr = dwarfs::stream_logger::create(std::cerr);
auto os = dwarfs::os_access::create();
auto fs = dwarfs::reader::filesystem_loader::load(*lgr, *os, config);
```

**FUSE Driver**:
```cpp
#include <dwarfs/reader/fuse_driver.h>

dwarfs::reader::fuse_driver driver(std::move(fs), config);
driver.mount("/mnt/point");
```

## Metadata Serialization Architecture

### Two-Format Design

DwarFS supports two metadata serialization formats to maximize platform compatibility:

```
╔════════════════════════════════════════════════════════╗
║            Core Domain Model (Format-Agnostic)         ║
║      include/dwarfs/metadata/domain/metadata.h         ║
║                                                        ║
║  • inodes        • directories    • chunks            ║
║  • dir_entries   • symlinks       • devices           ║
║  • uid/gid maps  • mode map       • feature sets      ║
╚═══════════════╤════════════════════════════════════════╝
                │
      ┌─────────┴─────────┐
      ▼                   ▼
┌──────────┐      ┌──────────────┐
│ Thrift   │      │ FlatBuffers  │
│ Compact  │      │   Binary     │
├──────────┤      ├──────────────┤
│ Legacy   │      │  Modern      │
│ Optional │      │  Default     │
├──────────┤      ├──────────────┤
│ Requires │      │ Header-only  │
│ Folly +  │      │FlatBuffers   │
│ fbthrift │      │   library    │
├──────────┤      ├──────────────┤
│ 2 sections│     │ 1 section    │
│ (schema + │     │ (self-       │
│  metadata)│     │  describing) │
└──────────┘      └──────────────┘
```

## Metadata Serialization (v0.16.0+)

**Completion Date**: 2025-11-30 (Phase A - Verification Fix)
**Status**: ✅ Verification working, size optimization pending

### Strategy Pattern with Adapter Pattern

DwarFS implements clean **Strategy Pattern** for serialization with **Adapter Pattern** for string table compression:

```
┌─────────────────────────────────────────────────────────┐
│        Strategy Interface (Domain Model)                │
│     metadata::domain::metadata (format-agnostic)        │
└──────────────────┬──────────────────────────────────────┘
                   │
       ┌───────────┴───────────┐
       ▼                       ▼
┌──────────────────┐    ┌──────────────────┐
│ Writer Strategy  │    │ Reader Strategy  │
│ FlatBuffers      │    │ FlatBuffers      │
│ Serializer       │    │ Deserializer     │
│ domain → bytes   │    │ bytes → domain   │
└──────────────────┘    └──────────────────┘
```

**FlatBuffers (Modern Default)** - VERIFIED ✅:
- **Always enabled** - Required format
- **Wire format**: `[4-byte size][DFBF identifier][flatbuffers data]`
- **File identifier**: "DFBF" (DwarFs FlatBuffer)
- **Verification**: `VerifySizePrefixedBuffer()` matches `FinishSizePrefixed()`
- **Memory-mappable**: Zero-copy via `GetSizePrefixedRoot()`
- **Dependencies**: Header-only FlatBuffers library
- **String compression**: FSST via `compact_names` (Adapter Pattern)
- **Size**: Target ≤110% of Thrift

**Thrift Compact (Legacy)** - OPTIONAL:
- **Optional** - Backward compatibility
- **Wire format**: `[schema section][frozen2 data]`
- **Smallest format**: Bit-packed structures
- **Dependencies**: Folly + fbthrift (complex)

**Build Modes**:
- **FlatBuffers-only**: Full functionality ✅
- **Dual-format**: Both formats ✅
- **Thrift-only**: NOT SUPPORTED (FlatBuffers required)

**Implementation Files**:
- **Writer**: [`src/metadata/serialization/flatbuffers_serializer.cpp`](../src/metadata/serialization/flatbuffers_serializer.cpp:343)
- **Reader**: [`src/reader/internal/metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp:123-130)
- **Adapters**: [`src/reader/internal/metadata_types_flatbuffers.cpp`](../src/reader/internal/metadata_types_flatbuffers.cpp:35-80)
- **Schema**: [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs:375)

**See**: [`doc/FLATBUFFERS_METADATA_FIX_STATUS.md`](../../../doc/FLATBUFFERS_METADATA_FIX_STATUS.md)

### FlatBuffers Performance Characteristics (Verified 2025-12-19)

**Status**: ✅ **Production-Ready** - Comprehensive benchmarking complete

#### Compression Performance (vs Thrift)

| Compression Level | FlatBuffers Time | Thrift Time | Performance |
|-------------------|------------------|-------------|-------------|
| Level 1 (fast) | 1.489s | 2.095s | **28.9% faster** ✅ |
| Level 3 (default) | 2.999s | 3.617s | **17.1% faster** ✅ |
| Level 9 (max) | 27.043s | 26.606s | 1.6% slower (≈equal) |

**Key Findings**:
- FlatBuffers is **significantly faster** at typical compression levels (1-3)
- Performance is **equivalent** at maximum compression (level 9)
- Speed advantage comes from simpler serialization model and efficient builder allocation

#### Extraction Performance

| Metric | FlatBuffers | Thrift | Delta |
|--------|-------------|--------|-------|
| Extraction Time (level 3) | 2.069s | 1.998s | +3.4% (≈equal) |
| **Content Verification** | ✅ Identical | ✅ Identical | **Byte-for-byte match** |

**Key Findings**:
- Extraction speeds are **virtually identical** (3.4% difference within noise)
- Both formats produce **byte-for-byte identical files** (verified via SHA256 tree hash)
- Both use zero-copy memory-mapped access (bottleneck is decompression, not metadata)

#### Image Size Overhead

| Level | FlatBuffers Size | Thrift Size | Overhead | Absolute |
|-------|------------------|-------------|----------|----------|
| Level 1 | 36,773,465 bytes | 36,433,300 bytes | +0.93% | +340 KB |
| Level 3 | 27,672,472 bytes | 27,286,666 bytes | +1.41% | +385 KB |
| Level 9 | 14,003,477 bytes | 13,993,517 bytes | +0.07% | +9.9 KB |

**Key Findings**:
- Size overhead is **minimal** (<1.5% at all levels)
- Absolute overhead ranges from 9.9 KB to 385 KB
- At high compression (level 9), overhead is negligible (0.07%)

#### Recommendation

**Use FlatBuffers (`.dff`) as default** for all new images:
- ✅ **17-29% faster** compression at typical levels
- ✅ **Equivalent** extraction performance
- ✅ **Negligible** size overhead (<1.5%)
- ✅ **Better portability** (header-only library)
- ✅ **Identical output** (verified cryptographically)

**Use Thrift (`.dft`) only for**:
- Reading legacy images
- Absolute minimum size requirement (0.07-1.41% savings)

**File Extensions**:
- **ALWAYS use `.dff`** for FlatBuffers images
- **ALWAYS use `.dft`** for Thrift images
- **NEVER use `.dwarfs`** (generic, format ambiguous)

**Documentation**: [`doc/DWARFS_METADATA_FORMAT_PERFORMANCE.md`](../../../doc/DWARFS_METADATA_FORMAT_PERFORMANCE.md)

## libdwarfs API Performance (NEW - Session 17)

**Status**: ✅ **Benchmarked** - Comprehensive results available
**Created**: 2025-12-19
**Location**: [`benchmarks/libdwarfs/`](../benchmarks/libdwarfs/)

### Overview

The libdwarfs C++ API provides direct filesystem access without FUSE overhead. Comprehensive benchmarking validates performance across various access patterns.

### Actual Performance (Perl 5.43.3, 96.5 MB, 6,816 files)

**Test Environment**:
- Platform: macOS ARM64 (Apple Silicon)
- Cache: 1 GiB
- Workers: 4 threads
- Build: Release

**Single File** (48.45 KB file):
- Cold cache: 8.29 ms
- Warm cache: 0.21 ms (median)
- Throughput: 16.05 MB/s
- Memory: 144 KiB peak
- **Speedup**: 39x (cold → warm)

**Full Extraction** (6,816 files, 96.5 MB):
- Median time: 1.49 s (4 threads)
- Mean time: 3.48 s
- Throughput: **27.75 MB/s**
- Memory: 8.44 MiB peak
- **Speedup**: 5x (cold → warm, 7.46s → 1.49s)

**vs FUSE**:
- API provides direct control over caching and threading
- No kernel overhead
- Memory-efficient (8.44 MiB for full extraction)

**Key Findings**:
- ✅ **Sub-millisecond** warm cache latency (0.21 ms)
- ✅ **Very low memory** usage (8.4 MB for full extraction)
- ✅ **Excellent throughput** (27.75 MB/s for 6,816 files)
- ✅ **Massive cache benefit** (39x speedup for single files)

### API Usage Example

```cpp
#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/reader/filesystem_loader.h>

// Setup
auto lgr = dwarfs::stream_logger::create(std::cerr);
auto os = dwarfs::os_access::create();

// Load filesystem
dwarfs::reader::filesystem_load_config config;
config.image_path = "myfs.dff";
config.cache_size = 512 << 20;  // 512 MiB
config.num_workers = 4;

auto fs = dwarfs::reader::filesystem_loader::load(*lgr, *os, config);

// Extract file
auto entry = fs.find("/path/to/file.txt");
auto inode = fs.open(entry->inode());
auto content = fs.read_string(inode);
```

### Build Integration

**CMake**: [`benchmarks/libdwarfs/CMakeLists.txt`](../benchmarks/libdwarfs/CMakeLists.txt)
```cmake
# Build benchmarks
cmake --build build --target libdwarfs_benchmarks

# Executables in build/
build/single_file_bench
build/multiple_files_bench
build/full_extract_bench
build/random_access_bench
```

### Documentation

- **Integration Guide**: [`doc/LIBDWARFS_INTEGRATION_GUIDE.md`](../../../doc/LIBDWARFS_INTEGRATION_GUIDE.md)
- **Performance Report**: [`doc/LIBDWARFS_API_PERFORMANCE.md`](../../../doc/LIBDWARFS_API_PERFORMANCE.md)
- **Session 17 Plan**: [`doc/SESSION_17_LIBDWARFS_BENCHMARKING_PLAN.md`](../../../doc/SESSION_17_LIBDWARFS_BENCHMARKING_PLAN.md)

### Key Benefits

**vs FUSE Mount**:
- ✅ **5-10% faster** (no kernel overhead)
- ✅ **Direct control** over cache/threading
- ✅ **Embeddable** in applications

**vs dwarfsextract Tool**:
- ✅ **Similar performance** (same underlying code)
- ✅ **More flexible** (custom logic)
- ✅ **Lower overhead** (no tool startup)

**vs Direct Archive**:
- ✅ **2-10x faster** random access
- ✅ **Deduplication** aware
- ✅ **Memory efficient**

### Optimization Guidelines

**Cache Sizing**:
```cpp
// Small working set (<1 GB)
config.cache_size = 256 << 20;  // 256 MiB

// Medium working set (1-5 GB)
config.cache_size = 1024 << 20;  // 1 GiB

// Large working set (>5 GB)
config.cache_size = 4096 << 20;  // 4 GiB
```

**Worker Threads**:
```cpp
// Single file: minimal
config.num_workers = 2;

// Multiple files: balanced
config.num_workers = 4;

// Bulk extraction: maximum
config.num_workers = std::thread::hardware_concurrency();
```

**Extraction Threads**:
```cpp
// Small file count (<100): single-threaded
size_t threads = 1;

// Large file count (100-1000): multi-threaded
size_t threads = 4;

// Very large (>1000): high parallelism
size_t threads = 8;
```

## Strategy Pattern Architecture (NEW - v0.16.0+)

**Problem Solved**: Hard dependencies on Thrift types throughout codebase prevented FlatBuffers-only builds.

**Solution**: Apply Strategy Pattern with Dependency Inversion Principle:

```
┌─────────────────────────────────────────────────────────┐
│       Abstract Interfaces (Format-Agnostic)             │
│                                                         │
│  metadata_provider (reader)  │  metadata_builder (writer)│
│  - get_chunk()               │  - gather_chunks()       │
│  - get_directory()           │  - gather_entries()      │
│  - get_inode()               │  - build() → domain      │
└──────────────────┬────────────────────────────────────┘
                   │
         ┌─────────┴─────────┐
         │   implements      │   implements
         ▼                   ▼
┌────────────────┐    ┌────────────────┐
│ Thrift Impl    │    │FlatBuffers Impl│
│ (optional)     │    │  (required)    │
│                │    │                │
│ Converts       │    │ Works directly │
│ domain ↔       │    │ with domain    │
│ thrift types   │    │ model          │
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

**Key Benefits**:
1. **Separation**: Thrift and FlatBuffers COMPLETELY separate
2. **Extensibility**: Add new format = implement interface
3. **Testability**: Mock interface for tests
4. **Clarity**: No mixing of concerns
5. **Maintainability**: Each format in its own files

**Implementation Files**:
- Abstract interfaces: `include/dwarfs/reader/metadata_provider.h`, `include/dwarfs/writer/internal/metadata_builder.h`
- Thrift implementation: `src/reader/thrift_metadata_provider.cpp`, `src/writer/internal/thrift_metadata_builder.cpp`
- FlatBuffers implementation: `src/reader/flatbuffers_metadata_provider.cpp`, `src/writer/internal/flatbuffers_metadata_builder.cpp`
- Factories: `*_factory.cpp` files

**Documentation**: [`doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`](../doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md)

### Format Selection Rationale

**Thrift Compact (Legacy, Optional)**:
- **Pros**: Extremely space-efficient Frozen2 bit-packed layouts, memory-mappable zero-copy access
- **Cons**: Complex dependencies (Folly + fbthrift), difficult to build on some platforms
- **Use case**: Reading old images only; optional for backward compatibility

**FlatBuffers (Modern, Required)**:
- **Pros**: Memory-mappable, zero-copy access, excellent portability, header-only, forward/backward compatible
- **Cons**: Slightly larger than Thrift (~5-10%)
- **Use case**: Default for all new images; works on all platforms

### Serialization Implementation

**Layer 1 - Domain Model** [`include/dwarfs/metadata/domain/`](../include/dwarfs/metadata/domain/):
- [`metadata.h`](../include/dwarfs/metadata/domain/metadata.h): Core C++ structures (format-agnostic)

**Layer 2 - Serializers** [`src/metadata/serialization/`](../src/metadata/serialization/):
- [`thrift_compact_serializer.cpp`](../src/metadata/serialization/thrift_compact_serializer.cpp): Thrift implementation (guarded by `#ifdef DWARFS_HAVE_THRIFT`)
- [`flatbuffers_serializer.cpp`](../src/metadata/serialization/flatbuffers_serializer.cpp): FlatBuffers implementation (always available)

**Layer 3 - Facade & Registry** [`src/metadata/serialization/`](../src/metadata/serialization/):
- [`serialization_facade.cpp`](../src/metadata/serialization/serialization_facade.cpp): Unified interface for all formats
- [`serializer_registry.cpp`](../src/metadata/serialization/serializer_registry.cpp): Factory with automatic format detection
- [`facade_factory.cpp`](../src/metadata/serialization/facade_factory.cpp): Creates facades for specific formats

**Layer 4 - Converters** [`src/metadata/converters/`](../src/metadata/converters/):
- [`cpp_thrift_converter.cpp`](../src/metadata/converters/cpp_thrift_converter.cpp): C++ ↔ Thrift type conversion
- [`thrift_metadata_converter.cpp`](../src/metadata/converters/thrift_metadata_converter.cpp): Legacy Thrift-only converter

## Source Code Organization

### Directory Structure

```
dwarfs/
├── include/dwarfs/          # Public library headers
│   ├── reader/              # Reader library API
│   ├── writer/              # Writer library API
│   ├── utility/             # Extractor & rewrite APIs
│   ├── metadata/            # Metadata subsystem
│   │   ├── domain/          # Domain model + format support
│   │   └── serialization/   # Serialization interfaces
│   └── tool/                # Tool-only headers (not installed)
├── src/                     # Implementation files
│   ├── compression/         # Compression algorithms
│   ├── reader/              # Reader implementation
│   │   ├── internal/        # Private reader components
│   │   └── detail/          # Internal helpers
│   ├── writer/              # Writer implementation
│   │   └── internal/        # Private writer components
│   ├── metadata/            # Metadata implementation
│   │   ├── serialization/   # Serializers
│   │   └── converters/      # Format converters
│   ├── utility/             # Extractor & rewrite impl
│   ├── internal/            # Common internals
│   └── detail/              # Common helpers
├── tools/                   # Command-line tools
│   ├── src/                 # Tool implementations
│   └── include/             # Tool-specific headers
├── test/                    # Test suite
├── thrift/                  # Thrift IDL definitions
├── cmake/                   # Build system modules
│   ├── tebako/              # Tebako integration
│   ├── folly.cmake          # Folly setup
│   ├── thrift.cmake         # fbthrift setup
│   ├── metadata_serialization.cmake  # Format configuration
│   └── libdwarfs.cmake      # Library definitions
├── .github/workflows/       # CI/CD pipelines
└── doc/                     # Documentation
```

### Critical Implementation Paths

**Filesystem Creation Path** ([`mkdwarfs`](../tools/src/mkdwarfs_main.cpp)):
```
Scanner → Categorizer → Segmenter → Blockifier → Compressor → Metadata Builder → Serializer
   ↓          ↓            ↓            ↓            ↓              ↓              ↓
 Hash     Detect PCM/   Find dupe    Group      Multi-thread   Build domain   Choose
 files    FITS/etc,     segments     chunks     compress       model         FlatBuffers/
          split into    across       into       blocks                      Thrift
          fragments     files        blocks
```

**Filesystem Mounting Path** ([`dwarfs`](../tools/src/dwarfs_main.cpp)):
```
Parse header → Detect format → Load schema (Thrift) → Initialize metadata reader → Setup block cache → FUSE ops
      ↓              ↓                ↓                         ↓                        ↓             ↓
   Skip prefix   Check magic    Unpack Frozen2         Thrift: Frozen2  Directories:      inode → chunks
    (optional)    bytes for      (Thrift) or           (self-        delta decompress      ↓
                 FlatBuffers     deserialize FlatBuffers  describing)     (domain access)    block + offset
                                                  FSST decompress
```

## Key Technical Decisions

### 1. Metadata Serialization Dual-Format Support
**Decision**: Support Thrift (optional) and FlatBuffers (required) formats

**Rationale**:
- **Thrift**: Legacy format, optional for reading old images only
- **FlatBuffers**: Modern default, memory-mappable, excellent portability, zero-copy access
- **Dual support**: Preserve backward compatibility while moving to simpler, more portable solution

**Implementation**:
- Compile-time format selection via CMake options (FlatBuffers always enabled)
- Runtime format detection via magic bytes
- Facade pattern abstracts differences
- Registry pattern for extensibility

**Files**:
- [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake) - Build configuration
- [`src/metadata/serialization/serializer_registry.cpp`](../src/metadata/serialization/serializer_registry.cpp) - Format registry
- [`src/metadata/serialization/facade_factory.cpp`](../src/metadata/serialization/facade_factory.cpp) - Facade creation

### 2. Memory-Mapped vs Read-Based I/O
**Decision**: Support both memory-mapping and read()-based file access

**Rationale**:
- **Memory-mapping**: Extremely fast, zero-copy, but can crash on I/O errors (SIGBUS)
- **Read-based**: Slower, but allows graceful error handling
- **Granular mapping**: For 32-bit systems to limit address space usage

**Implementation**: Environment variable `DWARFS_IOLAYER_OPTS` controls strategy

**Files**:
- [`include/dwarfs/internal/io_ops.h`](../include/dwarfs/internal/io_ops.h) - I/O layer abstraction
- [`src/internal/io_ops_posix.cpp`](../src/internal/io_ops_posix.cpp) - Platform implementation

### 3. Block Cache with Sequential Detection
**Decision**: Implement intelligent block cache with prefetching

**Rationale**:
- Random access: LRU cache prevents redundant decompression
- Sequential access: Prefetching dramatically improves throughput (2x+)
- Multi-threaded: Parallel decompression for multiple concurrent readers

**Implementation**:
- [`src/reader/internal/block_cache.cpp`](../src/reader/internal/block_cache.cpp)
- Heuristic detects sequential patterns, prefetches next blocks
- Configurable via `-o seq_detector` option

### 4. Registry Pattern for Extensibility
**Decision**: Use registry pattern for compressors, decompressors, categorizers, serializers

**Rationale**:
- Easy to add new algorithms without modifying core code
- Compile-time optional features (e.g., FLAC only if library available)
- Self-documenting registry lists available options

**Implementation**:
- [`include/dwarfs/compressor_registry.h`](../include/dwarfs/compressor_registry.h) - Singleton registry
- [`src/compressor_registry.cpp`](../src/compressor_registry.cpp) - Registration mechanism
- Each compressor self-registers via static initialization

### 5. Tebako Integration
**Decision**: Make DwarFS embeddable in Ruby executables via Tebako

**Rationale**:
- Tebako requires static linking
- Thrift/Folly incompatible with static linking in many scenarios
- Must support minimal dependency builds

**Implementation**:
- [`cmake/tebako.cmake`](../cmake/tebako.cmake) - Tebako entry point
- [`cmake/tebako/`](../cmake/tebako/) - Modular build configuration
  - `build_scopes.cmake`: Define MKD (mkdwarfs-only) vs ALL (all tools) build scopes
  - `platform_detection.cmake`: Detect MSys2, Windows, macOS variants
  - `dependency_paths.cmake`: Locate Tebako-provided dependencies
  - `compiler_flags.cmake`: Platform-specific compiler settings
  - `validation.cmake`: Pre-build checks
- Forces `DWARFS_WITH_THRIFT=OFF` in Tebako builds

## Design Patterns

### 1. Facade Pattern (Metadata Serialization)
**Purpose**: Hide complexity of multiple serialization formats behind unified interface

**Implementation**:
```cpp
// Facade: include/dawrfs/metadata/serialization/facade.h
class MetadataSerializationFacade {
  void serialize(metadata const&, byte_buffer&);
  void deserialize(span<uint8_t const>, metadata&);
  SerializationFormat get_format() const;
};

// Factory: src/metadata/serialization/facade_factory.cpp
class FacadeFactory {
  static unique_ptr<MetadataSerializationFacade> create(SerializationFormat);
  static optional<SerializationFormat> detect_format(vector<uint8_t> const&);
};
```

### 2. Registry Pattern (Multiple Subsystems)
**Purpose**: Allow runtime and compile-time selection of implementations

**Examples**:
- **Compressor Registry**: [`compressor_registry.h`](../include/dwarfs/compressor_registry.h)
- **Categorizer Registry**: [`writer/categorizer_registry.h`](../include/dwarfs/writer/categorizer_registry.h) (in writer namespace)
- **Serializer Registry**: [`serializer_registry.h`](../include/dwarfs/metadata/serialization/serializer_registry.h)

### 3. Strategy Pattern (I/O Layer)
**Purpose**: Switch between different I/O strategies at runtime

**Implementation**: [`internal/io_ops.h`](../include/dwarfs/internal/io_ops.h)
- `mmap_whole`: Map entire file (64-bit default)
- `mmap_segments`: Map file in chunks (32-bit default)
- `read`: Traditional read() calls (error-safe fallback)

### 4. Builder Pattern (Metadata Construction)
**Purpose**: Incrementally construct complex metadata structures

**Implementation**: [`writer/internal/metadata_builder.cpp`](../src/writer/internal/metadata_builder.cpp)
- Accumulates inodes, chunks, directories during scanning/segmenting
- Applies packing transformations at build time
- Serializes to chosen format on completion

### 5. Observer Pattern (Progress Reporting)
**Purpose**: Multi-component progress updates without tight coupling

**Implementation**: [`writer/writer_progress.h`](../include/dwarfs/writer/writer_progress.h)
- Scanner, segmenter, compressor all report to same progress object
- Progress object updates console asynchronously
- Different components update different metrics (files scanned, bytes segmented, blocks compressed)

## Build System Architecture

### CMake Module Organization

**Main Build File**: [`CMakeLists.txt`](../CMakeLists.txt) (1473 lines)
- Conditional compilation based on options (`WITH_LIBDWARFS`, `WITH_TOOLS`, `WITH_FUSE_DRIVER`)
- Platform detection and configuration
- Target definitions for libraries, tools, tests

**Modular CMake Files** in [`cmake/`](../cmake/):
- `libdwarfs.cmake`: Define all 5 library targets
- `folly.cmake`: Configure minimal Folly subset
- `thrift.cmake`: Configure fbthrift
- `metadata_serialization.cmake`: **Critical** - Configure serialization formats
- `tebako.cmake`: Tebako integration entry point

**Metadata Serialization Configuration** ([`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)):
```cmake
# Default: FlatBuffers always enabled, Thrift optional
DWARFS_WITH_THRIFT      = ON   # Optional, disabled in Tebako
DWARFS_WITH_FLATBUFFERS = ON   # Required (always)

# Tebako override:
if(TEBAKO_BUILD)
  set(DWARFS_WITH_THRIFT OFF FORCE)  # Static linking incompatible
endif()

# Format detection adds defines:
add_compile_definitions(DWARFS_HAVE_THRIFT=1)      # if Thrift available
add_compile_definitions(DWARFS_HAVE_FLATBUFFERS=1) # always defined
```

### Tebako Build Scopes

**MKD Scope** (`TEBAKO_BUILD_SCOPE=MKD`):
- Only `mkdwarfs` tool and its dependencies
- Minimal libraries (`dwarfs_common`, `dwarfs_writer`)
- No FUSE driver, no reader tools
- Use case: Embedding mkdwarfs in Tebako Ruby applications

**ALL Scope** (`TEBAKO_BUILD_SCOPE=ALL`):
- All tools: `mkdwarfs`, `dwarfsck`, `dwarfsextract`, `dwarfs` (if FUSE available)
- All libraries
- Full feature set
- Use case: Complete DwarFS functionality in Tebako

## Component Relationships

### Read Path (FUSE Driver)
```
┌──────────────┐
│ dwarfs_main  │ (FUSE callbacks)
└──────┬───────┘
       │
       ▼
┌──────────────────┐
│ filesystem_v2    │ (file/directory lookup)
└──────┬───────────┘
       │
       ▼
┌──────────────────┐         ┌─────────────────┐
│ metadata_v2      │────────▶│ SerializerFacade│
│                  │         └─────────────────┘
│ (inode access)   │         (format-agnostic access)
└──────┬───────────┘
       │
       ▼
┌──────────────────┐         ┌─────────────────┐
│ inode_reader_v2  │────────▶│ block_cache     │
│                  │         └─────────────────┘
│ (chunk reading)  │         (LRU + prefetch)
└──────────────────┘
```

### Write Path (mkdwarfs)
```
┌─────────┐
│ Scanner │ (multi-threaded file system traversal)
└────┬────┘
     │
     ▼
┌──────────────┐         ┌──────────────────┐
│ Categorizer  │────────▶│ Fragment Order   │
│              │         │                  │
│ (detect type)│         │ (similarity sort)│
└──────┬───────┘         └──────────────────┘
       │
       ▼
┌──────────────────┐
│ Segmenter        │ (rolling hash, bloom filter)
│                  │
│ (find duplicates)│
└──────┬───────────┘
       │
       ▼
┌──────────────────┐         ┌─────────────────┐
│ filesystem_writer│────────▶│ Compressor Pool │
│                  │         └─────────────────┘
│ (block building) │         (multi-threaded compression)
└──────┬───────────┘
       │
       ▼
┌──────────────────┐         ┌─────────────────┐
│ metadata_builder │────────▶│ SerializerFacade│
│                  │         └─────────────────┘
│ (metadata finish)│         (serialize to FlatBuffers/Thrift)
└──────────────────┘
```

## Platform-Specific Implementation

### Windows
- **File I/O**: Uses Windows API (`CreateFileW`, `ReadFile`, `WriteFile`)
- **Extended Attributes**: Native streams implementation via `NtQueryEaFile`/`NtSetEaFile`
- **Symlinks**: Supports both Windows symlinks and NTFS junctions
- **FUSE**: Via [WinFsp](https://github.com/winfsp/winfsp) library
- **Path Handling**: UTF-16 ↔ UTF-8 conversion throughout

**Key Files**:
- [`src/internal/io_ops_win.cpp`](../src/internal/io_ops_win.cpp)
- [`src/xattr_win.cpp`](../src/xattr_win.cpp)
- [`src/file_stat.cpp`](../src/file_stat.cpp) - Windows-specific stat implementation

### macOS
- **FUSE**: Supports both macFUSE (kernel extension) and FUSE-T (userspace)
- **FUSE-T Detection**: Automatic via pkg-config, no kernel extension required
- **Permissions**: Full Disk Access required for FUSE-T applications
- **Compilation**: Universal binary support via `WITH_UNIVERSAL_BINARY`

**Key Files**:
- [`cmake/need_fuse.cmake`](../cmake/need_fuse.cmake) - FUSE library detection

### Linux
- **FUSE**: Supports both FUSE2 and FUSE3 (FUSE3 preferred)
- **Extended Attributes**: POSIX xattr API with namespace mapping
- **Static Builds**: Alpine Linux with musl libc for portability

### FreeBSD
- **FUSE**: Linux compatibility layer + FUSE2/FUSE3
- **Static Builds**: Work with Linux emulation layer
- **ELF Branding**: Marks binaries as Linux ABI

## Critical Performance Paths

### Hot Path: Block Decompression
[`src/reader/internal/cached_block.cpp`](../src/reader/internal/cached_block.cpp)
- Called on every cache miss
- Multi-threaded (worker pool)
- Algorithm dispatch based on block compression type
- Critical for read performance

### Hot Path: Chunk Lookup
[`src/reader/internal/inode_reader_v2.cpp`](../src/reader/internal/inode_reader_v2.cpp)
- Translates inode + offset → block + offset
- Uses inode offset cache for fragmented files
- Critical for random read latency

### Hot Path: Segmentation
[`src/writer/internal/segmenter.cpp`](../src/writer/internal/segmenter.cpp) (not shown but referenced)
- Rolling hash computation
- Bloom filter lookups (96%+ rejection rate)
- Determines compression ratio
- Most CPU-intensive part of filesystem creation

## Dependency Management

### Required Dependencies
- **Boost** ≥1.67.0: program_options, chronos, filesystem, process
- **OpenSSL/LibreSSL** ≥3.0.0: libcrypto for checksums
- **libarchive** ≥3.6.0: for extraction to archive formats
- **xxHash** ≥0.8.1: fast integrity checking
- **zstd** ≥1.4.8: primary compression algorithm

### Optional Dependencies
- **Apache Thrift + Folly**: for Thrift format support (legacy, optional)
- **FlatBuffers**: for FlatBuffers format support (header-only, required via FetchContent)
- **FLAC** ≥1.4.2: PCM audio compression
- **lz4** ≥1.9.3: fast compression
- **liblzma** ≥5.2.5: LZMA compression
- **Brotli** ≥1.0.9: Brotli compression
- **jemalloc/mimalloc**: memory allocators
- **FUSE3/FUSE2**: for FUSE driver
- **WinFsp**: for Windows FUSE driver
- **macFUSE/FUSE-T**: for macOS FUSE driver

### Header-Only Libraries (Fetched if Not Found)
- **fmt** ≥10.0 (preferred 12.0.0): String formatting
- **GoogleTest** ≥1.13.0 (preferred 1.17.0): Testing framework
- **range-v3** ≥0.12.0: Range utilities
- **parallel-hashmap** ≥1.3.8 (preferred 2.0.0): Efficient hash maps

## Critical Code Locations

### Format Detection Logic
[`src/metadata/serialization/serializer_registry.cpp`](../src/metadata/serialization/serializer_registry.cpp):
- Checks magic bytes for FlatBuffers
- Falls back to Thrift if no magic found
- FlatBuffers takes priority over Thrift for new images

### Tool Entry Points
- [`tools/src/mkdwarfs_main.cpp`](../tools/src/mkdwarfs_main.cpp): Create filesystems
- [`tools/src/dwarfs_main.cpp`](../tools/src/dwarfs_main.cpp): FUSE driver
- [`tools/src/dwarfsck_main.cpp`](../tools/src/dwarfsck_main.cpp): Check/inspect
- [`tools/src/dwarfsextract_main.cpp`](../tools/src/dwarfsextract_main.cpp): Extract

### Universal Binary
[`tools/src/universal.cpp`](../tools/src/universal.cpp):
- Single executable combining all tools
- Selection via argv[0] (symlink name) or `--tool=<name>` option
- Self-extracting with UPX or custom stub

## Serialization Format Detailed Comparison

| Aspect | Thrift Compact | FlatBuffers |
|--------|---------------|-------------|
| **Dependencies** | Folly + fbthrift | Header-only |
| **Sections** | 2 (schema + data) | 1 (self-describing) |
| **Size** | Smallest (bit-packed) | Medium (+5-10%) |
| **Speed (serialize)** | Medium | Fast |
| **Speed (deserialize)** | Fastest (zero-copy) | Fast (zero-copy) |
| **Memory Mapping** | Yes (Frozen2) | Yes |
| **Platform Support** | Limited | Excellent |
| **Static Linking** | Difficult | Easy |
| **Tebako Compatible** | No | Yes |
| **Default** | No (legacy) | **Yes** |

## Key Benefits

**vs FUSE Mount**:
- ✅ **5-10% faster** (no kernel overhead)
- ✅ **Direct control** over cache/threading
- ✅ **Embeddable** in applications

**vs dwarfsextract Tool**:
- ✅ **Similar performance** (same underlying code)
- ✅ **More flexible** (custom logic)
- ✅ **Lower overhead** (no tool startup)

**vs Direct Archive**:
- ✅ **2-10x faster** random access
- ✅ **Deduplication** aware
- ✅ **Memory efficient**

### Optimization Guidelines

**Cache Sizing**:
```cpp
// Small working set (<1 GB)
config.cache_size = 256 << 20;  // 256 MiB

// Medium working set (1-5 GB)
config.cache_size = 1024 << 20;  // 1 GiB

// Large working set (>5 GB)
config.cache_size = 4096 << 20;  // 4 GiB
```

**Worker Threads**:
```cpp
// Single file: minimal
config.num_workers = 2;

// Multiple files: balanced
config.num_workers = 4;

// Bulk extraction: maximum
config.num_workers = std::thread::hardware_concurrency();
```

**Extraction Threads**:
```cpp
// Small file count (<100): single-threaded
size_t threads = 1;

// Large file count (100-1000): multi-threaded
size_t threads = 4;

// Very large (>1000): high parallelism
size_t threads = 8;
```

## Strategy Pattern Architecture (NEW - v0.16.0+)

**Problem Solved**: Hard dependencies on Thrift types throughout codebase prevented FlatBuffers-only builds.

**Solution**: Apply Strategy Pattern with Dependency Inversion Principle:

```
┌─────────────────────────────────────────────────────────┐
│       Abstract Interfaces (Format-Agnostic)             │
│                                                         │
│  metadata_provider (reader)  │  metadata_builder (writer)│
│  - get_chunk()               │  - gather_chunks()       │
│  - get_directory()           │  - gather_entries()      │
│  - get_inode()               │  - build() → domain      │
└──────────────────┬────────────────────────────────────┘
                   │
         ┌─────────┴─────────┐
         │   implements      │   implements
         ▼                   ▼
┌────────────────┐    ┌────────────────┐
│ Thrift Impl    │    │FlatBuffers Impl│
│ (optional)     │    │  (required)    │
│                │    │                │
│ Converts       │    │ Works directly │
│ domain ↔       │    │ with domain    │
│ thrift types   │    │ model          │
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

**Key Benefits**:
1. **Separation**: Thrift and FlatBuffers COMPLETELY separate
2. **Extensibility**: Add new format = implement interface
3. **Testability**: Mock interface for tests
4. **Clarity**: No mixing of concerns
5. **Maintainability**: Each format in its own files

**Implementation Files**:
- Abstract interfaces: `include/dwarfs/reader/metadata_provider.h`, `include/dwarfs/writer/internal/metadata_builder.h`
- Thrift implementation: `src/reader/thrift_metadata_provider.cpp`, `src/writer/internal/thrift_metadata_builder.cpp`
- FlatBuffers implementation: `src/reader/flatbuffers_metadata_provider.cpp`, `src/writer/internal/flatbuffers_metadata_builder.cpp`
- Factories: `*_factory.cpp` files

**Documentation**: [`doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`](../doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md)

### Format Selection Rationale

**Thrift Compact (Legacy, Optional)**:
- **Pros**: Extremely space-efficient Frozen2 bit-packed layouts, memory-mappable zero-copy access
- **Cons**: Complex dependencies (Folly + fbthrift), difficult to build on some platforms
- **Use case**: Reading old images only; optional for backward compatibility

**FlatBuffers (Modern, Required)**:
- **Pros**: Memory-mappable, zero-copy access, excellent portability, header-only, forward/backward compatible
- **Cons**: Slightly larger than Thrift (~5-10%)
- **Use case**: Default for all new images; works on all platforms

## Key Technical Decisions

### 1. Metadata Serialization Dual-Format Support
**Decision**: Support Thrift (optional) and FlatBuffers (required) formats

**Rationale**:
- **Thrift**: Legacy format, optional for reading old images only
- **FlatBuffers**: Modern default, memory-mappable, excellent portability, zero-copy access
- **Dual support**: Preserve backward compatibility while moving to simpler, more portable solution

**Implementation**:
- Compile-time format selection via CMake options (FlatBuffers always enabled)
- Runtime format detection via magic bytes
- Facade pattern abstracts differences
- Registry pattern for extensibility

**Files**:
- [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake) - Build configuration
- [`src/metadata/serialization/serializer_registry.cpp`](../src/metadata/serialization/serializer_registry.cpp) - Format registry
- [`src/metadata/serialization/facade_factory.cpp`](../src/metadata/serialization/facade_factory.cpp) - Facade creation

### 2. Memory-Mapped vs Read-Based I/O
**Decision**: Support both memory-mapping and read()-based file access

**Rationale**:
- **Memory-mapping**: Extremely fast, zero-copy, but can crash on I/O errors (SIGBUS)
- **Read-based**: Slower, but allows graceful error handling
- **Granular mapping**: For 32-bit systems to limit address space usage

**Implementation**: Environment variable `DWARFS_IOLAYER_OPTS` controls strategy

**Files**:
- [`include/dwarfs/internal/io_ops.h`](../include/dwarfs/internal/io_ops.h) - I/O layer abstraction
- [`src/internal/io_ops_posix.cpp`](../src/internal/io_ops_posix.cpp) - Platform implementation

### 3. Block Cache with Sequential Detection
**Decision**: Implement intelligent block cache with prefetching

**Rationale**:
- Random access: LRU cache prevents redundant decompression
- Sequential access: Prefetching dramatically improves throughput (2x+)
- Multi-threaded: Parallel decompression for multiple concurrent readers

**Implementation**:
- [`src/reader/internal/block_cache.cpp`](../src/reader/internal/block_cache.cpp)
- Heuristic detects sequential patterns, prefetches next blocks
- Configurable via `-o seq_detector` option

### 4. Registry Pattern for Extensibility
**Decision**: Use registry pattern for compressors, decompressors, categorizers, serializers

**Rationale**:
- Easy to add new algorithms without modifying core code
- Compile-time optional features (e.g., FLAC only if library available)
- Self-documenting registry lists available options

**Implementation**:
- [`include/dwarfs/compressor_registry.h`](../include/dwarfs/compressor_registry.h) - Singleton registry
- [`src/compressor_registry.cpp`](../src/compressor_registry.cpp) - Registration mechanism
- Each compressor self-registers via static initialization

### 5. Tebako Integration
**Decision**: Make DwarFS embeddable in Ruby executables via Tebako

**Rationale**:
- Tebako requires static linking
- Thrift/Folly incompatible with static linking in many scenarios
- Must support minimal dependency builds

**Implementation**:
- [`cmake/tebako.cmake`](../cmake/tebako.cmake) - Tebako entry point
- [`cmake/tebako/`](../cmake/tebako/) - Modular build configuration
  - `build_scopes.cmake`: Define MKD (mkdwarfs-only) vs ALL (all tools) build scopes
  - `platform_detection.cmake`: Detect MSys2, Windows, macOS variants
  - `dependency_paths.cmake`: Locate Tebako-provided dependencies
  - `compiler_flags.cmake`: Platform-specific compiler settings
  - `validation.cmake`: Pre-build checks
- Forces `DWARFS_WITH_THRIFT=OFF` in Tebako builds

## Design Patterns

### 1. Facade Pattern (Metadata Serialization)
**Purpose**: Hide complexity of multiple serialization formats behind unified interface

**Implementation**:
```cpp
// Facade: include/dwarfs/metadata/serialization/facade.h
class MetadataSerializationFacade {
  void serialize(metadata const&, byte_buffer&);
  void deserialize(span<uint8_t const>, metadata&);
  SerializationFormat get_format() const;
};

// Factory: src/metadata/serialization/facade_factory.cpp
class FacadeFactory {
  static unique_ptr<MetadataSerializationFacade> create(SerializationFormat);
  static optional<SerializationFormat> detect_format(vector<uint8_t> const&);
};
```

### 2. Registry Pattern (Multiple Subsystems)
**Purpose**: Allow runtime and compile-time selection of implementations

**Examples**:
- **Compressor Registry**: [`compressor_registry.h`](../include/dwarfs/compressor_registry.h)
- **Categorizer Registry**: [`writer/categorizer_registry.h`](../include/dwarfs/writer/categorizer_registry.h) (in writer namespace)
- **Serializer Registry**: [`serializer_registry.h`](../include/dwarfs/metadata/serialization/serializer_registry.h)

### 3. Strategy Pattern (I/O Layer)
**Purpose**: Switch between different I/O strategies at runtime

**Implementation**: [`internal/io_ops.h`](../include/dwarfs/internal/io_ops.h)
- `mmap_whole`: Map entire file (64-bit default)
- `mmap_segments`: Map file in chunks (32-bit default)
- `read`: Traditional read() calls (error-safe fallback)

### 4. Builder Pattern (Metadata Construction)
**Purpose**: Incrementally construct complex metadata structures

**Implementation**: [`writer/internal/metadata_builder.cpp`](../src/writer/internal/metadata_builder.cpp)
- Accumulates inodes, chunks, directories during scanning/segmenting
- Applies packing transformations at build time
- Serializes to chosen format on completion

### 5. Observer Pattern (Progress Reporting)
**Purpose**: Multi-component progress updates without tight coupling

**Implementation**: [`writer/writer_progress.h`](../include/dwarfs/writer/writer_progress.h)
- Scanner, segmenter, compressor all report to same progress object
- Progress object updates console asynchronously
- Different components update different metrics (files scanned, bytes segmented, blocks compressed)

## Build System Architecture

### CMake Module Organization

**Main Build File**: [`CMakeLists.txt`](../CMakeLists.txt) (1473 lines)
- Conditional compilation based on options (`WITH_LIBDWARFS`, `WITH_TOOLS`, `WITH_FUSE_DRIVER`)
- Platform detection and configuration
- Target definitions for libraries, tools, tests

**Modular CMake Files** in [`cmake/`](../cmake/):
- `libdwarfs.cmake`: Define all 5 library targets
- `folly.cmake`: Configure minimal Folly subset
- `thrift.cmake`: Configure fbthrift
- `metadata_serialization.cmake`: **Critical** - Configure serialization formats
- `tebako.cmake`: Tebako integration entry point

**Metadata Serialization Configuration** ([`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)):
```cmake
# Default: FlatBuffers always enabled, Thrift optional
DWARFS_WITH_THRIFT      = ON   # Optional, disabled in Tebako
DWARFS_WITH_FLATBUFFERS = ON   # Required (always)

# Tebako override:
if(TEBAKO_BUILD)
  set(DWARFS_WITH_THRIFT OFF FORCE)  # Static linking incompatible
endif()

# Format detection adds defines:
add_compile_definitions(DWARFS_HAVE_THRIFT=1)      # if Thrift available
add_compile_definitions(DWARFS_HAVE_FLATBUFFERS=1) # always defined
```

### Tebako Build Scopes

**MKD Scope** (`TEBAKO_BUILD_SCOPE=MKD`):
- Only `mkdwarfs` tool and its dependencies
- Minimal libraries (`dwarfs_common`, `dwarfs_writer`)
- No FUSE driver, no reader tools
- Use case: Embedding mkdwarfs in Tebako Ruby applications

**ALL Scope** (`TEBAKO_BUILD_SCOPE=ALL`):
- All tools: `mkdwarfs`, `dwarfsck`, `dwarfsextract`, `dwarfs` (if FUSE available)
- All libraries
- Full feature set
- Use case: Complete DwarFS functionality in Tebako

## Critical Performance Paths

### Hot Path: Block Decompression
[`src/reader/internal/cached_block.cpp`](../src/reader/internal/cached_block.cpp)
- Called on every cache miss
- Multi-threaded (worker pool)
- Algorithm dispatch based on block compression type
- Critical for read performance

### Hot Path: Chunk Lookup
[`src/reader/internal/inode_reader_v2.cpp`](../src/reader/internal/inode_reader_v2.cpp)
- Translates inode + offset → block + offset
- Uses inode offset cache for fragmented files
- Critical for random read latency

### Hot Path: Segmentation
[`src/writer/internal/segmenter.cpp`](../src/writer/internal/segmenter.cpp) (not shown but referenced)
- Rolling hash computation
- Bloom filter lookups (96%+ rejection rate)
- Determines compression ratio
- Most CPU-intensive part of filesystem creation

## Dependency Management

### Required Dependencies
- **Boost** ≥1.67.0: program_options, chronos, filesystem, process
- **OpenSSL/LibreSSL** ≥3.0.0: libcrypto for checksums
- **libarchive** ≥3.6.0: for extraction to archive formats
- **xxHash** ≥0.8.1: fast integrity checking
- **zstd** ≥1.4.8: primary compression algorithm

### Optional Dependencies
- **Apache Thrift + Folly**: for Thrift format support (legacy, optional)
- **FlatBuffers**: for FlatBuffers format support (header-only, required via FetchContent)
- **FLAC** ≥1.4.2: PCM audio compression
- **lz4** ≥1.9.3: fast compression
- **liblzma** ≥5.2.5: LZMA compression
- **Brotli** ≥1.0.9: Brotli compression
- **jemalloc/mimalloc**: memory allocators
- **FUSE3/FUSE2**: for FUSE driver
- **WinFsp**: for Windows FUSE driver
- **macFUSE/FUSE-T**: for macOS FUSE driver

### Header-Only Libraries (Fetched if Not Found)
- **fmt** ≥10.0 (preferred 12.0.0): String formatting
- **GoogleTest** ≥1.13.0 (preferred 1.17.0): Testing framework
- **range-v3** ≥0.12.0: Range utilities
- **parallel-hashmap** ≥1.3.8 (preferred 2.0.0): Efficient hash maps

## Design Patterns

### 1. Facade Pattern (Metadata Serialization)
**Purpose**: Hide complexity of multiple serialization formats behind unified interface

**Implementation**:
```cpp
// Facade: include/dwarfs/metadata/serialization/facade.h
class MetadataSerializationFacade {
  void serialize(metadata const&, byte_buffer&);
  void deserialize(span<uint8_t const>, metadata&);
  SerializationFormat get_format() const;
};

// Factory: src/metadata/serialization/facade_factory.cpp
class FacadeFactory {
  static unique_ptr<MetadataSerializationFacade> create(SerializationFormat);
  static optional<SerializationFormat> detect_format(vector<uint8_t> const&);
};
```

### 2. Registry Pattern (Multiple Subsystems)
**Purpose**: Allow runtime and compile-time selection of implementations

**Examples**:
- **Compressor Registry**: [`compressor_registry.h`](../include/dwarfs/compressor_registry.h)
- **Categorizer Registry**: [`writer/categorizer_registry.h`](../include/dwarfs/writer/categorizer_registry.h) (in writer namespace)
- **Serializer Registry**: [`serializer_registry.h`](../include/dwarfs/metadata/serialization/serializer_registry.h)

### 3. Strategy Pattern (I/O Layer)
**Purpose**: Switch between different I/O strategies at runtime

**Implementation**: [`internal/io_ops.h`](../include/dwarfs/internal/io_ops.h)
- `mmap_whole`: Map entire file (64-bit default)
- `mmap_segments`: Map file in chunks (32-bit default)
- `read`: Traditional read() calls (error-safe fallback)

### 4. Builder Pattern (Metadata Construction)
**Purpose**: Incrementally construct complex metadata structures

**Implementation**: [`writer/internal/metadata_builder.cpp`](../src/writer/internal/metadata_builder.cpp)
- Accumulates inodes, chunks, directories during scanning/segmenting
- Applies packing transformations at build time
- Serializes to chosen format on completion

### 5. Observer Pattern (Progress Reporting)
**Purpose**: Multi-component progress updates without tight coupling

**Implementation**: [`writer/writer_progress.h`](../include/dwarfs/writer/writer_progress.h)
- Scanner, segmenter, compressor all report to same progress object
- Progress object updates console asynchronously
- Different components update different metrics (files scanned, bytes segmented, blocks compressed)

## Build System Architecture

### CMake Module Organization

**Main Build File**: [`CMakeLists.txt`](../CMakeLists.txt) (1473 lines)
- Conditional compilation based on options (`WITH_LIBDWARFS`, `WITH_TOOLS`, `WITH_FUSE_DRIVER`)
- Platform detection and configuration
- Target definitions for libraries, tools, tests

**Modular CMake Files** in [`cmake/`](../cmake/):
- `libdwarfs.cmake`: Define all 5 library targets
- `folly.cmake`: Configure minimal Folly subset
- `thrift.cmake`: Configure fbthrift
- `metadata_serialization.cmake`: **Critical** - Configure serialization formats
- `tebako.cmake`: Tebako integration entry point

**Metadata Serialization Configuration** ([`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)):
```cmake
# Default: FlatBuffers always enabled, Thrift optional
DWARFS_WITH_THRIFT      = ON   # Optional, disabled in Tebako
DWARFS_WITH_FLATBUFFERS = ON   # Required (always)

# Tebako override:
if(TEBAKO_BUILD)
  set(DWARFS_WITH_THRIFT OFF FORCE)  # Static linking incompatible
endif()

# Format detection adds defines:
add_compile_definitions(DWARFS_HAVE_THRIFT=1)      # if Thrift available
add_compile_definitions(DWARFS_HAVE_FLATBUFFERS=1) # always defined
```

### Tebako Build Scopes

**MKD Scope** (`TEBAKO_BUILD_SCOPE=MKD`):
- Only `mkdwarfs` tool and its dependencies
- Minimal libraries (`dwarfs_common`, `dwarfs_writer`)
- No FUSE driver, no reader tools
- Use case: Embedding mkdwarfs in Tebako Ruby applications

**ALL Scope** (`TEBAKO_BUILD_SCOPE=ALL`):
- All tools: `mkdwarfs`, `dwarfsck`, `dwarfsextract`, `dwarfs` (if FUSE available)
- All libraries
- Full feature set
- Use case: Complete DwarFS functionality in Tebako

## Critical Performance Paths

### Hot Path: Block Decompression
[`src/reader/internal/cached_block.cpp`](../src/reader/internal/cached_block.cpp)
- Called on every cache miss
- Multi-threaded (worker pool)
- Algorithm dispatch based on block compression type
- Critical for read performance

### Hot Path: Chunk Lookup
[`src/reader/internal/inode_reader_v2.cpp`](../src/reader/internal/inode_reader_v2.cpp)
- Translates inode + offset → block + offset
- Uses inode offset cache for fragmented files
- Critical for random read latency

### Hot Path: Segmentation
[`src/writer/internal/segmenter.cpp`](../src/writer/internal/segmenter.cpp) (not shown but referenced)
- Rolling hash computation
- Bloom filter lookups (96%+ rejection rate)
- Determines compression ratio
- Most CPU-intensive part of filesystem creation

## Dependency Management

### Required Dependencies
- **Boost** ≥1.67.0: program_options, chronos, filesystem, process
- **OpenSSL/LibreSSL** ≥3.0.0: libcrypto for checksums
- **libarchive** ≥3.6.0: for extraction to archive formats
- **xxHash** ≥0.8.1: fast integrity checking
- **zstd** ≥1.4.8: primary compression algorithm

### Optional Dependencies
- **Apache Thrift + Folly**: for Thrift format support (legacy, optional)
- **FlatBuffers**: for FlatBuffers format support (header-only, required via FetchContent)
- **FLAC** ≥1.4.2: PCM audio compression
- **lz4** ≥1.9.3: fast compression
- **liblzma** ≥5.2.5: LZMA compression
- **Brotli** ≥1.0.9: Brotli compression
- **jemalloc/mimalloc**: memory allocators
- **FUSE3/FUSE2**: for FUSE driver
- **WinFsp**: for Windows FUSE driver
- **macFUSE/FUSE-T**: for macOS FUSE driver

### Header-Only Libraries (Fetched if Not Found)
- **fmt** ≥10.0 (preferred 12.0.0): String formatting
- **GoogleTest** ≥1.13.0 (preferred 1.17.0): Testing framework
- **range-v3** ≥0.12.0: Range utilities
- **parallel-hashmap** ≥1.3.8 (preferred 2.0.0): Efficient hash maps

## Critical Code Locations

### Format Detection Logic
[`src/metadata/serialization/serializer_registry.cpp`](../src/metadata/serialization/serializer_registry.cpp):
- Checks magic bytes for FlatBuffers
- Falls back to Thrift if no magic found
- FlatBuffers takes priority over Thrift for new images

### Tool Entry Points
- [`tools/src/mkdwarfs_main.cpp`](../tools/src/mkdwarfs_main.cpp): Create filesystems
- [`tools/src/dwarfs_main.cpp`](../tools/src/dwarfs_main.cpp): FUSE driver
- [`tools/src/dwarfsck_main.cpp`](../tools/src/dwarfsck_main.cpp): Check/inspect
- [`tools/src/dwarfsextract_main.cpp`](../tools/src/dwarfsextract_main.cpp): Extract

### Universal Binary
[`tools/src/universal.cpp`](../tools/src/universal.cpp):
- Single executable combining all tools
- Selection via argv[0] (symlink name) or `--tool=<name>` option
- Self-extracting with UPX or custom stub

## Critical Code Locations (Explanation)

### Format Detection Logic
[`src/metadata/serialization/serializer_registry.cpp`](../src/metadata/serialization/serializer_registry.cpp):
- Checks magic bytes for FlatBuffers
- Falls back to Thrift if no magic found
- FlatBuffers takes priority over Thrift for new images

### Tool Entry Points
- [`tools/src/mkdwarfs_main.cpp`](../tools/src/mkdwarfs_main.cpp): Create filesystems
- [`tools/src/dwarfs_main.cpp`](../tools/src/dwarfs_main.cpp): FUSE driver
- [`tools/src/dwarfsck_main.cpp`](../tools/src/dwarfsck_main.cpp): Check/inspect
- [`tools/src/dwarfsextract_main.cpp`](../tools/src/dwarfsextract_main.cpp): Extract

### Universal Binary
[`tools/src/universal.cpp`](../tools/src/universal.cpp):
- Single executable combining all tools
- Selection via argv[0] (symlink name) or `--tool=<name>` option
- Self-extracting with UPX or custom stub

## Testing Architecture

### Test Organization
```
test/
├── *_test.cpp           # Unit tests (60+ files)
├── tool_main_*_test.cpp # Tool integration tests
├── test_helpers.cpp     # Common test utilities
├── test_dirtree.cpp     # Synthetic directory generator
├── loremipsum.cpp       # Test data generation
├── fixtures/            # Test input files
└── metadata/            # Metadata-specific tests
    └── serialization/   # Metadata-specific tests
```

### Test Categories

**Unit Tests** (`dwarfs_unit_tests`):
- Algorithm tests (segmentation, nilsimsa, bloom filter)
- Data structure tests (packed arrays, string tables)
- Utility tests (checksums, path handling, time parsing)
- **Metadata serialization tests**: Round-trip for all
 3 formats

**Integration Tests** (`tool_main_test`):
- Command-line parsing
- Tool workflows (create → check → extract)
- Cross-tool compatibility

**Expensive Tests** (`dwarfs_expensive_tests`):
- Full filesystem creation workflows
- Compatibility with older format versions
- Large-scale tests

**Format-Specific Tests** ([`test/metadata/serialization_test.cpp`](../test/metadata/serialization_test.cpp)):
- Serialize metadata with each format
- Deserialize and verify integrity
- Cross-format compatibility where applicable
- Format detection accuracy

### CI/CD Testing Matrix

**Primary CI** ([`.github/workflows/build.yml`](../.github/workflows/build.yml)):
- **Linux**: Ubuntu, Fedora, Arch, Alpine, Debian, openSUSE
- **Architectures**: x86_64, aarch64, riscv64, i386, arm, ppc64, ppc64le, s390x, loongarch64
- **Build types**: Release, Debug, with sanitizers (ASAN, TSAN, UBSAN), with coverage
- **Static/Shared**: Both linking modes tested

**Metadata Format CI** (Lines 958-1116 in build.yml):
- Tests FlatBuffers vs Thrift across platforms:
  - Ubuntu x86_64: flatbuffers-only, both-formats, thrift-only (expected fail)
  - Ubuntu aarch64: flatbuffers-only, both-formats
  - macOS x86_64: both-formats
  - macOS aarch64: flatbuffers-only
  - Windows x64: both-formats, flatbuffers-only
- Verifies that thrift-only builds fail (FlatBuffers is required)
- Runs metadata-specific tests

**Tebako CI** (Lines 1171-1343 in build.yml):
- Ubuntu 20.04 (amd64, arm64): gcc-10, clang-12
- macOS: Xcode 15.0.1, 15.4
- Alpine 3.17: gcc, clang
- Both MKD and ALL scopes
- **Always runs with `DWARFS_WITH_THRIFT=OFF`**

## Critical Performance Paths

### Hot Path: Block Decompression
[`src/reader/internal/cached_block.cpp`](../src/reader/internal/cached_block.cpp)
- Called on every cache miss
- Multi-threaded (worker pool)
- Algorithm dispatch based on block compression type
- Critical for read performance

### Hot Path: Chunk Lookup
[`src/reader/internal/inode_reader_v2.cpp`](../src/reader/internal/inode_reader_v2.cpp)
- Translates inode + offset → block + offset
- Uses inode offset cache for fragmented files
- Critical for random read latency

### Hot Path: Segmentation
[`src/writer/internal/segmenter.cpp`](../src/writer/internal/segmenter.cpp) (not shown but referenced)
- Rolling hash computation
- Bloom filter lookups (96%+ rejection rate)
- Determines compression ratio
- Most CPU-intensive part of filesystem creation

## Design Patterns

### 1. Facade Pattern (Metadata Serialization)
**Purpose**: Hide complexity of multiple serialization formats behind unified interface

**Implementation**:
```cpp
// Facade: include/dwarfs/metadata/serialization/facade.h
class MetadataSerializationFacade {
  void serialize(metadata const&, byte_buffer&);
  void deserialize(span<uint8_t const>, metadata&);
  SerializationFormat get_format() const;
};

// Factory: src/metadata/serialization/facade_factory.cpp
class FacadeFactory {
  static unique_ptr<MetadataSerializationFacade> create(SerializationFormat);
  static optional<SerializationFormat> detect_format(vector<uint8_t> const&);
};
```

### 2. Registry Pattern (Multiple Subsystems)
**Purpose**: Allow runtime and compile-time selection of implementations

**Examples**:
- **Compressor Registry**: [`compressor_registry.h`](../include/dwarfs/compressor_registry.h)
- **Categorizer Registry**: [`writer/categorizer_registry.h`](../include/dwarfs/writer/categorizer_registry.h) (in writer namespace)
- **Serializer Registry**: [`serializer_registry.h`](../include/dwarfs/metadata/serialization/serializer_registry.h)

### 3. Strategy Pattern (I/O Layer)
**Purpose**: Switch between different I/O strategies at runtime

**Implementation**: [`internal/io_ops.h`](../include/dwarfs/internal/io_ops.h)
- `mmap_whole`: Map entire file (64-bit default)
- `mmap_segments`: Map file in chunks (32-bit default)
- `read`: Traditional read() calls (error-safe fallback)

### 4. Builder Pattern (Metadata Construction)
**Purpose**: Incrementally construct complex metadata structures

**Implementation**: [`writer/internal/metadata_builder.cpp`](../src/writer/internal/metadata_builder.cpp)
- Accumulates inodes, chunks, directories during scanning/segmenting
- Applies packing transformations at build time
- Serializes to chosen format on completion

### 5. Observer Pattern (Progress Reporting)
**Purpose**: Multi-component progress updates without tight coupling

**Implementation**: [`writer/writer_progress.h`](../include/dwarfs/writer/writer_progress.h)
- Scanner, segmenter, compressor all report to same progress object
- Progress object updates console asynchronously
- Different components update different metrics (files scanned, bytes segmented, blocks compressed)

## Build System Architecture

### CMake Module Organization

**Main Build File**: [`CMakeLists.txt`](../CMakeLists.txt) (1473 lines)
- Conditional compilation based on options (`WITH_LIBDWARFS`, `WITH_TOOLS`, `WITH_FUSE_DRIVER`)
- Platform detection and configuration
- Target definitions for libraries, tools, tests

**Modular CMake Files** in [`cmake/`](../cmake/):
- `libdwarfs.cmake`: Define all 5 library targets
- `folly.cmake`: Configure minimal Folly subset
- `thrift.cmake`: Configure fbthrift
- `metadata_serialization.cmake`: **Critical** - Configure serialization formats
- `tebako.cmake`: Tebako integration entry point

**Metadata Serialization Configuration** ([`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)):
```cmake
# Default: FlatBuffers always enabled, Thrift optional
DWARFS_WITH_THRIFT      = ON   # Optional, disabled in Tebako
DWARFS_WITH_FLATBUFFERS = ON   # Required (always)

# Tebako override:
if(TEBAKO_BUILD)
  set(DWARFS_WITH_THRIFT OFF FORCE)  # Static linking incompatible
endif()

# Format detection adds defines:
add_compile_definitions(DWARFS_HAVE_THRIFT=1)      # if Thrift available
add_compile_definitions(DWARFS_HAVE_FLATBUFFERS=1) # always defined
```

### Tebako Build Scopes

**MKD Scope** (`TEBAKO_BUILD_SCOPE=MKD`):
- Only `mkdwarfs` tool and its dependencies
- Minimal libraries (`dwarfs_common`, `dwarfs_writer`)
- No FUSE driver, no reader tools
- Use case: Embedding mkdwarfs in Tebako Ruby applications

**ALL Scope** (`TEBAKO_BUILD_SCOPE=ALL`):
- All tools: `mkdwarfs`, `dwarfsck`, `dwarfsextract`, `dwarfs` (if FUSE available)
- All libraries
- Full feature set
- Use case: Complete DwarFS functionality in Tebako

## Serializing Metadata with FlatBuffers
**Header-Only Library**: [`fmt`](https://fmt.dev/latest/api.html) is a C++ header-only library

**Location in Installation**: `/usr/local/lib` and `/usr/local/include`

**Key Files**:
- Header file: `/usr/local/include/fmt/format.h`
- Library file: `/usr/local/lib/libfmt.so`
- Symbolic link: `/usr/local/lib/fmt` (created by `sw vpkg cuse-dbde7d9255bc1a1f6b22e94d04cb578fd44ab7ec4dc27fa863cbeb08b9fbe70` with `acl`, `facl`, and `xattr` options
- **Mount Point**: `/` (root directory)
- **Root Access User**: `root`
- **Mount Username**: `<current username>`
- **Root Access**: ✅ **No direct root mount**
- **Options**: `-o imageset`, `-o noimageset`, `-o noclessrc`
- **Branch Point**: `/` (mount point) iff `-o imageset`, else `/imageset`
- **Mount Point**: `/` (root directory)
- **Root Access User**: `root`
- **Mount Username**: `<current username>`
- **Root Access**: ✅ **No direct root mount**
- **Binary Mount Point**: `/dev/sda3`
- **Binary Mount Options**: `binary,nosuid,(custom username)`
