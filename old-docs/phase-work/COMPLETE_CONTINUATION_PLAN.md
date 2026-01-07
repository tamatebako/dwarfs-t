# Complete Multi-Format Metadata Implementation Plan
**Created**: 2025-11-22 15:03 HKT | **Branch**: `feature/multi-format-serialization-fuse`

## Overview

This document provides a complete roadmap for finishing the multi-format metadata serialization refactoring. This work implements Option C (Complete Backend Separation) as documented in `doc/OPTION_C_COMPLETE_SEPARATION_PLAN.md`.

## Architecture Principles

All work follows these principles:
1. **Object-Oriented Design**: Pure OOP, no procedural code
2. **MECE**: Mutually Exclusive, Collectively Exhaustive organization
3. **Separation of Concerns**: Each component has one responsibility
4. **Open/Closed Principle**: Extensible without modification
5. **No Code Guards**: Use polymorphism and architecture over `#ifdef`
6. **Comprehensive Testing**: Each class has matching test file

## Progress Overview

```
Phase 1 (FlatBuffers Backend):  ███████████████████████░  98% (15 min remaining)
Phase 2 (Thrift Backend):       ░░░░░░░░░░░░░░░░░░░░░░   0% (2-3 hours)
Phase 3 (Dual-Format):          ░░░░░░░░░░░░░░░░░░░░░░   0% (2 hours)
Phase 4 (Cleanup):              ░░░░░░░░░░░░░░░░░░░░░░   0% (1 hour)
Phase 5 (Testing & Docs):       ░░░░░░░░░░░░░░░░░░░░░░   0% (3 hours)
═══════════════════════════════════════════════════════════════
Overall Progress:                ███░░░░░░░░░░░░░░░░░░░  15% (8-9 hours remaining)
```

---

## PHASE 1: FlatBuffers Backend (CURRENT - 98% Complete)

### Remaining Work

#### 1.1 Fix Constructor Signature (15 minutes)

**File**: `src/reader/internal/metadata_v2_flatbuffers.cpp:2166`

**Current**:
```cpp
metadata_(logger& lgr, std::span<uint8_t const> schema, ...)
```

**Needed**:
```cpp
metadata_(LoggerPolicy const&, logger& lgr, std::span<uint8_t const> schema, ...)
```

**Command**:
```bash
sed -i '' 's/metadata_(logger& lgr,/metadata_(LoggerPolicy const\&, logger\& lgr,/' \
  src/reader/internal/metadata_v2_flatbuffers.cpp
```

#### 1.2 Build & Test (10 minutes)

```bash
cd build-fb-test
ninja mkdwarfs dwarfsck dwarfsextract

# Functional test
./mkdwarfs -i testdata -o test.dff --format=flatbuffers -l1
./dwarfsck test.dff --print-header
./dwarfsextract -i test.dff -o test-out/
diff -r testdata/ test-out/
```

#### 1.3 Write Unit Tests for FlatBuffers Backend (30 minutes)

**New File**: `test/metadata_types_flatbuffers_test.cpp`

Tests needed:
- `global_metadata` construction and accessors
- `inode_view_impl` all getter methods
- `dir_entry_view_impl` name resolution
- `chunk_view` and `chunk_range` iteration
- Edge cases: empty metadata, malformed data

**Template**:
```cpp
#include <gtest/gtest.h>
#include <dwarfs/reader/internal/metadata_types_flatbuffers.h>

namespace dwarfs::reader::internal::flatbuffers_backend {

TEST(FlatBuffersBackendTest, GlobalMetadataBasic) {
  // Test construction, consistency checking
}

TEST(FlatBuffersBackendTest, InodeViewImpl) {
  // Test all accessors
}

// ... more tests
}
```

---

## PHASE 2: Thrift Backend Isolation (2-3 hours)

### Objective
Create isolated Thrift backend with same structure as FlatBuffers, eliminating all mixing.

### 2.1 Create Thrift Backend Headers (45 minutes)

**New File**: `include/dwarfs/reader/internal/metadata_types_thrift.h`

```cpp
namespace dwarfs::reader::internal::thrift_backend {

class global_metadata {
  // Same API as flatbuffers_backend::global_metadata
  // Implementation uses Apache Thrift Frozen2
};

class inode_view_impl {
  // Same API as flatbuffers_backend::inode_view_impl
};

class dir_entry_view_impl {
  // Same API as flatbuffers_backend::dir_entry_view_impl  
};

class chunk_view {
  // Same API as flatbuffers_backend::chunk_view
};

class chunk_range {
  // Same API as flatbuffers_backend::chunk_range
};

} // namespace
```

### 2.2 Implement Thrift Backend (1 hour)

