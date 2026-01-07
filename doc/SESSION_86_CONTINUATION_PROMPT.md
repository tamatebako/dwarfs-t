# Session 86: Modern Thrift Architecture Design - Continuation Prompt

**Start Here**: Begin Modern Thrift implementation with architecture design

---

## Quick Context

Session 85 achieved:
- ✅ Legacy Thrift (Frozen2) documentation complete
- ✅ metadata-formats.md created
- ✅ Session docs archived
- ✅ All tests passing (4/4)
- ⏳ Modern Thrift implementation ready to start

**Your Mission**: Design the Modern Thrift serialization architecture

---

## Step 1: Read Context (10 min)

```bash
# Read the plan
cat doc/SESSION_86_MODERN_THRIFT_CONTINUATION_PLAN.md

# Verify current status
cat .kilocode/rules/memory-bank/context.md

# Review existing architecture
cat doc/metadata-formats.md
```

**Expected**: Understand three-format vision and current status

---

## Step 2: Review Existing Implementations (20 min)

### Task 2.1: Study Strategy Pattern

Read these files to understand the current architecture:

```bash
# Strategy Pattern implementation
cat include/dwarfs/metadata/domain/metadata.h
cat src/reader/internal/backend_adapter.cpp
cat src/reader/internal/backend_adapter.h
```

**Key Points to Note**:
- How domain model is format-agnostic
- How adapters convert domain ↔ format-specific types
- How serializer registry works

### Task 2.2: Study FlatBuffers Serializer

```bash
# FlatBuffers implementation
cat include/dwarfs/metadata/serialization/flatbuffers_serializer.h
cat src/metadata/serialization/flatbuffers_serializer.cpp
```

**Key Points to Note**:
- serialize() signature
- deserialize() signature
- Magic bytes handling ("DFBF")
- Error handling patterns

### Task 2.3: Study Legacy Thrift Serializer

```bash
# Legacy Thrift implementation
cat include/dwarfs/metadata/legacy/frozen2_serializer.h
cat src/metadata/legacy/frozen2_serializer.cpp
```

**Key Points to Note**:
- Two-phase serialization (schema + values)
- No magic bytes (fallback format)
- How it converts domain model

---

## Step 3: Design Modern Thrift Architecture (30 min)

### Task 3.1: Create Header File

Create: `include/dwarfs/metadata/modern/thrift_compact_serializer.h`

```cpp
/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <utility>
#include <vector>

#include "dwarfs/metadata/domain/metadata.h"

namespace dwarfs::metadata::modern {

/**
 * Modern Thrift CompactProtocol serializer
 *
 * Uses apache::thrift::CompactSerializer from fbthrift v2025.12.29.00
 * to serialize metadata in CompactProtocol format with magic bytes.
 *
 * Magic Bytes: {0x82, 0x21}
 * Priority: 100 (between Legacy Thrift 50 and FlatBuffers 120)
 */
class ThriftCompactSerializer {
 public:
  /**
   * Serialize metadata to Modern Thrift CompactProtocol
   *
   * @param meta Domain metadata to serialize
   * @return Byte vector with magic bytes + CompactProtocol data
   * @throws std::runtime_error on serialization failure
   */
  static std::vector<uint8_t>
  serialize(domain::metadata const& meta);

  /**
   * Deserialize Modern Thrift CompactProtocol to metadata
   *
   * @param data Byte vector with magic bytes + CompactProtocol data
   * @return Domain metadata
   * @throws std::runtime_error on deserialization failure or invalid magic
   */
  static domain::metadata
  deserialize(std::vector<uint8_t> const& data);

  /**
   * Verify magic bytes
   *
   * @param data Byte vector to check
   * @return true if magic bytes {0x82, 0x21} present
   */
  static bool has_magic_bytes(std::vector<uint8_t> const& data);

 private:
  static constexpr uint8_t MAGIC_BYTES[2] = {0x82, 0x21};
};

} // namespace dwarfs::metadata::modern
```

### Task 3.2: Create Documentation

Create: `doc/MODERN_THRIFT_ARCHITECTURE.md`

Document:
- Overview of Modern Thrift approach
- How it differs from Legacy Thrift
- CompactProtocol advantages
- Integration with Strategy Pattern
- Conversion flow: domain → Thrift types → bytes

