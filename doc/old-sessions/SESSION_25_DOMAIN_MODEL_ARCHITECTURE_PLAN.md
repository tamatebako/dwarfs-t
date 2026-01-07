# Session 25: Clean OOP Architecture - Domain Model Migration Plan

**Date**: 2025-12-22
**Approach**: Leverage existing domain model for format-agnostic metadata
**Goal**: Zero-coupling architecture with full re-encoding support

## Background

Session 22-24 revealed that the current architecture has fundamental coupling issues:
- Both Thrift and FlatBuffers backends define `metadata_v2_data` in same namespace
- Direct wire format coupling throughout codebase
- No clean abstraction for format conversion

**KEY DISCOVERY**: The project ALREADY has a domain model at [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h) used by the **writer** side. We need to extend this to the **reader** side for consistency.

## Architecture Principles

### 1. Dependency Inversion
- High-level code depends on abstractions (domain model), not wire formats
- Wire formats are implementation details, hidden behind interfaces

### 2. Single Responsibility  
- **Domain model**: Represents metadata concepts (format-agnostic)
- **Readers**: Deserialize wire format → domain model
- **Writers**: Serialize domain model → wire format
- **Converters**: Transform between domain and wire formats

### 3. Open/Closed
- Add new format = implement reader + writer interfaces
- Zero changes to domain model or existing formats

### 4. Interface Segregation
- Separate reader and writer interfaces
- Clients depend only on what they need

### 5. MECE (Mutually Exclusive, Collectively Exhaustive)
- Each format in own namespace
- Domain model is shared, immutable truth
- No overlap, no gaps

## Target Architecture

```
┌────────────────────────────────────────────────────────┐
│           Application Layer (metadata_v2)              │
│  • Filesystem operations (find, getattr, read)         │
│  • Uses domain model internally                        │
└──────────────────┬─────────────────────────────────────┘
                   │
       ┌───────────┴───────────┐
       ▼                       ▼
┌─────────────────┐    ┌─────────────────┐
│ metadata_reader │    │ metadata_writer │
│  (interface)    │    │  (interface)    │
└────────┬────────┘    └────────┬────────┘
         │                      │
    ┌────┴────┐            ┌────┴────┐
    ▼         ▼            ▼         ▼
┌────────┐ ┌──────┐   ┌────────┐ ┌──────┐
│Thrift  │ │ FB   │   │Thrift  │ │ FB   │
│Reader  │ │Reader│   │Writer  │ │Writer│
└───┬────┘ └───┬──┘   └───┬────┘ └───┬──┘
    │          │          │          │
    └────┬─────┴──────────┴─────┬────┘
         │                      │
         ▼                      ▼
    ┌────────────────────────────────┐
    │   domain::metadata (SHARED)    │
    │                                │
    │  • Format-agnostic structs     │
    │  • Zero wire format deps       │
    │  • Single source of truth      │
    └────────────────────────────────┘
```

## Phase Breakdown

### Phase 1: Verify Existing Domain Model (2 hours)

**Objective**: Understand what's already implemented

**Tasks**:
1. Read [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h)
2. Identify gaps vs reader/writer needs
3. Document domain model structure
4. Verify it matches Thrift schema
5. Check FlatBuffers schema compatibility

**Deliverable**: `doc/DOMAIN_MODEL_ANALYSIS.md`

### Phase 2: Create Reader Interface (2 hours)

**Objective**: Abstract interface for reading metadata

**Location**: `include/dwarfs/metadata/io/metadata_reader.h`

**Interface**:
```cpp
namespace dwarfs::metadata::io {

class metadata_reader {
 public:
  virtual ~metadata_reader() = default;
  
  // Read wire format → domain model
  virtual domain::metadata read(
      std::span<uint8_t const> schema,
      std::span<uint8_t const> data) const = 0;
  
  // Format identification
  virtual std::string_view format_name() const = 0;
};

} // namespace io
```

