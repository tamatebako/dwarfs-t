# Session 24: Clean OOP Architecture for Dual-Format Metadata

**Date**: 2025-12-22
**Purpose**: Design fully OOP architecture supporting Thrift and FlatBuffers with zero coupling
**Goal**: Enable re-encoding between formats through clean domain model abstraction

## CRITICAL: Domain Model Already Exists

**IMPORTANT**: The project **already has** a domain model at [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h)! The serialization layer was designed with this architecture:

```
Domain Model (format-agnostic)
      ↓
Serializers (Thrift, FlatBuffers)
      ↓
Wire Formats
```

The **reader side** currently uses raw wire formats (Frozen2/FlatBuffers) for performance, but should be refactored to use the domain model for consistency.

## Proposed Clean Architecture

### Layer 1: Domain Model (Format-Agnostic)

**Location**: `include/dwarfs/metadata/domain/`

```cpp
namespace dwarfs::metadata::domain {

// Pure C++ structures - NO Thrift, NO FlatBuffers
struct chunk {
  uint32_t block;
  uint32_t offset;
  uint32_t size;
};

struct inode {
  uint32_t num;
  uint16_t mode;
  uint32_t uid;
  uint32_t gid;
  std::vector<chunk> chunks;
  // ... all metadata fields
};

struct directory {
  uint32_t inode_num;
  uint32_t parent_inode;
  std::vector<dir_entry> entries;
};

struct metadata {
  uint32_t block_size;
  std::vector<inode> inodes;
  std::vector<directory> directories;
  std::vector<chunk> chunks;
  // ... complete filesystem metadata
};

} // namespace domain
```

### Layer 2: Reader/Writer Interfaces

**Location**: `include/dwarfs/metadata/io/`

```cpp
namespace dwarfs::metadata::io {

// Abstract reader interface
class metadata_reader {
 public:
  virtual ~metadata_reader() = default;
  
  // Read wire format → domain model
  virtual domain::metadata read(
      std::span<uint8_t const> schema,
      std::span<uint8_t const> data) = 0;
};

// Abstract writer interface  
class metadata_writer {
 public:
  virtual ~metadata_writer() = default;
  
  // Write domain model → wire format
  virtual std::vector<uint8_t> write_schema(
      domain::metadata const& meta) = 0;
  
  virtual std::vector<uint8_t> write_data(
      domain::metadata const& meta) = 0;
};

} // namespace io
```

### Layer 3: Backend Implementations (Fully Isolated)

**Location**: `src/metadata/io/thrift/` and `src/metadata/io/flatbuffers/`

```cpp
// src/metadata/io/thrift/thrift_metadata_reader.h
namespace dwarfs::metadata::io::thrift {

class metadata_reader : public io::metadata_reader {
 public:
  domain::metadata read(
      std::span<uint8_t const> schema,
      std::span<uint8_t const> data) override;

 private:
  // Thrift-specific conversion: Frozen2 → domain
  domain::inode convert_inode(
      apache::thrift::frozen::View<thrift::metadata::inode> const&);
};

class metadata_writer : public io::metadata_writer {
 public:
  std::vector<uint8_t> write_schema(
      domain::metadata const&) override;
  
  std::vector<uint8_t> write_data(
      domain::metadata const&) override;

 private:
  // Thrift-specific conversion: domain → Frozen2
  thrift::metadata::inode convert_inode(
      domain::inode const&);
};

} // namespace thrift
```

```cpp
// src/metadata/io/flatbuffers/flatbuffers_metadata_reader.h
namespace dwarfs::metadata::io::flatbuffers {

class metadata_reader : public io::metadata_reader {
 public:
  domain::metadata read(
      std::span<uint8_t const> schema,
      std::span<uint8_t const> data) override;

 private:
  // FlatBuffers-specific conversion: FlatBuffers → domain
  domain::inode convert_inode(
      gen_flatbuffers::inode const*);
};

class metadata_writer : public io::metadata_writer {
 public:
  std::vector<uint8_t> write_schema(
      domain::metadata const&) override;
  
  std::vector<uint8_t> write_data(
      domain::metadata const&) override;

 private:
  // FlatBuffers-specific conversion: domain → FlatBuffers
  flatbuffers::Offset<gen_flatbuffers::inode>
  convert_inode(flatbuffers::FlatBufferBuilder&, domain::inode const&);
};

} // namespace flatbuffers
```

### Layer 4: Factory/Registry

**Location**: `include/dwarfs/metadata/io/factory.h`

```cpp
namespace dwarfs::metadata::io {

enum class format {
  THRIFT_FROZEN,
  FLATBUFFERS
};

class reader_factory {
 public:
  static std::unique_ptr<metadata_reader> create_reader(format fmt);
};

class writer_factory {
 public:
  static std::unique_ptr<metadata_writer> create_writer(format fmt);
};

} // namespace io
```

### Layer 5: Re-encoding API

**Location**: `include/dwarfs/metadata/transcoder.h`

```cpp
namespace dwarfs::metadata {

class transcoder {
 public:
  // Re-encode from one format to another
  static std::pair<std::vector<uint8_t>, std::vector<uint8_t>>
  transcode(
      std::span<uint8_t const> source_schema,
      std::span<uint8_t const> source_data,
      io::format source_format,
      io::format target_format);
};

} //namespace metadata
```

