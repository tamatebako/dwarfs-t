# DwarFS Writer Thrift Generalization - Complete OOP Architecture Plan

## Executive Summary

**Goal**: Remove hard dependency on Apache Thrift throughout DwarFS codebase by introducing abstract interfaces with concrete Thrift and FlatBuffers implementations.

**Scope**: 148+ locations using `thrift::metadata::` or `#ifdef DWARFS_HAVE_THRIFT`

**Pattern**: Follow existing **reader architecture** (metadata_v2_thrift / metadata_v2_flatbuffers) which already implements this OOP pattern successfully.

---

## Current State Analysis

### Thrift Dependencies by Component

From codebase analysis (148 occurrences):

1. **Writer Core** (CRITICAL - blocks mkdwarfs):
   - `metadata_builder.cpp` (1,288 lines) - 100% Thrift-dependent
   - `global_entry_data.cpp` - Thrift types
   - `inode_manager.cpp` - Thrift types
   - `entry.cpp` - `pack()` methods use Thrift
   - `metadata_freezer.cpp` - Thrift frozen serialization

2. **Reader Core** (✅ ALREADY GENERALIZED):
   - `metadata_v2_thrift.cpp` - Thrift implementation
   - `metadata_v2_flatbuffers.cpp` - FlatBuffers implementation
   - Both implement same interface via `metadata_v2.h`

3. **Utility Components**:
   - `rewrite_filesystem.cpp` - Uses Thrift metadata
   - `history.cpp` - Thrift history format

4. **Metadata Subsystem**:
   - `cpp_thrift_converter.cpp` - Domain ↔ Thrift conversion
   - `thrift_compact_serializer.cpp` - Thrift serialization
   - `serialization_facade.cpp` - Has Thrift → bytes

5. **Internal Utils**:
   - `features.cpp` - Thrift enum utilities
   - `metadata_utils.cpp` - Thrift-specific helpers
   - `string_table.cpp` - Thrift string table

---

## Architectural Principles

### 1. **Separation of Concerns**
- Format-specific code in separate implementations
- Business logic format-agnostic

### 2. **MECE (Mutually Exclusive, Collectively Exhaustive)**
- Thrift implementation when `DWARFS_HAVE_THRIFT`
- FlatBuffers implementation always available (required)
- No overlap between implementations

### 3. **DRY (Don't Repeat Yourself)**
- Shared interface for common operations
- Format conversion done once in converters

### 4. **Open/Closed Principle**
- Open for extension (new formats)
- Closed for modification (existing code)

### 5. **Dependency Inversion**
- Depend on abstractions, not concrete types
- Inject dependencies via factories

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                  Application Layer                           │
│              (Scanner, Tools, Utilities)                     │
└──────────────────────────┬──────────────────────────────────┘
                           │ Uses abstract interfaces
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                  Abstract Interfaces                         │
│  ┌──────────────┐  ┌─────────────┐  ┌──────────────┐       │
│  │ metadata_    │  │ global_     │  │ inode_       │       │
│  │ builder      │  │ entry_data  │  │ manager      │       │
│  └──────────────┘  └─────────────┘  └──────────────┘       │
└──────────────────────────┬──────────────────────────────────┘
                           │ Implemented by
          ┌────────────────┴────────────────┐
          ▼                                  ▼
┌──────────────────────┐        ┌──────────────────────────┐
│  Thrift Impl         │        │  FlatBuffers Impl        │
│  (legacy/optional)   │        │  (modern/required)       │
│                      │        │                          │
│  Uses:               │        │  Uses:                   │
│  thrift::metadata::  │        │  metadata::domain::      │
│  metadata            │        │  metadata                │
└──────────────────────┘        └──────────────────────────┘
          │                                  │
          └──────────────┬───────────────────┘
                         ▼
              ┌──────────────────────┐
              │   Domain Model       │
              │ metadata::domain::*  │
              └──────────────────────┘
```

---

## Component Refactoring Details

### 1. metadata_builder

#### Current State
```cpp
// src/writer/internal/metadata_builder.cpp
#ifdef DWARFS_HAVE_THRIFT
class metadata_builder_ {
  thrift::metadata::metadata md_;  // Direct Thrift dependency

