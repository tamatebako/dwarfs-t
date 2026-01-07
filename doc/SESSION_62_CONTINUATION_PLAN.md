// ... existing code ...
# Session 62: Legacy Thrift Format - Continuation Plan

**Created**: 2026-01-01 08:06 HKT
**Status**: 📋 **READY FOR IMPLEMENTATION**
**Strategy**: Port hand-coded Thrift Compact from dwarfs-rs (NO fbthrift dependency)

---

## Executive Summary

**Goal**: Implement 3rd metadata format (Legacy Thrift) by porting dwarfs-rs's hand-coded Thrift Compact protocol to C++

**Why**: Unblocks Homebrew v0.14.1 compatibility without fbthrift version hell (Session 60: 15 failed attempts)

**Duration**: 10-12 hours over 4 sessions (Phases 1-4)

**Benefit**:
- No Folly/fbthrift dependencies for legacy format
- Full v0.14.1 backward compatibility
- Can add Modern Thrift later when vcpkg stable

---

## Three-Format Architecture

After completion, dwarfs will support 3 metadata formats:

1. **FlatBuffers** (modern default) - ✅ Working (Session 50+)
2. **Legacy Thrift** (hand-coded) - 🆕 This implementation
3. **Modern Thrift** (fbthrift) - 🔮 Future (when vcpkg stable releases)

---

## Implementation Phases

### Phase 1: Thrift Compact Primitives (4 hours)

**Files to Create**:
```
include/dwarfs/metadata/legacy/
├── thrift_compact_writer.h   (~120 lines)
├── thrift_compact_reader.h   (~130 lines)
└── thrift_types.h            (~40 lines)

src/metadata/legacy/
├── thrift_compact_writer.cpp (~150 lines)
├── thrift_compact_reader.cpp (~180 lines)
└── thrift_types.cpp          (~20 lines)

test/metadata/legacy/
└── thrift_compact_test.cpp   (~300 lines)
```

**Implementation Steps**:
1. Setup directory structure + CMake
2. Implement Tag enum + helpers
3. Implement `ThriftCompactWriter`:
   - `write_varint()` - 7-bit encoding
   - `write_zigzag()` - Sign encoding
   - `write_bool()`, `write_i16()`, `write_i32()`, `write_string()`
   - Struct/map container support
4. Implement `ThriftCompactReader` (mirror of writer)
5. Write comprehensive unit tests
6. Validate byte-for-byte against dwarfs-rs

**Deliverable**: Working Thrift Compact primitives layer

### Phase 2: Metadata Serialization (3 hours)

**Files to Create**:
```
include/dwarfs/metadata/legacy/
└── legacy_metadata_serializer.h

src/metadata/legacy/
└── legacy_metadata_serializer.cpp

test/metadata/legacy/
└── metadata_serialization_test.cpp
```

**Implementation Steps**:
1. Map `domain::metadata` fields to Thrift Compact encoding
2. Implement serialization:
   - Serialize each field in correct order
   - Use struct/map primitives from Phase 1
3. Implement deserialization:
   - Parse Thrift Compact to domain model
   - Handle optional fields correctly
4. Write round-trip tests
5. Test with actual Homebrew v0.14.1 images

**Deliverable**: Can serialize/deserialize `domain::metadata`

### Phase 3: Frozen2 Support (4 hours)

**Files to Create**:
```
include/dwarfs/metadata/legacy/
├── frozen_schema.h
├── frozen_reader.h
└── frozen_writer.h

src/metadata/legacy/
├── frozen_schema.cpp
├── frozen_reader.cpp
└── frozen_writer.cpp

test/metadata/legacy/
└── frozen_test.cpp
```

**Implementation Steps**:
1. Port Schema structures from dwarfs-rs
2. Implement `FrozenReader`:
   - Bit-packed field access
   - Schema-driven layout parsing
3. Implement `FrozenWriter`:
   - Bit-packing optimization
   - Schema generation