**Files**:
- `include/dwarfs/metadata/io/metadata_reader.h` (NEW)
- `include/dwarfs/metadata/io/reader_factory.h` (NEW)

### Phase 3: Implement Thrift Reader (4-6 hours)

**Objective**: Convert Frozen2 format to domain model

**Location**: `src/metadata/io/thrift/thrift_metadata_reader.cpp`

**Key Components**:
```cpp
namespace dwarfs::metadata::io::thrift {

class metadata_reader : public io::metadata_reader {
 public:
  domain::metadata read(...) const override;

 private:
  // Converters: Thrift types → domain types
  domain::inode convert_inode(
      apache::thrift::frozen::View<::thrift::metadata::inode> const&) const;
  
  domain::directory convert_directory(
      apache::thrift::frozen::View<::thrift::metadata::directory> const&) const;
  
  domain::chunk convert_chunk(
      apache::thrift::frozen::View<::thrift::metadata::chunk> const&) const;
  
  // ... converters for all types
};

} // namespace thrift
```

**Challenges**:
- Frozen2 views are lazy - need eager evaluation to domain
- Packed structures need unpacking
- String tables need expansion

**Files**:
- `include/dwarfs/metadata/io/thrift/thrift_metadata_reader.h` (NEW)
- `src/metadata/io/thrift/thrift_metadata_reader.cpp` (NEW)
- `src/metadata/io/thrift/thrift_converters.cpp` (NEW, helpers)

### Phase 4: Implement FlatBuffers Reader (4-6 hours)

**Objective**: Convert FlatBuffers format to domain model

**Location**: `src/metadata/io/flatbuffers/flatbuffers_metadata_reader.cpp`

**Key Components**:
```cpp
namespace dwarfs::metadata::io::flatbuffers {

class metadata_reader : public io::metadata_reader {
 public:
  domain::metadata read(...) const override;

 private:
  // Converters: FlatBuffers types → domain types
  domain::inode convert_inode(
      gen_flatbuffers::inode const*) const;
  
  domain::directory convert_directory(
      gen_flatbuffers::directory const*) const;
  
  domain::chunk convert_chunk(
      gen_flatbuffers::chunk const*) const;
  
  // ... converters for all types
};

} // namespace flatbuffers
```

**Challenges**:
- FlatBuffers accessors return raw pointers
- Vector accessors need iteration
- String decompression (FSST) if enabled

**Files**:
- `include/dwarfs/metadata/io/flatbuffers/flatbuffers_metadata_reader.h` (NEW)
- `src/metadata/io/flatbuffers/flatbuffers_metadata_reader.cpp` (NEW)
- `src/metadata/io/flatbuffers/flatbuffers_converters.cpp` (NEW, helpers)

### Phase 5: Implement Writer Interfaces (2 hours)

**Objective**: Abstract interface for writing metadata

**Location**: `include/dwarfs/metadata/io/metadata_writer.h`

**Interface**:
```cpp
namespace dwarfs::metadata::io {

class metadata_writer {
 public:
  virtual ~metadata_writer() = default;
  
  // Write domain model → wire format
  virtual std::vector<uint8_t> write_schema(
      domain::metadata const& meta) const = 0;
  
  virtual std::vector<uint8_t> write_data(
      domain::metadata const& meta) const = 0;
  
  // Format identification
  virtual std::string_view format_name() const = 0;
};

} // namespace io
```

**Files**:
- `include/dwarfs/metadata/io/metadata_writer.h` (NEW)
- `include/dwarfs/metadata/io/writer_factory.h` (NEW)

### Phase 6: Implement Thrift Writer (3-4 hours)

**Objective**: Convert domain model to Frozen2 format

**Location**: `src/metadata/io/thrift/thrift_metadata_writer.cpp`

