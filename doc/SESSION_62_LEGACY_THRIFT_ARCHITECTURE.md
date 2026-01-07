# Session 62: Legacy Thrift Format Architecture

**Purpose**: Port hand-coded Thrift serialization from dwarfs-rs to create "legacy format" support
**Status**: 📋 **ARCHITECTURAL DESIGN**
**Created**: 2025-12-31 23:41 HKT

---

## Executive Summary

**Problem**: fbthrift dependency causes cascading version compatibility issues
**Solution**: Port dwarfs-rs's hand-coded Thrift Compact implementation to C++
**Benefit**: No Folly/fbthrift dependencies for legacy format, full v0.14.1 compatibility

---

## Three-Format Strategy

### Format 1: FlatBuffers (Modern Default) ✅
**Status**: WORKING
**Dependencies**: FlatBuffers (header-only)
**Use case**: All new images
**File extension**: `.dff`

### Format 2: Legacy Thrift (Hand-coded) 🆕
**Status**: TO BE IMPLEMENTED
**Dependencies**: NONE (hand-coded varint/zigzag/field encoding)
**Use case**: Read v0.14.1 images, write for compatibility testing
**File extension**: `.dft.legacy` or keep `.dft`
**Code size**: ~600 lines (ported from dwarfs-rs)

### Format 3: Modern Thrift (fbthrift) 🔮
**Status**: FUTURE (when vcpkg stable releases work)  
**Dependencies**: Folly + fbthrift (static)
**Use case**: Advanced Thrift features (if needed)
**File extension**: `.dft.modern` or versioned

---

## Architecture: Porting from dwarfs-rs

### Source Analysis

**dwarfs-rs Implementation** (`/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/`):

1. **`de_thrift.rs`** (273 lines):
   - Deserializer for Thrift Compact protocol
   - Implements: varint, zigzag, struct, map, string, bool, i16, i32
   - Uses Rust serde framework

2. **`ser_thrift.rs`** (322 lines):
   - Serializer for Thrift Compact protocol
   - Mirror implementation of deserializer
   - No fbthrift dependency

3. **`de_frozen.rs`** + **`ser_frozen.rs`**:
   - Frozen2 layout support
   - Schema-driven bit-packed structures

4. **`metadata.rs`** (602 lines):
   - Metadata structure definitions
   - `DenseMap<T>`, `OrderedSet<T>`, `OrderedMap<K,V>` helpers
   - Parse/serialize functions

### Key Features

**Minimal Type Support**:
- `struct` (field id delta encoding)
- `map` (key/value type byte)
- `string` (varint length + bytes)
- `bool` (inline in field tag or separate byte)
- `i16`, `i32` (zigzag encoded)

**What's NOT needed**:
- ❌ `i8`, `i64`, `u8`, `u16`, `u32`, `u64`
- ❌ `float`, `double`
- ❌ `list`, `set`
- ❌ `optional`, `union`

**Encoding Details**:
- Varint: 7-bit encoding with continuation bit (`(v & 0x7F) | (more << 7)`)
- Zigzag: Sign encoding (`(v << 1) ^ (v >> 31)`)
- Field delta: Upper 4 bits of field tag (0x10, 0x20, 0x30, etc.)

---

## C++ Implementation Plan

### Phase 1: Core Thrift Primitives (2-3 hours)

**Files to Create**:
```
include/dwarfs/metadata/legacy/
├── thrift_compact_writer.h   # Serialization
├── thrift_compact_reader.h   # Deserialization
└── thrift_types.h            # Tag enum, helpers

src/metadata/legacy/
├── thrift_compact_writer.cpp
├── thrift_compact_reader.cpp
└── thrift_types.cpp
```

**`thrift_compact_writer.h`**:
```cpp
namespace dwarfs::metadata::legacy {

class ThriftCompactWriter {
public:
  explicit ThriftCompactWriter(std::vector<uint8_t>& buffer);
  
  // Primitives
  void write_varint(uint32_t v);
  void write_zigzag(int32_t v);
  void write_bool(bool v);
  void write_i16(int16_t v);
  void write_i32(int32_t v);
  void write_string(std::string_view s);
  
  // Containers
  void begin_struct();
  void write_field(uint16_t field_id, auto const& value);
  void end_struct();
  
  void begin_map(uint32_t size);
  void write_map_entry(auto const& key, auto const& value);
  void end_map();

private:
  std::vector<uint8_t>& buf_;
  uint8_t field_id_diff_tag_{0x10};
};

} // namespace dwarfs::metadata::legacy
```

