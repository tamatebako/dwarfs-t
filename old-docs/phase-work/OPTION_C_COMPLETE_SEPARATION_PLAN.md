# Option C: Complete Backend Separation - Implementation Plan

**Goal**: Fully independent Thrift and FlatBuffers backends with NO shared type dependencies

**Estimated Time**: 8-12 hours
**Performance Impact**: ZERO (maintains current performance for both formats)
**Code Impact**: ~2000 lines duplicated, fully isolated backends

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                     metadata_v2 (Public API)                    │
│                  include/dwarfs/reader/internal/metadata_v2.h   │
└──────────────────────────────┬──────────────────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    │  metadata_v2.cpp    │
                    │  (format detection) │
                    └──────────┬──────────┘
                               │
        ┌──────────────────────┴──────────────────────┐
        │                                             │
        ▼                                             ▼
┌───────────────────┐                      ┌────────────────────┐
│ Thrift Backend    │                      │ FlatBuffers        │
│ (ORIGINAL)        │                      │ Backend (NEW)      │
├───────────────────┤                      ├────────────────────┤
│ metadata_v2_data  │                      │ metadata_v2_data   │
│ (Frozen2 types)   │                      │ (FlatBuf types)    │
├───────────────────┤                      ├────────────────────┤
│ metadata_types_   │                      │ metadata_types_    │
│ thrift.h/cpp      │                      │ flatbuffers.h/cpp  │
│                   │                      │                    │
│ • inode_view_impl │                      │ • inode_view_impl  │
│ • dir_entry_view_ │                      │ • dir_entry_view_  │
│ • directory_view  │                      │ • directory_view   │
│ • chunk_view      │                      │ • chunk_view       │
│ • global_metadata │                      │ • global_metadata  │
├───────────────────┤                      ├────────────────────┤
│ Uses:             │                      │ Uses:              │
│ • MappedFrozen<T> │                      │ • Metadata const*  │
│ • View<T>         │                      │ • FlatBuffer views │
│ • Thrift frozen   │                      │ • Zero-copy access │
└───────────────────┘                      └────────────────────┘
```

## Phase Breakdown

### Phase 1: Create FlatBuffers-Specific Type System (4 hours)

**Objective**: Duplicate and specialize all metadata_types for FlatBuffers

#### Step 1.1: Create FlatBuffers View Types (1 hour)

**New File**: `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`

**Content**: FlatBuffers-specific views that replace Thrift types
```cpp
namespace dwarfs::reader::internal::flatbuffers_backend {

// Forward declarations
class inode_view_impl;
class dir_entry_view_impl;
class directory_view;
class chunk_view;
class global_metadata;

// inode_view_impl: Wraps ::dwarfs::flatbuffers::InodeData const*
class inode_view_impl {
 public:
  inode_view_impl(::dwarfs::flatbuffers::InodeData const* inode,
                  uint32_t inode_num,
                  ::dwarfs::flatbuffers::Metadata const* meta);

  uint32_t inode_num() const { return inode_num_; }
  uint16_t mode() const;
  uint16_t getuid() const;
  uint16_t getgid() const;
  bool is_directory() const;
  bool is_regular_file() const;
  bool is_symlink() const;
  bool is_device() const;
  uint32_t nlink_minus_one() const;

 private:
  ::dwarfs::flatbuffers::InodeData const* inode_;
  uint32_t inode_num_;
  ::dwarfs::flatbuffers::Metadata const* meta_;
};

// dir_entry_view_impl: FlatBuffers directory entry
class dir_entry_view_impl {
 public:
  static dir_entry_view_impl from_dir_entry_index(
      uint32_t self_index, uint32_t parent_index,
      global_metadata const& g);

  std::string_view name() const;
  inode_view_impl inode() const;
  uint32_t self_index() const { return self_index_; }

 private:
  uint32_t self_index_;
  uint32_t parent_index_;
  global_metadata const* global_;
};

// directory_view: FlatBuffers directory access
class directory_view {
 public:
  directory_view(uint32_t inode, global_metadata const& g);

  uint32_t inode() const { return inode_; }
  uint32_t parent_inode() const;
  uint32_t first_entry() const;
  size_t entry_count() const;
  auto entry_range() const;

 private:
  uint32_t inode_;
  global_metadata const* global_;
};

// chunk_view: FlatBuffers chunk access
class chunk_view {
 public:
  chunk_view(::dwarfs::flatbuffers::Chunk const* chunk);

