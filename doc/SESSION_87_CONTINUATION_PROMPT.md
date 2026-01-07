# Session 87: Thrift Schema Definition - Continuation Prompt

**Start Here**: Implement Thrift schema and type converters for Modern Thrift

---

## Quick Context

Session 86 achieved:
- ✅ Architecture designed
- ✅ Header files created
- ✅ Documentation complete
- ✅ CMake configuration ready
- ⏳ Implementation pending

**Your Mission**: Define Thrift IDL schema and implement bidirectional converters

---

## Step 1: Read Context (5 min)

```bash
# Review architecture
cat doc/MODERN_THRIFT_ARCHITECTURE.md

# Review Session 86 summary
cat doc/SESSION_86_COMPLETION_SUMMARY.md

# Review domain model
cat include/dwarfs/metadata/domain/metadata.h

# Verify current status
cat doc/SESSION_86_IMPLEMENTATION_STATUS.md
```

**Expected**: Understand Modern Thrift architecture and domain model structure

---

## Step 2: Create Thrift IDL Schema (40 min)

### Task 2.1: Study Existing Thrift Schema

```bash
# Read existing Legacy Thrift schema (for reference)
cat thrift/metadata.thrift
```

**Key Points to Note**:
- How structs are defined
- How optional fields are marked
- How collections are handled
- Naming conventions

### Task 2.2: Create Modern Thrift Schema

**File**: `thrift/metadata_modern.thrift`

**Schema Structure**:
```thrift
namespace cpp dwarfs.thrift.modern

// Based on domain::metadata structure
struct Chunk {
  1: i64 block
  2: i64 offset
  3: i32 size
}

struct Directory {
  1: i32 first_entry
  2: i32 parent_entry
  // ... map all domain::directory fields
}

struct InodeData {
  1: i32 mode_index
  2: i32 owner_index
  3: i32 group_index
  // ... map all domain::inode_data fields
}

struct Metadata {
  1: list<Chunk> chunks
  2: list<Directory> directories
  3: list<InodeData> inodes
  // ... map all domain::metadata fields
  // Use optional<> for std::optional<> fields
}
```

**Requirements**:
- Map ALL fields from [`domain::metadata`](../include/dwarfs/metadata/domain/metadata.h)
- Use `optional<>` for `std::optional<>` fields
- Use Thrift types: `i32`, `i64`, `string`, `list<>`, `map<>`, `set<>`
- Follow naming convention: snake_case → camelCase

### Task 2.3: Configure fbthrift Compiler in CMake

**Edit**: `cmake/metadata_serialization.cmake`

**Add after line 169** (Modern Thrift section):
```cmake
if(DWARFS_HAVE_THRIFT)
  # Thrift compiler configuration
  set(THRIFT_IDL ${CMAKE_SOURCE_DIR}/thrift/metadata_modern.thrift)
  set(THRIFT_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen-cpp2)

  # Generate C++ code from Thrift IDL
  add_custom_command(
    OUTPUT ${THRIFT_GEN_DIR}/metadata_modern_types.h
           ${THRIFT_GEN_DIR}/metadata_modern_types.cpp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${THRIFT_GEN_DIR}
    COMMAND thrift1 --gen mstch_cpp2:no_metadata
            -out ${THRIFT_GEN_DIR}
            ${THRIFT_IDL}
    DEPENDS ${THRIFT_IDL}
    COMMENT "Generating Modern Thrift C++ types"
    VERBATIM
  )

  # Create target for generated code
  add_custom_target(dwarfs_metadata_modern_thrift_generate
    DEPENDS ${THRIFT_GEN_DIR}/metadata_modern_types.h
  )
endif()
```

---

## Step 3: Implement Type Converters (60 min)

### Task 3.1: Domain → Thrift Converter

**File**: `src/metadata/modern/domain_to_thrift.cpp`

**Header**: `include/dwarfs/metadata/modern/domain_to_thrift.h`

**Function Signature**:
```cpp
namespace dwarfs::metadata::modern {

/**
 * Convert domain::metadata to thrift::modern::Metadata
 *
 * @param domain_meta Domain model metadata
 * @return Thrift metadata ready for serialization
 */
thrift::modern::Metadata domain_to_thrift(
    domain::metadata const& domain_meta);

} // namespace dwarfs::metadata::modern
```

**Implementation Pattern**:
```cpp
thrift::modern::Metadata domain_to_thrift(
    domain::metadata const& dm) {

  thrift::modern::Metadata tm;

  // Convert chunks
  for (const auto& chunk : dm.chunks) {
    thrift::modern::Chunk tc;
    tc.block = chunk.block;
    tc.offset = chunk.offset;
    tc.size = chunk.size;
    tm.chunks.push_back(std::move(tc));
  }

  // Convert directories
  for (const auto& dir : dm.directories) {
    thrift::modern::Directory td;
    // ... convert all fields
    tm.directories.push_back(std::move(td));
  }

  // ... convert remaining fields

  // Handle optional fields
  if (dm.devices) {
    tm.devices = *dm.devices;
  }

  return tm;
}
```

### Task 3.2: Thrift → Domain Converter

**File**: `src/metadata/modern/thrift_to_domain.cpp`

**Header**: `include/dwarfs/metadata/modern/thrift_to_domain.h`

**Function Signature**:
```cpp
namespace dwarfs::metadata::modern {

/**
 * Convert thrift::modern::Metadata to domain::metadata
 *
 * @param thrift_meta Thrift metadata from deserialization
 * @return Domain model metadata
 */
domain::metadata thrift_to_domain(
    thrift::modern::Metadata const& thrift_meta);

} // namespace dwarfs::metadata::modern
```

