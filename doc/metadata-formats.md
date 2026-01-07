# DwarFS Metadata Formats

**Version**: v0.17.0
**Last Updated**: 2026-01-06

---

## Overview

DwarFS supports **three metadata serialization formats** to maximize compatibility across different deployment scenarios, build environments, and performance requirements.

### Format Summary

| Format | Status | Dependencies | Size | Speed | Portability |
|--------|--------|--------------|------|-------|-------------|
| **Legacy Thrift (Frozen2)** | ✅ Production | None | 100% | Fast | ⭐⭐⭐⭐⭐ |
| **FlatBuffers** | ✅ Production | Header-only | ~103% | Fastest | ⭐⭐⭐⭐⭐ |
| **Modern Thrift** | ✅ Production | Folly + fbthrift | 100% | Medium | ⭐⭐ |

---

## Format Comparison

### Legacy Thrift (Frozen2)

**Purpose**: Homebrew v0.14.1 backward compatibility without fbthrift

**Implementation**: Hand-coded Frozen2 bit-packed serializer (~2,500 lines across 8 files)

**Key Features**:
- ✅ **Zero dependencies**: No external libraries required
- ✅ **Smallest format**: Frozen2 bit-packing (100% baseline)
- ✅ **Full compatibility**: Byte-for-byte match with dwarfs-rs v0.14.x
- ✅ **Always available**: Compiled into every build
- ✅ **Fast**: Sequential parsing with efficient bit-level operations

**Technical Details**:
- **Magic Bytes**: None (fallback format)
- **File Extension**: `.dth` (recommended)
- **Priority**: 50 (lowest - used as fallback)
- **Architecture**: Three-phase serialization
  1. Layout Building: Analyze metadata structure
  2. Schema Generation: Create bit-packed layout schema
  3. Value Packing: Serialize values using schema

**Test Coverage**:
- ✅ SimpleStruct: 20 bytes (primitives)
- ✅ SmokeTest: 7 bytes (optional fields)
- ✅ BytesTest: 12 bytes (strings)
- ✅ CollectionTest: 28 bytes (vectors)

**When to Use**:
- Reading old Homebrew v0.14.1 images
- Portability without fbthrift complexity
- Environments where dependency minimization is critical

---

### FlatBuffers

**Purpose**: Modern default format with excellent portability

**Implementation**: FlatBuffers schema with standard serializer

**Key Features**:
- ✅ **Header-only**: FlatBuffers library auto-fetched
- ✅ **Memory-mappable**: Zero-copy access
- ✅ **Self-describing**: Schema embedded in data
- ✅ **Forward/backward compatible**: Schema evolution support
- ✅ **Fastest serialization**: Simple buffer writes

**Technical Details**:
- **Magic Bytes**: `"DFBF"` (DwarFs FlatBuffer)
- **File Extension**: `.dff` (recommended)
- **Priority**: 120 (highest - preferred format)
- **Size Overhead**: ~2.91% vs Thrift Compact
- **Format**: Size-prefixed buffer with schema

**Performance**:
- **Write**: Fastest (simple buffer operations)
- **Read**: Fast (zero-copy memory access)
- **Size**: 102.91% of Thrift Compact (minimal overhead)

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
- ✅ **Smallest metadata**: CompactProtocol bit-packing
- ✅ **Zero-copy access**: Frozen2 structures
- ✅ **Modern tooling**: Latest fbthrift v2025.12.29.00
- ⚠️ **Complex dependencies**: Folly, fizz, mvfst, wangle, fbthrift, jemalloc

**Technical Details**:
- **Magic Bytes**: `{0x82, 0x21}` (CompactProtocol)
- **File Extension**: `.dtc` (Thrift Compact)
- **Priority**: 100 (medium-high)
- **Size**: 100% baseline
- **Format**: CompactProtocol with schema

**Performance**:
- **Write**: Medium (CompactProtocol encoding)
- **Read**: Fast (Frozen2 zero-copy)
- **Size**: 100% (smallest possible)

**When to Use**:
- Absolute minimum metadata size required
- Existing Folly/fbthrift infrastructure
- Cutting-edge deployments with Facebook stack
- Size optimization critical

---

## Format Selection Guide

### Quick Decision Matrix

| If you... | Then use... | Because... |
|-----------|-------------|------------|
| **Are deploying new systems** | FlatBuffers | Best portability, minimal overhead |
| **Need Homebrew v0.14.1 compat** | Any format | Legacy Thrift always available |
| **Need minimum size** | Modern Thrift | Smallest format (100% baseline) |
| **Have Folly infrastructure** | Modern Thrift | Leverage existing stack |
| **Are developing DwarFS** | FlatBuffers + Modern Thrift | Test all formats |
| **Need zero dependencies** | Legacy Thrift fallback | Always available |
| **Are using Tebako** | FlatBuffers-only | Static linking requirement |

### Portability Comparison

| Platform | Legacy Thrift | FlatBuffers | Modern Thrift |
|----------|---------------|-------------|---------------|
| **Linux (all archs)** | ✅ Excellent | ✅ Excellent | ✅ Good |
| **macOS (Intel)** | ✅ Excellent | ✅ Excellent | ⚠️ Complex |
| **macOS (Apple Silicon)** | ✅ Excellent | ✅ Excellent | ⚠️ Complex |
| **Windows (x64)** | ✅ Excellent | ✅ Excellent | ❌ Difficult |
| **Windows (ARM64)** | ✅ Excellent | ✅ Excellent | ❌ Very difficult |
| **FreeBSD** | ✅ Excellent | ✅ Excellent | ⚠️ Complex |

