# Session 33: Reader Layer Architecture Fix

**Date**: 2025-12-23
**Prerequisite**: Session 32 complete (writer layer fixed)
**Estimated Time**: 2-3 hours

## Overview

Session 32 successfully fixed the writer layer but discovered a **critical reader layer architecture issue** that blocks both-formats and thrift-only builds:

**Problem**: [`common_metadata_operations.cpp:1134`](../src/reader/internal/common_metadata_operations.cpp:1134) constructs `chunk_range` with domain model, but Thrift backend expects Thrift frozen metadata.

**Impact**:
- ✅ FlatBuffers-only: Works (domain model compatible)
- ❌ Thrift-only: Fails (architecture mismatch)
- ❌ Both-formats: Fails (same mismatch)

## Root Cause Analysis

### The Architecture Mismatch

**Current Code** (`common_metadata_operations.cpp:1134`):
```cpp
chunk_range common_metadata_operations::get_chunks(int inode, std::error_code& ec) const {
  // ... calculations ...

  // Line 1134: PROBLEM - uses domain_meta_ for both backends
  return chunk_range{domain_meta_, begin, end};
}
```

**Thrift Backend Signature** (`metadata_types_thrift.h:491`):
```cpp
// Expects Thrift frozen metadata, NOT domain model
chunk_range(Meta const& meta, uint32_t begin, uint32_t end)
```

**FlatBuffers Backend**: Accepts domain model directly ✅

### Why This Matters

The `common_metadata_operations` class was designed to use domain model (`metadata::domain::metadata`) but each backend's types (`chunk_range`, `inode_view`, etc.) have different construction requirements:

- **FlatBuffers**: Domain-native (works)
- **Thrift**: Expects legacy frozen types (broken)

## Solution: Adapter Pattern in Common Operations

### Strategy

Apply **Adapter Pattern** to bridge domain model → backend-specific types:

```
common_metadata_operations (domain model)
         │
         ▼
    Backend Adapters
         │
    ┌────┴────┐
    ▼         ▼
FlatBuffers  Thrift
 Backend    Backend
  (direct) (convert)
```

## Phase 1: Fix chunk_range Construction (1.5 hours)

### 1.1: Create Adapter Helper (30 min)

**New File**: `src/reader/internal/backend_adapter.h`

```cpp
#pragma once

#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/reader/internal/metadata_types.h"

namespace dwarfs::reader::internal {

// Adapter to create chunk_range from domain model
// Handles both FlatBuffers and Thrift backends
class backend_adapter {
public:
  static chunk_range make_chunk_range(
      metadata::domain::metadata const& domain_meta,
      uint32_t begin,
      uint32_t end);

  static inode_view make_inode_view(
      metadata::domain::metadata const& domain_meta,
      uint32_t inode_index,
      int inode_num);
};

} // namespace
```

**Implementation**: `src/reader/internal/backend_adapter.cpp`

```cpp
#include "backend_adapter.h"

#ifdef DWARFS_HAVE_FLATBUFFERS
#include "flatbuffers_metadata_adapter.h"
#endif

#ifdef DWARFS_HAVE_THRIFT
#include "thrift_metadata_adapter.h"
#endif

namespace dwarfs::reader::internal {

chunk_range backend_adapter::make_chunk_range(
    metadata::domain::metadata const& domain_meta,
    uint32_t begin,
    uint32_t end) {

#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Both-formats: Use FlatBuffers (domain-native)
  auto range_impl = std::make_shared<domain_chunk_range_impl>(
      domain_meta, begin, end);
  return chunk_range{std::static_pointer_cast<chunk_range_interface const>(range_impl)};

#elif defined(DWARFS_HAVE_FLATBUFFERS)
  // FlatBuffers-only: Direct construction
  return chunk_range{domain_meta, begin, end};

#elif defined(DWARFS_HAVE_THRIFT)
  // Thrift-only: Convert domain → Thrift
  auto thrift_meta = convert_to_thrift_frozen(domain_meta);
  return chunk_range{thrift_meta, begin, end};

#else
  #error "At least one metadata format must be enabled"
#endif
}

} // namespace
```

