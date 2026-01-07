# DwarFS Thrift/Folly Optional Refactoring Plan

**Created**: 2025-12-13  
**Status**: Planning Phase  
**Target**: v0.16.0 Release  
**Priority**: HIGH (blocks full vcpkg/Tebako integration)

## Executive Summary

Make Thrift/Folly truly optional by:
1. Converting format-dependent tests to work with both FlatBuffers and Thrift
2. Removing folly/fbthrift as Git submodules
3. Adding vcpkg overlay ports for mhx/folly and facebook/fbthrift
4. Ensuring clean builds with or without Thrift support

## Current State

### What Works ✅
- FlatBuffers-only builds compile (334/334 targets)
- Core functionality works without Thrift
- Tests run without hanging (83s, 2,022 pass)
- jemalloc integrated via vcpkg overlay port

### What's Broken ❌
- 1,110 tests fail in FlatBuffers-only builds
- Many tests are Thrift-specific but should work with FlatBuffers
- folly/fbthrift still as Git submodules (bloat)
- No vcpkg overlay ports for folly/fbthrift

## Phase 1: Test Architecture Refactoring (HIGH PRIORITY)

### Goal
Convert Thrift-specific tests to format-agnostic tests that work with both FlatBuffers and Thrift.

### Test Categories Analysis

**Category A**: Pure Thrift features (skip in FlatBuffers-only)
- `compat_test.cpp` - Uses `rewrite_filesystem` ✅ Already conditional
- `tool_mkdwarfs_main_recompress_test.cpp` - Uses `--recompress` ✅ Already conditional
- `tool_mkdwarfs_main_sparse_test.cpp` - Uses `--change-block-size` ✅ Already conditional

**Category B**: Format-agnostic features (should work with both)
- `filesystem_test.cpp` - Basic filesystem operations
- `metadata_test.cpp` - Metadata reading/writing
- `file_scanner_test.cpp` - File scanning
- `integration_test.cpp` - End-to-end workflows
- `manpage_test.cpp` - Documentation rendering
- Many more...

**Category C**: Format-specific implementation tests
- `metadata/serialization_test.cpp` - Already handles both formats
- `backend_compatibility_test.cpp` - Tests both backends

### Implementation Strategy

#### 1.1 Create Test Parameterization Framework

**File**: `test/format_test_base.h` (NEW)

```cpp
namespace dwarfs::test {

// Test parameter for format selection
struct format_param {
  std::string name;
  bool has_thrift;
  bool has_flatbuffers;
};

// Macro for format-aware tests
#define DWARFS_FORMAT_TEST(test_suite, test_name)                             \
  class test_suite##_##test_name                                              \
      : public ::testing::TestWithParam<format_param> {};                     \
  TEST_P(test_suite##_##test_name, test) {                                    \
    auto const& param = GetParam();                                           \
    // Test implementation using param.has_thrift, param.has_flatbuffers      \
  }                                                                            \
  INSTANTIATE_TEST_SUITE_P(                                                   \
      Formats, test_suite##_##test_name,                                      \
      ::testing::Values(                                                      \
          format_param{"flatbuffers", false, true},                           \
          format_param{"thrift", true, false},                                \
          format_param{"both", true, true}),                                  \
      [](auto const& info) { return info.param.name; })
```

#### 1.2 Convert Category B Tests

**Priority order** (most impactful first):
1. `filesystem_test.cpp` (~300 tests)
2. `tool_mkdwarfs_main_basic_test.cpp` (~200 tests)
3. `tool_mkdwarfs_main_build_test.cpp` (~150 tests)
4. `metadata_test.cpp` (~100 tests)
5. Integration tests (~100 tests)

**Conversion pattern**:
```cpp
// Before (Thrift-only)
TEST(filesystem, read) {
  auto fs = create_filesystem(); // Uses Thrift implicitly
  // test logic
}

// After (Format-aware)
DWARFS_FORMAT_TEST(filesystem, read) {
  auto const& param = GetParam();
  
  // Skip if format not available
  if (param.has_thrift && !DWARFS_HAVE_THRIFT) return;
  if (param.has_flatbuffers && !DWARFS_HAVE_FLATBUFFERS) return;
  
  auto fs = create_filesystem(param); // Format-aware creation
  // test logic (unchanged)
}
```

