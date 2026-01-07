# DwarFS Metadata Architecture - Strategy Pattern Design

## Executive Summary

This document describes the high-level metadata architecture for DwarFS using the **Strategy Pattern** with **Dependency Inversion Principle (DIP)**. The goal is to eliminate hard dependencies on Apache Thrift throughout the codebase while maintaining backward compatibility.

**Key Principle**: Depend on abstractions, not concretions.

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
                  │                  │
                  │ metadata::domain │
                  │   ::metadata     │
                  └──────────────────┘
```

---

## Architecture Principles

### 1. Dependency Inversion Principle (DIP)
**High-level modules should not depend on low-level modules. Both should depend on abstractions.**

❌ **WRONG** (Current):
```cpp
// Writer depends directly on Thrift types
#ifdef DWARFS_HAVE_THRIFT
class metadata_builder {
  thrift::metadata::metadata md_;  // Direct dependency on Thrift
  thrift::metadata::metadata const& build();
};
#endif
```

✅ **CORRECT** (Strategy Pattern):
```cpp
// Writer depends on abstraction
class metadata_builder {
  virtual ~metadata_builder() = default;

  // ALL methods work with domain model
  virtual void gather_chunks(...) = 0;
  virtual metadata::domain::metadata build() = 0;  // Returns domain model

  // Factory creates concrete implementation
  static unique_ptr<metadata_builder> create(SerializationFormat format);
};
```

### 2. Strategy Pattern
**Define a family of algorithms, encapsulate each one, and make them interchangeable.**

```cpp
// Strategy interface
class metadata_provider {
  virtual ~metadata_provider() = default;
  virtual chunk get_chunk(inode_num, offset) = 0;
  virtual directory get_directory(inode_num) = 0;
  // ... all read operations
};

// Concrete strategies
class thrift_metadata_provider : public metadata_provider { ... };
class flatbuffers_metadata_provider : public metadata_provider { ... };

// Context uses strategy via interface
class filesystem_v2 {
  unique_ptr<metadata_provider> provider_;

  filesystem_v2(unique_ptr<metadata_provider> provider)
    : provider_(std::move(provider)) {}
};
```

### 3. Open/Closed Principle
**Open for extension, closed for modification.**

Adding a new serialization format:
```cpp
// New format: Protobuf (hypothetical)
class protobuf_metadata_provider : public metadata_provider {
  // Implement all virtual methods
};

class protobuf_metadata_builder : public metadata_builder {
  // Implement all virtual methods
};

// No changes needed to filesystem_v2, scanner, or any other component!
```

### 4. Single Responsibility Principle
- **Domain Model**: Data structures only (no serialization logic)
- **Serializers**: Format-specific serialization/deserialization
- **Providers**: Read operations from serialized data
- **Builders**: Write operations to build domain model
- **Converters**: Domain ↔ Format type conversion

### 5. Interface Segregation Principle
Separate interfaces for read and write operations:
```cpp
// Read interface (used by FUSE driver, dwarfsck, dwarfsextract)
class metadata_provider { ... };

// Write interface (used by mkdwarfs, rewriter)
class metadata_builder { ... };
```

---

## Component Architecture

### 1. Reader Architecture (metadata_provider)

```
┌─────────────────────────────────────────────────────────────┐
│              metadata_provider_interface.h                   │
│  (Pure Abstract Interface - Format Agnostic)                 │
│                                                              │
│  virtual chunk get_chunk(inode, offset) = 0;               │
│  virtual directory get_directory(inode) = 0;               │
│  virtual symlink get_symlink(inode) = 0;                   │
│  virtual void iterate_entries(dir, callback) = 0;          │
│  virtual inode_data get_inode(inode_num) = 0;              │
│  virtual feature_set get_features() = 0;                    │
│  virtual fs_options get_options() = 0;                      │
│                                                              │
│  static unique_ptr<metadata_provider>                        │
│    create(SerializationFormat, data);                        │
└──────────────────────────┬──────────────────────────────────┘
                           │
             ┌─────────────┴─────────────┐
             │                           │
             ▼                           ▼
