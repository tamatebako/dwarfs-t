# Session 76: Three-Format Validation & v0.17.0 Release

**Prerequisites**: Session 75 complete (all documentation updated)
**Goal**: Validate three-format system and prepare v0.17.0 release
**Deadline**: Critical - compressed timeline required

---

## Overview

Session 75 completed all documentation for the three-format metadata system. Session 76 will perform comprehensive validation to ensure all three formats work correctly, with special emphasis on backward compatibility with Homebrew dwarfs v0.14.1.

### Current State

✅ **Complete**:
- All three metadata serializers implemented and compiled
- FlatBuffers format (modern default)
- Modern Thrift Compact format (optional)
- Legacy Thrift format (always available)
- All official documentation updated

❌ **Remaining**:
- Clean build validation
- Cross-format compatibility testing
- **CRITICAL**: Homebrew v0.14.1 compatibility verification
- v0.17.0 release preparation

---

## Phase 1: Clean Build Environment (30 min)

### Objective
Ensure completely clean build environment for reliable testing.

### Tasks

**1.1: Create Clean Script** (10 min)

Create `scripts/clean-all.sh`:

```bash
#!/bin/bash
set -e

echo "=== DwarFS Complete Clean ==="
echo ""

# Remove all build directories
echo "Removing build directories..."
rm -rf build-*/ build/ 2>/dev/null || true

# Remove CMake cache files
echo "Removing CMake cache..."
find . -name "CMakeCache.txt" -delete 2>/dev/null || true
find . -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null || true

# Remove generated Thrift files
echo "Removing generated Thrift files..."
rm -rf include/dwarfs/gen-cpp2/ 2>/dev/null || true

# Remove vcpkg installed
echo "Removing vcpkg installed..."
rm -rf vcpkg_installed/ 2>/dev/null || true

# Remove test artifacts
echo "Removing test artifacts..."
rm -rf /tmp/dwarfs-test-* 2>/dev/null || true
rm -rf /tmp/test-*.{dff,dtc,dth,dwarfs} 2>/dev/null || true

echo ""
echo "✅ Clean complete. Ready for fresh build."
```

Make executable: `chmod +x scripts/clean-all.sh`

**1.2: Run Clean** (5 min)

```bash
./scripts/clean-all.sh
```

Verify all build artifacts removed.

**1.3: Verify Homebrew dwarfs Available** (15 min)

```bash
# Check Homebrew dwarfs version
brew list dwarfs --versions  # Should show v0.14.1 or similar

# Verify tools available
which mkdwarfs dwarfs dwarfsck dwarfsextract

# Test Homebrew tools work
mkdwarfs --version
dwarfs --version
dwarfsck --version
dwarfsextract --version
```

**Success Criteria**:
- All build artifacts removed
- Homebrew dwarfs v0.14.1 available and working
- Clean slate for fresh builds

---

## Phase 2: Build Matrix Testing (60 min)

### Objective
Build both FlatBuffers-only and Modern Thrift configurations from scratch.

### Tasks

**2.1: FlatBuffers-Only Build** (20 min)

```bash
# Configure
cmake -B build-fb-only -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON \
  -DWITH_TOOLS=ON

# Build
ninja -C build-fb-only

# Verify tools built
ls -lh build-fb-only/mkdwarfs
ls -lh build-fb-only/dwarfs*
```

**Expected**: All tools build successfully

**2.2: Modern Thrift Build** (40 min)

```bash
# Configure
cmake -B build-modern-thrift -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=$(pwd)/vcpkg_ports \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON \
  -DWITH_TOOLS=ON

# Build
ninja -C build-modern-thrift
```

**Expected**:
- All 5 libraries build successfully
- Tools may fail due to CMake linker bug (acceptable - use fb-only tools)

**Success Criteria**:
- build-fb-only: All tools working
- build-modern-thrift: All 5 libraries built
- Both builds verified functional

---

## Phase 3: Test Image Creation (45 min)

### Objective
Create test images with all three formats using multiple tool builds.

### Tasks

**3.1: Prepare Test Data** (10 min)

