// Create new file with complete architecture design
# DwarFS Strategy Pattern Architecture - Detailed Design

**Date**: 2025-11-17
**Status**: Phase 1 - Interface Design
**Branch**: feature/multi-format-serialization-fuse

## Executive Summary

This document provides the complete architectural design for implementing the Strategy Pattern to achieve format independence in DwarFS metadata handling. The design eliminates hard Thrift dependencies, enabling FlatBuffers-only builds on all platforms including AppleClang 17.

## Current State Analysis

### Problem
The writer (`metadata_builder.cpp`) has hard dependencies on Thrift types:
- Line 243: `thrift::metadata::metadata const& build()` - Returns Thrift type
- Throughout: Direct manipulation of `thrift::metadata::metadata md_`
- Result: Cannot build writer without Thrift, blocking FlatBuffers-only builds on AppleClang 17

### Reader State (Already Format-Independent ✓)
- `metadata_v2_flatbuffers.cpp` and `metadata_v2_thrift.cpp` exist
- Both work independently
- Good model to follow for writer refactoring

## Architecture Design

### Strategy Pattern Overview

```
┌─────────────────────────────────────────────────────────┐
│         HIGH-LEVEL ABSTRACTION (Interface)              │
│                                                         │
│  • metadata_provider (reader)                          │
│  • metadata_builder (writer)                           │
│                                                         │
│  ALL operations work with domain model ONLY            │
└──────────────────┬──────────────────────────────────────┘
                   │
         ┌─────────┴─────────┐
         │   implements      │   implements
         ▼                   ▼
┌────────────────┐    ┌────────────────┐
│ Thrift Impl    │    │FlatBuffers Impl│
│ (optional)     │    │  (required)    │
│                │    │                │
│ Converts       │    │ Works directly │
│ domain ↔       │    │ with domain    │
│ thrift types   │    │ model          │
└────────────────┘    └────────────────┘
         │                   │
         └───────────────────┤
                             ▼
                  ┌──────────────────┐
                  │   Domain Model   │
                  │ metadata::domain │
                  │   ::metadata     │
                  └──────────────────┘
```

### 1. Abstract Interfaces

#### 1.1 Reader Interface: `metadata_provider`
**File**: `include/dwarfs/reader/metadata_provider.h` (NEW)

```cpp
namespace dwarfs::reader {

/**
 * Abstract interface for reading metadata from serialized format.
 *
 * Strategy Pattern: Allows different serialization formats (Thrift, FlatBuffers)
 * to be used interchangeably.
 *
 * ALL methods work with domain model types from metadata::domain namespace.
 *
 * Implementation Details:
 * - Thrift implementation: Converts Frozen2 structures → domain on-demand
 * - FlatBuffers implementation: Converts FlatBuffer tables → domain on-demand
 * - Both provide zero-copy access where possible
 */
class metadata_provider {
public:
  virtual ~metadata_provider() = default;

  // Core metadata access - returns domain model types
  virtual metadata::domain::chunk get_chunk(uint32_t inode, size_t offset) const = 0;
  virtual metadata::domain::directory get_directory(uint32_t inode) const = 0;
  virtual metadata::domain::inode_data get_inode(uint32_t inode_num) const = 0;

  // Filesystem properties
  virtual uint32_t get_block_size() const = 0;
  virtual uint64_t get_timestamp_base() const = 0;
  virtual std::optional<metadata::domain::fs_options> get_options() const = 0;

  // Lookup tables - efficient access
  virtual uint32_t get_uid(uint32_t index) const = 0;
  virtual uint32_t get_gid(uint32_t index) const = 0;
  virtual uint32_t get_mode(uint32_t index) const = 0;
  virtual std::string_view get_name(uint32_t index) const = 0;
  virtual std::string get_symlink(uint32_t index) const = 0;

  // Feature detection
  virtual bool has_feature(std::string_view feature) const = 0;
  virtual std::set<std::string> get_features() const = 0;

  // Factory method - format auto-detection
  static std::unique_ptr<metadata_provider> create(
    SerializationFormat format,
    std::span<uint8_t const> schema,
    std::span<uint8_t const> data,
    metadata_options const& options);
};

} // namespace dwarfs::reader
```