**Implementation Pattern**:
```cpp
domain::metadata thrift_to_domain(
    thrift::modern::Metadata const& tm) {

  domain::metadata dm;

  // Convert chunks
  for (const auto& tc : tm.chunks) {
    domain::chunk dc;
    dc.block = tc.block;
    dc.offset = tc.offset;
    dc.size = tc.size;
    dm.chunks.push_back(std::move(dc));
  }

  // ... convert remaining fields

  // Handle optional fields
  if (tm.devices) {
    dm.devices = *tm.devices;
  }

  return dm;
}
```

---

## Step 4: Update Build System (20 min)

### Task 4.1: Add Converter Sources to CMake

**Edit**: `cmake/metadata_serialization.cmake`

**Update MODERN_THRIFT_SOURCES**:
```cmake
set(MODERN_THRIFT_SOURCES
  # Generated Thrift types
  ${THRIFT_GEN_DIR}/metadata_modern_types.cpp

  # Converters
  src/metadata/modern/domain_to_thrift.cpp
  src/metadata/modern/thrift_to_domain.cpp
)

# Create library for Modern Thrift components
add_library(dwarfs_metadata_modern_thrift ${MODERN_THRIFT_SOURCES})
add_dependencies(dwarfs_metadata_modern_thrift dwarfs_metadata_modern_thrift_generate)

target_include_directories(dwarfs_metadata_modern_thrift
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${THRIFT_GEN_DIR}>
)

target_link_libraries(dwarfs_metadata_modern_thrift
  PUBLIC
    FBThrift::thriftcpp2
    Folly::folly
)
```

### Task 4.2: Test Build

```bash
# Configure with Modern Thrift enabled
cmake -B build -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DDWARFS_WITH_THRIFT=ON

# Build converters
ninja -C build dwarfs_metadata_modern_thrift
```

**Expected**: Clean compile with no errors

---

## Step 5: Write Unit Tests (30 min)

### Task 5.1: Round-Trip Test

**File**: `test/metadata/modern/converter_test.cpp`

**Test Cases**:
```cpp
TEST(ModernThriftConverter, SimpleMetadata) {
  // Create simple domain metadata
  domain::metadata dm;
  dm.block_size = 131072;
  dm.timestamp_base = 1234567890;
  // ... set basic fields

  // Convert domain → thrift
  auto tm = domain_to_thrift(dm);

  // Convert thrift → domain
  auto dm2 = thrift_to_domain(tm);

  // Verify round-trip
  EXPECT_EQ(dm.block_size, dm2.block_size);
  EXPECT_EQ(dm.timestamp_base, dm2.timestamp_base);
  // ... verify all fields
}

TEST(ModernThriftConverter, ComplexMetadataWithOptionals) {
  // Create metadata with optional fields
  domain::metadata dm;
  dm.devices = std::vector<uint64_t>{1, 2, 3};
  dm.dwarfs_version = "0.17.0";
  // ... set all optional fields

  // Round-trip test
  auto tm = domain_to_thrift(dm);
  auto dm2 = thrift_to_domain(tm);

  // Verify optionals preserved
  ASSERT_TRUE(dm2.devices.has_value());
  EXPECT_EQ(*dm.devices, *dm2.devices);
  // ... verify all optional fields
}
```

---

## Step 6: Update Status (10 min)

Update `doc/SESSION_86_IMPLEMENTATION_STATUS.md`:
- Mark Phase 2 as COMPLETE
- Update progress: 2/6 phases → 33%

---

## Success Criteria

✅ **Schema Created**:
- [ ] `thrift/metadata_modern.thrift` created
- [ ] All domain::metadata fields mapped
- [ ] Optional fields properly marked

✅ **Converters Implemented**:
- [ ] `domain_to_thrift()` working
- [ ] `thrift_to_domain()` working
- [ ] Round-trip tests passing

✅ **Build System**:
- [ ] fbthrift compiler configured
- [ ] C++ types generated
- [ ] Library builds cleanly

✅ **Testing**:
- [ ] Simple metadata test passing
- [ ] Complex metadata test passing
- [ ] Optional fields test passing

---

## Time Budget

- Step 1 (Context): 5 min
- Step 2 (Schema): 40 min
- Step 3 (Converters): 60 min
- Step 4 (Build): 20 min
- Step 5 (Tests): 30 min
- Step 6 (Status): 5 min
- **Total**: 160 minutes (~2.5 hours)

---

## Common Issues & Solutions

### Issue: thrift1 not found

**Solution**: Ensure fbthrift is installed via vcpkg:
```bash
vcpkg install fbthrift --overlay-ports=./vcpkg_ports
```

### Issue: Generated types compile errors

**Solution**: Check IDL syntax:
```bash
thrift1 --gen mstch_cpp2 thrift/metadata_modern.thrift
# Check gen-cpp2/ for errors
```

### Issue: Converter linker errors

**Solution**: Ensure library links to FBThrift and Folly:
```cmake
target_link_libraries(dwarfs_metadata_modern_thrift
  PUBLIC FBThrift::thriftcpp2 Folly::folly
)
```

---

## Next Session

After Session 87, proceed to Session 88: CompactProtocol Serialization

Read: `doc/SESSION_88_CONTINUATION_PROMPT.md` (will be created in Session 87)

---

**Created**: 2026-01-06
**Session**: 87
**Goal**: Define Thrift schema and implement converters
**Next**: Implement CompactProtocol serialization