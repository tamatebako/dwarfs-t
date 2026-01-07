# Session 64 Part 2: START HERE

**Status**: 🟢 **READY TO IMPLEMENT**
**Mode**: Code Mode
**Duration**: ~3 hours

---

## Quick Start

**What was completed in Part 1**:
- ✅ Analyzed dwarfs-rs Frozen2 (1,366 lines of Rust)
- ✅ Discovered Frozen2 = bit-packing layer (NOT complex format)
- ✅ Simplified implementation plan (3h vs 4h)

**What to do now**:
1. Read this prompt
2. Read [`doc/SESSION_64_PART2_CONTINUATION_PLAN.md`](SESSION_64_PART2_CONTINUATION_PLAN.md)
3. Begin Step 1: Implement `frozen_bits.h/.cpp`

---

## Immediate First Steps

### Step 1: Create frozen_bits.h (30 min)

**File**: `include/dwarfs/metadata/legacy/frozen_bits.h`

**Template**:
```cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace dwarfs::metadata::legacy {

/// Bit-level read/write operations for Frozen2 format
class FrozenBits {
public:
  /// Load N bits (1-64) from data at bit offset
  static uint64_t load_bits(
    std::span<uint8_t const> data,
    uint32_t base_bit,
    uint16_t bits
  );

  /// Store N bits (1-64) to data at bit offset
  static void store_bits(
    std::span<uint8_t> data,
    uint32_t base_bit,
    uint16_t bits,
    uint64_t value
  );

  /// Load single bit
  static bool load_bit(std::span<uint8_t const> data, uint32_t base_bit);

  /// Store single bit
  static void store_bit(std::span<uint8_t> data, uint32_t base_bit, bool value);
};

} // namespace
```

### Step 2: Implement frozen_bits.cpp (30 min)

**File**: `src/metadata/legacy/frozen_bits.cpp`

**Key Algorithm** (from dwarfs-rs de_frozen.rs:73-111):
```cpp
uint64_t FrozenBits::load_bits(
    span<uint8_t const> data,
    uint32_t base_bit,
    uint16_t bits) {

  // Calculate byte position and bit offset within byte
  uint32_t byte_idx = base_bit / 8;
  uint16_t bit_start = base_bit % 8;

  // Load 8-byte chunk (little-endian)
  uint64_t x;
  if (byte_idx + 8 <= data.size()) {
    std::memcpy(&x, &data[byte_idx], 8);
  } else {
    // Near end: zero-pad
    uint8_t buf[8] = {0};
    size_t available = data.size() - byte_idx;
    std::memcpy(buf, &data[byte_idx], available);
    std::memcpy(&x, buf, 8);
  }

  // Extract bits
  uint16_t start_and_bits = bit_start + bits;
  if (start_and_bits <= 64) {
    // Simple case: all within 8 bytes
    return (x << (64 - start_and_bits)) >> (64 - bits);
  } else {
    // Overshooting: need 9th byte
    uint16_t overshooting_bits = start_and_bits & 63;
    uint64_t hi = data[byte_idx + 8];
    return (x >> bit_start) | (hi << (64 - overshooting_bits)) >> (64 - bits);
  }
}
```

### Step 3: Write Tests (30 min)

**File**: `test/metadata/legacy/frozen_bits_test.cpp`

**Critical Tests**:
```cpp
TEST(FrozenBits, LoadBits_AllWidths) {
  // Test 1-64 bit reads at various alignments
}

TEST(FrozenBits, RoundTrip_U64) {
  // Verify u64 values survive round-trip
  std::vector<uint8_t> buf(16, 0);
  uint64_t value = UINT64_MAX;
  FrozenBits::store_bits(buf, 0, 64, value);
  uint64_t read = FrozenBits::load_bits(buf, 0, 64);
  EXPECT_EQ(read, value);
}
```

---

## Success Criteria (Part 2 Complete When)

- [ ] `frozen_bits.h` created (~100 lines)
- [ ] `frozen_bits.cpp` implemented (~150 lines)
- [ ] `frozen_bits_test.cpp` written (~200 lines)
- [ ] All bit operation tests passing
- [ ] CMake build working
- [ ] Ready for Step 2 (schema integration)

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
ninja -C build-legacy frozen_bits_tests

# Test
./build-legacy/frozen_bits_tests
```

---

## Key Files

**Reference**:
- `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/de_frozen.rs` - Bit operations
- [`doc/SESSION_64_PART2_CONTINUATION_PLAN.md`](SESSION_64_PART2_CONTINUATION_PLAN.md) - Full plan

**Current**:
- [`src/metadata/legacy/legacy_metadata_serializer.cpp`](../src/metadata/legacy/legacy_metadata_serializer.cpp) - Will integrate in Step 2
- [`test/metadata/legacy/`](../test/metadata/legacy/) - Test directory

---

## Time Tracking

| Step | Duration | Status |
|------|----------|--------|
| Part 1: Analysis | 40 min | ✅ COMPLETE |
| Step 1: frozen_bits implementation | 1 hour | ⏳ NEXT |
| Step 2: Schema integration | 1 hour | Pending |
| Step 3: Testing | 1 hour | Pending |
| **Total Part 2** | **3 hours** | In Progress |

---

**Created**: 2026-01-01 16:45 HKT
**Start Command**: Switch to Code mode, begin implementing `frozen_bits.h`