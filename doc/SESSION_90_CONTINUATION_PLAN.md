# Session 90: Build System Fix - Continuation Plan

**Created**: 2026-01-06
**Status**: Ready to execute
**Goal**: Fix Modern Thrift build system to enable testing
**Estimated**: 1 hour

---

## Problem Summary

Session 89 discovered that Modern Thrift cannot build due to thrift1 compiler generating **absolute path includes** instead of relative ones. This blocks all testing.

**Root Cause**: CMake uses absolute paths when calling thrift1, causing it to generate:
```cpp
#include "/gen-cpp2/metadata_modern_types.tcc"  // ❌ BROKEN
```

**Solution**: Adopt the same **change-directory pattern** used by legacy Thrift builds.

---

## Phase 1: Fix thrift1 Compilation Pattern (30 min)

### Objective
Make Modern Thrift compilation use relative paths like legacy Thrift.

### Implementation

**File**: `cmake/metadata_serialization.cmake` (lines 180-202)

**Current Code** (BROKEN):
```cmake
set(THRIFT_MODERN_IDL ${CMAKE_SOURCE_DIR}/thrift/metadata_modern.thrift)
set(THRIFT_MODERN_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen-cpp2/modern)

add_custom_command(
  OUTPUT ${THRIFT_MODERN_TYPES_H} ${THRIFT_MODERN_TYPES_CPP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${THRIFT_MODERN_GEN_DIR}
  COMMAND ${THRIFT1_COMPILER} --gen mstch_cpp2:no_metadata
          -out ${THRIFT_MODERN_GEN_DIR}     # ❌ ABSOLUTE PATH
          ${THRIFT_MODERN_IDL}              # ❌ ABSOLUTE PATH
  DEPENDS ${THRIFT_MODERN_IDL}
  ...
)
```

**New Code** (FIXED):
```cmake
set(THRIFT_MODERN_IDL ${CMAKE_SOURCE_DIR}/thrift/metadata_modern.thrift)
set(THRIFT_MODERN_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/thrift/modern)
set(THRIFT_MODERN_IDL_COPY ${THRIFT_MODERN_BUILD_DIR}/metadata_modern.thrift)
set(THRIFT_MODERN_GEN_DIR ${THRIFT_MODERN_BUILD_DIR}/gen-cpp2)

# Update output paths
set(THRIFT_MODERN_TYPES_H ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types.h)
set(THRIFT_MODERN_TYPES_CPP ${THRIFT_MODERN_GEN_DIR}/metadata_modern_types.cpp)

add_custom_command(
  OUTPUT ${THRIFT_MODERN_TYPES_H} ${THRIFT_MODERN_TYPES_CPP}
  # Copy .thrift file to build subdirectory
  COMMAND ${CMAKE_COMMAND} -E make_directory ${THRIFT_MODERN_BUILD_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy ${THRIFT_MODERN_IDL} ${THRIFT_MODERN_IDL_COPY}
  # Change directory and run with RELATIVE paths
  COMMAND cd ${THRIFT_MODERN_BUILD_DIR} && 
          ${CMAKE_COMMAND} -E env ASAN_OPTIONS=detect_leaks=0 --
          ${THRIFT1_COMPILER} -o ${THRIFT_MODERN_BUILD_DIR}
          --gen mstch_cpp2:no_metadata
          metadata_modern.thrift
  DEPENDS ${THRIFT_MODERN_IDL}
  WORKING_DIRECTORY ${THRIFT_MODERN_BUILD_DIR}
  COMMENT "Generating Modern Thrift C++ types from ${THRIFT_MODERN_IDL}"
  VERBATIM
)
```

**Verification**:
```bash
# Rebuild and check generated file
ninja -C build-modern dwarfs_metadata_modern_thrift_generate
head -10 build-modern/thrift/modern/gen-cpp2/metadata_modern_types.cpp

# Expected: RELATIVE includes
# #include "metadata_modern_types.tcc"
# NOT: #include "/gen-cpp2/metadata_modern_types.tcc"
```

---

## Phase 2: Fix Include Paths (15 min)

### Objective
Ensure Modern Thrift library can find all required headers.

### Implementation

**File**: `cmake/metadata_serialization.cmake` (lines 220-226)

