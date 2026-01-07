# DwarFS Metadata Formats Reference

**Version**: v0.17.0
**Last Updated**: 2026-01-06

---

## Overview

DwarFS supports **three metadata serialization formats** to maximize compatibility across different deployment scenarios, build environments, and performance requirements.

### Format Summary

| Format | Status | Dependencies | Size | Speed | Portability |
|--------|--------|--------------|------|-------|-------------|
| **Legacy Thrift (Frozen2)** | Production | None | 100% | Fast | Excellent |
| **FlatBuffers** | Production | Header-only | ~103% | Fastest | Excellent |
| **Modern Thrift** | Production | Folly + fbthrift | 100% | Medium | Limited |

---

## Format Comparison

### Legacy Thrift (Frozen2)

**Purpose**: Homebrew v0.14.1 backward compatibility without fbthrift

**Implementation**: Hand-coded Frozen2 bit-packed serializer (~2,500 lines across 8 files)

**Key Features**:
- Zero dependencies: No external libraries required
- Smallest format: Frozen2 bit-packing (100% baseline)
- Full compatibility: Byte-for-byte match with dwarfs-rs v0.14.x
- Always available: Compiled into every build
- Fast: Sequential parsing with efficient bit-level operations

**Technical Details**:
- Magic Bytes: None (fallback format)
- File Extension: `.dth` (recommended)
- Priority: 50 (lowest - used as fallback)

**When to Use**:
- Reading old Homebrew v0.14.1 images
- Portability without fbthrift complexity
- Environments where dependency minimization is critical

---

### FlatBuffers

**Purpose**: Modern default format with excellent portability

**Implementation**: FlatBuffers schema with standard serializer

**Key Features**:
- Header-only: FlatBuffers library auto-fetched
- Memory-mappable: Zero-copy access
- Self-describing: Schema embedded in data
- Forward/backward compatible: Schema evolution support
- Fastest serialization: Simple buffer writes

**Technical Details**:
- Magic Bytes: `"DFBF"` (DwarFs FlatBuffer)
- File Extension: `.dff` (recommended)
- Priority: 120 (highest - preferred format)
- Size Overhead: ~2.91% vs Thrift Compact

**Performance**:
- Write: Fastest (simple buffer operations)
- Read: Fast (zero-copy memory access)
- Size: 102.91% of Thrift Compact (minimal overhead)

**When to Use**:
- **Default for all new images**
- Maximum portability required
- Static linking scenarios (Tebako)
- Header-only dependencies preferred
- Cross-platform development

---

### Modern Thrift (CompactProtocol)

**Purpose**: Modern Thrift serialization with Folly/fbthrift

**Implementation**: CompactProtocol serializer with Frozen2-like packing

**Key Features**:
- Smallest metadata: CompactProtocol bit-packing
- Zero-copy access: Frozen2 structures
- Modern tooling: Latest fbthrift v2025.12.29.00
- Complex dependencies: Folly, fizz, mvfst, wangle, fbthrift, jemalloc

**Technical Details**:
- Magic Bytes: `{0x82, 0x21}` (CompactProtocol)
- File Extension: `.dtc` (Thrift Compact)
- Priority: 100 (medium-high)

**When to Use**:
- Absolute minimum metadata size required
- Existing Folly/fbthrift infrastructure
- Cutting-edge deployments with Facebook stack

---

## Architecture

### Strategy Pattern Implementation

All three formats share a **common domain model** with format-specific adapters:

```
Domain Model (metadata)
    Format-agnostic business logic
         |
    +----+----+
    |         |
    v         v
Frozen2    FlatBuffers
Adapter    Adapter
               |
               v
           Thrift
           Adapter
```

**Benefits**:
- Easy to add new formats
- Testable (mock interfaces)
- Maintainable (separation of concerns)
- Flexible (runtime format detection)

### Domain Model Design

The domain model represents metadata in a **format-agnostic** way using native C++ types. All business logic operates on this model, completely independent of serialization format.

