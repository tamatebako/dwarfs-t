// ... existing code ...
# Session 25: Comprehensive Domain Model Architecture Plan

**Date**: 2025-12-22
**Approach**: Leverage existing backend namespaces + domain model
**Goal**: Zero-coupling architecture with full re-encoding support
**Timeline**: 32-42 hours (compressed from 38-51h due to existing infrastructure)

## Executive Summary

Session 22-24 revealed architectural coupling issues, but Session 24's backend namespace isolation (`thrift_backend::`, `flatbuffers_backend::`) was **architecturally correct**. The domain model at [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h) is comprehensive and production-ready.

**Key Discovery**: We don't need to start from scratch - we need to complete what Session 24 started by adding converters and interfaces.

## Architecture Principles

1. **Dependency Inversion**: High-level code depends on domain model, not wire formats
2. **Single Responsibility**: Each format isolated in own namespace
3. **Open/Closed**: Add new format = implement interface only
4. **Interface Segregation**: Separate reader/writer concerns
5. **MECE**: No overlap, no gaps between formats

## Target Architecture

```
┌────────────────────────────────────────────┐
│      Application (filesystem_v2)           │
│      Uses domain model internally          │
└──────────────┬─────────────────────────────┘
               │
     ┌─────────┴─────────┐
     ▼                   ▼
┌─────────┐         ┌─────────┐
│ Reader  │         │ Writer  │
│Interface│         │Interface│
└────┬────┘         └────┬────┘
     │                   │
  ┌──┴──┐             ┌──┴──┐
  ▼     ▼             ▼     ▼
┌────┐┌────┐       ┌────┐┌────┐
│ TB ││ FB │       │ TB ││ FB │
└──┬─┘└──┬─┘       └──┬─┘└──┬─┘
   │     │            │     │
   └──┬──┴────────────┴──┬──┘
      │                  │
      ▼                  ▼
┌────────────────────────────┐
│   domain::metadata         │
│   • Format-agnostic        │
│   • Single source of truth │
└────────────────────────────┘

TB = thrift_backend (existing)
FB = flatbuffers_backend (existing)
```

## Domain Model Analysis

**Location**: `include/dwarfs/metadata/domain/`

**Status**: ✅ **Complete** - All types implemented

| Domain Type | Purpose | Wire Format Support |
|-------------|---------|---------------------|
| `metadata` | Top-level container | ✅ Thrift + FlatBuffers |
| `chunk` | Data block references | ✅ 1:1 mapping |
| `inode_data` | File/dir attributes | ✅ 1:1 mapping |
| `directory` | Directory structure | ✅ 1:1 mapping |
| `dir_entry` | Name-to-inode links | ✅ 1:1 mapping |
| `string_table` | Compressed strings | ✅ 1:1 mapping |
| `fs_options` | FS parameters | ✅ 1:1 mapping |
| `history_entry` | Version tracking | ✅ 1:1 mapping |
| `inode_size_cache` | Performance cache | ✅ 1:1 mapping |

**Conclusion**: No gaps. Domain model is production-ready.

## Session 24 Infrastructure (Keep & Extend)

**What Exists**:
- ✅ `thrift_backend::` namespace with complete type hierarchy
- ✅ `flatbuffers_backend::` namespace with complete type hierarchy
- ✅ Separate compilation units (zero cross-contamination)
- ✅ Forward declarations ([`metadata_types_fwd.h`](../include/dwarfs/reader/internal/metadata_types_fwd.h))

**What's Missing** (our work):
- ❌ Domain model converters (wire format ↔ domain)
- ❌ Abstract reader/writer interfaces
- ❌ Factory pattern for format selection
- ❌ Transcoder API for format conversion

## 6-Phase Compressed Plan

### Phase 1: Domain Model Converters (8-10 hours)

**Objective**: Convert between wire formats and domain model

**Files to Create**:
```
src/metadata/converters/
├── thrift_converters.h         (NEW - 200 lines)
├── thrift_converters.cpp       (NEW - 800 lines)
├── flatbuffers_converters.h    (NEW - 200 lines)
└── flatbuffers_converters.cpp  (NEW - 800 lines)
```

**Converter Functions** (example for chunk):
```cpp
// Thrift converters
namespace dwarfs::metadata::converters::thrift {
  domain::chunk to_domain(thrift::metadata::chunk const&);
  thrift::metadata::chunk from_domain(domain::chunk const&);
}

// FlatBuffers converters
namespace dwarfs::metadata::converters::flatbuffers {
  domain::chunk to_domain(gen_flatbuffers::Chunk const*);
  flatbuffers::Offset<gen_flatbuffers::Chunk>
    from_domain(FlatBufferBuilder&, domain::chunk const&);
}
```

**Repeat for**: chunk, inode_data, directory, dir_entry, string_table, metadata

**Challenges**:
- Thrift Frozen2 lazy views → eager domain conversion
- FlatBuffers vector iteration → std::vector construction
- String table decompression (FSST)
- Packed data unpacking

### Phase 2: Reader/Writer Interfaces (6-8 hours)

