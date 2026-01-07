# Session 62 Continuation Prompt: Legacy Thrift Port

**Date**: 2025-12-31
**Status**: 📋 **READY FOR IMPLEMENTATION**
**Strategy**: Port hand-coded Thrift from dwarfs-rs (NO fbthrift dependency)
**Duration**: 10-12 hours over 4 sessions (Phases 1-4)

---

## CRITICAL CONTEXT

**Why This Approach**:
- Tebako requires **static linking** (RULE 4)
- Homebrew provides **dynamic libraries ONLY** (.dylib)
- fbthrift + Folly = version compatibility hell (Session 60: 15 failed attempts)
- dwarfs-rs has **hand-coded Thrift** with NO dependencies

**Solution**: Port dwarfs-rs's minimal Thrift Compact implementation to C++
- ✅ No fbthrift dependency
- ✅ No Folly dependency
- ✅ No version compatibility issues
- ✅ Full static linking support
- ✅ ~1400 lines of code (manageable)

---

## Three-Format Architecture

After porting, dwarfs will support 3 metadata formats:

1. **FlatBuffers** (modern default) - ✅ Already working
2. **Legacy Thrift** (hand-coded) - 🆕 To be implemented
3. **Modern Thrift** (fbthrift) - 🔮 Future (when vcpkg stable)

---

## Source Material: dwarfs-rs

**Location**: `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/`

**Key Files**:
- `de_thrift.rs` (273 lines) - Thrift Compact deserialization
- `ser_thrift.rs` (322 lines) - Thrift Compact serialization
- `de_frozen.rs` - Frozen2 deserialization
- `ser_frozen.rs` - Frozen2 serialization
- `metadata.rs` (602 lines) - Data structures

**Total Code**: ~1400 lines to port

---

## Implementation Roadmap

### Session 62: Phase 1 - Thrift Compact Primitives (3-4 hours)

**Files to Create**:
```
include/dwarfs/metadata/legacy/
├── thrift_compact_writer.h
├── thrift_compact_reader.h
└── thrift_types.h

src/metadata/legacy/
├── thrift_compact_writer.cpp
├── thrift_compact_reader.cpp
└── thrift_types.cpp

test/metadata/legacy/
└── thrift_compact_test.cpp
```

**Implementation Steps**:
1. Analyze dwarfs-rs `de_thrift.rs` and `ser_thrift.rs`
2. Create `ThriftCompactWriter` class (varint, zigzag, primitives)
3. Create `ThriftCompactReader` class (mirror of writer)
4. Write unit tests (byte-for-byte comparison with Rust)
5. Test with actual Homebrew v0.14.1 image snippets

**Deliverable**: Working primitives, ~400 lines + tests

### Session 63: Phase 2 - Metadata Serialization (3-4 hours)

**Goal**: Wire primitives to domain::metadata

**Tasks**:
1. Implement struct serialization (field delta encoding)
2. Implement map serialization (type byte)
3. Create `legacy_thrift_serializer.cpp` using primitives
4. Create `legacy_thrift_deserializer.cpp`
5. Write round-trip tests

**Deliverable**: Can serialize/deserialize domain::metadata, ~200 lines

### Session 64: Phase 3 - Frozen2 Support (4 hours)

**Goal**: Add Schema + bit-packing

**Tasks**:
1. Port Schema structures from `metadata.rs`
2. Implement `FrozenReader` (bit-packed access)
3. Implement `FrozenWriter` (bit-packing)
4. Integration with metadata serialization
5. Full compatibility tests

**Deliverable**: Complete Frozen2 support, ~600 lines

### Session 65: Phase 4 - Integration (2 hours)

**Goal**: Wire into dwarfs build system

**Tasks**:
1. Create `LegacyThriftFacade` implementing `MetadataSerializationFacade`
2. Register in `serializer_registry`
3. Update format detection (no magic bytes for legacy)
4. Run full test suite
5. Verify Homebrew v0.14.1 compatibility

**Deliverable**: Production-ready legacy format

---

## Quick Start (Session 62)

