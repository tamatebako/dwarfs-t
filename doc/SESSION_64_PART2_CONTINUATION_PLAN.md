# Session 64 Part 2: Frozen2 Bit-Packing Implementation

**Date**: 2026-01-01 16:40 HKT
**Status**: 🟡 **READY TO START**
**Duration**: ~3 hours (revised from 4 hours)
**Prerequisite**: Part 1 analysis complete ✅

---

## Executive Summary

**Discovery**: Frozen2 is a **bit-packing layer**, not a complex serialization format!

**What We Have** (Phase 1-2):
- ✅ Thrift Compact for Schema section
- ✅ All primitives working (bool, i32, string)
- ✅ Struct/map serialization

**What We Need** (Phase 3 - Simplified):
- Bit operations: `load_bits()`, `store_bits()`
- Schema-driven metadata packing
- u64 field support (no truncation!)

**Time Savings**: 25% reduction (4h → 3h)

---

## Part 2 Implementation Plan (3 hours)

### Step 1: Bit Operations (1 hour)

**Goal**: Implement low-level bit read/write functions

**Files to Create**:
1. `include/dwarfs/metadata/legacy/frozen_bits.h` (~100 lines)
2. `src/metadata/legacy/frozen_bits.cpp` (~150 lines)

**Key Functions**:
```cpp
namespace dwarfs::metadata::legacy {

class FrozenBits {
public:
  // Read 1-64 bits at bit offset (little-endian)
  static uint64_t load_bits(
    std::span<uint8_t const> data,
    uint32_t base_bit,
    uint16_t bits
  );

  // Write 1-64 bits at bit offset (little-endian)
  static void store_bits(
    std::span<uint8_t> data,
    uint32_t base_bit,
    uint16_t bits,
    uint64_t value
  );

  // Read single bit
  static bool load_bit(std::span<uint8_t const> data, uint32_t base_bit);

  // Write single bit
  static void store_bit(std::span<uint8_t> data, uint32_t base_bit, bool value);
};

} // namespace
```

**Algorithm** (from dwarfs-rs de_frozen.rs:73-111):
```cpp
uint64_t load_bits(span<uint8_t const> data, uint32_t base_bit, uint16_t bits) {
  uint32_t byte_idx = base_bit / 8;
  uint16_t bit_start = base_bit % 8;

  // Load 8-byte chunk (or zero-padded if near end)
  uint64_t x;
  if (byte_idx + 8 <= data.size()) {
    std::memcpy(&x, &data[byte_idx], 8);
    // Convert to little-endian if needed
  } else {
    // Near end: load what's available, zero-pad rest
    uint8_t buf[8] = {0};
    size_t available = data.size() - byte_idx;
    std::memcpy(buf, &data[byte_idx], available);
    std::memcpy(&x, buf, 8);
  }

  uint16_t start_and_bits = bit_start + bits;
  if (start_and_bits <= 64) {
    // Simple case: all within 8 bytes
    return (x << (64 - start_and_bits)) >> (64 - bits);
  } else {
    // Overshooting case: need 9th byte
    if (byte_idx + 8 >= data.size()) {
      throw std::runtime_error("bit location overflow");
    }
    uint16_t overshooting_bits = start_and_bits & 63;
    uint64_t hi = data[byte_idx + 8];
    return (x >> bit_start) | (hi << (64 - overshooting_bits)) >> (64 - bits);
  }
}
```

**Testing**:
- `test/metadata/legacy/frozen_bits_test.cpp` (~200 lines)
- Test cases:
  - Single bit read/write
  - 1-64 bit read/write at various alignments
  - Edge cases (near buffer end, overshooting)
  - Round-trip verification

### Step 2: Schema Integration (1 hour)

**Goal**: Use schema to drive bit-packed serialization

**Files to Modify**:
- `src/metadata/legacy/legacy_metadata_serializer.cpp` (~100 lines changes)

**Key Changes**:
```cpp
class LegacyMetadataSerializer {
public:
  // Phase 2 - Thrift Compact only (current)
  static void serialize_v1(metadata const& meta, std::vector<uint8_t>& output);
  static void deserialize_v1(std::span<uint8_t const> input, metadata& meta);

  // Phase 3 - Thrift Compact (schema) + Frozen2 (metadata) (new)
  static void serialize_v2(metadata const& meta, std::vector<uint8_t>& output);
  static void deserialize_v2(std::span<uint8_t const> input, metadata& meta);

private:
  // Create schema from metadata (analyze field usage to determine bit widths)
  static Schema create_schema(metadata const& meta);

  // Serialize metadata using schema layout
  static void serialize_frozen(
    metadata const& meta,
    Schema const& schema,
    std::vector<uint8_t>& output
  );

  // Deserialize metadata using schema layout
  static void deserialize_frozen(
    std::span<uint8_t const> input,
    Schema const& schema,
    metadata& meta
  );
};
```

**Schema Creation Logic**:
```cpp
Schema create_schema(metadata const& meta) {
  Schema schema;
  SchemaLayout root_layout;

  // Field 1: chunks (list)
  if (!meta.chunks.empty()) {
    SchemaLayout chunk_layout;
    chunk_layout.bits = 32 + 32 + 32;  // block + offset + size (all u32)
    // ... create layout for chunk struct
    root_layout.fields.add(1, chunk_layout);
  }

  // Field 12: timestamp_base (u64) - NO TRUNCATION!
  if (meta.timestamp_base != 0) {
    SchemaLayout u64_layout;
    u64_layout.bits = 64;  // Full 64 bits!
    root_layout.fields.add(12, u64_layout);
  }

  // ... similar for other fields

  return schema;
}
```

