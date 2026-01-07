# Session 79: Complete Frozen2 Implementation - Continuation Plan

**Created**: 2026-01-05 17:00 HKT
**Prerequisite**: Session 77 (infrastructure) + Session 78 (analysis) complete
**Goal**: Full Frozen2 serialization with tests from dwarfs-rs
**Estimated Time**: 10-12 hours

---

## Mission

Complete the Frozen2 serialization implementation by:
1. Porting all ~1,450 lines from dwarfs-rs ser_frozen.rs
2. Porting all tests from dwarfs-rs (ser_frozen.rs:859-961)
3. Achieving byte-for-byte compatibility with dwarfs-rs output
4. Enabling Homebrew v0.14.1 compatibility

---

## Phase 1: Layout System Implementation (3 hours)

### Task 1.1: Layout Class Hierarchy (30 min)
**File**: `src/metadata/legacy/frozen2_serializer.cpp`
**Reference**: `ser_frozen.rs:100-257`

Port complete Layout classes:
- ✅ `Layout` base class (already stubbed)
- ✅ `LayoutNone` (already stubbed)
- ✅ `LayoutPrimitive` (already stubbed)
- ✅ `LayoutStruct` (already stubbed)
- ✅ `LayoutCollection` (already stubbed)

**Add**:
- Virtual destructors
- Move constructors/assignment
- Proper polymorphism

### Task 1.2: Layout::finish() Optimization (45 min)
**Reference**: `ser_frozen.rs:143-193`

Implement for each type:
- `LayoutNone::finish()` → return 0
- `LayoutPrimitive::finish()` → return byte_size
- `LayoutStruct::finish()`:
  - Accumulate field sizes
  - Check MAX_STRUCT_BYTE_SIZE
  - Convert to None if empty
- `LayoutCollection::finish()`:
  - Finalize element
  - Calculate distance_size (0 or 4)
  - Convert to Struct{distance, count, element}

### Task 1.3: Layout Builder Helpers (45 min)
**Reference**: `ser_frozen.rs:195-256`

Implement:
- `make_primitive(byte_size)` → LayoutNone or LayoutPrimitive
- `build_bool(bool)` → LayoutPrimitive(1) or LayoutNone
- `build_u32(uint32_t)` → LayoutPrimitive(4) or LayoutNone
- `build_u64(uint64_t)` → LayoutPrimitive(8) or LayoutNone
- `build_bytes(string)` → Struct{distance, count}

### Task 1.4: Collection Builders (60 min)
**Reference**: `ser_frozen.rs:308-315` (serialize_seq)

Implement:
- `build_vector<T>(vector, builder)`
- `build_optional<T>(optional, builder)`
- `build_set<T>(set, builder)`
- `build_map<K,V>(map, builder)`

**Key**: Merge all element layouts into one unified layout

---

## Phase 2: Struct Type Builders (2 hours)

### Task 2.1: Core Struct Types (60 min)

**chunk** (`ser_frozen.rs` + domain/chunk.h):
```cpp
std::unique_ptr<Layout> build_chunk(chunk const& c) {
  auto st = std::make_unique<LayoutStruct>();
  st->add_field(build_u32(c.block));
  st->add_field(build_u32(c.offset));
  st->add_field(build_u32(c.size));
  return st;
}
```

**directory**:
```cpp
std::unique_ptr<Layout> build_directory(directory const& d) {
  auto st = std::make_unique<LayoutStruct>();
  st->add_field(build_u32(d.first_entry));
  st->add_field(build_u32(d.parent_entry));
  st->add_field(build_u32(d.self_entry));
  return st;
}
```

**inode_data**, **dir_entry**, **fs_options**, **string_table**, 
**inode_size_cache**, **history_entry** (similar pattern)

### Task 2.2: metadata Type (60 min)
**Reference**: Domain model has 30+ fields

Build struct with all fields in order:
```cpp
std::unique_ptr<Layout> build_metadata(metadata const& meta) {
  auto st = std::make_unique<LayoutStruct>();
  // 30+ fields, each with appropriate builder
  st->add_field(build_vector(meta.chunks, build_chunk));
  st->add_field(build_vector(meta.directories, build_directory));
  // ... etc for all 30 fields
  return st;
}
```

---

## Phase 3: Schema Conversion (1 hour)