**Key Components**:
```cpp
namespace dwarfs::metadata::io::thrift {

class metadata_writer : public io::metadata_writer {
 public:
  std::vector<uint8_t> write_schema(...) const override;
  std::vector<uint8_t> write_data(...) const override;

 private:
  // Converters: domain types → Thrift types
  ::thrift::metadata::inode convert_inode(
      domain::inode const&) const;
  
  // ... converters for all types
};

} // namespace thrift
```

**Files**:
- `include/dwarfs/metadata/io/thrift/thrift_metadata_writer.h` (NEW)
- `src/metadata/io/thrift/thrift_metadata_writer.cpp` (NEW)

**Note**: Writer may already exist in serialization layer, verify and refactor.

### Phase 7: Implement FlatBuffers Writer (3-4 hours)

**Objective**: Convert domain model to FlatBuffers format

**Location**: `src/metadata/io/flatbuffers/flatbuffers_metadata_writer.cpp`

**Key Components**:
```cpp
namespace dwarfs::metadata::io::flatbuffers {

class metadata_writer : public io::metadata_writer {
 public:
  std::vector<uint8_t> write_schema(...) const override;
  std::vector<uint8_t> write_data(...) const override;

 private:
  // Converters: domain types → FlatBuffers types
  flatbuffers::Offset<gen_flatbuffers::inode>
  convert_inode(
      flatbuffers::FlatBufferBuilder& builder,
      domain::inode const&) const;
  
  // ... converters for all types
};

} // namespace flatbuffers
```

**Files**:
- `include/dwarfs/metadata/io/flatbuffers/flatbuffers_metadata_writer.h` (NEW)
- `src/metadata/io/flatbuffers/flatbuffers_metadata_writer.cpp` (NEW)

**Note**: Writer may already exist in serialization layer, verify and refactor.

### Phase 8: Create Transcoder API (2 hours)

**Objective**: High-level API for format conversion

**Location**: `include/dwarfs/metadata/transcoder.h`

**API**:
```cpp
namespace dwarfs::metadata {

enum class image_format {
  THRIFT_FROZEN,
  FLATBUFFERS
};

class transcoder {
 public:
  // Re-encode between formats
  struct result {
    std::vector<uint8_t> schema;
    std::vector<uint8_t> data;
    image_format target_format;
  };
  
  static result transcode(
      std::span<uint8_t const> source_schema,
      std::span<uint8_t const> source_data,
      image_format source_format,
      image_format target_format,
      logger& lgr);
};

} // namespace metadata
```

**Files**:
- `include/dwarfs/metadata/transcoder.h` (NEW)
- `src/metadata/transcoder.cpp` (NEW)

### Phase 9: Refactor metadata_v2 to Use Domain Model (6-8 hours)

**Objective**: Replace wire format coupling with domain model

**Current State**:
```cpp
// metadata_v2_thrift.cpp - TIGHTLY COUPLED to Frozen2
MappedFrozen<thrift::metadata::metadata> meta_;  // Wire format!
```

**Target State**:
```cpp
// metadata_v2_impl.cpp - DECOUPLED via domain
domain::metadata meta_;  // Format-agnostic!
io::metadata_reader const& reader_;  // Strategy pattern
```

**Approach**:
1. Load wire format using appropriate reader
2. Convert to domain model once at construction
3. All operations work on domain model
4. Eliminates format-specific code paths

**Files to Modify**:
- `src/reader/internal/metadata_v2_thrift.cpp` → refactored
- `src/reader/internal/metadata_v2_flatbuffers.cpp` → refactored
- May consolidate into single `metadata_v2_impl.cpp`

### Phase 10: Create Tool: dwarfsreencode (4 hours)

**Objective**: Command-line tool for format conversion

**Location**: `tools/src/dwarfsreencode_main.cpp`

**Features**:
```bash
# Convert Thrift → FlatBuffers
dwarfsreencode --input old.dft --output new.dff --target-format flatbuffers

# Convert FlatBuffers → Thrift
dwarfsreencode --input old.dff --output new.dft --target-format thrift

# Verify lossless conversion
dwarfsreencode --input old.dft --output new.dff --verify
```