4. Integration with metadata serialization
5. Full compatibility tests with v0.14.1

**Deliverable**: Complete Frozen2 support

### Phase 4: Integration & Testing (2 hours)

**Files to Create**:
```
include/dwarfs/metadata/serialization/
└── legacy_thrift_facade.h

src/metadata/serialization/
└── legacy_thrift_facade.cpp
```

**Implementation Steps**:
1. Create `LegacyThriftFacade` implementing `MetadataSerializationFacade`
2. Register in `serializer_registry.cpp`
3. Update format detection logic
4. Run full test suite
5. Verify Homebrew v0.14.1 compatibility
6. Update documentation

**Deliverable**: Production-ready legacy format

---

## Architecture Principles

### Object-Oriented Design

**Class Hierarchy**:
```
MetadataSerializationFacade (interface)
├── FlatBuffersFacade ✅ (existing)
├── LegacyThriftFacade 🆕 (this work)
└── ModernThriftFacade 🔮 (future)
```

**Separation of Concerns**:
- `thrift_compact_*`: Protocol primitives (varint, zigzag, field encoding)
- `legacy_metadata_serializer`: Domain model ↔ Thrift wire format
- `frozen_*`: Schema + bit-packing layer
- `legacy_thrift_facade`: Registry integration

### MECE Implementation

**Format Detection** (Mutually Exclusive):
1. Check for FlatBuffers magic bytes ("DFBF") → FlatBuffersFacade
2. Try parse as Legacy Thrift → LegacyThriftFacade
3. Check for Modern Thrift magic ("DFTM") → ModernThriftFacade (future)
4. Return error if none match

**Collectively Exhaustive**: All valid metadata covered by one format

### No Code Guards

**WRONG** (conditional compilation):
```cpp
#ifdef DWARFS_HAVE_LEGACY_THRIFT
  // Legacy code
#else
  // Fallback
#endif
```

**RIGHT** (architectural solution):
```cpp
// Registry pattern - formats self-register
class SerializerRegistry {
  void register_format(SerializationFormat, unique_ptr<Facade>);
};

// Legacy format always available (no external deps)
static auto legacy_registration = []() {
  SerializerRegistry::instance().register_format(
    SerializationFormat::LEGACY_THRIFT,
    make_unique<LegacyThriftFacade>()
  );
}();
```

### One Responsibility

Each class has single purpose:
- `ThriftCompactWriter`: Encode primitives to bytes
- `ThriftCompactReader`: Decode primitives from bytes
- `LegacyMetadataSerializer`: Domain model ↔ primitives
- `FrozenReader/Writer`: Schema + bit-packing
- `LegacyThriftFacade`: Facade + registry integration

---

## Testing Strategy

### Unit Tests (Per Component)

**Primitives** (`thrift_compact_test.cpp`):
```cpp
TEST(ThriftCompactWriter, Varint_EdgeCases) {
  // Test 0, 1, 127, 128, 255, 256, 65535, 65536, max_u32
}

TEST(ThriftCompactWriter, Zigzag_Negative) {
  // Test -1, -127, -128, -32768, min_i32
}

TEST(ThriftCompact, RoundTrip_AllTypes) {
  // Write → Read → Verify for each type
}
```

**Metadata** (`metadata_serialization_test.cpp`):
```cpp
TEST(LegacyMetadata, RoundTrip_Simple) {
  domain::metadata original = create_simple_metadata();
  auto bytes = serialize(original);
  auto parsed = deserialize(bytes);
  EXPECT_EQ(original, parsed);
}

TEST(LegacyMetadata, ReadHomebrew_v0_14_1) {
  auto bytes = load_homebrew_image();
  auto meta = deserialize(bytes);
  // Verify all fields
}
```

**Frozen** (`frozen_test.cpp`):
```cpp
TEST(FrozenReader, BitPacked_i16) {
  // Test bit-packed field access
}

TEST(FrozenWriter, Schema_Generation) {
  // Verify schema correctness
}
```

