# Session 89: Testing & Validation - Continuation Prompt

**Start Here**: Test and validate the Modern Thrift CompactProtocol serializer

---

## Quick Context

Session 88 achieved:
- ✅ Fixed serializer to use modern converters
- ✅ Completed Thrift schema (37→175 lines)
- ✅ Fixed CMake configuration
- ✅ Registry integration verified
- ⏳ Testing pending (requires vcpkg)

**Your Mission**: Build with vcpkg and run comprehensive tests

---

## Prerequisites

### vcpkg Setup Required

Modern Thrift requires vcpkg overlay ports:

```bash
# Ensure vcpkg is available
export VCPKG_ROOT=/path/to/vcpkg

# Verify overlay ports exist
ls -la vcpkg_ports/folly
ls -la vcpkg_ports/fbthrift
ls -la vcpkg_ports/jemalloc
```

---

## Step 1: Build with vcpkg (30 min)

### Configure Build

```bash
cd /Users/mulgogi/src/external/dwarfs

cmake -B build-modern -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DDWARFS_WITH_THRIFT=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DWITH_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release
```

**Expected Output**:
```
-- Modern Thrift Compact: ENABLED (using fbthrift v2025.12.29.00)
-- Folly version: v2025.12.29.00
```

### Build Modern Thrift Library

```bash
ninja -C build-modern dwarfs_metadata_modern_thrift
```

**Expected**: Clean build with no errors

### Build Tests

```bash
ninja -C build-modern modern_thrift_converter_tests
ninja -C build-modern modern_thrift_serialization_tests
```

---

## Step 2: Run Unit Tests (20 min)

### Converter Tests

```bash
./build-modern/modern_thrift_converter_tests
```

**Expected Results** (6 test cases):
- ✅ SimpleMetadataRoundTrip
- ✅ ComplexMetadataWithOptionals
- ✅ EmptyMetadata
- ✅ FullMetadataEquality
- ✅ ChunkConversion
- ✅ DirectoryConversion

### Serialization Tests

```bash
./build-modern/modern_thrift_serialization_tests
```

**Expected Results** (10 test cases):
- ✅ SerializerExists
- ✅ MagicBytes
- ✅ RoundTripSerialization
- ✅ NullMetadataThrows
- ✅ InvalidMagicBytesThrows
- ✅ TooShortDataThrows
- ✅ SerializerRegistration
- ✅ FormatDetection
- ✅ PriorityOrder
- ✅ CompactSize

---

## Step 3: Integration Tests (30 min)

### Create Test Image with Modern Thrift

```bash
# Create test directory
mkdir -p /tmp/test-modern
echo "Hello Modern Thrift" > /tmp/test-modern/test.txt

# Create DwarFS image with Modern Thrift
./build-modern/mkdwarfs \
  -i /tmp/test-modern \
  -o /tmp/test-modern.dtc \
  --metadata-format=thrift
```

**Verify**:
```bash
# Check image
./build-modern/dwarfsck /tmp/test-modern.dtc --json | jq '.metadata.format'
# Expected: "thrift_compact"

# Extract and verify
./build-modern/dwarfsextract -i /tmp/test-modern.dtc -o /tmp/extracted
diff -r /tmp/test-modern /tmp/extracted
```

---

## Step 4: Cross-Format Tests (20 min)

### Test Format Detection

```bash
# Create images in all three formats
./build-modern/mkdwarfs -i /tmp/test-modern -o /tmp/test.dff --metadata-format=flatbuffers
./build-modern/mkdwarfs -i /tmp/test-modern -o /tmp/test.dtc --metadata-format=thrift
./build-modern/mkdwarfs -i /tmp/test-modern -o /tmp/test.dth --metadata-format=legacy

# Verify each format
for img in /tmp/test.{dff,dtc,dth}; do
  echo "=== $img ==="
  ./build-modern/dwarfsck $img --json | jq '.metadata.format'
done
```

**Expected**:
- test.dff: "flatbuffers"
- test.dtc: "thrift_compact"  
- test.dth: "legacy_thrift"

---

## Step 5: Performance Benchmarks (30 min)

### Size Comparison

```bash
ls -lh /tmp/test.{dff,dtc,dth}
```

**Expected Ratios**:
- FlatBuffers: ~103% (baseline + 3%)
- Modern Thrift: 100% (baseline, smallest)
- Legacy Thrift: 100% (baseline, same as Modern)

### Speed Benchmarks

```bash
# Serialization speed
time ./build-modern/mkdwarfs -i /tmp/test-modern -o /tmp/bench.dtc --metadata-format=thrift
time ./build-modern/mkdwarfs -i /tmp/test-modern -o /tmp/bench.dff --metadata-format=flatbuffers

# Extraction speed
time ./build-modern/dwarfsextract -i /tmp/bench.dtc -o /tmp/out1
time ./build-modern/dwarfsextract -i /tmp/bench.dff -o /tmp/out2
```

---

## Step 6: Update Documentation (20 min)

### Update Memory Bank

Update `.kilocode/rules/memory-bank/context.md`:
- Mark Phase 3 as COMPLETE
- Update progress: 3/6 phases → 50%
- Add test results

### Create Session 89 Completion Summary

Document:
- Build results
- Test pass rates
- Performance metrics
- Known issues (if any)

---

## Success Criteria

✅ **Build Success**:
- Modern Thrift library compiles
- All tests compile
- No linker errors

✅ **Unit Tests**:
- Converter tests: 6/6 passing
- Serialization tests: 10/10 passing

✅ **Integration Tests**:
- Can create .dtc images
- Can extract .dtc images
- Format detection works

✅ **Performance**:
- Size ≤100% (equal to Legacy Thrift)
- Speed competitive with FlatBuffers

---

## Time Budget

- Step 1 (Build): 30 min
- Step 2 (Unit Tests): 20 min
- Step 3 (Integration): 30 min
- Step 4 (Cross-Format): 20 min
- Step 5 (Benchmarks): 30 min
- Step 6 (Docs): 20 min
- **Total**: ~2.5 hours

---

## Common Issues & Solutions

### Issue: vcpkg folly not found

**Solution**: Use overlay ports
```bash
cmake -B build-modern -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports
```

### Issue: thrift1 not found

**Solution**: Install fbthrift via vcpkg
```bash
vcpkg install fbthrift --overlay-ports=./vcpkg_ports
```

### Issue: Thrift schema compile errors

**Solution**: Verify schema syntax
```bash
thrift1 --gen cpp2:no_metadata thrift/metadata_modern.thrift
```

---

## Next Session

After Session 89, proceed to Session 90: Build System Integration

Read: `doc/SESSION_90_CONTINUATION_PROMPT.md` (will be created in Session 89)

---

**Created**: 2026-01-06
**Session**: 89
**Goal**: Test and validate Modern Thrift implementation
**Estimated**: 2.5 hours