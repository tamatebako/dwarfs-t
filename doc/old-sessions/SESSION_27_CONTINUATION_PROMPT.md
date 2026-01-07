// ... existing code ...
# Session 27: Phase 1 Implementation - Guard-Free OOP Converters

**Date**: 2025-12-22+
**Objective**: Implement domain model converters using pure OOP (ZERO guards)
**Approach**: Vertical slice - one type at a time, CMake controls compilation
**Estimated Time**: 10-12 hours

## Context from Session 26

**Key Decision**: Use **guard-free OOP architecture** where:
- ✅ CMake controls which files compile (not preprocessor guards)
- ✅ Headers always available (forward declarations)
- ✅ Implementations are pure C++ (no `#ifdef` pollution)
- ✅ Linker enforces constraints (clear error messages)

**Architecture Approved**:
```
Application → Converters → Domain Model
    ↓            ↓              ↓
filesystem_v2   thrift/      domain::
(uses domain)   flatbuffers  metadata
                (CMake-     (complete)
                 controlled)
```

## Guard-Free Design Principles

### Principle 1: Separate Compilation Units
Each format is **separate .cpp file**, CMake decides if it compiles.

### Principle 2: Headers Declare Interface
Headers use **forward declarations** only, no format-specific includes.

### Principle 3: CMake is Single Control Point
Format availability checked **once** in [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake), not in code.

### Principle 4: Linker Enforces Constraints
Missing format → **clear linker error**, not cryptic preprocessor message.

## Phase 1 Tasks

### Task 1: Infrastructure (30 minutes - 1 hour)

**Create Directory Structure**:
```bash
mkdir -p include/dwarfs/metadata/converters
mkdir -p src/metadata/converters
mkdir -p test/metadata/converters
```

**Create CMake Files**:
1. `src/metadata/converters/CMakeLists.txt` - Converter library
2. `test/metadata/converters/CMakeLists.txt` - Test suite
3. Wire into parent CMakeLists

**CMake Pattern** (NO GUARDS in source):
```cmake
# File: src/metadata/converters/CMakeLists.txt

set(CONVERTER_SOURCES "")

# CMake controls which files compile
if(DWARFS_HAVE_THRIFT)
  list(APPEND CONVERTER_SOURCES thrift_converters.cpp)
endif()

if(DWARFS_HAVE_FLATBUFFERS)
  list(APPEND CONVERTER_SOURCES flatbuffers_converters.cpp)
endif()

add_library(dwarfs_metadata_converters STATIC ${CONVERTER_SOURCES})

target_include_directories(dwarfs_metadata_converters PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/include  # For generated headers
)

target_link_libraries(dwarfs_metadata_converters PUBLIC
  dwarfs_metadata_domain
  $<$<BOOL:${DWARFS_HAVE_THRIFT}>:thrift_metadata>
  $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:dwarfs_metadata_flatbuffers>
)
```

### Task 2: Chunk Converters (1.5-2 hours)

**Files to Create** (all guard-free):

**Header** (`include/dwarfs/metadata/converters/thrift_converters.h`):
```cpp
#pragma once

#include "dwarfs/metadata/domain/chunk.h"

// Forward declarations - NO Thrift includes
namespace thrift::metadata {
  struct chunk;
}

namespace dwarfs::metadata::converters::thrift {

// Wire → Domain
domain::chunk chunk_to_domain(
    ::thrift::metadata::chunk const& wire);

// Domain → Wire
::thrift::metadata::chunk chunk_from_domain(
    domain::chunk const& domain);

} // namespace thrift
```