  uint32_t block() const;
  uint32_t offset() const;
  uint32_t size() const;
  bool is_data() const;
  bool is_hole() const;

 private:
  ::dwarfs::flatbuffers::Chunk const* chunk_;
};

// global_metadata: FlatBuffers metadata wrapper
class global_metadata {
 public:
  using Meta = ::dwarfs::flatbuffers::Metadata const*;

  global_metadata(logger& lgr, Meta meta);

  Meta meta() const { return meta_; }
  string_table const& names() const { return names_; }
  std::string_view name(uint32_t index) const;
  void check_consistency(logger& lgr) const;

 private:
  Meta meta_;
  std::vector<std::string> names_vec_;  // Storage for names
  string_table names_;
};

} // namespace dwarfs::reader::internal::flatbuffers_backend
```

**Implementation**: `src/reader/internal/metadata_types_flatbuffers.cpp`
- Implement all view methods to access FlatBuffers data structures
- Use FlatBuffers accessors (Get(), size(), etc.)
- No Thrift dependencies

#### Step 1.2: Create FlatBuffers metadata_v2_data (2 hours)

**Modify**: `src/reader/internal/metadata_v2_flatbuffers.cpp`

**Changes**:
- Replace `using namespace dwarfs::reader::internal;` with `using namespace flatbuffers_backend;`
- Change all type references to FlatBuffers-specific types
- Remove all Thrift type usage
- Use FlatBuffers accessors throughout

#### Step 1.3: Update FlatBuffers Chunk Range (0.5 hour)

**New**: `include/dwarfs/reader/internal/chunk_range_flatbuffers.h`

```cpp
class chunk_range_flatbuffers {
 public:
  chunk_range_flatbuffers(::dwarfs::flatbuffers::Metadata const* meta,
                          uint32_t begin, uint32_t end);

  auto begin() const;
  auto end() const;
  size_t size() const { return end_ - begin_; }

 private:
  ::dwarfs::flatbuffers::Metadata const* meta_;
  uint32_t begin_;
  uint32_t end_;
};
```

#### Step 1.4: Test FlatBuffers Backend Independently (0.5 hour)

```bash
# Build with FlatBuffers only
cmake -B build-fb-only -DDWARFS_WITH_THRIFT=OFF -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build-fb-only mkdwarfs dwarfsck dwarfsextract

# Test
./build-fb-only/mkdwarfs -i testdata -o test.dff --format=flatbuffers
./build-fb-only/dwarfsck test.dff
./build-fb-only/dwarfsextract -i test.dff -o out/
```

### Phase 2: Isolate Thrift Backend (2 hours)

**Objective**: Ensure Thrift backend is fully independent

#### Step 2.1: Move Thrift Types to Namespace (1 hour)

**Modify**: `src/reader/internal/metadata_types_thrift.cpp`

**Changes**:
- Wrap all types in `namespace thrift_backend { }`
- Update metadata_v2_thrift.cpp to use `thrift_backend::` prefix
- Ensure NO cross-contamination with FlatBuffers types

#### Step 2.2: Test Thrift Backend Independently (0.5 hour)

```bash
# Build with Thrift only
cmake -B build-thrift-only -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=OFF
ninja -C build-thrift-only mkdwarfs dwarfsck dwarfsextract

# Test
./build-thrift-only/mkdwarfs -i testdata -o test.dft --format=thrift
./build-thrift-only/dwarfsck test.dft
./build-thrift-only/dwarfsextract -i test.dft -o out/
```

#### Step 2.3: Clean Up Shared Dependencies (0.5 hour)

**Files to check**:
- `src/internal/string_table.cpp` - Ensure both constructor variants work
- `src/internal/metadata_utils.cpp` - Split if needed
- `include/dwarfs/reader/internal/metadata_types.h` - Make format-agnostic

### Phase 3: Runtime Factory Integration (2 hours)

**Objective**: Make factory work with both isolated backends

#### Step 3.1: Update Factory Signatures (0.5 hour)

**File**: `src/reader/internal/metadata_v2_factory.cpp`

**Changes**:
```cpp
#ifdef DWARFS_HAVE_THRIFT
extern metadata_v2 make_metadata_v2_thrift(...);
#endif

#ifdef DWARFS_HAVE_FLATBUFFERS
extern metadata_v2 make_metadata_v2_flatbuffers(...);
#endif