---

## Step 4: Update Build System (20 min)

### Task 4.1: Add CMake Option

Edit: `cmake/metadata_serialization.cmake`

Add:
```cmake
# Modern Thrift CompactProtocol support (optional)
option(DWARFS_WITH_MODERN_THRIFT "Enable Modern Thrift metadata format" OFF)

if(DWARFS_WITH_MODERN_THRIFT)
  # Find dependencies
  find_package(folly CONFIG REQUIRED)
  find_package(FBThrift CONFIG REQUIRED)
  find_package(jemalloc REQUIRED)

  # Add compile definition
  add_compile_definitions(DWARFS_HAVE_MODERN_THRIFT=1)

  message(STATUS "Modern Thrift support: ENABLED")
else()
  message(STATUS "Modern Thrift support: DISABLED")
endif()
```

### Task 4.2: Update Library Configuration

Edit: `cmake/libdwarfs.cmake`

Add Modern Thrift sources conditionally:
```cmake
if(DWARFS_WITH_MODERN_THRIFT)
  target_sources(dwarfs_common PRIVATE
    src/metadata/modern/thrift_compact_serializer.cpp
    src/metadata/modern/domain_to_thrift.cpp
    src/metadata/modern/thrift_to_domain.cpp
  )

  target_link_libraries(dwarfs_common PUBLIC
    FBThrift::thriftcpp2
    Folly::folly
  )
endif()
```

---

## Step 5: Create Architecture Document (30 min)

### File: `doc/MODERN_THRIFT_ARCHITECTURE.md`

**Content Structure**:

1. **Overview**
   - Modern Thrift vs Legacy Thrift
   - Why CompactProtocol
   - Integration with v0.16.0 Strategy Pattern

2. **Data Flow**
   ```
   domain::metadata
         ↓
   domain_to_thrift()
         ↓
   thrift::Metadata (generated types)
         ↓
   apache::thrift::CompactSerializer::serialize()
         ↓
   prepend magic bytes {0x82, 0x21}
         ↓
   std::vector<uint8_t>
   ```

3. **Key Decisions**
   - CompactProtocol chosen for size efficiency
   - Magic bytes for format detection
   - Priority 100 (medium-high)

4. **Dependencies**
   - fbthrift v2025.12.29.00
   - Folly v2025.12.29.00
   - jemalloc (custom port)

---

## Step 6: Write Implementation Status (10 min)

### File: `doc/SESSION_86_IMPLEMENTATION_STATUS.md`

Track:
- [x] Architecture designed
- [x] Header file created
- [x] Documentation written
- [x] CMake updated
- [ ] Implementation (next session)
- [ ] Tests (next session)

---

## Success Criteria

✅ **Architecture**:
- [ ] Header file created with clear interfaces
- [ ] Documentation explains design
- [ ] CMake configuration supports Modern Thrift
- [ ] Strategy Pattern integration clear

✅ **Documentation**:
- [ ] MODERN_THRIFT_ARCHITECTURE.md created
- [ ] Design decisions documented
- [ ] Ready for implementation in Session 87

---

## Common Issues & Solutions

### Issue: fbthrift not found

**Solution**: Use vcpkg with overlay ports:
```bash
cmake -B build -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DDWARFS_WITH_MODERN_THRIFT=ON
```

### Issue: Unclear how to integrate

**Solution**: Study existing FlatBuffers implementation as template:
```bash
grep -r "flatbuffers_serializer" include/ src/
```

---

## Time Budget

- Step 1 (Context): 10 min
- Step 2 (Review): 20 min
- Step 3 (Design): 30 min
- Step 4 (CMake): 20 min
- Step 5 (Documentation): 30 min
- Step 6 (Status): 10 min
- **Total**: 120 minutes (2 hours)

---

## Next Session

After Session 86, proceed to Session 87: Thrift Schema Definition

Read: `doc/SESSION_87_CONTINUATION_PROMPT.md` (will be created in Session 86)

---

**Created**: 2026-01-06
**Session**: 86
**Goal**: Design Modern Thrift architecture
**Next**: Implement Thrift schema and converters