### Task 3.1: cvt_layout() Implementation (60 min)
**Reference**: `ser_frozen.rs:57-98`
**Status**: ✅ Already implemented (needs testing)

Verify:
- Handles LayoutNone → nullopt
- Handles LayoutPrimitive → SchemaLayout{bits}
- Handles LayoutStruct → SchemaLayout{fields}
- Handles LayoutCollection → convert via to_struct()
- Negative offsets for bit positions
- DenseMap construction

---

## Phase 4: Value Serialization (4 hours)

### Task 4.1: ValueSerializer Class (45 min)
**Reference**: `ser_frozen.rs:511-563`

Implement:
```cpp
class ValueSerializer {
  Layout const* layout_;
  std::vector<uint8_t>& buf_;
  uint32_t base_;
  uint32_t inline_pos_;
  
  uint32_t distance() const;
  void put_primitive(uint8_t const*, size_t);
};
```

### Task 4.2: Primitive Serializers (30 min)
**Reference**: `ser_frozen.rs:576-608`

Implement:
- `serialize_bool(bool)` → 1 byte
- `serialize_u32(uint32_t)` → 4 bytes LE
- `serialize_u64(uint64_t)` → 8 bytes LE
- `serialize_bytes(string)` → distance + count + outlined data

### Task 4.3: Optional Serializer (30 min)
**Reference**: `ser_frozen.rs:610-629`

Implement:
```cpp
template<typename T>
void serialize_optional(
    optional<T> const& opt,
    void (ValueSerializer::*serializer)(T const&))
```

Struct{is_some, inner}:
- None → {false, skip}
- Some(v) → {true, serialize(v)}

### Task 4.4: Collection Serializer (60 min)
**Reference**: `ser_frozen.rs:636-678`

Implement:
```cpp
template<typename T>
void serialize_vector(
    vector<T> const& vec,
    void (ValueSerializer::*serializer)(T const&))
```

Struct{distance, count, elements}:
- Calculate distance
- Write count
- Allocate outlined storage
- Serialize each element

### Task 4.5: Struct Serializer (60 min)
**Reference**: `ser_frozen.rs:631-635, 815-857`

Implement:
```cpp
class StructSerializer {
  void serialize_field(string_view name, auto const& value, auto serializer);
  void skip_field(string_view name);
};
```

Iterate fields, advance `inline_pos`

### Task 4.6: Type-Specific Serializers (60 min)

Implement for ALL types:
- `serialize_metadata(metadata const&)`
- `serialize_chunk(chunk const&)`
- `serialize_directory(directory const&)`
- `serialize_inode_data(inode_data const&)`
- etc. for all 30+ types

Each follows same pattern:
1. Cast layout to LayoutStruct
2. Get fields
3. Serialize each field with correct serializer
4. Advance inline_pos

---

## Phase 5: Entry Point Integration (30 min)

### Task 5.1: serialize() Method
**Reference**: `ser_frozen.rs:28-55`

Complete implementation:
```cpp
std::pair<Schema, std::vector<uint8_t>>
Frozen2Serializer::serialize(metadata const& meta) {
  // 1. Build layout
  auto layout = build_metadata(meta);
  
  // 2. Finalize
  auto size_opt = layout->finish();
  if (!size_opt) throw runtime_error("struct too large");
  
  // 3. Convert to schema
  vector<SchemaLayout> layout_vec;
  auto root_id = cvt_layout(layout.get(), layout_vec);
  if (!root_id) throw runtime_error("root empty");
  
  // Build Schema
  Schema schema;
  schema.layouts = DenseMap(move(layout_vec));
  schema.relax_type_checks = true;
  schema.root_layout = *root_id;
  schema.file_version = 1;
  
  // Adjust root size
  auto& root = schema.layouts.get(*root_id);
  root->size = (root->bits + 7) / 8;
  
  // 4. Serialize values
  vector<uint8_t> buf(*size_opt, 0);
  ValueSerializer ser(layout.get(), buf, 0, 0);
  serialize_metadata(meta, ser);
  
  return {schema, buf};
}
```

---

## Phase 6: Test Suite (2 hours)

### Task 6.1: Port dwarfs-rs Tests (60 min)
**Reference**: `ser_frozen.rs:859-961`