```bash
# Create consistent test data
mkdir -p /tmp/dwarfs-test-data
cd /tmp/dwarfs-test-data

# Create files
echo "Test file 1" > file1.txt
echo "Test file 2" > file2.txt
mkdir subdir
echo "Nested file" > subdir/nested.txt
ln -s file1.txt symlink.txt

# Create a binary file
dd if=/dev/urandom of=binary.dat bs=1024 count=100

# Get hash for verification
find . -type f -exec shasum -a 256 {} \; | sort > /tmp/test-data-hashes.txt
```

**3.2: Create Baseline Images (Homebrew)** (10 min)

```bash
# Use Homebrew mkdwarfs (v0.14.1)
/opt/homebrew/bin/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-homebrew-legacy.dwarfs

# Verify
/opt/homebrew/bin/dwarfsck /tmp/test-homebrew-legacy.dwarfs --check-integrity
```

**3.3: Create Images with FlatBuffers-Only Build** (10 min)

```bash
cd /Users/mulgogi/src/external/dwarfs

# FlatBuffers format
./build-fb-only/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-fb-only-flatbuffers.dff

# Legacy Thrift format (CRITICAL for Homebrew compatibility)
./build-fb-only/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-fb-only-legacy.dth \
  --metadata-format=legacy-thrift

# Verify magic bytes
echo "=== FlatBuffers Magic ==="
xxd /tmp/test-fb-only-flatbuffers.dff | head -2

echo "=== Legacy Thrift (no magic) ==="
xxd /tmp/test-fb-only-legacy.dth | head -2
```

**3.4: Create Images with Modern Thrift Build** (15 min)

```bash
# Use build-modern-thrift if tools built, otherwise skip
if [ -x ./build-modern-thrift/mkdwarfs ]; then
  # FlatBuffers format
  ./build-modern-thrift/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-modern-flatbuffers.dff

  # Modern Thrift Compact format
  ./build-modern-thrift/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-modern-thrift.dtc \
    --metadata-format=thrift-compact

  # Legacy Thrift format
  ./build-modern-thrift/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-modern-legacy.dth \
    --metadata-format=legacy-thrift

  # Verify magic bytes
  echo "=== Modern Thrift Magic ==="
  xxd /tmp/test-modern-thrift.dtc | head -2
else
  echo "⚠️ build-modern-thrift tools not built (CMake linker issue), using fb-only tools"
fi
```

**Success Criteria**:
- 6 test images created (or 3 if modern-thrift tools failed)
- Magic bytes correct for each format
- All images pass integrity checks

---

## Phase 4: Cross-Compatibility Matrix Testing (90 min)

### Objective
Verify all tool/format combinations work as expected, with special focus on Homebrew compatibility.

### Tasks

**4.1: Test Homebrew Tools with All Images** (30 min)

**CRITICAL TEST**: Homebrew v0.14.1 MUST read Legacy Thrift images created by OUR mkdwarfs