**`thrift_compact_reader.h`**:
```cpp
namespace dwarfs::metadata::legacy {

class ThriftCompactReader {
public:
  explicit ThriftCompactReader(std::span<uint8_t const> data);
  
  // Primitives
  uint32_t read_varint();
  int32_t read_zigzag();
  bool read_bool();
  int16_t read_i16();
  int32_t read_i32();
  std::string_view read_string();
  
  // Containers  
  void begin_struct();
  std::optional<uint16_t> read_field_header(Tag& out_type);
  void end_struct();
  
  uint32_t begin_map(Tag& out_ktype, Tag& out_vtype);
  void end_map();
  
  bool at_end() const;

private:
  std::span<uint8_t const> data_;
  size_t pos_{0};
};

} // namespace dwarfs::metadata::legacy
```

### Phase 2: Metadata Structures (1-2 hours)

**Port Rust structs to C++**:

From `metadata.rs`:
```rust
pub struct Metadata {
    pub chunks: Vec<Chunk>,
    pub directories: Vec<Directory>,
    pub inodes: Vec<InodeData>,
    // ... 26 fields total
}
```

To C++ (use existing DwarFS domain model):
```cpp
// We ALREADY have these in include/dwarfs/metadata/domain/metadata.h!
// Just need serialize/deserialize functions
```

**Key Insight**: dwarfs already has domain model in `include/dwarfs/metadata/domain/metadata.h`. We just need legacy serializer/deserializer that works with it!

### Phase 3: Frozen2 Support (2-3 hours)

**Port `de_frozen.rs` and `ser_frozen.rs`**:

```
include/dwarfs/metadata/legacy/
├── frozen_layout.h           # Schema structures
├── frozen_reader.h          # Bit-packed reader
└── frozen_writer.h          # Bit-packed writer

src/metadata/legacy/
├── frozen_layout.cpp
├── frozen_reader.cpp
└── frozen_writer.cpp
```

**Frozen2 Concept**:
- Schema defines bit-packed layout
- Metadata stored according to schema layout
- Two sections: Schema (Thrift) + Metadata (Frozen)

### Phase 4: Integration (1-2 hours)

**Create facade**:
```cpp
namespace dwarfs::metadata::serialization {

class LegacyThriftFacade : public MetadataSerializationFacade {
public:
  void serialize(domain::metadata const&, byte_buffer&) override;
  void deserialize(span<uint8_t const>, domain::metadata&) override;
  SerializationFormat get_format() const override { 
    return SerializationFormat::LEGACY_THRIFT; 
  }
};

} // namespace
```

**Register in registry**:
```cpp
// In serializer_registry.cpp
registry.register_format(
  SerializationFormat::LEGACY_THRIFT,
  std::make_unique<LegacyThriftFacade>()
);
```

---

## Comparison with Existing Implementation

### Current (Broken)
```
Domain Model
     ↕️
cpp_thrift_converter (BUGGY - asymmetric)
     ↕️
Folly/fbthrift Types (Frozen2)
     ↕️
fbthrift Library (VERSION HELL)
     ↕️
Wire Format
```

### Legacy Format (Proposed)
```
Domain Model
     ↕️
LegacyThriftSerializer/Deserializer (HAND-CODED)
     ↕️
Wire Format (Thrift Compact + Frozen2)
```

**Advantages**:
- ✅ No fbthrift dependency
- ✅ No version compatibility issues
- ✅ Direct control over serialization
- ✅ ~50% less code than converter approach
- ✅ Easier to debug and test

---

## Implementation Estimate

| Phase | Task | LOC | Time |
|-------|------|-----|------|
| **Phase 1** | Thrift Compact primitives | ~400 | 2-3 hours |
| **Phase 2** | Metadata serialization | ~200 | 1-2 hours |
| **Phase 3** | Frozen2 support | ~600 | 2-3 hours |
| **Phase 4** | Integration + testing | ~200 | 1-2 hours |
| **TOTAL** | | **~1400** | **6-10 hours** |

**Note**: Much of the complexity is already handled by dwarfs-rs - we're translating, not designing from scratch

---

## Format Compatibility

### Reading Path

