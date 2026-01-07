# Session 73 Continuation Plan

**Status**: 🟡 95% Complete - Custom jemalloc working, one final step
**Estimated Time**: 15 minutes
**Prerequisites**: Session 73 Phases 1-4 complete

---

## Current State

### Completed ✅

1. **Custom jemalloc vcpkg Port** - Production-ready
   - Exports unprefixed symbols (`nallocx`, `sdallocx`, `xallocx`)
   - Version: 5.3.0 (resolves `extent_hooks_s` conflict)
   - Build time: ~40-60 seconds
   - Location: `vcpkg_ports/jemalloc/`

2. **Folly vcpkg Integration** - 95% complete
   - jemalloc dependency added
   - CMAKE_REQUIRED_INCLUDES configured
   - Header detection working (`FOLLY_USE_JEMALLOC=1`)
   - Compilation succeeds (all jemalloc function calls resolve)

### Remaining ⏸️

3. **Folly Linkage** - Patch format issue
   - Logic: Correct (`find_library` + `list(APPEND FOLLY_LINK_LIBRARIES)`)
   - Format: vcpkg patch validator rejects
   - Error: "corrupt patch at line 20"

---

## Final Step: Complete Folly Linkage (15 min)

### Option A: Direct Linker Flags (RECOMMENDED - 10 min)

Bypass patching entirely by adding linker flags directly.

**File**: `vcpkg_ports/folly/portfile.cmake`

**Change**:
```cmake
set(JEMALLOC_CMAKE_ARGS)
if(NOT VCPKG_TARGET_IS_WINDOWS)
    list(APPEND JEMALLOC_CMAKE_ARGS
        "-DCMAKE_REQUIRED_INCLUDES=${CURRENT_INSTALLED_DIR}/include"
        "-DCMAKE_EXE_LINKER_FLAGS=-L${CURRENT_INSTALLED_DIR}/lib -ljemalloc"
        "-DCMAKE_SHARED_LINKER_FLAGS=-L${CURRENT_INSTALLED_DIR}/lib -ljemalloc"
    )
endif()
```

**Why this works**:
- Adds `-ljemalloc` to ALL link commands
- vcpkg toolchain already sets library paths
- No patching needed
- Platform-appropriate

**Test**:
```bash
rm -rf build-modern-thrift
cmake -B build-modern-thrift -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON

# Should complete without linker errors
```

### Option B: Fix Patch Format (Alternative - 20 min)

Debug vcpkg's exact patch format requirements.

**Steps**:
1. Extract working patch from vcpkg
2. Analyze format byte-by-byte
3. Match exactly
4. Test application

**Time**: Longer, uncertain outcome

---

## Testing Plan (10 min)

### Step 1: Verify Folly Build

```bash
# Check link succeeded
ls -lh build-modern-thrift/vcpkg_installed/arm64-osx/lib/libfolly.a

# Verify jemalloc linked
nm build-modern-thrift/vcpkg_installed/arm64-osx/lib/libfolly.a | \
  grep -E " U " | grep -E "nallocx|sdallocx|xallocx"
```

**Expected**: No undefined jemalloc symbols

### Step 2: Build DwarFS

```bash
ninja -C build-modern-thrift mkdwarfs dwarfsck
```

**Expected**: Tools build successfully

### Step 3: Run Metadata Tests

```bash
ctest --test-dir build-modern-thrift --tests-regex "metadata" --output-on-failure
```

**Expected**: 66/66 tests pass

### Step 4: Create Modern Thrift Image

```bash
mkdir -p /tmp/test-data
echo "Modern Thrift test" > /tmp/test-data/file.txt

# Create with explicit format
./build-modern-thrift/mkdwarfs \
  -i /tmp/test-data \
  -o /tmp/test-modern.dtc \
  --metadata-format=thrift-compact

# Verify magic bytes (should see 0x82 0x21)
xxd /tmp/test-modern.dtc | head -5
```

**Expected**: Image created with Modern Thrift format

### Step 5: Verify Format Detection

```bash
./build-modern-thrift/dwarfsck /tmp/test-modern.dtc --check-integrity
```

**Expected**: Integrity checks pass, format auto-detected

---

## Success Criteria

- [ ] Folly builds without linker errors
- [ ] DwarFS tools build successfully
- [ ] All 66 metadata tests pass
- [ ] Can create Modern Thrift images
- [ ] Can read Modern Thrift images
- [ ] Magic byte detection works (0x82 0x21)

---

## Rollback Plan

If linker flags approach fails:
1. Remove patch from portfile PATCHES list
2. Use manual CMake configuration
3. Test locally compiled Folly
4. Document as "experimental" for v0.17.1

---

## Files Reference

**Custom jemalloc**:
- `vcpkg_ports/jemalloc/portfile.cmake:16-18`
- `vcpkg_ports/jemalloc/vcpkg.json:4`

**Folly integration**:
- `vcpkg_ports/folly/vcpkg.json:43-46`
- `vcpkg_ports/folly/portfile.cmake:26-34`
- `vcpkg_ports/folly/add-jemalloc-linkage.patch` (unused)

**Format detection**:
- `src/metadata/serialization/serializer_registry.cpp:49-118`

---

**Estimated Completion**: 15 minutes with Option A
**Risk**: Low (direct approach avoids patching complexity)
**Benefit**: Full three-format metadata support in v0.17.0