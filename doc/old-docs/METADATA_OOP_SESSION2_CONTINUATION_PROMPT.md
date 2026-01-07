# Metadata OOP Refactoring - Session 2 Continuation Prompt

**Date**: 2025-11-28 11:20 HKT (Session 2 Start)  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Last Commit**: 4f95f30f - "docs(metadata): Session 1 summary and status update"  
**Working Directory**: `/Users/mulgogi/src/external/dwarfs`

---

## Quick Context

**Session 1 Achievement** (20 minutes): ✅ Phases A & B structurally complete!

- Discovered inheritance ALREADY DONE (saved 2-3h!)
- Added chunk_range inheritance
- Updated type aliases to interfaces
- FlatBuffers-only: ✅ SUCCESS
- Dual-format: 🔴 114 errors (expected - incomplete interfaces)

**Current Status**: 20% complete, on track

---

## Session 2 Objective

**Goal**: Extend interfaces to complete Phase B → Enable clean compilation

**Target**: Reduce 114 errors → <20 errors

**Time Budget**: 1.5-2 hours

---

## Start-of-Session Commands

```bash
# 1. Navigate to project
cd /Users/mulgogi/src/external/dwarfs

# 2. Read continuation documents (MANDATORY)
cat doc/METADATA_OOP_REFACTORING_STATUS.md
cat doc/METADATA_OOP_SESSION1_SUMMARY.md
cat doc/METADATA_OOP_REFACTORING_PLAN.md

# 3. Verify baseline (MUST stay working)
ninja -C build-flatbuffers-only mkdwarfs
# Expected: SUCCESS

# 4. Check current error count
ninja -C build-benchmark 2>&1 | grep "error:" | wc -l
# Current: 114 errors

# 5. Verify branch and status
git status --short
git log --oneline -5

# 6. Create session backup
git branch backup-before-session2-$(date +%Y%m%d-%H%M)
```

---

## Phase B.1: Extend Interfaces (PRIMARY FOCUS)

**Objective**: Add missing methods to interfaces so wrapper code compiles

**Time**: 1-1.5 hours

### Error Category Analysis

From build output, wrapper code needs these methods:

#### 1. `inode_view_interface` Missing Methods

**File**: `include/dwarfs/reader/internal/metadata_view_interface.h` (line 120)

**Add**:
```cpp
virtual posix_file_type::value type() const = 0;
```

**Used by**: `metadata_types.cpp:87,90,94,98`

**Why**: Wrapper checks file type (regular file, directory, symlink, etc.)

#### 2. `dir_entry_view_interface` Missing Methods

**File**: `include/dwarfs/reader/internal/metadata_view_interface.h` (line 170)

**Add**:
```cpp
// Path methods for different platforms
virtual std::string unix_path() const = 0;
virtual std::filesystem::path fs_path() const = 0;
virtual std::wstring wpath() const = 0;

// Backend-specific shared pointer access (optional - may refactor away)
// Note: These return concrete types, consider alternatives
```

**Used by**: `metadata_types.cpp:151,154,157`

**Why**: Cross-platform path handling

#### 3. `global_metadata_interface` Missing Methods

**File**: `include/dwarfs/reader/internal/metadata_view_interface.h` (line 246)

**Add**:
```cpp
virtual uint32_t first_dir_entry(uint32_t ino) const = 0;
virtual uint32_t parent_dir_entry(uint32_t ino) const = 0;
virtual uint32_t self_dir_entry(uint32_t ino) const = 0;
```

**Used by**: `metadata_types.cpp:165,172,173`

**Why**: Directory navigation

#### 4. Consider: Helper Methods vs Refactoring

Some methods like `inode_shared()`, `parent_shared()` return backend-specific `shared_ptr<concrete_type>`.

**Options**:
1. Add to interface (returns shared_ptr<interface>)
2. Refactor wrapper code to not need them

**Recommendation**: Add to interface, return interface pointers (Option 1)

### Implementation Steps (Phase B.1)

#### Step 1: Add type() to inode_view_interface (15 min)

1. Modify `metadata_view_interface.h`
2. Implement in both backends (`metadata_types_flatbuffers.cpp`, `metadata_types_thrift.cpp`)
3. Test FlatBuffers-only: `ninja -C build-flatbuffers-only mkdwarfs`