#### 1.3 CMake Integration

**File**: `cmake/tests.cmake` (MODIFY)

Add compile definitions for test format selection:
```cmake
foreach(test ${DWARFS_TESTS})
  target_compile_definitions(${test} PRIVATE
    $<$<BOOL:${DWARFS_HAVE_THRIFT}>:DWARFS_TEST_HAVE_THRIFT>
    $<$<BOOL:${DWARFS_HAVE_FLATBUFFERS}>:DWARFS_TEST_HAVE_FLATBUFFERS>
  )
endforeach()
```

### Estimated Impact

**Tests converted**: ~800 (from Category B)  
**Tests remaining Thrift-only**: ~300 (Category A)  
**New pass rate** (FlatBuffers-only): ~90% (up from 65%)  
**Time**: 15-20 hours

## Phase 2: Folly/Fbthrift vcpkg Overlay Ports (MEDIUM PRIORITY)

### Goal
Remove Git submodules, add vcpkg overlay ports for mhx/folly and facebook/fbthrift.

### 2.1 Check Current Submodule Commits

```bash
cd folly && git rev-parse HEAD
cd ../fbthrift && git rev-parse HEAD
```

### 2.2 Create Folly Overlay Port

**Directory**: `ports/folly/`

**Files**:
1. `vcpkg.json`:
   ```json
   {
     "name": "folly",
     "version-date": "2025-12-13",
     "description": "Facebook Folly (mhx fork for DwarFS)",
     "homepage": "https://github.com/mhx/folly",
     "license": "Apache-2.0",
     "dependencies": [
       "boost-system", "boost-filesystem", "boost-context",
       "boost-program-options", "boost-regex", "boost-thread",
       "double-conversion", "glog", "gflags", "libevent",
       "openssl", "lz4", "zstd", "snappy", "fmt",
       "vcpkg-cmake", "vcpkg-cmake-config"
     ]
   }
   ```

2. `portfile.cmake`:
   ```cmake
   # Fetch mhx/folly from GitHub at specific commit
   vcpkg_from_github(
       OUT_SOURCE_PATH SOURCE_PATH
       REPO mhx/folly
       REF <commit-hash-from-submodule>
       SHA512 0
       HEAD_REF master
   )
   
   vcpkg_cmake_configure(
       SOURCE_PATH "${SOURCE_PATH}"
       OPTIONS
           -DBUILD_TESTS=OFF
           -DFOLLY_HAVE_PTHREAD=ON
   )
   
   vcpkg_cmake_install()
   vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/folly)
   vcpkg_copy_pdbs()
   
   file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
   vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
   ```

### 2.3 Create Fbthrift Overlay Port

**Directory**: `ports/fbthrift/`

Similar structure, depends on folly overlay port.

### 2.4 Update CMake Configuration

**File**: `cmake/folly.cmake` (NEW, extracted from CMakeLists.txt)

```cmake
# Optional folly dependency via vcpkg or FetchContent
if(DWARFS_WITH_THRIFT)
  find_package(folly CONFIG QUIET)
  
  if(NOT folly_FOUND)
    # Fallback to Git submodule if exists
    if(EXISTS ${CMAKE_SOURCE_DIR}/folly)
      message(STATUS "Using folly from submodule")
      add_subdirectory(folly EXCLUDE_FROM_ALL)
    else()
      message(FATAL_ERROR "Folly required for Thrift but not found")
    endif()
  else()
    message(STATUS "Using folly from vcpkg")
  endif()
endif()
```

**File**: `cmake/thrift.cmake` (NEW, extracted from CMakeLists.txt)

Similar pattern for fbthrift.

### 2.5 Remove Submodules

```bash
git rm -rf folly fbthrift
git commit -m "refactor: remove folly/fbthrift submodules, use vcpkg overlay ports"
```

### 2.6 Update .gitmodules

Remove folly and fbthrift entries.

### Estimated Impact

