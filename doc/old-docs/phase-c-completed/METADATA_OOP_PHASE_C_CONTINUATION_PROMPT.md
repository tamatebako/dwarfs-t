# Metadata OOP Refactoring - Phase C Continuation Prompt

**Date**: 2025-11-28 16:47 HKT (Session 4 Start)  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Last Commit**: 01fb2909 - "docs(metadata): Update status tracker - Phase B 100% complete (70% overall)"  
**Working Directory**: `/Users/mulgogi/src/external/dwarfs`

---

## Quick Context

**Current Status**: Phase B Complete (70% overall)

**Phase B Achievements** (Sessions 1-3):
- ✅ All interfaces extended with 11 new methods
- ✅ Type visibility solved via `metadata_types_fwd.h`
- ✅ Both backends (FlatBuffers + Thrift) fully implement interfaces
- ✅ Expected: 0 compilation errors in dual-format builds

**Phase C Objective**: Add conditional upcasting at object creation points

**Why Needed**: In dual-format builds, wrapper objects expect interface types, but backends create concrete types. We need conditional upcasting ONLY in dual-format builds.

---

## Start-of-Session Commands

```bash
# 1. Navigate to project
cd /Users/mulgogi/src/external/dwarfs

# 2. Read session documents (MANDATORY)
cat doc/METADATA_OOP_SESSION3_COMPLETE.md
cat doc/METADATA_OOP_REFACTORING_STATUS.md
cat doc/METADATA_OOP_PHASE_C_CONTINUATION_PROMPT.md

# 3. Verify baseline (MUST stay working)
# Build FlatBuffers-only to confirm baseline
cd build-flatbuffers-only && make mkdwarfs -j4
# Expected: SUCCESS (or use cmake --build if make unavailable)

# 4. Check if dual-format has errors
cd build-benchmark && make mkdwarfs -j4 2>&1 | grep "error:" | wc -l
# Expected after Phase B: 0 errors
# If errors remain, document them before proceeding

# 5. Verify branch and status
git status --short
git log --oneline -5

# 6. Create session backup
git branch backup-before-phase-c-$(date +%Y%m%d-%H%M)
```

---

## Phase C: Conditional Upcasting (1.5-2h)

### Overview

**Goal**: Add conditional upcasting at object creation points so dual-format builds return interface types while single-format builds use concrete types directly.

**Principle**: 
- Single-format builds: Use concrete types (best performance, no overhead)
- Dual-format builds: Upcast to interfaces (enables polymorphism)

**Pattern**:
```cpp
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: upcast concrete → interface
  return wrapper_type{std::static_pointer_cast<interface_type const>(concrete)};
#else
  // Single-format: use concrete type directly
  return wrapper_type{concrete};
#endif
```

---

## Phase C.1: FlatBuffers Backend Upcasting (0.75h)

**File**: `src/reader/internal/metadata_v2_flatbuffers.cpp`

**Locations to Modify** (6 locations):

### 1. `make_inode_view()` method (~line 615)
**Current**:
```cpp
inode_view make_inode_view(uint32_t inode_num) const {
  return inode_view{fb::inode_view_impl::from_inode_num_shared(inode_num, *meta_)};
}
```

**Updated**:
```cpp
inode_view make_inode_view(uint32_t inode_num) const {
  auto concrete = fb::inode_view_impl::from_inode_num_shared(inode_num, *meta_);
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  return inode_view{std::static_pointer_cast<inode_view_interface const>(concrete)};
#else
  return inode_view{concrete};
#endif
}
```

### 2. `make_dir_entry_view()` method (~line 625)
**Current**:
```cpp
dir_entry_view make_dir_entry_view(uint32_t self_index, uint32_t parent_index) const {
  return dir_entry_view{
      fb::dir_entry_view_impl::from_dir_entry_index_shared(self_index, parent_index, *meta_)};
}
```

**Updated**:
```cpp
dir_entry_view make_dir_entry_view(uint32_t self_index, uint32_t parent_index) const {
  auto concrete = fb::dir_entry_view_impl::from_dir_entry_index_shared(
      self_index, parent_index, *meta_);
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  return dir_entry_view{std::static_pointer_cast<dir_entry_view_interface const>(concrete)};
#else
  return dir_entry_view{concrete};
#endif
}
```

