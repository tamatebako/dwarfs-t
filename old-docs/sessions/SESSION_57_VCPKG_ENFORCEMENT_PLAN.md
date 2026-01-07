# Session 57: Enforce vcpkg-Only Builds for Folly/Thrift

**Status**: 🟡 **READY TO START**
**Created**: 2025-12-31
**Previous Session**: Session 56 (Thrift converter bug FIXED)

---

## Background

**Session 56 Achievement**: Fixed Thrift converter bug in `cpp_thrift_converter.cpp:521-535`
- **Bug**: Asymmetric optional field handling in `string_table.index`
- **Fix**: Added `if (!st.index.empty())` guard to preserve optional semantics
- **Impact**: Restores backward compatibility with Homebrew dwarfs v0.14.1

**BLOCKER**: Cannot verify fix locally because build system still uses Folly/Thrift submodules instead of Tebako vcpkg overlay ports.

---

## Problem Statement

### Current State (BROKEN)

1. ✅ **vcpkg.json**: Added `folly` and `fbthrift` dependencies
2. ✅ **Submodules**: Deleted `folly/` and `fbthrift/` directories (git rm)
3. ❌ **CMake files**: Still reference deleted submodule paths
   - `cmake/folly.cmake:116`: `add_subdirectory(folly ...)`
   - `cmake/folly.cmake:169-203`: `${CMAKE_CURRENT_SOURCE_DIR}/folly/...` (lite sources)
   - `cmake/thrift.cmake:37`: `add_subdirectory(fbthrift ...)`
   - `cmake/thrift.cmake:48-67`: `${CMAKE_CURRENT_SOURCE_DIR}/fbthrift/...` (lite sources)

### Required Architecture

**vcpkg overlay ports** (`vcpkg_ports/`):
- `folly/` - Tebako fork with jemalloc support
- `fbthrift/` - Tebako fork
- `jemalloc/` - Tebako fork (5.5.0)

**Build flow**:
```
vcpkg install (using overlay ports)
  ↓
folly/fbthrift built by vcpkg with jemalloc
  ↓
DwarFS links against vcpkg-provided libraries
```

**NO submodules, NO Homebrew Folly, NO alternative build paths allowed.**

---

## Implementation Plan

### Phase 1: Remove Submodule References (1-2 hours)

**File**: `cmake/folly.cmake`

**Current (BROKEN)**:
```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/folly EXCLUDE_FROM_ALL SYSTEM)

add_library(dwarfs_folly_lite OBJECT
  ${CMAKE_CURRENT_SOURCE_DIR}/folly/folly/Conv.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/folly/folly/Demangle.cpp
  # ... 30+ files from submodule
)
```

**Required Changes**:
1. **Remove** `add_subdirectory(folly)` (line 116)
2. **Replace** with `find_package(folly CONFIG REQUIRED)` (vcpkg-provided)
3. **Remove** `dwarfs_folly_lite` target (lines 169-274)
   - vcpkg folly is already built
   - We don't need a "lite" version
4. **Add** vcpkg detection guard:
   ```cmake
   if(NOT VCPKG_TOOLCHAIN)
     message(FATAL_ERROR "Folly MUST be provided by vcpkg. Set CMAKE_TOOLCHAIN_FILE to vcpkg.cmake")
   endif()
   ```

**File**: `cmake/thrift.cmake`

**Current (BROKEN)**:
```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/fbthrift EXCLUDE_FROM_ALL SYSTEM)

add_library(dwarfs_thrift_lite OBJECT
  ${CMAKE_CURRENT_SOURCE_DIR}/fbthrift/thrift/lib/cpp/...
  # ... 20+ files from submodule
)
```

**Required Changes**:
1. **Remove** `add_subdirectory(fbthrift)` (line 37)
2. **Replace** with `find_package(FBThrift CONFIG REQUIRED)` (vcpkg-provided)
3. **Remove** `dwarfs_thrift_lite` target (lines 47-76)
   - vcpkg fbthrift is already built