```bash
echo "=== Testing Homebrew dwarfs v0.14.1 ==="

# Test reading Homebrew's own image (baseline)
echo "1. Baseline: Homebrew reads Homebrew image"
/opt/homebrew/bin/dwarfsck /tmp/test-homebrew-legacy.dwarfs --check-integrity
/opt/homebrew/bin/dwarfsextract -i /tmp/test-homebrew-legacy.dwarfs -o /tmp/extracted-homebrew-baseline
diff -r /tmp/dwarfs-test-data /tmp/extracted-homebrew-baseline || echo "❌ BASELINE FAILED"

# Test reading our Legacy Thrift image created by build-fb-only (CRITICAL!)
echo "2. CRITICAL: Homebrew reads our Legacy Thrift (build-fb-only)"
/opt/homebrew/bin/dwarfsck /tmp/test-fb-only-legacy.dth --check-integrity || echo "❌❌❌ CRITICAL FAILURE - Homebrew cannot read our Legacy Thrift!"
/opt/homebrew/bin/dwarfsextract -i /tmp/test-fb-only-legacy.dth -o /tmp/extracted-homebrew-our-legacy
diff -r /tmp/dwarfs-test-data /tmp/extracted-homebrew-our-legacy || echo "❌❌❌ CRITICAL FAILURE - Extraction mismatch!"

# Test reading our Legacy Thrift image created by build-modern-thrift (CRITICAL!)
if [ -f /tmp/test-modern-legacy.dth ]; then
  echo "3. CRITICAL: Homebrew reads our Legacy Thrift (build-modern-thrift)"
  /opt/homebrew/bin/dwarfsck /tmp/test-modern-legacy.dth --check-integrity || echo "❌❌❌ CRITICAL FAILURE"
  /opt/homebrew/bin/dwarfsextract -i /tmp/test-modern-legacy.dth -o /tmp/extracted-homebrew-modern-legacy
  diff -r /tmp/dwarfs-test-data /tmp/extracted-homebrew-modern-legacy || echo "❌❌❌ CRITICAL FAILURE"
fi

# Test reading FlatBuffers (should fail gracefully - Homebrew doesn't support it)
echo "4. Expected to fail: Homebrew cannot read FlatBuffers"
/opt/homebrew/bin/dwarfsck /tmp/test-fb-only-flatbuffers.dff 2>&1 | grep -q "error\|fail" && echo "✅ Failed as expected" || echo "⚠️ Unexpected success"

# Test reading Modern Thrift (should fail gracefully - Homebrew doesn't support it)
if [ -f /tmp/test-modern-thrift.dtc ]; then
  echo "5. Expected to fail: Homebrew cannot read Modern Thrift"
  /opt/homebrew/bin/dwarfsck /tmp/test-modern-thrift.dtc 2>&1 | grep -q "error\|fail" && echo "✅ Failed as expected" || echo "⚠️ Unexpected success"
fi

echo ""
echo "=== CRITICAL VALIDATION RESULT ==="
echo "Homebrew v0.14.1 MUST successfully:"
echo "  ✓ Read our Legacy Thrift images (check integrity)"
echo "  ✓ Extract our Legacy Thrift images (all files)"
echo "  ✓ Match original data byte-for-byte"
echo ""
echo "If ANY of these fail: STOP and fix Legacy Thrift serializer!"
```

**4.2: Test FlatBuffers-Only Build** (30 min)

```bash
echo "=== Testing FlatBuffers-Only Build ==="

# Test all readable formats
for img in /tmp/test-homebrew-legacy.dwarfs \
           /tmp/test-fb-only-legacy.dth \
           /tmp/test-fb-only-flatbuffers.dff; do
  echo "Testing: $img"
  ./build-fb-only/dwarfsck $img --check-integrity
  ./build-fb-only/dwarfsextract -i $img -o /tmp/extracted-$(basename $img)
  diff -r /tmp/dwarfs-test-data /tmp/extracted-$(basename $img) || echo "❌ Extraction mismatch"
done

# Test Modern Thrift (should fail - no Thrift support)
if [ -f /tmp/test-modern-thrift.dtc ]; then
  echo "Expected to fail: FlatBuffers-only cannot read Modern Thrift"
  ./build-fb-only/dwarfsck /tmp/test-modern-thrift.dtc && echo "❌ Should have failed" || echo "✅ Failed as expected"
fi
```

**4.3: Test Modern Thrift Build** (30 min)

```bash
echo "=== Testing Modern Thrift Build ==="

# Test all formats (should read everything)
for img in /tmp/test-*.{dwarfs,dff,dtc,dth}; do
  [ -f "$img" ] || continue
  echo "Testing: $img"
  ./build-modern-thrift/dwarfsck $img --check-integrity || echo "Using fb-only tools"

  # Use fb-only tools if modern-thrift tools not built
  if [ ! -x ./build-modern-thrift/dwarfsck ]; then
    ./build-fb-only/dwarfsck $img --check-integrity
  fi
done
```

**Success Criteria**:
- ✅ **CRITICAL**: Homebrew v0.14.1 reads our Legacy Thrift images successfully
- ✅ All cross-format reads succeed where expected
- ✅ All integrity checks pass
- ✅ All extractions produce byte-for-byte identical output

**If CRITICAL test fails**: STOP. Fix Legacy Thrift serializer before proceeding.

---

## Phase 5: Metadata Verification (30 min)

