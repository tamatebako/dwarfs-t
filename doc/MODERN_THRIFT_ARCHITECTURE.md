# Modern Thrift Architecture (v0.17.0)

**Created**: 2026-01-06
**Status**: Architecture Design Complete
**Implementation**: Pending (Sessions 87-91)

---

## Overview

Modern Thrift is the third metadata serialization format in DwarFS v0.17.0, using **apache::thrift::CompactSerializer** from fbthrift v2025.12.29.00 to provide the smallest possible metadata size while maintaining modern tooling support.

### Three-Format Vision

| Format | Magic Bytes | Priority | File Ext | Dependencies | Size |
|--------|-------------|----------|----------|--------------|------|
| Legacy Thrift | NONE | 50 | `.dth` | None | 100% |
| Modern Thrift | `{0x82, 0x21}` | 100 | `.dtc` | Folly + fbthrift | 100% |
| FlatBuffers | `"DFBF"` | 120 | `.dff` | Header-only | ~103% |

**Key Point**: Modern Thrift and Legacy Thrift produce identical size (~100%), both smaller than FlatBuffers.

---

## Why Modern Thrift?

### vs Legacy Thrift (Frozen2)

**Legacy Thrift**:
- ✅ Zero dependencies (hand-coded ~2,500 lines)
- ✅ Homebrew v0.14.1 compatibility
- ✅ Always available
- ❌ No modern tooling support
- ❌ Maintenance burden (manual implementation)

**Modern Thrift**:
- ✅ Uses official fbthrift library
- ✅ Modern tooling and IDE support
- ✅ Automatic schema evolution
- ✅ Same size as Legacy Thrift
- ❌ Complex dependencies (Folly + fbthrift + jemalloc)

**Use Case**: Users with existing Folly/fbthrift infrastructure can use Modern Thrift for smallest size + modern tooling, while others can fall back to Legacy Thrift or use FlatBuffers.

### vs FlatBuffers

**FlatBuffers**:
- ✅ Header-only (excellent portability)
- ✅ Zero-copy access
- ✅ Simple build
- ⚠️ Slightly larger (~103% vs 100%)

**Modern Thrift**:
- ✅ Smallest size (100% baseline)
- ✅ Zero-copy access (via Frozen2-like structures)
- ⚠️ Complex dependencies
- ⚠️ Limited platform support

---

## Architecture

### Strategy Pattern Integration

Modern Thrift follows the same Strategy Pattern as FlatBuffers and Legacy Thrift:

```
┌─────────────────────────────────────────────────────┐
│        IMetadataSerializer (Strategy Interface)     │
│                                                     │
│  + serialize(metadata) → bytes                      │
│  + deserialize(bytes) → metadata                    │
│  + get_format_name() → string_view                  │
│  + get_format() → SerializationFormat               │
│  + can_write() → bool                               │
│  + can_read() → bool                                │
│  + get_magic_bytes() → vector<uint8_t>             │
└──────────────────┬──────────────────────────────────┘
                   │
      ┌────────────┴────────────┬────────────────┐
      │                         │                │
      ▼                         ▼                ▼
┌─────────────┐      ┌──────────────────┐  ┌──────────────┐
│  Legacy     │      │  FlatBuffers     │  │  Modern      │
│  Thrift     │      │  Serializer      │  │  Thrift      │
│  Serializer │      │                  │  │  Serializer  │
│             │      │  (ALWAYS)        │  │              │
│  (ALWAYS)   │      │  Priority: 120   │  │  (OPTIONAL)  │
│  Priority:  │      │  Magic: "DFBF"   │  │  Priority:   │
│  50         │      │                  │  │  100         │
│  Magic:     │      └──────────────────┘  │  Magic:      │
│  NONE       │                            │  {0x82,0x21} │
└─────────────┘                            └──────────────┘
```

**Priority Order** (format detection):
1. **FlatBuffers** (120) - Check for "DFBF" magic first
2. **Modern Thrift** (100) - Check for {0x82, 0x21} magic second
3. **Legacy Thrift** (50) - Fallback if no magic bytes found

---

## Data Flow

### Write Path (Serialization)

```
domain::metadata (format-agnostic)
       │
       ▼
domain_to_thrift() converter
       │
       ▼
thrift::metadata (generated from IDL)
       │
       ▼
apache::thrift::CompactSerializer::serialize<thrift::metadata>()
       │
       ▼
std::string serialized_data
       │
       ▼
prepend magic bytes {0x82, 0x21}
       │
       ▼
std::vector<uint8_t> final_bytes
```