┌────────────────────────┐    ┌────────────────────────┐
│ thrift_metadata_       │    │ flatbuffers_metadata_  │
│ provider.cpp           │    │ provider.cpp           │
│                        │    │                        │
│ #ifdef DWARFS_HAVE_    │    │ (Always available)     │
│ THRIFT                 │    │                        │
│                        │    │                        │
│ Uses:                  │    │ Uses:                  │
│ - Frozen2 access       │    │ - Zero-copy access     │
│ - thrift types         │    │ - FlatBuffer tables    │
│                        │    │                        │
│ Converts to domain     │    │ Converts to domain     │
│ model on-demand        │    │ model on-demand        │
└────────────────────────┘    └────────────────────────┘
```

**Key Methods:**
```cpp
namespace dwarfs::reader {

class metadata_provider {
public:
  virtual ~metadata_provider() = default;

  // Chunk operations
  virtual metadata::domain::chunk get_chunk(uint32_t inode, size_t offset) = 0;
  virtual std::span<metadata::domain::chunk> get_chunks(uint32_t inode) = 0;

  // Directory operations
  virtual metadata::domain::directory get_directory(uint32_t inode) = 0;
  virtual void iterate_entries(
    uint32_t dir_inode,
    std::function<void(metadata::domain::dir_entry const&)> callback) = 0;

  // Inode operations
  virtual metadata::domain::inode_data get_inode(uint32_t inode_num) = 0;
  virtual uint32_t get_inode_count() = 0;

  // Symlink operations
  virtual std::string get_symlink(uint32_t inode) = 0;

  // Filesystem metadata
  virtual metadata::domain::fs_options get_options() = 0;
  virtual metadata::domain::feature_set get_features() = 0;
  virtual uint64_t get_timestamp_base() = 0;
  virtual uint32_t get_block_size() = 0;

  // Lookup tables
  virtual uint32_t get_uid(uint32_t index) = 0;
  virtual uint32_t get_gid(uint32_t index) = 0;
  virtual uint32_t get_mode(uint32_t index) = 0;
  virtual std::string_view get_name(uint32_t index) = 0;

  // Factory
  static std::unique_ptr<metadata_provider> create(
    SerializationFormat format,
    std::span<uint8_t const> data);
};

} // namespace dwarfs::reader
```

### 2. Writer Architecture (metadata_builder)

```
┌─────────────────────────────────────────────────────────────┐
│              metadata_builder_interface.h                    │
│  (Pure Abstract Interface - Format Agnostic)                 │
│                                                              │
│  virtual void set_block_size(uint32_t) = 0;                │
│  virtual void gather_chunks(...) = 0;                       │
│  virtual void gather_entries(...) = 0;                      │
│  virtual metadata::domain::metadata build() = 0;            │
│                                                              │
│  static unique_ptr<metadata_builder>                         │
│    create(SerializationFormat, options);                     │
└──────────────────────────┬──────────────────────────────────┘
                           │
             ┌─────────────┴─────────────┐
             │                           │
             ▼                           ▼
┌────────────────────────┐    ┌────────────────────────┐
│ thrift_metadata_       │    │ flatbuffers_metadata_  │
│ builder.cpp            │    │ builder.cpp            │
│                        │    │                        │
│ #ifdef DWARFS_HAVE_    │    │ (Always available)     │
│ THRIFT                 │    │                        │
│                        │    │                        │
│ Builds:                │    │ Builds:                │
│ - thrift::metadata::   │    │ - metadata::domain::   │
│   metadata internally  │    │   metadata directly    │
│                        │    │                        │
│ Returns domain model   │    │ Returns domain model   │
│ via conversion         │    │ (no conversion)        │
└────────────────────────┘    └────────────────────────┘
```

**Key Methods:**
```cpp
namespace dwarfs::writer::internal {

class metadata_builder {
public:
  virtual ~metadata_builder() = default;

  // Device management
  virtual void set_devices(std::vector<uint64_t> devices) = 0;
  virtual void set_block_size(uint32_t block_size) = 0;

  // Tables
  virtual void set_symlink_table_size(size_t size) = 0;
  virtual void set_shared_files_table(std::vector<uint32_t>) = 0;
  virtual void add_symlink_table_entry(size_t index, uint32_t entry) = 0;

