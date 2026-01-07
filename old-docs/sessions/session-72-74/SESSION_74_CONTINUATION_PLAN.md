# Session 74: Modern Thrift Serializer Implementation

**Prerequisites**: Session 73 complete (jemalloc working)
**Goal**: Implement Modern Thrift serializer and complete v0.17.0

---

## Overview

Session 73 resolved the jemalloc integration blocker. Now we need to implement the Modern Thrift serializer to complete the three-format architecture.

### Current State

✅ **Working**:
- Custom jemalloc with unprefixed symbols
- Folly v2025.12.29.00 builds and links
- FBThrift v2025.12.29.00 installed
- Thrift code generation for all schemas
- CMake detects all 3 formats

❌ **Missing**:
- Modern Thrift serializer implementation
- `register_thrift_serializer()` function
- Modern Thrift serialization facade
- CompactProtocol magic byte detection

---

## Architecture

### Three-Format System

```
┌─────────────────────────────────────────────┐
│         Format Detection (Priority)         │
├─────────────────────────────────────────────┤
│ 120: FlatBuffers  ("DFBF" at offset 0 or 8)│
│ 100: Modern Thrift (0x82 0x21 at offset 0) │
│  50: Legacy Thrift (no magic, fallback)     │
└─────────────────────────────────────────────┘
                    ↓
        ┌───────────┴───────────┐
        │   Serializer Registry │
        └───────────┬───────────┘
                    ↓
    ┌───────────────┼───────────────┐
    ↓               ↓               ↓
FlatBuffers    Modern Thrift   Legacy Thrift
Serializer     Serializer      Serializer
(READY)        (TODO)          (READY)
```

### Implementation Pattern

Follow the same pattern as FlatBuffers serializer:

1. **Serializer Class** (`thrift_serializer.cpp`)
   - Implements `metadata_serialization_facade` interface
   - Uses CompactProtocol for serialization
   - Handles domain model ↔ Thrift type conversion

2. **Registration** (`init_serializers.cpp`)
   - `register_thrift_serializer()` function
   - Registers with priority 100
   - Magic bytes: `{0x82, 0x21}`

3. **Detection** (`serializer_registry.cpp`)
   - Check bytes at offset 0
   - If `0x82 0x21` → Modern Thrift
   - If "DFBF" → FlatBuffers
   - Else → Legacy Thrift

---

## Implementation Steps

### Phase 1: Thrift Serializer Skeleton (30 min)

**Files to Create**:
1. `src/metadata/serialization/thrift_serializer.cpp`
2. `include/dwarfs/metadata/serialization/thrift_serializer.h`

**Structure**:
```cpp
// thrift_serializer.h
#pragma once
#ifdef DWARFS_HAVE_THRIFT

namespace dwarfs::metadata::serialization {

class thrift_serializer : public metadata_serialization_facade {
public:
  // Implement facade interface
  void serialize(domain::metadata const&, byte_buffer&) override;
  void deserialize(std::span<uint8_t const>, domain::metadata&) override;
  serialization_format get_format() const override;
};

void register_thrift_serializer();

} // namespace

#endif
```

### Phase 2: Serialization Implementation (60 min)

**Key Components**:

1. **Domain → Thrift Conversion**:
```cpp
auto to_thrift_metadata(domain::metadata const& meta) {
  thrift::metadata::metadata result;
  // Convert each field
  result.inodes = to_thrift_inodes(meta.inodes());
  result.directories = to_thrift_directories(meta.directories());
  // ... etc
  return result;
}
```

2. **CompactProtocol Serialization**:
```cpp
void serialize(domain::metadata const& meta, byte_buffer& buffer) {
  auto thrift_meta = to_thrift_metadata(meta);

  // Serialize with CompactProtocol
  apache::thrift::CompactSerializer<thrift::metadata::metadata> serializer;
  std::string output;
  serializer.serialize(thrift_meta, &output);

  buffer.assign(output.begin(), output.end());
}
```

3. **Magic Bytes**:
```cpp
constexpr std::array<uint8_t, 2> COMPACT_PROTOCOL_MAGIC = {0x82, 0x21};
```

### Phase 3: Deserialization Implementation (60 min)

**Key Components**:

1. **CompactProtocol Deserialization**:
```cpp
void deserialize(std::span<uint8_t const> data, domain::metadata& meta) {
  thrift::metadata::metadata thrift_meta;

  apache::thrift::CompactSerializer<thrift::metadata::metadata> deserializer;
  deserializer.deserialize(data, &thrift_meta);

  from_thrift_metadata(thrift_meta, meta);
}
```

2. **Thrift → Domain Conversion**:
```cpp
void from_thrift_metadata(thrift::metadata::metadata const& src, domain::metadata& dest) {
  dest.set_inodes(from_thrift_inodes(src.inodes));
  dest.set_directories(from_thrift_directories(src.directories));
  // ... etc
}
```

### Phase 4: Registration (15 min)

**File**: `src/metadata/serialization/init_serializers.cpp`

**Changes**:
```cpp
#ifdef DWARFS_HAVE_THRIFT
#include "dwarfs/metadata/serialization/thrift_serializer.h"
#endif

void initialize_serializers() {
  register_legacy_thrift_serializer();  // Priority 50

#ifdef DWARFS_HAVE_FLATBUFFERS
  register_flatbuffers_serializer();    // Priority 120
#endif

#ifdef DWARFS_HAVE_THRIFT
  register_thrift_serializer();         // Priority 100
#endif
}
```

