# Session 77: Complete Frozen2 Port - Continuation Prompt

**Start Here**: Full port of dwarfs-rs Frozen2 system to C++

---

## Mission

**Port COMPLETE Frozen2 implementation from dwarfs-rs** - no shortcuts, nothing missing.

**Scope**:
- ✅ Schema type system (Schema, SchemaLayout, SchemaField, DenseMap)
- ✅ Schema generation from domain::metadata
- ✅ Schema serialization/deserialization (Thrift CompactProtocol)
- ✅ Frozen2 bit-packing serialization
- ✅ Frozen2 bit-unpacking deserialization
- ✅ Complete integration with writer and reader
- ✅ Comprehensive unit and integration tests

**Time**: 10 hours

---

## Quick Start

### Step 1: Read Key Documentation (30 min)

```bash
# Full implementation plan
cat doc/SESSION_77_CONTINUATION_PLAN.md

# Status tracker
cat doc/SESSION_77_IMPLEMENTATION_STATUS.md

# Session 76 summary
cat doc/SESSION_76_COMPLETION_STATUS.md
```

### Step 2: Study dwarfs-rs Implementation (60 min)

**MUST READ ALL** before coding:

```bash
cd /Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata

# 1. Schema types (138-289)
cat ../metadata.rs | sed -n '138,289p'

# 2. Frozen2 serialization (COMPLETE FILE)
cat ser_frozen.rs

# 3. Frozen2 deserialization (COMPLETE FILE)
cat de_frozen.rs

# 4. Schema serialization
cat ser_thrift.rs

# 5. Schema deserialization
cat de_thrift.rs

# 6. Entry points
cat ../metadata.rs | grep -A5 "to_schema_and_bytes\|pub fn parse"
```

### Step 3: Port Schema System (2 hours)

**Phase 1 Tasks**:

1. **Create Schema Types** (30 min)
   - Port `frozen_schema.h` from metadata.rs:138-289
   - DenseMap, SchemaField, SchemaLayout, Schema
   - Validation methods

2. **Implement Schema Generator** (60 min)
   - Port `frozen_schema_generator.cpp` from ser_frozen.rs
   - Generate layouts for ALL struct types
   - Calculate field offsets and bit widths
   - Build complete schema map

3. **Implement Schema Ser/De** (30 min)
   - Port `frozen_schema_serializer.cpp` from ser_thrift.rs + de_thrift.rs
   - Serialize schema to Thrift CompactProtocol
   - Deserialize schema from Thrift CompactProtocol

### Step 4: Port Frozen2 Serialization (3 hours)

**Phase 2 Tasks**:

1. **Create Bit Writer** (45 min)
   - Port `frozen_bit_writer.cpp` from ser_frozen.rs
   - write_bits(), write_bits_advancing(), align_to_byte()

2. **Port Frozen2 Serializer Core** (60 min)
   - Port `frozen2_serializer.cpp` main logic
   - serialize() entry point
   - Schema generation + bit-packing coordination

3. **Port ALL Type Serializers** (75 min)
   - Port EVERY struct type serialization
   - Port EVERY collection serialization
   - Port EVERY primitive serialization
   - Match dwarfs-rs EXACTLY

### Step 5: Port Frozen2 Deserialization (2 hours)

**Phase 3 Tasks**:

1. **Create Bit Reader** (30 min)
   - Port `frozen_bit_reader.cpp` from de_frozen.rs
   - read_bits(), read_struct()

2. **Port Frozen2 Deserializer Core** (45 min)
   - Port `frozen2_deserializer.cpp` main logic
   - deserialize() entry point
   - Schema validation + bit-unpacking coordination

3. **Port ALL Type Deserializers** (45 min)
   - Port EVERY struct type deserialization
   - Port EVERY collection deserialization
   - Port EVERY primitive deserialization
   - Match dwarfs-rs EXACTLY

### Step 6: Integration (1.5 hours)

**Phase 4 Tasks**:

1. **Update metadata_freezer** (30 min)
   ```cpp
   if (format_ == SerializationFormat::LEGACY_THRIFT) {
     auto [schema, frozen_data] = legacy::Frozen2Serializer::serialize(data);
     auto schema_bytes = legacy::FrozenSchemaSerializer::serialize(schema);
     auto schema_buffer = malloc_byte_buffer::create(schema_bytes);
     auto data_buffer = malloc_byte_buffer::create(frozen_data);
     return {schema_buffer.share(), data_buffer.share()};
   }
   ```

2. **Create Legacy Thrift Reader** (30 min)
   - New `legacy_thrift_metadata_reader.cpp`
   - Use Frozen2Deserializer
   - Integrate with reader factory

3. **Update CMake** (30 min)
   - Add ALL 14 new files
   - Verify build

### Step 7: Testing & Validation (1.5 hours)

**Phase 5 Tasks**:

1. **Unit Tests** (45 min)
   - Schema generation tests
   - Bit operations tests
   - Frozen2 ser/de tests
   - Round-trip tests

2. **Critical Compatibility Test** (45 min)

```bash
# Rebuild
ninja -C build-fb-only

# Create our Legacy Thrift image
./build-fb-only/mkdwarfs -i /tmp/dwarfs-test-data \
  -o /tmp/test-legacy-full.dth --format=thrift

# CRITICAL TEST 1: Homebrew reads ours
echo "=== CRITICAL: Homebrew reads our Legacy Thrift ==="
/opt/homebrew/bin/dwarfsck /tmp/test-legacy-full.dth --check-integrity
/opt/homebrew/bin/dwarfsextract -i /tmp/test-legacy-full.dth \
  -o /tmp/extracted-homebrew-ours
diff -r /tmp/dwarfs-test-data /tmp/extracted-homebrew-ours

# CRITICAL TEST 2: We read Homebrew
echo "=== CRITICAL: We read Homebrew image ==="
./build-fb-only/dwarfsck -i /tmp/test-homebrew.dwarfs --check-integrity
./build-fb-only/dwarfsextract -i /tmp/test-homebrew.dwarfs \
  -o /tmp/extracted-ours-homebrew
diff -r /tmp/dwarfs-test-data /tmp/extracted-ours-homebrew

# If BOTH pass: SUCCESS!
echo "✅✅✅ HOMEBREW COMPATIBILITY VERIFIED!"
```

---

## Success Criteria (ALL must pass)

### Implementation Complete
- [ ] All 14 new files created
- [ ] All dwarfs-rs logic ported
- [ ] All types handled
- [ ] All validations included
- [ ] Build successful
- [ ] No compiler warnings

### Functionality
- [ ] Schema generation works for all metadata types
- [ ] Schema ser/de round-trips correctly
- [ ] Frozen2 serialization produces valid data
- [ ] Frozen2 deserialization reconstructs metadata
- [ ] Round-trip: metadata → Frozen2 → metadata (identity)

### Homebrew Compatibility (CRITICAL)
- [ ] ✅ Homebrew v0.14.1 reads our Legacy Thrift images
- [ ] ✅ Homebrew checks integrity without errors
- [ ] ✅ Homebrew extracts all files
- [ ] ✅ Extracted files match byte-for-byte
- [ ] ✅ We read Homebrew v0.14.1 images
- [ ] ✅ We extract Homebrew images correctly
- [ ] ✅ Metadata semantically equivalent

---

## Critical Files Checklist

### Must Create (14 files)

**Headers**:
- [ ] `include/dwarfs/metadata/legacy/frozen_schema.h`
- [ ] `include/dwarfs/metadata/legacy/frozen_schema_generator.h`
- [ ] `include/dwarfs/metadata/legacy/frozen_schema_serializer.h`
- [ ] `include/dwarfs/metadata/legacy/frozen_bit_writer.h`
- [ ] `include/dwarfs/metadata/legacy/frozen_bit_reader.h`
- [ ] `include/dwarfs/metadata/legacy/frozen2_serializer.h`
- [ ] `include/dwarfs/metadata/legacy/frozen2_deserializer.h`