  // Categories
  virtual void set_category_names(std::vector<std::string>) = 0;
  virtual void set_block_categories(std::vector<uint32_t>) = 0;
  virtual void set_category_metadata_json(std::vector<std::string>) = 0;
  virtual void set_block_category_metadata(std::map<uint32_t, uint32_t>) = 0;

  // Data gathering
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

  // Block remapping
  virtual void remap_blocks(
    std::span<block_mapping const> mapping,
    size_t new_block_count) = 0;

  // Build final metadata (RETURNS DOMAIN MODEL)
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
};

} // namespace dwarfs::writer::internal
```

### 3. Factory Pattern

```cpp
// Factory implementation
namespace dwarfs::writer::internal {

std::unique_ptr<metadata_builder>
metadata_builder::create(
    logger& lgr,
    metadata_options const& options,
    SerializationFormat format) {

  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT:
      return make_unique_logging_object<
        metadata_builder,
        thrift_metadata_builder,
        logger_policies>(lgr, options);
#endif

    case SerializationFormat::FLATBUFFERS:
      return make_unique_logging_object<
        metadata_builder,
        flatbuffers_metadata_builder,
        logger_policies>(lgr, options);

    default:
      throw std::runtime_error("Unsupported metadata format");
  }
}

} // namespace
```

---

## Domain Model as Universal Interchange

```cpp
// include/dwarfs/metadata/domain/metadata.h
namespace dwarfs::metadata::domain {

/**
 * Format-agnostic metadata representation
 *
 * This is the SINGLE SOURCE OF TRUTH for all metadata operations.
 * All serialization formats must convert to/from this model.
 */
class metadata {
public:
  // Core structures (format-agnostic)
  std::vector<chunk> chunks;
  std::vector<directory> directories;
  std::vector<inode_data> inodes;
  std::vector<dir_entry> dir_entries;

  // Lookup tables
  std::vector<uint32_t> uids;
  std::vector<uint32_t> gids;
  std::vector<uint32_t> modes;
  std::vector<std::string> names;
  std::vector<std::string> symlinks;

  // Optional features (all formats support)
  std::optional<std::vector<uint64_t>> devices;
  std::optional<fs_options> options;
  std::optional<feature_set> features;

  // ... see include/dwarfs/metadata/domain/metadata.h for complete definition
};

} // namespace dwarfs::metadata::domain
```

**Conversion Pattern:**
```cpp
// Thrift → Domain (in thrift_metadata_builder)
auto thrift_md = /* ... build thrift metadata ... */;
return metadata::converters::from_thrift(thrift_md);

// FlatBuffers → Domain (in flatbuffers_metadata_builder)
// No conversion needed - builds domain model directly!
return std::move(md_);
```

---

## Integration Points

### Scanner Integration

**Before** (Tightly Coupled):
```cpp
void scanner::scan(...) {
  #ifdef DWARFS_HAVE_THRIFT
  metadata_builder mb(lgr_, options.metadata);
  #endif

  // ... scanning logic ...

  #ifdef DWARFS_HAVE_THRIFT
  auto const& md = mb.build();  // Returns Thrift type
  #endif
}
```

**After** (Dependency Injection):
```cpp
void scanner::scan(...) {
  // Create via factory with chosen format
  auto mb = metadata_builder::create(
    lgr_,
    options.metadata,
    options.metadata_format);  // From metadata_options

  // ... scanning logic (UNCHANGED) ...

  // Build returns domain model (format-agnostic)
  auto md = mb->build();

  // Pass to writer
  fsw.write_metadata(std::move(md));
}
```

### FUSE Driver Integration

**Before** (Format-Aware):
```cpp
class filesystem_v2 {
  #ifdef DWARFS_HAVE_THRIFT
  thrift::metadata::metadata md_;  // Direct Thrift dependency
  #endif

  void init(std::span<uint8_t> data) {
    // Format detection, then direct Thrift access
  }
};
```

**After** (Strategy Pattern):
```cpp
class filesystem_v2 {
  std::unique_ptr<metadata_provider> provider_;

  void init(std::span<uint8_t> data) {
    // Auto-detect format
    auto format = SerializerRegistry::instance().detect_format(data);

    // Create provider via factory
    provider_ = metadata_provider::create(format, data);
  }

