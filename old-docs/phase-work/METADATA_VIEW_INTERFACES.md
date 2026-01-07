# Metadata View Interface Abstraction Design

**Status**: Phase 3 Week 1 - Implementation in Progress
**Created**: 2025-11-14
**Purpose**: Document the abstract view interface pattern for format-agnostic metadata access

---

## Overview

The metadata view interface abstraction layer provides a **format-agnostic API** for accessing DwarFS metadata, allowing the codebase to work with multiple serialization formats (Thrift, FlatBuffers, etc.) without requiring format-specific code throughout.

This design is critical for **Phase 3: Eliminating Runtime Thrift Dependency** by enabling native access to modern formats (FlatBuffers) without converting back to Thrift Frozen2.

---

## Architecture

### Three-Layer Design

```
┌─────────────────────────────────────────────────────────────┐
│              Consumer Code (metadata_v2.cpp)                │
│         Works with abstract view interfaces only            │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│           Abstract View Interfaces (Layer 1)                │
│     metadata_view_interface.h - Pure virtual base classes   │
│                                                              │
│  • chunk_view_interface       • dir_entry_view_interface    │
│  • chunk_range_interface      • directory_view_interface    │
│  • inode_view_interface       • global_metadata_interface   │
└─────────────────────┬───────────────────────────────────────┘
                      │
        ┌─────────────┴─────────────┐
        ▼                           ▼
┌──────────────────┐       ┌──────────────────┐
│ Thrift Views     │       │ FlatBuffers Views│
│    (Layer 2a)    │       │    (Layer 2b)    │
├──────────────────┤       ├──────────────────┤
│ • thrift_chunk   │       │ • fb_chunk_view  │
│ • thrift_inode   │       │ • fb_inode_view  │
│ • (existing)     │       │ • (to implement) │
└──────────────────┘       └──────────────────┘
        │                           │
        ▼                           ▼
┌──────────────────┐       ┌──────────────────┐
│ Thrift Frozen2   │       │ FlatBuffers Data │
│ Memory Layout    │       │ Memory Layout    │
└──────────────────┘       └──────────────────┘
```

### Key Design Principles

1. **Abstraction**: Consumer code depends only on interfaces, not implementations
2. **Zero-Copy**: View objects provide direct access to serialized data (no deserialization)
3. **Runtime Polymorphism**: Virtual functions enable format switching at runtime
4. **Type Safety**: Strong typing prevents incorrect usage
5. **Performance**: Inline-friendly design minimizes overhead

---

## Interface Definitions

### [`chunk_view_interface`](../include/dwarfs/reader/internal/metadata_view_interface.h)

Represents a segment of file data (either compressed data or a sparse hole).

```cpp
class chunk_view_interface {
 public:
  virtual ~chunk_view_interface() = default;

  // Data chunk access (only valid when is_data() == true)
  virtual uint32_t block() const = 0;        // Block number
  virtual uint32_t offset() const = 0;       // Offset within block

  // Common access
  virtual file_off_t size() const = 0;       // Chunk size
  virtual bool is_data() const = 0;          // True if data chunk
  virtual bool is_hole() const = 0;          // True if sparse hole
};
```

**Usage Pattern**:
```cpp
void process_chunk(chunk_view_interface const& chunk) {
  if (chunk.is_data()) {
    // Read from block chunk.block() + chunk.offset()
    read_data(chunk.block(), chunk.offset(), chunk.size());
  } else {
    // Sparse hole - no actual data
    skip_hole(chunk.size());
  }
}
```

### [`inode_view_interface`](../include/dwarfs/reader/internal/metadata_view_interface.h)

Represents filesystem object metadata (file, directory, symlink, device).

```cpp
class inode_view_interface {
 public:
  virtual ~inode_view_interface() = default;

  // Core attributes
  virtual mode_type mode() const = 0;        // Type + permissions
  virtual uid_type getuid() const = 0;       // Owner UID
  virtual gid_type getgid() const = 0;       // Owner GID
  virtual uint32_t inode_num() const = 0;    // Inode number

  // Helpers
  virtual std::string mode_string() const = 0;  // "drwxr-xr-x"
  virtual bool is_directory() const = 0;
};
```

