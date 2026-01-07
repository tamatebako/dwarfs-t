# OOP Refactoring - Continuation Plan

**Date**: 2025-12-03 11:45 HKT  
**Status**: Phase 2.3 - Ready to Continue  
**Deadline**: ASAP - Compress phases to finish quickly

---

## Objective

Fix dual-format build by applying proper OOP architecture with Strategy + Composition patterns to metadata builders. The critical issue is template visibility - templates defined in `.cpp` files cannot be instantiated from other translation units.

---

## Completed Work (Phases 1-2.2)

### ✅ Phase 1: Processor Interfaces & Utilities (100%)

**Created 10 files** establishing clean architecture:

**Abstract Interfaces** (4 files, 340 lines):
- [`include/dwarfs/writer/internal/metadata_chunk_processor.h`](../include/dwarfs/writer/internal/metadata_chunk_processor.h) - Chunk operations
- [`include/dwarfs/writer/internal/metadata_entry_processor.h`](../include/dwarfs/writer/internal/metadata_entry_processor.h) - Entry operations
- [`include/dwarfs/writer/internal/metadata_packing_processor.h`](../include/dwarfs/writer/internal/metadata_packing_processor.h) - Packing operations
- [`include/dwarfs/writer/internal/metadata_upgrade_processor.h`](../include/dwarfs/writer/internal/metadata_upgrade_processor.h) - Upgrade operations

**Utility Classes** (6 files, 319 lines):
- [`include/dwarfs/writer/internal/inode_size_calculator.h`](../include/dwarfs/writer/internal/inode_size_calculator.h) + `.cpp`
- [`include/dwarfs/writer/internal/metadata_validator.h`](../include/dwarfs/writer/internal/metadata_validator.h) + `.cpp`

### ✅ Phase 2.1: Strategy Header (100%)

**Created 1 file** (227 lines):
- [`include/dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h`](../include/dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h)

**KEY ACHIEVEMENT**: Template class properly declared in header - solves visibility!

### ✅ Phase 2.2: FlatBuffers Processors (100%)

**Created 8 files** (1,159 lines):
- Chunk processor: `.h` (120) + `.cpp` (266) = 386 lines
- Entry processor: `.h` (87) + `.cpp` (126) = 213 lines
- Packing processor: `.h` (71) + `.cpp` (132) = 203 lines
- Upgrade processor: `.h` (73) + `.cpp` (284) = 357 lines

**Total Created**: 19 files, ~1,800 lines of focused, testable code

---

## Remaining Work (Compressed Timeline)

### 🔄 Phase 2.3: Refactor Main Builder (45 min)

**File**: [`src/writer/internal/flatbuffers_metadata_builder.cpp`](../src/writer/internal/flatbuffers_metadata_builder.cpp)

**Current**: 1,264 lines with template class definition inside  
**Target**: ~500 lines with clean delegation to processors

**Tasks**:

1. **Replace template class definition** (10 min):
   ```cpp
   // Remove lines 142-307 (template class definition)
   // Replace with:
   #include <dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h>
   #include "flatbuffers_chunk_processor.h"
   #include "flatbuffers_entry_processor.h"
   #include "flatbuffers_packing_processor.h"
   #include "flatbuffers_upgrade_processor.h"
   ```

2. **Implement constructors** (15 min):
   ```cpp
   template <typename LoggerPolicy>
   flatbuffers_metadata_builder<LoggerPolicy>::flatbuffers_metadata_builder(
       logger& lgr, metadata_options const& options)
       : LOG_PROXY_INIT(lgr)
       , options_{options}
       , timeres_{options.time_resolution} {
     initialize_processors();
   }
   
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

3. **Delegate interface methods** (10 min):
   ```cpp
   template <typename LoggerPolicy>
   void flatbuffers_metadata_builder<LoggerPolicy>::gather_chunks(
       inode_manager const& im, block_manager const& bm, size_t chunk_count) {
     chunk_proc_->gather_chunks(im, bm, chunk_count);
   }
   
   template <typename LoggerPolicy>
   void flatbuffers_metadata_builder<LoggerPolicy>::remap_blocks(
       std::span<block_mapping const> mapping, size_t new_block_count) {
     chunk_proc_->remap_blocks(mapping, new_block_count);
   }
   
   // Similarly for gather_entries, gather_global_entry_data
   ```

4. **Keep existing helper methods** (10 min):
   - `update_inodes()`, `update_nlink()`, `update_totals_and_size_cache()`
   - `get_time_resolution()`, `get_subsec_mult()`, `get_chrono_time_resolution()`
   - `build()` method

5. **Verify explicit instantiations** (already at end):
   ```cpp
   template class flatbuffers_metadata_builder<debug_logger_policy>;
   template class flatbuffers_metadata_builder<prod_logger_policy>;
   ```

**Expected Result**: Clean 500-line file with composition pattern

### 🔧 Phase 4: Update Build System (30 min)

**SKIP PHASE 3** for now - focus on getting dual-format to work first!

#### 4.1: Update Factory (10 min)

**File**: [`src/writer/internal/metadata_builder_factory.cpp`](../src/writer/internal/metadata_builder_factory.cpp)

**Change**:
```cpp
// Add at top with other includes:
#ifdef DWARFS_HAVE_FLATBUFFERS
#include <dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h>
#endif