**Objective**: Abstract interfaces using Strategy Pattern

**Files to Create**:
```
include/dwarfs/metadata/io/
├── metadata_reader.h    (NEW - interface)
├── metadata_writer.h    (NEW - interface)
├── reader_factory.h     (NEW - factory)
└── writer_factory.h     (NEW - factory)

src/metadata/io/
├── reader_factory.cpp   (NEW - 150 lines)
└── writer_factory.cpp   (NEW - 150 lines)
```

**Interfaces**:
```cpp
namespace dwarfs::metadata::io {

class metadata_reader {
public:
  virtual ~metadata_reader() = default;
  virtual domain::metadata read(
    std::span<uint8_t const> schema,
    std::span<uint8_t const> data) const = 0;
  virtual std::string_view format_name() const = 0;
};

class metadata_writer {
public:
  virtual ~metadata_writer() = default;
  virtual std::vector<uint8_t> write_schema(
    domain::metadata const&) const = 0;
  virtual std::vector<uint8_t> write_data(
    domain::metadata const&) const = 0;
  virtual std::string_view format_name() const = 0;
};

} // namespace io
```

**Implementation Classes** (reuse existing serializers):
- Wrap existing `flatbuffers_serializer` as `flatbuffers_metadata_writer`
- Wrap existing `thrift_compact_serializer` as `thrift_metadata_writer`
- Create new readers using Phase 1 converters

### Phase 3: Transcoder API + dwarfsreencode Tool (8-10 hours)

**Objective**: High-level format conversion with CLI tool

**Files to Create**:
```
include/dwarfs/metadata/transcoder.h  (NEW - API)
src/metadata/transcoder.cpp           (NEW - 300 lines)

tools/src/dwarfsreencode_main.cpp     (NEW - 400 lines)
tools/include/dwarfs/tool/dwarfsreencode/
└── options_parser.h                  (NEW - 200 lines)
```

**Transcoder API**:
```cpp
namespace dwarfs::metadata {

enum class image_format { THRIFT, FLATBUFFERS };

class transcoder {
public:
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

**Tool Features**:
```bash
# Convert Thrift → FlatBuffers
dwarfsreencode -i old.dft -o new.dff --target flatbuffers

# Convert FlatBuffers → Thrift
dwarfsreencode -i old.dff -o new.dft --target thrift

