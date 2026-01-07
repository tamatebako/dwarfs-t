# DwarFS Metadata Architecture Guide

**Last Updated**: 2025-12-24
**Version**: 0.16.0+
**Status**: Complete (Sessions 28-35)

---

## Table of Contents

1. [Overview](#overview)
2. [Domain Model Design](#domain-model-design)
3. [Strategy Pattern Implementation](#strategy-pattern-implementation)
4. [Backend Adapter Pattern](#backend-adapter-pattern)
5. [Serialization Formats](#serialization-formats)
6. [Build Configurations](#build-configurations)
7. [Performance Characteristics](#performance-characteristics)

---

## Overview

DwarFS v0.16.0 implements a clean, extensible metadata architecture based on the **Strategy Pattern** with **Dependency Inversion Principle**. This design enables:

- ✅ **Format independence**: Core logic doesn't depend on serialization format
- ✅ **Extensibility**: Easy to add new formats (Protocol Buffers, Cap'n Proto, etc.)
- ✅ **Testability**: Mock interfaces for unit testing
- ✅ **Flexibility**: Build with one or both formats
- ✅ **Maintainability**: Each format in separate, focused files

---

## Domain Model Design

### Philosophy

The domain model represents metadata in a **format-agnostic** way using native C++ types. All business logic operates on this model, completely independent of serialization format.

**Location**: [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h)

### Core Structures

```cpp
namespace metadata::domain {

struct metadata {
  // Core tables
  std::vector<chunk> chunks;
  std::vector<directory> directories;
  std::vector<inode_data> inodes;

  // Index tables
  std::vector<uint32_t> chunk_table;
  std::vector<uint32_t> symlink_table;
  std::vector<uint32_t> uids;
  std::vector<uint32_t> gids;
  std::vector<uint16_t> modes;

  // String tables
  std::vector<std::string> names;
  std::vector<std::string> symlinks;

  // Optional features
  std::optional<std::vector<dir_entry>> dir_entries;  // v2.3+
  std::optional<std::vector<uint32_t>> shared_files_table;
  std::optional<std::vector<uint64_t>> devices;
  std::optional<fs_options> options;

  // Metadata
  uint32_t block_size;
  uint64_t total_fs_size;
  uint64_t timestamp_base;
  std::optional<uint64_t> total_allocated_fs_size;
  std::optional<uint32_t> hole_block_index;
  // ... more fields
};

} // namespace metadata::domain
```

### Benefits

- **Type Safety**: Compile-time checking, no runtime format errors
- **Performance**: Native C++ structures, no serialization overhead in memory
- **Clarity**: Self-documenting, obvious semantics
- **Compatibility**: Easy to extend without breaking existing code

---

## Strategy Pattern Implementation

### Abstract Interfaces

**Writer Interface** ([`metadata_writer_interface.h`](../include/dwarfs/writer/metadata_writer_interface.h)):

```cpp
class metadata_writer_interface {
 public:
  virtual ~metadata_writer_interface() = default;

  virtual void add_chunk(...) = 0;
  virtual void add_directory(...) = 0;
  virtual void add_inode(...) = 0;

  virtual metadata::domain::metadata build() = 0;
  virtual void serialize(byte_buffer& buf) const = 0;
};
```

**Reader Interface** ([`metadata_provider.h`](../include/dwarfs/reader/metadata_provider.h)):

```cpp
class metadata_provider {
 public:
  virtual ~metadata_provider() = default;

  virtual chunk_view get_chunk(uint32_t index) const = 0;
  virtual directory_view get_directory(uint32_t inode) const = 0;
  virtual inode_view get_inode(uint32_t inode) const = 0;
};
```

### Implementations

**FlatBuffers** (Always Available):
- **Writer**: [`flatbuffers_metadata_writer.cpp`](../src/writer/flatbuffers_metadata_writer.cpp)
- **Reader**: [`flatbuffers_metadata_provider.cpp`](../src/reader/flatbuffers_metadata_provider.cpp)
- **Serializer**: [`flatbuffers_serializer.cpp`](../src/metadata/serialization/flatbuffers_serializer.cpp)

**Thrift** (Optional):
- **Writer**: [`thrift_metadata_writer.cpp`](../src/writer/thrift_metadata_writer.cpp)
- **Reader**: [`thrift_metadata_provider.cpp`](../src/reader/thrift_metadata_provider.cpp)
- **Serializer**: [`thrift_compact_serializer.cpp`](../src/metadata/serialization/thrift_compact_serializer.cpp)

---

## Backend Adapter Pattern

### Problem Solved

The reader layer needs to construct view types (`chunk_range`, `inode_view`, etc.) that differ across build configurations:

- **FlatBuffers-only**: Views wrap domain model directly
- **Thrift-only**: Views expect Thrift frozen types
- **Both-formats**: Views use interface polymorphism

### Solution: Backend Adapter

**Location**: [`src/reader/internal/backend_adapter.{h,cpp}`](../src/reader/internal/backend_adapter.h)

**Purpose**: Translate domain model to backend-specific view types based on build configuration.

```cpp
class backend_adapter {
 public:
  // Single-format builds: convert from domain
  static chunk_range make_chunk_range(
      metadata::domain::metadata const& domain_meta,
      uint32_t begin, uint32_t end);

  // Dual-format builds: pass-through interfaces
  static dir_entry_view make_dir_entry_view(
      std::shared_ptr<dir_entry_view_interface const> impl);

  // Similar for inode_view, directory_view
};
```

### Implementation Details

**FlatBuffers-Only**:
```cpp
// Direct construction - domain types ARE the backend types
return chunk_range{domain_meta, begin, end};
```

**Thrift-Only** (with thread-local caching):
```cpp
// Convert domain → frozen Thrift → construct view
thread_local cache<domain_ptr, frozen_thrift>;
auto frozen = get_or_convert(domain_meta);
return thrift_backend::chunk_range{frozen, begin, end};
```

**Both-Formats**:
```cpp
// Wrap domain implementation in interface
auto impl = std::make_shared<domain_chunk_range_impl>(domain_meta, begin, end);
return chunk_range_wrapper{impl};
```

### Thread-Local Caching (Thrift-Only)

To avoid repeated domain→Thrift conversions:

```cpp
struct thrift_metadata_cache {
  Bundled<FrozenView> frozen;                           // Frozen metadata
  std::unique_ptr<thrift_backend::global_metadata> global;  // View wrapper
};

thread_local std::unordered_map<void const*, shared_ptr<cache>> caches;
```

**Performance**:
- First call: ~100μs (domain→Thrift→freeze)
- Subsequent calls: ~1μs (cache lookup)
- Memory: One frozen copy per thread per domain model

---

## Serialization Formats

### FlatBuffers (Required)

**Wire Format**:
```
[4-byte size prefix][DFBF magic][FlatBuffers data]
```

**Characteristics**:
- **Dependencies**: Header-only FlatBuffers library
- **Memory**: Zero-copy via `GetSizePrefixedRoot()`
- **Verification**: `VerifySizePrefixedBuffer()` with increased limits (v0.16.0)
- **Size**: ~105-108% of Thrift
- **Speed (compression)**: 17-29% faster than Thrift at levels 1-3
- **Speed (extraction)**: Equivalent to Thrift (~3.4% difference)

**Schema**: [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs)

### Thrift Compact (Optional)

**Wire Format**:
```
[Schema section][Frozen2 data]
```

**Characteristics**:
- **Dependencies**: Folly + fbthrift
- **Memory**: Zero-copy via Frozen2 layouts
- **Structure**: Bit-packed, extremely dense
- **Size**: Baseline (100%)
- **Speed**: Slightly slower compression, equivalent extraction

**Schema**: [`thrift/metadata.thrift`](../thrift/metadata.thrift)

---

## Build Configurations

### Three Valid Configurations

| Config | FlatBuffers | Thrift | Writer | Reader | Use Case |
|--------|-------------|--------|--------|--------|----------|
| **fb-only** | ON | OFF | ✅ | ✅ | **Recommended** (portable) |
| **both** | ON | ON | ✅ | ✅ | Development/testing |
| **thrift-only** | OFF | ON | ✅ | ✅ | Legacy environments |

### Type Aliases

**FlatBuffers-Only** (`metadata_types.h`):
```cpp
using inode_view_impl = domain_inode_view_impl;
using chunk_range = domain_chunk_range_impl;
using global_metadata = domain_global_metadata;
```

**Thrift-Only**:
```cpp
using inode_view_impl = thrift_backend::inode_view_impl;
using chunk_range = thrift_backend::chunk_range;
using global_metadata = thrift_backend::global_metadata;
```

**Both-Formats**:
```cpp
using inode_view_impl = inode_view_interface;           // Polymorphic
using chunk_range = chunk_range_wrapper;                // Value-semantic wrapper
using global_metadata = global_metadata_interface;      // Polymorphic
```

### CMake Configuration

**Metadata Serialization** ([`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)):

```cmake
# Default: FlatBuffers ON, Thrift ON (both optional)
option(DWARFS_WITH_FLATBUFFERS "Enable FlatBuffers" ON)
option(DWARFS_WITH_THRIFT "Enable Thrift" ON)

# Validation: At least one required
if(NOT DWARFS_WITH_FLATBUFFERS AND NOT DWARFS_WITH_THRIFT)
  message(FATAL_ERROR "At least one format (FlatBuffers or Thrift) required")
endif()

# Tebako override: Thrift incompatible with static linking
if(TEBAKO_BUILD)
  set(DWARFS_WITH_THRIFT OFF CACHE BOOL "" FORCE)
endif()
```

---

## Performance Characteristics

### Compression (FlatBuffers vs Thrift)

| Level | FlatBuffers Time | Thrift Time | Speedup |
|-------|------------------|-------------|---------|
| 1 (fast) | 1.489s | 2.095s | **+28.9%** |
| 3 (default) | 2.999s | 3.617s | **+17.1%** |
| 9 (max) | 27.043s | 26.606s | -1.6% (≈equal) |

**Conclusion**: FlatBuffers is **significantly faster** at typical compression levels.

### Extraction (FlatBuffers vs Thrift)

| Metric | FlatBuffers | Thrift | Delta |
|--------|-------------|--------|-------|
| Time (level 3) | 2.069s | 1.998s | +3.4% (≈equal) |
| Content | ✅ Identical | ✅ Identical | Byte-for-byte match |

**Conclusion**: Both formats produce **identical output** with **equivalent extraction speed**.

### Size Overhead

| Level | FlatBuffers | Thrift | Overhead | Absolute |
|-------|-------------|--------|----------|----------|
| 1 | 36.77 MB | 36.43 MB | +0.93% | +340 KB |
| 3 | 27.67 MB | 27.29 MB | +1.41% | +385 KB |
| 9 | 14.00 MB | 13.99 MB | +0.07% | +9.9 KB |

**Conclusion**: Size overhead is **negligible** (<1.5% at all levels).

### Memory Usage

**FlatBuffers**:
- Domain model in memory (native C++ structures)
- Zero-copy access to serialized form (memory-mapped)

**Thrift**:
- Frozen2 layout in memory (bit-packed)
- Zero-copy access (memory-mapped)

**Both**: Comparable memory footprint, both use zero-copy techniques.

---

## Thread-Local Caching (Thrift-Only Builds)

In thrift-only builds, the domain model must be converted to Thrift frozen types for view construction. To avoid repeated conversions:

### Cache Structure

```cpp
struct thrift_metadata_cache {
  // Frozen Thrift metadata (zero-copy views)
  apache::thrift::frozen::Bundled<FrozenView> frozen;

  // Thrift global metadata wrapper
  std::unique_ptr<thrift_backend::global_metadata> global;
};

// Thread-local cache: domain_ptr → cache
thread_local std::unordered_map<
    void const*,  // Domain model pointer (identity)
    std::shared_ptr<thrift_metadata_cache>
> cache;
```

### Cache Operations

**First Access** (~100μs):
1. Convert domain model → mutable Thrift
2. Freeze mutable Thrift → frozen layout
3. Create thrift_backend::global_metadata wrapper
4. Store in thread-local cache

**Subsequent Access** (~1μs):
1. Pointer lookup in cache
2. Return cached frozen metadata

### Cache Lifetime

- **Scope**: Per-thread
- **Key**: Pointer to domain model (identity-based)
- **Eviction**: Automatic when domain model destroyed
- **Memory**: One frozen copy per thread per unique domain model

---

## Key Benefits

### vs Monolithic Format-Specific Code

**Before** (pre-v0.16.0):
- Format-specific code scattered throughout codebase
- Difficult to add new formats
- Testing required both formats
- High coupling between business logic and serialization

**After** (v0.16.0+):
- Clean separation: domain model ↔ serialization
- New format = implement interfaces
- Test domain logic independently
- Low coupling, high cohesion

### vs Multiple Serialization Libraries

**Strategy Pattern** approach:
- ✅ Single domain model (one source of truth)
- ✅ Format-specific adapters (thin translation layer)
- ✅ Runtime format detection (automatic)
- ✅ Compile-time format selection (optional builds)

**Alternative** (direct serialization):
- ❌ Duplicate data structures for each format
- ❌ Complex synchronization logic
- ❌ Rigid format coupling
- ❌ Difficult to extend

---

## Build Configuration Matrix

### Detailed Feature Support

| Feature | fb-only | thrift-only | both |
|---------|---------|-------------|------|
| **Create FS** | ✅ .dff | ✅ .dft | ✅ Both |
| **Mount FS** | ✅ .dff | ✅ .dft | ✅ Both |
| **Check FS** | ✅ Both | ✅ Both | ✅ Both |
| **Extract FS** | ✅ Both | ✅ Both | ✅ Both |
| **Sparse files** | ✅ Full | ✅ Full | ⚠️ Dense |
| **Dependencies** | Minimal | Heavy | Heavy |
| **Build time** | Fast | Slow | Slow |
| **Binary size** | Small | Large | Largest |

**Note**: Both-formats disables `SEEK_DATA`/`SEEK_HOLE` due to format incompatibility. Files are treated as fully dense (minimal impact for 99.9% of use cases).

### Recommended Configuration

**For Production**: FlatBuffers-only
- ✅ Minimal dependencies (header-only FlatBuffers)
- ✅ Fast compilation
- ✅ Excellent portability
- ✅ Full sparse file support
- ✅ 17-29% faster compression

**For Development**: Both-formats (optional)
- ✅ Test cross-format compatibility
- ✅ Read legacy Thrift images
- ⚠️ Requires Folly + fbthrift
- ⚠️ Sparse file seeking disabled

**For Legacy**: Thrift-only (optional)
- ✅ Smallest metadata size (baseline)
- ✅ Full sparse file support
- ⚠️ Complex dependencies
- ⚠️ Limited platform support

---

## Implementation Files

### Domain Model
- **Header**: [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h)
- **Views**: [`include/dwarfs/reader/internal/domain_metadata_views.h`](../include/dwarfs/reader/internal/domain_metadata_views.h)
- **Implementation**: [`src/reader/internal/domain_metadata_views.cpp`](../src/reader/internal/domain_metadata_views.cpp)

### Backend Adapter
- **Header**: [`src/reader/internal/backend_adapter.h`](../src/reader/internal/backend_adapter.h)
- **Implementation**: [`src/reader/internal/backend_adapter.cpp`](../src/reader/internal/backend_adapter.cpp)

### Writer Implementations
- **FlatBuffers**: [`src/writer/flatbuffers_metadata_writer.cpp`](../src/writer/flatbuffers_metadata_writer.cpp)
- **Thrift**: [`src/writer/thrift_metadata_writer.cpp`](../src/writer/thrift_metadata_writer.cpp)

### Reader Implementations
- **FlatBuffers**: [`src/reader/internal/metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp)
- **Thrift**: [`src/reader/internal/metadata_v2_thrift.cpp`](../src/reader/internal/metadata_v2_thrift.cpp)

### Serialization
- **FlatBuffers**: [`src/metadata/serialization/flatbuffers_serializer.cpp`](../src/metadata/serialization/flatbuffers_serializer.cpp)
- **Thrift**: [`src/metadata/serialization/thrift_compact_serializer.cpp`](../src/metadata/serialization/thrift_compact_serializer.cpp)
- **Facade**: [`src/metadata/serialization/serialization_facade.cpp`](../src/metadata/serialization/serialization_facade.cpp)
- **Registry**: [`src/metadata/serialization/serializer_registry.cpp`](../src/metadata/serialization/serializer_registry.cpp)

### Build System
- **Format Config**: [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)
- **Library Config**: [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake)
- **Folly Config**: [`cmake/folly.cmake`](../cmake/folly.cmake)
- **Thrift Config**: [`cmake/thrift.cmake`](../cmake/thrift.cmake)

---

## Testing

### Test Coverage

**Unit Tests** ([`test/metadata/`](../test/metadata/)):
- Domain model serialization (round-trip)
- Format detection (magic bytes)
- Backend adapter (all configurations)
- View construction (polymorphism)

**Integration Tests**:
- mkdwarfs → dwarfsck (create + verify)
- dwarfsextract (full extraction)
- Cross-format operations (fb tool reading thrift image)

**CI Matrix** ([`.github/workflows/build.yml`](../.github/workflows/build.yml)):
- All three build configurations
- Multiple platforms (Linux, macOS, Windows)
- Multiple architectures (x86_64, aarch64, riscv64, etc.)

---

## Migration Path

### From Pre-v0.16.0

**Old images**: Continue to work (backward compatible)
**New images**: Use FlatBuffers by default

**No action required** - tools auto-detect format.

### From Thrift-Only Deployments

**Option A**: Keep Thrift-only builds (fully supported)
**Option B**: Switch to both-formats (read old images, create new FlatBuffers)
**Option C**: Switch to FlatBuffers-only (extract old images, recreate as FlatBuffers)

All options are valid - choose based on your requirements.

---

## Performance Tuning

### Cache Sizing

```cpp
// Reader config
filesystem_load_config config;
config.cache_size = 1024 << 20;  // 1 GiB
config.num_workers = 4;           // Decompression threads
```

**Guidelines**:
- Small working set (<1 GB): 256 MiB cache
- Medium working set (1-5 GB): 1 GiB cache
- Large working set (>5 GB): 4 GiB cache

### Worker Threads

**Writer** (mkdwarfs):
```bash
mkdwarfs -i input -o output.dff --num-workers=8
```

**Reader** (dwarfs FUSE):
```bash
dwarfs image.dff /mnt -o workers=4
```

**Guideline**: Use `std::thread::hardware_concurrency()` for maximum parallelism.

---

## Future Extensibility

### Adding New Formats

To add a new format (e.g., Protocol Buffers):

1. **Define schema**: `protobuf/metadata.proto`
2. **Implement writer**: `src/writer/protobuf_metadata_writer.cpp`
3. **Implement reader**: `src/reader/internal/metadata_v2_protobuf.cpp`
4. **Implement serializer**: `src/metadata/serialization/protobuf_serializer.cpp`
5. **Register format**: Add to `serializer_registry`
6. **Add CMake option**: `DWARFS_WITH_PROTOBUF`

**Impact**: Zero changes to core business logic!

---

## References

- **Session 28-35**: Complete metadata architecture refactoring
- **Performance Benchmarks**: [`doc/DWARFS_METADATA_FORMAT_PERFORMANCE.md`](DWARFS_METADATA_FORMAT_PERFORMANCE.md)
- **Strategy Pattern Documentation**: [`doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`](METADATA_ARCHITECTURE_STRATEGY_PATTERN.md)
- **Build Guide**: [`docs/_references/build-configurations.adoc`](../docs/_references/build-configurations.adoc)

---

**End of Guide**