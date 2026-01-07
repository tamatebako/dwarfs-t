# Session 77: Complete Frozen2 Port from dwarfs-rs

**Prerequisites**: Session 76 complete, dwarfs-rs available
**Goal**: **FULL PORT** of dwarfs-rs Frozen2 implementation to C++
**Scope**: Complete Frozen2 serialization + deserialization + schema generation

---

## Objective

Port **ALL** Frozen2 functionality from dwarfs-rs to C++ for complete Homebrew v0.14.1 compatibility:

1. **Schema Generation** - Generate Frozen2 layout from metadata structure
2. **Frozen2 Serialization** - Bit-pack metadata according to schema
3. **Frozen2 Deserialization** - Unpack bit-packed data using schema
4. **Schema Serialization** - Write schema in Thrift CompactProtocol format
5. **Schema Deserialization** - Read schema from Thrift CompactProtocol

**Reference**: `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/`
- `ser_frozen.rs` - Frozen2 serialization (schema generation + bit-packing)
- `de_frozen.rs` - Frozen2 deserialization (schema parsing + bit-unpacking)
- `ser_thrift.rs` - Thrift CompactProtocol serialization
- `de_thrift.rs` - Thrift CompactProtocol deserialization

---

## Architecture: Complete Frozen2 System

```
┌─────────────────────────────────────────────────────────┐
│            Domain Model (domain::metadata)              │
└────────────────┬────────────────────────────────────────┘
                 │
      ┌──────────┴──────────┐
      │                     │
      ▼                     ▼
┌──────────────┐      ┌──────────────┐
│  Serializer  │      │ Deserializer │
│              │      │              │
│ 1. Generate  │      │ 1. Parse     │
│    schema    │      │    schema    │
│ 2. Bit-pack  │      │ 2. Bit-      │
│    metadata  │      │    unpack    │
│              │      │    metadata  │
└──────┬───────┘      └──────┬───────┘
       │                     │
       ▼                     ▼
┌──────────────────────────────────┐
│   Two Sections in DwarFS Image   │
│                                  │
│ METADATA_V2_SCHEMA: Schema bytes │
│ METADATA_V2: Frozen data bytes   │
└──────────────────────────────────┘
```

---

## Phase 1: Port Schema Types (2 hours)

### File Structure

**Location**: `include/dwarfs/metadata/legacy/`

**Files to Create**:
1. `frozen_schema.h` - Schema types (Schema, SchemaLayout, SchemaField)
2. `frozen_schema_generator.h` - Generate schema from domain model
3. `frozen_schema_serializer.h` - Serialize/deserialize schema in Thrift format

### Task 1.1: Port Schema Data Types (30 min)

**Reference**: [`dwarfs-rs/dwarfs/src/metadata.rs:138-289`](../../../dwarfs-rs/dwarfs/src/metadata.rs:138)

Create `include/dwarfs/metadata/legacy/frozen_schema.h`:

```cpp
namespace dwarfs::metadata::legacy {

// Dense map: i16 -> T
template<typename T>
class DenseMap {
  std::vector<std::optional<T>> data_;
public:
  void insert(int16_t key, T value);
  std::optional<T> get(int16_t key) const;
  auto begin() const;
  auto end() const;
};

// Frozen schema field
struct SchemaField {
  int16_t layout_id;
  int16_t offset;  // If >= 0: byte offset * 8, if < 0: bit offset

  uint16_t offset_bits() const {
    return offset >= 0 ? offset * 8 : -offset;
  }
};

// Frozen schema layout
struct SchemaLayout {
  int32_t size;    // Total size in bytes
  int16_t bits;    // Bit width for primitives, or total bits for structs
  DenseMap<SchemaField> fields;
  std::string type_name;
};

// Frozen schema (file_version = 1)
struct Schema {
  bool relax_type_checks = false;
  DenseMap<SchemaLayout> layouts;
  int16_t root_layout = 0;
  int32_t file_version = 1;

  // Validate schema integrity
  void validate() const;
};

} // namespace
```

### Task 1.2: Port Schema Generator (60 min)

**Reference**: [`dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs`](../../../dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs)

Create `include/dwarfs/metadata/legacy/frozen_schema_generator.h`:

```cpp
namespace dwarfs::metadata::legacy {

class FrozenSchemaGenerator {
public:
  // Generate Frozen2 schema from domain metadata structure
  static Schema generate(domain::metadata const& meta);

private:
  // Generate layout for each struct type
  static SchemaLayout generate_metadata_layout();
  static SchemaLayout generate_chunk_layout();
  static SchemaLayout generate_directory_layout();
  static SchemaLayout generate_inode_layout();
  static SchemaLayout generate_dir_entry_layout();
  static SchemaLayout generate_string_table_layout();
  // ... all other struct types

  // Generate layout for primitive types
  static SchemaLayout generate_i32_layout();
  static SchemaLayout generate_i64_layout();
  static SchemaLayout generate_string_layout();
  static SchemaLayout generate_list_layout(SchemaLayout const& element);

  // Calculate struct layout (field offsets, total size)
  static void calculate_layout(SchemaLayout& layout);
};

} // namespace
```

### Task 1.3: Port Schema Serializer (30 min)

**Reference**: [`dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs`](../../../dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs) + [`de_thrift.rs`](../../../dwarfs-rs/dwarfs/src/metadata/de_thrift.rs)

Create `include/dwarfs/metadata/legacy/frozen_schema_serializer.h`:

```cpp
namespace dwarfs::metadata::legacy {

class FrozenSchemaSerializer {
public:
  // Serialize schema to Thrift CompactProtocol
  static std::vector<uint8_t> serialize(Schema const& schema);

  // Deserialize schema from Thrift CompactProtocol
  static Schema deserialize(std::span<uint8_t const> data);

private:
  // Use existing ThriftCompactWriter/Reader
  static void write_schema(ThriftCompactWriter& w, Schema const& s);
  static void write_layout(ThriftCompactWriter& w, SchemaLayout const& l);
  static void write_field(ThriftCompactWriter& w, SchemaField const& f);

  static void read_schema(ThriftCompactReader& r, Schema& s);
  static void read_layout(ThriftCompactReader& r, SchemaLayout& l);
  static void read_field(ThriftCompactReader& r, SchemaField& f);
};

} // namespace
```

---

## Phase 2: Port Frozen2 Serialization (3 hours)

### Task 2.1: Port Bit-Packing Utilities (45 min)

**Reference**: [`dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs`](../../../dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs)

Create `src/metadata/legacy/frozen_bit_writer.cpp`:

```cpp
namespace dwarfs::metadata::legacy {

class FrozenBitWriter {
  std::vector<uint8_t> buffer_;
  size_t bit_pos_ = 0;

public:
  // Write value at specific bit offset
  void write_bits(size_t bit_offset, size_t bit_width, uint64_t value);

  // Write at current position and advance
  void write_bits_advancing(size_t bit_width, uint64_t value);

  // Align to byte boundary
  void align_to_byte();

  // Get final buffer
  std::vector<uint8_t> take_buffer();
};

} // namespace
```

### Task 2.2: Port Frozen2 Struct Serializer (90 min)

**Reference**: Full `ser_frozen.rs` implementation

Create `src/metadata/legacy/frozen2_serializer.cpp`:

```cpp
namespace dwarfs::metadata::legacy {

class Frozen2Serializer {
public:
  // Main entry: Generate schema + serialize metadata
  static std::pair<Schema, std::vector<uint8_t>>
  serialize(domain::metadata const& meta);

private:
  Schema schema_;
  FrozenBitWriter writer_;
  DenseMap<int16_t> layout_cache_;  // Type -> layout_id

  // Serialize metadata struct using schema
  void serialize_metadata(domain::metadata const& meta, SchemaLayout const& layout);

  // Serialize each struct type
  void serialize_chunk(domain::chunk const& c, SchemaLayout const& layout);
  void serialize_directory(domain::directory const& d, SchemaLayout const& layout);
  void serialize_inode(domain::inode_data const& i, SchemaLayout const& layout);
  void serialize_dir_entry(domain::dir_entry const& e, SchemaLayout const& layout);
  void serialize_string_table(domain::string_table const& st, SchemaLayout const& layout);
  // ... all other types

  // Serialize collections
  void serialize_list(auto const& vec, SchemaLayout const& layout);
  void serialize_map(auto const& map, SchemaLayout const& layout);
  void serialize_set(auto const& set, SchemaLayout const& layout);

  // Serialize primitive types
  void serialize_i32(int32_t val, size_t bit_offset, size_t bit_width);
  void serialize_i64(int64_t val, size_t bit_offset, size_t bit_width);
  void serialize_string(std::string const& val, size_t bit_offset);
  void serialize_bool(bool val, size_t bit_offset);
};

} // namespace
```

### Task 2.3: Port Schema Generator (45 min)

Implement `frozen_schema_generator.cpp`:
- Generate layout for ALL metadata struct types
- Calculate field offsets and bit widths
- Build complete schema map
- Set root layout

---