  void set_devices(std::vector<uint64_t> devices);
  void gather_chunks(inode_manager const&, block_manager const&, size_t);
  void gather_entries(std::span<dir*>, global_entry_data const&, uint32_t);
  thrift::metadata::metadata const& build();
};
#endif
```

**Problem**: Entire class wrapped in `#ifdef DWARFS_HAVE_THRIFT`, preventing compilation without Thrift.

#### Proposed Architecture

**Abstract Interface** (`include/dwarfs/writer/internal/metadata_builder.h`):
```cpp
namespace dwarfs::writer::internal {

class metadata_builder {
public:
  virtual ~metadata_builder() = default;

  // Device management
  virtual void set_devices(std::vector<uint64_t> devices) = 0;
  virtual void set_symlink_table_size(size_t size) = 0;
  virtual void set_block_size(uint32_t block_size) = 0;

  // Shared files
  virtual void set_shared_files_table(std::vector<uint32_t> shared_files) = 0;

  // Categories
  virtual void set_category_names(std::vector<std::string> category_names) = 0;
  virtual void set_block_categories(std::vector<uint32_t> block_categories) = 0;
  virtual void set_category_metadata_json(std::vector<std::string> metadata_json) = 0;
  virtual void set_block_category_metadata(std::map<uint32_t, uint32_t> block_metadata) = 0;

  // Symlink table
  virtual void add_symlink_table_entry(size_t index, uint32_t entry) = 0;

  // Data gathering
  virtual void gather_chunks(inode_manager const& im,
                            block_manager const& bm,
                            size_t chunk_count) = 0;

  virtual void gather_entries(std::span<dir*> dirs,
                             global_entry_data const& ge_data,
                             uint32_t num_inodes) = 0;

  virtual void gather_global_entry_data(global_entry_data const& ge_data) = 0;

  // Block remapping
  virtual void remap_blocks(std::span<block_mapping const> mapping,
                           size_t new_block_count) = 0;

  // Build final metadata (returns domain model)
  virtual metadata::domain::metadata build() = 0;

  // Factory methods
  static std::unique_ptr<metadata_builder> create(
    logger& lgr,
    metadata_options const& options,
    metadata::serialization::SerializationFormat format);

  static std::unique_ptr<metadata_builder> create_from_existing(
    logger& lgr,
    metadata::domain::metadata const& existing_md,
    fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options,
    metadata::serialization::SerializationFormat format);
};

} // namespace dwarfs::writer::internal
```

**Thrift Implementation** (`src/writer/internal/metadata_builder_thrift.cpp`):
```cpp
#ifdef DWARFS_HAVE_THRIFT

namespace dwarfs::writer::internal {

template <typename LoggerPolicy>
class metadata_builder_thrift final : public metadata_builder {
public:
  metadata_builder_thrift(logger& lgr, metadata_options const& options);

  metadata_builder_thrift(logger& lgr,
                         thrift::metadata::metadata&& md,
                         thrift::metadata::fs_options const* orig_fs_options,
                         filesystem_version const& orig_fs_version,
                         metadata_options const& options);

  ~metadata_builder_thrift() override = default;

  // Implement all virtual methods
  void set_devices(std::vector<uint64_t> devices) override;
  void set_block_size(uint32_t block_size) override;
  // ... all other methods ...

  metadata::domain::metadata build() override {
    // Build Thrift metadata
    auto const& thrift_md = build_thrift();

    // Convert to domain model
    return metadata::converters::from_thrift(thrift_md);
  }

private:
  thrift::metadata::metadata md_;  // Internal Thrift representation

  thrift::metadata::metadata const& build_thrift();

  // Move all existing implementation here
  void remap_holes(chunks_t& new_chunks, size_t new_hole_index, size_t max_data_chunk_size);
  void upgrade_metadata(...);
  void upgrade_from_pre_v2_2();
  void update_inodes();
  void update_nlink();
  void update_totals_and_size_cache();
  void apply_chmod();
};

} // namespace

#endif // DWARFS_HAVE_THRIFT
```

