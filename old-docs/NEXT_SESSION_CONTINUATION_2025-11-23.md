# Next Session: Complete Phase 2.5+ Multi-Format Metadata (80% Done)

**For AI Session Starting**: 2025-11-23 16:30+ HKT
**Estimated Time**: 2-3 hours to fix remaining build errors, then 4-6 hours for testing/docs
**Current Branch**: `feature/multi-format-serialization-fuse`
**Working Directory**: `/Users/mulgogi/src/external/dwarfs`

---

## QUICK START - What You Need to Know

You're **80% done** completing Phase 2.5+ of DwarFS multi-format metadata serialization. The Strategy Pattern architecture is **CORRECT** - you aligned backend implementations with the existing `metadata_view_interface.h`.

**What's Left**: Backend `global_metadata` classes need to implement ALL interface methods. Currently missing 8 methods.

**Current Build Status**: Compiles to 92%, fails because `global_metadata` doesn't fully implement `global_metadata_interface`.

---

## What Was Already Completed (80%)

✅ Deleted 4 duplicate interface files (inode_view_interface.h, dir_entry_view_interface.h, global_metadata_interface.h, chunk_view_interface.h)
✅ Updated all includes to use existing `metadata_view_interface.h`
✅ Fixed `inode()` return type from `unique_ptr` to `shared_ptr`
✅ Removed incorrect `override` keywords from backend helper methods
✅ Fixed wrapper classes to use `parent_shared()` for concrete types
✅ Updated CMake to remove deleted file references
✅ Fixed factory includes
✅ Fixed `append_to()` recursion

---

## Current Build Error (The Only Remaining Issue)

```
/Users/mulgogi/src/external/dwarfs/include/dwarfs/reader/internal/metadata_view_interface.h:253:35: 
note: unimplemented pure virtual method 'uids' in 'global_metadata'
```

**Root Cause**: `flatbuffers_backend::global_metadata` and `thrift_backend::global_metadata` classes inherit from `global_metadata_interface` but don't implement all required methods.

**Required Interface Methods** (from `metadata_view_interface.h` lines 246-289):
```cpp
class global_metadata_interface {
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

**What Backends Currently Have** (wrong signature/not virtual):
- `names()` method returning `string_table const&` (helper method, keep it)
- Need to ADD `name_at(uint32_t)` returning `std::string` (interface method)
- Similar for other methods

---

## YOUR TASK - Fix Backend Classes (1-2 hours)

### Step 1: Fix FlatBuffers Backend (45 min)

**File**: `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`

Add these virtual `override` methods to lines 60-75 (after existing constructors):

```cpp
class global_metadata : public global_metadata_interface {
 public:
  // ... existing Meta type alias and constructor ...
  
  // ADD THESE: Implement ALL interface methods with override
  std::span<uint8_t const> uids() const override;
  std::span<uint8_t const> gids() const override;
  std::span<uint8_t const> modes() const override;
  std::string name_at(uint32_t index) const override;
  std::string symlink_at(uint32_t index) const override;
  uint32_t block_size() const override;
  uint64_t total_fs_size() const override;
  std::optional<uint32_t> hole_block_index() const override;
  
  // KEEP THESE: Existing helper methods (NOT virtual, NOT override)
  Meta const& meta() const { return meta_; }
  uint32_t first_dir_entry(uint32_t ino) const;
  uint32_t parent_dir_entry(uint32_t ino) const;
  uint32_t self_dir_entry(uint32_t ino) const;
  dwarfs::internal::string_table const& names() const { return names_; }
  ::dwarfs::flatbuffers::Directory const* get_directory(uint32_t index) const;
  
 private:
  // ... existing members ...
};
```

**File**: `src/reader/internal/metadata_types_flatbuffers.cpp`

Add implementations after line 149 (after `get_directory()` method):

```cpp
//==============================================================================
// global_metadata interface implementations
//==============================================================================

std::span<uint8_t const> global_metadata::uids() const {
  if (auto uids_vec = meta_->uids()) {
    return std::span<uint8_t const>(
        reinterpret_cast<uint8_t const*>(uids_vec->data()),
        uids_vec->size() * sizeof(uint32_t));
  }
  return {};
}

std::span<uint8_t const> global_metadata::gids() const {
  if (auto gids_vec = meta_->gids()) {
    return std::span<uint8_t const>(
        reinterpret_cast<uint8_t const*>(gids_vec->data()),
        gids_vec->size() * sizeof(uint32_t));
  }
  return {};
}