## Phase 3: Port Frozen2 Deserialization (2 hours)

### Task 3.1: Port Bit-Reading Utilities (30 min)

**Reference**: [`dwarfs-rs/dwarfs/src/metadata/de_frozen.rs`](../../../dwarfs-rs/dwarfs/src/metadata/de_frozen.rs)

Create `src/metadata/legacy/frozen_bit_reader.cpp`:

```cpp
class FrozenBitReader {
  std::span<uint8_t const> buffer_;

public:
  // Read bits at specific offset
  uint64_t read_bits(size_t bit_offset, size_t bit_width) const;

  // Read struct at byte offset
  template<typename T>
  T read_struct(size_t byte_offset, SchemaLayout const& layout) const;
};
```

### Task 3.2: Port Frozen2 Deserializer (90 min)

Create `src/metadata/legacy/frozen2_deserializer.cpp`:

```cpp
class Frozen2Deserializer {
public:
  // Deserialize frozen data using schema
  static domain::metadata deserialize(
      Schema const& schema,
      std::span<uint8_t const> data);

private:
  // Deserialize each struct type
  domain::chunk read_chunk(size_t offset, SchemaLayout const& layout);
  domain::directory read_directory(size_t offset, SchemaLayout const& layout);
  domain::inode_data read_inode(size_t offset, SchemaLayout const& layout);
  // ... all types

  // Deserialize collections
  template<typename T>
  std::vector<T> read_list(size_t offset, SchemaLayout const& layout);

  // Deserialize primitives
  int32_t read_i32(size_t bit_offset, size_t bit_width);
  int64_t read_i64(size_t bit_offset, size_t bit_width);
  std::string read_string(size_t bit_offset);
  bool read_bool(size_t bit_offset);
};
```

---

## Phase 4: Integration (1.5 hours)

### Task 4.1: Update metadata_freezer (30 min)

Fix [`src/writer/internal/metadata_freezer.cpp`](../src/writer/internal/metadata_freezer.cpp:111):

```cpp
if (format_ == SerializationFormat::LEGACY_THRIFT) {
#ifdef DWARFS_HAVE_LEGACY_THRIFT
  // Use complete Frozen2 implementation
  auto [schema, frozen_data] = legacy::Frozen2Serializer::serialize(data);

  // Serialize schema to Thrift CompactProtocol
  auto schema_bytes = legacy::FrozenSchemaSerializer::serialize(schema);
  auto schema_buffer = malloc_byte_buffer::create(schema_bytes);

  // Frozen data is already serialized
  auto data_buffer = malloc_byte_buffer::create(frozen_data);

  ti << "freezing metadata (Legacy Thrift/Frozen2) to "
     << data_buffer.size() << " bytes...";

  return {schema_buffer.share(), data_buffer.share()};
#else
  throw std::runtime_error("Legacy Thrift not available");
#endif
}
```

### Task 4.2: Update Legacy Thrift Reader (30 min)

Update reader to use Frozen2 deserializer when schema present.

**File**: Create `src/reader/legacy_thrift_metadata_reader.cpp`

```cpp
class LegacyThriftMetadataReader : public MetadataReader {
public:
  void read(std::span<uint8_t const> schema_data,
            std::span<uint8_t const> metadata_data) override {
    // Deserialize schema from Thrift CompactProtocol
    auto schema = legacy::FrozenSchemaSerializer::deserialize(schema_data);

    // Deserialize metadata using Frozen2
    auto metadata = legacy::Frozen2Deserializer::deserialize(schema, metadata_data);

    // Store in domain model
    metadata_ = std::move(metadata);
  }
};
```

### Task 4.3: Update CMake Build (30 min)

Add all new files to `cmake/metadata_serialization.cmake`:

```cmake
set(LEGACY_THRIFT_SOURCES
  # Existing
  src/metadata/legacy/frozen_bits.cpp
  src/metadata/legacy/thrift_types.cpp
  src/metadata/legacy/thrift_compact_writer.cpp
  src/metadata/legacy/thrift_compact_reader.cpp
  src/metadata/legacy/legacy_metadata_serializer.cpp

  # NEW: Frozen2 schema
  src/metadata/legacy/frozen_schema.cpp
  src/metadata/legacy/frozen_schema_generator.cpp
  src/metadata/legacy/frozen_schema_serializer.cpp

  # NEW: Frozen2 bit operations
  src/metadata/legacy/frozen_bit_writer.cpp
  src/metadata/legacy/frozen_bit_reader.cpp

  # NEW: Frozen2 serialization
  src/metadata/legacy/frozen2_serializer.cpp
  src/metadata/legacy/frozen2_deserializer.cpp
)
```