#### 1.2 Writer Interface: `metadata_builder`
**File**: `include/dwarfs/writer/internal/metadata_builder.h` (MODIFY EXISTING)

**Key Change**: `build()` returns `metadata::domain::metadata` instead of `thrift::metadata::metadata`

```cpp
namespace dwarfs::writer::internal {

class metadata_builder {
public:
  // Constructor for new metadata
  metadata_builder(logger& lgr, metadata_options const& options);

  // Constructor for existing metadata (rewrite scenario)
  metadata_builder(logger& lgr,
                   metadata::domain::metadata const& md,
                   fs_options const* orig_fs_options,
                   filesystem_version const& orig_fs_version,
                   metadata_options const& options);

  virtual ~metadata_builder() = default;

  // Data gathering methods (unchanged)
  virtual void set_devices(std::vector<uint64_t> devices) = 0;
  virtual void set_symlink_table_size(size_t size) = 0;
  virtual void set_block_size(uint32_t block_size) = 0;
  virtual void set_shared_files_table(std::vector<uint32_t>) = 0;
  virtual void set_category_names(std::vector<std::string>) = 0;
  virtual void set_block_categories(std::vector<uint32_t>) = 0;
  virtual void add_symlink_table_entry(size_t index, uint32_t entry) = 0;

  virtual void gather_chunks(
    inode_manager const& im,
    block_manager const& bm,
    size_t chunk_count) = 0;

  virtual void gather_entries(
    std::span<dir*> dirs,
    global_entry_data const& ge_data,
    uint32_t num_inodes) = 0;

  virtual void gather_global_entry_data(
    global_entry_data const& ge_data) = 0;

  virtual void remap_blocks(
    std::span<block_mapping const> mapping,
    size_t new_block_count) = 0;

  // *** KEY CHANGE: Returns domain model, not Thrift type ***
  virtual metadata::domain::metadata build() = 0;

  // Factory methods
  static std::unique_ptr<metadata_builder> create(
    logger& lgr,
    metadata_options const& options,
    SerializationFormat format);

  static std::unique_ptr<metadata_builder> create_from_existing(
    logger& lgr,
    metadata::domain::metadata const& existing_md,
    fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options,
    SerializationFormat format);

  class impl;  // Forward declaration for PIMPL pattern
private:
  std::unique_ptr<impl> impl_;
};

} // namespace dwarfs::writer::internal
```

### 2. Concrete Implementations

#### 2.1 Thrift Strategy (Reader)
**File**: `src/reader/thrift_metadata_provider.cpp` (NEW)

```cpp
#ifdef DWARFS_HAVE_THRIFT

namespace dwarfs::reader::internal {

/**
 * Thrift-based metadata provider.
 *
 * Wraps Frozen2 Thrift structures and converts to domain model on-demand.
 * Uses memory-mapped zero-copy access internally.
 */
class thrift_metadata_provider final : public metadata_provider {
public:
  thrift_metadata_provider(
    std::span<uint8_t const> schema,
    std::span<uint8_t const> data,
    metadata_options const& options);

  // Implement all virtual methods
  metadata::domain::chunk get_chunk(uint32_t inode, size_t offset) const override {
    // Access Thrift Frozen2 structure
    auto thrift_chunk = meta_.chunks()[compute_chunk_index(inode, offset)];

    // Convert to domain model
    return metadata::converters::chunk_from_thrift(thrift_chunk);
  }

  metadata::domain::directory get_directory(uint32_t inode) const override;
  // ... implement all other methods

private:
  MappedFrozen<thrift::metadata::metadata> meta_;  // Thrift internal
  global_metadata global_;  // Helper for directory navigation

  // Conversion helpers
  size_t compute_chunk_index(uint32_t inode, size_t offset) const;
};

} // namespace dwarfs::reader::internal

#endif // DWARFS_HAVE_THRIFT
```

#### 2.2 FlatBuffers Strategy (Reader)
**File**: `src/reader/flatbuffers_metadata_provider.cpp` (NEW)

