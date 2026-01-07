// ... existing code ...
# Session 26: Phase 1 - Domain Model Converters

**Date**: 2025-12-22+
**Objective**: Implement converters between wire formats and domain model
**Approach**: Vertical slice - one type at a time
**Estimated Time**: 8-10 hours

## Context

Sessions 22-24 revealed architectural coupling. Session 25 analyzed the codebase and discovered:
- ✅ Domain model is **complete** and production-ready
- ✅ Backend namespaces exist (`thrift_backend::`, `flatbuffers_backend::`)
- ✅ Wire format mappings are 1:1 (straightforward converters)

**Your Mission**: Implement converters to bridge wire formats ↔ domain model

## Architecture Overview

```
Wire Formats          Converters           Domain Model
(Thrift/FlatBuffers)  (Phase 1 work)      (Exists)
       ↓                    ↓                  ↓
   thrift::              to_domain()      domain::
   metadata::            from_domain()    metadata::
   chunk                     ↕             chunk
```

## Vertical Slice Approach

**Strategy**: Implement one complete type before moving to next

**Advantages**:
- Validates architecture early
- Catches issues quickly
- Provides working example for remaining types
- Reduces risk

**Order**:
1. `chunk` (simplest - 3 fields)
2. `directory` (simple - 3 fields)
3. `dir_entry` (simple - 2 fields)
4. `inode_data` (medium - timestamps, optional fields)
5. `string_table` (complex - FSST compression, packed index)
6. `metadata` (most complex - all fields, optional arrays)

## Phase 1 Tasks

### Task 1.1: Thrift Chunk Converter (1 hour)

**Files to Create**:
- `include/dwarfs/metadata/converters/thrift_converters.h`
- `src/metadata/converters/thrift_converters.cpp`
- `test/metadata/converters/thrift_chunk_test.cpp`

**Implementation**:

**Header** (`thrift_converters.h`):
```cpp
#pragma once

#include "dwarfs/metadata/domain/chunk.h"

namespace thrift::metadata { struct chunk; }

namespace dwarfs::metadata::converters::thrift {

// Wire format → Domain
domain::chunk chunk_to_domain(
    ::thrift::metadata::chunk const& wire);

// Domain → Wire format
::thrift::metadata::chunk chunk_from_domain(
    domain::chunk const& domain);

} // namespace thrift
```

**Implementation** (`thrift_converters.cpp`):
```cpp
#include "dwarfs/metadata/converters/thrift_converters.h"
#include <thrift/metadata_types.h>

namespace dwarfs::metadata::converters::thrift {

domain::chunk chunk_to_domain(
    ::thrift::metadata::chunk const& wire) {
  // Direct field mapping
  return domain::chunk(
      wire.block(),
      wire.offset(),
      wire.size());
}

::thrift::metadata::chunk chunk_from_domain(
    domain::chunk const& domain) {
  ::thrift::metadata::chunk wire;
  wire.block() = domain.block();
  wire.offset() = domain.offset();
  wire.size() = domain.size();
  return wire;
}

} // namespace thrift
```

**Test** (`thrift_chunk_test.cpp`):
```cpp
#include <gtest/gtest.h>
#include "dwarfs/metadata/converters/thrift_converters.h"
#include <thrift/metadata_types.h>

using namespace dwarfs::metadata;

TEST(ThriftChunkConverter, ToDomain) {
  ::thrift::metadata::chunk wire;
  wire.block() = 42;
  wire.offset() = 1024;
  wire.size() = 4096;

  auto domain = converters::thrift::chunk_to_domain(wire);

  EXPECT_EQ(domain.block(), 42u);
  EXPECT_EQ(domain.offset(), 1024u);
  EXPECT_EQ(domain.size(), 4096u);
}

TEST(ThriftChunkConverter, FromDomain) {
  domain::chunk domain(42, 1024, 4096);

  auto wire = converters::thrift::chunk_from_domain(domain);

  EXPECT_EQ(wire.block(), 42u);
  EXPECT_EQ(wire.offset(), 1024u);
  EXPECT_EQ(wire.size(), 4096u);
}

TEST(ThriftChunkConverter, RoundTrip) {
  domain::chunk original(42, 1024, 4096);

  auto wire = converters::thrift::chunk_from_domain(original);
  auto restored = converters::thrift::chunk_to_domain(wire);

  EXPECT_EQ(original, restored);
}
```

**Verification**:
```bash
cmake --build build --target dwarfs_unit_tests
./build/dwarfs_unit_tests --gtest_filter="ThriftChunkConverter.*"
```

