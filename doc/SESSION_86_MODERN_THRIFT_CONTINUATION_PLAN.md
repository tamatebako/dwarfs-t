# Session 86+: Modern Thrift Implementation - Continuation Plan

**Created**: 2026-01-06
**Prerequisite**: Session 85 (Documentation & Cleanup Complete)
**Goal**: Implement Modern Thrift metadata serialization using fbthrift v2025.12.29.00
**Estimated Time**: 6-8 sessions (~12-16 hours)

---

## Current Status (Session 85 Output)

✅ **Legacy Thrift (Frozen2)**: COMPLETE - All 4 tests passing
✅ **FlatBuffers**: COMPLETE - Production-ready
✅ **Documentation**: COMPLETE - Comprehensive guides
⏳ **Modern Thrift**: NOT STARTED - Next phase

### Three-Format Vision (v0.17.0)

| Format | Status | Dependencies | Priority |
|--------|--------|--------------|----------|
| **Legacy Thrift** | ✅ Complete | None | 50 |
| **FlatBuffers** | ✅ Complete | Header-only | 120 |
| **Modern Thrift** | ⏳ Next | Folly + fbthrift | 100 |

---

## Phase 1: Architecture Design (Session 86) - 2 hours

### Objective
Design the Modern Thrift serialization architecture using the Strategy Pattern established in v0.16.0.

### Tasks

#### 1.1: Review Existing Architecture
- [ ] Read Strategy Pattern implementation in [`src/reader/internal/backend_adapter.cpp`](../src/reader/internal/backend_adapter.cpp)
- [ ] Review FlatBuffers serializer: [`src/metadata/serialization/flatbuffers_serializer.cpp`](../src/metadata/serialization/flatbuffers_serializer.cpp)
- [ ] Review Legacy Thrift: [`src/metadata/legacy/frozen2_serializer.cpp`](../src/metadata/legacy/frozen2_serializer.cpp)
- [ ] Understand domain model: [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h)

#### 1.2: Design Modern Thrift Adapter
- [ ] Create header: `include/dwarfs/metadata/modern/thrift_compact_serializer.h`
- [ ] Design class hierarchy:
  ```cpp
  class ModernThriftSerializer {
    static std::pair<Schema, std::vector<uint8_t>>
    serialize(domain::metadata const& meta);

    static domain::metadata
    deserialize(std::vector<uint8_t> const& data);
  };
  ```
- [ ] Define interfaces for fbthrift integration

#### 1.3: Define Build Configuration
- [ ] Add CMake option: `DWARFS_WITH_MODERN_THRIFT` (default: OFF)
- [ ] Update `cmake/metadata_serialization.cmake`
- [ ] Add conditional compilation guards

#### 1.4: Document Architecture
- [ ] Create `doc/MODERN_THRIFT_ARCHITECTURE.md`
- [ ] Diagram: Domain Model ↔ Thrift Types conversion
- [ ] Explain CompactProtocol usage

**Deliverables**:
- Architecture document
- Header files with interface definitions
- CMake configuration updated

---

## Phase 2: Thrift Schema Definition (Session 87) - 2 hours

### Objective
Define Thrift schema that maps to the domain model.

### Tasks

#### 2.1: Create Thrift IDL
- [ ] Create `thrift/metadata_modern.thrift`
- [ ] Define structs matching domain model:
  ```thrift
  struct Metadata {
    1: i64 timestamp_base
    2: i64 total_fs_size
    3: list<Inode> inodes
    4: list<Chunk> chunks
    5: list<Directory> directories
    // ... etc
  }
  ```
- [ ] Use Thrift annotations for optimization

#### 2.2: Generate C++ Code
- [ ] Configure fbthrift compiler in CMake
- [ ] Generate C++ types from IDL
- [ ] Verify generated code compiles

#### 2.3: Write Type Converters
- [ ] Create `src/metadata/modern/domain_to_thrift.cpp`
- [ ] Create `src/metadata/modern/thrift_to_domain.cpp`
- [ ] Implement bidirectional conversion functions

**Deliverables**:
- Thrift IDL file
- Generated C++ types
- Conversion utility functions

---