```cpp
namespace dwarfs::reader::internal {

/**
 * FlatBuffers-based metadata provider.
 *
 * Wraps FlatBuffers tables and converts to domain model on-demand.
 * Uses zero-copy access internally.
 */
class flatbuffers_metadata_provider final : public metadata_provider {
public:
  flatbuffers_metadata_provider(
    std::span<uint8_t const> data,
    metadata_options const& options);

  // Implement all virtual methods
  metadata::domain::chunk get_chunk(uint32_t inode, size_t offset) const override {
    // Access FlatBuffers table
    auto chunks = meta_->chunks();
    auto fb_chunk = chunks->Get(compute_chunk_index(inode, offset));

    // Convert to domain model
    return metadata::converters::chunk_from_flatbuffers(fb_chunk);
  }

  metadata::domain::directory get_directory(uint32_t inode) const override;
  // ... implement all other methods

private:
  ::dwarfs::flatbuffers::Metadata const* meta_;  // FlatBuffers internal
  global_metadata global_;  // Helper for directory navigation

  // Conversion helpers
  size_t compute_chunk_index(uint32_t inode, size_t offset) const;
};

} // namespace dwarfs::reader::internal
```

#### 2.3 Thrift Strategy (Writer)
**File**: `src/writer/internal/thrift_metadata_builder.cpp` (NEW - move from metadata_builder.cpp)

```cpp
#ifdef DWARFS_HAVE_THRIFT

namespace dwarfs::writer::internal {

/**
 * Thrift-based metadata builder.
 *
 * Builds metadata using Thrift structures internally, then converts
 * to domain model on build().
 */
class thrift_metadata_builder final : public metadata_builder::impl {
public:
  using uid_type = file_stat::uid_type;
  using gid_type = file_stat::gid_type;

  thrift_metadata_builder(logger& lgr, metadata_options const& options);

  // Constructor for rewrite scenario
  template <typename T>
    requires(std::same_as<std::decay_t<T>, thrift::metadata::metadata>)
  thrift_metadata_builder(logger& lgr, T&& md,
                          thrift::metadata::fs_options const* orig_fs_options,
                          filesystem_version const& orig_fs_version,
                          metadata_options const& options);

  // Implement all data gathering methods (mostly unchanged from original)
  void set_devices(std::vector<uint64_t> devices) override;
  void gather_chunks(inode_manager const& im, block_manager const& bm,
                    size_t chunk_count) override;
  void gather_entries(std::span<dir*> dirs, global_entry_data const& ge_data,
                     uint32_t num_inodes) override;
  // ... all other methods from original metadata_builder.cpp

  // *** KEY CHANGE: Convert Thrift → domain before returning ***
  metadata::domain::metadata build() override {
    // Build Thrift structures (original code)
    thrift::metadata::metadata const& thrift_md = build_thrift_internal();

    // Convert to domain model using converter
    return metadata::converters::from_thrift(thrift_md);
  }

private:
  thrift::metadata::metadata md_;  // Internal Thrift representation
  feature_set features_;
  metadata_options const& options_;
  std::optional<size_t> old_block_size_;
  time_resolution_converter timeres_;

  // Original build() becomes internal helper
  thrift::metadata::metadata const& build_thrift_internal();

  // All helper methods from original (unchanged)
  void update_inodes();
  void update_nlink();
  void update_totals_and_size_cache();
  void apply_chmod();
  // ...
};

} // namespace dwarfs::writer::internal

#endif // DWARFS_HAVE_THRIFT
```

#### 2.4 FlatBuffers Strategy (Writer)
**File**: `src/writer/internal/flatbuffers_metadata_builder.cpp` (NEW)

