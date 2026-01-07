# Session 76 Continuation Prompt

**Start Here**: Quick-start guide for three-format validation and v0.17.0 release

---

## Context

Session 75 **COMPLETED** all documentation for the three-format metadata system. Session 76 will validate the implementation and prepare v0.17.0 release.

## Current State

✅ **Working**:
- All 3 metadata serializers implemented
- FlatBuffers, Modern Thrift Compact, Legacy Thrift
- All 5 libraries compiled successfully
- All official documentation updated

⏹ **Pending**:
- Clean build validation
- Cross-format compatibility testing
- **CRITICAL**: Homebrew v0.14.1 compatibility with our Legacy Thrift images
- v0.17.0 release preparation

---

## Quick Start (5 hours)

### Step 1: Read Documentation (10 min)

```bash
# Read the full plan
cat doc/SESSION_76_CONTINUATION_PLAN.md

# Read Session 75 completion
cat doc/SESSION_75_COMPLETION_STATUS.md

# Check implementation status
cat doc/SESSION_76_IMPLEMENTATION_STATUS.md
```

### Step 2: Clean Environment (30 min)

```bash
# Run clean script
./scripts/clean-all.sh

# Verify Homebrew dwarfs available
brew list dwarfs --versions  # Should show v0.14.1
which mkdwarfs dwarfs dwarfsck dwarfsextract
```

### Step 3: Build Both Configurations (60 min)

```bash
# FlatBuffers-only build
cmake -B build-fb-only -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON \
  -DWITH_TOOLS=ON
ninja -C build-fb-only

# Modern Thrift build
cmake -B build-modern-thrift -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=$(pwd)/vcpkg_ports \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON \
  -DWITH_TOOLS=ON
ninja -C build-modern-thrift
```

### Step 4: Create Test Images (45 min)

```bash
# Prepare test data
mkdir -p /tmp/dwarfs-test-data
cd /tmp/dwarfs-test-data
echo "Test file 1" > file1.txt
echo "Test file 2" > file2.txt
mkdir subdir
echo "Nested file" > subdir/nested.txt
dd if=/dev/urandom of=binary.dat bs=1024 count=100

# Create images with all formats
cd /Users/mulgogi/src/external/dwarfs

# Homebrew baseline
/opt/homebrew/bin/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-homebrew.dwarfs

# Our FlatBuffers
./build-fb-only/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-fb.dff

# Our Legacy Thrift (CRITICAL for Homebrew compatibility!)
./build-fb-only/mkdwarfs -i /tmp/dwarfs-test-data -o /tmp/test-legacy.dth \
  --metadata-format=legacy-thrift

# Verify magic bytes
xxd /tmp/test-fb.dff | head -2     # Should show "DFBF"
xxd /tmp/test-legacy.dth | head -2  # No magic bytes
```

### Step 5: CRITICAL Test - Homebrew Compatibility (30 min)

**PRIMARY VALIDATION GOAL**: Homebrew dwarfs v0.14.1 MUST successfully read Legacy Thrift images created by OUR mkdwarfs