metadata_v2::metadata_v2(...) {
  auto format = detect_format(data);

  #ifdef DWARFS_HAVE_FLATBUFFERS
  if (format == FLATBUFFERS) {
    *this = make_metadata_v2_flatbuffers(...);
    return;
  }
  #endif

  #ifdef DWARFS_HAVE_THRIFT
  if (format == THRIFT_COMPACT) {
    *this = make_metadata_v2_thrift(...);
    return;
  }
  #endif

  throw runtime_error("Unsupported format");
}
```

#### Step 3.2: Remove CMake Type Isolation Flags (0.5 hour)

**File**: `cmake/libdwarfs.cmake`

**Remove lines 138-154** (the `-UDWARFS_HAVE_THRIFT` flags):
```cmake
# REMOVE THIS BLOCK - no longer needed with separated types
if(DWARFS_HAVE_THRIFT AND DWARFS_HAVE_FLATBUFFERS)
  set_source_files_properties(
    src/reader/internal/metadata_types_flatbuffers.cpp
    src/reader/internal/metadata_v2_flatbuffers.cpp
    PROPERTIES
    COMPILE_OPTIONS "-UDWARFS_HAVE_THRIFT;-DDWARFS_FLATBUFFERS_BACKEND"
  )
endif()
```

**Reason**: With namespaced types, no conflict exists

#### Step 3.3: Test Dual-Format Build (1 hour)

```bash
# Build with both formats
cmake -B build-dual -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build-dual mkdwarfs dwarfsck dwarfsextract

# Test both formats
./build-dual/mkdwarfs -i testdata -o test.dff --format=flatbuffers
./build-dual/mkdwarfs -i testdata -o test.dft --format=thrift

# Extract both
./build-dual/dwarfsextract -i test.dff -o out-fb/
./build-dual/dwarfsextract -i test.dft -o out-thrift/

# Verify identical
diff -r out-fb/ out-thrift/
```

### Phase 4: Remove Conversion Layer (1 hour)

**Objective**: Delete FlatBuffers→Thrift conversion from Thrift backend

#### Step 4.1: Remove Conversion Code

**File**: `src/reader/internal/metadata_v2_thrift.cpp`

**Delete lines 649-693** (the entire conversion block):
```cpp
// DELETE THIS ENTIRE BLOCK
if (detected.has_value() &&
    *detected == metadata::serialization::SerializationFormat::FLATBUFFERS) {
  // ... 40 lines of conversion code ...
}
```

**Replace with**:
```cpp
// Native Thrift frozen format - use directly
return check_frozen(map_frozen<thrift::metadata::metadata>(schema_, data_));
```

#### Step 4.2: Verify No Conversion Messages

```bash
./build-dual/dwarfsck test.dff 2>&1 | grep -i "convert"
# Expected: No output (no conversion happening)
```

### Phase 5: Documentation & Cleanup (2 hours)

#### Step 5.1: Update Official Documentation (1 hour)

**Files to update**:

1. **README.adoc** - Add metadata formats section:
```adoc
== Metadata Formats

DwarFS supports two metadata serialization formats:

* **FlatBuffers** (default, recommended): Modern, portable, zero-copy format
  - File extension: `.dff`
  - Excellent cross-platform support
  - Header-only dependencies

* **Thrift Compact** (legacy, optional): Legacy format for backward compatibility
  - File extension: `.dft`
  - Requires Folly + fbthrift
  - Slightly more space-efficient

=== Creating with Specific Format

[source,bash]
----
# FlatBuffers (default)
mkdwarfs -i /path/to/source -o output.dff --format=flatbuffers

# Thrift (legacy)
mkdwarfs -i /path/to/source -o output.dft --format=thrift
----

=== Migration

Existing `.dwarfs` images remain compatible. Recommended migration:

[source,bash]
----
mkdwarfs --recompress=metadata --format=flatbuffers -i old.dwarfs -o new.dff
----
```

2. **doc/dwarfs-format.md** - Document FlatBuffers format specification
3. **doc/mkdwarfs.md** - Document `--format` option
4. **CHANGES.md** - Add v0.16.0 release notes

#### Step 5.2: Move Outdated Documentation (0.5 hour)

```bash
mkdir -p old-docs/dual-format-work
mv doc/DUAL_FORMAT_*.md old-docs/dual-format-work/
mv doc/STRATEGY_PATTERN_*.md old-docs/
mv doc/PHASE_*.md old-docs/
mv doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md old-docs/
```

#### Step 5.3: Final Code Cleanup (0.5 hour)

**Remove**:
- Temporary test directories (`test-dual/`, `test-fb-out/`, etc.)
- Backup files (`*.bak`, `*.tmp`, `*.orig`)
- Debug logging in FlatBuffers backend

**Verify**:
- No `#ifdef DWARFS_HAVE_FLATBUFFERS` in Thrift backend
- No `#ifdef DWARFS_HAVE_THRIFT` in FlatBuffers backend (except factory conditional)
- Clean git status