**Advantages**:
- ✅ Clean repository (no large submodules)
- ✅ Faster clones
- ✅ vcpkg binary caching
- ✅ Easier Tebako integration

**Risks**:
- ⚠️ mhx/folly fork may have custom patches
- ⚠️ Specific commit SHA needed for reproducibility
- ⚠️ vcpkg portfile complexity

**Time**: 10-15 hours

## Phase 3: CMake Architecture Cleanup (LOW PRIORITY)

### Goal
Modularize remaining CMake configuration for clarity.

### 3.1 Extract Compression Configuration

**File**: `cmake/compression.cmake` (NEW)
- Extract lines 251-264 from `CMakeLists.txt`
- PKG module checks for lz4, lzma, brotli, flac

### 3.2 Extract Dependency Configuration

**File**: `cmake/dependencies.cmake` (NEW)
- Extract lines 222-273 from `CMakeLists.txt`
- Boost, libarchive, xxhash, zstd, crypto

### 3.3 Update Main CMakeLists.txt

```cmake
if(WITH_LIBDWARFS)
  include(${CMAKE_SOURCE_DIR}/cmake/dependencies.cmake)
  include(${CMAKE_SOURCE_DIR}/cmake/compression.cmake)
  include(${CMAKE_SOURCE_DIR}/cmake/metadata_serialization.cmake)
  include(${CMAKE_SOURCE_DIR}/cmake/libdwarfs.cmake)
endif()
```

**Time**: 5-8 hours

## Implementation Timeline

### Week 1 (Now - 2025-12-20)
- ✅ Phase 1.1: Test framework (2 hours) - COMPLETE
- ✅ Phase 1.2: Convert 200 tests (8 hours) - 25% done
- ⏳ Phase 1.2: Convert remaining 600 tests (12 hours)

### Week 2 (2025-12-21 - 2025-12-27)
- ⏳ Phase 2.1-2.3: Create overlay ports (10 hours)
- ⏳ Phase 2.4-2.6: Update CMake, remove submodules (5 hours)

### Week 3 (2025-12-28 - 2026-01-03)
- ⏳ Phase 3: CMake cleanup (8 hours)
- ⏳ Documentation updates (5 hours)
- ⏳ CI/CD validation (3 hours)

**Total Estimated Time**: 50-60 hours  
**Target Completion**: 2026-01-03

## Success Criteria

### Phase 1 Success
- [ ] 90%+ test pass rate in FlatBuffers-only builds
- [ ] All format-agnostic tests parameterized
- [ ] Test time <120s (currently 83s)
- [ ] No test hangs

### Phase 2 Success
- [ ] No Git submodules for folly/fbthrift
- [ ] Both formats build via vcpkg overlay ports
- [ ] Binary cache works in CI
- [ ] Tebako builds simplified

### Phase 3 Success
- [ ] CMakeLists.txt <500 lines (currently 355)
- [ ] All modules <300 lines each
- [ ] Clear separation of concerns

## Risk Mitigation

### High Risk: mhx/folly Custom Patches

**Mitigation**:
1. Document all folly patches in `ports/folly/patches/`
2. Include patch files in vcpkg port
3. Test both vcpkg and submodule builds in parallel during transition

### Medium Risk: Test Conversion Regression

**Mitigation**:
1. Convert tests incrementally, validate each batch
2. Keep both-formats CI running continuously
3. Compare test counts before/after

### Low Risk: vcpkg Port Complexity

**Mitigation**:
1. Start with simple ports, iterate
2. Use vcpkg's troubleshooting tools
3. Leverage vcpkg community experience

## Documentation Updates Required

### README.md
- [ ] Update build instructions for vcpkg overlay ports
- [ ] Remove submodule instructions
- [ ] Add troubleshooting for overlay ports

### docs/_guides/
- [ ] `vcpkg-integration.adoc` (NEW) - How to use vcpkg overlay ports
- [ ] `testing-strategy.adoc` (NEW) - Format-aware testing guide

### CHANGES.md
- [ ] v0.16.0 entry: Thrift/Folly optional, vcpkg integration

## File Structure Changes