**Location**: `include/dwarfs/metadata/domain/metadata.h`

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
  std::optional<std::vector<dir_entry>> dir_entries;
  std::optional<std::vector<uint32_t>> shared_files_table;
  std::optional<fs_options> options;

  // Metadata
  uint32_t block_size;
  uint64_t total_fs_size;
  uint64_t timestamp_base;
  // ... more fields
};

} // namespace metadata::domain
```

---

## Performance Benchmarks

### Metadata Size (101 files, 156 KiB dataset)

| Format | Size | Relative | Notes |
|--------|------|----------|-------|
| **Legacy Thrift** | ~100 KB | 100% | Frozen2 bit-packing baseline |
| **Modern Thrift** | ~100 KB | 100% | CompactProtocol baseline |
| **FlatBuffers** | 103,135 bytes | 102.91% | Minimal overhead |

**Conclusion**: FlatBuffers adds only 2.91% overhead - negligible for the portability gain.

### Compression Performance (Perl 5.43.3, 96.51 MiB)

| Level | FlatBuffers | Thrift | Speedup |
|-------|-------------|--------|---------|
| 1 (fast) | 1.489s | 2.095s | **+28.9%** |
| 3 (default) | 2.999s | 3.617s | **+17.1%** |
| 9 (max) | 27.043s | 26.606s | ~Equal |

**Conclusion**: FlatBuffers is **significantly faster** at typical compression levels.

### Extraction Performance

| Metric | FlatBuffers | Thrift | Delta |
|--------|-------------|--------|-------|
| Time (level 3) | 2.069s | 1.998s | +3.4% (~equal) |
| Content | Identical | Identical | Byte-for-byte match |

---

## Build Configurations

### FlatBuffers-Only (Recommended)

```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
ninja -C build
```

**Capabilities**:
- Create/read FlatBuffers images (`.dff`)
- Read Legacy Thrift images (`.dth`) - always available
- Cannot create Modern Thrift images (`.dtc`)

**Build Time**: Fast (~5 minutes with vcpkg)
**Dependencies**: Header-only FlatBuffers
**Portability**: Excellent (all platforms)

---

### Modern Thrift-Only

```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON
ninja -C build
```

**Capabilities**:
- Cannot create FlatBuffers images
- Read Legacy Thrift images (`.dth`) - always available
- Create/read Modern Thrift images (`.dtc`)

**Build Time**: Slow (~30-60 minutes first build)
**Dependencies**: Folly, fbthrift, jemalloc, etc.
**Portability**: Limited (complex build)

---

### All Formats

```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON
ninja -C build
```

**Capabilities**:
- Create/read all three formats
- Cross-format conversion
- Sparse file seeking disabled (`SEEK_DATA`/`SEEK_HOLE`)

---

## File Extensions

| Extension | Format | Status | Recommendation |
|-----------|--------|--------|----------------|
| `.dff` | FlatBuffers | Modern | **Use for new images** |
| `.dtc` | Modern Thrift | Modern | Use for Thrift builds |
| `.dth` | Legacy Thrift | Legacy | Backward compatibility |
| `.dwarfs` | Auto-detect | Generic | Works, but ambiguous |

**Note**: Extensions are UI hints only - format detected via magic bytes.

---

## Migration Guide

### From Homebrew v0.14.1

**Old images** (`.dwarfs` or `.dft`):
- Work with any build configuration (Legacy Thrift always available)
- Can be mounted, checked, extracted
- Can be converted to FlatBuffers via `dwarfsextract` + `mkdwarfs`

```bash
# Extract old image
dwarfsextract -i old-image.dwarfs -o /tmp/extracted

# Create new FlatBuffers image
mkdwarfs -i /tmp/extracted -o new-image.dff
```

### Format Conversion

```bash
# Any format to FlatBuffers
dwarfsextract -i image.dtc -o /tmp/extracted
mkdwarfs -i /tmp/extracted -o image.dff

# FlatBuffers to Modern Thrift (requires both-format build)
dwarfsextract -i image.dff -o /tmp/extracted
mkdwarfs -i /tmp/extracted -o image.dtc --format=thrift
```

---

## Troubleshooting

### "FlatBuffers metadata verification failed"

**Solution**: Use v0.16.0+ which increases verification limits:
- `max_depth`: 64 -> 256
- `max_tables`: 1M -> 10M

### "Cannot create .dtc images"

**Check**: Build configuration
```bash
build/mkdwarfs --version | grep -i thrift
```

**Solution**: Rebuild with Modern Thrift support enabled

### "Format detection failed"

**Reason**: Corrupted magic bytes or unknown format

**Solution**:
```bash
dwarfsck image.dff --print-header
```

---

## Implementation Files

### Domain Model
- **Header**: `include/dwarfs/metadata/domain/metadata.h`
- **Views**: `include/dwarfs/reader/internal/domain_metadata_views.h`
- **Implementation**: `src/reader/internal/domain_metadata_views.cpp`

### Writer Implementations
- **FlatBuffers**: `src/writer/flatbuffers_metadata_writer.cpp`
- **Thrift**: `src/writer/thrift_metadata_writer.cpp`

### Reader Implementations
- **FlatBuffers**: `src/reader/internal/metadata_v2_flatbuffers.cpp`
- **Thrift**: `src/reader/internal/metadata_v2_thrift.cpp`

### Serialization
- **FlatBuffers**: `src/metadata/serialization/flatbuffers_serializer.cpp`
- **Thrift**: `src/metadata/serialization/thrift_compact_serializer.cpp`
- **Legacy**: `src/metadata/legacy/frozen2_deserializer.cpp`

---

## See Also

- [Architecture Overview](architecture.md) - System architecture
- [Building DwarFS](building.md) - Build instructions
- [Benchmark Guide](benchmarking.md) - Performance benchmarking
