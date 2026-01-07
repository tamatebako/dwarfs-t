# Session 65: Phase 4 Integration Plan

**Date**: TBD (After Session 64 Part 2)
**Status**: 🟡 **READY TO START**
**Duration**: ~3 hours
**Prerequisite**: Session 64 Part 2 complete ✅

---

## Executive Summary

**Goal**: Integrate Legacy Thrift format into the serialization facade system

**What We Have** (Session 64):
- ✅ Full Thrift Compact implementation
- ✅ i64 support (no u64 truncation)
- ✅ All 56 tests passing
- ✅ frozen_bits ready (for future Frozen2 optimization)

**What We Need** (Session 65):
- Facade implementation
- Registry integration
- Cross-format testing
- Documentation updates

---

## Phase 4 Implementation Plan (3 hours)

### Step 1: Create LegacyThriftFacade (1 hour)

**Goal**: Implement `MetadataSerializationFacade` interface for Legacy Thrift

**Files to Create**:
1. `include/dwarfs/metadata/serialization/legacy_thrift_facade.h` (~60 lines)
2. `src/metadata/serialization/legacy_thrift_facade.cpp` (~80 lines)

**Interface to Implement**:
```cpp
class LegacyThriftFacade : public MetadataSerializationFacade {
public:
  void serialize(domain::metadata const& meta,
                 std::vector<uint8_t>& output) override;

  void deserialize(std::span<uint8_t const> data,
                   domain::metadata& meta) override;

  SerializationFormat get_format() const override {
    return SerializationFormat::LEGACY_THRIFT;
  }

  std::string_view format_name() const override {
    return "Legacy Thrift";
  }
};
```

**Implementation**:
- Delegate to `LegacyMetadataSerializer::serialize/deserialize`
- Add error handling and validation
- Support metadata versioning if needed

### Step 2: Registry Integration (1 hour)

**Goal**: Register Legacy Thrift in serialization system

**Files to Modify**:
1. `include/dwarfs/metadata/serialization/serialization_format.h` (~5 lines)
   - Add `LEGACY_THRIFT` enum value

2. `src/metadata/serialization/serializer_registry.cpp` (~30 lines)
   - Register `LegacyThriftFacade`
   - Add format detection (no FlatBuffers magic = Legacy Thrift)
   - Set fallback priority

3. `src/metadata/serialization/init_serializers.cpp` (~10 lines)
   - Call registration function

**Format Detection Logic**:
```cpp
optional<SerializationFormat> detect_format(span<uint8_t const> data) {
  // Check FlatBuffers magic first
  if (has_flatbuffers_magic(data)) {
    return SerializationFormat::FLATBUFFERS;
  }

  // Check for Thrift magic (if fbthrift available)
  if (has_thrift_magic(data)) {
    return SerializationFormat::THRIFT;
  }

  // Default: Legacy Thrift (no magic bytes)
  return SerializationFormat::LEGACY_THRIFT;
}
```

### Step 3: Cross-Format Testing (30 minutes)

**Goal**: Verify Legacy Thrift works with existing infrastructure

**Files to Update**:
1. `test/metadata/serialization_test.cpp` (~50 lines)
   - Add Legacy Thrift to format matrix
   - Test round-trip for all formats
   - Test cross-format conversion

2. `test/metadata/format_conversion_test.cpp` (~40 lines)
   - Legacy → FlatBuffers
   - FlatBuffers → Legacy
   - Legacy → Thrift (if available)

**Test Cases**:
```cpp
TEST(FormatConversion, LegacyToFlatBuffers) {
  metadata original = create_test_metadata();

  // Serialize with Legacy Thrift
  auto legacy_facade = create_facade(SerializationFormat::LEGACY_THRIFT);
  vector<uint8_t> legacy_data;
  legacy_facade->serialize(original, legacy_data);

  // Deserialize with FlatBuffers
  auto fb_facade = create_facade(SerializationFormat::FLATBUFFERS);
  metadata converted;
  fb_facade->deserialize(legacy_data, converted);  // Should auto-detect Legacy

  EXPECT_EQ(converted, original);
}
```

### Step 4: Documentation (30 minutes)

**Goal**: Update architecture and user documentation

**Files to Update**:
1. `.kilocode/rules/memory-bank/architecture.md` (~20 lines)
   - Add Legacy Thrift to format diagram
   - Update serialization flow

2. `doc/SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md` (~30 lines)
   - Document i64 support
   - Update wire format specification
   - Add performance characteristics

3. `README.md` or `doc/dwarfs-format.md` (~10 lines)
   - Mention Legacy Thrift availability
   - Document format detection

---

## Build Commands

**Configure**:
```bash
cmake -B build-all -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
```

**Build**:
```bash
ninja -C build-all dwarfs_unit_tests
```

**Test**:
```bash
./build-all/dwarfs_unit_tests --gtest_filter="*Legacy*"
./build-all/dwarfs_unit_tests --gtest_filter="*FormatConversion*"
```

---

## Success Criteria

### Phase 4 Complete When:

- [ ] `LegacyThriftFacade` implemented
- [ ] Registered in `serializer_registry`
- [ ] Format detection works (auto-detect Legacy)
- [ ] Cross-format conversion tests passing
- [ ] All integration tests passing
- [ ] Documentation updated

### Code Quality:

- [ ] Clean facade implementation
- [ ] Proper error handling
- [ ] Comprehensive integration tests
- [ ] Clear documentation

---

## Files Summary

**New Files** (2 files, ~140 lines):
1. `include/dwarfs/metadata/serialization/legacy_thrift_facade.h` (~60 lines)
2. `src/metadata/serialization/legacy_thrift_facade.cpp` (~80 lines)

**Modified Files** (6 files, ~165 lines changes):
1. `include/dwarfs/metadata/serialization/serialization_format.h` (~5 lines)
2. `src/metadata/serialization/serializer_registry.cpp` (~30 lines)
3. `src/metadata/serialization/init_serializers.cpp` (~10 lines)
4. `test/metadata/serialization_test.cpp` (~50 lines)
5. `test/metadata/format_conversion_test.cpp` (~40 lines)
6. Documentation files (~30 lines)

**Total**: 8 files, ~305 lines

---

## Expected Outcomes

**After Session 65**:
1. ✅ Full 3-format support: FlatBuffers, Thrift, Legacy Thrift
2. ✅ Automatic format detection
3. ✅ Cross-format conversion working
4. ✅ Homebrew v0.14.1 compatibility achieved
5. ✅ All tests passing (100+ tests)

**Remaining Work**:
- None! Full backward compatibility achieved
- Frozen2 optimization deferred (optional)

---

## Risk Assessment

**Risks**: None identified

**Mitigations**:
- All code already tested in isolation
- Clean facade pattern minimizes integration risk
- Comprehensive test coverage

---

**Created**: 2026-01-01 17:06 HKT
**Status**: Ready to begin Session 65
**Estimated Duration**: 3 hours (conservative)