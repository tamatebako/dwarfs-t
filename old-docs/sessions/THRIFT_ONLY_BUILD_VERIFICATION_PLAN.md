# Thrift-Only Build Verification & Fix Plan

**Created**: 2025-12-02  
**Priority**: CRITICAL  
**Goal**: Ensure Thrift-only builds work correctly with all tests passing

---

## Problem Statement

While CI has been configured to expect Thrift-only builds to pass, the actual codebase may have hard dependencies on FlatBuffers that prevent Thrift-only builds from working.

**Current State**:
- ✅ CI configured to test all 3 builds
- ❌ Thrift-only build success NOT VERIFIED
- ❌ CMake may require FlatBuffers unconditionally
- ❌ Code may have FlatBuffers dependencies even when DWARFS_WITH_FLATBUFFERS=OFF

---

## Phase 1: Verification (Est: 30 min)

### 1.1 Attempt Thrift-Only Build Locally

**Objective**: Determine if Thrift-only build works

**Steps**:
```bash
# Clean build
rm -rf build-tb

# Configure Thrift-only
cmake -B build-tb -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TOOLS=ON \
  -DWITH_TESTS=ON \
  -DWITH_FUSE_DRIVER=ON 2>&1 | tee build-tb-config.log

# Build
cmake --build build-tb 2>&1 | tee build-tb-build.log

# Test
cd build-tb && ./dwarfs_unit_tests 2>&1 | tee ../build-tb-test.log
```

**Expected Outcomes**:
- ✅ **Success**: Build completes, tests pass (1,600/1,613)
- ❌ **Failure**: Configuration fails OR build fails OR tests fail

**Action on Failure**: Proceed to Phase 2

---

## Phase 2: Root Cause Analysis (Est: 30 min)

### 2.1 Analyze CMake Dependencies

**Check Files**:
- `CMakeLists.txt` (main)
- `cmake/metadata_serialization.cmake`
- `cmake/libdwarfs.cmake`
- Any `cmake/*.cmake` with metadata/serialization references

**Look For**:
- Unconditional FlatBuffers requirements
- `REQUIRED` flags on FlatBuffers
- `target_link_libraries` with FlatBuffers without conditionals
- `add_compile_definitions` that always add FLATBUFFERS flags

### 2.2 Analyze Source Code Dependencies

**Check For**:
```bash
# Find direct FlatBuffers includes without guards
rg '#include.*flatbuffers' --type cpp | rg -v '#if.*FLATBUFFERS'

# Find FlatBuffers types used without guards
rg 'flatbuffers::' src/ include/ --type cpp

# Find DWARFS_HAVE_FLATBUFFERS checks
rg 'DWARFS_HAVE_FLATBUFFERS' src/ include/ --type cpp
```

**Common Issues**:
1. Files including FlatBuffers headers without `#ifdef DWARFS_HAVE_FLATBUFFERS`
2. Factory code that assumes FlatBuffers is always available
3. Default values set to FlatBuffers format
4. Registry code that requires FlatBuffers serializers

---

## Phase 3: Fix CMake Configuration (Est: 1 hour)

### 3.1 Make FlatBuffers Truly Optional

**File**: `cmake/metadata_serialization.cmake`

**Required Changes**:
1. Remove `REQUIRED` from `find_package(FlatBuffers)`
2. Make FlatBuffers target creation conditional
3. Ensure `DWARFS_WITH_FLATBUFFERS` can be OFF without errors

**Example Pattern**:
```cmake
if(DWARFS_WITH_FLATBUFFERS)
  find_package(FlatBuffers CONFIG)  # NOT REQUIRED
  if(FlatBuffers_FOUND)
    # Create targets
  else()
    set(DWARFS_WITH_FLATBUFFERS OFF CACHE BOOL "FlatBuffers not found" FORCE)
  endif()
endif()
```

### 3.2 Update Library Dependencies

**File**: `cmake/libdwarfs.cmake`

**Required Changes**:
1. Make FlatBuffers libraries conditional in `target_link_libraries`
2. Use generator expressions: `$<$<BOOL:${DWARFS_WITH_FLATBUFFERS}>:flatbuffers::target>`
3. Ensure compile definitions are conditional

---

## Phase 4: Fix Source Code (Est: 2 hours)

### 4.1 Guard FlatBuffers Includes

**Pattern**:
```cpp
#ifdef DWARFS_HAVE_FLATBUFFERS
#include <flatbuffers/...>
#endif
```

**Files Likely Needing Guards**:
- `src/metadata/serialization/flatbuffers_serializer.cpp`
- `src/reader/internal/metadata_v2_flatbuffers.cpp`
- `src/reader/internal/metadata_types_flatbuffers.cpp`
- Factory files creating serializers
- Registry files listing serializers

### 4.2 Make Registry Conditional

**File**: `src/metadata/serialization/serializer_registry.cpp`

**Example Pattern**:
```cpp
void SerializerRegistry::register_all() {
#ifdef DWARFS_HAVE_THRIFT
  register_serializer(std::make_unique<ThriftSerializer>());
#endif
#ifdef DWARFS_HAVE_FLATBUFFERS
  register_serializer(std::make_unique<FlatBuffersSerializer>());
#endif
}
```

### 4.3 Fix Default Format Selection

**Files**:
- Tool argument parsers (mkdwarfs, dwarfsck, etc.)
- Configuration defaults

**Logic**:
```cpp
std::string get_default_format() {
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
  return "flatbuffers";  // Prefer FlatBuffers if both available
#elif defined(DWARFS_HAVE_FLATBUFFERS)
  return "flatbuffers";
#elif defined(DWARFS_HAVE_THRIFT)
  return "thrift";
#else
  #error "At least one serialization format must be enabled"
#endif
}
```