### 1.2: Update common_metadata_operations (30 min)

**File**: `src/reader/internal/common_metadata_operations.cpp`

**Changes**:
```cpp
#include "backend_adapter.h"

// Line 1134: Replace direct construction
chunk_range common_metadata_operations::get_chunks(int inode, std::error_code& ec) const {
  // ... existing logic ...

  ec.clear();

  // OLD: return chunk_range{domain_meta_, begin, end};
  // NEW: Use adapter
  return backend_adapter::make_chunk_range(domain_meta_, begin, end);
}
```

### 1.3: Build and Test (30 min)

```bash
# Test FlatBuffers-only (should still work)
cmake -B build-fb-verify -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
cmake --build build-fb-verify --target mkdwarfs dwarfsck -j8
./build-fb-verify/mkdwarfs -i example/pg11339-h -o /tmp/test-fb.dff
./build-fb-verify/dwarfsck /tmp/test-fb.dff

# Test Thrift-only (should now work)
cmake -B build-thrift-verify -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
cmake --build build-thrift-verify --target mkdwarfs dwarfsck -j8
./build-thrift-verify/mkdwarfs -i example/pg11339-h -o /tmp/test-thrift.dft
./build-thrift-verify/dwarfsck /tmp/test-thrift.dft

# Test both-formats (should now work)
cmake -B build-both-verify -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
cmake --build build-both-verify --target mkdwarfs dwarfsck -j8
./build-both-verify/mkdwarfs -i example/pg11339-h -o /tmp/test-both.dft
./build-both-verify/dwarfsck /tmp/test-both.dft
```

## Phase 2: Update Official Documentation (1 hour)

### 2.1: Update README.adoc (30 min)

**File**: `README.adoc`

**Add Architecture Section** (after Features, before Installation):

```asciidoc
== Architecture

=== Core Libraries

DwarFS consists of five modular C++20 libraries:

* **dwarfs_common**: Foundation layer (compression, I/O, utilities)
* **dwarfs_reader**: Read and interpret DwarFS images
* **dwarfs_writer**: Create DwarFS images from directory trees
* **dwarfs_extractor**: Extract images to disk or archive formats
* **dwarfs_rewrite**: Recompress or rebuild existing images

=== Metadata Serialization

DwarFS supports two optional metadata formats:

* **FlatBuffers** (modern default):
  - Memory-mappable, zero-copy access
  - Excellent portability (header-only library)
  - Slightly larger than Thrift (~5-10%)
  - Works on all platforms

* **Thrift Compact** (legacy):
  - Memory-mappable, zero-copy access
  - Smallest format (bit-packed)
  - Requires Folly + fbthrift (complex dependencies)
  - Backward compatibility for old images

Both formats use a **domain-based architecture** with thin format adapters:

----
Domain Model (format-agnostic)
      │
      ├── Reader Layer
      │   ├── common_metadata_operations (shared logic)
      │   └── Format Adapters
      │       ├── flatbuffers_metadata_adapter
      │       └── thrift_metadata_adapter
      │
      ├── Writer Layer
      │   └── Format Implementations
      │       ├── flatbuffers_metadata_writer
      │       └── thrift_metadata_writer
      │
      └── Converters
          ├── domain_flatbuffers_converter
          └── domain_thrift_converter
----

=== Build Configurations

Choose metadata formats via CMake:

[source,bash]
----
# FlatBuffers-only (recommended)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF

# Thrift-only (legacy support)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON

# Both formats (maximum compatibility)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
----
```

### 2.2: Create Architecture Document (20 min)

**New File**: `doc/dwarfs-metadata-architecture.md`

(Full architecture documentation with diagrams, patterns, implementation details)

### 2.3: Move Temporary Documentation (10 min)

