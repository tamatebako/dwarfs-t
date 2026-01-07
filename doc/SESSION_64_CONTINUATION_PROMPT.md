# Session 64 Continuation Prompt: Frozen2 Implementation

**Date**: 2026-01-01
**Status**: 🟡 **READY TO START**
**Mode**: Switch to **Code Mode**
**Duration**: ~4 hours

---

## Quick Context

**What**: Implement Frozen2 schema and bit-packing for Legacy Thrift format

**Why**: Add proper u64 support and complete DwarFS v0.14.1 compatibility

**Progress**: Phase 2 complete (serialize/deserialize working), Phase 3 pending

---

## Session 64 Goals (4 hours)

### Part 1: Frozen2 Analysis (1 hour)

1. **Read dwarfs-rs Frozen2 implementation** (30 min)
   - `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/de_frozen.rs`
   - `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs`
   - Understand schema format and bit-packing algorithm

2. **Design C++ architecture** (30 min)
   - Schema representation (FrozenSchema class)
   - Bit-packed reader/writer
   - Integration with existing serializer

### Part 2: Schema Implementation (1.5 hours)

3. **Create frozen_schema.h** (30 min)
   - Schema type definitions
   - Field layout structures
   - Bit-packing helpers

4. **Implement frozen_schema.cpp** (45 min)
   - Schema parsing from Thrift
   - Field offset calculation
   - u64 field support

5. **Write schema tests** (15 min)
   - Schema parsing validation
   - Offset calculation tests

### Part 3: Reader/Writer (1.5 hours)

6. **Implement frozen_reader.cpp** (45 min)
   - Bit-packed field reading
   - u64 decoding
   - Integration with deserialize()

7. **Implement frozen_writer.cpp** (45 min)
   - Bit-packed field writing
   - u64 encoding
   - Integration with serialize()

---

## Pre-Flight Checklist

### Files to Read First

**dwarfs-rs Reference**:
- [ ] `de_frozen.rs` - Frozen2 deserializer (415 lines)
- [ ] `ser_frozen.rs` - Frozen2 serializer
- [ ] `metadata.rs` - Schema definitions

**Current Implementation**:
- [ ] [`doc/SESSION_63_COMPLETION_SUMMARY.md`](SESSION_63_COMPLETION_SUMMARY.md)
- [ ] [`src/metadata/legacy/legacy_metadata_serializer.cpp`](../src/metadata/legacy/legacy_metadata_serializer.cpp)
- [ ] [`doc/SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md`](SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md)

### Verify Current State

```bash
# Verify Phase 2 tests still pass
cd /Users/mulgogi/src/external/dwarfs
./build-legacy/legacy_thrift_tests
./build-legacy/metadata_serializer_tests

# Should show: 39/39 tests passing
```

---

## Implementation Plan

### Step 1: Analyze Frozen2 Format

**Key Concepts from dwarfs-rs**:
- Schema section (Thrift-encoded)
- Metadata section (bit-packed)
- Field types: VarInt, Fixed, Nested
- Bit-level packing for space efficiency

**Example Schema**:
```rust
pub struct Schema {
    pub fields: Vec<FieldSchema>,
    pub layout_size: u32,
}

pub enum FieldSchema {
    VarInt { offset: u32, bits: u8 },
    Fixed { offset: u32, size: u8 },
    Nested { offset: u32, schema: Box<Schema> },
}
```

### Step 2: Design C++ Architecture

**File Structure**:
```
include/dwarfs/metadata/legacy/
├── frozen_schema.h        # Schema types and parsing
├── frozen_reader.h        # Bit-packed reading
└── frozen_writer.h        # Bit-packed writing

src/metadata/legacy/
├── frozen_schema.cpp
├── frozen_reader.cpp
└── frozen_writer.cpp

test/metadata/legacy/
└── frozen_schema_test.cpp
```

**Class Design**:
```cpp
namespace dwarfs::metadata::legacy {

// Schema representation
class FrozenSchema {
public:
  struct FieldLayout {
    uint32_t offset;  // Bit offset
    uint8_t bits;     // Bit width
    FieldType type;   // VarInt, Fixed, Nested
  };

  void parse_from_thrift(ThriftCompactReader& r);
  FieldLayout const& get_field(size_t index) const;
  uint32_t total_bits() const;
};

// Bit-packed reader
class FrozenReader {
public:
  FrozenReader(std::span<uint8_t const> data, FrozenSchema const& schema);

  uint64_t read_u64(size_t field_index);
  int32_t read_i32(size_t field_index);
  bool read_bool(size_t field_index);
};

// Bit-packed writer
class FrozenWriter {
public:
  FrozenWriter(std::vector<uint8_t>& buffer, FrozenSchema const& schema);

  void write_u64(size_t field_index, uint64_t value);
  void write_i32(size_t field_index, int32_t value);
  void write_bool(size_t field_index, bool value);
};

} // namespace
```

### Step 3: Implement frozen_schema.cpp