**Implementation**:
- [ ] `src/metadata/legacy/frozen_schema.cpp`
- [ ] `src/metadata/legacy/frozen_schema_generator.cpp`
- [ ] `src/metadata/legacy/frozen_schema_serializer.cpp`
- [ ] `src/metadata/legacy/frozen_bit_writer.cpp`
- [ ] `src/metadata/legacy/frozen_bit_reader.cpp`
- [ ] `src/metadata/legacy/frozen2_serializer.cpp`
- [ ] `src/metadata/legacy/frozen2_deserializer.cpp`

### Must Modify (3 files)

- [ ] `src/writer/internal/metadata_freezer.cpp:111` - Add Frozen2 case
- [ ] `src/reader/internal/metadata_factory.cpp` - Add Legacy Thrift reader
- [ ] `cmake/metadata_serialization.cmake` - Add new files

### Must Copy From dwarfs-rs

- [ ] `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs` → frozen2_serializer.cpp
- [ ] `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/de_frozen.rs` → frozen2_deserializer.cpp
- [ ] `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata.rs:138-289` → frozen_schema.h/cpp

---

## Porting Reference

### Example: Port Rust struct to C++

**Rust** (metadata.rs:178-186):
```rust
pub struct SchemaField {
    pub layout_id: i16,
    pub offset: i16,
}

impl SchemaField {
    fn offset_bits(&self) -> u16 {
        let o = self.offset;
        if o >= 0 { o as u16 * 8 } else { (-o) as u16 }
    }
}
```

**C++** (frozen_schema.h):
```cpp
struct SchemaField {
  int16_t layout_id;
  int16_t offset;

  uint16_t offset_bits() const {
    int16_t o = offset;
    return o >= 0 ? static_cast<uint16_t>(o) * 8
                  : static_cast<uint16_t>(-o);
  }
};
```

---

## Key Architectural Points

### 1. Two-Phase Serialization

```
Phase 1: Schema Generation
  domain::metadata → analyze structure → generate Schema

Phase 2: Frozen2 Bit-Packing
  domain::metadata + Schema → bit-pack → frozen bytes
```

### 2. Schema-Driven Deserialization

```
Step 1: Parse Schema (Thrift CompactProtocol)
Step 2: Use Schema to Unpack Frozen Data (bit-level read)
Step 3: Reconstruct domain::metadata
```

### 3. Complete Type Coverage

**Must handle ALL types in domain::metadata**:
- chunks, directories, inodes, chunk_table, entry_table
- symlink_table, uids, gids, modes, names, symlinks
- timestamp_base, chunk_inode_offset, link_inode_offset
- block_size, total_fs_size
- devices, options, dir_entries, shared_files_table
- total_hardlink_size, dwarfs_version, create_timestamp
- compact_names, compact_symlinks
- preferred_path_separator, features, category_names
- block_categories, reg_file_size_cache

**NO fields can be skipped!**

---

## Time Budget

| Task | Duration | Cumulative |
|------|----------|------------|
| Read documentation | 30 min | 0:30 |
| Study dwarfs-rs | 60 min | 1:30 |
| Port schema types | 2 hours | 3:30 |
| Port Frozen2 serialization | 3 hours | 6:30 |
| Port Frozen2 deserialization | 2 hours | 8:30 |
| Integration | 1.5 hours | 10:00 |
| Testing | included | 10:00 |

**Total**: 10 hours for complete implementation

---

## Quick Reference Commands

```bash
# Build after each phase
ninja -C build-fb-only

# Test after serialization complete
./build-fb-only/mkdwarfs -i /tmp/dwarfs-test-data \
  -o /tmp/test-frozen2.dth --format=thrift

# Critical test after integration
/opt/homebrew/bin/dwarfsck /tmp/test-frozen2.dth --check-integrity

# Success = no errors!
```

---

**Created**: 2026-01-05 13:21 HKT
**Scope**: COMPLETE Frozen2 port from dwarfs-rs
**Files**: 14 new + 3 modified
**Time**: 10 hours
**Next**: Start Phase 1 - Schema types