### 3. `root_` initialization in constructor (~line 755)
**Current**:
```cpp
: meta_{std::move(meta)}
, root_{make_dir_entry_view(0, 0)}
```

**Updated**: No change needed (uses make_dir_entry_view which already handles upcasting)

### 4. `find_impl()` return (~line 1931)
**Current**:
```cpp
return dir_entry_view{
    fb::dir_entry_view_impl::from_dir_entry_index_shared(index, *meta_)};
```

**Updated**:
```cpp
auto concrete = fb::dir_entry_view_impl::from_dir_entry_index_shared(index, *meta_);
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
return dir_entry_view{std::static_pointer_cast<dir_entry_view_interface const>(concrete)};
#else
return dir_entry_view{concrete};
#endif
```

### 5-7. `readdir()` returns (3 locations, ~lines 2000-2017)
**Pattern**: Same as find_impl(), apply to all 3 return statements

**Locations**:
- Self entry return
- Parent entry return  
- Regular entry return

**Strategy**: Apply same upcasting pattern to each

---

## Phase C.2: Thrift Backend Upcasting (0.75h)

**File**: `src/reader/internal/metadata_v2_thrift.cpp`

**Locations to Modify** (6 locations):

### 1. `make_inode_view()` (~line 539)
### 2. `make_dir_entry_view()` (~line 545)
### 3. `root_` initialization (~line 708) - may use factory method
### 4. `find_impl()` (~line 2105)
### 5-7. `readdir()` returns (~lines 2174-2191, 3 locations)

**Apply exact same pattern as FlatBuffers**, just with `tb::` namespace instead of `fb::`.

---

## Phase C.3: Simplify metadata_types.cpp (0.25h)

**File**: `src/reader/metadata_types.cpp`

**Current Problem** (lines 115-147):
```cpp
std::optional<dir_entry_view> dir_entry_view::parent() const {
  if (is_root()) {
    return std::nullopt;
  }

#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
  // FlatBuffers-only: downcast to concrete
  auto fb_impl = std::dynamic_pointer_cast<
      flatbuffers_backend::dir_entry_view_impl const>(impl_);
  if (auto p = fb_impl->parent_shared()) {
    return dir_entry_view{p};
  }
#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  // Thrift-only: downcast to concrete
  auto tb_impl = std::dynamic_pointer_cast<
      thrift_backend::dir_entry_view_impl const>(impl_);
  if (auto p = tb_impl->parent_shared()) {
    return dir_entry_view{p};
  }
#else
  // Dual-format: complex logic
  ...
#endif

  return std::nullopt;
}
```

**Solution**: Work through interface only (no downcasting needed)

```cpp
std::optional<dir_entry_view> dir_entry_view::parent() const {
  if (is_root()) {
    return std::nullopt;
  }

  // Call interface method parent()
  if (auto p = impl_->parent()) {
    // Convert unique_ptr<interface> → shared_ptr<interface>
    return dir_entry_view{
        std::shared_ptr<dir_entry_view_interface const>(std::move(p))};
  }

  return std::nullopt;
}
```

**Key Insight**: After Phase B, `parent()` is an interface method, so we don't need backend-specific code!

---

## Validation Strategy

### After Each Sub-Phase

```bash
# 1. ALWAYS verify FlatBuffers baseline
cd build-flatbuffers-only && make clean && make mkdwarfs -j4
# MUST succeed every time

# 2. Check dual-format progress
cd build-benchmark && make mkdwarfs -j4 2>&1 | tee build.log
grep "error:" build.log | wc -l

# 3. Commit progress
git add -A
git commit -m "feat(metadata): Phase C.X - <description>"
```

### Special Note on Conditional Compilation

The upcasting guards use:
```cpp
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
```

This ONLY activates in dual-format builds. Single-format builds skip the overhead.

---

## Expected Outcomes

### Success Criteria

After Phase C completion:
- ✅ FlatBuffers-only: Still compiles (0 errors)
- ✅ Thrift-only: Compiles (0 errors) 
- ✅ Dual-format: Compiles (0 errors)
- ✅ All wrapper types receive correct interface pointers in dual-format
- ✅ No performance regression in single-format builds

### Error Tracking