**Implementation**:
```cpp
auto transcoder::transcode(...) -> ... {
  // 1. Read source format → domain model
  auto reader = io::reader_factory::create_reader(source_format);
  auto domain_meta = reader->read(source_schema, source_data);
  
  // 2. Write domain model → target format
  auto writer = io::writer_factory::create_writer(target_format);
  auto target_schema = writer->write_schema(domain_meta);
  auto target_data = writer->write_data(domain_meta);
  
  return {std::move(target_schema), std::move(target_data)};
}
```

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────┐
│                   Public API Layer                       │
│  inode_view, directory_view, chunk_range (interfaces)   │
└──────────────────────┬──────────────────────────────────┘
                       │
           ┌───────────┴───────────┐
           ▼                       ▼
┌──────────────────────┐  ┌──────────────────────┐
│  metadata_reader     │  │  metadata_writer     │
│   (interface)        │  │   (interface)        │
└──────────┬───────────┘  └──────────┬───────────┘
           │                         │
    ┌──────┴──────┐           ┌──────┴──────┐
    ▼             ▼           ▼             ▼
┌────────┐  ┌──────────┐  ┌────────┐  ┌──────────┐
│Thrift  │  │FlatBuf   │  │Thrift  │  │FlatBuf   │
│Reader  │  │Reader    │  │Writer  │  │Writer    │
└────┬───┘  └────┬─────┘  └────┬───┘  └────┬─────┘
     │           │             │           │
     └─────┬─────┴─────────────┴─────┬─────┘
           │                         │
           ▼                         ▼
    ┌────────────────────────────────────┐
    │   domain::metadata (SINGLE model)  │
    │                                    │
    │  • Format-agnostic structures      │
    │  • Zero wire format dependencies   │
    │  • Bidirectional conversion        │
    └────────────────────────────────────┘
```

## Re-encoding Use Cases

### 1. Image Upgrade
```cpp
// Upgrade Thrift image to FlatBuffers
auto [schema, data] = transcoder::transcode(
    old_schema, old_data,
    format::THRIFT_FROZEN,
    format::FLATBUFFERS);
```

### 2. Format Testing
```cpp
// Round-trip test
auto domain1 = thrift_reader->read(schema, data);
auto flatbuf_data = flatbuf_writer->write_data(domain1);
auto domain2 = flatbuf_reader->read({}, flatbuf_data);
assert(domain1 == domain2);  // Lossless conversion
```

### 3. Tool: dwarfsreencode
```bash
# New tool for format conversion
dwarfsreencode --input old.dft --output new.dff --target-format flatbuffers
```

## Benefits

### Separation of Concerns
- ✅ Wire formats NEVER interact
- ✅ Domain model is single source of truth
- ✅ Each backend is independently testable

### Extensibility
- ✅ Add new format = implement 2 classes (reader + writer)
- ✅ Zero changes to existing formats
- ✅ Domain model grows organically

### Maintainability
- ✅ Format-specific bugs isolated to backend
- ✅ Domain model bugs visible across all formats
- ✅ Clear ownership boundaries

### Performance
- ⚠️ Conversion overhead for re-encoding (acceptable - not hot path)
- ✅ Can optimize readers to skip domain model (lazy loading)
- ✅ Writers always need full model anyway

## Implementation Effort

### Phase 1: Create Domain Model (~2-4 hours)
- Define `domain::metadata` structs
- Mirror Thrift/FlatBuffers schema structure
- Add equality operators for testing

### Phase 2: Implement Thrift Backend (~4-6 hours)
- `thrift_metadata_reader`: Frozen2 → domain
- `thrift_metadata_writer`: domain → Frozen2
- Conversion logic for all types

### Phase 3: Implement FlatBuffers Backend (~4-6 hours)
- `flatbuffers_metadata_reader`: FlatBuffers → domain
- `flatbuffers_metadata_writer`: domain → FlatBuffers
- Conversion logic for all types

### Phase 4: Factory + Transcoder (~2 hours)
- Reader factory
- Writer factory
- Transcoder implementation

### Phase 5: Integration (~4-6 hours)
- Update `metadata_v2` to use readers
- Maintain backward compatibility
- Update tests

### Phase 6: Tool Implementation (~2-4 hours)
- Create `dwarfsreencode` tool
- CLI for format conversion
- Validation and error handling

**Total Estimate**: 18-28 hours

## Migration Strategy

### Option A: Big Bang (Risky)
Replace entire metadata layer at once

### Option B: Incremental (Recommended)
1. Create domain model alongside existing code
2. Implement converters (Thrift ↔ domain, FlatBuffers ↔ domain)
3. Add conversion layer in readers
4. Gradually migrate callsites to use domain model
5. Remove old code once fully migrated

### Option C: Hybrid (Pragmatic)
- Keep fast path using wire formats directly
- Use domain model only for re-encoding/utilities
- Best of both worlds (performance + flexibility)

## Trade-offs

### Performance Impact
- **Read path**: Can stay zero-copy (Frozen2/FlatBuffers)
- **Write path**: Domain model already used
- **Re-encode path**: Acceptable overhead (not hot path)

### Complexity
- **More abstractions**: +5-10 classes
- **Clearer boundaries**: Each class <500 lines
- **Better testing**: Mock interfaces easily

### Compatibility
- **Backward compatible**: Existing code unchanged
- **Forward compatible**: New formats plug in cleanly
- **Cross-format**: Re-encoding becomes trivial

## Recommendation

Implement **Option C (Hybrid)** with phased rollout:

1. **Session 24 (Current)**: Fix immediate duplicate symbol issue with namespace isolation
2. **Session 25**: Create domain model + converters  
3. **Session 26**: Implement re-encoding API + transcoder
4. **Session 27**: Create `dwarfsreencode` tool
5. **Future**: Gradually migrate hot paths if needed

This keeps current performance while enabling future flexibility.

Would you like me to proceed with Session 24's immediate fix (namespace isolation), or pivot to clean-room redesign starting with domain model?