#ifdef DWARFS_HAVE_THRIFT
#include "thrift_metadata_builder.h"  // Stays as-is for now
#endif
```

#### 4.2: Update CMake (20 min)

**File**: [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake)

**Add to `dwarfs_writer` library sources** (around line 213):
```cmake
# Metadata builder (always)
src/writer/internal/metadata_builder.cpp
src/writer/internal/metadata_builder_factory.cpp

# Utility classes (always)
src/writer/internal/inode_size_calculator.cpp
src/writer/internal/metadata_validator.cpp

# FlatBuffers strategy (conditional)
$<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_metadata_builder.cpp>
$<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_chunk_processor.cpp>
$<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_entry_processor.cpp>
$<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_packing_processor.cpp>
$<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:src/writer/internal/flatbuffers_upgrade_processor.cpp>

# Thrift strategy (conditional)
$<$<BOOL:${DWARFS_HAVE_THRIFT}>:src/writer/internal/thrift_metadata_builder.cpp>
```

### ✅ Phase 5: Build & Verify (30 min)

#### 5.1: Build All Configurations (20 min)

```bash
cd /Users/mulgogi/src/external/dwarfs

# Clean all previous builds
rm -rf build-fb build-tb build-dual

# FlatBuffers-only (should work)
cmake -B build-fb -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
time ninja -C build-fb
echo "FB-only: $?"

# Thrift-only (should work)
cmake -B build-tb -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
time ninja -C build-tb
echo "TB-only: $?"

# Dual-format (CRITICAL - was failing, should work now!)
cmake -B build-dual -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
time ninja -C build-dual
echo "Dual: $?"
```

#### 5.2: Run Tests (10 min)

```bash
# Run tests in each configuration
cd build-fb && ./dwarfs_unit_tests --gtest_filter="metadata*"
cd ../build-tb && ./dwarfs_unit_tests --gtest_filter="metadata*"
cd ../build-dual && ./dwarfs_unit_tests --gtest_filter="metadata*"
```

**Success Criteria**:
- ✅ All three configurations build without errors
- ✅ Dual-format build works (was failing before!)
- ✅ Tests pass in all configurations
- ✅ No regressions

---

## Timeline (Compressed)

| Phase | Tasks | Est Time | Dependencies |
|-------|-------|----------|--------------|
| 2.3 | Refactor main builder | 45m | Phases 1-2.2 ✅ |
| 4.1 | Update factory | 10m | Phase 2.3 |
| 4.2 | Update CMake | 20m | Phase 2.3 |
| 5.1 | Build configs | 20m | Phase 4 |
| 5.2 | Run tests | 10m | Phase 5.1 |
| **Total** | | **1h 45m** | |

**Phase 3 DEFERRED** - Can be done later when Thrift-only needs the same refactoring

---

## Success Metrics

### Must Have
- [ ] Dual-format build compiles (was failing)
- [ ] All files <800 lines (target: <500)
- [ ] Tests pass in all configurations
- [ ] No template visibility errors

### Should Have
- [ ] Clear OOP architecture
- [ ] No code duplication
- [ ] Clean separation of concerns
- [ ] Processor tests pass

### Nice to Have
- [ ] Build time improvements
- [ ] Phase 3 (Thrift refactoring)
- [ ] Additional processor tests

---

## Risk Mitigation

### If Build Fails

**Symptom**: Undefined references  
**Check**: Are all `.cpp` files in CMakeLists?  
**Fix**: Add missing files to `cmake/libdwarfs.cmake`

**Symptom**: Template instantiation errors  
**Check**: Are explicit instantiations at end of `.cpp`?  
**Fix**: Add template instantiations

**Symptom**: Missing includes  
**Check**: Are all processor headers included?  
**Fix**: Add missing `#include` directives

### If Tests Fail

**Check**: Which tests are failing?  
**Analyze**: Is it architecture issue or test needs updating?  
**Fix**: Update tests if new architecture requires it (remember: correct architecture is priority!)

---

## Documentation Tasks (After Success)

1. Update [`README.md`](../README.md) - Add OOP architecture section
2. Move [`doc/PHASE_2_REFACTORING_STATUS.md`](../doc/PHASE_2_REFACTORING_STATUS.md) to `old-docs/`
3. Update [`doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`](../doc/METADATA_ARCHITECTURE_STRATEGY_PATTERN.md) with processor pattern
4. Create architecture diagram showing processor composition

---

## Key Files Reference

### Headers Created
- `include/dwarfs/writer/internal/metadata_*_processor.h` (4 files)
- `include/dwarfs/writer/internal/inode_size_calculator.h`
- `include/dwarfs/writer/internal/metadata_validator.h`
- `include/dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h`

### Implementations Created
- `src/writer/internal/flatbuffers_*_processor.{h,cpp}` (8 files)
- `src/writer/internal/inode_size_calculator.cpp`
- `src/writer/internal/metadata_validator.cpp`

### Files to Modify
- `src/writer/internal/flatbuffers_metadata_builder.cpp` - Refactor to use processors
- `src/writer/internal/metadata_builder_factory.cpp` - Include new header
- `cmake/libdwarfs.cmake` - Add new source files

---

**Next Action**: Start Phase 2.3 - Refactor main builder file