### New Files Created
```
ports/
├── jemalloc/          (✅ COMPLETE)
│   ├── portfile.cmake
│   ├── vcpkg.json
│   └── usage
├── folly/             (⏳ PLANNED)
│   ├── portfile.cmake
│   ├── vcpkg.json
│   ├── usage
│   └── patches/       (if needed)
└── fbthrift/          (⏳ PLANNED)
    ├── portfile.cmake
    ├── vcpkg.json
    └── usage

test/
├── format_test_base.h (NEW) - Test parameterization framework
└── <existing tests>   (MODIFY) - Add format parameters

cmake/
├── folly.cmake        (MODIFY) - vcpkg-first, submodule fallback
├── thrift.cmake       (MODIFY) - vcpkg-first, submodule fallback
├── compression.cmake  (NEW) - Extract compression config
└── dependencies.cmake (NEW) - Extract dependency config
```

### Files Removed
```
folly/             (Git submodule)
fbthrift/          (Git submodule)
.gitmodules        (folly/fbthrift entries)
```

## Dependencies

### Phase 1 → Phase 2
- Phase 2 can start before Phase 1 completes
- Test conversion and overlay port creation are independent

### Phase 2 → Phase 3
- Phase 3 requires Phase 2 complete (need clean CMake before refactoring)

### All Phases → v0.16.0 Release
- All phases must complete for clean v0.16.0 release

## Detailed Task Breakdown

### Phase 1: Test Conversion

#### Task 1.1: Test Framework (2 hours) ✅
- [x] Create `test/format_test_base.h`
- [x] Define `format_param` structure
- [x] Create `DWARFS_FORMAT_TEST` macro
- [x] Add CMake compile definitions

#### Task 1.2: Convert Filesystem Tests (8 hours)
Source: `test/filesystem_test.cpp` (~300 tests)

- [ ] Parameterize `filesystem.root_access_github204`
- [ ] Parameterize `filesystem.uid_gid_32bit`
- [ ] Parameterize `filesystem.uid_gid_count`
- [ ] Parameterize `filesystem.uid_gid_override`
- [ ] Parameterize `filesystem.find_by_path`
- [ ] Parameterize `filesystem.read`
- [ ] Parameterize `filesystem.inode_size_cache`
- [ ] Parameterize `filesystem.multi_image`
- [ ] Parameterize `filesystem.case_insensitive_lookup`
- [ ] ... (continue for all ~300 tests)

#### Task 1.3: Convert Tool Tests (6 hours)
Sources: `test/tool_mkdwarfs_main_*.cpp`

- [ ] `tool_mkdwarfs_main_basic_test.cpp` - Format-agnostic
- [ ] `tool_mkdwarfs_main_build_test.cpp` - Format-agnostic
- [ ] `tool_mkdwarfs_main_metadata_test.cpp` - Format-agnostic
- [ ] `tool_mkdwarfs_main_rebuild_test.cpp` - Partial (some need Thrift)
- [ ] `tool_mkdwarfs_main_time_resolution_test.cpp` - Format-agnostic
- [ ] `tool_mkdwarfs_integration_test.cpp` - Format-agnostic

#### Task 1.4: Convert Metadata Tests (4 hours)
Source: `test/metadata_test.cpp`

- [ ] Parameterize all metadata read/write tests
- [ ] Keep serialization-specific tests separate
- [ ] Add format-specific expectations where needed

#### Task 1.5: Validate Conversion (2 hours)
- [ ] Run tests with both formats enabled
- [ ] Verify FlatBuffers-only pass rate ≥90%
- [ ] Verify Thrift-only pass rate ≥95%
- [ ] Ensure no test hangs

### Phase 2: vcpkg Overlay Ports

#### Task 2.1: Document Current State (1 hour)
- [ ] Record folly submodule commit: `cd folly && git rev-parse HEAD`
- [ ] Record fbthrift submodule commit: `cd fbthrift && git rev-parse HEAD`
- [ ] Document any custom patches in folly/fbthrift

