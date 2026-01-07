# Session 65: Phase 4 Integration - START HERE

**Date**: TBD (After Session 64 Part 2)
**Status**: 🟡 **READY TO START**
**Duration**: ~3 hours
**Mode**: Code Mode

---

## Quick Context

**What Was Completed in Session 64**:
- ✅ Part 1: Analyzed Frozen2 (40 min)
- ✅ Part 2: Added i64 support (25 min)
- ✅ All 56 tests passing

**What We're Doing Now**:
Integrate Legacy Thrift format into the serialization facade system so it works alongside FlatBuffers.

---

## Session 65 Objective

**Goal**: Create `LegacyThriftFacade` and integrate into serialization registry

**Why**: This enables:
- Automatic format detection
- Cross-format conversion (Legacy ↔ FlatBuffers)
- Unified API for all formats
- Homebrew v0.14.1 compatibility

---

## Implementation Steps (3 hours)

### Step 1: Create LegacyThriftFacade (1h)

**Create**:
1. `include/dwarfs/metadata/serialization/legacy_thrift_facade.h`
2. `src/metadata/serialization/legacy_thrift_facade.cpp`

**What to Implement**:
```cpp
namespace dwarfs::metadata::serialization {

class LegacyThriftFacade : public MetadataSerializationFacade {
public:
  void serialize(domain::metadata const& meta,
                 std::vector<uint8_t>& output) override {
    legacy::LegacyMetadataSerializer::serialize(meta, output);
  }

  void deserialize(std::span<uint8_t const> data,
                   domain::metadata& meta) override {
    legacy::LegacyMetadataSerializer::deserialize(data, meta);
  }

  SerializationFormat get_format() const override {
    return SerializationFormat::LEGACY_THRIFT;
  }

  std::string_view format_name() const override {
    return "Legacy Thrift";
  }
};

} // namespace
```

### Step 2: Registry Integration (1h)

**Modify**:
1. `include/dwarfs/metadata/serialization/serialization_format.h`
   - Add `LEGACY_THRIFT` to enum

2. `src/metadata/serialization/serializer_registry.cpp`
   - Register `LegacyThriftFacade`
   - Update `detect_format()` logic

3. `src/metadata/serialization/init_serializers.cpp`
   - Call Legacy Thrift registration

**Format Detection Priority**:
1. Check for FlatBuffers magic ("DFBF")
2. Check for Thrift magic (if fbthrift available)
3. Default to Legacy Thrift (backward compat)

### Step 3: Cross-Format Testing (30m)

**Update Tests**:
1. `test/metadata/serialization_test.cpp`
   - Add Legacy Thrift to format test matrix

2. `test/metadata/format_conversion_test.cpp`
   - Test Legacy → FlatBuffers
   - Test FlatBuffers → Legacy

### Step 4: Documentation (30m)

**Update**:
1. `.kilocode/rules/memory-bank/context.md`
2. `.kilocode/rules/memory-bank/architecture.md`
3. `doc/SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md`

---

## Build & Test Commands

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
# All metadata tests
./build-all/dwarfs_unit_tests --gtest_filter="*Metadata*"

# Legacy-specific tests
./build-all/frozen_bits_tests
./build-all/metadata_serializer_tests
./build-all/legacy_thrift_tests

# Format conversion tests
./build-all/dwarfs_unit_tests --gtest_filter="*FormatConversion*"
```

---

## Success Criteria

### Session 65 Complete When:

- [ ] `LegacyThriftFacade` implemented
- [ ] Registered in `serializer_registry`
- [ ] Format detection autoprompt detects Legacy format
- [ ] Cross-format conversion tests passing
- [ ] All tests passing (100+ tests)
- [ ] Documentation updated
- [ ] Memory bank updated

### Integration Quality:

- [ ] Clean facade implementation
- [ ] Works with existing serialization infrastructure
- [ ] No breaking changes to other formats
- [ ] Comprehensive test coverage

---

## Reference Documents

**Session 64 Output**:
- [`doc/SESSION_64_PART2_COMPLETION_SUMMARY.md`](SESSION_64_PART2_COMPLETION_SUMMARY.md) - What was completed
- [`doc/SESSION_64_IMPLEMENTATION_STATUS.md`](SESSION_64_IMPLEMENTATION_STATUS.md) - Full status

**Background**:
- [`doc/SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md`](SESSION_62_LEGACY_THRIFT_ARCHITECTURE.md) - Format architecture
- [`doc/SESSION_63_COMPLETION_SUMMARY.md`](SESSION_63_COMPLETION_SUMMARY.md) - Phase 2 completion

**Existing Facades** (for reference):
- `src/metadata/serialization/flatbuffers_serializer.cpp`
- `src/metadata/serialization/thrift_compact_serializer.cpp`

---

## Expected Timeline

**Session 64**: 1h 5m - ✅ COMPLETE
- Part 1: 40m (Analysis)
- Part 2: 25m (Implementation + Tests)

**Session 65**: 3h - ⏳ PENDING
- Step 1: 1h (Facade)
- Step 2: 1h (Registry)
- Step 3: 30m (Testing)
- Step 4: 30m (Docs)

**Total Remaining**: 3 hours (vs 12 planned = 75% time savings!)

---

**Created**: 2026-01-01 17:07 HKT
**Status**: Ready to begin Session 65 (Code Mode)
**Next Action**: Read this prompt, implement LegacyThriftFacade