| Checkpoint | Expected Errors | Notes |
|------------|----------------|-------|
| After C.1 (FlatBuffers) | 0-3 | May have Thrift-side mismatches |
| After C.2 (Thrift) | 0-1 | May have metadata_types.cpp issue |
| After C.3 (Simplify) | 0 | All complete |

---

## Common Pitfalls & Solutions

### Pitfall 1: Breaking FlatBuffers Baseline
**Symptom**: build-flatbuffers-only fails  
**Solution**: Revert immediately. The `#if` guard may be wrong.

**Check**: Ensure guard is `FLATBUFFERS && THRIFT` (both must be true), not `FLATBUFFERS || THRIFT`

### Pitfall 2: Forgetting std::move() for unique_ptr
**Symptom**: Compilation error about deleted copy constructor  
**Solution**: Use `std::move(p)` when converting `unique_ptr` to `shared_ptr`

### Pitfall 3: Wrong Pointer Cast Direction
**Symptom**: Runtime error or crash  
**Solution**: Always upcast (concrete → interface), never downcast in wrapper creation

### Pitfall 4: Missing const in Cast
**Symptom**: Type mismatch error  
**Solution**: Use `std::static_pointer_cast<interface_type const>()` with const

---

## Testing Checklist (After Phase C Complete)

```bash
# 1. Clean build all configs
rm -rf build-{flatbuffers-only,benchmark}/*

# 2. FlatBuffers-only
cd build-flatbuffers-only
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF
ninja mkdwarfs
./mkdwarfs --version  # Quick smoke test

# 3. Dual-format
cd build-benchmark
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON
ninja mkdwarfs
./mkdwarfs --version

# 4. Expected: Both succeed
```

---

## Reference: Complete Upcasting Pattern

```cpp
// In backend implementation file (metadata_v2_flatbuffers.cpp or metadata_v2_thrift.cpp)

wrapper_type create_wrapper(...) {
  // Step 1: Create concrete object
  auto concrete = backend::concrete_type::factory_method(...);
  
  // Step 2: Conditional upcast
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  // Dual-format: upcast to interface
  return wrapper_type{
      std::static_pointer_cast<interface_type const>(concrete)};
#else
  // Single-format: use concrete directly
  return wrapper_type{concrete};
#endif
}
```

---

## After Phase C: What's Next?

**Phase D**: Fix Return Type Mismatches (1h)
- May not be needed if Phase C resolves all issues
- Check for any remaining type mismatches

**Phase E**: Testing & Validation (1h)
- Build all 3 configurations
- Run test suites
- Runtime validation

**Estimated Completion**: 2.5-3h for Phases C+D+E

---

## Session End Checklist

After completing Phase C:

- [ ] All 6 FlatBuffers upcasting locations updated
- [ ] All 6 Thrift upcasting locations updated
- [ ] metadata_types.cpp simplified (no downcasting)
- [ ] FlatBuffers-only build: SUCCESS
- [ ] Dual-format build: SUCCESS  
- [ ] All changes committed with clear messages
- [ ] Status tracker updated
- [ ] Session summary created

---

## Quick Reference: File Locations

**FlatBuffers Backend**:
- `src/reader/internal/metadata_v2_flatbuffers.cpp` (lines ~615, ~625, ~1931, ~2000-2017)

**Thrift Backend**:
- `src/reader/internal/metadata_v2_thrift.cpp` (lines ~539, ~545, ~2105, ~2174-2191)

**Wrapper Code**:
- `src/reader/metadata_types.cpp` (lines ~115-147 for parent() simplification)

---

## If Issues Arise

### Minor Compilation Errors (<5)
**Action**: Debug and fix in this session

### Significant Architecture Issues
**Action**: 
1. Document the issue in detail
2. Commit current working state
3. Create analysis document
4. Request guidance before proceeding

### Cannot Complete in Time
**Action**:
1. Complete what you can (prioritize FlatBuffers → Thrift → Simplify)
2. Commit all working code
3. Document remaining work clearly
4. Create continuation plan for next session

---

**Ready to Begin?**

Start with **Phase C.1**: FlatBuffers backend upcasting (6 locations).

**Good luck!** 🚀

---

**Document Version**: 1.0  
**Created**: 2025-11-28 16:47 HKT  
**For**: Session 4 (Phase C: Conditional Upcasting)  
**Previous**: Session 3 Summary in `doc/METADATA_OOP_SESSION3_COMPLETE.md`