## Implementation Checklist

### Phase 1: FlatBuffers Type System
- [ ] Create `metadata_types_flatbuffers.h` with all view types
- [ ] Implement `metadata_types_flatbuffers.cpp` with FlatBuffers accessors
- [ ] Create `chunk_range_flatbuffers.h/cpp`
- [ ] Update `metadata_v2_flatbuffers.cpp` to use flatbuffers_backend namespace
- [ ] Test FlatBuffers-only build
- [ ] Verify extraction works (FlatBuffers-only)

### Phase 2: Thrift Backend Isolation
- [ ] Wrap Thrift types in `thrift_backend` namespace
- [ ] Update `metadata_v2_thrift.cpp` to use thrift_backend namespace
- [ ] Test Thrift-only build
- [ ] Verify extraction works (Thrift-only)

### Phase 3: Factory Integration
- [ ] Update factory with proper #ifdef guards
- [ ] Remove CMake type isolation flags
- [ ] Test dual-format build compiles
- [ ] Test format auto-detection
- [ ] Test both formats extract correctly

### Phase 4: Remove Conversion
- [ ] Delete lines 649-693 from metadata_v2_thrift.cpp
- [ ] Verify no conversion messages in logs
- [ ] Benchmark both formats (ensure no performance regression)

### Phase 5: Documentation & Cleanup
- [ ] Update README.adoc with formats section
- [ ] Update doc/mkdwarfs.md with --format
- [ ] Update doc/dwarfs-format.md with FlatBuffers spec
- [ ] Update CHANGES.md for v0.16.0
- [ ] Move all old docs to old-docs/
- [ ] Remove test artifacts
- [ ] Clean up debug logging
- [ ] Final commit

## Success Criteria

- [ ] Both formats CREATE successfully
- [ ] Both formats READ natively (no conversion)
- [ ] Both formats EXTRACT identically
- [ ] FUSE mounting works for both
- [ ] No performance regression in benchmarks
- [ ] Zero cross-dependencies between backends
- [ ] Clean build in all 3 modes (Thrift-only, FlatBuffers-only, Dual)
- [ ] All documentation updated
- [ ] Ready to merge to main branch

## File Structure After Completion

```
src/reader/internal/
├── metadata_v2.cpp                     # Factory (dual-format only)
├── metadata_v2_factory.cpp             # Constructor logic
├── metadata_types.h                    # Format-agnostic public API
│
├── metadata_types_thrift.h/cpp         # Thrift backend types
├── metadata_v2_thrift.cpp              # Thrift backend impl
│
├── metadata_types_flatbuffers.h/cpp    # FlatBuffers backend types
├── metadata_v2_flatbuffers.cpp         # FlatBuffers backend impl
├── flatbuffer_metadata_views.h/cpp     # FlatBuffers view helpers
│
└── time_resolution_handler.cpp         # Shared utility
```

## Risk Mitigation

**Risk 1**: Code duplication makes maintenance harder
- **Mitigation**: Template common algorithms where possible
- **Acceptance**: DwarFS is mature; metadata reading rarely changes

**Risk 2**: More complex build system
- **Mitigation**: Clear CMake modularization
- **Testing**: CI matrix covers all 3 build modes

**Risk 3**: Potential for divergence between backends
- **Mitigation**: Integration tests that verify identical behavior
- **Testing**: Differential testing (both formats → same extraction)

## Performance Validation

After implementation, run benchmarks to verify:

```bash
python3 benchmarks/run_complete_comparison.py \
  --dataset benchmark-files/perl-5.43.3 \
  --formats flatbuffers,thrift \
  --metrics read_latency,random_access,metadata_ops
```

**Expected results**:
- FlatBuffers: Same or better than Option A type-erased
- Thrift: Unchanged from current
- Both: Native format access, zero conversion overhead