```cpp
namespace dwarfs::writer::internal {

/**
 * FlatBuffers-based metadata builder.
 *
 * Builds domain model DIRECTLY - no conversion needed!
 * This is simpler and more efficient than Thrift approach.
 */
class flatbuffers_metadata_builder final : public metadata_builder::impl {
public:
  using uid_type = file_stat::uid_type;
  using gid_type = file_stat::gid_type;

  flatbuffers_metadata_builder(logger& lgr, metadata_options const& options);

  // Constructor for rewrite scenario
  flatbuffers_metadata_builder(logger& lgr,
                               metadata::domain::metadata const& md,
                               fs_options const* orig_fs_options,
                               filesystem_version const& orig_fs_version,
                               metadata_options const& options);

  // Implement all data gathering methods
  void set_devices(std::vector<uint64_t> devices) override {
    md_.devices = std::move(devices);
  }

  void gather_chunks(inode_manager const& im, block_manager const& bm,
                    size_t chunk_count) override {
    md_.chunk_table.resize(im.count() + 1);
    md_.chunks.reserve(chunk_count);

    // Build chunks directly in domain model
    // ... implementation similar to Thrift but using domain types
  }

  void gather_entries(std::span<dir*> dirs, global_entry_data const& ge_data,
                     uint32_t num_inodes) override {
    md_.dir_entries.emplace();
    md_.inodes.resize(num_inodes);
    md_.directories.reserve(dirs.size() + 1);

    // Build entries directly in domain model
    // ... implementation similar to Thrift but using domain types
  }

  // ... all other data gathering methods

  // *** KEY ADVANTAGE: Build domain model DIRECTLY (no conversion!) ***
  metadata::domain::metadata build() override {
    update_nlink();
    update_totals_and_size_cache();
    pack_metadata();

    return std::move(md_);  // No conversion needed!
  }

private:
  metadata::domain::metadata md_;  // Build domain model directly!
  feature_set features_;
  metadata_options const& options_;
  std::optional<size_t> old_block_size_;
  time_resolution_converter timeres_;

  // Helper methods (adapted from Thrift version)
  void update_inodes();
  void update_nlink();
  void update_totals_and_size_cache();
  void apply_chmod();
  void pack_metadata();
};

} // namespace dwarfs::writer::internal
```

### 3. Factory Pattern

#### 3.1 Reader Factory
**File**: `src/reader/metadata_provider_factory.cpp` (NEW)

```cpp
namespace dwarfs::reader {

std::unique_ptr<metadata_provider> metadata_provider::create(
    SerializationFormat format,
    std::span<uint8_t const> schema,
    std::span<uint8_t const> data,
    metadata_options const& options) {

  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT:
      return std::make_unique<internal::thrift_metadata_provider>(
        schema, data, options);
#endif

    case SerializationFormat::FLATBUFFERS:
      return std::make_unique<internal::flatbuffers_metadata_provider>(
        data, options);

    default:
      DWARFS_THROW(runtime_error,
        fmt::format("Unsupported metadata format: {}",
                    get_format_name(format)));
  }
}

} // namespace dwarfs::reader
```

#### 3.2 Writer Factory
**File**: `src/writer/internal/metadata_builder_factory.cpp` (NEW)

```cpp
namespace dwarfs::writer::internal {

std::unique_ptr<metadata_builder> metadata_builder::create(
    logger& lgr,
    metadata_options const& options,
    SerializationFormat format) {

  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT:
      return make_unique_logging_object<
        metadata_builder::impl,
        thrift_metadata_builder,
        logger_policies>(lgr, options);
#endif

    case SerializationFormat::FLATBUFFERS:
      return make_unique_logging_object<
        metadata_builder::impl,
        flatbuffers_metadata_builder,
        logger_policies>(lgr, options);

    default:
      DWARFS_THROW(runtime_error,
        fmt::format("Unsupported metadata format: {}",
                    get_format_name(format)));
  }
}

std::unique_ptr<metadata_builder> metadata_builder::create_from_existing(
    logger& lgr,
    metadata::domain::metadata const& existing_md,
    fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options,
    SerializationFormat format) {

  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT: {
      // Convert domain → Thrift for rewrite
      auto thrift_md = metadata::converters::to_thrift(existing_md);
      return make_unique_logging_object<
        metadata_builder::impl,
        thrift_metadata_builder,
        logger_policies>(lgr, std::move(thrift_md),
                        orig_fs_options, orig_fs_version, options);
    }
#endif

    case SerializationFormat::FLATBUFFERS:
      return make_unique_logging_object<
        metadata_builder::impl,
        flatbuffers_metadata_builder,
        logger_policies>(lgr, existing_md,
                        orig_fs_options, orig_fs_version, options);

    default:
      DWARFS_THROW(runtime_error,
        fmt::format("Unsupported metadata format: {}",
                    get_format_name(format)));
  }
}

} // namespace dwarfs::writer::internal
```