**Usage Pattern**:
```cpp
void process_inode(inode_view_interface const& inode) {
  file_stat stbuf;
  stbuf.set_mode(inode.mode());
  stbuf.set_uid(inode.getuid());
  stbuf.set_gid(inode.getgid());
  stbuf.set_ino(inode.inode_num());
}
```

### [`dir_entry_view_interface`](../include/dwarfs/reader/internal/metadata_view_interface.h)

Represents a directory entry (filename + inode reference).

```cpp
class dir_entry_view_interface {
 public:
  virtual ~dir_entry_view_interface() = default;

  virtual std::string name() const = 0;
  virtual std::shared_ptr<inode_view_interface> inode() const = 0;
  virtual uint32_t self_index() const = 0;
  virtual uint32_t parent_index() const = 0;
  virtual bool is_root() const = 0;
  virtual std::string path() const = 0;
};
```

### [`directory_view_interface`](../include/dwarfs/reader/internal/metadata_view_interface.h)

Represents directory structure.

```cpp
class directory_view_interface {
 public:
  virtual ~directory_view_interface() = default;

  virtual uint32_t inode() const = 0;
  virtual size_t entry_count() const = 0;
  virtual uint32_t first_entry() const = 0;
  virtual uint32_t parent_entry() const = 0;
  virtual uint32_t self_entry() const = 0;
};
```

### [`chunk_range_interface`](../include/dwarfs/reader/internal/metadata_view_interface.h)

Provides iteration over file chunks.

```cpp
class chunk_range_interface {
 public:
  virtual ~chunk_range_interface() = default;

  virtual size_t size() const = 0;
  virtual bool empty() const = 0;
  virtual std::shared_ptr<chunk_view_interface const>
    at(size_t index) const = 0;
};
```

### [`global_metadata_interface`](../include/dwarfs/reader/internal/metadata_view_interface.h)

Provides access to global metadata tables.

```cpp
class global_metadata_interface {
 public:
  virtual ~global_metadata_interface() = default;

  virtual std::span<uint8_t const> uids() const = 0;
  virtual std::span<uint8_t const> gids() const = 0;
  virtual std::span<uint8_t const> modes() const = 0;
  virtual std::string name_at(uint32_t index) const = 0;
  virtual std::string symlink_at(uint32_t index) const = 0;
  virtual uint32_t block_size() const = 0;
  virtual uint64_t total_fs_size() const = 0;
  virtual std::optional<uint32_t> hole_block_index() const = 0;
};
```

---

## Implementation Strategy

### Phase 1: Abstract Interfaces (Week 1) ✅

**Status**: COMPLETE

Created pure virtual base classes in [`metadata_view_interface.h`](../include/dwarfs/reader/internal/metadata_view_interface.h).

### Phase 2: Thrift Concrete Views (Week 2)

**Goal**: Wrap existing Thrift frozen views to implement interfaces

**Implementation**:
```cpp
// Example: Thrift chunk view wrapper
class thrift_chunk_view : public chunk_view_interface {
  ::apache::thrift::frozen::View<thrift::metadata::chunk> chunk_;
  uint32_t hole_block_idx_;

 public:
  thrift_chunk_view(auto chunk, uint32_t hole_idx)
    : chunk_(chunk), hole_block_idx_(hole_idx) {}

  uint32_t block() const override { return chunk_.block(); }
  uint32_t offset() const override { return chunk_.offset(); }
  file_off_t size() const override { return chunk_.size(); }
  bool is_data() const override {
    return chunk_.block() != hole_block_idx_;
  }
  bool is_hole() const override { return !is_data(); }
};
```

### Phase 3: FlatBuffers Concrete Views (Week 3-4)

**Goal**: Implement FlatBuffers views for native zero-copy access

**Implementation**:
```cpp
// Example: FlatBuffers chunk view
class fb_chunk_view : public chunk_view_interface {
  dwarfs::flatbuffers::Chunk const* chunk_;
  uint32_t hole_block_idx_;

 public:
  fb_chunk_view(auto* chunk, uint32_t hole_idx)
    : chunk_(chunk), hole_block_idx_(hole_idx) {}

  uint32_t block() const override { return chunk_->block(); }
  uint32_t offset() const override { return chunk_->offset(); }
  file_off_t size() const override { return chunk_->size(); }
  bool is_data() const override {
    return chunk_->block() != hole_block_idx_;
  }
  bool is_hole() const override { return !is_data(); }
};
```

