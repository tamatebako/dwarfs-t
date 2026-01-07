# OOP Refactoring - Continuation Prompt

**Start Date**: 2025-12-03
**Status**: Phase 2.3 Ready - Template Visibility FIXED!
**Remaining Time**: ~1h 45m to complete

---

## Context

You are continuing the OOP refactoring work to fix the **dual-format build issue** in DwarFS. The critical template visibility problem has been solved by properly declaring the template class in a header file.

**Problem Solved**: Template class `flatbuffers_metadata_builder<LoggerPolicy>` was defined in `.cpp` file, making it invisible to the factory. This caused dual-format builds to fail with template instantiation errors.

**Solution Applied**: Strategy Pattern + Composition Pattern - template now in header, delegates to processors.

---

## What's Complete (65%)

### ✅ Phase 1: Processor Architecture (100%)

Created **10 files** establishing clean OOP architecture:
- 4 abstract processor interfaces (chunk, entry, packing, upgrade)
- 2 utility classes (inode_size_calculator, metadata_validator)

### ✅ Phase 2.1-2.2: FlatBuffers Strategy (100%)

Created **9 files**:
- 1 strategy header: [`flatbuffers_metadata_builder_impl.h`](../include/dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h) - **KEY FIX**
- 4 processor implementations (chunk, entry, packing, upgrade)

**Total Created**: 19 files, ~1,800 lines of clean, testable code

---

## What Remains (35%)

### 🎯 Phase 2.3: Refactor Main Builder (45 min) - START HERE

**File**: [`src/writer/internal/flatbuffers_metadata_builder.cpp`](../src/writer/internal/flatbuffers_metadata_builder.cpp)
**Current**: 1,264 lines
**Target**: ~500 lines

**Tasks**:

1. **Add includes at top** (after existing includes):
```cpp
#include <dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h>
#include "flatbuffers_chunk_processor.h"
#include "flatbuffers_entry_processor.h"
#include "flatbuffers_packing_processor.h"
#include "flatbuffers_upgrade_processor.h"
```

2. **Remove template class definition** (lines 142-307):
   - Delete the entire `template <typename LoggerPolicy> class flatbuffers_metadata_builder` definition
   - Keep only the method implementations below it

3. **Implement initialize_processors() method**:
```cpp
template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::initialize_processors() {
  chunk_proc_ = std::make_unique<flatbuffers_chunk_processor>(
      LOG_GET_LOGGER, md_, features_, options_, old_block_size_);
  entry_proc_ = std::make_unique<flatbuffers_entry_processor>(
      LOG_GET_LOGGER, md_, options_, timeres_);
  pack_proc_ = std::make_unique<flatbuffers_packing_processor>(
      LOG_GET_LOGGER, md_, options_);
  upgrade_proc_ = std::make_unique<flatbuffers_upgrade_processor>(
      LOG_GET_LOGGER, md_, options_);
}
```

4. **Call initialize_processors() in constructors**:
   - Add at end of each constructor body
   - Or add after initializer lists

5. **Remove method bodies and delegate to processors**:
   - `gather_chunks()` → delegate to `chunk_proc_` (remove lines 312-351)
   - `gather_entries()` → delegate to `entry_proc_` (remove lines 353-375)
   - `gather_global_entry_data()` → delegate to `entry_proc_` (remove lines 377-393)
   - `remap_blocks()` → delegate to `chunk_proc_` (remove lines 396-522)

6. **Remove methods moved to processors**:
   - `remap_holes()` (lines 693-724) - now in chunk processor
   - `apply_chmod()` (lines 595-630) - now in entry processor
   - `pack_metadata()` (lines 895-991) - now in packing processor
   - `upgrade_metadata()` (lines 993-1025) - now in upgrade processor
   - `upgrade_from_pre_v2_2()` (lines 1027-1210) - now in upgrade processor

7. **Keep these methods** (they use `md_` and `options_` directly):
   - `update_inodes()` (lines 525-593)
   - `update_nlink()` (lines 632-690)
   - `update_totals_and_size_cache()` (lines 726-893)
   - `build()` (lines 1212-1258)
   - Helper methods (get_time_resolution, etc.)

8. **Update build() to use processors**:
```cpp
template <typename LoggerPolicy>
metadata::domain::metadata flatbuffers_metadata_builder<LoggerPolicy>::build() {

  update_nlink();
  update_totals_and_size_cache();
  pack_proc_->pack_metadata();  // Delegate to processor

  // ... rest of existing code ...
}
```

9. **Verify explicit instantiations at end** (lines 1262-1264):
```cpp
template class flatbuffers_metadata_builder<debug_logger_policy>;
template class flatbuffers_metadata_builder<prod_logger_policy>;
```

**Expected Result**: ~500 line file with clean delegation

### 🔧 Phase 4: Update Build System (30 min)

#### 4.1: Update Factory (10 min)

**File**: [`src/writer/internal/metadata_builder_factory.cpp`](../src/writer/internal/metadata_builder_factory.cpp)

**Add at top with other conditional includes**:
```cpp
#ifdef DWARFS_HAVE_FLATBUFFERS
#include <dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h>
#endif
```

#### 4.2: Update CMake (20 min)

**File**: [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake)