### Integration Tests

**Cross-Format** (`format_compatibility_test.cpp`):
```cpp
TEST(FormatCompatibility, LegacyToFlatBuffers) {
  // Serialize Legacy → Deserialize to domain → Serialize FlatBuffers
  // Verify equivalent
}

TEST(FormatCompatibility, AllFormatsRoundTrip) {
  domain::metadata original = create_test();

  // Legacy round-trip
  auto legacy_bytes = legacy_facade.serialize(original);
  auto legacy_parsed = legacy_facade.deserialize(legacy_bytes);
  EXPECT_EQ(original, legacy_parsed);

  // FlatBuffers round-trip
  auto fb_bytes = fb_facade.serialize(original);
  auto fb_parsed = fb_facade.deserialize(fb_bytes);
  EXPECT_EQ(original, fb_parsed);

  // Cross-format equality
  EXPECT_EQ(legacy_parsed, fb_parsed);
}
```

### Compatibility Tests

**Homebrew v0.14.1** (`homebrew_compatibility_test.cpp`):
```cpp
TEST(HomebrewCompat, ReadV0_14_1_Image) {
  auto bytes = load_test_file("homebrew-v0.14.1.dft");
  LegacyThriftFacade facade;
  domain::metadata meta;
  ASSERT_NO_THROW(facade.deserialize(bytes, meta));

  // Verify key fields
  EXPECT_GT(meta.chunks.size(), 0);
  EXPECT_GT(meta.inodes.size(), 0);
  EXPECT_EQ(meta.block_size, expected_value);
}

TEST(HomebrewCompat, WriteCompatibleImage) {
  domain::metadata meta = create_test();
  LegacyThriftFacade facade;
  auto bytes = facade.serialize(meta);

  // Manual verification:
  // $ /opt/homebrew/bin/dwarfsck -i our-legacy.dft
  // $ /opt/homebrew/bin/dwarfsextract -i our-legacy.dft -o /tmp/out
}
```

---

## Build System Integration

### CMake (`cmake/metadata_serialization.cmake`)

```cmake
# Legacy Thrift (always available, no external deps)
set(DWARFS_LEGACY_THRIFT_SOURCES
  src/metadata/legacy/thrift_types.cpp
  src/metadata/legacy/thrift_compact_writer.cpp
  src/metadata/legacy/thrift_compact_reader.cpp
  src/metadata/legacy/legacy_metadata_serializer.cpp
  src/metadata/legacy/frozen_schema.cpp
  src/metadata/legacy/frozen_reader.cpp
  src/metadata/legacy/frozen_writer.cpp
  src/metadata/serialization/legacy_thrift_facade.cpp
)

add_library(dwarfs_metadata_legacy ${DWARFS_LEGACY_THRIFT_SOURCES})

target_include_directories(dwarfs_metadata_legacy
  PUBLIC include
)

target_compile_features(dwarfs_metadata_legacy
  PUBLIC cxx_std_20
)

target_compile_definitions(dwarfs_metadata_legacy
  PUBLIC DWARFS_HAVE_LEGACY_THRIFT=1
)

target_link_libraries(dwarfs_metadata_legacy
  PUBLIC dwarfs_metadata_domain
)

# Tests
if(WITH_TESTS)
  add_executable(legacy_thrift_tests
    test/metadata/legacy/thrift_compact_test.cpp
    test/metadata/legacy/metadata_serialization_test.cpp
    test/metadata/legacy/frozen_test.cpp
    test/metadata/legacy/format_compatibility_test.cpp
    test/metadata/legacy/homebrew_compatibility_test.cpp
  )

  target_link_libraries(legacy_thrift_tests
    dwarfs_metadata_legacy
    GTest::gtest_main
  )

  gtest_discover_tests(legacy_thrift_tests)
endif()
```

---

