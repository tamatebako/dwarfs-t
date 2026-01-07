# Session 12 Continuation Plan: Folly Allocator Fix Completion

**Created**: 2025-12-17
**Status**: Ready to Execute
**Estimated Completion**: 1-2 hours

---

## Current Status

### ✅ Completed (Session 12)
- **Phase A**: Investigated Folly allocator configuration
- **Phase B**: Fixed `cmake/folly.cmake` to allow jemalloc detection
- **Root Cause Identified**: Original code forced `FOLLY_USE_JEMALLOC OFF` unconditionally

### ⬜ Remaining Work
- **Install jemalloc**: From tamatebako main branch
- **Phase C**: Verify builds with jemalloc
- **Phase D**: Update test automation script
- **Phase E**: Documentation updates

---

## Phase C: Install and Verify (30 min)

### Step 1: Install Tamatebako Jemalloc (10 min)

**From Main Branch** (not released version):

```bash
# Set up vcpkg if needed
if [ ! -d ~/vcpkg ]; then
  git clone https://github.com/microsoft/vcpkg ~/vcpkg
  ~/vcpkg/bootstrap-vcpkg.sh
fi

# Install jemalloc from tamatebako main branch via overlay
~/vcpkg/vcpkg install jemalloc \
  --overlay-ports=$HOME/src/tamatebako/ports
```

**Verify installation**:
```bash
~/vcpkg/vcpkg list | grep jemalloc
```

### Step 2: Test Thrift-Only Build (10 min)

```bash
rm -rf build-thrift-verify
cmake -B build-thrift-verify -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DENABLE_RICEPP=OFF \
  -DWITH_TESTS=ON \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=OFF \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=$HOME/src/tamatebako/ports

ninja -C build-thrift-verify dwarfs_filesystem_tests
./build-thrift-verify/dwarfs_filesystem_tests --gtest_color=yes
```

**Expected**: ✅ Build success, 18/18 tests passing

### Step 3: Test Dual-Format Build (10 min)

```bash
rm -rf build-both-verify
cmake -B build-both-verify -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DENABLE_RICEPP=OFF \
  -DWITH_TESTS=ON \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=OFF \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=$HOME/src/tamatebako/ports

ninja -C build-both-verify dwarfs_filesystem_tests
./build-both-verify/dwarfs_filesystem_tests --gtest_color=yes
```

**Expected**: ✅ Build success, 18/18 tests passing

---

## Phase D: Update Test Automation (15 min)

### File: `scripts/test-all-configs.sh`

**Changes Required**:

1. **Remove platform skip** (lines 24-30):
```bash
# DELETE these lines:
# Skip problematic configs on macOS
if [[ "$PLATFORM" == "Darwin" ]]; then
  echo "⚠️  macOS detected: Skipping Thrift/dual-format (Folly linking issues)"
  echo ""
  CONFIGS=("flatbuffers-only")
  FLATBUFFERS_FLAGS=("ON")
  THRIFT_FLAGS=("OFF")
fi
```

2. **Add vcpkg toolchain support**:
```bash
# After line 16, add:
VCPKG_TOOLCHAIN=""
if [ -f "$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
  VCPKG_TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake"
  if [ -d "$HOME/src/tamatebako/ports" ]; then
    VCPKG_TOOLCHAIN="$VCPKG_TOOLCHAIN -DVCPKG_OVERLAY_PORTS=$HOME/src/tamatebako/ports"
  fi
fi
```

3. **Update CMake calls** (lines 50-59):
```bash
# Add $VCPKG_TOOLCHAIN to cmake command:
if ! cmake -B build-test-$name -GNinja \
  $VCPKG_TOOLCHAIN \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  ...
```

### Verification

```bash
./scripts/test-all-configs.sh
```

**Expected Output**:
```
Platform: Darwin arm64

Testing: flatbuffers-only
✅ PASSED: flatbuffers-only (18 tests)

Testing: thrift-only
✅ PASSED: thrift-only (18 tests)

Testing: both-formats
✅ PASSED: both-formats (18 tests)

✅ ALL TESTED CONFIGURATIONS PASSED
```

---

## Phase E: Documentation Updates (15 min)

### 1. Update `cmake/need_jemalloc.cmake`

**Current** (line 19):
```cmake
# Use tamatebako/jemalloc via vcpkg overlay port (ports/jemalloc/)
# The overlay port fetches from GitHub tag v5.5.0 and builds with CMake
# CRITICAL: This is the ONLY jemalloc with ARM64 support (Windows, Linux, macOS)
```

**Update to**:
```cmake
# Use tamatebako/jemalloc via vcpkg overlay port (ports/jemalloc/)
# The overlay port fetches from main branch and builds with CMake
# CRITICAL: This is the ONLY jemalloc with ARM64 support (Windows, Linux, macOS)
# Install via: vcpkg install jemalloc --overlay-ports=$HOME/src/tamatebako/ports
```

### 2. Create `doc/SESSION_12_COMPLETE_SUMMARY.md`

Document:
- Problem statement (Folly allocator linking on macOS ARM64)
- Root cause (forced FOLLY_USE_JEMALLOC OFF)
- Solution (allow jemalloc detection)
- Files modified
- Jemalloc installation instructions
- Test results

### 3. Update Memory Bank

**File**: `.kilocode/rules/memory-bank/context.md`

Mark Session 12 complete, update current status.

### 4. Move Old Documentation

```bash
# Move temporary investigation docs
mv doc/SESSION_12_FOLLY_ALLOCATOR_FIX_PROMPT.md doc/old-docs/
mv doc/SESSION_12_FOLLY_ALLOCATOR_FIX_PLAN.md doc/old-docs/
mv doc/SESSION_12_FOLLY_ALLOCATOR_FIX_STATUS.md doc/old-docs/

# Keep final summary and continuation plan
```

---

## Success Criteria

- [x] `cmake/folly.cmake` allows jemalloc detection
- [ ] Tamatebako jemalloc (main branch) installed via vcpkg
- [ ] Thrift-only build: ✅ Success, 18/18 tests
- [ ] Dual-format build: ✅ Success, 18/18 tests
- [ ] FlatBuffers-only build: ✅ Success, 18/18 tests (already verified Session 11)
- [ ] Test automation works on macOS without platform skip
- [ ] Documentation updated
- [ ] Old docs moved to old-docs/

---

## Files to Modify

**Modified (Session 12)**:
- [x] `cmake/folly.cmake` - Allow jemalloc detection

**To Modify (Remaining)**:
- [ ] `scripts/test-all-configs.sh` - Remove platform skip, add vcpkg support
- [ ] `cmake/need_jemalloc.cmake` - Update comment about main branch
- [ ] `.kilocode/rules/memory-bank/context.md` - Mark Session 12 complete

**To Create**:
- [ ] `doc/SESSION_12_COMPLETE_SUMMARY.md` - Final documentation

**To Move**:
- [ ] `doc/SESSION_12_FOLLY_ALLOCATOR_FIX_*.md` → `doc/old-docs/`

---

## Timeline

**Total Remaining**: 1-2 hours

- **Phase C**: 30 minutes (install + verify)
- **Phase D**: 15 minutes (test script)
- **Phase E**: 15 minutes (documentation)
- **Buffer**: 30 minutes (contingency)

---

**Status**: Ready to Execute
**Next Action**: Install tamatebako jemalloc from main branch