# Metadata OOP Refactoring - Session Continuation Prompt

**Date**: 2025-11-28 11:01 HKT  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Commit**: `9347e379` - exploration phase complete  
**Working Directory**: `/Users/mulgogi/src/external/dwarfs`

---

## Quick Context

We're implementing **clean OOP architecture** for DwarFS metadata multi-format support using the **Strategy Pattern**.

**Current Problem**: 37 errors in dual-format build due to incomplete type system.

**Solution**: Add proper inheritance hierarchy where backend types inherit from interface types.

**Principle**: High-level code depends on abstractions, not concrete implementations (Dependency Inversion).

---

## Start-of-Session Commands

```bash
# 1. Navigate to project
cd /Users/mulgogi/src/external/dwarfs

# 2. Read plan and status (MANDATORY)
cat doc/METADATA_OOP_REFACTORING_PLAN.md
cat doc/METADATA_OOP_REFACTORING_STATUS.md

# 3. Verify FlatBuffers baseline (MUST stay working)
ninja -C build-flatbuffers-only mkdwarfs
# Expected: SUCCESS

# 4. Check current error count
ninja -C build-benchmark 2>&1 | grep "error:" | wc -l
# Current: 37 errors

# 5. Check git status
git status --short

# 6. Create new backup if starting major phase
git branch backup-before-phaseA-$(date +%Y%m%d-%H%M)
```

---

## Current Architecture (Broken)

```
┌────────────────────────────────────┐
│    Wrapper Types (Public API)     │
│  inode_view, dir_entry_view, etc. │
└──────────────┬─────────────────────┘
               │ expects
               ▼
    std::shared_ptr<internal::inode_view_impl const>
               │
               └─ INCOMPLETE TYPE in dual-format! ❌
```

## Target Architecture (Clean OOP)

```
┌────────────────────────────────────┐
│    Wrapper Types (Public API)     │
│  inode_view, dir_entry_view, etc. │
└──────────────┬─────────────────────┘
               │ uses
               ▼
    std::shared_ptr<inode_view_interface const>
               │
       ┌───────┴───────┐
       ▼               ▼
┌─────────────┐ ┌─────────────┐
│ FlatBuffers │ │   Thrift    │
│   Backend   │ │   Backend   │
│  (inherits) │ │  (inherits) │
└─────────────┘ └─────────────┘
       ✅               ✅
```

---

## Implementation Strategy

### Phase A: Add Inheritance (START HERE)

**Objective**: Make backend types inherit from interface types.

**Why First**: This is the foundation - all other fixes depend on proper inheritance.

**Steps**:

1. **A.1: FlatBuffers Backend** (1h)
   - Open: `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`
   - Change: `class inode_view_impl : public inode_view_interface`
   - Add: `override` keywords to all interface methods
   - Test: `ninja -C build-flatbuffers-only mkdwarfs` (must succeed)

2. **A.2: Thrift Backend** (1h)
   - Same pattern for Thrift backend
   - Test FlatBuffers still works

3. **A.3: chunk_range Inheritance** (0.5h)
   - More complex due to iterator
   - May need type erasure

**Key Files**:
- `include/dwarfs/reader/internal/metadata_types_flatbuffers.h` (325 lines)
- `src/reader/internal/metadata_types_flatbuffers.cpp` (~600 lines)
- `include/dwarfs/reader/internal/metadata_types_thrift.h` (415 lines)
- `src/reader/internal/metadata_types_thrift.cpp` (~1200 lines)

---

## Detailed Instructions for Phase A.1

### Step 1: Modify inode_view_impl Declaration (15 min)

**File**: `include/dwarfs/reader/internal/metadata_types_flatbuffers.h` (line 95)

**Change**:
```cpp
// BEFORE:
class inode_view_impl {

// AFTER:
class inode_view_impl : public inode_view_interface {
```

**Add override keywords**:
```cpp
mode_type mode() const override;
std::string mode_string() const override;
std::string perm_string() const override;
uid_type getuid() const override;
gid_type getgid() const override;
uint32_t inode_num() const override { return inode_num_; }
bool is_directory() const override { return type() == posix_file_type::directory; }
```

### Step 2: Modify dir_entry_view_impl Declaration (15 min)

**File**: Same file (line 143)

**Change**:
```cpp
// BEFORE:
class dir_entry_view_impl {

// AFTER:
class dir_entry_view_impl : public dir_entry_view_interface {
```

**Add override keywords** to all interface methods.

### Step 3: Modify global_metadata Declaration (10 min)

**File**: Same file (line 57)

**Change**:
```cpp
// BEFORE:
class global_metadata {

// AFTER:
class global_metadata : public global_metadata_interface {
```

**Add override keywords**.

### Step 4: Modify chunk_view Declaration (10 min)

**File**: Same file (line 226)

**Change**:
```cpp
// BEFORE:
class chunk_view {

// AFTER:
class chunk_view : public chunk_view_interface {
```

**Add override keywords**.

### Step 5: Test FlatBuffers (10 min)

```bash
ninja -C build-flatbuffers-only mkdwarfs
# MUST succeed - if not, debug before proceeding
```

---

## Critical Patterns

### Pattern 1: Adding Override Keywords

```cpp
// Before:
uint32_t inode_num() const { return inode_num_; }

// After:
uint32_t inode_num() const override { return inode_num_; }
```

### Pattern 2: Keeping Backend-Specific Methods

```cpp
class inode_view_impl : public inode_view_interface {
public:
  // Interface methods (must have override)
  uint32_t inode_num() const override;
  mode_type mode() const override;
  
  // Backend-specific (no override, not in interface)
  uint32_t mode_index() const { return data_->mode_index(); }
  uint32_t owner_index() const { return data_->owner_index(); }
};
```