---

## Performance Benchmarks

### Metadata Size (101 files, 156 KiB dataset)

| Format | Size | Relative | Notes |
|--------|------|----------|-------|
| **Legacy Thrift** | ~100 KB | 100% | Frozen2 bit-packing baseline |
| **Modern Thrift** | ~100 KB | 100% | CompactProtocol baseline |
| **FlatBuffers** | 103,135 bytes | 102.91% | Minimal overhead, excellent trade-off |

**Conclusion**: FlatBuffers adds only 2.91% overhead - negligible for the portability gain.

### Read Performance

| Format | Cold Cache | Warm Cache | Notes |
|--------|------------|------------|-------|
| **Legacy Thrift** | Fast | Fast | Sequential parsing |
| **FlatBuffers** | Fastest | Fastest | Zero-copy access |
| **Modern Thrift** | Fast | Fast | Frozen2 zero-copy |

### Write Performance

| Format | Speed | Notes |
|--------|-------|-------|
| **Legacy Thrift** | Fast | Efficient bit-packing |
| **FlatBuffers** | **Fastest** | Simple buffer writes |
| **Modern Thrift** | Medium | CompactProtocol encoding |

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
- ✅ Create/read FlatBuffers images (`.dff`)
- ✅ Read Legacy Thrift images (`.dth`) - always available
- ❌ Cannot create Modern Thrift images (`.dtc`)

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
- ❌ Cannot create FlatBuffers images
- ✅ Read Legacy Thrift images (`.dth`) - always available
- ✅ Create/read Modern Thrift images (`.dtc`)

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
- ✅ Create/read all three formats
- ✅ Cross-format conversion
- ⚠️ Sparse file seeking disabled (`SEEK_DATA`/`SEEK_HOLE`)

**Build Time**: Slowest (includes Folly)
**Use case**: Development, testing, mixed environments

---

## Migration Guide

### From Homebrew v0.14.1

**Old images** (`.dwarfs` or `.dft`):
- ✅ Work with any build configuration (Legacy Thrift always available)
- ✅ Can be mounted, checked, extracted
- ✅ Can be converted to FlatBuffers via `dwarfsextract` + `mkdwarfs`

**Recommended workflow**:
```bash
# Extract old image
dwarfsextract -i old-image.dwarfs -o /tmp/extracted

# Create new FlatBuffers image
mkdwarfs -i /tmp/extracted -o new-image.dff
```

---

### From FlatBuffers to Modern Thrift

```bash
# Extract FlatBuffers image
dwarfsextract -i image.dff -o /tmp/extracted

# Create Modern Thrift image (requires both-format build)
mkdwarfs -i /tmp/extracted -o image.dtc --format=thrift
```

---

### From Modern Thrift to FlatBuffers

```bash
# Extract Modern Thrift image
dwarfsextract -i image.dtc -o /tmp/extracted

# Create FlatBuffers image
mkdwarfs -i /tmp/extracted -o image.dff
```

---

## Architecture

### Strategy Pattern Implementation

All three formats share a **common domain model** with format-specific adapters:

```
┌─────────────────────────────────┐
│    Domain Model (metadata)      │
│    Format-agnostic business     │
│    logic operates here           │
└──────────────┬──────────────────┘
               │
    ┌──────────┴──────────┐
    │                     │
    ▼                     ▼
┌─────────┐         ┌─────────┐
│ Frozen2 │         │  Flat   │
│ Adapter │         │Buffers  │
│         │         │ Adapter │
└─────────┘         └─────────┘
                          │
                          ▼
                    ┌─────────┐
                    │ Thrift  │
                    │ Adapter │
                    └─────────┘
```

**Benefits**:
- ✅ Easy to add new formats
- ✅ Testable (mock interfaces)
- ✅ Maintainable (separation of concerns)
- ✅ Flexible (runtime format detection)

---

## File Extensions

| Extension | Format | Status | Recommendation |
|-----------|--------|--------|----------------|
| `.dff` | FlatBuffers | Modern | ✅ **Use for new images** |
| `.dtc` | Modern Thrift | Modern | Use for Thrift builds |
| `.dth` | Legacy Thrift | Legacy | Backward compatibility |
| `.dwarfs` | Auto-detect | Generic | Works, but ambiguous |

**Note**: Extensions are UI hints only - format detected via magic bytes.

---

## Troubleshooting

### "FlatBuffers metadata verification failed"

**Solution**: Use v0.16.0+ which increases verification limits:
- `max_depth`: 64 → 256
- `max_tables`: 1M → 10M

---

### "Cannot create .dtc images"

**Check**: Build configuration
```bash
# Verify Thrift support
build/mkdwarfs --version | grep -i thrift
```

**Solution**: Rebuild with Modern Thrift support enabled

---

### "Format detection failed"

**Reason**: Corrupted magic bytes or unknown format

**Solution**: Use format detection tool:
```bash
dwarfsck image.dff --print-header
```

---

## References

- **Main README**: [`README.md`](../README.md)
- **Build Guide**: [`vcpkg-build-guide.md`](vcpkg-build-guide.md)
- **Session 84 Summary**: [`SESSION_84_COMPLETION_SUMMARY.md`](SESSION_84_COMPLETION_SUMMARY.md)
- **Architecture**: [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md)

---

**Last Updated**: 2026-01-06
**Version**: v0.17.0
**Status**: All three formats production-ready