**Legacy Thrift Images** (v0.14.1 Homebrew, our legacy writer):
```
Wire bytes → LegacyThriftReader → Domain Model → FlatBuffers/Thrift/Legacy writers
```

**Modern Thrift Images** (if/when we add fbthrift):
```
Wire bytes → ModernThriftReader (fbthrift) → Domain Model → Any writer
```

**Cross-compatibility**:
- Legacy reader can read v0.14.1 images ✅
- Legacy writer produces v0.14.1-compatible images ✅
- Modern Thrift is future enhancement (not blocking)

### Three-Way Compatibility Matrix

| Reader | FlatBuffers | Legacy Thrift | Modern Thrift |
|--------|-------------|---------------|---------------|
| **FlatBuffers** | ✅ Native | N/A | N/A |
| **Legacy Thrift** | ✅ Via domain | ✅ Native | ✅ Compatible* |
| **Modern Thrift** | ✅ Via domain | ✅ Compatible* | ✅ Native |

*Via domain model conversion

---

## Testing Strategy

### Unit Tests (Per Component)

**Thrift Compact Primitives**:
```cpp
TEST(ThriftCompactWriter, Varint) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);
  w.write_varint(0);
  w.write_varint(127);
  w.write_varint(128);
  w.write_varint(16383);
  // Compare with dwarfs-rs test vectors
}

TEST(ThriftCompactReader, Varint) {
  uint8_t data[] = {0x00, 0x7F, 0x80, 0x01, 0xFF, 0x7F};
  ThriftCompactReader r(data);
  EXPECT_EQ(r.read_varint(), 0);
  EXPECT_EQ(r.read_varint(), 127);
  EXPECT_EQ(r.read_varint(), 128);
  EXPECT_EQ(r.read_varint(), 16383);
}
```

**Frozen2 Layout**:
```cpp
TEST(FrozenReader, SimpleBitPacked) {
  // Test schema: struct with 2 i16 fields (16 bits each)
  // Create test data
  // Parse with FrozenReader
  // Verify field values
}
```

### Integration Tests (Round-trip)

**Legacy Format Round-trip**:
```cpp
TEST(LegacyThrift, MetadataRoundTrip) {
  // Create domain::metadata with test data
  domain::metadata original = create_test_metadata();
  
  // Serialize to legacy format
  LegacyThriftFacade facade;
  byte_buffer serialized;
  facade.serialize(original, serialized);
  
  // Deserialize back
  domain::metadata deserialized;
  facade.deserialize(serialized.span(), deserialized);
  
  // Verify equality
  EXPECT_EQ(original, deserialized);
}
```

**Cross-format Compatibility**:
```cpp
TEST(FormatCompatibility, Legacy To FlatBuffers) {
  // Serialize with legacy
  // Deserialize to domain model
  // Serialize with FlatBuffers
  // Deserialize FlatBuffers
  // Verify equality
}
```

### Compatibility Tests (v0.14.1)

**Test with actual Homebrew v0.14.1 images**:
```cpp
TEST(LegacyThrift, ReadHomebrew_v0_14_1) {
  // Load image created by Homebrew mkdwarfs v0.14.1
  auto bytes = load_test_file("homebrew-v0.14.1.dft");
  
  // Parse with our legacy reader
  LegacyThriftFacade facade;
  domain::metadata meta;
  facade.deserialize(bytes, meta);
  
  // Verify all fields correct
  EXPECT_EQ(meta.block_size, expected_block_size);
  // ... more assertions
}

TEST(LegacyThrift, WriteCompatibleWithHomebrew) {
  // Create metadata
  // Serialize with our legacy writer
  
  // Verify Homebrew can read it (manual test with actual tool):
  // $ /opt/homebrew/bin/dwarfsck -i our-legacy.dft
  // $ /opt/homebrew/bin/dwarfsextract -i our-legacy.dft -o /tmp/out
}
```

---

## Implementation Roadmap

### Session 62: Core Primitives (3 hours)

**Goal**: Implement and test Thrift Compact encoding/decoding

1. Create `thrift_compact_writer.h/.cpp`
2. Create `thrift_compact_reader.h/.cpp`  
3. Implement varint/zigzag encoding
4. Write unit tests (compare with dwarfs-rs)
5. Verify byte-for-byte compatibility

**Deliverable**: Working primitives with tests

### Session 63: Metadata Serialization (3 hours)

