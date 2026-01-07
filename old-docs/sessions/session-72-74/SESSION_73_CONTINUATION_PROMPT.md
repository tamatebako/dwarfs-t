# Session 73 Continuation Prompt

**Status**: 🟡 95% Complete - Final 15 minutes
**Prerequisites**: Custom jemalloc working
**Goal**: Complete Modern Thrift format support

---

## Quick Start (15 minutes)

Execute Option A from continuation plan:

### Step 1: Update Folly Portfile (5 min)

Replace patch approach with direct linker flags.

**File**: `vcpkg_ports/folly/portfile.cmake`

**Find** (around line 26):
```cmake
set(JEMALLOC_CMAKE_ARGS)
if(NOT VCPKG_TARGET_IS_WINDOWS)
    list(APPEND JEMALLOC_CMAKE_ARGS
        "-DCMAKE_REQUIRED_INCLUDES=${CURRENT_INSTALLED_DIR}/include"
    )
endif()
```

**Replace with**:
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

**Remove** from PATCHES list (line ~15):
```cmake
add-jemalloc-linkage.patch  # Remove this line
```

### Step 2: Test Build (5 min)

```bash
rm -rf build-modern-thrift
cmake -B build-modern-thrift -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON

ninja -C build-modern-thrift
```

**Expected Output**:
```
[97/115] folly:arm64-osx...installed
...
[115/115] Configuring complete
```

### Step 3: Run Tests (3 min)

```bash
ctest --test-dir build-modern-thrift --tests-regex "metadata" -j
```

**Expected**: 66/66 tests passing

### Step 4: Verify Three Formats (2 min)

```bash
# Create test data
mkdir -p /tmp/test-3formats
echo "test" > /tmp/test-3formats/file.txt

# FlatBuffers
./build-modern-thrift/mkdwarfs -i /tmp/test-3formats -o /tmp/test.dff
xxd /tmp/test.dff | head -2  # Should see "DFBF" at offset 8

# Modern Thrift
./build-modern-thrift/mkdwarfs -i /tmp/test-3formats -o /tmp/test.dtc \
  --metadata-format=thrift-compact
xxd /tmp/test.dtc | head -2  # Should see "82 21" at start

# Legacy Thrift
./build-modern-thrift/mkdwarfs -i /tmp/test-3formats -o /tmp/test.dth \
  --metadata-format=thrift-legacy
xxd /tmp/test.dth | head -2  # No magic bytes

# Verify all read correctly
./build-modern-thrift/dwarfsck /tmp/test.dff --check-integrity
./build-modern-thrift/dwarfsck /tmp/test.dtc --check-integrity
./build-modern-thrift/dwarfsck /tmp/test.dth --check-integrity
```

---

## Success Criteria

- [x] Custom jemalloc exports unprefixed symbols
- [ ] Folly links jemalloc successfully
- [ ] DwarFS builds with Modern Thrift support
- [ ] All 66 metadata tests pass
- [ ] Three formats (FlatBuffers, Modern Thrift, Legacy Thrift) all work
- [ ] Format auto-detection working

---

## If Issues Occur

### Linker still fails

Check if jemalloc is in the link command:
```bash
grep "ljemalloc" /Users/mulgogi/src/external/vcpkg/buildtrees/folly/install-arm64-osx-dbg-out.log
```

If not present, CMAKE flags not applied. Try MAYBE_UNUSED_VARIABLES.

### Tests fail

Check which format:
```bash
ctest --test-dir build-modern-thrift --tests-regex "metadata" --output-on-failure
```

Likely issue: Format detection priority

### Format creation fails

Check `--metadata-format` options:
```bash
./build-modern-thrift/mkdwarfs --help | grep metadata-format
```

Should show: `flatbuffers`, `thrift-compact`, `thrift-legacy`

---

## Context from Session 73

**Problem Solved**: vcpkg's je malloc used `je_` prefix incompatible with Folly

**Solution**: Custom jemalloc port with `--with-jemalloc-prefix=` (empty)

**Verification**:
```bash
$ nm libjemalloc.a | grep nallocx
0000000000005cfc T _nallocx  # ✅ Unprefixed!
```

**Files Modified**:
1. `vcpkg_ports/jemalloc/portfile.cmake` - Custom build
2. `vcpkg_ports/jemalloc/vcpkg.json` - Version bump
3. `vcpkg_ports/folly/vcpkg.json` - jemalloc dependency
4. `vcpkg_ports/folly/portfile.cmake` - Link flags (to be added)

---

**Start With**: Step 1 (Update Folly portfile)
**End Goal**: Modern Thrift format working in v0.17.0
**Time Estimate**: 15 minutes