## Phase 3: CompactProtocol Serialization (Session 88) - 3 hours

### Objective
Implement serialization using `apache::thrift::CompactSerializer`.

### Tasks

#### 3.1: Implement Writer
- [ ] Create `src/metadata/modern/thrift_compact_serializer.cpp`
- [ ] Implement serialize():
  ```cpp
  auto serialize(domain::metadata const& meta) {
    // 1. Convert domain → Thrift types
    auto thrift_meta = domain_to_thrift(meta);

    // 2. Serialize with CompactProtocol
    auto bytes = apache::thrift::CompactSerializer::serialize(thrift_meta);

    // 3. Add magic bytes {0x82, 0x21}
    return prepend_magic(bytes);
  }
  ```
- [ ] Handle magic bytes prepending
- [ ] Implement error handling

#### 3.2: Implement Reader
- [ ] Implement deserialize():
  ```cpp
  auto deserialize(vector<uint8_t> const& data) {
    // 1. Verify magic bytes
    verify_magic(data);

    // 2. Deserialize with CompactProtocol
    auto thrift_meta = apache::thrift::CompactSerializer::deserialize(data);

    // 3. Convert Thrift types → domain
    return thrift_to_domain(thrift_meta);
  }
  ```
- [ ] Implement magic byte verification
- [ ] Implement error handling

#### 3.3: Integration with Serializer Registry
- [ ] Register Modern Thrift in `serializer_registry.cpp`
- [ ] Set priority: 100 (between Legacy 50 and FlatBuffers 120)
- [ ] Add format detection via magic bytes

**Deliverables**:
- Complete serializer implementation
- Registry integration
- Format detection working

---

## Phase 4: Testing (Session 89) - 3 hours

### Objective
Comprehensive testing of Modern Thrift serialization.

### Tasks

#### 4.1: Unit Tests
- [ ] Create `test/metadata/modern/thrift_compact_test.cpp`
- [ ] Test cases:
  ```cpp
  TEST(ModernThriftTest, SimpleMetadata)
  TEST(ModernThriftTest, ComplexMetadata)
  TEST(ModernThriftTest, RoundTrip)
  TEST(ModernThriftTest, MagicBytes)
  TEST(ModernThriftTest, ErrorHandling)
  ```
- [ ] Verify byte-for-byte consistency

#### 4.2: Integration Tests
- [ ] Test mkdwarfs with `--format=modern-thrift`
- [ ] Test dwarfsck on Modern Thrift images
- [ ] Test dwarfsextract from Modern Thrift images
- [ ] Test FUSE mount of Modern Thrift images

#### 4.3: Cross-Format Tests
- [ ] Test conversion: FlatBuffers → Modern Thrift
- [ ] Test conversion: Legacy Thrift → Modern Thrift
- [ ] Test conversion: Modern Thrift → FlatBuffers
- [ ] Verify metadata preservation

#### 4.4: Performance Benchmarks
- [ ] Benchmark serialization speed
- [ ] Benchmark deserialization speed
- [ ] Benchmark size vs FlatBuffers
- [ ] Benchmark size vs Legacy Thrift

**Deliverables**:
- Comprehensive test suite (10+ tests)
- Integration tests passing
- Performance benchmark results

---

## Phase 5: Build System Integration (Session 90) - 2 hours

### Objective
Integrate Modern Thrift into all build configurations.

### Tasks

#### 5.1: CMake Configuration
- [ ] Update `cmake/metadata_serialization.cmake`
- [ ] Add vcpkg dependencies:
  ```cmake
  if(DWARFS_WITH_MODERN_THRIFT)
    find_package(folly CONFIG REQUIRED)
    find_package(fbthrift CONFIG REQUIRED)
    find_package(jemalloc CONFIG REQUIRED)
  endif()
  ```
- [ ] Configure conditional compilation

#### 5.2: Build Configurations
- [ ] Test: FlatBuffers-only (Modern Thrift OFF)
- [ ] Test: Modern Thrift-only (FlatBuffers OFF)
- [ ] Test: Both formats (FlatBuffers ON, Modern Thrift ON)
- [ ] Test: All three formats (Legacy + FlatBuffers + Modern Thrift)