---

## Phase 5: Testing & Validation (1.5 hours)

### Task 5.1: Unit Tests (45 min)

Create comprehensive tests for each component:

1. `test/metadata/legacy/frozen_schema_test.cpp`
2. `test/metadata/legacy/frozen_bit_operations_test.cpp`
3. `test/metadata/legacy/frozen2_serializer_test.cpp`
4. `test/metadata/legacy/frozen2_round_trip_test.cpp`

### Task 5.2: Integration Testing (45 min)

```bash
# Build
ninja -C build-fb-only

# Create Legacy Thrift image
./build-fb-only/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-legacy-full.dth --format=thrift

# CRITICAL: Homebrew compatibility
/opt/homebrew/bin/dwarfsck /tmp/test-legacy-full.dth --check-integrity
/opt/homebrew/bin/dwarfsextract -i /tmp/test-legacy-full.dth -o /tmp/extracted-homebrew
diff -r /tmp/dwarfs-test-data /tmp/extracted-homebrew

# Read Homebrew image with our tools
./build-fb-only/dwarfsck -i /tmp/test-homebrew.dwarfs --check-integrity
./build-fb-only/dwarfsextract -i /tmp/test-homebrew.dwarfs -o /tmp/extracted-ours
diff -r /tmp/dwarfs-test-data /tmp/extracted-ours

# Verify schema/metadata structure matches
./build-fb-only/dwarfsck -i /tmp/test-homebrew.dwarfs --export-metadata=/tmp/meta-homebrew.json
./build-fb-only/dwarfsck -i /tmp/test-legacy-full.dth --export-metadata=/tmp/meta-ours.json
diff /tmp/meta-homebrew.json /tmp/meta-ours.json
```

---

## Implementation Checklist

### Frozen Schema System
- [ ] `frozen_schema.h` - Data types (Schema, SchemaLayout, SchemaField, DenseMap)
- [ ] `frozen_schema.cpp` - Schema validation logic
- [ ] `frozen_schema_generator.h` - Schema generation interface
- [ ] `frozen_schema_generator.cpp` - Generate schema from metadata
- [ ] `frozen_schema_serializer.h` - Schema ser/de interface
- [ ] `frozen_schema_serializer.cpp` - Thrift CompactProtocol ser/de

### Frozen2 Bit Operations
- [ ] `frozen_bit_writer.h` - Bit-level write interface
- [ ] `frozen_bit_writer.cpp` - Bit-packing implementation
- [ ] `frozen_bit_reader.h` - Bit-level read interface
- [ ] `frozen_bit_reader.cpp` - Bit-unpacking implementation

### Frozen2 Serialization
- [ ] `frozen2_serializer.h` - Serialization interface
- [ ] `frozen2_serializer.cpp` - Complete serialization (all types)
- [ ] `frozen2_deserializer.h` - Deserialization interface
- [ ] `frozen2_deserializer.cpp` - Complete deserialization (all types)

### Integration
- [ ] Update `metadata_freezer.cpp` - Use Frozen2 for LEGACY_THRIFT
- [ ] Create `legacy_thrift_metadata_reader.cpp` - Frozen2-aware reader
- [ ] Update `cmake/metadata_serialization.cmake` - Add all new files
- [ ] Update `cmake/libdwarfs.cmake` - Link new components

### Testing
- [ ] Unit tests for schema generation
- [ ] Unit tests for bit operations
- [ ] Unit tests for serialization round-trip
- [ ] Integration test: Homebrew reads our images
- [ ] Integration test: We read Homebrew images

---

## dwarfs-rs Files to Port

**Complete List**:

1. **Schema Types** (port to `frozen_schema.h/cpp`):
   - `metadata.rs:138-289` - Schema, SchemaLayout, SchemaField, DenseMap

2. **Schema Serialization** (port to `frozen_schema_serializer.cpp`):
   - `ser_thrift.rs` - Thrift CompactProtocol writer for schema
   - `de_thrift.rs` - Thrift CompactProtocol reader for schema

3. **Frozen2 Serialization** (port to `frozen2_serializer.cpp`):
   - `ser_frozen.rs` - COMPLETE FILE
   - Schema generation logic
   - Bit-packing for all types
   - Layout calculation

4. **Frozen2 Deserialization** (port to `frozen2_deserializer.cpp`):
   - `de_frozen.rs` - COMPLETE FILE
   - Bit-unpacking for all types
   - Struct reconstruction

---

## Time Estimate