### Read Path (Deserialization)

```
std::vector<uint8_t> bytes
       │
       ▼
verify magic bytes {0x82, 0x21}
       │
       ▼
strip magic bytes
       │
       ▼
apache::thrift::CompactSerializer::deserialize<thrift::metadata>()
       │
       ▼
thrift::metadata (generated types)
       │
       ▼
thrift_to_domain() converter
       │
       ▼
domain::metadata (format-agnostic)
```

---

## CompactProtocol Details

### Why CompactProtocol?

Apache Thrift supports multiple protocols:
- **BinaryProtocol**: Simple, fast, but larger
- **CompactProtocol**: Smaller, variable-length encoding
- **JSONProtocol**: Human-readable, largest

**CompactProtocol** chosen for:
- ✅ Smallest binary representation
- ✅ Efficient variable-length integer encoding
- ✅ Same size as Frozen2 (~100% baseline)

### Magic Bytes: {0x82, 0x21}

```
0x82 = 1000 0010
       │    │
       │    └─ field delta = 1 (first field)
       └────── field type = struct (10)

0x21 = 0010 0001
       │    │
       │    └─ field id delta
       └────── field type indicator
```

**Detection**: These bytes appear at the start of every CompactProtocol struct, making them reliable for format detection.

**Priority**: 100 (higher than Legacy Thrift 50, lower than FlatBuffers 120) ensures proper format detection order.

---

## Dependencies

### Required Libraries