### Task 1.2: FlatBuffers Chunk Converter (1 hour)

**Files to Create**:
- `include/dwarfs/metadata/converters/flatbuffers_converters.h`
- `src/metadata/converters/flatbuffers_converters.cpp`
- `test/metadata/converters/flatbuffers_chunk_test.cpp`

**Implementation** (similar structure to Task 1.1):

**Header** (`flatbuffers_converters.h`):
```cpp
#pragma once

#include "dwarfs/metadata/domain/chunk.h"
#include <flatbuffers/flatbuffers.h>

namespace gen_flatbuffers { struct Chunk; }

namespace dwarfs::metadata::converters::flatbuffers {

// Wire format → Domain
domain::chunk chunk_to_domain(
    gen_flatbuffers::Chunk const* wire);

// Domain → Wire format (returns offset for FlatBufferBuilder)
::flatbuffers::Offset<gen_flatbuffers::Chunk>
chunk_from_domain(
    ::flatbuffers::FlatBufferBuilder& builder,
    domain::chunk const& domain);

} // namespace flatbuffers
```

**Key Differences from Thrift**:
- FlatBuffers uses pointers (`Chunk const*` not `Chunk const&`)
- Writing requires `FlatBufferBuilder` passed by reference
- Returns `Offset<T>` for builder chaining

### Task 1.3-1.6: Remaining Types (6-8 hours)

**Repeat vertical slice** for:
- directory (Task 1.3)
- dir_entry (Task 1.4)
- inode_data (Task 1.5 - more complex, handle timestamps)
- string_table (Task 1.6 - handle FSST, packed index)
- metadata (Task 1.7 - most complex, all fields)

Each type follows same pattern:
1. Create converters (both Thrift and FlatBuffers)
2. Write tests (to_domain, from_domain, round-trip)
3. Verify tests pass
4. Move to next type

## Critical Implementation Notes

### Thrift Frozen2 Handling

Thrift uses Frozen2 views (lazy access):
```cpp
// Frozen2 view - lazy
apache::thrift::frozen::View<thrift::metadata::chunk> wire_view;

// Domain conversion - eager
domain::chunk domain = chunk_to_domain(wire_view);
```

### FlatBuffers Vector Handling

FlatBuffers vectors need iteration:
```cpp
// FlatBuffers vector
auto* wire_chunks = fb_meta->chunks();  // flatbuffers::Vector<Chunk>*

// Domain conversion
std::vector<domain::chunk> domain_chunks;
for (auto const* wire_chunk : *wire_chunks) {
  domain_chunks.push_back(chunk_to_domain(wire_chunk));
}
```

### String Table Compression

Both formats support FSST compression:
```cpp
// Check if compressed
if (wire_table.symtab()) {
  // Decompress using FSST
  domain_table.symtab = std::string(wire_table.symtab());
  domain_table.buffer = decompress_fsst(
      wire_table.buffer(),
      wire_table.symtab());
} else {
  domain_table.buffer = wire_table.buffer();
}
```

## CMake Integration

**Add to `src/metadata/CMakeLists.txt`** (or create if doesn't exist):
```cmake
add_library(dwarfs_metadata_converters STATIC
  converters/thrift_converters.cpp
  converters/flatbuffers_converters.cpp
)

target_include_directories(dwarfs_metadata_converters PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(dwarfs_metadata_converters PUBLIC
  dwarfs_metadata_domain
  $<$<BOOL:${DWARFS_HAVE_THRIFT}>:thrift_metadata>
  $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:dwarfs_metadata_flatbuffers>
)
```

## Success Criteria

- ✅ All converter functions compile
- ✅ All round-trip tests pass
- ✅ Zero coupling between Thrift and FlatBuffers converters
- ✅ Converters use only domain model + wire format types

## Next Phase Preview

After Phase 1 complete:
- **Phase 2**: Create abstract `metadata_reader` and `metadata_writer` interfaces
- **Phase 3**: Implement transcoder using converters
- **Phase 4**: Refactor `metadata_v2` to use domain model

## Starting Phase 1

**First Action**: Implement Task 1.1 (Thrift chunk converter)

**Verification**: Run test, confirm round-trip is lossless

**Next Action**: Task 1.2 (FlatBuffers chunk converter)

---

**Ready to Start**: Yes
**Start with**: Task 1.1 - Thrift chunk converter
**Goal**: Complete Phase 1 (all converters)
// ... existing code ...