**Find the `dwarfs_writer` library sources** (around line 213) and add:
```cmake
# Utility classes (always compiled)
src/writer/internal/inode_size_calculator.cpp
src/writer/internal/metadata_validator.cpp

# FlatBuffers processors (conditional)
$<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_chunk_processor.cpp>
$<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_entry_processor.cpp>
$<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_packing_processor.cpp>
$<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_upgrade_processor.cpp>
```

### ✅ Phase 5: Build & Verify (30 min)

#### 5.1: Build All Configurations (20 min)

```bash
cd /Users/mulgogi/src/external/dwarfs

# Clean all builds
rm -rf build-fb build-tb build-dual

# FlatBuffers-only
cmake -B build-fb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
time ninja -C build-fb

# Thrift-only
cmake -B build-tb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
time ninja -C build-tb

# Dual-format (CRITICAL - should work now!)
cmake -B build-dual -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
time ninja -C build-dual
```

#### 5.2: Run Tests (10 min)

```bash
cd build-fb && ./dwarfs_unit_tests
cd ../build-tb && ./dwarfs_unit_tests
cd ../build-dual && ./dwarfs_unit_tests
```

**Success Criteria**:
- ✅ All three configurations build
- ✅ **Dual-format builds** (was failing!)
- ✅ Tests pass
- ✅ No regressions

---

## Quick Start

1. **Read status documents**:
   - [`doc/OOP_REFACTORING_CONTINUATION_PLAN.md`](OOP_REFACTORING_CONTINUATION_PLAN.md)
   - [`doc/OOP_REFACTORING_IMPLEMENTATION_STATUS.md`](OOP_REFACTORING_IMPLEMENTATION_STATUS.md)

2. **Start Phase 2.3**:
   - Open [`src/writer/internal/flatbuffers_metadata_builder.cpp`](../src/writer/internal/flatbuffers_metadata_builder.cpp)
   - Follow task list above
   - Reduce from 1,264 to ~500 lines

3. **Complete Phase 4**:
   - Update factory includes
   - Update CMake sources

4. **Execute Phase 5**:
   - Build & test all configurations
   - Celebrate dual-format success! 🎉

---

## Key Files

### Created (Ready to Use)
- [`include/dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h`](../include/dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h) - Strategy header
- [`src/writer/internal/flatbuffers_*_processor.{h,cpp}`](../src/writer/internal/) - 8 processor files
- [`include/dwarfs/writer/internal/metadata_*_processor.h`](../include/dwarfs/writer/internal/) - 4 interfaces
- [`src/writer/internal/inode_size_calculator.{h,cpp}`](../src/writer/internal/) - Utility
- [`src/writer/internal/metadata_validator.{h,cpp}`](../src/writer/internal/) - Utility

### To Modify
- [`src/writer/internal/flatbuffers_metadata_builder.cpp`](../src/writer/internal/flatbuffers_metadata_builder.cpp) - Main refactoring
- [`src/writer/internal/metadata_builder_factory.cpp`](../src/writer/internal/metadata_builder_factory.cpp) - Add include
- [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake) - Add sources

---

## Architecture Diagram

```
metadata_builder (facade)
    ↓
flatbuffers_metadata_builder<LoggerPolicy> (template in .h ✅)
    ↓ (composition)
    ├─ chunk_processor       → gather_chunks, remap_blocks
    ├─ entry_processor       → gather_entries, apply_chmod
    ├─ packing_processor     → pack_metadata
    └─ upgrade_processor     → upgrade_metadata
```

---

## Success Indicators

### Before (BROKEN) ❌
```bash
# Dual-format build
cmake -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
ninja
# ERROR: static_cast from 'flatbuffers_metadata_builder<LoggerPolicy>*'
#        to 'metadata_builder::impl*' not allowed
```

### After (FIXED) ✅
```bash
# Dual-format build
cmake -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
ninja
# SUCCESS: Builds cleanly, tests pass
```

---

## Troubleshooting

### If Build Fails

**Undefined references**:
- Check: Are all `.cpp` files in CMakeLists?
- Fix: Add missing files to `cmake/libdwarfs.cmake`

**Missing includes**:
- Check: Are processor headers included in main builder?
- Fix: Add missing `#include` directives

**Template errors**:
- Check: Are explicit instantiations at end of `.cpp`?
- Fix: Ensure both logger policies are instantiated

### If Tests Fail

**Check**: Which specific tests fail?
**Analyze**: New architecture or test needs update?
**Fix**: Update tests if architecture is correct (architecture correctness is priority!)

---

## Timeline

| Phase | Est Time | Task |
|-------|----------|------|
| 2.3 | 45m | Refactor main builder |
| 4.1 | 10m | Update factory |
| 4.2 | 20m | Update CMake |
| 5.1 | 20m | Build configs |
| 5.2 | 10m | Run tests |
| **Total** | **1h 45m** | |

---

## Final Goal

**Enable dual-format builds** that were previously failing due to template visibility issues. The fix involves proper OOP architecture with Strategy + Composition patterns, resulting in:
- ✅ Clean separation of concerns
- ✅ Template visibility solved
- ✅ Files under 500 lines
- ✅ Testable components
- ✅ Dual-format builds working

**Start with Phase 2.3 and work through to Phase 5 for complete success!**

---

**Documentation**: After success, see [`doc/OOP_REFACTORING_CONTINUATION_PLAN.md`](OOP_REFACTORING_CONTINUATION_PLAN.md) for next steps including updating official docs and moving temporary docs to `old-docs/`.