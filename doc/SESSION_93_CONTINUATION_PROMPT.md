# Session 93: Modern Thrift Testing - Continuation Prompt

**Start Here**: Test Modern Thrift metadata format with comprehensive validation

---

## Quick Context

Session 92 **successfully completed** Modern Thrift library (261 KB):
- ✅ Schema fixed (10 type corrections)
- ✅ Converters working (domain ↔ thrift)
- ✅ Serializer working (CompactProtocol)
- ✅ All components compiled

**Your Mission**: Validate Modern Thrift through comprehensive testing (~1.5-2 hours)

---

## Prerequisites Verified ✅

- Modern Thrift library built: `libdwarfs_metadata_modern_thrift.a` (261 KB)
- All converters compile: `domain_to_thrift` (26 KB), `thrift_to_domain` (34 KB)
- Serializer compiles: `thrift_compact_serializer` (35 KB)
- Thrift types generated: 21 files in `build-modern/thrift/modern/gen-cpp2/`

---

## Phase 4: Testing Strategy

### Test 1: Round-Trip Serialization (30 min)

**Objective**: Verify domain → thrift → bytes → thrift → domain preserves data

**Steps**:

1. **Create Test Domain Model**:
   ```cpp
   domain::metadata create_test_metadata() {
     domain::metadata dm;
     // Populate all fields including v2.5+ optional fields
     dm.category_names = {"pcm", "fits", "text"};
     dm.block_categories = {0, 1, 0, 2};
     dm.category_metadata_json = {"pcm_config", "fits_config"};
     dm.block_category_metadata = {{0, 0}, {1, 1}};
     dm.large_hole_size = {4096, 8192, 16384};
     // ... all other fields
     return dm;
   }
   ```

2. **Test Round-Trip**:
   ```cpp
   TEST(ModernThriftSerializer, RoundTrip) {
     auto original = create_test_metadata();

     ThriftCompactSerializer serializer;
     auto bytes = serializer.serialize(&original);

     auto deserialized_ptr = serializer.deserialize(bytes);
     auto& restored = *static_cast<domain::metadata*>(deserialized_ptr.get());

     EXPECT_EQ(original, restored);
   }
   ```

3. **Verify**:
   - All core fields preserved
   - All optional fields preserved
   - v2.5+ fields work correctly
   - Maps and vectors intact

**Expected Result**: Test passes, all data preserved

### Test 2: Magic Bytes & Format Detection (15 min)

**Objective**: Verify Modern Thrift is correctly detected

**Steps**:

1. **Serialize with Modern Thrift**:
   ```cpp
   auto bytes = thrift_serializer.serialize(&metadata);
   ```

2. **Check Magic Bytes**:
   ```cpp
   EXPECT_EQ(bytes[0], 0x82);  // CompactProtocol
   EXPECT_EQ(bytes[1], 0x21);
   ```

3. **Test Auto-Detection**:
   ```cpp
   auto detected = SerializerRegistry::detect_format(bytes);
   EXPECT_EQ(detected, SerializationFormat::MODERN_THRIFT);
   ```

**Expected Result**: Format correctly identified as Modern Thrift

### Test 3: Cross-Format Conversion (35 min)

**Objective**: Verify Modern Thrift interoperates with other formats

**Test Cases**:

1. **FlatBuffers → Modern Thrift**:
   ```cpp
   // Create with FlatBuffers
   FlatBuffersSerializer fb_ser;
   auto fb_bytes = fb_ser.serialize(&metadata);

   // Deserialize
   auto fb_restored = fb_ser.deserialize(fb_bytes);

   // Re-serialize with Modern Thrift
   ThriftCompactSerializer mt_ser;
   auto mt_bytes = mt_ser.serialize(fb_restored.get());

   // Verify
   auto mt_restored = mt_ser.deserialize(mt_bytes);
   EXPECT_EQ(*fb_restored, *mt_restored);
   ```

2. **Modern Thrift → FlatBuffers**:
   - Reverse of above
   - Verify data integrity

3. **Legacy Thrift → Modern Thrift** (if applicable):
   - Convert from Frozen2 format
   - Verify data integrity

**Expected Result**: All conversions preserve data

### Test 4: Integration Test (40 min)

**Objective**: End-to-end filesystem workflow with Modern Thrift

**Steps**:

1. **Create Filesystem**:
   ```bash
   # Create test data
   mkdir -p /tmp/test-modern-thrift
   echo "Modern Thrift Test" > /tmp/test-modern-thrift/file.txt

   # Create with Modern Thrift metadata (need CLI support from Session 90)
   ./build-modern/mkdwarfs \
     -i /tmp/test-modern-thrift \
     -o /tmp/test-modern.dtc \
     --metadata-format=modern-thrift
   ```

2. **Inspect Metadata**:
   ```bash
   # Check format detection
   ./build-modern/dwarfsck /tmp/test-modern.dtc --check-integrity

   # Verify magic bytes
   xxd /tmp/test-modern.dtc | head -5
   ```

3. **Extract & Verify**:
   ```bash
   # Extract
   ./build-modern/dwarfsextract -i /tmp/test-modern.dtc -o /tmp/extracted

   # Compare
   diff -r /tmp/test-modern-thrift /tmp/extracted
   ```

**Expected Result**: Perfect extraction, no differences

---

## Build Setup

### Ensure Modern Thrift Enabled

```bash
# Verify build configuration
cmake -B build-modern -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=/Users/mulgogi/src/external/dwarfs/vcpkg_ports

# Build library
ninja -C build-modern dwarfs_metadata_modern_thrift

# Build test executables
ninja -C build-modern modern_thrift_serialization_tests
```

---

## Test Files to Create

### 1. Unit Tests: `test/metadata/modern/converter_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/modern/domain_to_thrift.h"
#include "dwarfs/metadata/modern/thrift_to_domain.h"

TEST(ModernThriftConverter, RoundTrip) {
  // Test implementation
}

TEST(ModernThriftConverter, OptionalFields) {
  // Test v2.5+ fields
}

TEST(ModernThriftConverter, EmptyMetadata) {
  // Test edge case
}
```

### 2. Serialization Tests: `test/metadata/modern/serialization_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "dwarfs/metadata/serialization/thrift_compact_serializer.h"

TEST(ThriftCompactSerializer, MagicBytes) {
  // Verify {0x82, 0x21}
}

TEST(ThriftCompactSerializer, RoundTrip) {
  // Full serialize → deserialize
}
```

### 3. Integration Tests: `test/metadata/modern/integration_test.cpp`

```cpp
TEST(ModernThriftIntegration, CreateAndMount) {
  // End-to-end test
}

TEST(ModernThriftIntegration, CrossFormatConversion) {
  // FlatBuffers ↔ Modern Thrift
}
```

---

## Success Criteria

### Must Pass
- ✅ All round-trip tests identical data
- ✅ Magic bytes correct ({0x82, 0x21})
- ✅ Format detection works
- ✅ Cross-format conversions preserve data

### Should Pass
- ✅ Integration tests work end-to-end
- ✅ Performance comparable to FlatBuffers
- ✅ Size smaller than FlatBuffers

### Nice to Have
- ✅ Benchmarks show expected performance
- ✅ All optional fields preserved
- ✅ Edge cases handled gracefully

---

## Troubleshooting

### If Round-Trip Fails

**Check**:
1. Converter logic (verify all fields copied)
2. Type conversions (unsigned ↔ signed)
3. field_ref dereferences (all use `*`)
4. Optional field handling

**Debug**:
```cpp
// Add logging to converters
std::cerr << "Converting field X: " << value << "\n";
```

### If Magic Bytes Wrong

**Check**:
1. Serializer adds bytes correctly
2. Order is {0x82, 0x21}
3. Bytes prepended, not appended

### If Size Too Large

**Check**:
1. Using CompactSerializer (not BinarySerializer)
2. Packed structures where appropriate
3. String table compression enabled

---

## Time Budget

- Test 1 (round-trip): 30 min
- Test 2 (magic bytes): 15 min
- Test 3 (cross-format): 35 min
- Test 4 (integration): 40 min
- **Total**: ~2 hours

---

## After Session 93 Complete

**Next**: Read `doc/SESSION_90_CONTINUATION_PROMPT.md` (build system integration)

Or if testing reveals issues: Fix and re-test before proceeding.

---

**Created**: 2026-01-06 18:47 HKT
**Session**: 93
**Goal**: Validate Modern Thrift through comprehensive testing
**Dependencies**: Session 92 ✅ complete