**Implementation**:
```cpp
int main(int argc, char** argv) {
  // Parse args
  std::string input, output;
  image_format target;
  bool verify = false;
  
  // Load source image
  auto [schema, data, source_format] = load_image(input);
  
  // Transcode
  auto result = transcoder::transcode(
      schema, data, source_format, target, lgr);
  
  // Write target image
  write_image(output, result.schema, result.data);
  
  // Optional: verify round-trip
  if (verify) {
    auto domain1 = reader->read(schema, data);
    auto domain2 = reader->read(result.schema, result.data);
    assert(domain1 == domain2);
  }
}
```

**Files**:
- `tools/src/dwarfsreencode_main.cpp` (NEW)
- `tools/include/dwarfs/tool/dwarfsreencode/options_parser.h` (NEW)
- Update `tools/CMakeLists.txt`

### Phase 11: Testing (4-6 hours)

**Objective**: Comprehensive test coverage

**Test Categories**:

1. **Unit Tests** - Domain model operations
   - `test/metadata/domain/metadata_test.cpp`
   - Equality, comparison, validation

2. **Integration Tests** - Reader/Writer round-trips
   - `test/metadata/io/thrift_reader_test.cpp`
   - `test/metadata/io/flatbuffers_reader_test.cpp`
   - Read wire → domain → write wire → verify identical

3. **Transcode Tests** - Format conversion
   - `test/metadata/transcoder_test.cpp`
   - Thrift → FlatBuffers → Thrift (lossless)
   - FlatBuffers → Thrift → FlatBuffers (lossless)

4. **Tool Tests** - dwarfsreencode CLI
   - `test/tool_main_dwarfsreencode_test.cpp`
   - Option parsing, error handling

**Test Images**:
- Use existing test images (`.dft`, `.dff`)
- Create minimal test images
- Test with `/test/testtree` dataset

### Phase 12: Documentation (2-3 hours)

**Objective**: Document new architecture and APIs

**Files to Update**:
1. **README.md**: Add re-encoding section
2. **doc/dwarfs-format.md**: Document domain model
3. **doc/METADATA_ARCHITECTURE.md**: Architecture overview (NEW)
4. **doc/dwarfsreencode.md**: Tool manual (NEW)
5. **Memory bank**: Update architecture.md

**Documentation Sections**:
- Domain model structure
- Reader/Writer interfaces
- Re-encoding workflow
- Adding new formats
- Performance considerations

## Implementation Strategy

### Option A: Sequential (Safe, Slower)
Complete each phase fully before next

### Option B: Parallel (Risky, Faster)
- Phases 2-4 in parallel (readers)
- Phases 5-7 in parallel (writers)
- Requires careful coordination

### Option C: Vertical Slice (Recommended)
1. Implement one complete type (e.g., `inode`)
   - Domain model
   - Thrift reader/writer
   - FlatBuffers reader/writer
   - Tests
2. Repeat for remaining types
3. Integrate incrementally

**Recommendation**: **Option C** - Proves architecture early, reduces risk

## File Structure