### Objective
Verify metadata is correctly written and read for all formats.

### Tasks

**5.1: Compare Metadata Structures** (30 min)

```bash
# Extract metadata from all images
./build-fb-only/dwarfsck /tmp/test-homebrew-legacy.dwarfs --print-metadata > /tmp/meta-homebrew.txt
./build-fb-only/dwarfsck /tmp/test-fb-only-flatbuffers.dff --print-metadata > /tmp/meta-fb.txt
./build-fb-only/dwarfsck /tmp/test-fb-only-legacy.dth --print-metadata > /tmp/meta-legacy.txt

if [ -f /tmp/test-modern-thrift.dtc ]; then
  ./build-fb-only/dwarfsck /tmp/test-modern-thrift.dtc --print-metadata > /tmp/meta-modern.txt || true
fi

# Compare (should be semantically identical)
echo "Comparing metadata outputs..."
diff /tmp/meta-homebrew.txt /tmp/meta-legacy.txt && echo "✅ Legacy Thrift matches Homebrew" || echo "⚠️ Differences found"
diff /tmp/meta-fb.txt /tmp/meta-legacy.txt && echo "✅ FlatBuffers matches Legacy" || echo "⚠️ Differences found"
```

**Success Criteria**:
- Metadata semantically equivalent across formats
- File counts, sizes, inodes match

---

## Phase 6: v0.17.0 Release Preparation (45 min)

### Objective
Prepare release artifacts and documentation.

### Tasks

**6.1: Update CHANGES.md** (20 min)

Document v0.17.0 features:
- Three metadata formats support
- Modern Thrift CompactProtocol serializer
- FlatBuffers as modern default
- Legacy Thrift for v0.14.1 compatibility
- Custom jemalloc/Folly/fbthrift ports for vcpkg

**6.2: Version Bump** (10 min)

Update `cmake/version.cmake` to 0.17.0

**6.3: Create Release Tag** (5 min)

```bash
git tag -a v0.17.0 -m "Three metadata formats: FlatBuffers (default), Modern Thrift Compact, Legacy Thrift"
```

**6.4: Final Verification** (10 min)

Review all documentation is current:
- README.md
- doc/mkdwarfs.md
- doc/metadata-formats.md
- CHANGES.md

**Success Criteria**:
- CHANGES.md updated
- Version bumped to 0.17.0
- Git tag created
- All documentation current

---

## Success Criteria (Overall)

✅ **Phase 1**: Clean build environment verified
✅ **Phase 2**: Both build configurations successful
✅ **Phase 3**: All test images created with correct magic bytes
✅ **Phase 4**: **CRITICAL** - Homebrew v0.14.1 reads our Legacy Thrift images
✅ **Phase 5**: Metadata verification passes
✅ **Phase 6**: v0.17.0 release artifacts prepared

### Critical Success Requirement

**Homebrew v0.14.1 MUST successfully**:
- Check integrity of our Legacy Thrift images
- Extract our Legacy Thrift images
- Produce byte-for-byte identical extractions

**If this fails**: Do NOT proceed with v0.17.0 release.

---

## Time Estimate

| Phase | Time | Cumulative |
|-------|------|------------|
| 1. Clean environment | 30 min | 0:30 |
| 2. Build matrix | 60 min | 1:30 |
| 3. Test images | 45 min | 2:15 |
| 4. Cross-compat testing | 90 min | 3:45 |
| 5. Metadata verification | 30 min | 4:15 |
| 6. Release prep | 45 min | 5:00 |

**Total**: 5 hours (compressed timeline)

---

## Contingency Plans

### If Homebrew Cannot Read Our Legacy Thrift

**Action**: Debug and fix Legacy Thrift serializer
1. Compare binary output with Homebrew images
2. Check Thrift Compact wire format encoding
3. Verify all fields match v0.14.1 expectations
4. Fix serializer
5. Re-test

**Time**: +2-4 hours

### If CMake Linker Bug Blocks Testing

**Workaround**: Use build-fb-only tools for all testing
- Can still test all 3 formats (read via format detection)
- Cannot create Modern Thrift images (acceptable for initial validation)