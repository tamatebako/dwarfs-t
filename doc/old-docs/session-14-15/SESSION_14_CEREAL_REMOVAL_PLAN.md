# Session 14: Complete Cereal/Bitsery Removal Plan

**Created**: 2025-12-18
**Status**: Ready to Execute
**Priority**: CRITICAL (Code Cleanup)
**Estimated Time**: 1-2 hours

---

## Objective

**Remove ALL Cereal and Bitsery code** - both are completely dead code since FlatBuffers is required.

## Discovery

### History Serialization Hierarchy

**Current Priority** ([`src/history.cpp`](../src/history.cpp:110-360)):
```cpp
#ifdef DWARFS_HAVE_THRIFT
  // Use Thrift for history
#else
  #ifdef DWARFS_HAVE_FLATBUFFERS
    // Use FlatBuffers for history ← ALREADY IMPLEMENTED!
  #else
    #ifdef DWARFS_HAVE_CEREAL
      // Use Cereal ← DEAD CODE (FlatBuffers is required!)
    #endif
  #endif
#endif
```

**Conclusion**: Cereal history code (lines 197-215, 349-354) **can never execute** because FlatBuffers is required.

### Files with Cereal/Bitsery References

**Source Code** (actual usage):
1. [`src/history.cpp`](../src/history.cpp) - Dead Cereal fallback code
2. [`tools/src/mkdwarfs_main.cpp:417`](../tools/src/mkdwarfs_main.cpp:417) - Error message (keep, already correct)
3. [`tools/src/mkdwarfs/options_parser.cpp:216`](../tools/src/mkdwarfs/options_parser.cpp:216) - Error message (keep, already correct)
4. [`test/metadata/converters/thrift_metadata_converter_test.cpp`](../test/metadata/converters/thrift_metadata_converter_test.cpp) - Test data "bitsery" string

**Build System**:
5. [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake:340,344,488-489) - Cereal/Bitsery linking
6. [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake) - May have references

---

## Phase 1: Remove Cereal from history.cpp (30 min)

### Task 1.1: Remove Cereal includes (5 min)
**File**: [`src/history.cpp`](../src/history.cpp:45-52)

Remove lines 45-52:
```cpp
#ifdef DWARFS_HAVE_CEREAL
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#endif
```

### Task 1.2: Remove Cereal serialization templates (10 min)
**File**: [`src/history.cpp`](../src/history.cpp:63-96)

Remove entire `#ifndef DWARFS_HAVE_THRIFT` block (lines 63-96):
```cpp
#ifndef DWARFS_HAVE_THRIFT
namespace history_internal {
#ifdef DWARFS_HAVE_CEREAL
  // ... cereal template code ...
#endif
}
#endif
```

### Task 1.3: Simplify parse_append() (10 min)
**File**: [`src/history.cpp`](../src/history.cpp:119-217)