std::span<uint8_t const> global_metadata::modes() const {
  if (auto modes_vec = meta_->modes()) {
    return std::span<uint8_t const>(
        reinterpret_cast<uint8_t const*>(modes_vec->data()),
        modes_vec->size() * sizeof(uint16_t));
  }
  return {};
}

std::string global_metadata::name_at(uint32_t index) const {
  return names_[index];
}

std::string global_metadata::symlink_at(uint32_t index) const {
  if (auto symlinks = meta_->symlinks()) {
    if (index < symlinks->size()) {
      return symlinks->Get(index)->str();
    }
  }
  return "";
}

uint32_t global_metadata::block_size() const {
  return meta_->block_size();
}

uint64_t global_metadata::total_fs_size() const {
  return meta_->total_fs_size();
}

std::optional<uint32_t> global_metadata::hole_block_index() const {
  auto idx = meta_->hole_block_index();
  return idx != 0 ? std::optional<uint32_t>(idx) : std::nullopt;
}
```

### Step 2: Fix Thrift Backend (45 min)

**File**: `include/dwarfs/reader/internal/metadata_types_thrift.h`

Add same methods to `global_metadata` class around lines 88-118:

```cpp
class global_metadata : public global_metadata_interface {
 public:
  // ... existing types and constructor ...
  
  // ADD THESE: Interface implementations
  std::span<uint8_t const> uids() const override;
  std::span<uint8_t const> gids() const override;
  std::span<uint8_t const> modes() const override;
  std::string name_at(uint32_t index) const override;
  std::string symlink_at(uint32_t index) const override;
  uint32_t block_size() const override;
  uint64_t total_fs_size() const override;
  std::optional<uint32_t> hole_block_index() const override;
  
  // KEEP THESE: Helper methods
  Meta const& meta() const { return meta_; }
  uint32_t first_dir_entry(uint32_t ino) const;
  uint32_t parent_dir_entry(uint32_t ino) const;
  uint32_t self_dir_entry(uint32_t ino) const;
  dwarfs::internal::string_table const& names() const { return names_; }
  std::optional<directories_view> bundled_directories() const;
  
 private:
  // ... existing members ...
};
```

**File**: `src/reader/internal/metadata_types_thrift.cpp`

Add implementations after line 899 (after `bundled_directories()` method):

```cpp
//==============================================================================
// global_metadata interface implementations
//==============================================================================

std::span<uint8_t const> global_metadata::uids() const {
  auto uids_view = meta_.uids();
  if (uids_view.empty()) {
    return {};
  }
  return std::span<uint8_t const>(
      reinterpret_cast<uint8_t const*>(uids_view.begin()),
      uids_view.size() * sizeof(uint32_t));
}

std::span<uint8_t const> global_metadata::gids() const {
  auto gids_view = meta_.gids();
  if (gids_view.empty()) {
    return {};
  }
  return std::span<uint8_t const>(
      reinterpret_cast<uint8_t const*>(gids_view.begin()),
      gids_view.size() * sizeof(uint32_t));
}

std::span<uint8_t const> global_metadata::modes() const {
  auto modes_view = meta_.modes();
  if (modes_view.empty()) {
    return {};
  }
  return std::span<uint8_t const>(
      reinterpret_cast<uint8_t const*>(modes_view.begin()),
      modes_view.size() * sizeof(uint16_t));
}

std::string global_metadata::name_at(uint32_t index) const {
  return names_[index];
}

std::string global_metadata::symlink_at(uint32_t index) const {
  auto symlinks_view = meta_.symlinks();
  if (index < symlinks_view.size()) {
    return std::string(symlinks_view[index]);
  }
  return "";
}

uint32_t global_metadata::block_size() const {
  return meta_.block_size();
}

uint64_t global_metadata::total_fs_size() const {
  return meta_.total_fs_size();
}