4. **Add** vcpkg detection guard:
   ```cmake
   if(NOT VCPKG_TOOLCHAIN)
     message(FATAL_ERROR "FBThrift MUST be provided by vcpkg. Set CMAKE_TOOLCHAIN_FILE to vcpkg.cmake")
   endif()
   ```

### Phase 2: Update Target Links (0.5-1 hour)

**Files**: `cmake/libdwarfs.cmake`, `cmake/tool_support.cmake`, etc.

**Current** (uses lite targets):
```cmake
target_link_libraries(dwarfs_common PRIVATE dwarfs_folly_lite)
target_link_libraries(dwarfs_writer PRIVATE dwarfs_thrift_lite)
```

**Required** (use vcpkg targets):
```cmake
target_link_libraries(dwarfs_common PRIVATE Folly::folly)
target_link_libraries(dwarfs_writer PRIVATE FBThrift::thrift)
```

### Phase 3: vcpkg Port Configuration (0.5 hour)

**Ensure overlay ports are correct**:

`vcpkg_ports/folly/portfile.cmake`:
- Should build Folly from Tebako fork
- Must enable jemalloc support
- Must disable TCMalloc

`vcpkg_ports/fbthrift/portfile.cmake`:
- Should build fbthrift from Tebako fork
- Must depend on `folly` from overlay

`vcpkg_ports/jemalloc/portfile.cmake`:
- Should build jemalloc 5.5.0 from Tebako fork

### Phase 4: Test Build (0.5 hour)

```bash
# Clean rebuild with vcpkg
rm -rf build-converter-test
cmake -B build-converter-test -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_TESTS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_OVERLAY_PORTS="$(pwd)/vcpkg_ports"

ninja -C build-converter-test dwarfs_unit_tests
```

**Expected**: Clean build using vcpkg folly/fbthrift/jemalloc

### Phase 5: Verify Converter Fix (0.5 hour)

```bash
# Run round-trip tests
./build-converter-test/dwarfs_unit_tests --gtest_filter="*ConverterRoundTrip*"

# Test Homebrew compatibility
./build-converter-test/mkdwarfs -i /tmp/test -o /tmp/our.dwarfs --format=thrift -l1
/opt/homebrew/bin/dwarfsck -i /tmp/our.dwarfs  # Should succeed
```

**Expected**: ALL tests PASS, Homebrew can read our files

---

## Success Criteria

- [x] Submodules deleted (folly/, fbthrift/)
- [x] vcpkg.json includes folly/fbthrift
- [ ] cmake/folly.cmake uses vcpkg `find_package()`
- [ ] cmake/thrift.cmake uses vcpkg `find_package()`
- [ ] All targets link against vcpkg-provided libraries
- [ ] Build succeeds with vcpkg toolchain
- [ ] Build FAILS without vcpkg toolchain (enforced)
- [ ] Converter round-trip tests PASS
- [ ] Homebrew v0.14.1 can read our Thrift files

---

## Files to Modify

1. **cmake/folly.cmake** - Remove submodule, use vcpkg
2. **cmake/thrift.cmake** - Remove submodule, use vcpkg
3. **cmake/libdwarfs.cmake** - Update target links
4. **cmake/tool_support.cmake** - Update target links
5. **CMakeLists.txt** - Ensure vcpkg enforcement

---

## Estimated Time

- Phase 1: 1-2 hours
- Phase 2: 0.5-1 hour
- Phase 3: 0.5 hour
- Phase 4: 0.5 hour
- Phase 5: 0.5 hour

**Total**: 3-4.5 hours

---

## Dependencies from Session 56

**CRITICAL FIX APPLIED** (ready to verify):
- `src/metadata/converters/cpp_thrift_converter.cpp:521-535`
- Added `if (!st.index.empty())` guard
- Preserves Thrift optional semantics

**Tests Created** (ready to run):
- `test/metadata/converter_roundtrip_test.cpp` (7 tests, 157 lines)

**Documentation**:
- `doc/SESSION_56_FIX_SUMMARY.md` - Complete analysis

---

**Last Updated**: 2025-12-31 15:23 HKT
**Status**: Ready for implementation
**Next Session**: Execute this plan, verify converter fix