```bash
echo "=== CRITICAL TEST: Homebrew reads OUR Legacy Thrift images ==="
echo "This is the PRIMARY validation requirement for v0.17.0"
echo ""

# Baseline: Homebrew reads its own image
echo "1. Baseline: Homebrew reads Homebrew image"
/opt/homebrew/bin/dwarfsck /tmp/test-homebrew.dwarfs --check-integrity
/opt/homebrew/bin/dwarfsextract -i /tmp/test-homebrew.dwarfs -o /tmp/extracted-homebrew
diff -r /tmp/dwarfs-test-data /tmp/extracted-homebrew

# CRITICAL: Homebrew reads OUR Legacy Thrift image
echo ""
echo "2. ❗CRITICAL TEST: Homebrew reads OUR Legacy Thrift image"
echo "   (Created by our mkdwarfs --metadata-format=legacy-thrift)"
echo ""

/opt/homebrew/bin/dwarfsck /tmp/test-legacy.dth --check-integrity
if [ $? -ne 0 ]; then
    echo "❌❌❌ CRITICAL FAILURE: Homebrew cannot check our Legacy Thrift image!"
    echo "❌❌❌ STOP: Do NOT proceed with v0.17.0"
    echo "❌❌❌ Fix Legacy Thrift serializer before continuing"
    exit 1
fi

/opt/homebrew/bin/dwarfsextract -i /tmp/test-legacy.dth -o /tmp/extracted-our-legacy
if [ $? -ne 0 ]; then
    echo "❌❌❌ CRITICAL FAILURE: Homebrew cannot extract our Legacy Thrift image!"
    echo "❌❌❌ STOP: Do NOT proceed with v0.17.0"
    exit 1
fi

diff -r /tmp/dwarfs-test-data /tmp/extracted-our-legacy
if [ $? -ne 0 ]; then
    echo "❌❌❌ CRITICAL FAILURE: Extracted data doesn't match original!"
    echo "❌❌❌ Our Legacy Thrift images are NOT Homebrew-compatible"
    echo "❌❌❌ STOP: Do NOT proceed with v0.17.0"
    exit 1
fi

echo ""
echo "✅✅✅ SUCCESS: Homebrew v0.14.1 CAN read our Legacy Thrift images!"
echo "✅✅✅ Backward compatibility VERIFIED"
echo ""
echo "This means:"
echo "  ✓ Our Legacy Thrift serializer produces v0.14.1-compatible output"
echo "  ✓ Homebrew users can read our Legacy Thrift images"
echo "  ✓ Backward compatibility is MAINTAINED"
echo ""
```

**What This Validates**:
- Our Legacy Thrift serializer writes format compatible with Homebrew v0.14.1
- Homebrew dwarfs can mount our Legacy Thrift images
- Homebrew dwarfs can extract our Legacy Thrift images
- Extracted data is byte-for-byte identical to original

**If This Test Fails**:
1. ❌ STOP immediately
2. ❌ Do NOT proceed with any other tests
3. ❌ Do NOT proceed with v0.17.0 release
4. Debug Legacy Thrift serializer:
   - Compare binary output: `xxd /tmp/test-homebrew.dwarfs > homebrew.hex`
   - Compare with ours: `xxd /tmp/test-legacy.dth > ours.hex`
   - Find differences: `diff homebrew.hex ours.hex`
   - Fix serialization code to match Homebrew's format
5. Re-run this test until it passes

### Step 6: Cross-Compatibility Matrix (90 min)

Test all tool/format combinations per the compatibility matrix in the continuation plan.

### Step 7: Release Preparation (45 min)

```bash
# Update CHANGES.md with v0.17.0 features
# Update cmake/version.cmake to 0.17.0
# Create git tag: git tag -a v0.17.0 -m "Three metadata formats"
```

---

## Success Criteria

- [ ] Clean environment verified
- [ ] Both builds successful
- [ ] All test images created with correct magic bytes
- [ ] **CRITICAL**: Homebrew v0.14.1 reads our Legacy Thrift images
- [ ] All cross-compatibility tests pass
- [ ] Metadata verification passes
- [ ] v0.17.0 release artifacts prepared

---

## Critical Failure Condition

**If Homebrew v0.14.1 cannot read our Legacy Thrift images**:
1. STOP immediately
2. Debug Legacy Thrift serializer
3. Compare binary output with Homebrew images
4. Fix and re-test
5. Do NOT proceed with v0.17.0 until fixed

---

## Key References

- **Full Plan**: [`doc/SESSION_76_CONTINUATION_PLAN.md`](SESSION_76_CONTINUATION_PLAN.md)
- **Status Tracker**: [`doc/SESSION_76_IMPLEMENTATION_STATUS.md`](SESSION_76_IMPLEMENTATION_STATUS.md)
- **Session 75 Status**: [`doc/SESSION_75_COMPLETION_STATUS.md`](SESSION_75_COMPLETION_STATUS.md)
- **Architecture**: [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md)

---

## Estimated Time

**Total**: 5 hours (compressed timeline)

**Breakdown**:
- Clean environment: 30 min
- Builds: 60 min
- Test images: 45 min
- Critical test: 30 min
- Cross-compat: 90 min
- Metadata verify: 30 min
- Release prep: 45 min

---

**Created**: 2026-01-05 11:25 HKT
**Prerequisites**: Session 75 complete, Homebrew dwarfs v0.14.1 available
**Next Session**: Comprehensive validation and v0.17.0 release