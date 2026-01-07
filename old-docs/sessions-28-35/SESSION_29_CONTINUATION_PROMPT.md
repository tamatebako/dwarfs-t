# Session 29 Continuation Prompt

**Date**: 2025-12-22+
**Previous**: Session 28 - Phases 1-3 Complete
**Goal**: COMPLETE OOP MIGRATION - Delete old code, compress Phases 4+5

## Context from Session 28

✅ **Phase 1: Converters** - VERIFIED, all builds pass
✅ **Phase 2: Reader interfaces** - Created (4 files, 381 lines)
✅ **Phase 3: Writer interfaces** - Created (3 files, 168 lines)

**Total Ready**: 2,300 lines of clean OOP code ready to integrate

## Your Mission (Session 29)

**Execute the aggressive migration plan in [`SESSION_29_COMPRESSED_PHASE45_PLAN.md`](SESSION_29_COMPRESSED_PHASE45_PLAN.md)**

### Step 1: DELETE Old Backend Code (15 min) 🔥

```bash
# Backup first
mkdir -p .backup/session29-deleted
cp src/reader/internal/metadata_v2_flatbuffers.cpp .backup/session29-deleted/
cp src/reader/internal/metadata_v2_thrift.cpp .backup/session29-deleted/
cp src/reader/internal/metadata_types_flatbuffers.cpp .backup/session29-deleted/
cp src/reader/internal/metadata_types_thrift.cpp .backup/session29-deleted/

# DELETE the old code
rm src/reader/internal/metadata_v2_flatbuffers.cpp
rm src/reader/internal/metadata_v2_thrift.cpp
rm src/reader/internal/metadata_types_flatbuffers.cpp
rm src/reader/internal/metadata_types_thrift.cpp
```

### Step 2: Rewrite metadata_v2.cpp (3-4h)

**File**: `src/reader/internal/metadata_v2.cpp`

**Pattern**:
```cpp
class metadata_v2::impl {
  std::unique_ptr<metadata_reader_interface> reader_;
  metadata::domain::metadata domain_;

  impl(logger& lgr, std::span<uint8_t const> data) {
    reader_ = create_metadata_reader(data.data(), data.size());
    domain_ = reader_->read();
  }

  // All methods use domain_ directly
  chunk get_chunk(size_t i) { return domain_.chunks[i]; }
};
```

**Key**: Use [`metadata_reader_interface.h`](../include/dwarfs/reader/metadata_reader_interface.h) + converters

### Step 3: Update metadata_builder.cpp (1h)

**File**: `src/writer/internal/metadata_builder.cpp`

**Pattern**:
```cpp
// Build domain model
metadata::domain::metadata domain_meta;
// ... populate during scanning/segmentation

// Serialize
auto writer = create_metadata_writer(format);
return writer->serialize(domain_meta);
```

**Key**: Use [`metadata_writer_interface.h`](../include/dwarfs/writer/metadata_writer_interface.h)

### Step 4: Update CMake (30min)

Remove old file references, test all 3 builds.

### Step 5: Test Everything (2-3h)

- Build all 3 configurations
- Run unit tests
- Functional tests (mount/extract)
- Performance check

## Files Ready to Use

**Converters** (VALIDATED):
- [`include/dwarfs/metadata/converters/domain_thrift_converter.h`](../include/dwarfs/metadata/converters/domain_thrift_converter.h)
- [`src/metadata/converters/domain_thrift_converter.cpp`](../src/metadata/converters/domain_thrift_converter.cpp)
- [`include/dwarfs/metadata/converters/domain_flatbuffers_converter.h`](../include/dwarfs/metadata/converters/domain_flatbuffers_converter.h)
- [`src/metadata/converters/domain_flatbuffers_converter.cpp`](../src/metadata/converters/domain_flatbuffers_converter.cpp)

**Reader** (CREATED):
- [`include/dwarfs/reader/metadata_reader_interface.h`](../include/dwarfs/reader/metadata_reader_interface.h)
- [`src/reader/flatbuffers_metadata_reader.cpp`](../src/reader/flatbuffers_metadata_reader.cpp)
- [`src/reader/thrift_metadata_reader.cpp`](../src/reader/thrift_metadata_reader.cpp)
- [`src/reader/metadata_reader_factory.cpp`](../src/reader/metadata_reader_factory.cpp)

**Writer** (CREATED):
- [`include/dwarfs/writer/metadata_writer_interface.h`](../include/dwarfs/writer/metadata_writer_interface.h)
- [`src/writer/flatbuffers_metadata_writer.cpp`](../src/writer/flatbuffers_metadata_writer.cpp)
- [`src/writer/thrift_metadata_writer.cpp`](../src/writer/thrift_metadata_writer.cpp)

## Architecture Target

```
Application Code
      ↓
metadata_reader_interface (format-agnostic)
      ↓
FlatBuffers/Thrift Reader (format-specific)
      ↓
Converters (domain ↔ wire format)
      ↓
Domain Model (pure C++ structures)
```

**NO backend namespaces, NO guards, FULLY OOP**

## Critical Success Factors

1. **Cache domain model** in metadata_v2 for performance
2. **Use converters** for all wire format access
3. **Remove ALL backend namespace code**
4. **Keep zero guards** in new code
5. **Test aggressively** after each change

## Timeline

**Target**: Complete in 6-8 hours (1 focused work day)

**Aggressive but achievable** because:
- All interfaces are ready
- Converters are validated
- Pattern is clear: use domain model everywhere
- Testing is straightforward

---

**Start Command**: Read this file, then execute Step 1 (DELETE old code)