#### Task 2.2: Create Folly Port (5 hours)
- [ ] Create `ports/folly/vcpkg.json`
- [ ] Create `ports/folly/portfile.cmake`
- [ ] Test local vcpkg build
- [ ] Verify CMake targets created
- [ ] Document any issues

#### Task 2.3: Create Fbthrift Port (4 hours)
- [ ] Create `ports/fbthrift/vcpkg.json`
- [ ] Create `ports/fbthrift/portfile.cmake`
- [ ] Add dependency on folly overlay port
- [ ] Test local vcpkg build
- [ ] Verify compiler builds

#### Task 2.4: Update CMake Modules (3 hours)
- [ ] Modify `cmake/folly.cmake` - vcpkg first, submodule fallback
- [ ] Modify `cmake/thrift.cmake` - vcpkg first, submodule fallback
- [ ] Update `cmake/metadata_serialization.cmake` - handle both sources
- [ ] Test both vcpkg and submodule builds

#### Task 2.5: Remove Submodules (2 hours)
- [ ] Backup submodules: `cp -r folly fbthrift /tmp/backup/`
- [ ] Remove from Git: `git rm -rf folly fbthrift`
- [ ] Update `.gitmodules`
- [ ] Test vcpkg-only build
- [ ] Verify CI works

### Phase 3: CMake Cleanup

#### Task 3.1: Extract Compression Config (2 hours)
- [ ] Create `cmake/compression.cmake`
- [ ] Move pkg_check_modules for lz4, lzma, brotli, flac
- [ ] Update `CMakeLists.txt` to include new module

#### Task 3.2: Extract Dependency Config (3 hours)
- [ ] Create `cmake/dependencies.cmake`
- [ ] Move Boost, libarchive, crypto, xxhash, zstd
- [ ] Update `CMakeLists.txt` to include new module

#### Task 3.3: Validate Modular Build (3 hours)
- [ ] Test all build configurations
- [ ] Verify cmake-gui works correctly
- [ ] Check ccmake organization

## CI/CD Integration

### GitHub Actions Updates

**File**: `.github/workflows/build.yml`

Add vcpkg overlay port support:
```yaml
- name: Update vcpkg
  run: |
    cd $VCPKG_ROOT
    git pull
    ./bootstrap-vcpkg.sh

- name:Configure with vcpkg
  run: |
    cmake -B build -GNinja \
      -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
      -DDWARFS_WITH_THRIFT=${{ matrix.thrift }} \
      -DWITH_TESTS=ON
```

### Test Matrix Expansion

| Config | Thrift | FlatBuffers | Expected Pass | Expected Skip |
|--------|--------|-------------|---------------|---------------|
| fb-only | OFF | ON | 2,800+ | 300+ |
| thrift-only | ON | OFF | - | - (invalid) |
| both | ON | ON | 3,100+ | 0-20 |

## Next Session Workflow

1. **Start Here**: Read `doc/THRIFT_OPTIONAL_REFACTORING_STATUS.md`
2. **Current Phase**: Check implementation status
3. **Pick Next Task**: From task breakdown above
4. **Update Status**: Mark completed tasks
5. **Test**: Validate after each major change
6. **Document**: Update as you go

## Open Questions

1. **Q**: Should we keep submodule support as fallback?  
   **A**: YES, for git builds without vcpkg

2. **Q**: How to handle mhx/folly fork differences from upstream?  
   **A**: Document in ports/folly/patches/, apply in portfile.cmake

3. **Q**: What about fbthrift compiler version requirements?  
   **A**: Pin to specific commit that works with mhx/folly

4. **Q**: Should all tests be format-aware eventually?  
   **A**: NO, keep Thrift-specific tests for recompress/change-block-size

## References

- [`cmake/need_jemalloc.cmake`](../cmake/need_jemalloc.cmake) - vcpkg overlay port pattern
- [`ports/jemalloc/`](../ports/jemalloc/) - Working example
- [`cmake/tests.cmake`](../cmake/tests.cmake) - Conditional test compilation
- [`test/metadata/serialization_test.cpp`](../test/metadata/serialization_test.cpp) - Format-aware test example

---

**Last Updated**: 2025-12-13  
**Next Update**: Start of Phase 1 implementation