**Goal**: Serialize/deserialize domain::metadata using Thrift Compact

1. Implement struct serialization (field delta encoding)
2. Implement map serialization (type byte)
3. Wire up to domain::metadata fields
4. Write round-trip tests
5. Test with real Homebrew v0.14.1 images

**Deliverable**: Legacy Thrift serializer/deserializer

### Session 64: Frozen2 Support (4 hours)

**Goal**: Add Schema + Frozen2 bit-packing

1. Port Schema structures from dwarfs-rs
2. Implement Frozen2 reader (bit-packed access)
3. Implement Frozen2 writer (bit-packing)
4. Integration with metadata serialization
5. Full compatibility testing

**Deliverable**: Complete legacy Thrift format support

### Session 65: Integration & Testing (2 hours)

**Goal**: Wire into dwarfs build system and tools

1. Create `LegacyThriftFacade`
2. Register in `serializer_registry`
3. Update format detection logic
4. Run full test suite
5. Verify Homebre w v0.14.1 compatibility

**Deliverable**: Prod uction-ready legacy format

---

## Technical Comparison

### dwarfs-rs Rust Code
```rust
// Varint encoding
fn write_varint(&mut self, mut v: u32) {
    loop {
        let more = v >> 7;
        let has_more = more > 0;
        self.w.push((v as u8 & 0x7F) | ((has_more as u8) << 7));
        v = more;
        if !has_more { break; }
    }
}
```

### Proposed C++ Port
```cpp
// Varint encoding
void ThriftCompactWriter::write_varint(uint32_t v) {
  do {
    uint32_t more = v >> 7;
    bool has_more = more > 0;
    buf_.push_back(static_cast<uint8_t>(v & 0x7F) | (has_more << 7));
    v = more;
  } while (v > 0);
}
```

**Complexity**: Direct 1:1 translation from Rust to C++

---

## Benefits Over fbthrift Approach

### Development Speed
- **fbthrift**: 4+ hours debugging, still blocked
- **Legacy**: ~10 hours total, guaranteed to work

### Maintainability
- **fbthrift**: Cascading version dependencies
- **Legacy**: Self-contained, no external deps

### Compatibility
- **fbthrift**: Requires matching Folly version
- **Legacy**: Works with any Folly version (no dependency)

### Code Size
- **fbthrift**: Converter + wrapper (~2000 lines)
- **Legacy**: Direct implementation (~1400 lines)

### Debugging
- **fbthrift**: Abstract Frozen2 library
- **Legacy**: Full control, easy to trace

---

## Migration Path

### Phase 1: Add Legacy Format (Sessions 62-65)
```
Before:
- FlatBuffers ✅
- Thrift      🔴 BROKEN (fbthrift issues)

After:
- FlatBuffers ✅
- Legacy Thrift ✅ (hand-coded, no deps)
- Modern Thrift ❌ REMOVED (temporarily)
```

### Phase 2: Restore Modern Thrift (Future, optional)
```
When vcpkg stable releases work:
- FlatBuffers ✅
- Legacy Thrift ✅ (backward compatibility)
- Modern Thrift ✅ (advanced features)
```

### Benefits of This Path
1. Unblocks v0.14.1 compatibility **NOW** (10 hours)
2. No fbthrift dependency hell
3. Can add Modern Thrift later when vcpkg stable
4. Clean architectural separation

---

## File Organization

### Directory Structure
```
include/dwarfs/metadata/
├── domain/                    # Existing domain model ✅
├── serialization/             # Existing facade/registry ✅
└── legacy/                    # NEW - Hand-coded Thrift
    ├── thrift_compact_writer.h
    ├── thrift_compact_reader.h
    ├── frozen_schema.h
    ├── frozen_reader.h
    └── frozen_writer.h

src/metadata/
├── domain/                    # Existing ✅
├── serialization/             # Existing ✅
├── converters/                # REMOVE (replace with legacy)
└── legacy/                    # NEW
    ├── thrift_compact_writer.cpp
    ├── thrift_compact_reader.cpp
    ├── frozen_schema.cpp
    ├── frozen_reader.cpp
    └── frozen_writer.cpp
```

### Build System Changes