**Testing**:
- Update `test/metadata/legacy/metadata_serializer_test.cpp`
- Add `TEST(Frozen2, RoundTrip_U64Fields)` to verify no truncation

### Step 3: Testing & Validation (1 hour)

**Goal**: Ensure correctness and compatibility

**Test Files to Update**:
1. `test/metadata/legacy/frozen_bits_test.cpp` (new)
2. `test/metadata/legacy/metadata_serializer_test.cpp` (add Frozen2 tests)

**Test Cases**:

**Bit Operations**:
```cpp
TEST(FrozenBits, LoadBits_SingleBit) {
  uint8_t data[] = {0b10101010};
  EXPECT_TRUE(FrozenBits::load_bit(data, 0));
  EXPECT_FALSE(FrozenBits::load_bit(data, 1));
  // ... test all 8 bits
}

TEST(FrozenBits, LoadBits_CrossByte) {
  uint8_t data[] = {0xFF, 0x00};
  // Load bits 6-9 (crosses byte boundary)
  uint64_t value = FrozenBits::load_bits(data, 6, 4);
  EXPECT_EQ(value, 0b1100);  // 2 bits from first byte, 2 from second
}

TEST(FrozenBits, RoundTrip_AllBitWidths) {
  std::vector<uint8_t> buf(16, 0);
  for (uint16_t bits = 1; bits <= 64; ++bits) {
    uint64_t test_value = (1ULL << (bits - 1));  // MSB set
    FrozenBits::store_bits(buf, 0, bits, test_value);
    uint64_t read_value = FrozenBits::load_bits(buf, 0, bits);
    EXPECT_EQ(read_value, test_value) << "Failed at " << bits << " bits";
  }
}
```

**Frozen2 Metadata**:
```cpp
TEST(Frozen2Metadata, RoundTrip_U64_NoTruncation) {
  metadata meta;
  meta.timestamp_base = UINT64_MAX;  // Max u64 value
  meta.total_fs_size = (uint64_t)1 << 40;  // 1 TiB

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize_v2(meta, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize_v2(serialized, deserialized);

  EXPECT_EQ(deserialized.timestamp_base, UINT64_MAX);  // No truncation!
  EXPECT_EQ(deserialized.total_fs_size, (uint64_t)1 << 40);
}

TEST(Frozen2Metadata, RoundTrip_Comprehensive) {
  metadata meta = create_comprehensive_test_metadata();

  std::vector<uint8_t> serialized;
  LegacyMetadataSerializer::serialize_v2(meta, serialized);

  metadata deserialized;
  LegacyMetadataSerializer::deserialize_v2(serialized, deserialized);

  EXPECT_EQ(meta, deserialized);  // Full round-trip equality
}
```

---

## Success Criteria

### Part 2 Complete When:

- [x] Part 1 analysis complete
- [ ] `frozen_bits.h/.cpp` implemented (~250 lines)
- [ ] `load_bits()` / `store_bits()` working for 1-64 bits
- [ ] Schema integration in `legacy_metadata_serializer.cpp`
- [ ] All bit operation tests passing
- [ ] u64 fields (timestamp_base, total_fs_size) work correctly
- [ ] No truncation in u64 fields
- [ ] All previous tests still passing (39+ tests)
- [ ] Frozen2 round-trip tests passing

### Code Quality:

- [ ] Clean separation: bit ops ↔ schema ↔ serializer
- [ ] Comprehensive tests for bit operations
- [ ] Error handling for invalid bit ranges
- [ ] Documentation for Frozen2 format

---

## Files Summary

**New Files** (3 files, ~450 lines):
1. `include/dwarfs/metadata/legacy/frozen_bits.h` (~100 lines)
2. `src/metadata/legacy/frozen_bits.cpp` (~150 lines)
3. `test/metadata/legacy/frozen_bits_test.cpp` (~200 lines)

**Modified Files** (2 files, ~150 lines changes):
1. `src/metadata/legacy/legacy_metadata_serializer.cpp` (~100 lines)
2. `test/metadata/legacy/metadata_serializer_test.cpp` (~50 lines)

**Total**: 5 files, ~600 lines (vs 800 originally)

---

## Build Commands

```bash
# Configure
cmake -B build-legacy -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON

# Build
ninja -C build-legacy frozen_bits_tests metadata_serializer_tests

# Test
./build-legacy/frozen_bits_tests
./build-legacy/metadata_serializer_tests
```

---

## Next Steps After Part 2

### Part 3: Integration (Session 65)
1. Create `LegacyThriftFacade` implementing `MetadataSerializationFacade`
2. Register in `serializer_registry`
3. Test with Homebrew v0.14.1 images
4. Cross-format conversion (Legacy ↔ FlatBuffers)

---

## References

**Source Analysis**:
- [`/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/de_frozen.rs`](../../../../../../dwarfs-rs/dwarfs/src/metadata/de_frozen.rs) - Bit operations reference
- [`/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs`](../../../../../../dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs) - Schema creation reference

**Documentation**:
- [`doc/SESSION_64_IMPLEMENTATION_STATUS.md`](SESSION_64_IMPLEMENTATION_STATUS.md) - Overall status
- [`doc/SESSION_63_COMPLETION_SUMMARY.md`](SESSION_63_COMPLETION_SUMMARY.md) - Phase 2 completion
- [`doc/SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md`](SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md) - Architecture

---

**Created**: 2026-01-01 16:40 HKT
**Status**: Ready to begin Part 2 implementation
**Estimated Duration**: 3 hours