### 4. Integration Points

#### 4.1 Scanner (Writer)
**File**: `src/writer/scanner.cpp`

**Before**:
```cpp
#ifdef DWARFS_HAVE_THRIFT
metadata_builder mb(lgr_, options.metadata);
// ... use mb ...
auto const& md = mb.build();  // Returns Thrift type
#endif
```

**After**:
```cpp
// Create via factory with chosen format
auto mb = metadata_builder::create(
  lgr_,
  options.metadata,
  options.metadata_format);  // SerializationFormat from options

// ... use mb (via interface, no format knowledge) ...

// Build returns domain model (format-agnostic)
auto md = mb->build();

// Pass to writer (also format-agnostic now)
fsw.write_metadata(std::move(md));
```

#### 4.2 Filesystem Writer
**File**: `src/writer/filesystem_writer.cpp`

**Before**:
```cpp
void write_metadata(thrift::metadata::metadata const& md);
```

**After**:
```cpp
void write_metadata(metadata::domain::metadata const& md) {
  // Serialize using facade (already exists)
  auto facade = metadata::serialization::FacadeFactory::create(
    options_.metadata_format);

  auto bytes = facade->serialize(md);

  // Write bytes to image
  section.write(bytes.data(), bytes.size());
}
```

#### 4.3 FUSE Driver (Reader)
**File**: `src/reader/filesystem_v2.cpp`

**Before**:
```cpp
class filesystem_v2 {
#ifdef DWARFS_HAVE_THRIFT
  MappedFrozen<thrift::metadata::metadata> md_;
#endif

  void init(std::span<uint8_t> schema, std::span<uint8_t> data) {
    md_ = map_frozen<thrift::metadata::metadata>(schema, data);
  }
};
```

**After**:
```cpp
class filesystem_v2 {
  std::unique_ptr<metadata_provider> provider_;

  void init(std::span<uint8_t> schema, std::span<uint8_t> data) {
    // Auto-detect format
    auto format = metadata::serialization::SerializerRegistry::instance()
      .detect_format(data);

    // Create provider via factory
    provider_ = metadata_provider::create(format, schema, data, options_);
  }

  // All methods use provider interface (format-agnostic)
  chunk get_chunk(inode_num inode, size_t offset) {
    return provider_->get_chunk(inode, offset);  // Returns domain type
  }
};
```

### 5. File Organization

#### New Files to Create
```
include/dwarfs/reader/
  metadata_provider.h                    # NEW: Abstract reader interface

src/reader/
  thrift_metadata_provider.cpp           # NEW: Thrift implementation
  flatbuffers_metadata_provider.cpp      # NEW: FlatBuffers implementation
  metadata_provider_factory.cpp          # NEW: Reader factory

src/writer/internal/
  thrift_metadata_builder.cpp            # NEW: Move from metadata_builder.cpp
  flatbuffers_metadata_builder.cpp       # NEW: FlatBuffers implementation
  metadata_builder_factory.cpp           # NEW: Writer factory
```

#### Files to Modify
```
include/dwarfs/writer/internal/metadata_builder.h
  - Change build() return type: thrift::metadata::metadata → metadata::domain::metadata
  - Add factory static methods
  - Remove Thrift forward declarations from public API

src/reader/filesystem_v2.cpp
  - Replace direct metadata access with metadata_provider interface
  - Remove format-specific includes

src/writer/scanner.cpp
  - Use metadata_builder factory instead of direct construction
  - Work with domain model instead of Thrift types

src/writer/filesystem_writer.cpp
  - Accept metadata::domain::metadata instead of thrift::metadata::metadata
  - Use serialization facade for format-agnostic serialization
```

#### Files to Eventually Remove
```
src/writer/internal/metadata_builder.cpp
  # Split into thrift_metadata_builder.cpp and flatbuffers_metadata_builder.cpp
  # Keep temporarily during migration for reference
```