std::optional<uint32_t> global_metadata::hole_block_index() const {
  auto idx_opt = meta_.hole_block_index();
  return idx_opt ? std::optional<uint32_t>(*idx_opt) : std::nullopt;
}
```

### Step 3: Test Build (30 min)

```bash
cd /Users/mulgogi/src/external/dwarfs
cmake --build build-fb --target dwarfs_reader -j4
```

**Expected**: Build succeeds! All 5 libraries compile.

If you see different errors, they're likely minor (missing includes, typos). Fix and retry.

---

## After Build Success (4-6 hours)

### Update TODO List
```
[x] Phase 2.5-A: Delete duplicates & align interfaces
[x] Phase 2.6: Factory integration
[-] Phase 2.7: Build system validation (30min)
[ ] Phase 2.8: Comprehensive testing (3-4h)
[ ] Phase 2.9: Documentation & validation (2-3h)
```

### Phase 2.7: Validate All Builds (1h)

Test these configurations:

1. **FlatBuffers-only** (already done):
```bash
cmake -B build-fb -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
cmake --build build-fb
```

2. **Dual-format** (if Thrift available):
```bash
cmake -B build-dual -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
cmake --build build-dual
```

3. **Tebako** (FlatBuffers forced):
```bash
cmake -B build-tebako -DTEBAKO_BUILD=ON -DTEBAKO_BUILD_SCOPE=ALL
cmake --build build-tebako
```

### Phase 2.8: Write Tests (3-4h)

Create 3 test files in `test/`:

**1. `test/metadata_view_interface_test.cpp`** - Test interface polymorphism
**2. `test/metadata_factory_test.cpp`** - Test format detection & backend creation
**3. `test/backend_compatibility_test.cpp`** - Test both backends produce same results

### Phase 2.9: Update Documentation (2-3h)

1. Update `README.adoc` - Add metadata serialization section
2. Create `doc/metadata-formats.md` - Technical format guide
3. Update tool manuals (`doc/mkdwarfs.md`, `doc/dwarfs.md`, etc.)
4. Update `.kilocode/rules/memory-bank/context.md` - Mark Phase 2.5+ complete
5. Archive temp docs to `old-docs/phase-work/`

---

## Key Files You'll Edit

**Will Edit** (2 files):
- `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`
- `include/dwarfs/reader/internal/metadata_types_thrift.h`
- `src/reader/internal/metadata_types_flatbuffers.cpp`
- `src/reader/internal/metadata_types_thrift.cpp`

**Reference** (don't edit):
- `include/dwarfs/reader/internal/metadata_view_interface.h` (lines 246-289)

---

## Architecture Reminder

The architecture is **CORRECT**. You're just completing the implementation:

```
Interface Layer (abstract):
  metadata_view_interface.h
     ↑ implements
     │
Backend Layer (concrete):
  flatbuffers_backend::global_metadata
  thrift_backend::global_metadata
     ↑ creates
     │
Factory Layer:
  metadata_factory::create_global_metadata()
     ↑ uses
     │
Wrapper Layer:
  dir_entry_view, inode_view (public API)
```

---

## Troubleshooting

**Error: "cannot override non-virtual method X"**
→ Remove `override` keyword - X is a helper method, not in interface

**Error: "unimplemented pure virtual method X"**
→ Add `X` to backend class with `override` keyword

**Error: "no matching function for call to `meta_->X()`"**
→ Check FlatBuffers vs Thrift API differences (use `->` for FlatBuffers, `.` for Thrift)

**Error: "cannot convert `string_view` to `string`"**
→ Wrap with `std::string(...)` for explicit conversion

---

## Success Criteria

When done, you should have:
✅ Zero build errors for FlatBuffers-only build
✅ All 5 libraries (dwarfs_common, dwarfs_reader, etc.) compile
✅ (Optional) Dual-format build succeeds if Thrift available
✅ (Optional) Tests pass
✅ (Optional) Documentation updated

---

## Memory Bank Update After Completion

Update `.kilocode/rules/memory-bank/context.md`:
```markdown
## Recent Major Changes (v0.15.0 - In Development)

### Multi-Format Metadata Serialization (COMPLETED)

**Status**: ✅ Complete
**Date**: 2025-11-23
**Achievement**: Successfully aligned backend implementations with existing `metadata_view_interface.h`. Strategy Pattern implementation complete with both FlatBuffers and Thrift backends fully functional.

**Key Changes**:
- Removed duplicate interface files
- Aligned all backend method signatures with existing interfaces
- Implemented all required interface methods in both backends
- Factory pattern properly creates backends via interfaces
- All builds (FlatBuffers-only, dual-format, Tebako) succeed
```

---

**Working Directory**: `/Users/mulgogi/src/external/dwarfs`
**Branch**: `feature/multi-format-serialization-fuse`
**Next Steps**: Add 8 interface methods to both backend classes (1-2 hours)
**Estimated Total Remaining**: 6-9 hours (including testing & docs)

**START HERE**: Step 1 above - Add methods to FlatBuffers backend
