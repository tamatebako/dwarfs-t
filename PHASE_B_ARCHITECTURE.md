# Phase B: Reader-Side Thrift to Cereal Migration Architecture

## Objective

Complete removal of Apache Thrift from reader-side code, replacing `MappedFrozen<thrift::metadata::metadata>` with domain models loaded via Cereal serialization.

## Architectural Principles

1. **Object-Oriented Design**: All components use proper encapsulation with clear interfaces
2. **MECE (Mutually Exclusive, Collectively Exhaustive)**: Each class has a single, well-defined responsibility
3. **Separation of Concerns**: Serialization, domain logic, and access patterns are separated
4. **Open/Closed Principle**: Extensible design that allows new formats without modifying existing code
5. **Single Responsibility**: Each class/module serves one clear purpose

## Current Architecture (Thrift-based)

```
MetadataReader (Phase A - complete)
    ↓ deserializes
domain::Metadata (memory-based domain model)
    ↓ NOT USED BY READER
MappedFrozen<thrift::metadata::metadata> (memory-mapped frozen Thrift)
    ↓ used by
metadata_v2_data
    ↓ wraps with
global_metadata (frozen view wrapper)
    ↓ provides access via
inode_view_impl, dir_entry_view_impl, chunk_view, etc.
```

## Target Architecture (Cereal-based)

```
MetadataReader
    ↓ deserializes to
domain::Metadata (in-memory domain model)
    ↓ loaded into
MetadataAccessor (new abstraction layer)
    ↓ provides efficient access via
DomainMetadataView (replaces global_metadata)
    ↓ provides typed views
DomainInodeView, DomainDirEntryView, DomainChunkView
```

## Key Design Decisions

### 1. MetadataAccessor Pattern

**Purpose**: Provide efficient access to domain::Metadata without exposing implementation

```cpp
class MetadataAccessor {
public:
    explicit MetadataAccessor(std::unique_ptr<domain::Metadata> meta);

    // Efficient indexed access
    domain::InodeData const& inode(size_t index) const;
    domain::Directory const& directory(size_t index) const;
    domain::Chunk const& chunk(size_t index) const;

    // Range access
    std::span<domain::InodeData const> inodes() const;
    std::span<domain::Directory const> directories() const;

    // Metadata properties
    uint32_t block_size() const;
    std::optional<uint32_t> hole_block_index() const;

private:
    std::unique_ptr<domain::Metadata> meta_;
    // Cached/optimized access structures
};
```

### 2. View Layer Abstraction

**Purpose**: Maintain same interface as frozen views but operate on domain models

```cpp
// Replace global_metadata with DomainMetadataView
class DomainMetadataView {
public:
    explicit DomainMetadataView(MetadataAccessor const& accessor);

    // Same interface as global_metadata
    uint32_t first_dir_entry(uint32_t ino) const;
    uint32_t parent_dir_entry(uint32_t ino) const;
    uint32_t self_dir_entry(uint32_t ino) const;

private:
    MetadataAccessor const& accessor_;
    // Unpacked/cached structures if needed
};
```

### 3. Typed View Classes

**Purpose**: Provide type-safe, efficient access to specific metadata elements

```cpp
// Replace inode_view_impl
class DomainInodeView {
public:
    DomainInodeView(domain::InodeData const& inode,
                    uint32_t inode_num,
                    MetadataAccessor const& meta);

    // Same interface as inode_view_impl
    uint32_t mode() const;
    uid_type getuid() const;
    gid_type getgid() const;
    uint32_t inode_num() const;
    bool is_directory() const;

private:
    domain::InodeData const& inode_;
    uint32_t inode_num_;
    MetadataAccessor const& meta_;
};
```

## Migration Strategy

### Phase 1: Foundation (Low Risk)

**Files**: time_resolution_handler.h
- Convert View<thrift::metadata::metadata> to domain::Metadata const&
- Minimal impact, single file

### Phase 2: Analysis Layer (Medium Risk)

**Files**: metadata_analyzer.h, metadata_analyzer.cpp
- Update to work with domain::Metadata
- Create domain model analysis tools
- No impact on runtime, only diagnostic tools

### Phase 3: Type System (High Risk - Critical)

**Files**: metadata_types.h, metadata_types.cpp

**Changes**:
1. Replace `global_metadata` with `DomainMetadataView`
2. Replace `inode_view_impl` with `DomainInodeView`
3. Replace `dir_entry_view_impl` with `DomainDirEntryView`
4. Replace `chunk_view` with `DomainChunkView`
5. Update all frozen::View references to domain model references