### 4.4 Update Format Detection

**File**: `src/metadata/serialization/serializer_registry.cpp`

**Ensure**:
- Format detection works with only Thrift
- Error messages are clear about which formats are available
- Auto-detection falls back gracefully

---

## Phase 5: Testing & Validation (Est: 1 hour)

### 5.1 Local Build Matrix

**Build All 3**:
```bash
# FlatBuffers-only
cmake -B build-fb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
cmake --build build-fb
cd build-fb && ./dwarfs_unit_tests

# Thrift-only (CRITICAL TEST)
cmake -B build-tb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
cmake --build build-tb
cd build-tb && ./dwarfs_unit_tests

# Dual-format
cmake -B build-dual -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
cmake --build build-dual
cd build-dual && ./dwarfs_unit_tests
```

**Success Criteria**:
- ✅ All 3 builds succeed
- ✅ FlatBuffers-only: 1,600/1,613 tests pass
- ✅ Thrift-only: 1,600/1,613 tests pass
- ✅ Dual-format: 1,613/1,613 tests pass

### 5.2 Functional Testing

**For Each Build**:
```bash
# Create test image
./mkdwarfs -i /tmp/test-data -o test.dwarfs

# Verify
./dwarfsck test.dwarfs

# Extract
./dwarfsextract -i test.dwarfs -o /tmp/extracted

# Mount (if FUSE available)
./dwarfs test.dwarfs /tmp/mount
```

### 5.3 CI Validation

**Push to GitHub and verify**:
- metadata-formats job passes for Thrift-only
- Compression benchmark works with Thrift-only
- Comprehensive benchmark works with all 3 builds

---

## Phase 6: Documentation Update (Est: 30 min)

### 6.1 Update Build Documentation

**File**: `README.md`

**Add Section**:
```markdown
### Build Options

DwarFS supports three metadata serialization formats:

1. **FlatBuffers-only** (recommended):
   ```bash
   cmake -B build -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
   ```

2. **Thrift-only** (legacy):
   ```bash
   cmake -B build -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
   ```

3. **Dual-format** (maximum compatibility):
   ```bash
   cmake -B build -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
   ```
```

### 6.2 Update Benchmark Documentation

**File**: `doc/BENCHMARK_CI_GUIDE.md`

**Verify sections reflect**:
- All 3 builds are valid
- Test expectations for each build
- Build instructions for each config

---

## Potential Issues & Solutions

### Issue 1: CMake Requires FlatBuffers

**Symptom**: `cmake` fails with "FlatBuffers not found"

**Solution**: Remove `REQUIRED` from FlatBuffers find_package

**Files**: `cmake/metadata_serialization.cmake`, `CMakeLists.txt`

### Issue 2: FlatBuffers Headers Unconditionally Included

**Symptom**: Compile error: `flatbuffers/flatbuffers.h: No such file`

**Solution**: Guard includes with `#ifdef DWARFS_HAVE_FLATBUFFERS`

**Files**: All `*_flatbuffers.cpp`, factory files

### Issue 3: Default Format Hardcoded to FlatBuffers

**Symptom**: Tools fail at runtime: "Unknown format: flatbuffers"

**Solution**: Make default format conditional on available formats

**Files**: Tool CLI parsers, configuration classes

### Issue 4: Tests Assume FlatBuffers Available

**Symptom**: Tests seg fault or fail with "FlatBuffers not available"

**Solution**: Guard FlatBuffers tests with `#ifdef DWARFS_HAVE_FLATBUFFERS`

**Files**: `test/*flatbuffers*`, roundtrip tests

### Issue 5: Serializer Registry Requires FlatBuffers

**Symptom**: Runtime error: "No serializers registered"

**Solution**: Make serializer registration conditional

**Files**: `src/metadata/serialization/serializer_registry.cpp`

---

## Timeline

| Phase | Duration | Status |
|-------|----------|--------|
| 1. Verification | 30 min | Pending |
| 2. Root Cause | 30 min | Pending |
| 3. CMake Fixes | 1 hour | Pending |
| 4. Source Fixes | 2 hours | Pending |
| 5. Testing | 1 hour | Pending |
| 6. Documentation | 30 min | Pending |
| **Total** | **5.5 hours** | **0% Complete** |

---

## Success Criteria

- [ ] Thrift-only build completes without errors
- [ ] Thrift-only tests pass (1,600/1,613)
- [ ] All 3 builds verified locally
- [ ] CI passes for all 3 build configurations
- [ ] Documentation updated
- [ ] No regressions in existing builds

---

## Files to Check/Modify

### CMake Files:
- [ ] `CMakeLists.txt`
- [ ] `cmake/metadata_serialization.cmake`
- [ ] `cmake/libdwarfs.cmake`

### Source Files (Likely):
- [ ] `src/metadata/serialization/*_flatbuffers.cpp`
- [ ] `src/metadata/serialization/serializer_registry.cpp`
- [ ] `src/metadata/serialization/facade_factory.cpp`
- [ ] `src/reader/internal/metadata_v2_flatbuffers.cpp`
- [ ] `src/reader/internal/metadata_types_flatbuffers.cpp`
- [ ] Tool CLIs: `tools/src/mkdwarfs_main.cpp`, etc.

### Test Files:
- [ ] `test/metadata/*flatbuffers*`
- [ ] Any test with FlatBuffers-specific logic

### Documentation:
- [ ] `README.md`
- [ ] `doc/BENCHMARK_CI_GUIDE.md`

---

**Status**: Ready to start Phase 1 - Verification  
**Next Action**: Attempt Thrift-only build locally and analyze results  
**Created**: 2025-12-02 12:15 HKT