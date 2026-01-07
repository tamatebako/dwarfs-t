# Session 31H Implementation Prompt: Architectural Purity

**Date**: 2025-12-23
**Objective**: Fix architectural violations and achieve clean build
**Duration**: 1-2 hours
**Prerequisites**: Read [`SESSION_31G_ARCHITECTURAL_CONTINUATION_PLAN.md`](SESSION_31G_ARCHITECTURAL_CONTINUATION_PLAN.md)

## Quick Start

```bash
# FIRST: Read architectural plan
cat doc/SESSION_31G_ARCHITECTURAL_CONTINUATION_PLAN.md

# THEN: Start with architectural fixes
```

## Critical Architectural Violations to Fix

### Violation 1: Type Casting in directory_view Construction

**Location**: [`src/reader/internal/common_metadata_operations.cpp:695`](../src/reader/internal/common_metadata_operations.cpp:695)

**Current (WRONG)**:
```cpp
return directory_view{iv.inode_num(), *reinterpret_cast<internal::global_metadata const*>(&global)};
```

**Problem**:
- `reinterpret_cast` violates type safety
- Assumes `domain_global_metadata` is binary-compatible with `global_metadata_interface`
- **NOT CLEAN ARCHITECTURE**

**Solution (CLEAN)**:

**Option A**: Accept domain type directly (RECOMMENDED)
```cpp
// In metadata_types.h - Add constructor overload
class directory_view {
private:
  directory_view(uint32_t inode, domain_global_metadata const& g)
      : inode_{inode}, g_{nullptr}, dg_{&g} {}

  uint32_t inode_{0};
  internal::global_metadata const* g_{nullptr};
  domain_global_metadata const* dg_{nullptr};  // For FlatBuffers-only builds

  friend class internal::common_metadata_operations;
};

// In common_metadata_operations.cpp
return directory_view{iv.inode_num(), global};  // CLEAN - no cast
```

**Option B**: Proper adapter (if polymorphism needed)
```cpp
// Create adapter that implements interface
class domain_global_metadata_adapter : public global_metadata_interface {
  domain_global_metadata const& underlying_;
public:
  explicit domain_global_metadata_adapter(domain_global_metadata const& g)
    : underlying_(g) {}
  // Implement all interface methods by delegating to underlying_
};
```

### Violation 2: Iterator Type Mismatch

**Location**: [`src/reader/internal/common_metadata_operations.cpp:395`](../src/reader/internal/common_metadata_operations.cpp:395)

**Current (WRONG)**:
```cpp
first_chunk_block[std::distance(entries.data(), it)] = ...
```

**Problem**:
- `entries.data()` returns `pair<uint32_t, uint32_t>*` (raw pointer)
- `it` is `vector<pair<...>>::iterator`
- **Type mismatch** - `std::distance` requires same type

**Solution (CLEAN)**:
```cpp
first_chunk_block[std::distance(entries.begin(), it)] = ...
```

**Why**: Both `entries.begin()` and `it` are same iterator type

### Violation 3: Missing Include Dependencies

**Location**: [`src/reader/internal/common_metadata_operations.cpp`](../src/reader/internal/common_metadata_operations.cpp)

**Problem**: Trying to include non-existent `dwarfs/filesystem_info.h`

**Solution**: Use correct include
```cpp
#include <dwarfs/fstypes.h>  // Contains filesystem_info struct
```

## Implementation Steps (STRICT ORDER)

### Step 1: Fix Architectural Violations (30 min)

**Task 1.1**: Fix `directory_view` construction
- Choose Option A (direct domain type) for clean architecture
- Modify `directory_view` class to accept `domain_global_metadata`
- Update all call sites in `common_metadata_operations.cpp`

**Task 1.2**: Fix iterator type mismatch
- Change `entries.data()` to `entries.begin()`
- Verify type consistency

**Task 1.3**: Verify includes
- Ensure `#include <dwarfs/fstypes.h>` is present
- Remove any incorrect includes

### Step 2: Clean FlatBuffers-Only Build (30 min)

```bash
# Remove existing build
rm -rf build-fb-clean

# Configure CLEAN build (FlatBuffers ONLY)
cmake -B build-fb-clean -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON

# Build and capture output
ninja -C build-fb-clean 2>&1 | tee build-clean.log

# Check for errors
grep -c "error:" build-clean.log
# EXPECTED: 0
```

**Success Criteria**: Build completes with 0 errors

### Step 3: Run Unit Tests (15 min)

```bash
# Run all tests
ctest --test-dir build-fb-clean --output-on-failure

# Check metadata tests specifically
ctest --test-dir build-fb-clean -R metadata --output-on-failure
```

**Success Criteria**: All tests pass