**New File**: `src/reader/internal/metadata_types_thrift.cpp`

Move all Thrift-specific code from existing files into this isolated implementation.

### 2.3 Create metadata_v2_thrift.cpp (45 minutes)

**New File**: `src/reader/internal/metadata_v2_thrift.cpp`

Copy structure from `metadata_v2_flatbuffers.cpp`, replace:
- `namespace fb = flatbuffers_backend;` → `namespace tb = thrift_backend;`
- All `fb::` prefixes → `tb::`
- FlatBuffers deserialization → Thrift Frozen2 deserialization

### 2.4 Update CMake (15 minutes)

**File**: `cmake/libdwarfs.cmake`

```cmake
if(DWARFS_HAVE_THRIFT)
  target_sources(dwarfs_reader PRIVATE
    src/reader/internal/metadata_types_thrift.cpp
    src/reader/internal/metadata_v2_thrift.cpp
  )
endif()
```

### 2.5 Tests (30 minutes)

**New File**: `test/metadata_types_thrift_test.cpp`

Same test structure as FlatBuffers tests, ensuring API compatibility.

---

## PHASE 3: Dual-Format Integration (2 hours)

### Objective
Factory pattern to select backend based on detected format, eliminating hardcoded format assumptions.

### 3.1 Create Metadata Factory (1 hour)

**New File**: `include/dwarfs/reader/internal/metadata_v2_factory.h`

```cpp
namespace dwarfs::reader::internal {

class metadata_v2_factory {
public:
  static std::unique_ptr<metadata_v2::impl> create(
      logger& lgr,
      std::span<uint8_t const> schema,
      std::span<uint8_t const> data,
      metadata_options const& options,
      int inode_offset,
      bool force_consistency_check,
      std::shared_ptr<performance_monitor const> const& perfmon);

private:
  static SerializationFormat detect_format(std::span<uint8_t const> data);
};

}
```

**Implementation** in `src/reader/internal/metadata_v2_factory.cpp`:

```cpp
std::unique_ptr<metadata_v2::impl> metadata_v2_factory::create(...) {
  auto format = detect_format(data);
  
  switch (format) {
    case SerializationFormat::FLATBUFFERS:
#ifdef DWARFS_HAVE_FLATBUFFERS
      return create_flatbuffers_backend(lgr, schema, data, ...);
#else
      throw std::runtime_error("FlatBuffers support not compiled in");
#endif

    case SerializationFormat::THRIFT:
#ifdef DWARFS_HAVE_THRIFT
      return create_thrift_backend(lgr, schema, data, ...);
#else
      throw std::runtime_error("Thrift support not compiled in");
#endif

    default:
      throw std::runtime_error("Unsupported metadata format");
  }
}
```

### 3.2 Update metadata_v2 Constructor (15 minutes)

**File**: `src/reader/internal/metadata_v2.cpp` (to be created)

```cpp
metadata_v2::metadata_v2(
    logger& lgr, std::span<uint8_t const> schema, std::span<uint8_t const> data,
    metadata_options const& options, int inode_offset,
    bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon)
    : impl_(metadata_v2_factory::create(lgr, schema, data, options, 
            inode_offset, force_consistency_check, perfmon)) {}
```

### 3.3 Integration Tests (45 minutes)

**New File**: `test/metadata_v2_factory_test.cpp`

```cpp
TEST(MetadataV2FactoryTest, DetectFlatBuffers) {
  // Create FlatBuffers data, verify factory creates correct backend
}

TEST(MetadataV2FactoryTest, DetectThrift) {
  // Create Thrift data, verify factory creates correct backend
}

TEST(MetadataV2FactoryTest, CrossFormatCompatibility) {
  // Same filesystem, different formats, identical results
}
```

---

## PHASE 4: Cleanup & Refactoring (1 hour)

### 4.1 Remove Conversion Code (30 minutes)

Files to clean:
- Remove any temporary Thrift ↔ FlatBuffers conversion code
- Remove `metadata_v2_data::unpack_metadata()` if not needed
- Clean up any `#ifdef` guards that are now unnecessary

### 4.2 Code Review Checklist (30 minutes)

□ All backends use namespace isolation
□ No mixing of backend types
□ No `#ifdef` in implementation files (only in headers/CMake)
□ All tests passing
□ No compiler warnings
□ No code duplication between backends
□ Factory pattern properly implemented
□ Error handling for unsupported formats

---

## PHASE 5: Testing & Documentation (3 hours)

### 5.1 Comprehensive Test Suite (1.5 hours)

#### Unit Tests (existing + new)
- `test/metadata_types_flatbuffers_test.cpp` (NEW)
- `test/metadata_types_thrift_test.cpp` (NEW)
- `test/metadata_v2_factory_test.cpp` (NEW)
- Update existing `test/metadata_test.cpp`