**Add**:
```cmake
target_include_directories(dwarfs_metadata_modern_thrift
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>           # For config.h
    $<BUILD_INTERFACE:${THRIFT_MODERN_GEN_DIR}>              # For generated types
    $<BUILD_INTERFACE:${THRIFT_MODERN_BUILD_DIR}>            # For thrift output
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
```

**Verification**:
```bash
ninja -C build-modern dwarfs_metadata_modern_thrift 2>&1 | grep "error:"
# Expected: NO errors about missing headers
```

---

## Phase 3: Fix Converter Type Visibility (10 min)

### Objective
Fix incomplete type errors in converters.

### Root Cause
Converters use forward declarations in header but need full types in .cpp.

### Implementation

**Files**:
- `include/dwarfs/metadata/modern/domain_to_thrift.h`
- `include/dwarfs/metadata/modern/thrift_to_domain.h`

**Current** (forward declarations only):
```cpp
namespace thrift::modern {
struct Metadata;
struct Chunk;
// ... etc
}
```

**Fix**: In `.cpp` files, include full generated header:
```cpp
#include "dwarfs/metadata/modern/domain_to_thrift.h"
#include "metadata_modern_types.h"  // ✅ FULL TYPES
```

**Verification**:
```bash
ninja -C build-modern dwarfs_metadata_modern_thrift 2>&1 | grep "incomplete type"
# Expected: NO incomplete type errors
```

---

## Phase 4: Build and Test (5 min)

### Objective
Verify full Modern Thrift library builds successfully.

### Commands

```bash
# Clean previous failed build
rm -rf build-modern/CMakeFiles/dwarfs_metadata_modern_thrift.dir

# Rebuild Modern Thrift library
ninja -C build-modern dwarfs_metadata_modern_thrift

# Expected: BUILD SUCCESS
# [6/6] Linking CXX static library libdwarfs_metadata_modern_thrift.a
```

### Success Criteria

✅ All 6 files compile without errors:
1. `metadata_modern_types.cpp`
2. `metadata_modern_types_compact.cpp`  
3. `metadata_modern_types_binary.cpp`
4. `domain_to_thrift.cpp`
5. `thrift_to_domain.cpp`
6. `thrift_compact_serializer.cpp`

✅ Library created: `build-modern/libdwarfs_metadata_modern_thrift.a`

---

## Phase 5: Resume Session 89 Testing (return to Session 89)

Once build succeeds, return to Session 89 to execute the full testing plan:

1. **Unit Tests** (20 min)
   - Converter tests (6 cases)
   - Serialization tests (10 cases)

2. **Integration Tests** (30 min)
   - Create `.dtc` image with mkdwarfs
   - Verify with dwarfsck
   - Extract with dwarfsextract

3. **Performance Benchmarks** (30 min)
   - Size comparison (vs FlatBuffers, vs Legacy Thrift)
   - Speed benchmarks (serialization, extraction)

4. **Documentation Updates** (20 min)
   - Update memory bank
   - Create completion summary

---

## Risk Mitigation

### Risk 1: Additional Include Path Issues
**Mitigation**: Use verbose build output to diagnose:
```bash
VERBOSE=1 ninja -C build-modern dwarfs_metadata_modern_thrift
```

### Risk 2: Linker Errors
**Mitigation**: Verify all symbols are defined:
```bash
nm build-modern/libdwarfs_metadata_modern_thrift.a | grep -i metadata
```

### Risk 3: Test Failures After Build Fix
**Mitigation**: Tests may need updates for Modern Thrift API differences from legacy. Document and fix incrementally.

---

## Success Metrics

| Metric | Target |
|--------|--------|
| **Build Time** | < 2 min |
| **Build Errors** | 0 |
| **Library Size** | ~500 KB - 1 MB |
| **Include Paths** | 4 (source, binary, generated, thrift) |
| **Time to Complete** | ~1 hour total |

---

## Rollback Plan

If Phase 1-3 fail unexpectedly:

1. Revert `cmake/metadata_serialization.cmake` to Session 88 state
2. Document blocker in new issue
3. Defer Modern Thrift to future release
4. Continue with FlatBuffers + Legacy Thrift only in v0.17.0

---

**Next Document**: `SESSION_90_IMPLEMENTATION_STATUS.md`
**Next Prompt**: `SESSION_90_CONTINUATION_PROMPT.md`