#### Step 2: Add path methods to dir_entry_view_interface (20 min)

1. Add `unix_path()`, `fs_path()`, `wpath()` to interface
2. Implement in both backends
3. Test FlatBuffers-only

#### Step 3: Add dir_entry methods to global_metadata_interface (15 min)

1. Add `first_dir_entry()`, `parent_dir_entry()`, `self_dir_entry()`
2. Implement in both backends
3. Test FlatBuffers-only

#### Step 4: Add shared pointer helpers (20 min)

Consider adding:
```cpp
// To dir_entry_view_interface
virtual std::unique_ptr<dir_entry_view_interface> parent() const = 0;

// Backend implementations return unique_ptr wrapping concrete type
```

#### Step 5: Verify error reduction (10 min)

```bash
ninja -C build-benchmark 2>&1 | grep "error:" | wc -l
# Expected: ~40-50 errors (from 114)
```

---

## Phase B.2: Fix Type Visibility (SECONDARY)

**Objective**: Make `chunk_range` type visible in internal headers

**Time**: 0.25-0.5 hours

### Problem

Internal headers like `inode_reader_v2.h` use `chunk_range` but don't have access to the type alias from `metadata_types.h`.

### Solution Options

#### Option A: Forward Declaration (Cleanest)

In files that need chunk_range:
```cpp
namespace dwarfs::reader::internal {
  class chunk_range_interface;
  using chunk_range = chunk_range_interface; // In dual-format
}
```

#### Option B: Include Strategy

Create `internal/metadata_types_fwd.h`:
```cpp
#pragma once
#include <dwarfs/reader/internal/metadata_view_interface.h>

namespace dwarfs::reader::internal {
#if defined(DWARFS_HAVE_FLATBUFFERS) && !defined(DWARFS_HAVE_THRIFT)
  using chunk_range = flatbuffers_backend::chunk_range;
#elif defined(DWARFS_HAVE_THRIFT) && !defined(DWARFS_HAVE_FLATBUFFERS)
  using chunk_range = thrift_backend::chunk_range;
#else
  using chunk_range = chunk_range_interface;
#endif
}
```

Include this in internal headers.

#### Option C: Fully Qualified (Quick Fix)

Change `chunk_range` → `internal::chunk_range` in internal headers.

**Recommendation**: Option B (cleaner architecture)

### Implementation Steps

1. Create `internal/metadata_types_fwd.h` with forward declarations
2. Include in `inode_reader_v2.h` and other internal headers
3. Test all builds

---

## Validation Strategy

### After Each Sub-Phase

```bash
# 1. Always verify FlatBuffers baseline
ninja -C build-flatbuffers-only mkdwarfs
# MUST succeed

# 2. Check dual-format progress
ninja -C build-benchmark 2>&1 | grep "error:" | wc -l
# Track reduction

# 3. Commit progress
git add -A
git commit -m "feat(metadata): Phase B.X - <description>"
```

### End of Session 2

```bash
# Build all available configs
ninja -C build-flatbuffers-only mkdwarfs
ninja -C build-benchmark 2>&1 | tee build-log.txt

# Count remaining errors
grep "error:" build-log.txt | wc -l

# If <20 errors, proceed to Phase C
# If >20 errors, analyze and continue Phase B
```

---

## Expected Outcomes

### Success Criteria

- ✅ FlatBuffers-only: Still compiles
- ✅ Error count: <50 (from 114)
- ✅ All interface methods implemented in both backends
- ✅ No regression in existing functionality

### Expected Error Distribution (Post B.1)

| Category | Before | After | Delta |
|----------|--------|-------|-------|
| Missing methods | 60 | 0 | -60 |
| Type visibility | 40 | 40 | 0 |
| Abstract returns | 4 | 4 | 0 |
| Test failures | 10 | 10 | 0 |
| **TOTAL** | **114** | **~54** | **-60** |

After Phase B.2, expect: ~10-20 errors remaining

---

## Architectural Principles to Follow

### 1. Interface Completeness

Every method in wrapper code (`metadata_types.cpp`) must have corresponding interface method.

### 2. Backend Symmetry

If FlatBuffers backend implements a method, Thrift backend must too (with same signature).

### 3. Minimal Change

Only add methods actually used. Don't add "nice to have" methods.

### 4. Test After Each Addition