#### Integration Tests
- `test/dual_format_integration_test.cpp` (NEW)

Test matrix:
```
           | FlatBuffers | Thrift | Both
-----------|-------------|--------|------
Create FS  |     ✓       |   ✓    |  ✓
Read FS    |     ✓       |   ✓    |  ✓
Extract    |     ✓       |   ✓    |  ✓
Check      |     ✓       |   ✓    |  ✓
Rewrite    |     ✓→✓     |  ✓→✓   | ✓→✓
```

#### Performance Tests
- `test/metadata_format_benchmark.cpp`
- Compare read/write performance
- Memory usage comparison
- Cache behavior

### 5.2 Update Official Documentation (1 hour)

#### Update README.adoc

**Section to Add**: "Metadata Serialization Formats"

```adoc
== Metadata Serialization Formats

DwarFS supports multiple metadata serialization formats:

=== FlatBuffers (Default)

The modern default format, providing excellent portability:

* Memory-mappable zero-copy access
* Platform-independent (all architectures, all OSes)
* Header-only library dependency
* Forward and backward compatible
* Slightly larger than Thrift (~5-10%)

To create a FlatBuffers filesystem:

[source,bash]
----
mkdwarfs -i input/ -o output.dff --format=flatbuffers
----

=== Apache Thrift (Legacy)

The legacy format, optional for backward compatibility:

* Smallest format (Frozen2 bit-packed)
* Memory-mappable zero-copy access  
* Requires Apache Thrift + Facebook Folly
* May not build on all platforms
* Default in versions < 0.16.0

To read existing Thrift filesystems:

[source,bash]
----
dwarfs old-image.dwarfs /mnt/point
----

=== Format Detection

DwarFS automatically detects the format when mounting or extracting:

[source,bash]
----
dwarfsck image.dwarfs --json | jq '.metadata_format'
----

=== Build Configuration

Control format support at build time:

[source,bash]
----
# FlatBuffers only (minimal dependencies)
cmake -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF ..

# Both formats (full compatibility)
cmake -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON ..
----
```

#### Create doc/metadata-formats.md

Detailed technical documentation:
- Format specifications
- Migration guide (Thrift → FlatBuffers)
- Performance comparison
- Build system integration

#### Update doc/dwarfs-format.md

Add section on metadata format versioning and detection.

### 5.3 Move Outdated Documentation (15 minutes)

```bash
mkdir -p old-docs/phase-work
mv doc/PHASE_*_2025-11-22.md old-docs/phase-work/
mv doc/OPTION_C_*.md old-docs/phase-work/
mv doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md old-docs/
mv doc/THRIFT_OPTIONAL_*.md old-docs/

# Keep in doc/:
# - README.adoc (updated)
# - metadata-formats.md (new)
# - dwarfs-format.md (updated)
# - mkdwarfs.md, dwarfs.md, etc. (existing)
```

### 5.4 Update CMake Documentation (15 minutes)

**File**: `cmake/README.md` (create if needed)

Document all metadata-related CMake options:
- `DWARFS_WITH_FLATBUFFERS`
- `DWARFS_WITH_THRIFT`
- How format detection works
- Tebako-specific behavior

---

## Testing Strategy

### Test Organization

```
test/
├── metadata_types_flatbuffers_test.cpp  # FlatBuffers backend unit tests
├── metadata_types_thrift_test.cpp       # Thrift backend unit tests
├── metadata_v2_factory_test.cpp         # Factory pattern tests
├── dual_format_integration_test.cpp     # Cross-format tests
├── metadata_format_benchmark.cpp        # Performance comparison
└── fixtures/
    ├── fb_metadata_sample.bin           # Test FlatBuffers data
    └── thrift_metadata_sample.bin       # Test Thrift data
```

### Test Coverage Requirements

Each backend MUST have:
- Construct/destruct tests
- All accessor method tests
- Edge case tests (empty, malformed, corrupt data)
- Performance benchmarks
- Memory leak tests (valgrind/ASAN)

Minimum coverage: 85% for new code

### CI/CD Updates

**.github/workflows/build.yml** additions:

```yaml
- name: Test FlatBuffers-only build
  run: |
    cmake -B build-fb -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF ..
    cmake --build build-fb
    ctest --test-dir build-fb --output-on-failure

- name: Test Thrift-only build  
  run: |
    cmake -B build-thrift -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON ..
    cmake --build build-thrift
    ctest --test-dir build-thrift --output-on-failure

- name: Test dual-format build
  run: |
    cmake -B build-dual -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON ..
    cmake --build build-dual
    ctest --test-dir build-dual --output-on-failure
```