**FlatBuffers Implementation** (`src/writer/internal/metadata_builder_flatbuffers.cpp`):
```cpp
namespace dwarfs::writer::internal {

template <typename LoggerPolicy>
class metadata_builder_flatbuffers final : public metadata_builder {
public:
  metadata_builder_flatbuffers(logger& lgr, metadata_options const& options);

  metadata_builder_flatbuffers(logger& lgr,
                               metadata::domain::metadata const& md,
                               fs_options const* orig_fs_options,
                               filesystem_version const& orig_fs_version,
                               metadata_options const& options);

  ~metadata_builder_flatbuffers() override = default;

  // Implement all virtual methods
  void set_devices(std::vector<uint64_t> devices) override {
    md_.devices = std::move(devices);
  }

  void gather_chunks(inode_manager const& im, block_manager const& bm,
                    size_t chunk_count) override;

  // ... all other methods ...

  metadata::domain::metadata build() override {
    // Apply final transformations
    update_nlink();
    update_totals_and_size_cache();
    pack_metadata();

    return std::move(md_);  // Return domain model directly
  }

private:
  metadata::domain::metadata md_;  // Direct domain model

  // Helper methods (similar structure to Thrift impl)
  void update_inodes();
  void update_nlink();
  void update_totals_and_size_cache();
  void pack_metadata();
  void remap_holes(...);
};

} // namespace
```

**Factory Implementation** (`src/writer/internal/metadata_builder_factory.cpp`):
```cpp
namespace dwarfs::writer::internal {

std::unique_ptr<metadata_builder>
metadata_builder::create(
    logger& lgr,
    metadata_options const& options,
    metadata::serialization::SerializationFormat format) {

  using namespace metadata::serialization;

  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT:
      return make_unique_logging_object<metadata_builder,
                                       metadata_builder_thrift,
                                       logger_policies>(lgr, options);
#endif

    case SerializationFormat::FLATBUFFERS:
      return make_unique_logging_object<metadata_builder,
                                       metadata_builder_flatbuffers,
                                       logger_policies>(lgr, options);

    default:
      throw std::runtime_error("Unsupported metadata format");
  }
}

std::unique_ptr<metadata_builder>
metadata_builder::create_from_existing(
    logger& lgr,
    metadata::domain::metadata const& existing_md,
    fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options,
    metadata::serialization::SerializationFormat format) {

  using namespace metadata::serialization;

  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT:
      // Convert domain model to Thrift
      auto thrift_md = metadata::converters::to_thrift(existing_md);
      return make_unique_logging_object<metadata_builder,
                                       metadata_builder_thrift,
                                       logger_policies>(
                                         lgr, std::move(thrift_md),
                                         orig_fs_options, orig_fs_version, options);
#endif

    case SerializationFormat::FLATBUFFERS:
      return make_unique_logging_object<metadata_builder,
                                       metadata_builder_flat buffers,
                                       logger_policies>(
                                         lgr, existing_md,
                                         orig_fs_options, orig_fs_version, options);

    default:
      throw std::runtime_error("Unsupported metadata format");
  }
}

} // namespace
```

---

### 2. global_entry_data

#### Current State
```cpp
// Implicitly uses Thrift types in pack methods
class global_entry_data {
  void pack_inode_stat(thrift::metadata::inode_data& inode, ...);
  // ... other Thrift-dependent methods
};
```

#### Proposed Architecture

**Abstract Interface** (`include/dwarfs/writer/internal/global_entry_data.h`):
```cpp
namespace dwarfs::writer::internal {

class global_entry_data {
public:
  virtual ~global_entry_data() = default;

  // Data collection
  virtual void add_uid(uid_type uid) = 0;
  virtual void add_gid(gid_type gid) = 0;
  virtual void add_mode(mode_type mode) = 0;
  virtual void add_mtime(uint64_t mtime) = 0;
  virtual void add_atime(uint64_t atime) = 0;
  virtual void add_ctime(uint64_t ctime) = 0;

  // Indexing
  virtual void index() = 0;

  // Packing (format-agnostic - uses domain model)
  virtual void pack_inode_stat(
    metadata::domain::inode_data& inode,
    file_stat const& st,
    time_resolution_converter const& timeres) const = 0;

  // Retrieval
  virtual uint32_t get_symlink_table_entry(std::string const& link) const = 0;
  virtual std::vector<std::string> get_names() const = 0;
  virtual std::vector<std::string> get_symlinks() const = 0;
  virtual std::vector<uid_type> get_uids() const = 0;
  virtual std::vector<gid_type> get_gids() const = 0;
  virtual std::vector<mode_type> get_modes() const = 0;
  virtual uint64_t get_timestamp_base() const = 0;

  // Factory
  static std::unique_ptr<global_entry_data> create(
    metadata_options const& options);
};

} // namespace
```

