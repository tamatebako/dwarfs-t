# Session 74 Continuation Prompt

**Start Here**: Quick-start guide for implementing Modern Thrift serializer

---

## Context

Session 73 **COMPLETED** jemalloc integration. Folly and FBThrift build successfully with custom unprefixed jemalloc. The build currently fails because the Modern Thrift serializer implementation doesn't exist yet.

## Current State

✅ **Working**:
- jemalloc:arm64-osx@5.3.0#5 (custom, unprefixed symbols)
- Folly v2025.12.29.00 builds and links
- FBThrift v2025.12.29.00 installed
- Thrift code generated for all schemas
- CMake detects all 3 formats

❌ **Missing** (this session's work):
- Modern Thrift serializer implementation
- `register_thrift_serializer()` function

## Quick Start (4-5 hours)

### Step 1: Read Documentation (5 min)

```bash
# Read the full plan
cat doc/SESSION_74_CONTINUATION_PLAN.md

# Read Session 73 completion
cat doc/SESSION_73_COMPLETION_STATUS.md
```

### Step 2: Study Reference Implementation (10 min)

**FlatBuffers serializer** (our template):
- Header: `include/dwarfs/metadata/serialization/flatbuffers_serializer.h`
- Implementation: `src/metadata/serialization/flatbuffers_serializer.cpp`

**Key patterns**:
1. Inherit from `metadata_serialization_facade`
2. Implement `serialize()`, `deserialize()`, `get_format()`
3. Register with priority and magic bytes

### Step 3: Create Thrift Serializer Skeleton (30 min)

**Files to create**:
```bash
# Header
touch include/dwarfs/metadata/serialization/thrift_serializer.h

# Implementation
touch src/metadata/serialization/thrift_serializer.cpp
```

**Template** (thrift_serializer.h):
```cpp
#pragma once
#ifdef DWARFS_HAVE_THRIFT

#include "dwarfs/metadata/serialization/metadata_serialization_facade.h"
#include <thrift/lib/cpp2/protocol/Serializer.h>

namespace dwarfs::metadata::serialization {

class thrift_serializer : public metadata_serialization_facade {
public:
  void serialize(domain::metadata const&, byte_buffer&) override;
  void deserialize(std::span<uint8_t const>, domain::metadata&) override;
  serialization_format get_format() const override {
    return serialization_format::thrift_compact;
  }
};

void register_thrift_serializer();

} // namespace dwarfs::metadata::serialization

#endif // DWARFS_HAVE_THRIFT
```

### Step 4: Implement Serialization (60 min)

**Key task**: Convert domain model to Thrift types, then serialize with CompactProtocol

**Reference**: Study how FlatBuffers serializer converts domain → flatbuffers types in `flatbuffers_serializer.cpp`

**Magic bytes**: `{0x82, 0x21}` (CompactProtocol header)

### Step 5: Implement Deserialization (60 min)

**Key task**: Deserialize with CompactProtocol, then convert Thrift types to domain model

### Step 6: Wire Up Registration (15 min)

**File**: `src/metadata/serialization/init_serializers.cpp`

**Add**:
```cpp
#ifdef DWARFS_HAVE_THRIFT
#include "dwarfs/metadata/serialization/thrift_serializer.h"
#endif

// In initialize_serializers():
#ifdef DWARFS_HAVE_THRIFT
  register_thrift_serializer();  // Priority 100
#endif
```

### Step 7: Add Format Detection (15 min)

**File**: `src/metadata/serialization/serializer_registry.cpp`

**Enhance** `detect_format()`:
```cpp
// Check CompactProtocol magic (0x82 0x21)
if (data.size() >= 2 && data[0] == 0x82 && data[1] == 0x21) {
  return serialization_format::thrift_compact;
}
```

### Step 8: Update CMake (15 min)

**File**: `cmake/metadata_serialization.cmake`

**Add** to METADATA_SERIALIZATION_SOURCES (under `if(DWARFS_HAVE_THRIFT)`):
```cmake
src/metadata/serialization/thrift_serializer.cpp
```

### Step 9: Build and Test (60 min)

```bash
# Clean build
rm -rf build-modern-thrift

# Configure
cmake -B build-modern-thrift -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/src/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON

# Build
ninja -C build-modern-thrift

# Test
ctest --test-dir build-modern-thrift --tests-regex "metadata" -j
```

### Step 10: Verify Three Formats (30 min)

```bash
# Create test data
mkdir -p /tmp/test-3formats
echo "test content" > /tmp/test-3formats/file.txt

# Test FlatBuffers
./build-modern-thrift/mkdwarfs -i /tmp/test-3formats -o /tmp/test.dff
xxd /tmp/test.dff | head -2  # Should see "DFBF"

# Test Modern Thrift
./build-modern-thrift/mkdwarfs -i /tmp/test-3formats -o /tmp/test.dtc \
  --metadata-format=thrift-compact
xxd /tmp/test.dtc | head -2  # Should see "82 21"

# Test Legacy Thrift
./build-modern-thrift/mkdwarfs -i /tmp/test-3formats -o /tmp/test.dth \
  --metadata-format=thrift-legacy
xxd /tmp/test.dth | head -2  # No magic bytes

# Verify all read correctly
./build-modern-thrift/dwarfsck /tmp/test.dff --check-integrity
./build-modern-thrift/dwarfsck /tmp/test.dtc --check-integrity
./build-modern-thrift/dwarfsck /tmp/test.dth --check-integrity
```

---

## Success Criteria

- [ ] All code compiles without errors
- [ ] All 66 metadata tests pass
- [ ] Can create and read `.dtc` files
- [ ] Magic bytes correct (`0x82 0x21`)
- [ ] All 3 formats work: `.dff`, `.dtc`, `.dth`
- [ ] Format auto-detection works

---

## Key References

- **Full Plan**: [`doc/SESSION_74_CONTINUATION_PLAN.md`](SESSION_74_CONTINUATION_PLAN.md)
- **Session 73 Status**: [`doc/SESSION_73_COMPLETION_STATUS.md`](SESSION_73_COMPLETION_STATUS.md)
- **FlatBuffers Template**: `src/metadata/serialization/flatbuffers_serializer.cpp`
- **Thrift Schema**: `thrift/metadata.thrift`

---

## Estimated Time

**Total**: 4-5 hours

**Breakdown**:
- Skeleton: 30 min
- Serialization: 60 min
- Deserialization: 60 min
- Registration + Detection: 30 min
- CMake: 15 min
- Testing: 90 min

---

**Created**: 2026-01-05 09:58 HKT
**Next Session**: Implement Modern Thrift serializer (Session 74)