**Implementation** (`src/metadata/converters/thrift_converters.cpp` - **NO GUARDS**):
```cpp
#include "dwarfs/metadata/converters/thrift_converters.h"
#include <thrift/metadata_types.h>

// Pure C++ - CMake decides if this compiles

namespace dwarfs::metadata::converters::thrift {

domain::chunk chunk_to_domain(
    ::thrift::metadata::chunk const& wire) {
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

**Test** (`test/metadata/converters/thrift_chunk_test.cpp` - **NO GUARDS**):
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

**Repeat for FlatBuffers** (similar pattern, different types).

**Verification**:
```bash
# Build and test
cmake -B build -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build
ctest --test-dir build --tests-regex "Chunk"
```

### Task 3-7: Remaining Types (8-10 hours)

**Implementation Order** (simplest → complex):
1. **directory** (1.5h) - 3 uint32 fields
2. **dir_entry** (1.5h) - 2 uint32 fields
3. **inode_data** (3h) - 13 fields, 2 optional (v2.2 compat)
4. **string_table** (3h) - FSST compression handling
5. **metadata** (4-5h) - 32 fields, 21 optional, nested types

**Process per type**:
1. Add functions to headers (incremental)
2. Implement in .cpp (**NO GUARDS**)
3. Write tests (**NO GUARDS**)
4. Build, test, verify
5. Commit
6. Next type

## Critical Implementation Notes

### Thrift Optional Handling

```cpp
// Thrift uses .has_value() for optionals
if (wire.devices().has_value()) {
  domain.devices = std::vector<uint64_t>(
      wire.devices()->begin(),
      wire.devices()->end());
}
```

### FlatBuffers Optional Handling

```cpp
// FlatBuffers uses nullptr for optionals
if (auto* devices = wire->devices(); devices != nullptr) {
  domain.devices = std::vector<uint64_t>();
  for (uint32_t i = 0; i < devices->size(); ++i) {
    domain.devices->push_back(devices->Get(i));
  }
}
```

### FlatBuffers Builder Pattern

```cpp
// Writing requires FlatBufferBuilder
::flatbuffers::Offset<::dwarfs::flatbuffers::Chunk>
chunk_from_domain(
    ::flatbuffers::FlatBufferBuilder& builder,
    domain::chunk const& domain) {
  return dwarfs::flatbuffers::CreateChunk(
      builder,
      domain.block(),
      domain.offset(),
      domain.size());
}
```

### String Table FSST Handling

```cpp
// Check if FSST compressed
if (wire.symtab().has_value()) {
  // Decompress buffer using FSST symtab
  domain.symtab = std::string(wire.symtab().value());
  domain.buffer = decompress_fsst(
      wire.buffer(),
      wire.symtab().value());
} else {
  // No compression
  domain.buffer = wire.buffer();
}
domain.packed_index = wire.packed_index();
```

## Build Verification Strategy

### Verify 3 Configurations

**Configuration 1: Both formats**
```bash
cmake -B build-both -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build-both converter_tests
./build-both/converter_tests
```

**Configuration 2: Thrift only**
```bash
cmake -B build-thrift -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=OFF
ninja -C build-thrift converter_tests
./build-thrift/converter_tests --gtest_filter="Thrift*"
```

**Configuration 3: FlatBuffers only**
```bash
cmake -B build-fb -DDWARFS_WITH_THRIFT=OFF -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build-fb converter_tests
./build-fb/converter_tests --gtest_filter="FlatBuffers*"
```

## Success Criteria

✅ **Zero Guards**:
- No `#ifdef DWARFS_HAVE_THRIFT` in any .cpp file
- No `#ifdef DWARFS_HAVE_FLATBUFFERS` in any .cpp file
- Headers use forward declarations only
- CMake controls all compilation

✅ **Clean Separation**:
- `thrift_converters.cpp` never includes FlatBuffers headers
- `flatbuffers_converters.cpp` never includes Thrift headers
- Each format is completely isolated

✅ **Functional Correctness**:
- All round-trip tests pass (domain → wire → domain == original)
- All 3 build configurations work
- No memory leaks (verify with ASAN if available)

✅ **Architecture Validation**:
- Converters are stateless utility functions
- Domain model is single source of truth
- Adding 3rd format = new .cpp file only

## Starting Point

**First Action**: Create infrastructure (Task 1)
- Directory structure
- CMake files
- Wire into build system

**Second Action**: Implement chunk converters (Task 2)
- Thrift chunk converter
- FlatBuffers chunk converter
- Verify both work before proceeding

**Validation**: Run all 3 build configurations, confirm zero guards in source.

---

**Mode Switch**: Switch to **Code mode** to implement Phase 1.

**See Also**:
- [`SESSION_27_IMPLEMENTATION_STATUS.md`](SESSION_27_IMPLEMENTATION_STATUS.md) - Progress tracker
- [`SESSION_25_COMPREHENSIVE_PLAN.md`](SESSION_25_COMPREHENSIVE_PLAN.md) - Original 6-phase plan
- Architecture diagram in updated [`architecture.md`](.kilocode/rules/memory-bank/architecture.md)
// ... existing code ...