---

## Architecture Documentation

### Class Hierarchy

```
metadata_v2 (public API)
    │
    ├── impl (pure virtual interface)
    │   │
    │   ├── flatbuffers_backend::metadata_ (template<LoggerPolicy>)
    │   │   └── uses flatbuffers_backend::metadata_v2_data
    │   │       └── uses flatbuffers_backend::global_metadata
    │   │           └── uses flatbuffers_backend::inode_view_impl
    │   │               └── uses flatbuffers_backend::chunk_range
    │   │
    │   └── thrift_backend::metadata_ (template<LoggerPolicy>)
    │       └── uses thrift_backend::metadata_v2_data
    │           └── uses thrift_backend::global_metadata
    │               └── uses thrift_backend::inode_view_impl
    │                   └── uses thrift_backend::chunk_range
    │
    └── metadata_v2_factory (selects backend at runtime)
```

### Namespace Organization

```
dwarfs::reader::internal::
    ├── flatbuffers_backend::  # All FlatBuffers types
    ├── thrift_backend::       # All Thrift types
    └── (root namespace)       # Factory, public API
```

### Design Patterns Used

1. **Strategy Pattern**: Backend selection
2. **Factory Pattern**: Backend creation
3. **Template Method**: Logger policy injection
4. **Adapter Pattern**: Unified API over different formats

---

## Definition of Done

### Phase 1 Complete When:
- [x] All FlatBuffers backend files compile
- [ ] Constructor signature fixed
- [ ] All tools build successfully
- [ ] Functional test passes (create/extract/verify)
- [ ] Unit tests written and passing

### Phase 2 Complete When:
- [ ] Thrift backend isolated in own namespace
- [ ] No mixing of Thrift/FlatBuffers types
- [ ] Thrift backend compiles independently
- [ ] Unit tests passing

### Phase 3 Complete When:
- [ ] Factory pattern implemented
- [ ] Format auto-detection works
- [ ] Both formats can coexist in single build
- [ ] Integration tests passing

### Phase 4 Complete When:
- [ ] All conversion code removed
- [ ] No compiler warnings
- [ ] Code review checklist complete
- [ ] CI/CD passing

### Phase 5 Complete When:
- [ ] All unit tests passing (>85% coverage)
- [ ] All integration tests passing
- [ ] Performance benchmarks run
- [ ] README.adoc updated
- [ ] Technical docs written
- [ ] Old docs moved to old-docs/

---

## Risk Mitigation

### Known Risks

1. **Thrift Build Complexity**: May not build on all platforms
   - Mitigation: Comprehensive platform testing in CI
   
2. **API Incompatibility**: Backends might drift
   - Mitigation: Shared interface, integration tests

3. **Performance Regression**: New abstraction overhead
   - Mitigation: Benchmark against baseline

4. **Breaking Changes**: Existing code might break
   - Mitigation: Comprehensive test suite, feature flags

### Rollback Plan

If critical issues found:
1. Revert to commit before Phase 1
2. Keep feature branch for future work
3. Document issues found

---

## Success Criteria

### Technical
- Zero compile warnings
- All tests passing (>85% coverage)
- Both formats functional
- Performance within 5% of baseline
- Memory usage unchanged

### Documentation
- All new features documented
- Migration guide available
- Build instructions clear
- Outdated docs archived

### Process
- Code reviewed
- CI/CD passing
- Branch ready to merge
- No regressions

---

## Estimated Timeline

| Phase | Description | Time | Cost (Est) |
|-------|-------------|------|------------|
| 1 | FlatBuffers Backend | 0.5h | $5 |
| 2 | Thrift Backend | 3h | $20 |
| 3 | Dual-Format Integration | 2h | $15 |
| 4 | Cleanup | 1h | $8 |
| 5 | Testing & Docs | 3h | $20 |
| **Total** | **All Phases** | **9.5h** | **$68** |

---

## Next Steps

**Immediate** (Phase 1 finish):
1. Fix constructor signature
2. Build & test FlatBuffers-only build
3. Write unit tests

**Short-term** (Phase 2-3):
1. Isolate Thrift backend
2. Implement factory pattern
3. Integration testing

**Medium-term** (Phase 4-5):
1. Code cleanup
2. Comprehensive testing
3. Documentation updates

---

## Contact & Support

For questions or issues with this plan:
- Branch: `feature/multi-format-serialization-fuse`
- Reference: `doc/OPTION_C_COMPLETE_SEPARATION_PLAN.md`
- Upstream: https://github.com/tamatebako/dwarfs

---

**Last Updated**: 2025-11-22 15:03 HKT