Port 3 test cases to C++ GoogleTest:
1. **smoke test** (lines 864-886):
   - Minimal metadata with options
   - Verify byte output matches expected

2. **bytes test** (lines 888-915):
   - Test string serialization
   - Verify distance field relaxation

3. **collection test** (lines 917-960):
   - Test vector serialization
   - Verify distance + count + elements
   - Test all-zero element handling

### Task 6.2: Write Test File (30 min)
**File**: `test/frozen2_serializer_test.cpp`

Structure:
```cpp
#include <gtest/gtest.h>
#include "dwarfs/metadata/legacy/frozen2_serializer.h"
#include "dwarfs/metadata/domain/metadata.h"

namespace dwarfs::metadata::legacy::test {

TEST(Frozen2SerializerTest, SmokeTest) {
  domain::metadata meta;
  // Set up minimal metadata
  
  auto [schema, bytes] = Frozen2Serializer::serialize(meta);
  
  // Verify schema
  EXPECT_EQ(schema.file_version, 1);
  EXPECT_TRUE(schema.relax_type_checks);
  
  // Verify bytes (exact match with dwarfs-rs output)
  std::vector<uint8_t> expected = {
    1, // options.is_some
    1, // options.inner.mtime_only
    1, // options.inner.time_resolution.is_some
    42, 0, 0, 0, // options.inner.time_resolution.inner
  };
  EXPECT_EQ(bytes, expected);
}

TEST(Frozen2SerializerTest, BytesTest) {
  // Port ser_frozen.rs:888-915
}

TEST(Frozen2SerializerTest, CollectionTest) {
  // Port ser_frozen.rs:917-960
}

} // namespace
```

### Task 6.3: Integration Test (30 min)

Test round-trip with dwarfs-rs:
1. Create DwarFS image with C++ implementation
2. Read with dwarfs-rs
3. Verify byte-for-byte match

---

## Success Criteria

### Code Complete ✓
- [ ] All Layout builders implemented
- [ ] All ValueSerializer methods implemented
- [ ] All type-specific serializers implemented
- [ ] serialize() entry point working
- [ ] Builds without errors/warnings

### Tests Pass ✓
- [ ] smoke test passes
- [ ] bytes test passes
- [ ] collection test passes
- [ ] Round-trip with dwarfs-rs succeeds

### Integration ✓
- [ ] Can create Homebrew-compatible image
- [ ] Homebrew v0.14.1 can read our images
- [ ] Byte-for-byte match with dwarfs-rs

---

## Implementation Strategy

### Best Approach
1. **Systematic porting**: Start with Layout, then Schema, then Value
2. **Test incrementally**: Add test after each phase
3. **Reference constantly**: Keep ser_frozen.rs open
4. **Validate early**: Test each component before moving on

### Common Pitfalls to Avoid
- ❌ Don't skip `finish()` - it's critical for optimization
- ❌ Don't forget distance field relaxation (empty elements)
- ❌ Don't mix up bit offsets (negative) vs byte offsets (positive)
- ❌ Don't forget little-endian byte order
- ❌ Don't skip merging element layouts in collections

---

## File Checklist

### Implementation
- [x] `src/metadata/legacy/frozen2_serializer.cpp` - STUB (needs ~1,450 lines)
- [ ] `test/frozen2_serializer_test.cpp` - NEW (needs creation)

### Documentation
- [x] `doc/SESSION_77_COMPLETION_SUMMARY.md` - Session 77 summary
- [x] `doc/SESSION_78_COMPLETION_SUMMARY.md` - Session 78 (this session)
- [ ] `doc/SESSION_79_COMPLETION_SUMMARY.md` - After implementation

### Configuration
- [x] `cmake/metadata_serialization.cmake` - Already includes frozen2_serializer.cpp

---

## Time Budget

| Phase | Task | Estimate | Cumulative |
|-------|------|----------|------------|
| 1 | Layout system | 3h | 3h |
| 2 | Struct builders | 2h | 5h |
| 3 | Schema conversion | 1h | 6h |
| 4 | Value serialization | 4h | 10h |
| 5 | Entry point | 0.5h | 10.5h |
| 6 | Tests | 2h | 12.5h |

**Total**: 12.5 hours for complete implementation + tests

---

**Created**: 2026-01-05 17:00 HKT
**Status**: Ready to start
**Next**: Session 79 - Full implementation