  // All methods use provider interface
  chunk get_chunk(inode, offset) {
    return provider_->get_chunk(inode, offset);
  }
};
```

---

## File Structure

### New Files to Create

```
include/dwarfs/reader/
  metadata_provider.h                  # Abstract reader interface (NEW)

include/dwarfs/writer/internal/
  metadata_builder.h                   # Already exists, but UPDATE to return domain model

src/reader/
  thrift_metadata_provider.cpp         # Thrift implementation (NEW)
  flatbuffers_metadata_provider.cpp    # FlatBuffers implementation (NEW)
  metadata_provider_factory.cpp        # Factory (NEW)

src/writer/internal/
  thrift_metadata_builder.cpp          # Move from metadata_builder.cpp (NEW)
  flatbuffers_metadata_builder.cpp     # FlatBuffers implementation (NEW)
  metadata_builder_factory.cpp         # Factory (NEW)
```

### Files to Modify

```
include/dwarfs/writer/internal/metadata_builder.h
  - Change build() return type: thrift::metadata::metadata → metadata::domain::metadata
  - Remove Thrift forward declarations from public API

src/reader/filesystem_v2.cpp
  - Use metadata_provider interface
  - Remove direct Thrift/FlatBuffers access

src/writer/scanner.cpp
  - Use metadata_builder factory
  - Work with domain model
```

### Files to Remove (Eventually)

```
src/writer/internal/metadata_builder.cpp  # Split into format-specific implementations
```

---

## Serialization Flow

### Write Path (mkdwarfs)

```
Scanner
  ↓
  gather_chunks()
  gather_entries()
  gather_global_entry_data()
  ↓
metadata_builder::build()
  ↓
metadata::domain::metadata  ← Universal format
  ↓
MetadataSerializationFacade::serialize()
  ↓
┌─────────────────┴─────────────────┐
│                                   │
▼                                   ▼
Thrift Serializer             FlatBuffers Serializer
(converts domain→thrift)      (converts domain→flatbuffers)
    ↓                             ↓
Thrift bytes                  FlatBuffers bytes
```

### Read Path (dwarfs FUSE)

```
Filesystem image bytes
  ↓
Format detection (magic bytes or fallback)
  ↓
metadata_provider::create(format, data)
  ↓
┌─────────────────┴─────────────────┐
│                                   │
▼                                   ▼
Thrift Provider               FlatBuffers Provider
(Frozen2 access)              (Zero-copy access)
    ↓                             ↓
On-demand conversion         On-demand conversion
thrift→domain                flatbuffers→domain
    ↓                             ↓
metadata::domain types ← Universal format
  ↓
FUSE operations
```

---

## Implementation Checklist

### Phase 1: Reader Infrastructure ✅ (Already Done)
- [x] Reader already uses format-agnostic pattern
- [x] `metadata_v2_thrift.cpp` and `metadata_v2_flatbuffers.cpp` exist

### Phase 2: Writer Abstract Interface
- [ ] Create `metadata_provider.h` abstract interface
- [ ] Extract interface from current `metadata_builder.h`
- [ ] Change `build()` return type to `metadata::domain::metadata`
- [ ] Remove Thrift types from public API

### Phase 3: Thrift Implementation
- [ ] Create `thrift_metadata_builder.cpp`
- [ ] Move code from `metadata_builder.cpp`
- [ ] Implement conversion: Thrift → domain model in `build()`
- [ ] Guard with `#ifdef DWARFS_HAVE_THRIFT`

### Phase 4: FlatBuffers Implementation
- [ ] Create `flatbuffers_metadata_builder.cpp`
- [ ] Implement building domain model directly
- [ ] No Thrift dependency
- [ ] Always available (required)

### Phase 5: Factory & Integration
- [ ] Create factories for reader and writer
- [ ] Update `scanner.cpp` to use factory
- [ ] Update `filesystem_writer.cpp` to accept domain model
- [ ] Update `rewrite_filesystem.cpp`

### Phase 6: Testing
- [ ] Build with Thrift enabled
- [ ] Build with Thrift disabled (FlatBuffers only)
- [ ] Run full test suite
- [ ] Performance benchmarks

---

## Success Criteria