### Step 4: Integration Testing (30 min)

```bash
# Test 1: Create image
./build-fb-clean/mkdwarfs -i /usr/bin -o test-clean.dff \
  --compression=zstd:level=3

# Test 2: Check integrity
./build-fb-clean/dwarfsck test-clean.dff

# Test 3: Extract
mkdir -p extracted-clean
./build-fb-clean/dwarfsextract -i test-clean.dff -o extracted-clean/

# Test 4: Verify byte-for-byte
diff -r /usr/bin extracted-clean/
# EXPECTED: No differences (or only timestamps if not preserved)

# Test 5: Verify file count
find /usr/bin -type f | wc -l
find extracted-clean -type f | wc -l
# EXPECTED: Same count
```

**Success Criteria**: All files extracted correctly

### Step 5: Delete Legacy Code (15 min)

**CRITICAL**: Only proceed if Steps 1-4 ALL pass

```bash
# Delete old backend implementations
git rm src/reader/internal/metadata_v2_flatbuffers.cpp
git rm src/reader/internal/metadata_v2_thrift.cpp
git rm src/reader/internal/metadata_types_flatbuffers.cpp
git rm src/reader/internal/metadata_types_thrift.cpp

# Verify deletion
git status | grep deleted
# EXPECTED: 4 files deleted
```

### Step 6: Update CMake (10 min)

Edit [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake):

**Remove these lines**:
```cmake
src/reader/internal/metadata_v2_flatbuffers.cpp
src/reader/internal/metadata_v2_thrift.cpp
src/reader/internal/metadata_types_flatbuffers.cpp
src/reader/internal/metadata_types_thrift.cpp
```

**Verify**:
```bash
# Rebuild to verify CMake changes
ninja -C build-fb-clean
```

### Step 7: Git Commit (10 min)

```bash
git add -A
git commit -m "feat(metadata): Complete domain-based metadata migration

ARCHITECTURAL ACHIEVEMENT:
- Single unified domain-based implementation
- Eliminated 7,288 lines of duplicate backend code
- 85.6% code reduction while maintaining full functionality

CLEAN ARCHITECTURE:
- Pure domain model (format-agnostic)
- Clean separation of concerns
- No type casting violations
- Proper abstraction layers

IMPLEMENTATION:
- Added: domain_metadata_views.{h,cpp} (350 lines)
- Added: common_metadata_operations.cpp (1,325 lines)
- Deleted: 4 backend implementation files (7,288 lines)
- Net reduction: -5,613 lines (-79.4%)

TESTING:
- FlatBuffers-only build: ✅ PASS
- All unit tests: ✅ PASS
- Integration tests: ✅ PASS
- Byte-for-byte extraction: ✅ VERIFIED

ARCHITECTURAL PRINCIPLES:
- Single Responsibility: Each class has one job
- Open/Closed: Domain closed, adapters open
- Dependency Inversion: High-level depends on abstractions
- Separation of Concerns: Domain/Operations/Adapters separated
- Interface Segregation: Focused, minimal interfaces

This migration establishes a clean, maintainable architecture
for metadata operations across all supported serialization formats.

Ref: Session 31 Domain Migration
"
```

## Architectural Quality Checklist

Before committing, verify:

- [ ] No `reinterpret_cast` or `static_cast` of domain types
- [ ] No format-specific logic in domain layer
- [ ] All includes are correct and necessary
- [ ] Iterator types are consistent
- [ ] Build produces 0 errors, 0 warnings (except deprecations)
- [ ] All tests pass
- [ ] Integration tests verify correctness
- [ ] Code follows SOLID principles
- [ ] Separation of concerns maintained
- [ ] Domain model is format-agnostic

## Common Pitfalls to Avoid

1. **DON'T** skip architectural fixes - they're not optional
2. **DON'T** use type casts to make code compile
3. **DON'T** proceed to Step 5 before validating Steps 1-4
4. **DON'T** commit without running tests
5. **DO** fix root causes, not symptoms
6. **DO** maintain clean abstractions
7. **DO** verify byte-for-byte correctness

## Expected Outcomes

### Architecture
- ✅ Clean domain layer (no format knowledge)
- ✅ Proper abstraction boundaries
- ✅ Type-safe operations
- ✅ Single implementation path

### Code Quality
- ✅ 85.6% code reduction
- ✅ 0 compilation errors
- ✅ 0 type violations
- ✅ SOLID principles applied

### Functionality
- ✅ All tests pass
- ✅ Byte-for-byte correctness
- ✅ Full feature parity
- ✅ Performance maintained

---

**Last Updated**: 2025-12-23
**Status**: Ready for implementation
**Priority**: Fix architectural violations FIRST