## Timeline Compression

**Original Estimate**: 4 sessions × 3 hours = 12 hours

**Compressed**: 3 sessions × 4 hours = 12 hours (same total, fewer sessions)

| Session | Phases | Duration | Cumulative |
|---------|--------|----------|------------|
| **Session 62** | Phase 1 + Phase 2 start | 4h | 4h |
| **Session 63** | Phase 2 finish + Phase 3 | 4h | 8h |
| **Session 64** | Phase 4 + Docs | 4h | 12h |

---

## Format Comparison

| Aspect | FlatBuffers | Legacy Thrift | Modern Thrift |
|--------|-------------|---------------|---------------|
| **Status** | ✅ Working | 🆕 This work | 🔮 Future |
| **Dependencies** | Header-only | None | Folly + fbthrift |
| **Build complexity** | Simple | Simple | Complex |
| **Size overhead** | ~105-108% | 100% (baseline) | 100% |
| **Speed** | Fast | Fast | Fast |
| **Compatibility** | v0.15.0+ | v0.14.1+ | v0.16.0+ |
| **File extension** | `.dff` | `.dft` | `.dftm` (future) |
| **Magic bytes** | "DFBF" | None | "DFTM" (future) |

---

## Success Criteria

### Functional
- [x] Architecture designed
- [ ] All primitives work (varint, zigzag, bool, i16, i32, string)
- [ ] Struct/map containers work
- [ ] Metadata serialization works
- [ ] Frozen2 schema + bit-packing works
- [ ] Round-trip tests pass

### Compatibility
- [ ] Can read Homebrew v0.14.1 images
- [ ] Can write images Homebrew v0.14.1 can read
- [ ] Cross-format conversion works (Legacy ↔ FlatBuffers)
- [ ] All existing tests pass with legacy format

### Code Quality
- [ ] Comprehensive unit tests (>90% coverage)
- [ ] Clean OOP design (MECE, separation of concerns)
- [ ] No external dependencies
- [ ] C++20 best practices
- [ ] Documentation complete

---

## Risk Mitigation

### Risk: Translation Errors (Rust → C++)

**Mitigation**:
- Use dwarfs-rs test vectors
- Byte-for-byte comparison
- Cross-test with actual Homebrew images

### Risk: Frozen2 Complexity

**Mitigation**:
- Start with simple non-packed structures
- Add bit-packing incrementally
- Extensive testing at each step

### Risk: Missed Edge Cases

**Mitigation**:
- Test with diverse real-world images
- Fuzzing (if time permits)
- Compare with fbthrift behavior (when working)

---

## Documentation Updates

After implementation, update:

1. **User-Facing**:
   - `README.md`: Add Legacy Thrift format support
   - `doc/dwarfs-format.md`: Document 3 formats
   - `doc/mkdwarfs.md`: Add `--format=legacy-thrift` option
   - `doc/dwarfsck.md`: Document format detection

2. **Developer-Facing**:
   - `doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`: Add Legacy Thrift
   - `doc/SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md`: Keep as reference

3. **Move to Archive**:
   - `doc/SESSION_60_VCPKG_FINDINGS.md` → `old-docs/session-60/`
   - `doc/HOMEBREW_COMPATIBILITY_ISSUE.md` → Update status (RESOLVED)

---

## Next Steps

1. **Read** [`doc/SESSION_62_CONTINUATION_PROMPT.md`](SESSION_62_CONTINUATION_PROMPT.md)
2. **Switch to Code mode**
3. **Start Phase 1**: Implement Thrift Compact primitives
4. **Track progress** in [`doc/SESSION_62_IMPLEMENTATION_STATUS.md`](SESSION_62_IMPLEMENTATION_STATUS.md)

---

**Created**: 2026-01-01 08:06 HKT
**Author**: Kilo Code (Architect Mode)
**Status**: 📋 **READY FOR IMPLEMENTATION**
// ... existing code ...