### Build System
- ✅ `cmake -DDWARFS_WITH_THRIFT=OFF` builds ALL tools
- ✅ `cmake -DDWARFS_WITH_THRIFT=ON` builds ALL tools
- ✅ Both configurations pass all tests

### Functionality
- ✅ mkdwarfs creates valid filesystems (both formats)
- ✅ dwarfs mounts both formats
- ✅ dwarfsck checks both formats
- ✅ dwarfsextract extracts from both formats

### Code Quality
- ✅ No `#ifdef DWARFS_HAVE_THRIFT` in public headers
- ✅ No `thrift::metadata::` in application logic
- ✅ Format-specific code isolated in implementations
- ✅ Domain model as single source of truth

---

## Additional Tasks

### Fix library_dependencies.cpp

**Current Issues:**
1. Missing FlatBuffers version
2. Needs jemalloc version (if using tamatebako fork)

**Fix:**
```cpp
void library_dependencies::add_common_libraries() {
  add_library("libxxhash", ::XXH_versionNumber(),
              version_format::maj_min_patch_dec_100);
  add_library("libfmt", FMT_VERSION, version_format::maj_min_patch_dec_100);
  add_library(fmt::format("crypto-{}", get_crypto_version()));
  add_library("libboost", BOOST_VERSION, version_format::boost);
  add_library("phmap", PHMAP_VERSION_MAJOR, PHMAP_VERSION_MINOR,
              PHMAP_VERSION_PATCH);
#ifdef DWARFS_HAVE_FLATBUFFERS
  add_library("libflatbuffers", FLATBUFFERS_VERSION_MAJOR,
              FLATBUFFERS_VERSION_MINOR, FLATBUFFERS_VERSION_REVISION);
#endif
#ifdef DWARFS_HAVE_JEMALLOC
  // Add jemalloc version if available
  add_library("jemalloc", JEMALLOC_VERSION_MAJOR, JEMALLOC_VERSION_MINOR,
              JEMALLOC_VERSION_BUGFIX);
#endif
#ifdef DWARFS_STACKTRACE_ENABLED
  add_library("cpptrace", CPPTRACE_VERSION_MAJOR, CPPTRACE_VERSION_MINOR,
              CPPTRACE_VERSION_PATCH);
#endif
}
```

### Add ASCII Art for Tebako Fork

In relevant documentation or startup banners, add "T" indicator:

```
 ___________
< DwarFS  T >  (T = Tebako fork)
 -----------
```

---

## Benefits of This Architecture

### 1. **Separation of Concerns**
- Format-specific code confined to implementations
- Business logic format-agnostic
- Easy to understand and maintain

### 2. **Extensibility**
- Add new format = implement interface
- No changes to existing code
- Plugin-style architecture

### 3. **Testability**
- Mock interface for unit tests
- Test each format independently
- Integration tests via factory

### 4. **Flexibility**
- Runtime format selection
- Compile-time optional formats
- Gradual migration path

### 5. **Maintainability**
- Clear responsibilities
- Minimal coupling
- Domain model as documentation

---

## Design Patterns Summary

| Pattern | Component | Purpose |
|---------|-----------|---------|
| **Strategy** | metadata_provider, metadata_builder | Interchangeable serialization formats |
| **Factory** | create() methods | Abstract object creation |
| **Facade** | SerializationFacade | Simplify serialization API |
| **Adapter** | Converters | Translate between formats and domain |
| **PIMPL** | metadata_builder | Hide implementation details |
| **Registry** | SerializerRegistry | Auto-register formats at startup |

---

## Conclusion

This architecture achieves **complete format independence** through proper application of SOLID principles and design patterns. The domain model serves as the universal interchange format, eliminating tight coupling to any specific serialization library.

**Key Takeaway**: By depending on abstractions (interfaces) rather than concretions (Thrift types), we achieve a flexible, extensible, and maintainable metadata subsystem that supports multiple serialization formats without compromising code quality or performance.

**Estimated Effort**: 4-6 weeks for complete implementation
**Complexity**: High (148+ touch points, extensive refactoring)
**Priority**: Critical (blocks FlatBuffers-only builds)
**Risk**: Medium (phased approach mitigates risk)