**CMake** (`cmake/metadata_serialization.cmake`):
```cmake
# Legacy Thrift (always available, no external deps)
add_library(dwarfs_metadata_legacy
  src/metadata/legacy/thrift_compact_writer.cpp
  src/metadata/legacy/thrift_compact_reader.cpp
  src/metadata/legacy/frozen_schema.cpp
  src/metadata/legacy/frozen_reader.cpp
  src/metadata/legacy/frozen_writer.cpp
)
target_compile_definitions(dwarfs_metadata_legacy
  PUBLIC DWARFS_HAVE_LEGACY_THRIFT=1
)

# Modern Thrift (optional, fbthrift-based)
if(DWARFS_WITH_THRIFT)
  # ... existing fbthrift setup
  target_compile_definitions(dwarfs_metadata_thrift
    PUBLIC DWARFS_HAVE_MODERN_THRIFT=1
  )
endif()
```

---

## Format Detection Logic

### Magic Bytes Strategy

Update `serializer_registry.cpp`:
```cpp
std::optional<SerializationFormat> detect_format(span<uint8_t const> data) {
  // FlatBuffers: "DFBF" identifier
  if (has_flatbuffers_magic(data)) {
    return SerializationFormat::FLATBUFFERS;
  }
  
  // Legacy Thrift: No magic, try parse as Thrift Compact
  // (v0.14.1 images don't have magic bytes)
  if (can_parse_as_legacy_thrift(data)) {
    return SerializationFormat::LEGACY_THRIFT;
  }
  
  // Modern Thrift: Custom magic "DFTM" (future)
  if (has_modern_thrift_magic(data)) {
    return SerializationFormat::MODERN_THRIFT;
  }
  
  return std::nullopt;
}
```

### Backward Compatibility
- Old images (no magic): Detected as Legacy Thrift ✅
- FlatBuffers images: Magic bytes take precedence ✅
- Future Modern Thrift: Different magic byte ✅

---

## Success Criteria

### Technical
- [ ] All Thrift Compact primitives work (varint, zigzag, etc.)
- [ ] Struct/map serialization matches dwarfs-rs behavior
- [ ] Frozen2 schema parse/generate works
- [ ] Round-trip tests pass (serialize → deserialize → equal)
- [ ] No external Thrift dependencies

### Compatibility
- [ ] Can read Homebrew v0.14.1 images
- [ ] Can write images Homebrew v0.14.1 can read
- [ ] Cross-format conversion works (Legacy ↔ FlatBuffers)
- [ ] All existing tests pass with legacy format

### Code Quality
- [ ] Comprehensive unit tests
- [ ] Integration tests  
- [ ] Documentation
- [ ] Clean separation from FlatBuffers code

---

## Risks & Mitigations

### Risk: Rust→C++ Translation Errors

**Mitigation**:
- Use dwarfs-rs test vectors
- Byte-for-byte comparison
- Cross-test with actual Homebre w images

### Risk: Frozen2 Complexity

**Mitigation**:
- Start with simple non-packed structures
- Add bit-packing incrementally
- Extensive testing at each step

### Risk: Missing Edge Cases

**Mitigation**:
- Test with diverse real-world images
- Fuzzing (if time permits)
- Compare with fbthrift behavior (when working)

---

## Long-term Vision

### Version 0.16.0 Release
- **FlatBuffers**: Modern default ✅
- **Legacy Thrift**: Backward compatibility ✅
- **Modern Thrift**: Not included (fbthrift optional)

### Version 0.17.0 or later
- **FlatBuffers**: Modern default ✅
- **Legacy Thrift**: Deprecated but supported ✅
- **Modern Thrift**: Added with stable fbthrift ✅

### Benefits
1. v0.14.1 compatibility maintained throughout
2. No dependency hell during transition
3. Clean architecture with clear separation
4. Can deprecate legacy when modern stable

---

## Next Session Plan (Session 62)

**Objective**: Implement Phase 1 (Thrift Compact Primitives)

**Tasks**:
1. Analyze dwarfs-rs `de_thrift.rs` and `ser_thrift.rs` in detail
2. Create header files for writer/reader
3. Implement varint/zigzag encoding/decoding
4. Write comprehensive unit tests
5. Cross-validate with dwarfs-rs test vectors

**Deliverable**: Working `ThriftCompactWriter` and `ThriftCompactReader` classes

**Time**: 3-4 hours

---

**Created**: 2025-12-31 23:41 HKT
**Author**: Kilo Code (Architect Mode)
**Status**: ARCHITECTURE COMPLETE - Ready for Session 62 implementation