**Implementation**: Single implementation (no format dependency):
```cpp
// src/writer/internal/global_entry_data.cpp
namespace dwarfs::writer::internal {

class global_entry_data_impl final : public global_entry_data {
public:
  explicit global_entry_data_impl(metadata_options const& options);

  void add_uid(uid_type uid) override;
  void add_gid(gid_type gid) override;
  // ... implement all methods ...

  void pack_inode_stat(
    metadata::domain::inode_data& inode,
    file_stat const& st,
    time_resolution_converter const& timeres) const override {

    // Pack using domain model (format-agnostic)
    inode.mode_index = get_mode_index(st.mode());
    inode.owner_index = get_uid_index(st.uid());
    inode.group_index = get_gid_index(st.gid());
    inode.mtime_offset = timeres.convert_offset(st.mtime());
    // ...
  }

private:
  // Internal data structures (format-independent)
  std::unordered_map<uid_type, uint32_t> uid_map_;
  std::unordered_map<gid_type, uint32_t> gid_map_;
  std::unordered_map<mode_type, uint32_t> mode_map_;
  // ...
};

std::unique_ptr<global_entry_data>
global_entry_data::create(metadata_options const& options) {
  return std::make_unique<global_entry_data_impl>(options);
}

} // namespace
```

---

### 3. inode_manager

#### Current State
```cpp
// Header uses format-agnostic types, but implementation may depend on Thrift
class inode_manager {
  // ... methods ...
};
```

#### Proposed Architecture

**Interface** (likely already format-agnostic, verify):
```cpp
namespace dwarfs::writer::internal {

class inode_manager {
public:
  inode_manager(logger& lgr, progress& prog,
                std::filesystem::path const& root,
                inode_options const& options);

  // All methods work with inode abstraction (not format-specific)
  void add(std::shared_ptr<inode> const& ino);
  size_t count() const;
  void for_each_inode_in_order(std::function<void(std::shared_ptr<inode> const&)> func) const;
  // ...
};

} // namespace
```

**Note**: inode_manager likely does NOT need refactoring if it only uses `inode` abstraction. Verify during implementation.

---

### 4. entry (dir/file classes)

#### Current State
```cpp
// src/writer/internal/entry.cpp
#ifdef DWARFS_HAVE_THRIFT
void dir::pack_entry(thrift::metadata::metadata& mv2, ...);
void dir::pack(thrift::metadata::metadata& mv2, ...);
#endif
```

#### Proposed Architecture

**Interface** (`include/dwarfs/writer/internal/entry.h`):
```cpp
namespace dwarfs::writer::internal {

class entry {
protected:
  // Pack to domain model (format-agnostic)
  virtual void pack_entry(metadata::domain::metadata& md,
                         global_entry_data const& data,
                         time_resolution_converter const& timeres) = 0;

  virtual void pack(metadata::domain::metadata& md,
                   global_entry_data const& data,
                   time_resolution_converter const& timeres) const = 0;
};

class dir : public entry {
public:
  void pack_entry(metadata::domain::metadata& md,
                 global_entry_data const& data,
                 time_resolution_converter const& timeres) override;

  void pack(metadata::domain::metadata& md,
           global_entry_data const& data,
           time_resolution_converter const& timeres) const override;
};

} // namespace
```

**Implementation** (`src/writer/internal/entry.cpp`):
```cpp
// Remove #ifdef DWARFS_HAVE_THRIFT guards
void dir::pack_entry(metadata::domain::metadata& md,
                    global_entry_data const& data,
                    time_resolution_converter const& timeres) {
  auto& de = md.dir_entries->emplace_back();
  de.name_index = data.get_name_index(name());
  de.inode_num = inode_->num();
}

void dir::pack(metadata::domain::metadata& md,
              global_entry_data const& data,
              time_resolution_converter const& timeres) const {
  metadata::domain::directory d;
  d.parent_entry = has_parent() ? parent_entry() : 0;
  d.first_entry = entry_index_;
  d.self_entry = ent_index_;
  md.directories->push_back(d);
}
```

