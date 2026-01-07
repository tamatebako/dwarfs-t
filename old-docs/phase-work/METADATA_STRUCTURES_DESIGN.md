# DwarFS Metadata Structures - Architectural Design

## Executive Summary

This document describes the architectural design for replacing Thrift frozen layout dependencies in DwarFS metadata with plain C++ structures. The design maintains backward compatibility while enabling optional Thrift compilation.

## Table of Contents

1. [Overview](#overview)
2. [Current State Analysis](#current-state-analysis)
3. [Design Goals](#design-goals)
4. [Architecture](#architecture)
5. [Structure Definitions](#structure-definitions)
6. [Metadata Accessor Interface](#metadata-accessor-interface)
7. [Migration Strategy](#migration-strategy)
8. [Phase 2 Recommendations](#phase-2-recommendations)

---

## 1. Overview

### Context

DwarFS currently uses Apache Thrift frozen layouts for metadata serialization with ~50+ files depending on it. The goal is to make Thrift optional while maintaining backward compatibility with existing filesystem images.

### Scope

This design covers:
- Plain C++ structure definitions for all Thrift metadata types
- Abstraction interface for transparent access (frozen vs structured)
- Migration path for `metadata_v2.cpp`
- Serialization-agnostic design (Thrift/Cereal/Bitsery)

---

## 2. Current State Analysis

### Thrift Dependencies

**From metadata.thrift (529 lines):**
- Root structure: `metadata` (36+ fields across versions 2.1-2.5)
- Core structures: `chunk`, `directory`, `inode_data`, `dir_entry`
- Configuration: `fs_options`, `string_table`
- Caching: `inode_size_cache`
- History: `history_entry`

**From other Thrift files:**
- `features.thrift`: `feature` enum (sparsefiles)
- `history.thrift`: `dwarfs_version`, `history_entry`, `history`
- `compression.thrift`: `flac_block_header`, `ricepp_block_header`

### Current Usage Pattern

```cpp
// metadata_v2.cpp line 652
MappedFrozen<thrift::metadata::metadata> meta_;

// Access pattern (line 310)
size_t block_size() const { return meta_.block_size(); }

// Optional field access (line 383)
if (auto catnames = meta_.category_names()) {
  // use catnames.value()
}
```

### Key Observations

1. **Frozen Format**: Uses `MappedFrozen<T>` for zero-copy access
2. **Modern Formats**: Cereal/Bitsery already deserialize to Thrift, then re-freeze (lines 690-760)
3. **Optional Fields**: Heavy use of `std::optional` semantics
4. **Collections**: Uses Thrift `list<T>`, `map<K,V>`, `set<T>`

---

## 3. Design Goals

### Primary Goals

1. **MECE Principle**: Each structure is mutually exclusive and collectively exhaustive
2. **Backward Compatibility**: Support reading existing frozen format images
3. **Zero Dependencies**: Plain C++17, no Thrift/external dependencies
4. **Serialization Agnostic**: Works with Thrift frozen, Cereal, Bitsery, or manual
5. **Memory Efficient**: Appropriate container types, no waste

### Secondary Goals

1. **Type Safety**: Use strong types, avoid raw integers where possible
2. **Clear Semantics**: Self-documenting field names and types
3. **Maintainability**: Easy to extend for future versions
4. **Performance**: Minimal overhead vs frozen layout

---

## 4. Architecture

### High-Level Design

```
┌─────────────────────────────────────────────────────────┐
│                 Application Code                        │
│            (filesystem_v2.cpp, etc.)                    │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
         ┌─────────────────────────────┐
         │   metadata_accessor         │  ◄─── Abstract Interface
         │      (interface)            │
         └──────────┬──────────────────┘
                    │
         ┌──────────┴──────────┐
         │                     │
         ▼                     ▼
┌────────────────┐    ┌────────────────────┐
│frozen_accessor │    │ structured_accessor│
│  (MappedFrozen)│    │  (plain structs)   │
└────────┬───────┘    └─────────┬──────────┘
         │                      │
         ▼                      ▼
┌─────────────────┐    ┌─────────────────────┐
│ Thrift Frozen   │    │ metadata_structures │
│    Layout       │    │   (plain C++)       │
└─────────────────┘    └─────────────────────┘
```

### Component Responsibilities

**metadata_structures.h**
- Plain C++ struct definitions
- No serialization code
- Header-only, namespace `dwarfs::internal`

**metadata_accessor (interface)**
- Abstract access to metadata fields
- Hides frozen vs structured implementation
- Template/concept-based for zero overhead

**frozen_accessor (implementation)**
- Wraps `MappedFrozen<thrift::metadata::metadata>`
- Existing code path
- Minimal changes

**structured_accessor (implementation)**
- Uses plain C++ structures
- Future default path
- Enables Thrift-optional builds

---

## 5. Structure Definitions

### File: `include/dwarfs/internal/metadata_structures.h`

#### Namespace and Type Mappings

```cpp
namespace dwarfs::internal {

// Thrift UInt8 → uint8_t
// Thrift UInt16 → uint16_t
// Thrift UInt32 → uint32_t
// Thrift UInt64 → uint64_t
// Thrift string → std::string
// Thrift binary → std::vector<uint8_t>
// Thrift list<T> → std::vector<T>
// Thrift set<T> → std::unordered_set<T>
// Thrift map<K,V> → std::unordered_map<K,V>
// Thrift optional<T> → std::optional<T>
```

#### Core Structures

**chunk** (3 fields)
```cpp
struct chunk {
  uint32_t block;   // file system block number
  uint32_t offset;  // offset from start of block, in bytes
  uint32_t size;    // size of chunk, in bytes
};
```

**directory** (3 fields)
```cpp
struct directory {
  uint32_t parent_entry;  // indexes into dir_entries
  uint32_t first_entry;   // indexes into dir_entries
  uint32_t self_entry;    // indexes into dir_entries (v2.5+)
};
```

**inode_data** (14 fields)
```cpp
struct inode_data {
  // Core fields (v2.0+)
  uint32_t mode_index;   // index into modes[]
  uint32_t owner_index;  // index into uids[]
  uint32_t group_index;  // index into gids[]
  uint64_t atime_offset; // relative to timestamp_base
  uint64_t mtime_offset; // relative to timestamp_base
  uint64_t ctime_offset; // relative to timestamp_base

  // Extended fields (v2.5+)
  uint64_t btime_offset;     // birth time
  uint64_t atime_subsec;     // subsecond part of atime
  uint64_t mtime_subsec;     // subsecond part of mtime
  uint64_t ctime_subsec;     // subsecond part of ctime
  uint64_t btime_subsec;     // subsecond part of btime
  uint32_t nlink_minus_one;  // number of hard links - 1

  // Deprecated fields (v2.2, kept for compatibility)
  uint32_t name_index_v2_2;  // DEPRECATED: v2.2 only
  uint32_t inode_v2_2;       // DEPRECATED: v2.2 only
};
```

**dir_entry** (2 fields)
```cpp
struct dir_entry {
  uint32_t name_index;  // index into names[]
  uint32_t inode_num;   // index into inodes[]
};
```

**fs_options** (8 fields)
```cpp
struct fs_options {
  bool mtime_only;                                    // only mtime timestamps stored
  std::optional<uint32_t> time_resolution_sec;        // time resolution in seconds
  bool packed_chunk_table;                            // chunk table is delta-compressed
  bool packed_directories;                            // directories are packed
  bool packed_shared_files_table;                     // shared files table is packed
  std::optional<uint32_t> subsecond_resolution_nsec_multiplier;  // v2.5+
  bool has_btime;                                     // v2.5+ birth time included
  bool inodes_have_nlink;                             // v2.5+ nlinks in inode_data
};
```

**string_table** (4 fields)
```cpp
struct string_table {
  std::string buffer;                    // concatenated strings (may be FSST compressed)
  std::optional<std::string> symtab;     // FSST symbol table (if compressed)
  std::vector<uint32_t> index;           // offsets into buffer (may be packed)
  bool packed_index;                     // index is delta-compressed
};
```

**inode_size_cache** (3 fields)
```cpp
struct inode_size_cache {
  std::unordered_map<uint32_t, uint64_t> size_lookup;            // inode → size
  uint64_t min_chunk_count;                                       // cache threshold
  std::unordered_map<uint32_t, uint64_t> allocated_size_lookup;  // v2.5+ sparse files
};
```

**history_entry** (5 fields)
```cpp
struct history_entry {
  uint8_t major;                          // major version
  uint8_t minor;                          // minor version
  std::optional<std::string> dwarfs_version;  // version string
  uint32_t block_size;                    // block size in bytes
  std::optional<fs_options> options;      // filesystem options
};
```

**metadata** (36+ fields - ROOT STRUCTURE)
```cpp
struct metadata {
  // Core metadata (v2.0+)
  std::vector<chunk> chunks;                    // 1: file data chunks
  std::vector<directory> directories;           // 2: directory metadata
  std::vector<inode_data> inodes;              // 3: inode metadata
  std::vector<uint32_t> chunk_table;           // 4: chunk lookup table (may be packed)
  std::vector<uint32_t> entry_table_v2_2;      // 5: DEPRECATED v2.2 only
  std::vector<uint32_t> symlink_table;         // 6: symlink lookup table
  std::vector<uint32_t> uids;                  // 7: user IDs
  std::vector<uint32_t> gids;                  // 8: group IDs
  std::vector<uint32_t> modes;                 // 9: file modes
  std::vector<std::string> names;              // 10: entry names
  std::vector<std::string> symlinks;           // 11: symlink targets
  uint64_t timestamp_base;                     // 12: base timestamp for offsets
  uint32_t block_size;                         // 15: filesystem block size
  uint64_t total_fs_size;                      // 16: total filesystem size

  // Version 2.1+ fields
  std::optional<std::vector<uint64_t>> devices;     // 17: device IDs
  std::optional<fs_options> options;                // 18: filesystem options

  // Version 2.3+ fields
  std::optional<std::vector<dir_entry>> dir_entries;         // 19: all directory entries
  std::optional<std::vector<uint32_t>> shared_files_table;   // 20: shared file mappings (may be packed)
  std::optional<uint64_t> total_hardlink_size;               // 21: DEPRECATED (kept for compat)
  std::optional<std::string> dwarfs_version;                 // 22: version string
  std::optional<uint64_t> create_timestamp;                  // 23: creation timestamp
  std::optional<string_table> compact_names;                 // 24: compressed names
  std::optional<string_table> compact_symlinks;              // 25: compressed symlinks

  // Version 2.5+ fields
  std::optional<uint32_t> preferred_path_separator;          // 26: path separator char
  std::optional<std::unordered_set<std::string>> features;   // 27: enabled features
  std::optional<std::vector<std::string>> category_names;    // 28: block category names
  std::optional<std::vector<uint32_t>> block_categories;     // 29: block → category index
  std::optional<inode_size_cache> reg_file_size_cache;       // 30: highly fragmented file cache
  std::optional<std::vector<std::string>> category_metadata_json;  // 31: category metadata
  std::optional<std::unordered_map<uint32_t, uint32_t>> block_category_metadata;  // 32: block → metadata
  std::optional<std::vector<history_entry>> metadata_version_history;  // 33: version history
  std::optional<uint32_t> hole_block_index;                  // 34: sparse file hole indicator
  std::optional<std::vector<uint64_t>> large_hole_size;      // 35: large sparse holes
  std::optional<uint64_t> total_allocated_fs_size;          // 36: allocated size (with holes)

  // Field 37 reserved (never released but may exist)
};
```

#### Feature Enum

```cpp
enum class feature : uint8_t {
  sparsefiles = 0,  // support for sparse files (v0.14.0)
};
```

#### History Structures

```cpp
// From history.thrift
struct dwarfs_version {
  uint16_t major;
  uint16_t minor;
  uint16_t patch;
  bool is_release;
  std::optional<std::string> git_rev;
  std::optional<std::string> git_branch;
  std::optional<std::string> git_desc;
};

struct history_entry_full {
  dwarfs_version version;
  std::string system_id;
  std::string compiler_id;
  std::optional<std::vector<std::string>> arguments;
  std::optional<uint64_t> timestamp;
  std::optional<std::unordered_set<std::string>> library_versions;
};

struct history {
  std::vector<history_entry_full> entries;
};
```

#### Compression Structures

```cpp
// From compression.thrift
struct flac_block_header {
  uint16_t num_channels;
  uint8_t bits_per_sample;
  uint8_t flags;
};

struct ricepp_block_header {
  uint32_t block_size;
  uint16_t component_count;
  uint8_t bytes_per_sample;
  uint8_t unused_lsb_count;
  bool big_endian;
  uint16_t ricepp_version;
};
```

### Design Principles Applied

1. **MECE**: Each structure is self-contained with clear boundaries
2. **Field Ordering**: Exact match to Thrift definitions for compatibility
3. **Optional Semantics**: `std::optional<T>` for all optional Thrift fields
4. **Type Safety**: Strong types (uint32_t vs uint64_t) maintained
5. **Collections**: Standard C++ containers (vector, unordered_map, unordered_set)

---

## 6. Metadata Accessor Interface

### Design Philosophy

The accessor interface abstracts whether metadata is accessed from frozen layout or plain structures, enabling transparent migration and testing.

### Interface Definition

**File: `include/dwarfs/internal/metadata_accessor.h`**

```cpp
namespace dwarfs::internal {

// Concept for metadata access
template <typename T>
concept MetadataAccessor = requires(T const& t) {
  // Core scalar accessors
  { t.block_size() } -> std::convertible_to<uint32_t>;
  { t.timestamp_base() } -> std::convertible_to<uint64_t>;
  { t.total_fs_size() } -> std::convertible_to<uint64_t>;

  // Collection accessors - return range-like views
  { t.chunks() } -> std::ranges::input_range;
  { t.directories() } -> std::ranges::input_range;
  { t.inodes() } -> std::ranges::input_range;
  { t.chunk_table() } -> std::ranges::input_range;
  { t.symlink_table() } -> std::ranges::input_range;
  { t.uids() } -> std::ranges::input_range;
  { t.gids() } -> std::ranges::input_range;
  { t.modes() } -> std::ranges::input_range;
  { t.names() } -> std::ranges::input_range;
  { t.symlinks() } -> std::ranges::input_range;

  // Optional field accessors - return std::optional or similar
  { t.devices() } -> std::convertible_to<bool>;  // has_value() check
  { t.options() } -> std::convertible_to<bool>;
  { t.dir_entries() } -> std::convertible_to<bool>;
  { t.dwarfs_version() } -> std::convertible_to<bool>;
  { t.features() } -> std::convertible_to<bool>;
  { t.preferred_path_separator() } -> std::convertible_to<bool>;
};

// Frozen accessor implementation (wraps MappedFrozen)
class frozen_metadata_accessor {
public:
  explicit frozen_metadata_accessor(
    apache::thrift::frozen::MappedFrozen<thrift::metadata::metadata> meta)
    : meta_(std::move(meta)) {}

  // Scalar accessors
  uint32_t block_size() const { return meta_.block_size(); }
  uint64_t timestamp_base() const { return meta_.timestamp_base(); }
  uint64_t total_fs_size() const { return meta_.total_fs_size(); }

  // Collection accessors return frozen views
  auto chunks() const { return meta_.chunks(); }
  auto directories() const { return meta_.directories(); }
  auto inodes() const { return meta_.inodes(); }
  auto chunk_table() const { return meta_.chunk_table(); }
  auto symlink_table() const { return meta_.symlink_table(); }
  auto uids() const { return meta_.uids(); }
  auto gids() const { return meta_.gids(); }
  auto modes() const { return meta_.modes(); }
  auto names() const { return meta_.names(); }
  auto symlinks() const { return meta_.symlinks(); }

  // Optional accessors return frozen optionals
  auto devices() const { return meta_.devices(); }
  auto options() const { return meta_.options(); }
  auto dir_entries() const { return meta_.dir_entries(); }
  auto dwarfs_version() const { return meta_.dwarfs_version(); }
  auto features() const { return meta_.features(); }
  auto preferred_path_separator() const { return meta_.preferred_path_separator(); }

  // ... all other fields ...

private:
  apache::thrift::frozen::MappedFrozen<thrift::metadata::metadata> meta_;
};

// Structured accessor implementation (wraps plain C++ structs)
class structured_metadata_accessor {
public:
  explicit structured_metadata_accessor(metadata const& meta)
    : meta_(meta) {}

  // Scalar accessors
  uint32_t block_size() const { return meta_.block_size; }
  uint64_t timestamp_base() const { return meta_.timestamp_base; }
  uint64_t total_fs_size() const { return meta_.total_fs_size; }

  // Collection accessors return spans/ranges
  auto chunks() const {
    return std::span{meta_.chunks};
  }
  auto directories() const {
    return std::span{meta_.directories};
  }
  auto inodes() const {
    return std::span{meta_.inodes};
  }
  auto chunk_table() const {
    return std::span{meta_.chunk_table};
  }
  auto symlink_table() const {
    return std::span{meta_.symlink_table};
  }
  auto uids() const {
    return std::span{meta_.uids};
  }
  auto gids() const {
    return std::span{meta_.gids};
  }
  auto modes() const {
    return std::span{meta_.modes};
  }
  auto names() const {
    return std::span{meta_.names};
  }
  auto symlinks() const {
    return std::span{meta_.symlinks};
  }

  // Optional accessors return std::optional refs
  auto devices() const { return meta_.devices; }
  auto options() const { return meta_.options; }
  auto dir_entries() const { return meta_.dir_entries; }
  auto dwarfs_version() const { return meta_.dwarfs_version; }
  auto features() const { return meta_.features; }
  auto preferred_path_separator() const { return meta_.preferred_path_separator; }

  // ... all other fields ...

private:
  metadata const& meta_;
};

// Type-erased accessor using std::variant
using metadata_accessor_variant = std::variant<
  frozen_metadata_accessor,
  structured_metadata_accessor
>;

// Helper for visiting
template <typename Visitor>
auto visit_metadata(metadata_accessor_variant const& accessor, Visitor&& vis) {
  return std::visit(std::forward<Visitor>(vis), accessor);
}

} // namespace dwarfs::internal
```

### Usage Examples

```cpp
// Using frozen accessor (current code path)
auto frozen_meta = map_frozen<thrift::metadata::metadata>(schema, data);
frozen_metadata_accessor accessor(std::move(frozen_meta));
auto block_size = accessor.block_size();

// Using structured accessor (new code path)
metadata meta = deserialize_from_cereal(data);
structured_metadata_accessor accessor(meta);
auto block_size = accessor.block_size();

// Generic code using concept
template <MetadataAccessor Accessor>
void process_metadata(Accessor const& accessor) {
  for (auto const& chunk : accessor.chunks()) {
    // process chunk
  }
}

// Type-erased variant
metadata_accessor_variant accessor =
  frozen_metadata_accessor(frozen_meta);
visit_metadata(accessor, [](auto const& acc) {
  std::cout << "Block size: " << acc.block_size() << "\n";
});
```

---

## 7. Migration Strategy

### Phase 1: Preparation (Current Phase)

**Goal**: Define structures and interfaces without breaking existing code

**Tasks**:
1. ✅ Create `metadata_structures.h` with all struct definitions
2. ✅ Create `metadata_accessor.h` with interface definitions
3. ✅ Document migration strategy
4. Add unit tests for structure layout compatibility

**Files Modified**: None (new files only)
**Risk**: Low (no existing code changes)

### Phase 2: Abstraction Layer

**Goal**: Introduce accessor interface to `metadata_v2_data`

**Current State** (`metadata_v2_data` class, line 296):
```cpp
class metadata_v2_data {
private:
  MappedFrozen<thrift::metadata::metadata> meta_;  // line 652

  // Direct access throughout
  size_t block_size() const { return meta_.block_size(); }
  bool has_symlinks() const { return !meta_.symlink_table().empty(); }
  // ... 50+ direct accesses to meta_ ...
};
```

**Migration Step 2A: Add Accessor Member**
```cpp
class metadata_v2_data {
private:
  MappedFrozen<thrift::metadata::metadata> meta_;         // Keep for now
  frozen_metadata_accessor accessor_;                      // NEW

public:
  metadata_v2_data(/* ... */)
    : meta_{/* ... */}
    , accessor_{meta_}  // Initialize accessor from frozen
  {}

  // Update methods to use accessor
  size_t block_size() const { return accessor_.block_size(); }
  bool has_symlinks() const { return !accessor_.symlink_table().empty(); }
};
```

**Migration Step 2B: Make Accessor Polymorphic**
```cpp
class metadata_v2_data {
private:
  metadata_accessor_variant accessor_;  // Can be frozen or structured

public:
  metadata_v2_data(/* ... */, bool use_frozen = true) {
    if (use_frozen) {
      auto meta = map_frozen<thrift::metadata::metadata>(schema, data);
      accessor_ = frozen_metadata_accessor(std::move(meta));
    } else {
      metadata meta = deserialize_structured(data);
      accessor_ = structured_metadata_accessor(meta);
    }
  }

  size_t block_size() const {
    return visit_metadata(accessor_,
      [](auto const& acc) { return acc.block_size(); });
  }
};
```

**Files Modified**:
- `src/reader/internal/metadata_v2.cpp` (main changes)
- `include/dwarfs/reader/internal/metadata_v2.h` (interface)

**Risk**: Medium (core metadata access logic)

### Phase 3: Deserialization Support

**Goal**: Add deserialization from modern formats directly to structures

**New File**: `src/metadata/converters/structured_metadata_converter.cpp`

```cpp
namespace dwarfs::metadata::converters {

// Convert Thrift to structured
metadata thrift_to_structured(thrift::metadata::metadata const& thrift_meta) {
  metadata result;

  // Copy core fields
  result.block_size = thrift_meta.block_size();
  result.timestamp_base = thrift_meta.timestamp_base();
  result.total_fs_size = thrift_meta.total_fs_size();

  // Copy collections
  result.chunks.reserve(thrift_meta.chunks().size());
  for (auto const& c : thrift_meta.chunks()) {
    result.chunks.push_back({
      .block = c.block(),
      .offset = c.offset(),
      .size = c.size()
    });
  }

  // ... copy all fields ...

  return result;
}

// Deserialize Cereal directly to structured
metadata deserialize_cereal_to_structured(std::vector<uint8_t> const& data) {
  // Cereal → structured (bypassing Thrift)
  metadata result;
  std::istringstream iss(std::string(data.begin(), data.end()));
  cereal::BinaryInputArchive archive(iss);
  archive(result);
  return result;
}

// Deserialize Bitsery directly to structured
metadata deserialize_bitsery_to_structured(std::vector<uint8_t> const& data) {
  metadata result;
  bitsery::Deserializer<bitsery::InputBufferAdapter<uint8_t>> des(
    data.begin(), data.size());
  des.object(result);
  return result;
}

} // namespace dwarfs::metadata::converters
```

**Files Modified**:
- NEW: `src/metadata/converters/structured_metadata_converter.cpp`
- `src/reader/internal/metadata_v2.cpp` (use new converters)

**Risk**: Medium (new code paths)

### Phase 4: Thrift Optional

**Goal**: Enable building without Thrift dependency

**Build Changes** (`CMakeLists.txt`):
```cmake
option(DWARFS_USE_THRIFT "Enable Thrift frozen metadata support" ON)

if(DWARFS_USE_THRIFT)
  target_compile_definitions(dwarfs PRIVATE DWARFS_HAVE_THRIFT)
  target_link_libraries(dwarfs PRIVATE thrift_frozen)
endif()
```

**Conditional Compilation**:
```cpp
#ifdef DWARFS_HAVE_THRIFT
  // frozen_metadata_accessor available
#else
  // Only structured_metadata_accessor available
#endif
```

**Files Modified**:
- `CMakeLists.txt` (build configuration)
- All files using Thrift types (conditional compilation)

**Risk**: High (build system changes, testing matrix expands)

### Phase 5: Migration Complete

**Goal**: Structured format becomes default, frozen is legacy

**Timeline**: After 6 months of dual-format support
**Deprecation Notice**: Release notes announcing Thrift frozen deprecation
**Default Behavior**: New images serialize as Cereal/Bitsery by default

---

## 8. Phase 2 Recommendations

### Implementation Priority

**High Priority** (Phase 2A):
1. Implement `metadata_structures.h` completely
2. Implement `frozen_metadata_accessor` wrapper
3. Unit tests for structure sizes and alignment
4. Update `metadata_v2_data` to use accessor internally

**Medium Priority** (Phase 2B):
1. Implement `structured_metadata_accessor`
2. Add `thrift_to_structured` converter
3. Add runtime selection (frozen vs structured)
4. Integration tests with existing images

**Low Priority** (Phase 2C):
1. Direct Cereal/Bitsery → structured deserialization
2. Performance comparison frozen vs structured
3. Memory usage profiling
4. Documentation updates

### Testing Strategy

**Unit Tests**:
- Structure layout matches Thrift (size, alignment)
- Accessor interface compatibility
- Conversion correctness (Thrift ↔ structured)

**Integration Tests**:
- Read existing frozen format images
- Write and read structured format images
- Mixed format support (frozen metadata, structured serialization)

**Performance Tests**:
- Access latency (frozen vs structured)
- Memory usage (frozen vs structured)
- Deserialization speed (Thrift vs Cereal vs Bitsery)

**Compatibility Tests**:
- All existing test images still work
- Cross-version compatibility maintained
- Feature flag combinations tested

### Performance Considerations

**Frozen Format Advantages**:
- Zero-copy access (memory mapped)
- Minimal deserialization overhead
- Compact in-memory representation

**Structured Format Advantages**:
- No Thrift dependency
- Standard C++ containers (tooling support)
- Easier debugging and inspection
- Flexible serialization backends

**Mitigation Strategies**:
- Keep frozen as default for read-only access
- Use structured for write operations
- Profile critical paths before/after migration
- Implement lazy loading for large structures

### Documentation Updates

**Required Documentation**:
1. API documentation for new structures
2. Migration guide for downstream users
3. Build system documentation (Thrift optional)
4. Performance characteristics comparison
5. Serialization format specification

**Update Existing Docs**:
- `doc/dwarfs-format.md` (add structured format)
- `README.md` (update build dependencies)
- `CHANGES.md` (migration timeline)

### Risk Mitigation

**Technical Risks**:
1. **Structure Padding**: Ensure consistent layout across compilers
   - Mitigation: Use `#pragma pack` or explicit padding
   - Testing: Size assertions in unit tests

2. **Optional Semantics**: Thrift optional ≠ C++ std::optional
   - Mitigation: Careful conversion code
   - Testing: Round-trip conversion tests

3. **Performance Regression**: Structured slower than frozen
   - Mitigation: Benchmarking before/after
   - Fallback: Keep frozen as option

4. **Concurrent Access**: Thread safety with mutable structures
   - Mitigation: Document thread safety guarantees
   - Design: Prefer immutable structures

**Project Risks**:
1. **Migration Complexity**: Large surface area
   - Mitigation: Incremental rollout (phases)
   - Timeline: 3-6 months per phase

2. **Testing Coverage**: Hard to test all combinations
   - Mitigation: Comprehensive test matrix
   - CI: Multiple build configurations

3. **Backward Compatibility**: Break existing users
   - Mitigation: Dual support period (1 year)
   - Communication: Early deprecation warnings

### Success Criteria

**Phase 2 Complete When**:
1. ✅ All structures defined in `metadata_structures.h`
2. ✅ Accessor interface implemented and tested
3. ✅ `metadata_v2_data` uses accessor internally
4. ✅ All existing tests pass unchanged
5. ✅ No performance regression (< 5% overhead)
6. ✅ Documentation complete

**Long-term Success**:
- Thrift becomes optional dependency (Phase 4)
- New format adoption > 50% (Phase 5)
- No compatibility issues reported
- Clear performance characteristics documented

---

## Appendix A: Complete Structure Reference

See [Structure Definitions](#structure-definitions) section above for full details.

## Appendix B: Frozen vs Structured Comparison

| Aspect | Frozen (Current) | Structured (Future) |
|--------|-----------------|-------------------|
| **Dependency** | Requires Thrift | Plain C++17 |
| **Access Pattern** | Zero-copy views | Value semantics |
| **Memory** | Memory mapped | Heap allocated |
| **Initialization** | Complex (schema + data) | Simple (deserialize) |
| **Debugging** | Difficult (opaque) | Easy (standard types) |
| **Serialization** | Thrift only | Thrift/Cereal/Bitsery |
| **Thread Safety** | Immutable (safe) | Depends on usage |
| **Modification** | Read-only | Read/Write |

## Appendix C: Migration Checklist

- [ ] Phase 1: Design complete (this document)
- [ ] Phase 2A: Structures defined
- [ ] Phase 2A: Frozen accessor implemented
- [ ] Phase 2A: Unit tests passing
- [ ] Phase 2B: Structured accessor implemented
- [ ] Phase 2B: Conversion functions working
- [ ] Phase 2B: Integration tests passing
- [ ] Phase 3: Direct deserialization implemented
- [ ] Phase 3: Performance benchmarks complete
- [ ] Phase 4: Thrift optional builds working
- [ ] Phase 4: CI testing all configurations
- [ ] Phase 5: Deprecation notices issued
- [ ] Phase 5: Default format switched

---

**Document Version**: 1.0
**Date**: 2025-01-10
**Author**: Kilo Code
**Status**: Design Phase - Ready for Review