### Phase 5: Format Detection (15 min)

**File**: `src/metadata/serialization/serializer_registry.cpp`

**Enhanced Detection**:
```cpp
std::optional<serialization_format> detect_format(std::vector<uint8_t> const& data) {
  if (data.size() < 12) return std::nullopt;

  // Check for FlatBuffers "DFBF" at offset 0 or 8
  if (has_flatbuffers_magic(data, 0) || has_flatbuffers_magic(data, 8)) {
    return serialization_format::flatbuffers;
  }

  // Check for CompactProtocol magic at offset 0
  if (data.size() >= 2 && data[0] == 0x82 && data[1] == 0x21) {
    return serialization_format::thrift_compact;
  }

  // Fallback to legacy Thrift
  return serialization_format::legacy_thrift;
}
```

### Phase 6: CMake Integration (15 min)

**File**: `cmake/metadata_serialization.cmake`

**Ensure Modern Thrift Files Compiled**:
```cmake
if(DWARFS_HAVE_THRIFT)
  list(APPEND METADATA_SERIALIZATION_SOURCES
    src/metadata/serialization/thrift_serializer.cpp
  )
  list(APPEND METADATA_SERIALIZATION_HEADERS
    include/dwarfs/metadata/serialization/thrift_serializer.h
  )
endif()
```

### Phase 7: Testing (30 min)

**Create Test**: `test/metadata/serialization/thrift_serializer_test.cpp`

**Test Cases**:
1. Round-trip serialization
2. Magic byte detection
3. Format priority (FlatBuffers > Modern Thrift > Legacy)
4. Cross-format compatibility (read Thrift, write FlatBuffers, etc.)

### Phase 8: Integration Testing (30 min)

**End-to-End Test**:
```bash
# Create with Modern Thrift
./mkdwarfs -i /tmp/test -o /tmp/test.dtc --metadata-format=thrift-compact

# Verify magic bytes
xxd /tmp/test.dtc | head -2  # Should see "82 21"

# Check with dwarfsck
./dwarfsck /tmp/test.dtc --check-integrity

# Extract
./dwarfsextract -i /tmp/test.dtc -o /tmp/extracted

# Verify
diff -r /tmp/test /tmp/extracted
```

---

## File Organization

### New Files

```
include/dwarfs/metadata/serialization/
  thrift_serializer.h                    (NEW)

src/metadata/serialization/
  thrift_serializer.cpp                  (NEW)

test/metadata/serialization/
  thrift_serializer_test.cpp             (NEW)
```

### Modified Files

```
src/metadata/serialization/
  init_serializers.cpp                   (add register call)
  serializer_registry.cpp                (add CompactProtocol detection)

cmake/
  metadata_serialization.cmake           (add new files)
```

---

## Success Criteria

✅ **Build**:
- All code compiles without errors
- No jemalloc-related errors
- Modern Thrift library links correctly

✅ **Functionality**:
- Can create `.dtc` files with Modern Thrift
- Can read `.dtc` files
- Magic bytes correct (`0x82 0x21`)
- Round-trip works (serialize → deserialize → identical)

✅ **Integration**:
- All 3 formats work together
- Format auto-detection works
- mkdwarfs `--metadata-format` option supports all 3
- dwarfsck reads all 3 formats

✅ **Tests**:
- All metadata serialization tests pass (66 tests)
- New Modern Thrift tests pass
- Cross-format tests pass

---

## Time Estimate

| Phase | Time | Cumulative |
|-------|------|------------|
| 1. Skeleton | 30 min | 0:30 |
| 2. Serialization | 60 min | 1:30 |
| 3. Deserialization | 60 min | 2:30 |
| 4. Registration | 15 min | 2:45 |
| 5. Detection | 15 min | 3:00 |
| 6. CMake | 15 min | 3:15 |
| 7. Testing | 30 min | 3:45 |
| 8. Integration | 30 min | 4:15 |

**Total**: ~4-5 hours

---

## Known Considerations

### Thrift Type Mapping

Modern Thrift uses **CompactProtocol**, not Frozen2:
- **Legacy Thrift**: Hand-coded binary with Frozen2 layouts
- **Modern Thrift**: Standard Thrift CompactProtocol
- **FlatBuffers**: FlatBuffers binary format

### Magic Bytes

- **FlatBuffers**: `"DFBF"` (0x44 0x46 0x42 0x46)
- **Modern Thrift**: `0x82 0x21` (CompactProtocol header)
- **Legacy Thrift**: None (fallback)

### Compatibility

All 3 formats use the same **domain model** (`metadata::domain::metadata`), ensuring cross-format compatibility via the facade pattern.

---

## References

- Session 73 Completion: [`SESSION_73_COMPLETION_STATUS.md`](SESSION_73_COMPLETION_STATUS.md)
- FlatBuffers Implementation: `src/metadata/serialization/flatbuffers_serializer.cpp`
- Thrift Schema: `thrift/metadata.thrift`
- CompactProtocol Spec: https://github.com/apache/thrift/blob/master/doc/specs/thrift-compact-protocol.md

---

**Plan Created**: 2026-01-05 09:57 HKT