---

### 5. metadata_freezer

#### Current State
```cpp
#ifdef DWARFS_HAVE_THRIFT
class metadata_freezer_ {
  std::pair<shared_byte_buffer, shared_byte_buffer>
  freeze(thrift::metadata::metadata const& data) const;
};
#endif
```

#### Proposed Architecture

**Abstract Interface** (`include/dwarfs/writer/internal/metadata_freezer.h`):
```cpp
namespace dwarfs::writer::internal {

class metadata_freezer {
public:
  virtual ~metadata_freezer() = default;

  // Freeze domain model to bytes
  virtual std::pair<shared_byte_buffer, shared_byte_buffer>
  freeze(metadata::domain::metadata const& md) const = 0;

  // Factory
  static std::unique_ptr<metadata_freezer> create(
    logger& lgr,
    metadata::serialization::SerializationFormat format);
};

} // namespace
```

**Implementations**:
```cpp
// Thrift implementation
#ifdef DWARFS_HAVE_THRIFT
class metadata_freezer_thrift final : public metadata_freezer {
  std::pair<shared_byte_buffer, shared_byte_buffer>
  freeze(metadata::domain::metadata const& md) const override {
    // Convert domain → Thrift
    auto thrift_md = metadata::converters::to_thrift(md);

    // Freeze using Thrift frozen format
    return freeze_thrift(thrift_md);
  }
};
#endif

// FlatBuffers implementation (always available)
class metadata_freezer_flatbuffers final : public metadata_freezer {
  std::pair<shared_byte_buffer, shared_byte_buffer>
  freeze(metadata::domain::metadata const& md) const override {
    // Serialize using FlatBuffers serializer
    auto serializer = metadata::serialization::FacadeFactory::create(
      metadata::serialization::SerializationFormat::FLATBUFFERS);

    auto bytes = serializer->serialize_from_domain(md);

    // FlatBuffers is self-describing, no separate schema
    return {shared_byte_buffer(std::move(bytes)), shared_byte_buffer()};
  }
};
```

---

## Scanner Integration

### Before (Tightly Coupled)
```cpp
// src/writer/scanner.cpp
void scanner::scan(...) {
  #ifdef DWARFS_HAVE_THRIFT
  metadata_builder mb(lgr_, options.metadata);
  #endif

  // ... scanning logic ...

  #ifdef DWARFS_HAVE_THRIFT
  auto const& md = mb.build();
  #endif
}
```

### After (Dependency Injection)
```cpp
// src/writer/scanner.cpp
void scanner::scan(...) {
  // Create via factory with chosen format
  auto mb = metadata_builder::create(
    lgr_,
    options.metadata,
    options.metadata_format);  // From metadata_options

  // ... scanning logic (unchanged) ...

  // Build returns domain model (format-agnostic)
  auto md = mb->build();

  // Pass to writer
  fsw.write_metadata(std::move(md));
}
```

---

## File Structure

### New Files to Create

```
include/dwarfs/writer/internal/
  metadata_builder.h                    # Abstract interface (NEW)
  global_entry_data.h                   # Already exists, may need updates

src/writer/internal/
  metadata_builder_thrift.cpp           # Move from metadata_builder.cpp (NEW)
  metadata_builder_flatbuffers.cpp      # FlatBuffers impl (NEW)
  metadata_builder_factory.cpp          # Factory (NEW)
  metadata_freezer_flatbuffers.cpp      # FlatBuffers freezer (NEW)
  global_entry_data.cpp                 # Update to use domain model
  entry.cpp                             # Update to use domain model
```

### Files to Modify

```
src/writer/scanner.cpp                  # Use factory for metadata_builder
src/writer/filesystem_writer.cpp        # Accept domain model
src/utility/rewrite_filesystem.cpp      # Update for new architecture
```

### Files to Remove (Eventually)

