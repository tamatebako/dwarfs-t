# Session 31G: Architectural Purity - Domain Migration Completion

**Date**: 2025-12-23
**Objective**: Complete domain-based metadata architecture with exemplar purity
**Status**: Phase 1 Complete, Moving to Clean Architecture Phase

## Architectural Vision

### Current Problem: Dual-Format Complexity Violation

The build is failing because we're attempting to compile **BOTH** old backend implementations AND new domain implementation simultaneously. This violates:

1. **Single Responsibility**: System should use ONE implementation approach
2. **Separation of Concerns**: Domain and backend implementations are intermingled
3. **Clean Architecture**: We're not respecting layer boundaries

### Solution: Clean Architectural Migration

```
STEP 1: Complete Domain Layer (PURE)
    ↓
STEP 2: Remove Old Backend Implementations (CLEAN)
    ↓
STEP 3: Wire Domain Layer via Adapters (SEPARATION)
    ↓
STEP 4: Validate via Tests (CORRECTNESS)
```

## Architectural Layers (CLEAN SEPARATION)

```
┌─────────────────────────────────────────────────────┐
│           Application Layer (Tools)                 │
│    mkdwarfs │ dwarfs │ dwarfsck │ dwarfsextract     │
└───────────────────────┬─────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────┐
│          Metadata Operations Layer                  │
│     common_metadata_operations (PURE LOGIC)         │
│     - No format knowledge                           │
│     - Domain model only                             │
│     - Single responsibility                         │
└───────────────────────┬─────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────┐
│            Domain Model Layer                       │
│      metadata::domain::metadata (PURE DATA)         │
│      - Format-agnostic structures                   │
│      - Complete, consistent model                   │
│      - No serialization logic                       │
└───────────────────────┬─────────────────────────────┘
                        │
           ┌────────────┴────────────┐
           ▼                         ▼
┌──────────────────┐      ┌──────────────────┐
│  Adapter Layer   │      │  Adapter Layer   │
│  FlatBuffers     │      │  Thrift          │
│  CLEAN BOUNDARY  │      │  CLEAN BOUNDARY  │
└──────────────────┘      └──────────────────┘
```

## Immediate Actions Required

### PRE-WORK: Fix Architectural Violations

**Issue 1**: `common_metadata_operations.cpp` has type casting violations

**Current** (Line 695):
```cpp
return directory_view{iv.inode_num(), *reinterpret_cast<internal::global_metadata const*>(&global)};
```

**Problem**: `reinterpret_cast` is architectural violation - type system should guarantee compatibility

**Solution**: Use proper abstraction
```cpp
// Option A: Make directory_view accept domain_global_metadata directly
directory_view(uint32_t inode, domain_global_metadata const& g);

// Option B: Create proper adapter
class global_metadata_adapter : public global_metadata_interface {
    domain_global_metadata const& underlying_;
public:
    explicit global_metadata_adapter(domain_global_metadata const& g) : underlying_(g) {}
    // Implement interface methods
};
```

**Issue 2**: Lambda capture issues indicate iterator type mismatch

**Root Cause**: Mixing `entries.data()` (raw pointer) with `it` (iterator)

**Solution**: Use consistent iterator types throughout

### Phase 2: Clean Build (FlatBuffers-Only)

**Step 1**: Fix remaining type issues in `common_metadata_operations.cpp`
- Fix `directory_view` constructor (no casts)
- Fix `std::distance` call (consistent iterators)
- Ensure all domain types flow cleanly

**Step 2**: Create fresh FlatBuffers-only build
```bash
rm -rf build-fb-clean
cmake -B build-fb-clean -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON

ninja -C build-fb-clean 2>&1 | tee build-clean.log
```

**Expected Result**: 0 errors (clean compilation)

### Phase 3: Delete Legacy Backend Code

**ONLY AFTER Phase 2 succeeds**, remove old implementations:

```bash
git rm src/reader/internal/metadata_v2_flatbuffers.cpp    # 2,516 lines
git rm src/reader/internal/metadata_v2_thrift.cpp         # 2,470 lines
git rm src/reader/internal/metadata_types_flatbuffers.cpp # 1,151 lines
git rm src/reader/internal/metadata_types_thrift.cpp      # 1,151 lines
```

**Total Deletion**: 7,288 lines

**Result**: Single, clean domain-based implementation

### Phase 4: Update CMake for Clean Architecture

Edit `cmake/libdwarfs.cmake`:

**Remove** (old backend files):
```cmake
src/reader/internal/metadata_v2_flatbuffers.cpp
src/reader/internal/metadata_v2_thrift.cpp
src/reader/internal/metadata_types_flatbuffers.cpp
src/reader/internal/metadata_types_thrift.cpp
```

**Keep** (clean domain implementation):
```cmake
src/reader/internal/common_metadata_operations.cpp
src/reader/internal/domain_metadata_views.cpp
include/dwarfs/reader/internal/domain_metadata_views.h
```

### Phase 5: Validate Architecture

**Test 1**: Unit tests
```bash
ctest --test-dir build-fb-clean --output-on-failure
```

**Test 2**: Integration tests
```bash
# Create
./build-fb-clean/mkdwarfs -i /usr/bin -o test.dff

# Check
./build-fb-clean/dwarfsck test.dff

# Mount (if FUSE available)
./build-fb-clean/dwarfs test.dff /tmp/mnt
ls -la /tmp/mnt/ls
umount /tmp/mnt

# Extract
./build-fb-clean/dwarfsextract -i test.dff -o extracted/
diff -r /usr/bin extracted/
```

**Expected**: All tests pass, byte-for-byte correctness

## Architectural Principles Applied

### 1. Single Responsibility
- `common_metadata_operations`: Domain logic ONLY
- `domain_metadata_views`: View abstractions ONLY
- Adapters: Format conversion ONLY

### 2. Open/Closed Principle
- Domain model: Closed for modification (stable)
- Adapters: Open for extension (new formats via new adapters)

### 3. Dependency Inversion
- High-level (operations) depends on abstractions (domain)
- Low-level (adapters) implements abstractions
- No high→low dependencies

### 4. Separation of Concerns
- Domain: Data structures
- Operations: Business logic
- Adapters: Serialization/deserialization
- Tools: User interface

### 5. Interface Segregation
- Each view type has focused, minimal interface
- No fat interfaces
- Clients depend only on methods they use

## Success Criteria

✅ **Clean Compilation**: 0 errors, 0 architectural violations
✅ **Pure Implementation**: No casts, no format knowledge in domain layer
✅ **Test Passage**: All unit + integration tests pass
✅ **Code Reduction**: 85.6% reduction (7,288 → 1,675 lines)
✅ **Maintainability**: Single implementation to maintain

## Next Session Start

Read `doc/SESSION_31H_IMPLEMENTATION_PROMPT.md` for detailed implementation steps.

---

**Last Updated**: 2025-12-23
**Status**: Architectural plan complete, ready for clean implementation
**Priority**: Fix type violations FIRST, then proceed with clean build