**Current structure** (3 nested #ifdefs):
```cpp
#ifdef DWARFS_HAVE_THRIFT
  // Thrift parsing
#else
  #ifdef DWARFS_HAVE_FLATBUFFERS
    // FlatBuffers parsing
  #else
    #ifdef DWARFS_HAVE_CEREAL
      // Cereal parsing ← REMOVE THIS
    #endif
  #endif
#endif
```

**New structure** (2 levels only):
```cpp
#ifdef DWARFS_HAVE_THRIFT
  // Thrift parsing
#else
  // FlatBuffers parsing (required, always available)
  auto fb_history = dwarfs::flatbuffers::history::GetHistory(data.data());
  // ... existing FlatBuffers code ...
#endif
```

### Task 1.4: Simplify serialize() (5 min)
**File**: [`src/history.cpp`](../src/history.cpp:281-361)

**Current structure** (3 nested #ifdefs):
```cpp
#ifdef DWARFS_HAVE_THRIFT
  // Thrift serialization
#else
  #ifdef DWARFS_HAVE_FLATBUFFERS
    // FlatBuffers serialization
  #else
    #ifdef DWARFS_HAVE_CEREAL
      // Cereal serialization ← REMOVE THIS
    #endif
  #endif
#endif
```

**New structure** (2 levels only):
```cpp
#ifdef DWARFS_HAVE_THRIFT
  // Thrift serialization
#else
  // FlatBuffers serialization (required, always available)
  ::flatbuffers::FlatBufferBuilder builder;
  // ... existing FlatBuffers code ...
#endif
```

---

## Phase 2: Remove Bitsery References (15 min)

### Task 2.1: Update test data (5 min)
**File**: [`test/metadata/converters/thrift_metadata_converter_test.cpp`](../test/metadata/converters/thrift_metadata_converter_test.cpp:118,347,354)

Replace `"bitsery"` test data with `"fsst"` or `"flatbuffers"`:
```cpp
// Line 118:
t_meta.features() = std::set<std::string>{"fsst", "flatbuffers"};

// Line 347:
t_meta.features() = std::set<std::string>{"fsst", "flatbuffers", "sparse"};

// Line 354:
EXPECT_TRUE(d_meta.features->count("flatbuffers"));
```

### Task 2.2: Remove error messages (already correct)
**Files**: 
- [`tools/src/mkdwarfs_main.cpp:417`](../tools/src/mkdwarfs_main.cpp:417)
- [`tools/src/mkdwarfs/options_parser.cpp:216`](../tools/src/mkdwarfs/options_parser.cpp:216)

**KEEP** - Error messages correctly inform users Cereal/Bitsery were removed.

---

## Phase 3: Remove CMake Dependencies (30 min)

### Task 3.1: Remove Cereal dependency (15 min)
**File**: [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake:340)

Remove line 340:
```cmake
target_link_libraries(dwarfs_common PRIVATE cereal::cereal)
```

### Task 3.2: Remove Bitsery dependency (15 min)
**File**: [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake:344,488-489)

Remove lines 344 and 488-489:
```cmake
# Line 344:
target_link_libraries(dwarfs_common PRIVATE bitsery)

# Lines 488-489:
if(TARGET bitsery)
  list(APPEND LIBDWARFS_OBJECT_TARGETS bitsery)
```

### Task 3.3: Check metadata_serialization.cmake (5 min)
**File**: [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake)

Search for Cereal/Bitsery references and remove if found.

---

## Phase 4: Build & Test (30 min)

### Task 4.1: Clean build (5 min)
```bash
rm -rf build-test-cereal-removal
cmake -B build-test-cereal-removal -GNinja \
  -DWITH_TESTS=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON
```

### Task 4.2: Build (15 min)
```bash
ninja -C build-test-cereal-removal
```

Expected: Build succeeds without Cereal/Bitsery

### Task 4.3: Run tests (10 min)
```bash
ctest --test-dir build-test-cereal-removal -R "history|metadata"
```

Expected: All tests pass

---

## Phase 5: Documentation (15 min)

### Task 5.1: Update memory bank (10 min)
**Files**:
- `.kilocode/rules/memory-bank/brief.md` - Remove Cereal/Bitsery mentions
- `.kilocode/rules/memory-bank/product.md` - Remove Cereal/Bitsery mentions
- `.kilocode/rules/memory-bank/tech.md` - Document FlatBuffers for history
- `.kilocode/rules/memory-bank/architecture.md` - Update serialization diagram

### Task 5.2: Move outdated docs (5 min)
```bash
mkdir -p doc/old-docs/session-14
mv doc/SESSION_14_FLATBUFFERS_OPTIMIZATION_PLAN.md doc/old-docs/session-14/
mv doc/SESSION_13_OPTIONAL_CLEANUP_PLAN.md doc/old-docs/session-14/
```

---

## Success Criteria

- [ ] Zero Cereal code in src/history.cpp
- [ ] Zero Bitsery code in entire codebase
- [ ] Zero CMake Cereal/Bitsery dependencies
- [ ] All builds successful
- [ ] All tests pass
- [ ] Memory bank updated
- [ ] Outdated docs moved

---

## Files to Modify

### Source Code (3 files)
1. `src/history.cpp` - Remove Cereal dead code
2. `test/metadata/converters/thrift_metadata_converter_test.cpp` - Update test data

### Build System (1 file)
3. `cmake/libdwarfs.cmake` - Remove Cereal/Bitsery linking

### Documentation (5 files)
4. `.kilocode/rules/memory-bank/brief.md` - Remove mentions
5. `.kilocode/rules/memory-bank/product.md` - Remove mentions  
6. `.kilocode/rules/memory-bank/tech.md` - Document FlatBuffers for history
7. `.kilocode/rules/memory-bank/architecture.md` - Update diagrams
8. Move outdated docs to old-docs/

---

## Timeline

- **Phase 1**: 30 min (history.cpp cleanup)
- **Phase 2**: 15 min (Bitsery removal)
- **Phase 3**: 30 min (CMake cleanup)
- **Phase 4**: 30 min (build & test)
- **Phase 5**: 15 min (documentation)

**Total**: 2 hours

---

## Dependencies

- FlatBuffers history schema exists: `flatbuffers/history.fbs` ✅
- FlatBuffers history reader exists: lines 133-195 ✅
- FlatBuffers history writer exists: lines 287-347 ✅

---

## Risks

**Risk 1**: Breaking history reading for old images with Cereal
- **Mitigation**: No such images exist (Cereal metadata was removed before v0.16.0)

**Risk 2**: Build failures due to missing includes
- **Mitigation**: Clean build will catch any issues

**Risk 3**: Test failures
- **Mitigation**: Tests will validate FlatBuffers path works

---

**Status**: Ready to execute
**Next Step**: Phase 1 - Remove Cereal from history.cpp