### Pattern 3: Covariant Return Types

```cpp
class dir_entry_view_impl : public dir_entry_view_interface {
public:
  // Interface returns abstract type
  std::shared_ptr<inode_view_interface const> inode() const override {
    return inode_shared();  // Covariant: concrete → interface
  }
  
  // Backend-specific helper returns concrete type
  std::shared_ptr<fb::inode_view_impl const> inode_shared() const;
};
```

---

## Validation Commands

### After Each Sub-Phase

```bash
# 1. Verify FlatBuffers (CRITICAL - must always work)
ninja -C build-flatbuffers-only mkdwarfs
# Expected: SUCCESS

# 2. Check dual-format progress
ninja -C build-benchmark 2>&1 | grep "error:" | wc -l
# After A.1+A.2: Expect same (~37)
# After B: Expect ~10
# After C: Expect ~5
# After D: Expect 0

# 3. Commit progress
git add -A
git commit -m "feat(metadata): Phase A.X - <what you completed>"
```

### Full Validation (After All Phases)

```bash
# Build all configs
for config in fb-only thrift-only dual; do
  ninja -C build-$config mkdwarfs
  ctest --test-dir build-$config
done

# Runtime test
./doc/test-multi-format.sh  # Script from Phase E.2
```

---

## Common Pitfalls & Solutions

### Pitfall 1: Breaking FlatBuffers-Only Build
**Symptom**: build-flatbuffers-only fails to compile  
**Solution**: Revert last change, reconsider approach

### Pitfall 2: Forgetting override Keyword
**Symptom**: "function doesn't override any base class method"  
**Solution**: Add `override` keyword to all interface methods

### Pitfall 3: Pure Virtual Method Not Implemented
**Symptom**: "cannot instantiate abstract class"  
**Solution**: Implement ALL interface methods in backend

### Pitfall 4: Mixing Backend Types
**Symptom**: "no matching constructor"  
**Solution**: Ensure upcasting at creation, not after

---

## Expected Timeline

| Phase | Duration | End Time |
|-------|----------|----------|
| **START** | | 11:00 HKT |
| A.1: FB Inheritance | 1h | 12:00 HKT |
| A.2: Thrift Inheritance | 1h | 13:00 HKT |
| A.3: chunk_range | 0.5h | 13:30 HKT |
| B: Aliases | 0.2h | 13:42 HKT |
| **Lunch Break** | 0.5h | 14:12 HKT |
| C.1: FB Upcasting | 0.75h | 14:57 HKT |
| C.2: Thrift Upcasting | 0.75h | 15:42 HKT |
| C.3: metadata_types | 0.25h | 16:00 HKT |
| D.1: chunk_range return | 0.5h | 16:30 HKT |
| D.2: Misc fixes | 0.25h | 16:45 HKT |
| E: Testing | 1h | 17:45 HKT |
| **COMPLETE** | | **~18:00 HKT** |

**Total**: ~7 hours of focused work

---

## Success Indicators

### After Phase A (Inheritance)
- ✅ FlatBuffers-only: Compiles, runs
- ✅ Error count: Still ~37 (structure added, no behavior change yet)
- ✅ All backend types now inherit from interfaces

### After Phase B (Aliases)
- ✅ FlatBuffers-only: Compiles, runs
- ✅ Error count: ~10 (type system now consistent!)
- ✅ `internal::inode_view_impl` now resolves to interface in multi-format

### After Phase C (Upcasting)
- ✅ FlatBuffers-only: Compiles, runs
- ✅ Error count: ~5 (creation points fixed)
- ✅ Wrappers properly constructed with interface pointers

### After Phase D (Returns)
- ✅ All configs: Compile, run
- ✅ Error count: **0** ✅
- ✅ Virtual function return types match

### After Phase E (Testing)
- ✅ All build configs work
- ✅ All runtime tests pass
- ✅ Cross-format reading works
- ✅ Clean OOP architecture achieved

---

## Reference Files

### Read First (Architecture Understanding)
1. [`doc/METADATA_OOP_REFACTORING_PLAN.md`](METADATA_OOP_REFACTORING_PLAN.md) - Complete plan
2. [`doc/METADATA_OOP_REFACTORING_STATUS.md`](METADATA_OOP_REFACTORING_STATUS.md) - Progress tracker
3. [`include/dwarfs/reader/internal/metadata_view_interface.h`](../include/dwarfs/reader/internal/metadata_view_interface.h) - Interface definitions

### Working Reference (Study for Patterns)
4. [`include/dwarfs/reader/internal/metadata_types_flatbuffers.h`](../include/dwarfs/reader/internal/metadata_types_flatbuffers.h) - FlatBuffers backend
5. [`src/reader/internal/metadata_types_flatbuffers.cpp`](../src/reader/internal/metadata_types_flatbuffers.cpp) - FB implementation

### Will Modify (Apply Inheritance)
6. All backend header and implementation files (list in plan)

---

## Key Principles (Never Forget)

1. **Test FlatBuffers After EVERY Change** - It must never break
2. **Add `override` to ALL Interface Methods** - Compiler catches mistakes
3. **Keep Backend-Specific Methods** - Only interface methods need override
4. **Commit After Each Sub-Phase** - Small, reversible commits
5. **Update Status Tracker** - Track actual time and issues

---

**Ready to Begin?**  

Start with **Phase A.1** - Add inheritance to FlatBuffers backend types.

Follow the detailed instructions in the plan document.

**Good luck!** 🚀

---

**Document Version**: 1.0  
**Created**: 2025-11-28 11:01 HKT  
**Next Review**: After Phase A completion