```
src/writer/internal/metadata_builder.cpp  # Split into _thrift/_flatbuffers
```

---

## Implementation Strategy

### Phase 1: Abstract Interfaces (Week 1)
1. Create `metadata_builder.h` with abstract interface
2. Create `metadata_freezer.h` with abstract interface
3. Define all pure virtual methods based on existing API

### Phase 2: Thrift Implementation (Week 2)
1. Create `metadata_builder_thrift.cpp`
2. Move existing code from `metadata_builder.cpp`
3. Wrap in template with logger policy
4. Guard with `#ifdef DWARFS_HAVE_THRIFT`
5. Return domain model from `build()`

### Phase 3: FlatBuffers Implementation (Week 3)
1. Create `metadata_builder_flatbuffers.cpp`
2. Implement using `metadata::domain::metadata` directly
3. No Thrift conversion needed
4. Always available (required)

### Phase 4: Factory & Integration (Week 4)
1. Create `metadata_builder_factory.cpp`
2. Update `scanner.cpp` to use factory
3. Update `filesystem_writer.cpp` to accept domain model
4. Update `rewrite_filesystem.cpp`

### Phase 5: Supporting Components (Week 5)
1. Update `global_entry_data` to use domain model
2. Update `entry.cpp` pack methods
3. Implement `metadata_freezer_flatbuffers`
4. Update all call sites

### Phase 6: Testing & Validation (Week 6)
1. Build with Thrift enabled - verify compatibility
2. Build with Thrift disabled - verify FlatBuffers works
3. Run full test suite
4. Performance benchmarks (Thrift vs FlatBuffers)

---

## Success Criteria

### Build System
- ✅ `cmake -DDWARFS_WITH_THRIFT=OFF` successfully builds ALL tools
- ✅ `cmake -DDWARFS_WITH_THRIFT=ON` successfully builds ALL tools
- ✅ Both configurations pass all tests

### Functionality
- ✅ mkdwarfs creates valid filesystems with FlatBuffers format
- ✅ mkdwarfs creates valid filesystems with Thrift format
- ✅ dwarfs can mount both formats
- ✅ dwarfsck can check both formats
- ✅ dwarfsextract can extract from both formats

### Performance
- ✅ FlatBuffers performance within 10% of Thrift
- ✅ No memory leaks in either implementation
- ✅ File size comparable between formats

### Code Quality
- ✅ No `#ifdef DWARFS_HAVE_THRIFT` in public headers
- ✅ No direct use of `thrift::metadata::` in writer logic
- ✅ All format-specific code isolated in implementations
- ✅ Domain model serves as single source of truth

---

## Risk Mitigation

### Risk 1: Complex Migration
**Mitigation**: Phased approach, one component at a time

### Risk 2: Performance Regression
**Mitigation**: Benchmark at each phase, optimize if needed

### Risk 3: Breaking Changes
**Mitigation**: Keep Thrift implementation as-is initially, test extensively

### Risk 4: Time Estimation
**Mitigation**: 6-week timeline is aggressive but achievable with focused effort

---

## Dependencies

### Required Skills
- C++20 features (concepts, templates)
- OOP design patterns (Factory, Strategy, Bridge)
- Understanding of serialization formats
- DwarFS codebase knowledge

### Required Tools
- C++ compiler with C++20 support
- CMake ≥3.28
- FlatBuffers library (via FetchContent)
- Optional: Apache Thrift + Folly

---

## Conclusion

This architecture follows proven OOP principles and mirrors the existing reader implementation. By introducing abstract interfaces and concrete implementations, we achieve:

1. ✅ **Format independence** - Business logic decoupled from serialization
2. ✅ **Build flexibility** - Can build without Thrift
3. ✅ **Extensibility** - Easy to add new formats
4. ✅ **Maintainability** - Clear separation of concerns
5. ✅ **Testability** - Can mock interfaces for testing

The domain model (`metadata::domain::metadata`) serves as the universal interchange format, eliminating the need for format-specific code in the writer logic.

**Estimated Effort**: 6 weeks full-time or 12 weeks part-time
**Complexity**: High (148+ touch points, 1,288+ lines in metadata_builder alone)
**Priority**: Critical (blocks FlatBuffers-only builds)