**Folly** (Facebook's C++ library foundations):
- Version: v2025.12.29.00
- Required by fbthrift
- Provides: futures, format, dynamic, etc.

**fbthrift** (Facebook's Thrift implementation):
- Version: v2025.12.29.00
- Provides: `apache::thrift::CompactSerializer`
- Requires: Folly, fizz, mvfst, wangle

**jemalloc** (Memory allocator):
- Custom port: v5.3.0
- Required by Folly
- Provides: Better memory allocation performance

### vcpkg Overlay Ports

All dependencies managed via vcpkg overlay ports:

```
vcpkg_ports/
├── folly/           # Custom Folly port (jemalloc disabled for compatibility)
├── fbthrift/        # Custom fbthrift port
├── wangle/          # Custom wangle port (with TFO typo fix)
├── jemalloc/        # Custom jemalloc port
├── glog/            # Downgraded to 0.6.0 for compatibility
└── ...
```

**Build Command**:
```bash
cmake -B build -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DDWARFS_WITH_THRIFT=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON
```

---

## Build Configuration

### CMake Options

```cmake
# Enable Modern Thrift support (default: OFF)
option(DWARFS_WITH_THRIFT "Enable Modern Thrift serialization" OFF)

# Modern Thrift is OPTIONAL
# If ON: Find Folly, fbthrift, add define DWARFS_HAVE_THRIFT
# If OFF: Skip Modern Thrift, no define
```

### Conditional Compilation

```cpp
#ifdef DWARFS_HAVE_THRIFT
  // Modern Thrift code here
  #include "dwarfs/metadata/serialization/modern_thrift_serializer.h"
  register_modern_thrift_serializer();
#endif
```

### Build Configurations

**1. FlatBuffers-only** (minimal dependencies):
```bash
cmake -B build -DDWARFS_WITH_THRIFT=OFF -DDWARFS_WITH_FLATBUFFERS=ON
# Can read: .dff, .dth (Legacy Thrift)
# Can write: .dff
```

**2. Modern Thrift-only** (smallest size):
```bash
cmake -B build -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=OFF
# Can read: .dtc, .dth (Legacy Thrift)
# Can write: .dtc
```

**3. All formats** (development):
```bash
cmake -B build -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=ON
# Can read: .dff, .dtc, .dth
# Can write: .dff, .dtc
```

---

## Implementation Plan

### Session 87: Thrift Schema Definition
- Create `thrift/metadata_modern.thrift` IDL
- Generate C++ types via fbthrift compiler
- Write `domain_to_thrift()` converter
- Write `thrift_to_domain()` converter

### Session 88: CompactProtocol Serialization
- Implement `ModernThriftSerializer::serialize()`
- Implement `ModernThriftSerializer::deserialize()`
- Add magic byte handling
- Register with SerializerRegistry

### Session 89: Testing
- Unit tests (simple, complex, round-trip)
- Integration tests (mkdwarfs, dwarfsck, dwarfsextract, FUSE)
- Cross-format tests (FlatBuffers ↔ Modern Thrift ↔ Legacy Thrift)
- Performance benchmarks (size, speed)

### Session 90: Build System Integration
- Update `cmake/metadata_serialization.cmake`
- Add vcpkg dependencies
- Test all build configurations
- Update CI/CD workflows

### Session 91: Documentation & Release
- Update README.md
- Update metadata-formats.md
- Create v0.17.0 release notes
- Archive session docs

---

## Key Design Decisions

### 1. CompactProtocol vs BinaryProtocol

**Decision**: Use CompactProtocol

**Rationale**:
- Smallest possible size (matches Frozen2 ~100%)
- Variable-length integer encoding
- Industry-standard format

### 2. Magic Bytes {0x82, 0x21}

**Decision**: Use CompactProtocol struct header as magic bytes

**Rationale**:
- Natural magic bytes (part of protocol)
- Reliable for format detection
- Priority 100 (between Legacy 50 and FlatBuffers 120)

### 3. Optional vs Required

**Decision**: Modern Thrift is OPTIONAL (default: OFF)

**Rationale**:
- Complex dependencies (Folly + fbthrift)
- Most users prefer FlatBuffers (simpler) or Legacy Thrift (zero deps)
- Only enable for users with existing Folly infrastructure

### 4. Priority: 100

**Decision**: Medium-high priority (between Legacy 50 and FlatBuffers 120)

**Rationale**:
- Check FlatBuffers first (most common, recommended default)
- Check Modern Thrift second (specific use case)
- Fall back to Legacy Thrift last (no magic bytes)

---

## Performance Characteristics

### Expected Performance (based on CompactProtocol specs)

**Size**:
- Modern Thrift: 100% baseline (same as Legacy Thrift Frozen2)
- FlatBuffers: ~103% (2.91% overhead)

**Serialization Speed**:
- Modern Thrift: Medium (CompactProtocol encoding)
- FlatBuffers: Fastest (simple buffer writes)
- Legacy Thrift: Fast (efficient bit-packing)

**Deserialization Speed**:
- Modern Thrift: Fast (zero-copy Frozen2-like structures)
- FlatBuffers: Fast (zero-copy memory access)
- Legacy Thrift: Fast (zero-copy Frozen2 access)

---

## File Extensions

| Extension | Format | Priority | Recommendation |
|-----------|--------|----------|----------------|
| `.dff` | FlatBuffers | 120 | ✅ **Default for new images** |
| `.dtc` | Modern Thrift Compact | 100 | Use if you have Folly infrastructure |
| `.dth` | Legacy Thrift (Frozen2) | 50 | Backward compatibility only |
| `.dwarfs` | Auto-detect | - | Works but ambiguous (avoid) |

---

## Migration Paths

### From Legacy Thrift to Modern Thrift

```bash
# Extract Legacy Thrift image
dwarfsextract -i old.dth -o /tmp/extracted

# Create Modern Thrift image
mkdwarfs -i /tmp/extracted -o new.dtc --format=thrift
```

### From FlatBuffers to Modern Thrift

```bash
# Extract FlatBuffers image
dwarfsextract -i image.dff -o /tmp/extracted

# Create Modern Thrift image
mkdwarfs -i /tmp/extracted -o image.dtc --format=thrift
```

---

## Testing Strategy

### Unit Tests
- Simple metadata serialization
- Complex metadata with all features
- Round-trip (domain → thrift → domain)
- Magic byte detection
- Error handling

### Integration Tests
- Create image with Modern Thrift via mkdwarfs
- Check image with dwarfsck
- Extract image with dwarfsextract
- Mount image with dwarfs FUSE driver

### Cross-Format Tests
- Convert FlatBuffers → Modern Thrift
- Convert Legacy Thrift → Modern Thrift
- Convert Modern Thrift → FlatBuffers
- Verify metadata preservation

### Performance Benchmarks
- Serialization speed vs FlatBuffers
- Deserialization speed vs FlatBuffers
- Size comparison with all three formats

---

## References

- **fbthrift Documentation**: https://github.com/facebook/fbthrift
- **Folly Documentation**: https://github.com/facebook/folly
- **CompactProtocol Spec**: https://github.com/apache/thrift/blob/master/doc/specs/thrift-compact-protocol.md
- **Session 86 Status**: [`SESSION_86_IMPLEMENTATION_STATUS.md`](SESSION_86_IMPLEMENTATION_STATUS.md)
- **Metadata Formats Guide**: [`metadata-formats.md`](metadata-formats.md)

---

**Last Updated**: 2026-01-06
**Status**: Architecture complete, ready for implementation
**Next Session**: 87 (Thrift Schema Definition)