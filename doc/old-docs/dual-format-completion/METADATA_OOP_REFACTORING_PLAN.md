# Metadata OOP Refactoring Plan - Multi-Format Support

**Date**: 2025-11-28 10:58 HKT  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Objective**: Implement clean OOP architecture for multi-format metadata support  
**Priority**: CRITICAL - Required for v0.16.0  
**Estimated Time**: 8-12 hours (compressed into 6-8h execution)

---

## Architectural Principles

### 1. Strategy Pattern (Design Pattern)
Each serialization format is a **Strategy** implementing common interfaces:
- FlatBuffers Strategy
- Thrift Strategy  
- Future formats (e.g., Cap'n Proto, Protocol Buffers)

### 2. Dependency Inversion Principle (SOLID)
High-level code depends on abstractions (interfaces), not concrete implementations.

### 3. Open/Closed Principle (SOLID)
System is open for extension (add new formats) but closed for modification (existing code unchanged).

### 4. Single Responsibility Principle (SOLID)
Each class has ONE reason to change:
- Backend classes: Handle format-specific serialization
- Wrapper classes: Provide unified API
- Factory classes: Create appropriate backend
- Interface classes: Define contracts

### 5. Separation of Concerns
- **Serialization Layer**: Format-specific details (FlatBuffers, Thrift)
- **Domain Layer**: Format-agnostic business logic (wrapper types)
- **Interface Layer**: Abstract contracts (interfaces)

---

## Current Architecture Problems

### Problem 1: Incomplete Type Aliasing
```cpp
// In dual-format builds (metadata_types.h:66-71)
#else
class inode_view_impl;        // Forward declaration - INCOMPLETE TYPE
class dir_entry_view_impl;    // Forward declaration - INCOMPLETE TYPE  
class global_metadata;        // Forward declaration - INCOMPLETE TYPE
#endif
```

**Impact**: Cannot use `std::shared_ptr<internal::inode_view_impl>` because type is incomplete.

### Problem 2: No Inheritance Hierarchy
```cpp
// Current: No inheritance
namespace flatbuffers_backend {
  class inode_view_impl { /* standalone */ };
}

namespace thrift_backend {
  class inode_view_impl { /* standalone */ };
}
```

**Impact**: Cannot upcast to common base, breaking polymorphism.

### Problem 3: Backend-Specific Return Types
```cpp
// FlatBuffers backend
fb::chunk_range get_chunks(...) const;  // Returns backend-specific type

// Thrift backend  
tb::chunk_range get_chunks(...) const;  // Returns different backend-specific type

// Interface expects
chunk_range get_chunks(...) const = 0;  // Returns undefined type in dual-format
```

**Impact**: Virtual function return type mismatch.

---

## Target Architecture

### Layer 1: Abstract Interfaces (Format-Agnostic)

```cpp
// include/dwarfs/reader/internal/metadata_view_interface.h
namespace dwarfs::reader::internal {

class inode_view_interface {
public:
  virtual ~inode_view_interface() = default;
  virtual uint32_t inode_num() const = 0;
  virtual mode_type mode() const = 0;
  virtual bool is_directory() const = 0;
  // ... other pure virtual methods
};

class dir_entry_view_interface {
public:
  virtual ~dir_entry_view_interface() = default;
  virtual std::string name() const = 0;
  virtual std::shared_ptr<inode_view_interface const> inode() const = 0;
  virtual uint32_t self_index() const = 0;
  // ... other pure virtual methods
};

class global_metadata_interface {
public:
  virtual ~global_metadata_interface() = default;
  virtual uint32_t first_dir_entry(uint32_t ino) const = 0;
  virtual std::string name_at(uint32_t index) const = 0;
  // ... other pure virtual methods
};

class chunk_view_interface {
public:
  virtual ~chunk_view_interface() = default;
  virtual uint32_t block() const = 0;
  virtual uint32_t offset() const = 0;
  virtual file_off_t size() const = 0;
  virtual bool is_data() const = 0;
  virtual bool is_hole() const = 0;
};

class chunk_range_interface {
public:
  virtual ~chunk_range_interface() = default;
  virtual size_t size() const = 0;
  virtual bool empty() const = 0;
  
  // Iterator support
  class iterator_interface {
  public:
    virtual ~iterator_interface() = default;
    virtual chunk_view_interface const& operator*() const = 0;
    virtual iterator_interface& operator++() = 0;
    virtual bool equals(iterator_interface const& other) const = 0;
  };
  
  virtual std::unique_ptr<iterator_interface> begin() const = 0;
  virtual std::unique_ptr<iterator_interface> end() const = 0;
};

} // namespace dwarfs::reader::internal
```

### Layer 2: Backend Implementations (Inherit from Interfaces)

```cpp
// include/dwarfs/reader/internal/metadata_types_flatbuffers.h
namespace dwarfs::reader::internal::flatbuffers_backend {

class inode_view_impl : public inode_view_interface {
public:
  // Implement all interface methods
  uint32_t inode_num() const override { return inode_num_; }
  mode_type mode() const override { /* FB-specific */ }
  bool is_directory() const override { /* FB-specific */ }
  
  // FB-specific methods (not in interface)
  uint32_t mode_index() const { return inode_data_->mode_index(); }
  
private:
  ::dwarfs::flatbuffers::InodeData const* inode_data_;
  uint32_t inode_num_;
  ::dwarfs::flatbuffers::Metadata const* meta_;
};

class dir_entry_view_impl : public dir_entry_view_interface {
public:
  std::string name() const override { /* FB-specific */ }
  std::shared_ptr<inode_view_interface const> inode() const override {
    // Returns interface type (covariant with concrete fb::inode_view_impl)
    return inode_shared();
  }
  
  // FB-specific helper (returns concrete type)
  std::shared_ptr<fb::inode_view_impl const> inode_shared() const;
  
private:
  // FB-specific data
};

class chunk_view : public chunk_view_interface {
  // Implement interface
};

class chunk_range : public chunk_range_interface {
public:
  class iterator : public iterator_interface {
    // Implement iterator interface
  };
  
  std::unique_ptr<iterator_interface> begin() const override;
  std::unique_ptr<iterator_interface> end() const override;
  
private:
  // FB-specific data
};

} // namespace flatbuffers_backend
```

### Layer 3: Type Aliases (Format-Dependent)

```cpp
// include/dwarfs/reader/metadata_types.h
namespace dwarfs::reader::internal {

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
// FlatBuffers-only: use concrete types directly (performance)
using inode_view_impl = flatbuffers_backend::inode_view_impl;
using dir_entry_view_impl = flatbuffers_backend::dir_entry_view_impl;
using global_metadata = flatbuffers_backend::global_metadata;
using chunk_range = flatbuffers_backend::chunk_range;

#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
// Thrift-only: use concrete types directly (performance)
using inode_view_impl = thrift_backend::inode_view_impl;
using dir_entry_view_impl = thrift_backend::dir_entry_view_impl;
using global_metadata = thrift_backend::global_metadata;
using chunk_range = thrift_backend::chunk_range;

#else
// Multi-format: use interface types (polymorphism)
using inode_view_impl = inode_view_interface;
using dir_entry_view_impl = dir_entry_view_interface;
using global_metadata = global_metadata_interface;
using chunk_range = chunk_range_interface;
#endif

} // namespace internal
```

### Layer 4: Wrapper Types (Format-Agnostic)

```cpp
// Wrapper types work identically in all build configurations
class inode_view {
public:
  explicit inode_view(std::shared_ptr<internal::inode_view_impl const> iv)
      : iv_{std::move(iv)} {}
      
  // Delegate to interface (works in all configs)
  mode_type mode() const { return iv_->mode(); }
  bool is_directory() const { return iv_->is_directory(); }
  
  internal::inode_view_impl const& raw() const { return *iv_; }
  
private:
  std::shared_ptr<internal::inode_view_impl const> iv_;
};
```

---

## Implementation Phases (Compressed Timeline)

### Phase A: Add Inheritance to Backend Types (3-4h → 2-3h)

**Objective**: Make all backend types inherit from interface types.

#### A.1: FlatBuffers Backend Inheritance (1.5h → 1h)

**Files**:
- `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`
- `src/reader/internal/metadata_types_flatbuffers.cpp`

**Tasks**:
- [ ] Modify `fb::inode_view_impl` to inherit from `inode_view_interface`
- [ ] Modify `fb::dir_entry_view_impl` to inherit from `dir_entry_view_interface`
- [ ] Modify `fb::chunk_view` to inherit from `chunk_view_interface`
- [ ] Modify `fb::global_metadata` to inherit from `global_metadata_interface`
- [ ] Add `override` keywords to all interface methods
- [ ] Keep backend-specific methods (not in interface)

**Validation**:
```bash
# Must still compile
ninja -C build-flatbuffers-only mkdwarfs
# Expected: SUCCESS
```

#### A.2: Thrift Backend Inheritance (1.5h → 1h)

**Files**:
- `include/dwarfs/reader/internal/metadata_types_thrift.h`
- `src/reader/internal/metadata_types_thrift.cpp`

**Tasks**:
- [ ] Modify `tb::inode_view_impl` to inherit from `inode_view_interface`
- [ ] Modify `tb::dir_entry_view_impl` to inherit from `dir_entry_view_interface`
- [ ] Modify `tb::chunk_view` to inherit from `chunk_view_interface`
- [ ] Modify `tb::global_metadata` to inherit from `global_metadata_interface`
- [ ] Add `override` keywords
- [ ] Keep Thrift-specific methods

**Validation**:
```bash
# Must still compile FlatBuffers (shouldn't break)
ninja -C build-flatbuffers-only mkdwarfs
```

#### A.3: Add chunk_range Inheritance (1h → 0.5h)

**Challenge**: `chunk_range` has complex iterator interface.

**Solution**: Make iterator polymorphic too.

**Files**:
- `include/dwarfs/reader/internal/metadata_view_interface.h` (add chunk_range_interface)
- Both backend headers

**Tasks**:
- [ ] Design `chunk_range_interface` with polymorphic iterator
- [ ] Implement in both backends
- [ ] Test iteration works

---

### Phase B: Update Type Aliases (0.5h → 0.3h)

**Objective**: Make `internal::TYPE` aliases point to interfaces in multi-format builds.

**File**: `include/dwarfs/reader/metadata_types.h` (lines 56-71)

**Current**:
```cpp
#else
// Forward declarations - INCOMPLETE
class inode_view_impl;
#endif
```

**Target**:
```cpp
#else
// Multi-format: use interface types
using inode_view_impl = inode_view_interface;
using dir_entry_view_impl = dir_entry_view_interface;
using global_metadata = global_metadata_interface;
using chunk_range = chunk_range_interface;
#endif
```

**Validation**:
```bash
# FlatBuffers must still work
ninja -C build-flatbuffers-only mkdwarfs

# Dual-format error count should drop dramatically
ninja -C build-benchmark 2>&1 | grep "error:" | wc -l
# Expected: <10 errors
```

---

### Phase C: Add Upcasting at Creation Points (2-3h → 1.5-2h)

**Objective**: Backends upcast concrete types to interface types when creating wrappers.

#### C.1: FlatBuffers Upcasting (1h)

**Files**:
- `src/reader/internal/metadata_v2_flatbuffers.cpp`

**Pattern**:
```cpp
inode_view make_inode_view(uint32_t inode) const {
  auto concrete = std::make_shared<fb::inode_view_impl>(...);
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: upcast to interface
  return inode_view{std::static_pointer_cast<inode_view_interface const>(concrete)};
#else
  // Single-format: use directly (internal::inode_view_impl IS fb::inode_view_impl)
  return inode_view{concrete};
#endif
}
```

**Tasks**:
- [ ] Add conditional upcasting to `make_inode_view()` (line ~610)
- [ ] Add conditional upcasting to `make_dir_entry_view()` (line ~620)
- [ ] Add conditional upcasting in `readdir()` (lines ~1995-2013)
- [ ] Add conditional upcasting in `find_impl()` (line ~1926)
- [ ] Update constructor initialization for `root_` (line ~751)

**Validation**:
```bash
ninja -C build-flatbuffers-only mkdwarfs  # Must work
ninja -C build-benchmark 2>&1 | grep "metadata_v2_flatbuffers.*error:" | wc -l
# Expected: 0
```

#### C.2: Thrift Upcasting (1h)

**Files**:
- `src/reader/internal/metadata_v2_thrift.cpp`

**Tasks**:
- [ ] Same pattern as FlatBuffers
- [ ] Add conditional upcasting to all wrapper creation points
- [ ] Lines: 535 (make_inode_view), 541 (make_dir_entry_view), 704 (root_), 2166-2183 (readdir), 2097 (find_impl)

**Validation**:
```bash
ninja -C build-benchmark 2>&1 | grep "metadata_v2_thrift.*error:" | wc -l
# Expected: <5
```

#### C.3: Fix metadata_types.cpp parent() (0.5h)

**File**: `src/reader/metadata_types.cpp` (lines 115-147)

**Problem**: Complex casting logic that tries to downcast interfaces.

**Solution**: Simplify - work through interface only.

```cpp
std::optional<dir_entry_view> dir_entry_view::parent() const {
  if (auto p = impl_->parent()) {
    // p is unique_ptr<dir_entry_view_interface>
    // Convert to shared_ptr for wrapper
    return dir_entry_view{
      std::shared_ptr<dir_entry_view_interface const>(std::move(p))
    };
  }
  return std::nullopt;
}
```

---

### Phase D: Fix Return Type Mismatches (1-2h → 1h)

**Objective**: Align virtual function return types.

#### D.1: chunk_range Return Type (0.5h)

**Problem**: Interface expects `chunk_range`, backends return `fb::chunk_range` or `tb::chunk_range`.

**Solution Option 1**: Make backends return interface type with wrapper
```cpp
// In metadata_v2_flatbuffers.cpp
chunk_range get_chunks(int inode, std::error_code& ec) const override {
  auto backend_range = get_chunk_range_backend(inode, ec);
  return chunk_range_wrapper{std::make_unique<fb::chunk_range>(backend_range)};
}
```

**Solution Option 2**: Make metadata_v2::impl use templates
```cpp
template <typename ChunkRange>
class metadata_impl : public metadata_v2::impl {
  ChunkRange get_chunks(...) override;
};
```

**Recommended**: Option 1 (wrapper), cleaner and follows Strategy Pattern.

**Files**:
- Create `include/dwarfs/reader/internal/chunk_range_wrapper.h`
- Modify both backend `metadata_v2_data::get_chunks()` methods

#### D.2: Fix Other Type Mismatches (0.5h)

**Files**:
- `src/reader/internal/metadata_factory.cpp:102` - Bundled.view()
- `src/reader/internal/metadata_types_thrift.cpp:1230` - append_to()

**Tasks**:
- [ ] Fix Bundled access (Phase 7 from old plan)
- [ ] Add append_to() to interface or remove usage

---

### Phase E: Testing & Validation (1-2h → 1h)

#### E.1: Build Matrix (0.5h)

**Configurations to test**:
```bash
# 1. FlatBuffers-only
cmake -B build-fb-only -GNinja -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
ninja -C build-fb-only
ctest --test-dir build-fb-only

# 2. Thrift-only  
cmake -B build-thrift-only -GNinja -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
ninja -C build-thrift-only
ctest --test-dir build-thrift-only

# 3. Dual-format (target configuration)
cmake -B build-dual -GNinja -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
ninja -C build-dual
ctest --test-dir build-dual
```

**Success Criteria**:
- [ ] All 3 configs compile (0 errors)
- [ ] mkdwarfs runs in all configs
- [ ] Can create images in all configs
- [ ] Can read images created by any config

#### E.2: Runtime Testing (0.5h)

```bash
# Create test filesystem
mkdir -p /tmp/test-dwarfs
echo "test" > /tmp/test-dwarfs/file.txt

# Test all configs
./build-fb-only/mkdwarfs -i /tmp/test-dwarfs -o test-fb.dwarfs
./build-thrift-only/mkdwarfs -i /tmp/test-dwarfs -o test-thrift.dwarfs
./build-dual/mkdwarfs -i /tmp/test-dwarfs -o test-dual-fb.dwarfs --metadata-format=flatbuffers
./build-dual/mkdwarfs -i /tmp/test-dwarfs -o test-dual-thrift.dwarfs --metadata-format=thrift

# Cross-read test (dual-format should read all)
./build-dual/dwarfsck test-fb.dwarfs
./build-dual/dwarfsck test-thrift.dwarfs
./build-dual/dwarfsck test-dual-fb.dwarfs
./build-dual/dwarfsck test-dual-thrift.dwarfs
```

---

## Implementation Order (Optimized)

### Session 1: Interface Inheritance (3-4h)
1. ✅ Create this plan document
2. A.1: FlatBuffers inheritance (1h)
3. A.2: Thrift inheritance (1h)
4. A.3: chunk_range inheritance (0.5h)
5. B: Update type aliases (0.3h)
6. Validation: Verify dramatically reduced errors

### Session 2: Upcasting & Fixes (2-3h)
7. C.1: FlatBuffers upcasting (1h)
8. C.2: Thrift upcasting (1h)
9. C.3: Fix metadata_types.cpp (0.5h)
10. D.1: chunk_range wrapper (0.5h)
11. D.2: Fix remaining mismatches (0.5h)

### Session 3: Testing (1-2h)
12. E.1: Build all configurations (0.5h)
13. E.2: Runtime testing (0.5h)
14. Fix any remaining issues (0.5h)
15. Documentation update (0.5h)

**Total Compressed**: 6-9 hours

---

## Key Design Decisions

### Decision 1: Interface Methods Must Be Sufficient
All operations needed by wrapper types must be in interface.

**Check**: Review all uses of `iv.raw()`, `entry.raw()` - these access backend-specific members. Convert to interface method calls.

### Decision 2: Backend-Specific Methods Allowed
Backends can have extra methods not in interface (for internal use).

Example:
```cpp
class fb::inode_view_impl : public inode_view_interface {
public:
  // Interface methods
  uint32_t inode_num() const override;
  
  // Backend-specific (for internal use)
  uint32_t mode_index() const { return inode_data_->mode_index(); }
};
```

### Decision 3: Minimize Virtual Calls
Only wrapper creation and top-level operations use polymorphism. Internal backend operations remain non-virtual for performance.

### Decision 4: No #ifdef in Business Logic
Conditional compilation only at:
1. Type alias definitions
2. Wrapper creation points (upcasting)
3. Backend implementations

Business logic (algorithms, workflows) must be format-agnostic.

---

## Risk Mitigation

### Risk 1: Breaking FlatBuffers-Only Build
**Mitigation**: Test after EVERY change
```bash
ninja -C build-flatbuffers-only mkdwarfs || git checkout HEAD -- <file>
```

### Risk 2: Performance Regression
**Impact**: Virtual calls add overhead

**Mitigation**:
- Measure before/after
- Interface calls only at API boundaries
- Internal backend operations stay non-virtual

### Risk 3: Complex Iterator Polymorphism
**Challenge**: `chunk_range` iterator is complex

**Solution**: Type-erased iterator wrapper (like `std::function` pattern)

---

## Success Criteria

### Build Success
- [ ] build-flatbuffers-only: 0 errors, mkdwarfs runs
- [ ] build-thrift-only: 0 errors, mkdwarfs runs
- [ ] build-dual: 0 errors, all tools run

### Runtime Success
- [ ] Can create FlatBuffers images in all configs
- [ ] Can create Thrift images in dual-format config
- [ ] Can read any image format in dual-format config
- [ ] No crashes, no errors

### Architecture Success
- [ ] Clean inheritance hierarchy
- [ ] No #ifdef in business logic
- [ ] Easy to add 3rd format (Protocol Buffers, Cap'n Proto)
- [ ] MECE: Each class single responsibility
- [ ] Open/Closed: Add format without modifying existing code

### Test Success
- [ ] All unit tests pass
- [ ] No regressions
- [ ] New tests for multi-format scenarios

---

## Files to Modify (Complete List)

### Phase A.1: FlatBuffers Inheritance
1. `include/dwarfs/reader/internal/metadata_types_flatbuffers.h` (325 lines)
2. `src/reader/internal/metadata_types_flatbuffers.cpp` (~600 lines)

### Phase A.2: Thrift Inheritance  
3. `include/dwarfs/reader/internal/metadata_types_thrift.h` (415 lines)
4. `src/reader/internal/metadata_types_thrift.cpp` (~1200 lines)

### Phase A.3: chunk_range Interface
5. `include/dwarfs/reader/internal/metadata_view_interface.h` (add chunk_range_interface)

### Phase B: Type Aliases
6. `include/dwarfs/reader/metadata_types.h` (already modified - lines 56-71)

### Phase C: Upcasting
7. `src/reader/internal/metadata_v2_flatbuffers.cpp` (2390 lines - 10 creation points)
8. `src/reader/internal/metadata_v2_thrift.cpp` (2365 lines - 10 creation points)
9. `src/reader/metadata_types.cpp` (240 lines - parent() method)

### Phase D: Return Types
10. `src/reader/internal/metadata_factory.cpp` (line 102)
11. `src/reader/internal/metadata_types_thrift.cpp` (line 1230)

### Phase E: Testing
12. Create test scripts

**Total**: 11 source files + 1 test script

---

## Reference Implementation Pattern

### Backend Type with Inheritance

```cpp
// Header
class inode_view_impl : public inode_view_interface {
public:
  // Standard constructor
  inode_view_impl(Data data, uint32_t ino, Meta meta);
  
  // Interface methods (pure virtual in base)
  uint32_t inode_num() const override { return inode_num_; }
  mode_type mode() const override;
  bool is_directory() const override;
  // ... all interface methods
  
  // Backend-specific methods (not in interface)
  uint32_t mode_index() const { return data_->mode_index(); }
  
private:
  Data const* data_;
  uint32_t inode_num_;
  Meta const* meta_;
};
```

### Wrapper Creation with Upcasting

```cpp
inode_view make_inode_view(uint32_t inode) const {
  auto concrete_impl = std::make_shared<fb::inode_view_impl>(...);
  
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Multi-format: upcast to interface for polymorphism
  return inode_view{std::static_pointer_cast<inode_view_interface const>(concrete_impl)};
#else
  // Single-format: use directly (internal::inode_view_impl IS fb::inode_view_impl via alias)
  return inode_view{concrete_impl};
#endif
}
```

### Working with Incomplete Types

**Problem**: In multi-format, can't use `iv.raw()` to get backend type.

**Solution**: Extract needed data, call appropriate overload:
```cpp
// BEFORE (doesn't work in multi-format):
directory_view make_directory_view(inode_view const& iv) const {
  return make_directory_view(iv.raw());  // iv.raw() returns incomplete type
}

// AFTER (works in all configs):
directory_view make_directory_view(inode_view const& iv) const {
  DWARFS_CHECK(iv.is_directory(), "not a directory");
  return make_directory_view(iv.inode_num());  // Just pass inode number
}
```

---

## Completion Checklist

### Phase A: Inheritance
- [ ] A.1: FlatBuffers backend inherits from interfaces
- [ ] A.2: Thrift backend inherits from interfaces
- [ ] A.3: chunk_range inheritance implemented
- [ ] FlatBuffers-only still compiles and runs

### Phase B: Aliases
- [ ] Type aliases use interfaces in multi-format builds
- [ ] Single-format builds use concrete types (performance)

### Phase C: Upcasting
- [ ] FlatBuffers wrapper creation upcasts properly
- [ ] Thrift wrapper creation upcasts properly
- [ ] metadata_types.cpp simplified (no backend-specific casts)

### Phase D: Return Types
- [ ] chunk_range return type unified
- [ ] metadata_factory.cpp fixed
- [ ] metadata_types_thrift.cpp fixed

### Phase E: Testing
- [ ] All 3 build configs succeed
- [ ] Runtime tests pass
- [ ] No regressions

### Documentation
- [ ] Update memory bank with new architecture
- [ ] Move old dual-format docs to old-docs/
- [ ] Update README.adoc with multi-format support
- [ ] Document Strategy Pattern architecture

---

## Expected Error Reduction Timeline

| Phase | Error Count | Change |
|-------|-------------|--------|
| Current | 37 | Baseline |
| After A (Inheritance) | 37 | Same (just structure) |
| After B (Aliases) | ~10 | -27 (type system now consistent) |
| After C (Upcasting) | ~5 | -5 (creation points fixed) |
| After D (Returns) | 0 | -5 (final fixes) |

---

## Document Version
**Version**: 1.0  
**Created**: 2025-11-28 10:58 HKT  
**Status**: READY FOR IMPLEMENTATION  
**Estimated Completion**: 2025-11-28 19:00 HKT (8 hours from now)