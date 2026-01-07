# Phase 5: Smoke Testing & Final Cleanup

## Context

Phase 4 complete - FlatBuffers integrated, jemalloc improved, CLI updated. Now need:
1. **Smoke test** - Compare FlatBuffers vs Thrift performance
2. **Complete cleanup** - Remove all 777 Cereal/Bitsery references

## Current Blocker

Local build cannot complete due to pre-existing AppleClang 17 issue:
- File: `src/writer/internal/similarity_ordering.cpp:687`
- Issue: Lambda capture of move-only types
- Status: Unrelated to FlatBuffers work
- Resolution: CI/CD with GCC/Clang should succeed

## Phase 5A: Smoke Testing

### Prerequisites
Need working build with BOTH formats (FlatBuffers + Thrift)

**Option 1: Use CI/CD Artifacts** (Recommended)
```bash
# Wait for CI/CD to complete
# Download artifacts from successful build
# Platform: Linux x86_64 or macOS x86_64 (GCC/Clang succeed)

gh run download <RUN_ID> -n dwarfs-<artifact-name>
```

**Option 2: Fix Local Build** (Alternative)
Fix `similarity_ordering.cpp:687` lambda capture issue, then rebuild locally.

### Smoke Test Plan

Once we have working binaries:

```bash
# Create test dataset
mkdir -p /tmp/smoketest/source
cp -r /usr/include /tmp/smoketest/source/headers
dd if=/dev/urandom of=/tmp/smoketest/source/randomdata bs=1M count=50

# Test 1: Create FlatBuffers image
time ./mkdwarfs -i /tmp/smoketest/source \
  -o /tmp/smoketest/flatbuffers.dwarfs \
  --metadata-format=flatbuffers \
  -l 5

# Test 2: Create Thrift image
time ./mkdwarfs -i /tmp/smoketest/source \
  -o /tmp/smoketest/thrift.dwarfs \
  --metadata-format=thrift \
  -l 5

# Compare sizes
ls -lh /tmp/smoketest/*.dwarfs

# Test 3: Check both images
time ./dwarfsck /tmp/smoketest/flatbuffers.dwarfs --checksum
time ./dwarfsck /tmp/smoketest/thrift.dwarfs --checksum

# Test 4: Extract both (measure decompression time)
time ./dwarfsextract -i /tmp/smoketest/flatbuffers.dwarfs \
  -o /tmp/smoketest/extracted-fb
time ./dwarfsextract -i /tmp/smoketest/thrift.dwarfs \
  -o /tmp/smoketest/extracted-thrift

# Test 5: Verify format detection
./dwarfsck /tmp/smoketest/flatbuffers.dwarfs --json | \
  jq '.metadata.serialization_format'
./dwarfsck /tmp/smoketest/thrift.dwarfs --json | \
  jq '.metadata.serialization_format'

# Test 6: Compare extracted contents
diff -r /tmp/smoketest/extracted-fb /tmp/smoketest/extracted-thrift
```

### Expected Results

**Image Sizes:**
- FlatBuffers: Slightly larger metadata (est. +5-10%)
- Thrift: Slightly smaller (bit-packed Frozen2)
- Block data: Identical (same compression)

**Performance:**
- FlatBuffers: Faster deserialization (zero-copy)
- Thrift: Slightly slower (unpacking required)
- Difference: Minimal for typical use cases

**Compatibility:**
- Both formats: Identical extracted content
- Format detection: Automatic, transparent

## Phase 5B: Cereal/Bitsery Cleanup

### Overview

Remove all 777 references to development-only formats.

### Breakdown by Category

**1. WIP Documentation (~290 refs) - DELETE entirely**
```bash
rm -f doc/CEREAL_BITSERY_REMOVAL_PLAN.md
rm -f doc/FLATBUFFERS_FINAL_CEREAL_BITSERY_REMOVED.md
rm -f doc/FLATBUFFERS_CONTINUATION_NEXT_SESSION.md
rm -f doc/FLATBUFFERS_WORK_COMPLETE.md
rm -f doc/FLATBUFFERS_NEXT_PHASE.md
rm -f doc/SESSION_CONTINUATION_2025-11-*.md
rm -f .github/CI-UNIFIED-WORKFLOW.md
rm -f benchmarks/results/test_report.md
```

**2. Test Files (~223 refs) - REMOVE test cases**
- `test/metadata/serialization_test.cpp` (60 refs)
- `test/metadata/format_conversion_test.cpp` (67 refs)
- `test/metadata/serialization_benchmark_test.cpp` (32 refs)
- `test/metadata/serialization/serialization_facade_test.cpp` (30 refs)
- `test/tool_mkdwarfs_format_conversion_test.cpp` (33 refs)

Action: Remove Cereal/Bitsery test cases, keep FlatBuffers/Thrift tests

**3. CMake Configuration (~20 refs)**
- `cmake/libdwarfs.cmake` (10 refs)
- `CMakeLists.txt` (~10 refs)

Action: Remove DWARFS_HAVE_CEREAL/BITSERY definitions and linking

**4. Source Code (~28 refs)**
- `src/history.cpp` (28 refs)

Action: Check if these are old serialized strings (likely can ignore or clean)

**5. Memory Bank/Docs (~50 refs)**
- Memory bank files
- Design docs
- User-facing documentation

Action: Update to mention only FlatBuffers + Thrift

### Execution Priority

**HIGH PRIORITY** (User-facing):
1. Delete WIP documentation
2. Update user-facing docs (dwarfs-format.md, etc.)
3. Clean memory bank

**MEDIUM PRIORITY** (Build clarity):
4. Update CMake configuration
5. Clean test files

**LOW PRIORITY** (Historical):
6. Check src/history.cpp (may be in old serialized data)

### Commit Strategy

```bash
git add -A
git commit -m "chore: remove development-only format references

- Delete WIP documentation files
- Remove Cereal/Bitsery test cases
- Clean CMake configuration
- Update user documentation

Note: Cereal/Bitsery were never released, development only."
```

## Timeline

**Phase 5A (Smoke Test)**:
- Depends on: CI/CD completion OR local build fix
- Duration: 15-30 minutes
- Output: Performance comparison report

**Phase 5B (Cleanup)**:
- Independent: Can proceed in parallel
- Duration: 1-1.5 hours
- Output: Clean codebase (0 refs to removed formats)

## Success Criteria

### Smoke Test Success
- [ ] FlatBuffers image created successfully
- [ ] Thrift image created successfully
- [ ] Both images verified with dwarfsck
- [ ] Both images extracted correctly
- [ ] Format auto-detection works
- [ ] Performance metrics documented

### Cleanup Success
- [ ] WIP docs deleted
- [ ] Test files cleaned (only FlatBuffers+Thrift)
- [ ] CMake cleaned
- [ ] User docs updated
- [ ] Memory bank updated
- [ ] Verification: 0 Cereal/Bitsery refs in code/docs

## Next Steps

**Immediate**:
1. Wait for CI/CD successful build OR fix similarity_ordering.cpp
2. Download artifacts or rebuild locally
3. Run smoke test suite
4. Document results

**Then**:
1. Execute cleanup plan (Phase 5B)
2. Commit cleanup
3. Final verification
4. Create PR for merge to main