# Session 97: Clean OOP Architecture - Eliminate Dual Metadata System

**Created**: 2026-01-06
**Goal**: Implement FULLY CLEAN OOP architecture for all 3 metadata formats
**Strategy**: ELIMINATE old backend system, USE ONLY SerializerRegistry

---

## Critical Finding from Session 96

**Problem**: We have TWO parallel metadata architectures:

1. **OLD System** (metadata_factory → backend classes):
   - `thrift_backend::global_metadata`
   - `flatbuffers_backend::global_metadata`
   - Each format has its own backend class
   - Used by reader code

2. **NEW System** (SerializerRegistry → domain model):
   - `SerializerRegistry::deserialize()` → `domain::metadata`
   - All formats work through unified interface
   - Used by writer/serialization code

**Root Cause**: Legacy Thrift was added to NEW system, but reader uses OLD system.

**User Requirement**: **NO BRIDGES. FULLY CLEAN OOP ARCHITECTURE.**

---

## Clean OOP Solution

### Principle: ONE Unified Architecture

**ELIMINATE** the old backend system entirely.
**USE ONLY** the SerializerRegistry architecture for ALL code.

```
┌─────────────────────────────────────────────────────┐
│              SerializerRegistry                     │
│  (Unified interface for ALL formats)                │
└──────────────────┬──────────────────────────────────┘
                   │
         ┌─────────┼─────────┐
         ▼         ▼         ▼
   ┌─────────┬─────────┬─────────┐
   │FlatBuf  │Modern   │Legacy   │
   │Serial   │Thrift   │Thrift   │
   │izer     │Serial   │Serial   │
   │         │izer     │izer     │
   └────┬────┴────┬────┴────┬────┘
        │         │         │
        └─────────┼─────────┘
                  ▼
         domain::metadata
         (ALL code works with this)
```

### Benefits

1. **Single Responsibility**: SerializerRegistry handles ALL format logic
2. **Open/Closed**: Add new formats = implement IMetadataSerializer
3. **No Duplication**: One codepath for all formats
4. **Testable**: Mock SerializerRegistry for tests
5. **MECE**: Format logic ONLY in serializers

---

## Phase 1: Architecture Cleanup (60 min)

### Task 1.1: Delete Old Backend Classes (20 min)

**Files to DELETE**:
- `src/reader/internal/metadata_types_thrift.cpp`
- `src/reader/internal/metadata_types_flatbuffers.cpp`
- `src/reader/internal/thrift_metadata_adapter.cpp`
- `src/reader/internal/flatbuffers_metadata_adapter.cpp`
- `include/dwarfs/reader/internal/metadata_types_thrift.h`
- `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`

**Rationale**: These are the OLD backend classes. They duplicate logic already in serializers.

### Task 1.2: Rewrite metadata_factory to Use SerializerRegistry (40 min)

```cpp
// NEW CLEAN IMPLEMENTATION
namespace dwarfs::reader::internal {

std::unique_ptr<domain::metadata>
metadata_factory::load_metadata(logger& lgr, std::span<uint8_t const> data) {
  auto& registry = serialization::SerializerRegistry::instance();

  // Detect format using SerializerRegistry
  auto format = registry.detect_format(
      std::vector<uint8_t>(data.begin(), data.end()));

  if (!format) {
    DWARFS_THROW(runtime_error, "Unable to detect metadata format");
  }

  // Create serializer for detected format
  auto serializer = registry.create_serializer(*format);

  if (!serializer) {
    DWARFS_THROW(runtime_error,
        fmt::format("Serializer for format {} not available",
                    static_cast<int>(*format)));
  }

  // Deserialize to domain model
  std::vector<uint8_t> data_vec(data.begin(), data.end());
  auto metadata_ptr = serializer->deserialize(data_vec);

  // Cast to domain::metadata
  return std::unique_ptr<domain::metadata>(
      static_cast<domain::metadata*>(metadata_ptr.release()));
}

} // namespace
```

**Key Changes**:
- Returns `domain::metadata` directly (not backend class)
- Uses SerializerRegistry (unified system)
- Eliminates format-specific code paths
- Clean, testable, MECE

---

## Phase 2: Reader Code Refactoring (90 min)

### Task 2.1: Update filesystem_v2 to Use domain::metadata (40 min)

**Current** (uses backend classes):
```cpp
class filesystem_v2 {
  std::unique_ptr<thrift_backend::global_metadata> meta_;  // OLD
};
```

**NEW** (uses domain model):
```cpp
class filesystem_v2 {
  std::unique_ptr<domain::metadata> meta_;  // CLEAN
};
```

**Files to Update**:
- `src/reader/filesystem_v2.cpp` (~200 lines affected)
- All code that accesses `meta_->get_xyz()` → `meta_->xyz`

### Task 2.2: Create Domain Model Access Helpers (30 min)

**Purpose**: Provide clean API to access domain model data

```cpp
namespace dwarfs::reader::domain_helpers {

// Helper functions to access domain::metadata cleanly
inline auto get_inode(domain::metadata const& meta, uint32_t ino) {
  return meta.inodes[ino];
}

inline auto get_chunk(domain::metadata const& meta, uint32_t idx) {
  return meta.chunks[idx];
}

// etc...

} // namespace
```

**Rationale**:
- Encapsulates domain model access
- Easy to optimize later
- Clear separation of concerns

### Task 2.3: Update All Reader Components (20 min)