### Phase 4: metadata_v2 Integration (Week 4)

**Goal**: Replace Thrift-specific code with interface-based code

**Before** (Thrift-specific):
```cpp
auto chunks = meta_.chunks();
for (auto chunk : chunks) {
  uint32_t block = chunk.block();  // Direct Thrift access
  uint32_t offset = chunk.offset();
  // ...
}
```

**After** (Interface-based):
```cpp
std::shared_ptr<chunk_range_interface> chunks = get_chunks(inode);
for (size_t i = 0; i < chunks->size(); ++i) {
  auto chunk = chunks->at(i);
  uint32_t block = chunk->block();  // Virtual call
  uint32_t offset = chunk->offset();
  // ...
}
```

---

## Benefits

### 1. **Format Independence**

Consumer code works with **any** serialization format without changes:

```cpp
// This code works with BOTH Thrift and FlatBuffers
void process_file(chunk_range_interface const& chunks) {
  for (size_t i = 0; i < chunks.size(); ++i) {
    auto chunk = chunks.at(i);
    if (chunk->is_data()) {
      read_block(chunk->block(), chunk->offset(), chunk->size());
    }
  }
}
```

### 2. **Eliminates Runtime Thrift Dependency**

With FlatBuffers views, **no Thrift code runs at mount time**:
- FlatBuffers metadata → fb_*_view implementations → interface consumers
- Thrift only needed at **build time** for legacy format support

### 3. **Easy to Add New Formats**

Adding a new format requires:
1. Implement concrete view classes
2. Register format detection
3. No changes to consumer code

### 4. **Testing Flexibility**

Mock implementations for testing:
```cpp
class mock_chunk_view : public chunk_view_interface {
  // Test-specific implementation
};
```

### 5. **Performance**

- **Zero-copy**: Views access serialized data directly
- **Inlining**: Compiler can inline through virtual calls
- **Minimal overhead**: Pointer indirection only

---

## Migration Path

### Current State (Phase 2 Complete)

```
metadata_v2.cpp → Thrift Frozen2 Views (direct)
```

### Target State (Phase 3 Complete)

```
metadata_v2.cpp → Abstract Interfaces → {Thrift Views, FlatBuffers Views}
```

### Transition Strategy

1. **Week 1** (Complete): Define interfaces
2. **Week 2**: Create Thrift wrapper views
3. **Week 3**: Implement FlatBuffers views
4. **Week 4**: Migrate metadata_v2.cpp to use interfaces
5. **Week 5-6**: Testing, optimization, documentation

---

## Design Decisions

### Why Virtual Functions?

**Pros**:
- Simple, well-understood
- Compiler optimizations (devirtualization)
- Type-safe, compile-time checked

**Cons**:
- Small overhead (1 pointer indirection)
- Larger binary size (vtables)

**Decision**: Acceptable tradeoff for flexibility and clarity

### Why Shared Pointers?

Views are **lightweight wrappers** around serialized data, but consumers need **stable references**. Shared pointers provide:
- Safe lifetime management
- Easy passing between functions
- Works with standard containers

### Why Not Templates?

Templates would avoid virtual calls but:
- **Compile-time format selection** (less flexible)
- **Code duplication** (every consumer instantiated per format)
- **Complex error messages**

Virtual functions provide **runtime flexibility** with **minimal cost**.

---

## Future Extensions

### Potential Additions

1. **Async views**: For network-backed filesystems
2. **Streaming views**: For very large metadata
3. **Cached views**: Memoization for expensive operations
4. **Compressed views**: On-demand decompression

### Format Support Roadmap

- **Phase 3**: FlatBuffers (primary modern format)
- **Phase 4**: Cap'n Proto (if needed)
- **Phase 5**: Custom binary format (performance-focused)

---

## References

- **FlatBuffers**: https://google.github.io/flatbuffers/
- **Thrift Frozen2**: (Facebook internal, limited docs)
- **Phase 3 Plan**: [`doc/PHASE_3_METADATA_ACCESS_LAYER.md`](./PHASE_3_METADATA_ACCESS_LAYER.md)
- **Interface Header**: [`include/dwarfs/reader/internal/metadata_view_interface.h`](../include/dwarfs/reader/internal/metadata_view_interface.h)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-14
**Status**: Interfaces defined, implementation pending