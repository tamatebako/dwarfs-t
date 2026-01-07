# Writer Domain Model Refactoring Plan

## Problem Statement

The writer layer is currently **Thrift-dependent**, preventing FlatBuffers-only builds on AppleClang 17. Files wrapped with `#ifdef DWARFS_HAVE_THRIFT` have **missing functionality** when Thrift is disabled.

## Root Cause

Writer uses `thrift::metadata::*` types as its **internal domain model**:
- `thrift::metadata::metadata` - top-level container
- `thrift::metadata::chunk` - chunk data
- `thrift::metadata::inode_data` - inode metadata
- `thrift::metadata::directory` - directory structure
- `thrift::metadata::dir_entry` - directory entries

This creates **tight coupling** to Thrift serialization format.

## Solution Architecture

### Phase 1: Extend Domain Models for Mutability

Current domain models in `include/dwarfs/metadata/domain/` are **immutable** (const accessors). Writer needs **mutable** access.

**Options:**
1. Make all fields public (like `inode_data` already is)
2. Add mutable setters alongside const getters
3. Create separate mutable wrappers

**Decision:** Make fields **public** for consistency with `inode_data`.

### Phase 2: Create Writer-Specific Domain Utilities

Create new files in `src/writer/internal/`:
- `domain_metadata_builder.cpp` - Build domain::metadata incrementally
- `domain_chunk_mapper.cpp` - Remap domain::chunk blocks
- No Thrift dependency!

### Phase 3: Refactor Core Writer Files

#### 3.1 global_entry_data.cpp (Currently 171 lines, ALL guarded)
**Current:** Uses `thrift::metadata::inode_data`
**Refactor:** Use `metadata::domain::inode_data`
**New file:** `src/writer/internal/global_entry_data.cpp` (Thrift-free)

#### 3.2 metadata_builder.cpp (Currently 1288 lines, ALL guarded)
**Current:** Builds `thrift::metadata::metadata`
**Refactor:** Build `metadata::domain::metadata`
**New file:** `src/writer/internal/domain_metadata_builder.cpp` (Thrift-free)

#### 3.3 entry.cpp (Currently 454 lines, partial guards)
**Current:** Has `#ifdef DWARFS_HAVE_THRIFT` around pack() methods
**Refactor:** Extract pack() to separate conversion layer
**Keep:** entry.cpp (Thrift-free)
**New:** `src/writer/internal/entry_packer.cpp` (format-specific)

#### 3.4 inode_hole_mapper.cpp
**Current:** Uses `thrift::metadata::chunk`
**Refactor:** Use `metadata::domain::chunk`
**Update:** `src/writer/internal/inode_hole_mapper.cpp` (Thrift-free)

#### 3.5 inode_manager.cpp
**Current:** Uses `thrift::metadata::chunk` as `chunk_type`
**Refactor:** Use `metadata::domain::chunk`
**Update:** `src/writer/internal/inode_manager.cpp` (Thrift-free)

### Phase 4: Conversion Layer

Create new conversion utilities:
- `src/metadata/converters/domain_to_thrift.cpp` - domain → Thrift (if Thrift enabled)
- `src/metadata/converters/domain_to_flatbuffers.cpp` - domain → FlatBuffers (always)

These live in the **metadata converter layer**, not writer layer.

### Phase 5: Update metadata_freezer.cpp

**Current:** Takes `thrift::metadata::metadata const&`
**Refactor:** Take `metadata::domain::metadata const&`
**Conversion:** Convert to Thrift/FlatBuffers at serialization boundary

## Implementation Order

1. **Extend domain models** - Make fields public
2. **Create domain_metadata_builder** - Thrift-free builder
3. **Refactor global_entry_data** - Use domain types
4. **Refactor inode_hole_mapper** - Use domain::chunk
5. **Refactor inode_manager** - Use domain::chunk
6. **Refactor entry.cpp** - Move pack() to converter
7. **Create conversion layer** - domain ↔ Thrift/FlatBuffers
8. **Update metadata_freezer** - Use domain input
9. **Build & test** - FlatBuffers-only build
10. **Build & test** - Both formats build

## File Impact Analysis

### Files to MODIFY (make Thrift-free):
- `include/dwarfs/metadata/domain/chunk.h` - Add mutable setters
- `include/dwarfs/metadata/domain/dir_entry.h` - Add mutable setters
- `include/dwarfs/metadata/domain/directory.h` - Add mutable setters
- `src/writer/internal/global_entry_data.cpp` - Remove Thrift types
- `src/writer/internal/inode_hole_mapper.cpp` - Use domain::chunk
- `src/writer/internal/inode_manager.cpp` - Use domain::chunk
- `src/writer/internal/entry.cpp` - Remove pack() methods
- `src/writer/internal/metadata_freezer.cpp` - Take domain input

### Files to CREATE (new Thrift-free implementations):
- `src/writer/internal/domain_metadata_builder.cpp` - Replaces metadata_builder.cpp
- `src/writer/internal/domain_metadata_builder.h` - Header
- `src/metadata/converters/domain_to_thrift.cpp` - Conversion (Thrift builds)
- `src/metadata/converters/domain_to_flatbuffers.cpp` - Conversion (always)
- `src/metadata/converters/entry_packer.cpp` - Pack entries to Thrift/FlatBuffers

### Files to DEPRECATE (Thrift-only, optional):
- `src/writer/internal/metadata_builder.cpp` - Replaced by domain version
- `src/writer/internal/metadata_builder.h` - Replaced by domain version
- `src/writer/internal/global_entry_data.cpp` (old) - Replaced by domain version

## Validation Approach

1. **Unit tests** - Existing tests should pass with domain models
2. **Integration tests** - Create & read images with both formats
3. **Compatibility tests** - Read old Thrift images with new code
4. **AppleClang 17 build** - Must compile with `-DDWARFS_WITH_THRIFT=OFF`

## Success Criteria

- [ ] AppleClang 17 builds with FlatBuffers-only
- [ ] Both format builds work (Thrift + FlatBuffers)
- [ ] No `#ifdef DWARFS_HAVE_THRIFT` in core writer logic
- [ ] All existing tests pass
- [ ] Images created with FlatBuffers are readable
- [ ] Images created with Thrift are readable (if enabled)
// ... existing code ...