**Files to Update**:
- `src/reader/internal/inode_reader_v2.cpp`
- `src/reader/internal/metadata_analyzer.cpp`
- Any code using old backend interfaces

---

## Phase 3: Writer Code Verification (30 min)

### Task 3.1: Verify Writer Uses SerializerRegistry (10 min)

**Check**: Ensure `filesystem_writer` uses SerializerRegistry (not old system)

**Expected**: Already using NEW system (SerializerRegistry)

### Task 3.2: Test All 3 Formats Write/Read (20 min)

```bash
# FlatBuffers
./mkdwarfs -i /usr/share/dict -o test.dff
./dwarfsck test.dff

# Modern Thrift (if DWARFS_WITH_THRIFT=ON)
./mkdwarfs -i /usr/share/dict -o test.dtc --metadata-format=modern-thrift
./dwarfsck test.dtc

# Legacy Thrift
./mkdwarfs -i /usr/share/dict -o test.dth --metadata-format=legacy-thrift
./dwarfsck test.dth
```

---

## Phase 4: CMake & Build Cleanup (45 min)

### Task 4.1: Remove Old Backend from CMakeLists (15 min)

**Files to Update**:
- `cmake/libdwarfs.cmake` - Remove old backend sources
- `cmake/metadata_serialization.cmake` - Simplify (no backend targets)

### Task 4.2: Update Conditional Compilation (15 min)

**OLD** (backend-based):
```cmake
$<$<BOOL:${DWARFS_HAVE_THRIFT}>:src/reader/internal/thrift_metadata_adapter.cpp>
```

**NEW** (serializer-based):
```cmake
# All formats always available via SerializerRegistry
# No conditional sources in reader!
```

### Task 4.3: Build & Test (15 min)

```bash
rm -rf build/
cmake -B build -GNinja -DWITH_TESTS=ON
ninja -C build
ctest --test-dir build -j
```

---

## Phase 5: Homebrew Compatibility Validation (60 min)

### Task 5.1: Test Homebrew v0.14.1 Image Reading (20 min)

```bash
# Create image with Homebrew dwarfs v0.14.1
/opt/homebrew/bin/mkdwarfs -i /usr/share/dict -o homebrew.dwarfs

# Read with our NEW clean implementation
./build/dwarfsck homebrew.dwarfs
./build/dwarfsextract -i homebrew.dwarfs -o /tmp/extracted/

# Verify content
diff -r /usr/share/dict /tmp/extracted/
```

### Task 5.2: Test Our Legacy Thrift → Homebrew Reading (20 min)

```bash
# Create with our mkdwarfs (Legacy Thrift)
./build/mkdwarfs -i /usr/share/dict -o ours.dwarfs --metadata-format=legacy-thrift

# Read with Homebrew dwarfs
/opt/homebrew/bin/dwarfsck ours.dwarfs
mkdir /tmp/hb-mount
/opt/homebrew/bin/dwarfs ours.dwarfs /tmp/hb-mount
ls /tmp/hb-mount/
umount /tmp/hb-mount
```

### Task 5.3: Create Formal Compatibility Test (20 min)

**File**: `test/metadata/legacy/homebrew_compatibility_test.cpp`

```cpp
TEST(HomebrewCompatibility, RoundTrip) {
  // 1. Create test data
  // 2. Write with our Legacy Thrift
  // 3. Read with our reader
  // 4. Verify data integrity
  // 5. Ensure format detected as legacy_thrift
}
```

---

## Phase 6: Documentation & Release (45 min)

### Task 6.1: Update Architecture Documentation (20 min)

**File**: `.kilocode/rules/memory-bank/architecture.md`

**Add Section**: "Metadata Serialization Clean Architecture"

```markdown
## Metadata Serialization (v0.17.0+)

### Unified Architecture

ALL metadata serialization uses SerializerRegistry:

1. **SerializerRegistry**: Singleton registry of all serializers
2. **IMetadataSerializer**: Interface for all formats
3. **domain::metadata**: Unified data model
4. **Format Detection**: Via magic bytes + priority

### Three Formats

1. **FlatBuffers** - Modern default
   - Magic: "DFBF" at offset 4
   - Priority: 120
   - Always available

2. **Modern Thrift** - Optional
   - Magic: {0x82, 0x21} at offset 0
   - Priority: 100
   - Requires DWARFS_WITH_THRIFT=ON

3. **Legacy Thrift** - Homebrew compatibility
   - Magic: None (fallback)
   - Priority: 50
   - Always available
```

### Task 6.2: Update README (15 min)

**Section**: "Metadata Formats"

Document all 3 formats and their use cases.

### Task 6.3: Create Release Notes (10 min)

**File**: `RELEASE_NOTES_v0.17.0.md`

---

## Success Criteria

- [ ] OLD backend system completely removed
- [ ] ALL code uses SerializerRegistry
- [ ] All 3 formats work end-to-end
- [ ] Homebrew v0.14.1 compatibility verified
- [ ] All tests pass
- [ ] Clean, MECE, OOP architecture
- [ ] Zero code duplication
- [ ] Documentation complete

---

## Timeline

**Total Estimated Time**: 5 hours

**Phase 1**: Architecture Cleanup - 60 min
**Phase 2**: Reader Refactoring - 90 min
**Phase 3**: Writer Verification - 30 min
**Phase 4**: Build Cleanup - 45 min
**Phase 5**: Homebrew Validation - 60 min
**Phase 6**: Documentation - 45 min

---

## Next Session Start

Read this plan, then begin Phase 1, Task 1.1.