### 6. Migration Strategy

#### Phase 1: Interfaces (Week 1) - 2-3 days
**Goal**: Define abstract interfaces, no implementation changes yet

1. Create `include/dwarfs/reader/metadata_provider.h`
   - Pure virtual interface with domain model types
   - Document all methods thoroughly
   - Add factory method signature

2. Update `include/dwarfs/writer/internal/metadata_builder.h`
   - Change `build()` return type to `metadata::domain::metadata`
   - Add factory static methods
   - Keep PIMPL pattern (impl class)

3. Document interface contracts
   - Method preconditions/postconditions
   - Thread safety guarantees
   - Error handling expectations

**Validation**: Both headers compile, tests compile (may not link yet)

#### Phase 2: Thrift Refactor (Week 2) - 3-4 days
**Goal**: Move existing Thrift code into strategy pattern

1. Create `src/writer/internal/thrift_metadata_builder.cpp`
   - Copy code from `metadata_builder.cpp`
   - Rename class to `thrift_metadata_builder`
   - Add Thrift → domain conversion in `build()`
   - Guard with `#ifdef DWARFS_HAVE_THRIFT`

2. Create `src/reader/thrift_metadata_provider.cpp`
   - Extract Thrift-specific code from `metadata_v2_thrift.cpp`
   - Implement `metadata_provider` interface
   - Add Thrift → domain conversions

3. Test existing functionality
   - All tests should still pass
   - No behavioral changes yet

**Validation**: Full test suite passes with Thrift enabled

#### Phase 3: FlatBuffers Implementation (Week 2-3) - 4-5 days
**Goal**: Implement FlatBuffers strategies

1. Create `src/writer/internal/flatbuffers_metadata_builder.cpp`
   - Implement `metadata_builder::impl` interface
   - Build domain model directly (no conversion)
   - Reuse logic from Thrift version where applicable

2. Create `src/reader/flatbuffers_metadata_provider.cpp`
   - Extract FlatBuffers-specific code from `metadata_v2_flatbuffers.cpp`
   - Implement `metadata_provider` interface
   - Add FlatBuffers → domain conversions

3. Test FlatBuffers path
   - Create test images with FlatBuffers format
   - Verify round-trip (write + read)

**Validation**: Tests pass with FlatBuffers format

#### Phase 4: Factories (Week 3) - 1-2 days
**Goal**: Add factory implementations

1. Create `src/reader/metadata_provider_factory.cpp`
   - Implement `metadata_provider::create()`
   - Format detection integration

2. Create `src/writer/internal/metadata_builder_factory.cpp`
   - Implement `metadata_builder::create()`
   - Implement `metadata_builder::create_from_existing()`

3. Update CMakeLists.txt
   - Add new source files
   - Conditional compilation for Thrift files

**Validation**: Factory creates correct implementation based on format

#### Phase 5: Integration (Week 4) - 3-4 days
**Goal**: Update all integration points

1. Update `src/writer/scanner.cpp`
   - Use `metadata_builder::create()`
   - Work with domain model

2. Update `src/writer/filesystem_writer.cpp`
   - Accept `metadata::domain::metadata`
   - Use serialization facade

3. Update `src/reader/filesystem_v2.cpp`
   - Use `metadata_provider` interface
   - Remove format-specific code

4. Update `src/utility/rewrite_filesystem.cpp`
   - Use domain model throughout

**Validation**: All integration tests pass

#### Phase 6: Testing & Validation (Week 5-6) - 4-5 days
**Goal**: Comprehensive testing across platforms

1. Test both formats independently
   - Create/mount/extract with Thrift
   - Create/mount/extract with FlatBuffers
   - Verify compatibility

2. Test FlatBuffers-only build
   - `cmake -DDWARFS_WITH_THRIFT=OFF`
   - Build on AppleClang 17
   - All tests pass

3. Cross-platform CI validation
   - Linux (multiple distros + architectures)
   - macOS (x86_64 + arm64)
   - Windows (x64)
   - FreeBSD

4. Performance benchmarks
   - Compare FlatBuffers vs Thrift
   - Read performance
   - Write performance
   - Memory usage