```bash
cd /Users/mulgogi/src/external/dwarfs

# 1. Analyze source
cat /Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/de_thrift.rs
cat /Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs

# 2. Create directory structure
mkdir -p include/dwarfs/metadata/legacy
mkdir -p src/metadata/legacy
mkdir -p test/metadata/legacy

# 3. Start with thrift_types.h (Tag enum)
# 4. Implement ThriftCompactWriter
# 5. Implement ThriftCompactReader
# 6. Write unit tests
```

---

## Reference: Thrift Compact Encoding

### Varint (from dwarfs-rs)
```rust
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

### Zigzag (from dwarfs-rs)
```rust
fn write_zigzag(&mut self, v: i32) {
    self.write_varint((v << 1 ^ (v >> 31)) as u32);
}
```

### Field Encoding (from dwarfs-rs)
```rust
// Upper 4 bits: field ID delta (1-15, or 0 for zigzag ID)
// Lower 4 bits: type tag
fn serialize_field<T>(&mut self, _key: &'static str, value: &T) -> Result<()> {
    let pos = self.w.len();
    self.w.push(0);  // Placeholder

    let tag = value.serialize(ValueSerializer { ... })?;
    self.w[pos] = self.field_id_diff_tag | tag as u8;
    self.field_id_diff_tag = 0x10;  // Next field +1
    Ok(())
}
```

---

## Testing Strategy

### Unit Tests (Per Component)
```cpp
// test/metadata/legacy/thrift_compact_test.cpp
TEST(ThriftCompactWriter, VarintEncoding) {
  std::vector<uint8_t> buf;
  ThriftCompactWriter w(buf);

  w.write_varint(0);       // → 0x00
  w.write_varint(127);     // → 0x7F
  w.write_varint(128);     // → 0x80 0x01
  w.write_varint(16383);   // → 0xFF 0x7F

  EXPECT_EQ(buf, std::vector<uint8_t>{0x00, 0x7F, 0x80, 0x01, 0xFF, 0x7F});
}

TEST(ThriftCompactReader, VarintDecoding) {
  std::vector<uint8_t> data{0x00, 0x7F, 0x80, 0x01, 0xFF, 0x7F};
  ThriftCompactReader r(data);

  EXPECT_EQ(r.read_varint(), 0);
  EXPECT_EQ(r.read_varint(), 127);
  EXPECT_EQ(r.read_varint(), 128);
  EXPECT_EQ(r.read_varint(), 16383);
}
```

### Integration Tests
```cpp
TEST(LegacyThrift, MetadataRoundTrip) {
  // Create test metadata
  domain::metadata original = create_simple_test();

  // Serialize
  LegacyThriftFacade facade;
  byte_buffer buf;
  facade.serialize(original, buf);

  // Deserialize
  domain::metadata parsed;
  facade.deserialize(buf.span(), parsed);

  // Verify
  EXPECT_EQ(original, parsed);
}
```

### Compatibility Tests
```cpp
TEST(LegacyThrift, ReadHomebrew_v0_14_1) {
  // Load actual Homebrew v0.14.1 image
  auto bytes = read_file("test/fixtures/homebrew-v0.14.1.dft");

  // Parse with legacy reader
  // Verify all fields match expected values
}
```

---

## Documentation to Review

**Before Starting Session 62**:
1. [`doc/SESSION_60_VCPKG_FINDINGS.md`](SESSION_60_VCPKG_FINDINGS.md) - Why fbthrift doesn't work
2. [`doc/SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md`](SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md) - Full architecture
3. dwarfs-rs source code (Rust reference implementation)

---

## Success Milestones

**Session 62** (Phase 1):
- [ ] ThriftCompactWriter class complete
- [ ] ThriftCompactReader class complete
- [ ] All primitive encoding tests pass
- [ ] Byte-for-byte match with dwarfs-rs

**Session 63** (Phase 2):
- [ ] Metadata serializer complete
- [ ] Metadata deserializer complete
- [ ] Round-trip tests pass
- [ ] Can read simple Homebrew images

**Session 64** (Phase 3):
- [ ] Frozen2 schema support
- [ ] Bit-packing reader/writer
- [ ] Full Homebrew v0.14.1 compatibility

**Session 65** (Phase 4):
- [ ] Facade integration
- [ ] All tests pass
- [ ] Documentation complete
- [ ] READY FOR RELEASE

---

**START SESSION 62 WITH**: Read [`doc/SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md`](SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md) → Analyze dwarfs-rs code → Implement Phase 1