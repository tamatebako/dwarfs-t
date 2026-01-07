# Session 79: Complete Frozen2 Implementation - Continuation Prompt

**Start Here**: Implement complete Frozen2 serialization with tests

---

## Quick Start

### Step 1: Review Context (10 min)

```bash
# Read Session 77-78 summaries
cat doc/SESSION_77_COMPLETION_SUMMARY.md
cat doc/SESSION_78_COMPLETION_SUMMARY.md

# Read continuation plan
cat doc/SESSION_79_CONTINUATION_PLAN.md

# Read implementation status
cat doc/SESSION_79_IMPLEMENTATION_STATUS.md
```

### Step 2: Study Reference Implementation (20 min)

```bash
cd /Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata

# Complete serializer (961 lines)
cat ser_frozen.rs

# Pay special attention to:
# - Lines 100-257: Layout system
# - Lines 511-857: ValueSerializer
# - Lines 859-961: Tests (MUST PORT THESE)
```

### Step 3: Implement Systematically (10 hours)

Follow the phases in [`SESSION_79_CONTINUATION_PLAN.md`](SESSION_79_CONTINUATION_PLAN.md):

1. **Phase 1** (3h): Layout System
   - Complete all Layout classes
   - Implement finish() optimization
   - Build all helper functions

2. **Phase 2** (2h): Struct Type Builders
   - All domain types (chunk, directory, inode_data, etc.)
   - Complete metadata builder (30+ fields)

3. **Phase 3** (1h): Schema Conversion
   - Verify cvt_layout() works correctly
   - Test DenseMap construction

4. **Phase 4** (4h): Value Serialization
   - ValueSerializer class
   - All primitive/collection/struct serializers
   - Type-specific serializers for all 30+ types

5. **Phase 5** (0.5h): Entry Point
   - Complete serialize() method
   - Integration test

6. **Phase 6** (2h): Test Suite
   - Port all 3 tests from dwarfs-rs
   - Verify byte-for-byte match
   - Integration test with Homebrew

---

## Critical Requirements

### MUST Port dwarfs-rs Tests

**File**: `test/frozen2_serializer_test.cpp` (NEW)

Port these 3 tests verbatim:
1. `smoke` test (ser_frozen.rs:864-886)
2. `bytes` test (ser_frozen.rs:888-915)
3. `collection` test (ser_frozen.rs:917-960)

**Expected byte output MUST match dwarfs-rs exactly**.

### MUST Achieve Byte-for-Byte Compatibility

The output MUST be identical to dwarfs-rs for Homebrew v0.14.1 compatibility.

### MUST Follow Architecture Principles

- Object-oriented design
- MECE (Mutually Exclusive, Collectively Exhaustive)
- Separation of concerns
- No code guards, use architecture instead
- One class, one responsibility

---

## Implementation File

**Target**: `src/metadata/legacy/frozen2_serializer.cpp`
**Current**: 86-line stub
**Target**: ~1,500 lines complete implementation

**Structure**:
```cpp
namespace dwarfs::metadata::legacy {
namespace {
  // Layout classes (LayoutNone, LayoutPrimitive, LayoutStruct, LayoutCollection)
  // cvt_layout() function
  // Helper builders (make_primitive, build_bool, build_u32, etc.)
  // Collection builders (build_vector, build_optional, etc.)
  // Struct builders (build_chunk, build_directory, etc.)
  // build_metadata() - complete 30+ fields
  // ValueSerializer class
  // Primitive serializers
  // Collection serializers  
  // Struct serializers
  // Type-specific serializers for all domain types
}

// Public API
std::pair<Schema, std::vector<uint8_t>>
Frozen2Serializer::serialize(metadata const& meta) {
  // Complete implementation as per ser_frozen.rs:28-55
}
}
```

---

## Testing Strategy

### Unit Tests

Create `test/frozen2_serializer_test.cpp`:

```cpp
#include <gtest/gtest.h>
#include "dwarfs/metadata/legacy/frozen2_serializer.h"

namespace dwarfs::metadata::legacy::test {

// Port ser_frozen.rs:864-886
TEST(Frozen2SerializerTest, SmokeTest) {
  domain::metadata meta;
  auto opts = domain::fs_options{};
  opts.mtime_only = true;
  opts.time_resolution_sec = 42;
  meta.options = opts;
  
  auto [schema, out] = Frozen2Serializer::serialize(meta);
  
  // Verify exact byte output
  std::vector<uint8_t> expected = {
    1,              // options.is_some = true
    1,              // options.inner.mtime_only = true
    1,              // options.inner.time_resolution.is_some = true
    42, 0, 0, 0,    // options.inner.time_resolution.inner = 42
  };
  EXPECT_EQ(out, expected);
}

// Port ser_frozen.rs:888-915 (bytes test)
TEST(Frozen2SerializerTest, BytesTest) {
  // TODO: Port complete test
}

// Port ser_frozen.rs:917-960 (collection test)
TEST(Frozen2SerializerTest, CollectionTest) {
  // TODO: Port complete test
}

} // namespace
```

### Build and Run

```bash
# Build with tests
cmake -B build-fb-only -GNinja \
  -DWITH_TESTS=ON \
  -DWITH_LIBDWARFS=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON

ninja -C build-fb-only

# Run frozen2 tests only
build-fb-only/dwarfs_unit_tests --gtest_filter='Frozen2Serializer*'
```

---

## Success Criteria

### Phase Completion

- [x] Phase 1: Layout System - ALL types implemented
- [x] Phase 2: Struct Builders - ALL 30+ types
- [x] Phase 3: Schema Conversion - Verified working
- [x] Phase 4: Value Serialization - ALL serializers
- [x] Phase 5: Entry Point - Complete serialize()
- [x] Phase 6: Tests - ALL 3 tests passing

### Build Status

- [x] Compiles without errors
- [x] Compiles without warnings
- [x] Links successfully
- [x] Tests build

### Test Results

- [x] smoke test PASSES
- [x] bytes test PASSES
- [x] collection test PASSES
- [x] Byte output matches dwarfs-rs exactly

### Integration

- [x] Can create DwarFS image with legacy Thrift format
- [x] Homebrew v0.14.1 can read our images
- [x] Files extract correctly
- [x] Checksums match

---

## Common Issues & Solutions

### Issue: Template compilation errors

**Solution**: Use `std::function` instead of function pointers for builders:
```cpp
template<typename T>
std::unique_ptr<Layout> build_vector(
    std::vector<T> const& vec,
    std::function<std::unique_ptr<Layout>(T const&)> builder);
```

### Issue: DenseMap field access

**Solution**: Use `raw_data()` method:
```cpp
ret_fields.raw_data() = std::move(field_vec);
```

### Issue: Ternary with std::make_unique

**Solution**: Use if-else instead:
```cpp
if (size == 0) {
  return std::make_unique<LayoutNone>();
} else {
  return std::make_unique<LayoutPrimitive>(size);
}
```

### Issue: Byte order

**Solution**: Always use little-endian explicitly:
```cpp
void serialize_u32(uint32_t v) {
  uint8_t bytes[4];
  bytes[0] = v & 0xFF;
  bytes[1] = (v >> 8) & 0xFF;
  bytes[2] = (v >> 16) & 0xFF;
  bytes[3] = (v >> 24) & 0xFF;
  put_prim(bytes, 4);
}
```

---

## After Completion

1. **Update memory bank** with completion status
2. **Create SESSION_79_COMPLETION_SUMMARY.md**
3. **Test Homebrew compatibility** with real v0.14.1
4. **Update main README** if needed
5. **Archive old session docs** to old-docs/

---

**Created**: 2026-01-05 17:02 HKT
**Session**: 79
**Goal**: Complete Frozen2 implementation + tests
**Time**: 10-12 hours
**Next**: Session 80 - Deserialization or Integration (if needed)