```
include/dwarfs/metadata/
├── domain/
│   ├── metadata.h              (EXISTS - verify/extend)
│   ├── chunk.h                 (NEW - if not in metadata.h)
│   ├── inode.h                 (NEW - if not in metadata.h)
│   └── directory.h             (NEW - if not in metadata.h)
├── io/
│   ├── metadata_reader.h       (NEW - abstract interface)
│   ├── metadata_writer.h       (NEW - abstract interface)
│   ├── reader_factory.h        (NEW)
│   ├── writer_factory.h        (NEW)
│   ├── thrift/
│   │   ├── thrift_metadata_reader.h    (NEW)
│   │   └── thrift_metadata_writer.h    (NEW)
│   └── flatbuffers/
│       ├── flatbuffers_metadata_reader.h   (NEW)
│       └── flatbuffers_metadata_writer.h   (NEW)
└── transcoder.h                (NEW - high-level API)

src/metadata/io/
├── thrift/
│   ├── thrift_metadata_reader.cpp      (NEW)
│   ├── thrift_metadata_writer.cpp      (NEW)
│   └── thrift_converters.cpp           (NEW - helpers)
├── flatbuffers/
│   ├── flatbuffers_metadata_reader.cpp (NEW)
│   ├── flatbuffers_metadata_writer.cpp (NEW)
│   └── flatbuffers_converters.cpp      (NEW - helpers)
├── reader_factory.cpp          (NEW)
├── writer_factory.cpp          (NEW)
└── transcoder.cpp              (NEW)

tools/src/
├── dwarfsreencode_main.cpp     (NEW)
└── dwarfsreencode/
    └── options_parser.cpp      (NEW)

test/metadata/
├── domain/
│   └── metadata_test.cpp       (NEW)
├── io/
│   ├── thrift_reader_test.cpp  (NEW)
│   ├── flatbuffers_reader_test.cpp (NEW)
│   └── transcoder_test.cpp     (NEW)
└── tool_dwarfsreencode_test.cpp (NEW)
```

## Estimated Effort

| Phase | Description | Estimate |
|-------|-------------|----------|
| 1 | Domain model analysis | 2h |
| 2 | Reader interface | 2h |
| 3 | Thrift reader | 4-6h |
| 4 | FlatBuffers reader | 4-6h |
| 5 | Writer interface | 2h |
| 6 | Thrift writer | 3-4h |
| 7 | FlatBuffers writer | 3-4h |
| 8 | Transcoder API | 2h |
| 9 | Refactor metadata_v2 | 6-8h |
| 10 | dwarfsreencode tool | 4h |
| 11 | Testing | 4-6h |
| 12 | Documentation | 2-3h |
| **Total** | | **38-51 hours** |

## Migration Path

### Keep Current Code Working
- Don't delete existing `metadata_v2_thrift.cpp` / `metadata_v2_flatbuffers.cpp` yet
- Build new architecture alongside
- Switch over once validated
- Remove old code in final phase

### Backward Compatibility
- All existing APIs remain unchanged
- Internal implementation uses domain model
- Performance may degrade slightly (CPU, not I/O bound)

### Performance Optimization (Future)
- Cache domain model after first read
- Lazy conversion for unused metadata
- Zero-copy views where possible

## Risk Mitigation

### Risk 1: Domain Model Incomplete
**Mitigation**: Phase 1 validates completeness early

### Risk 2: Performance Regression  
**Mitigation**: Benchmark before/after, optimize hot paths

### Risk 3: Complex Refactoring
**Mitigation**: Vertical slice approach proves architecture early

### Risk 4: Time Overrun
**Mitigation**: Can stop after Phase 8 (readers + writers + transcoder), defer metadata_v2 refactor

## Success Criteria

### Architectural
- ✅ Zero coupling between Thrift and FlatBuffers code
- ✅ Each format in own namespace, own files
- ✅ Domain model is single source of truth
- ✅ Adding 3rd format requires ~10 hours, zero existing code changes

### Functional
- ✅ All existing tests pass
- ✅ Re-encoding is lossless (Thrift ↔ FlatBuffers)
- ✅ `dwarfsreencode` tool works correctly
- ✅ Backward compatible with existing images

### Performance
- ✅ Read path within 5% of current (acceptable)
- ✅ Write path unchanged (already uses domain model)
- ✅ Re-encoding completes in <5 seconds for typical image

## Next Steps

1. **User approves** this architecture
2. **Revert** Session 24's partial changes
3. **Start Phase 1**: Analyze domain model
4. **Proceed sequentially** through phases

See: [`SESSION_25_IMPLEMENTATION_STATUS.md`](SESSION_25_IMPLEMENTATION_STATUS.md)