```bash
mkdir -p old-docs/sessions-28-32
mv doc/SESSION_28_*.md old-docs/sessions-28-32/
mv doc/SESSION_29_*.md old-docs/sessions-28-32/
mv doc/SESSION_30_*.md old-docs/sessions-28-32/
mv doc/SESSION_31[A-K]_*.md old-docs/sessions-28-32/
mv doc/SESSION_32_*.md old-docs/sessions-28-32/

# Keep final summaries
git add doc/SESSION_31L_COMPLETION_SUMMARY.md
git add doc/SESSION_33_COMPLETION_SUMMARY.md
```

## Phase 3: Verification & Commit (30 min)

### 3.1: Build Matrix Verification (20 min)

Test all three configurations:

| Config | Build | Create FS | Verify | Size |
|--------|-------|-----------|--------|------|
| FlatBuffers-only | ✅ | ✅ | ✅ | Baseline |
| Thrift-only | ✅ | ✅ | ✅ | ~5-10% smaller |
| Both-formats | ✅ | ✅ | ✅ | Both work |

### 3.2: Git Commit (10 min)

```bash
git add src/reader/internal/backend_adapter.h
git add src/reader/internal/backend_adapter.cpp
git add src/reader/internal/common_metadata_operations.cpp
git add cmake/libdwarfs.cmake  # if needed
git add README.adoc
git add doc/dwarfs-metadata-architecture.md

git commit -m "fix(reader): resolve architecture mismatch for Thrift backend

Completed metadata serialization architecture (Sessions 28-33).

Changes:
- Created backend_adapter for chunk_range construction
- Fixed common_metadata_operations to use adapters
- Updated official documentation with architecture
- Moved temporary docs to old-docs/sessions-28-32/

All build configurations now functional:
- FlatBuffers-only: ✅ (domain-native)
- Thrift-only: ✅ (via adapter)
- Both-formats: ✅ (both work)

Sessions: 28-33 complete"
```

## Success Criteria

After Session 33:

1. ✅ **All Builds Work**:
   - FlatBuffers-only: ✅
   - Thrift-only: ✅
   - Both-formats: ✅

2. ✅ **Architecture Clean**:
   - Domain model throughout
   - Adapters for backend differences
   - No format-specific code in common operations

3. ✅ **Documentation Current**:
   - README.adoc reflects architecture
   - Architecture doc comprehensive
   - Temporary docs archived

4. ✅ **Metadata Serialization 100% Complete**:
   - Reader layer: ✅
   - Writer layer: ✅
   - Converters: ✅
   - All formats: ✅

## Timeline

| Phase | Description | Time | Total |
|-------|-------------|------|-------|
| 1 | Fix chunk_range construction | 1.5h | 1.5h |
| 2 | Update documentation | 1h | 2.5h |
| 3 | Verify & commit | 30m | **3h** |

## Files to Create

- `src/reader/internal/backend_adapter.h` - NEW
- `src/reader/internal/backend_adapter.cpp` - NEW
- `doc/dwarfs-metadata-architecture.md` - NEW

## Files to Modify

- `src/reader/internal/common_metadata_operations.cpp` - Use adapter
- `cmake/libdwarfs.cmake` - Add backend_adapter to build (if needed)
- `README.adoc` - Add architecture section

## Files to Move

- Move ~25 temporary session docs to `old-docs/sessions-28-32/`

## Known Issues (Pre-Session 33)

1. ❌ **Thrift-only build**: Architecture mismatch in chunk_range construction
2. ❌ **Both-formats build**: Same architecture mismatch

## Expected Outcome (Post-Session 33)

1. ✅ All three build configurations functional
2. ✅ Clean domain-based architecture throughout
3. ✅ Official documentation current and comprehensive
4. ✅ Metadata serialization work 100% complete (Sessions 28-33)

---

**Last Updated**: 2025-12-23 16:56 HKT
**Status**: Ready for Session 33 execution