Build FlatBuffers-only after EVERY interface addition. Never break it.

### 5. Covariant Returns

Backends can return concrete types that inherit from interface types:
```cpp
// Interface
virtual std::shared_ptr<inode_view_interface> inode() const = 0;

// Backend (covariant)
std::shared_ptr<fb::inode_view_impl> inode_shared() const;
std::shared_ptr<inode_view_interface> inode() const override {
  return inode_shared();  // Upcast
}
```

---

## Common Pitfalls & Solutions

### Pitfall 1: Forgetting override Keyword
**Symptom**: "function doesn't override any base class method"  
**Solution**: Add `override` to ALL interface method implementations

### Pitfall 2: Return Type Mismatch
**Symptom**: "return type differs"  
**Solution**: Ensure backend method returns interface type, use helper for concrete

### Pitfall 3: Breaking FlatBuffers-Only
**Symptom**: build-flatbuffers-only fails  
**Solution**: Revert last change immediately, reconsider approach

### Pitfall 4: Circular Dependencies
**Symptom**: "incomplete type" compilation errors  
**Solution**: Use forward declarations, careful include order

---

## Files to Modify

### Phase B.1 (Extend Interfaces)

1. `include/dwarfs/reader/internal/metadata_view_interface.h`
   - Add ~6-8 new virtual methods

2. `src/reader/internal/metadata_types_flatbuffers.cpp`
   - Implement new methods (bodies)

3. `src/reader/internal/metadata_types_thrift.cpp`
   - Implement new methods (bodies)

4. Headers may need `override` keywords:
   - `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`
   - `include/dwarfs/reader/internal/metadata_types_thrift.h`

### Phase B.2 (Type Visibility)

5. Create `include/dwarfs/reader/internal/metadata_types_fwd.h` (NEW)

6. Modify headers that use chunk_range:
   - `include/dwarfs/reader/internal/inode_reader_v2.h`
   - Any others flagged by compiler

---

## Reference Patterns

### Adding Interface Method

```cpp
// 1. In metadata_view_interface.h
class inode_view_interface {
public:
  virtual posix_file_type::value type() const = 0;
};

// 2. In metadata_types_flatbuffers.h
class inode_view_impl : public inode_view_interface {
public:
  posix_file_type::value type() const override;
};

// 3. In metadata_types_flatbuffers.cpp
posix_file_type::value inode_view_impl::type() const {
  return posix_file_type::from_mode(mode());
}

// 4. Repeat for Thrift backend
```

### Forward Declaration Pattern

```cpp
// In internal header needing chunk_range
#include <dwarfs/reader/internal/metadata_types_fwd.h>

// Now can use internal::chunk_range
```

---

## Session End Checklist

- [ ] All added interface methods implemented in both backends
- [ ] FlatBuffers-only build: SUCCESS
- [ ] Dual-format error count: <50
- [ ] All changes committed with clear messages
- [ ] Status tracker updated
- [ ] Session summary created

---

## If Session Runs Over Time

**Priority 1**: Complete Phase B.1 (interface extension)  
**Priority 2**: Test and validate  
**Priority 3**: Phase B.2 (type visibility) can defer to Session 3

**Rationale**: Interface completion reduces the most errors (60+)

---

## Next Session Preview (Session 3: Upcasting)

After interfaces are complete and errors <20, Session 3 will:

1. Add conditional upcasting in `metadata_v2_flatbuffers.cpp`
2. Add conditional upcasting in `metadata_v2_thrift.cpp`
3. Fix abstract return types
4. Target: 0 compilation errors

---

## Key Reminders

1. **Test FlatBuffers After EVERY Change** - It's our baseline, never break it
2. **Add override to ALL Interface Methods** - Compiler catches mistakes
3. **Commit After Each Sub-Phase** - Small, reversible commits
4. **Update Status Tracker** - Keep documentation current

---

**Ready to Begin?**

Start with **Phase B.1, Step 1**: Add `type()` method to `inode_view_interface`.

Follow the detailed steps in this prompt.

**Good luck!** 🚀

---

**Document Version**: 1.0  
**Created**: 2025-11-28 11:20 HKT  
**For**: Session 2 (Interface Extension)  
**Previous**: Session 1 Summary in `doc/METADATA_OOP_SESSION1_SUMMARY.md`