**Architecture**:
```cpp
// Old: global_metadata wraps MappedFrozen
class global_metadata {
    Meta const& meta_;  // MappedFrozen<thrift::metadata::metadata>
};

// New: DomainMetadataView wraps MetadataAccessor
class DomainMetadataView {
    MetadataAccessor const& accessor_;
};
```

### Phase 4: Core Metadata (Highest Risk - Keystone)

**Files**: metadata_v2.cpp

**Changes**:
1. Replace `MappedFrozen<thrift::metadata::metadata> meta_` with `std::unique_ptr<domain::Metadata> meta_`
2. Update constructor to use `MetadataReader` instead of `map_frozen`
3. Replace all `.thaw()` calls with direct domain model access
4. Update serialization methods to work with domain models

**Pattern**:
```cpp
// Old constructor
metadata_v2_data(...)
    : meta_{check_frozen(map_frozen<thrift::metadata::metadata>(schema, data_))}
    , global_{lgr, meta_}

// New constructor
metadata_v2_data(...)
    : meta_{MetadataReader().read(data)}
    , accessor_{std::make_unique<MetadataAccessor>(std::move(meta_))}
    , global_{lgr, *accessor_}
```

## Optimization Strategies

### 1. Lazy Unpacking

For packed structures (directories, chunk table), unpack on first access:

```cpp
class MetadataAccessor {
    mutable std::optional<std::vector<UnpackedDirectory>> unpacked_dirs_;

    std::vector<UnpackedDirectory> const& unpacked_directories() const {
        if (!unpacked_dirs_) {
            unpacked_dirs_ = unpack_directories(meta_->directories);
        }
        return *unpacked_dirs_;
    }
};
```

### 2. Index Caching

Cache frequently accessed lookups:

```cpp
class DomainMetadataView {
    mutable std::unordered_map<uint32_t, uint32_t> parent_cache_;

    uint32_t parent_dir_entry(uint32_t ino) const {
        if (auto it = parent_cache_.find(ino); it != parent_cache_.end()) {
            return it->second;
        }
        auto parent = compute_parent(ino);
        parent_cache_[ino] = parent;
        return parent;
    }
};
```

### 3. String Table Optimization

Reuse string_table infrastructure for names/symlinks:

```cpp
class MetadataAccessor {
    string_table names_;
    string_table symlinks_;

    MetadataAccessor(std::unique_ptr<domain::Metadata> meta)
        : meta_{std::move(meta)}
        , names_{create_string_table(meta_->names)}
        , symlinks_{create_string_table(meta_->symlink_table)}
    {}
};
```

## Testing Strategy

### 1. Unit Tests

- Test each view class independently
- Verify domain model access patterns
- Test packed structure unpacking

### 2. Integration Tests

- Load existing DwarFS filesystems
- Verify metadata reading
- Compare results with Thrift-based reader

### 3. Performance Tests

- Benchmark metadata access speed
- Compare memory usage
- Profile hot paths

## Backward Compatibility

The `MetadataReader` provides format detection:
- Thrift Compact → domain::Metadata
- Cereal Binary → domain::Metadata

Both paths produce the same domain model, ensuring compatibility.

## Risk Mitigation

### 1. Incremental Conversion

Convert one file at a time, building and testing after each change.

### 2. Parallel Implementation

Keep Thrift code temporarily to compare behavior during development.

### 3. Comprehensive Testing

Test with real-world DwarFS filesystems before removing Thrift.

## Success Criteria

- ✅ All reader files converted to domain models
- ✅ Zero Thrift dependencies in reader code
- ✅ All tests passing
- ✅ No performance regression
- ✅ Memory usage comparable or better
- ✅ Clean, maintainable code following OO principles

## Implementation Order

1. ✅ Phase A: Writer-side conversion (complete)
2. → Create MetadataAccessor class
3. → Convert time_resolution_handler.h
4. → Convert metadata_analyzer
5. → Create DomainMetadataView and typed views
6. → Convert metadata_types
7. → Convert metadata_v2 constructor
8. → Convert metadata_v2 access methods
9. → Convert metadata_v2 serialization
10. → Remove all Thrift dependencies
11. → Final testing and optimization

## Next Steps

1. Create `MetadataAccessor` class in new file
2. Create `DomainMetadataView` and typed view classes
3. Begin incremental file conversion
4. Build and test after each conversion
5. Remove Thrift once all conversions complete