#### 5.3: CI/CD Integration
- [ ] Update `.github/workflows/build.yml`
- [ ] Add Modern Thrift build matrix:
  - Ubuntu: modern-thrift-only, both-formats
  - macOS: both-formats
  - Windows: Skip (fbthrift difficult)
- [ ] Verify all CI builds pass

**Deliverables**:
- CMake configuration complete
- All build configurations tested
- CI/CD passing

---

## Phase 6: Documentation & Release (Session 91) - 2 hours

### Objective
Complete documentation and prepare for v0.17.0 release.

### Tasks

#### 6.1: Update Official Docs
- [ ] Update `README.md`:
  - Modern Thrift section complete
  - Three-format comparison table
  - Build instructions for all configs
- [ ] Update `doc/metadata-formats.md`:
  - Modern Thrift section complete
  - Performance benchmarks
  - Migration guide

#### 6.2: Create Release Notes
- [ ] Create `doc/RELEASE_NOTES_v0.17.0.md`
- [ ] Highlight three-format support
- [ ] Migration guide from v0.16.0
- [ ] Breaking changes (if any)

#### 6.3: Update Memory Bank
- [ ] Update `.kilocode/rules/memory-bank/context.md`
- [ ] Update `.kilocode/rules/memory-bank/architecture.md`
- [ ] Update `.kilocode/rules/memory-bank/tech.md`

#### 6.4: Archive Session Docs
- [ ] Move Sessions 84-91 to `doc/old-docs/v0.17.0-sessions/`
- [ ] Keep only latest status docs in `doc/`

**Deliverables**:
- Complete documentation
- Release notes
- Clean doc structure

---

## Success Criteria

### Functional Requirements
- [ ] Modern Thrift serializer working
- [ ] All three formats coexist peacefully
- [ ] Format auto-detection working
- [ ] Cross-format conversion working

### Performance Requirements
- [ ] Modern Thrift size: ~100% (smallest)
- [ ] Serialization speed: Acceptable (medium)
- [ ] Deserialization speed: Fast (Frozen2)

### Quality Requirements
- [ ] 100% test coverage for Modern Thrift
- [ ] All CI builds passing
- [ ] Documentation complete
- [ ] No regressions in existing formats

### Build Requirements
- [ ] FlatBuffers-only: Works
- [ ] Modern Thrift-only: Works
- [ ] Both formats: Works
- [ ] All three formats: Works

---

## Risk Mitigation

### Risk 1: fbthrift Complexity
**Impact**: High
**Mitigation**:
- Use vcpkg overlay ports (already working)
- Test on Linux first (easiest platform)
- Skip Windows builds if needed

### Risk 2: Size Overhead
**Impact**: Medium
**Mitigation**:
- Use CompactProtocol (not BinaryProtocol)
- Apply Thrift annotations for optimization
- Benchmark early, optimize as needed

### Risk 3: Build Time
**Impact**: Medium
**Mitigation**:
- Modern Thrift is optional (default: OFF)
- Users can build without it
- vcpkg caches dependencies

---

## Timeline Estimate

| Phase | Sessions | Time | Cumulative |
|-------|----------|------|------------|
| 1. Architecture | 1 | 2h | 2h |
| 2. Thrift Schema | 1 | 2h | 4h |
| 3. Serialization | 1 | 3h | 7h |
| 4. Testing | 1 | 3h | 10h |
| 5. Build System | 1 | 2h | 12h |
| 6. Documentation | 1 | 2h | 14h |

**Total**: 6 sessions, ~14 hours for complete Modern Thrift implementation

---

## Dependencies

### External
- fbthrift v2025.12.29.00 (via vcpkg)
- Folly v2025.12.29.00 (via vcpkg)
- jemalloc (custom port)

### Internal
- Domain model stable
- Strategy Pattern established
- Serializer registry working

---

## Next Session Start

Read [`doc/SESSION_86_CONTINUATION_PROMPT.md`](SESSION_86_CONTINUATION_PROMPT.md) to begin Phase 1.

---

**Created**: 2026-01-06
**Status**: Ready to start
**Goal**: Complete three-format system for v0.17.0