# Verify lossless
dwarfsreencode -i old.dft -o new.dff --verify
```

### Phase 4: Refactor metadata_v2 (8-10 hours)

**Objective**: Replace wire format coupling with domain model

**Current Problem**:
```cpp
// metadata_v2_thrift.cpp - TIGHTLY COUPLED
MappedFrozen<thrift::metadata::metadata> meta_;  // Wire format!
```

**Target Solution**:
```cpp
// metadata_v2_impl.cpp - DECOUPLED
domain::metadata meta_;             // Format-agnostic!
io::metadata_reader const& reader_; // Strategy pattern
```

**Approach**:
1. Load wire format using appropriate reader
2. Convert to domain model once at construction
3. All operations work on domain model
4. Format-specific code eliminated

**Files to Refactor** (may consolidate):
- `src/reader/internal/metadata_v2_thrift.cpp` → uses domain
- `src/reader/internal/metadata_v2_flatbuffers.cpp` → uses domain
- Or create unified `src/reader/internal/metadata_v2_impl.cpp`

### Phase 5: Testing (6-8 hours)

**Objective**: Comprehensive test coverage

**Test Files to Create**:
```
test/metadata/
├── converters/
│   ├── thrift_converters_test.cpp     (NEW - 400 lines)
│   └── flatbuffers_converters_test.cpp (NEW - 400 lines)
├── io/
│   ├── thrift_reader_test.cpp         (NEW - 300 lines)
│   ├── flatbuffers_reader_test.cpp    (NEW - 300 lines)
│   └── transcoder_test.cpp            (NEW - 400 lines)
└── tool_dwarfsreencode_test.cpp       (NEW - 300 lines)
```

**Test Categories**:
1. **Converter Tests**: Wire format ↔ domain round-trips
2. **Reader Tests**: Read wire → domain, verify correctness
3. **Writer Tests**: Domain → wire, verify format compliance
4. **Transcoder Tests**: Thrift ↔ FlatBuffers (lossless verification)
5. **Tool Tests**: CLI parsing, error handling, file I/O

**Test Images**:
- Use existing `.dft` and `.dff` test images
- Create minimal test images with known content
- Test with `/test/testtree` dataset

### Phase 6: Documentation (4-6 hours)

**Objective**: Document architecture and update official docs

**Files to Update**:
1. **README.md**: Add re-encoding feature
2. **doc/dwarfs-format.md**: Document domain model layer
3. **doc/dwarfsreencode.md**: Tool manual (NEW)
4. **doc/METADATA_ARCHITECTURE.md**: Architecture overview (NEW)
5. **Memory bank** (`.kilocode/rules/memory-bank/architecture.md`): Update

**Move to old-docs/**:
- SESSION_22-24 temporary plans (work complete)
- Implementation status docs (obsolete)
- Continuation prompts (superseded)

## Implementation Strategy

### Vertical Slice Approach (Recommended)

**Benefit**: Proves architecture early, reduces risk

**Process**:
1. Pick one type (e.g., `chunk`)
2. Implement full stack:
   - Domain model (exists) ✅
   - Thrift converters (to_domain, from_domain)
   - FlatBuffers converters (to_domain, from_domain)
   - Tests (round-trip verification)
3. Repeat for remaining types
4. Integrate into io:: layer
5. Build transcoder on top

**Advantage**: Each type validates architecture before moving to next

## Effort Summary

| Phase | Description | Original | Compressed | Reason |
|-------|-------------|----------|------------|--------|
| 1 | Converters (Thrift + FB) | 8-12h | 8-10h | 1:1 mappings, straightforward |
| 2 | Reader/Writer interfaces | 4h | 6-8h | Wrap existing serializers |
| 3 | Transcoder + Tool | 6h | 8-10h | New functionality |
| 4 | metadata_v2 refactor | 6-8h | 8-10h | Core integration work |
| 5 | Testing | 4-6h | 6-8h | Comprehensive coverage |
| 6 | Documentation | 2-3h | 4-6h | Official docs + cleanup |
| **Total** | **38-51h** | **32-42h** | Backend namespaces exist |

## Risk Mitigation

### Risk 1: Performance Regression
**Mitigation**: Benchmark before/after, optimize hot paths

### Risk 2: Complex Refactoring
**Mitigation**: Vertical slice approach, validate incrementally

### Risk 3: Test Failures
**Mitigation**: Update tests to new expectations (architecture correctness > test compliance)

### Risk 4: Time Overrun
**Mitigation**: Can stop after Phase 3 (converters + transcoder), defer metadata_v2 refactor

## Success Criteria

### Architectural
- ✅ Zero coupling between Thrift and FlatBuffers code
- ✅ Each format in own namespace, own files
- ✅ Domain model is single source of truth
- ✅ Adding 3rd format requires ~8 hours, zero existing code changes

### Functional
- ✅ All existing tests pass (or updated to correct behavior)
- ✅ Re-encoding is lossless (Thrift ↔ FlatBuffers)
- ✅ `dwarfsreencode` tool works correctly
- ✅ Backward compatible with existing images

### Performance
- ✅ Read path within 5% of current (acceptable)
- ✅ Write path unchanged (already uses domain model)
- ✅ Re-encoding completes in <5 seconds for typical image

## File Structure

```
include/dwarfs/metadata/
├── domain/              (EXISTS - complete)
│   ├── metadata.h
│   ├── chunk.h
│   ├── inode_data.h
│   ├── directory.h
│   ├── dir_entry.h
│   ├── string_table.h
│   ├── fs_options.h
│   ├── history_entry.h
│   └── inode_size_cache.h
├── converters/          (NEW - Phase 1)
│   ├── thrift_converters.h
│   └── flatbuffers_converters.h
├── io/                  (NEW - Phase 2)
│   ├── metadata_reader.h
│   ├── metadata_writer.h
│   ├── reader_factory.h
│   └── writer_factory.h
└── transcoder.h         (NEW - Phase 3)

src/metadata/converters/ (NEW - Phase 1)
├── thrift_converters.cpp
└── flatbuffers_converters.cpp

src/metadata/io/         (NEW - Phase 2)
├── reader_factory.cpp
└── writer_factory.cpp

src/metadata/            (NEW - Phase 3)
└── transcoder.cpp

tools/src/               (NEW - Phase 3)
├── dwarfsreencode_main.cpp
└── dwarfsreencode/
    └── options_parser.cpp

test/metadata/           (NEW - Phase 5)
├── converters/
│   ├── thrift_converters_test.cpp
│   └── flatbuffers_converters_test.cpp
├── io/
│   ├── thrift_reader_test.cpp
│   ├── flatbuffers_reader_test.cpp
│   └── transcoder_test.cpp
└── tool_dwarfsreencode_test.cpp
```

## Migration Path

### Keep Working Code
- Don't delete Session 24's backend namespaces
- Build new layers on top of existing infrastructure
- Switch over once validated
- Remove deprecated code only after full validation

### Backward Compatibility
- All existing APIs remain unchanged
- Internal implementation uses domain model
- Performance may degrade slightly (CPU-bound, not I/O-bound)

### Performance Optimization (Future Work)
- Cache domain model after first read
- Lazy conversion for unused metadata
- Zero-copy views where possible

## Next Steps

1. **Review and approve** this comprehensive plan
2. **Start Phase 1**: Implement domain model converters
3. **Use vertical slice** approach (one type at a time)
4. **Validate incrementally** before proceeding

See: [`SESSION_25_IMPLEMENTATION_STATUS.md`](SESSION_25_IMPLEMENTATION_STATUS.md)
See: [`SESSION_25_PHASE1_CONTINUATION_PROMPT.md`](SESSION_25_PHASE1_CONTINUATION_PROMPT.md)
// ... existing code ...