| Phase | Task | Time |
|-------|------|------|
| **Phase 1** | Schema types, generator, serializer | **2 hours** |
| **Phase 2** | Frozen2 serialization (bit-packing) | **3 hours** |
| **Phase 3** | Frozen2 deserialization (bit-unpacking) | **2 hours** |
| **Phase 4** | Integration (freezer + reader + cmake) | **1.5 hours** |
| **Phase 5** | Testing & validation | **1.5 hours** |
| **Total** | **Full Frozen2 port** | **10 hours** |

---

## Success Criteria

### Core Functionality
- ✅ Schema generation from domain::metadata
- ✅ Schema serialization to Thrift CompactProtocol
- ✅ Schema deserialization from Thrift CompactProtocol
- ✅ Frozen2 bit-packing of ALL metadata types
- ✅ Frozen2 bit-unpacking of ALL metadata types

### Compatibility
- ✅ **CRITICAL**: Homebrew v0.14.1 reads our Legacy Thrift images
- ✅ **CRITICAL**: We read Homebrew v0.14.1 images
- ✅ Byte-for-byte extraction equality
- ✅ Schema structure matches dwarfs-rs
- ✅ All metadata fields preserved

### Code Quality
- ✅ All types fully ported (no shortcuts)
- ✅ Comprehensive error handling
- ✅ Unit tests for all components
- ✅ Integration tests pass
- ✅ No memory leaks (valgrind clean)

---

## Reference Documentation

**Read BEFORE starting**:

1. **Schema Structure**:
   - [`metadata.rs:138-289`](../../../dwarfs-rs/dwarfs/src/metadata.rs:138) - Schema types
   - [`metadata.rs:195-288`](../../../dwarfs-rs/dwarfs/src/metadata.rs:195) - Schema validation

2. **Frozen2 Serialization**:
   - [`ser_frozen.rs`](../../../dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs) - COMPLETE serializer
   - [`metadata.rs:454-457`](../../../dwarfs-rs/dwarfs/src/metadata.rs:454) - Entry point

3. **Frozen2 Deserialization**:
   - [`de_frozen.rs`](../../../dwarfs-rs/dwarfs/src/metadata/de_frozen.rs) - COMPLETE deserializer
   - [`metadata.rs:432-434`](../../../dwarfs-rs/dwarfs/src/metadata.rs:432) - Entry point

4. **Schema Ser/De**:
   - [`ser_thrift.rs`](../../../dwarfs-rs/dwarfs/src/metadata/ser_thrift.rs) - Schema serialization
   - [`de_thrift.rs`](../../../dwarfs-rs/dwarfs/src/metadata/de_thrift.rs) - Schema deserialization

5. **Helpers**:
   - Existing `ThriftCompactWriter` - Use for schema serialization
   - Existing `ThriftCompactReader` - Use for schema deserialization
   - Existing `domain::metadata` - Target data structure

---

## Architecture Principles

### 1. Complete Separation of Concerns

```
Schema Generation  →  frozen_schema_generator.cpp
Schema Ser/De      →  frozen_schema_serializer.cpp
Bit Operations     →  frozen_bit_writer.cpp + frozen_bit_reader.cpp
Frozen2 Ser/De     →  frozen2_serializer.cpp + frozen2_deserializer.cpp
Integration        →  metadata_freezer.cpp (writer) + reader (TBD)
```

### 2. No Shortcuts

- Port **EVERY** type serialization from dwarfs-rs
- Port **EVERY** validation check
- Port **EVERY** error case
- Complete schema generation for ALL structs
- Complete bit-packing for ALL fields

### 3. Full Compatibility

- Generated schemas **MUST** match dwarfs-rs exactly
- Frozen data **MUST** match dwarfs-rs exactly
- **MUST** pass round-trip tests with dwarfs-rs images
- **MUST** be readable by Homebrew v0.14.1

---

## Validation Strategy

### Level 1: Unit Tests
- Schema generation produces valid schema
- Bit operations work correctly (write/read round-trip)
- Each struct type serializes correctly
- Schema ser/de round-trips correctly

### Level 2: Integration Tests
- Create DwarFS image with Legacy Thrift
- Homebrew can read it
- We can read Homebrew images
- Metadata matches semantically

### Level 3: Compatibility Matrix
- Test with multiple Homebrew-created images
- Test with dwarfs-rs created images
- Verify all metadata variations work
- Cross-platform validation

---

**Created**: 2026-01-05 13:18 HKT
**Scope**: COMPLETE Frozen2 port - nothing missing
**Next Session**: Start with Phase 1 - Schema types and generator