**Validation**: All tests pass, performance within 10%

### 7. Success Criteria

- [ ] AppleClang 17 builds with `-DDWARFS_WITH_THRIFT=OFF`
- [ ] Both Thrift and FlatBuffers work independently
- [ ] No `#ifdef DWARFS_HAVE_THRIFT` in public headers
- [ ] No `thrift::metadata::` types in scanner.cpp, filesystem_writer.cpp
- [ ] Each format in separate implementation files
- [ ] Clean interfaces with domain model types only
- [ ] All existing tests pass with both formats
- [ ] New format-specific tests pass
- [ ] Performance within 10% between formats
- [ ] Cross-platform CI passes on all targets

### 8. Risk Mitigation

| Risk | Mitigation |
|------|------------|
| **Complex migration (148+ files)** | Phased approach, one component at a time. Keep old code temporarily. |
| **Existing code breaks** | Keep Thrift implementation identical initially. Extensive testing after each phase. |
| **Performance regression** | Benchmark each phase. Profile hot paths. Optimize domain model conversions. |
| **AppleClang 17 compilation** | Test FlatBuffers-only build early and often. Fix issues incrementally. |
| **Time overrun** | 6-week timeline includes 30% buffer. Can adjust scope if needed. |
| **Test failures** | Fix tests incrementally. Don't proceed to next phase until all tests pass. |

### 9. Quick Wins (Can Do Immediately)

#### Fix 1: Library Version Reporting
**File**: `src/library_dependencies.cpp:126-129`

**Issue**: FlatBuffers version not reported

**Fix**:
```cpp
#ifdef DWARFS_HAVE_FLATBUFFERS
  add_library("libflatbuffers", FLATBUFFERS_VERSION_MAJOR,
              FLATBUFFERS_VERSION_MINOR, FLATBUFFERS_VERSION_REVISION);
#endif
```

#### Fix 2: ASCII Art for Tebako Fork
Add "dwarfs-t" identifier in tool banners to distinguish from upstream dwarfs.

## Dependencies

### Required for All Phases
- Domain model types (already exist): `include/dwarfs/metadata/domain/`
- Serialization converters (will create): `include/dwarfs/metadata/converters/`
- Serialization facade (already exists): `include/dwarfs/metadata/serialization/`
- SerializerRegistry (already exists): `src/metadata/serialization/serializer_registry.cpp`

### Optional Dependencies
- Thrift/Folly (optional): For Thrift strategy
- FlatBuffers (required): Header-only, always available via FetchContent

## Testing Strategy

### Unit Tests
- Test each strategy implementation independently
- Test domain model conversions (both directions)
- Test factory pattern (format selection)

### Integration Tests
- Test full workflow: create → mount → extract
- Test both formats
- Test format compatibility (can read old images)

### Performance Tests
- Benchmark read operations
- Benchmark write operations
- Compare FlatBuffers vs Thrift
- Memory usage comparison

### Compatibility Tests
- Test old format images still work
- Test new format images work
- Test cross-format compatibility where applicable

## Documentation

### Code Documentation
- Doxygen comments for all interfaces
- Usage examples in headers
- Architecture diagrams in markdown

### User Documentation
- Update mkdwarfs.md with new format option
- Update dwarfs-format.md with architecture details
- Migration guide for library users

## Conclusion

This architecture provides:

1. **Complete separation** of Thrift and FlatBuffers code
2. **Clear interfaces** using domain model types only
3. **Proven pattern** - reader already works this way
4. **Incremental migration** - safe, phased approach
5. **Testable design** - validation at each phase
6. **Future extensibility** - easy to add new formats

**Status**: Ready for Phase 1 implementation (Interface Design)

**Next Step**: Begin Phase 1 - Create abstract interfaces

---

**References**:
- Main architecture doc: `doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`
- Implementation roadmap: `doc/METADATA_STRATEGY_PATTERN_ROADMAP.md`
- Previous attempts: `doc/WRITER_DOMAIN_MODEL_REFACTOR.md`
- Project architecture: `.kilocode/rules/memory-bank/architecture.md`