**Key Functions**:
```cpp
void FrozenSchema::parse_from_thrift(ThriftCompactReader& r) {
  // Parse schema section (Thrift Compact format)
  // Extract field layouts (offset + bits)
  // Calculate total layout size
}

uint32_t FrozenSchema::total_bits() const {
  // Sum all field bit widths
  // Round up to byte boundary
}
```

### Step 4: Implement frozen_reader.cpp

**Key Functions**:
```cpp
uint64_t FrozenReader::read_u64(size_t field_index) {
  auto const& layout = schema_.get_field(field_index);
  // Read layout.bits from data_ at layout.offset
  // Return as uint64_t
}
```

### Step 5: Implement frozen_writer.cpp

**Key Functions**:
```cpp
void FrozenWriter::write_u64(size_t field_index, uint64_t value) {
  auto const& layout = schema_.get_field(field_index);
  // Write value (layout.bits) to buffer_ at layout.offset
}
```

### Step 6: Integration with Serializer

**Updated serialize()**:
```cpp
void LegacyMetadataSerializer::serialize(metadata const& meta,
                                        std::vector<uint8_t>& output) {
  // Option 1: Thrift Compact only (current)
  // ... existing code ...

  // Option 2: Thrift Compact + Frozen2 (new)
  if (use_frozen2) {
    // Write schema section (Thrift)
    FrozenSchema schema = create_metadata_schema();
    write_schema_section(schema, output);

    // Write metadata section (bit-packed)
    FrozenWriter writer(output, schema);
    writer.write_u64(TIMESTAMP_BASE_FIELD, meta.timestamp_base);
    writer.write_u64(TOTAL_FS_SIZE_FIELD, meta.total_fs_size);
    // ... other fields
  }
}
```

---

## Testing Strategy

### Unit Tests

**frozen_schema_test.cpp**:
```cpp
TEST(FrozenSchema, ParseSimpleSchema) {
  // Create Thrift-encoded schema
  // Parse with FrozenSchema
  // Verify field layouts
}

TEST(FrozenSchema, CalculateTotalBits) {
  // Create schema with known bit widths
  // Verify total_bits() calculation
}
```

**frozen_reader_test.cpp**:
```cpp
TEST(FrozenReader, ReadU64) {
  // Create bit-packed data with known u64 value
  // Read with FrozenReader
  // Verify correct value
}
```

**frozen_writer_test.cpp**:
```cpp
TEST(FrozenWriter, WriteU64) {
  // Write u64 value with FrozenWriter
  // Verify bit-packed output
}
```

### Integration Tests

**metadata_serializer_frozen_test.cpp**:
```cpp
TEST(FrozenMetadata, RoundTrip_U64Fields) {
  metadata meta;
  meta.timestamp_base = UINT64_MAX;
  meta.total_fs_size = (uint64_t)1 << 40;  // 1 TiB

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize(meta, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize(serialized, deserialized);

  EXPECT_EQ(deserialized.timestamp_base, meta.timestamp_base);
  EXPECT_EQ(deserialized.total_fs_size, meta.total_fs_size);
}
```

---

## Success Criteria

### Phase 3 Complete When

- [ ] FrozenSchema parses from Thrift
- [ ] FrozenReader reads u64 correctly
- [ ] FrozenWriter writes u64 correctly
- [ ] serialize() produces Frozen2 format
- [ ] deserialize() reads Frozen2 format
- [ ] All tests pass (primitives + metadata + frozen)
- [ ] u64 fields (timestamp_base, total_fs_size) work correctly

### Code Quality

- [ ] Clean separation: Schema / Reader / Writer
- [ ] Comprehensive tests for each component
- [ ] Error handling for invalid schemas
- [ ] Documentation for Frozen2 format

---

## Deliverables

**Files to Create** (~800 lines):
1. `include/dwarfs/metadata/legacy/frozen_schema.h` (~150 lines)
2. `src/metadata/legacy/frozen_schema.cpp` (~250 lines)
3. `include/dwarfs/metadata/legacy/frozen_reader.h` (~100 lines)
4. `src/metadata/legacy/frozen_reader.cpp` (~200 lines)
5. `include/dwarfs/metadata/legacy/frozen_writer.h` (~100 lines)
6. `src/metadata/legacy/frozen_writer.cpp` (~200 lines)
7. `test/metadata/legacy/frozen_schema_test.cpp` (~200 lines)

**Files to Modify**:
1. `src/metadata/legacy/legacy_metadata_serializer.cpp` - Add Frozen2 support
2. `cmake/metadata_serialization.cmake` - Add frozen tests
3. `doc/SESSION_64_IMPLEMENTATION_STATUS.md` - Track progress

---

## Quick Reference

**dwarfs-rs Files**: `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/`
- `de_frozen.rs` - Deserializer reference
- `ser_frozen.rs` - Serializer reference
- `metadata.rs` - Type definitions

**Build Commands**:
```bash
# Configure
cmake -B build-legacy -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=ON

# Build
ninja -C build-legacy frozen_schema_tests

# Test
./build-legacy/frozen_schema_tests
```

---

**Start Command**: Switch to **Code mode** and begin with Frozen2 analysis