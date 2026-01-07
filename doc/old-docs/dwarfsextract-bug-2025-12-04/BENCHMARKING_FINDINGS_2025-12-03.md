# Benchmarking Findings Report - December 3, 2025

**Date**: 2025-12-03 20:39 HKT  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Build**: FlatBuffers-only (build-fb/)  
**Objective**: Validate FlatBuffers format for v0.16.0 release

---

## Executive Summary

Attempted comprehensive benchmarking of 3 build configurations (FlatBuffers-only, Thrift-only, Dual-format) but discovered **two critical pre-existing issues** that block complete testing:

1. ❌ **Missing Thrift header** prevents Thrift/Dual builds
2. ❌ **dwarfsextract bug** prevents extraction verification

Despite these blockers, we successfully validated core FlatBuffers functionality:
- ✅ Image creation works
- ✅ Metadata format correct
- ✅ Image verification works
- ✅ Format detection works

---

## Build Status

### ✅ FlatBuffers-Only Build (build-fb/)

**Status**: Complete and Functional  
**Tools Built**:
- `mkdwarfs` - 4.7M
- `dwarfsck` - 2.4M  
- `dwarfsextract` - 2.6M

**Test Results**:
```bash
# Image creation
✅ Created test image: 30 MiB → 20 MiB (FlatBuffers format)
✅ Deduplication working: Saved 10 MiB (detected file3 = file1)
✅ Format: FlatBuffers 24.3.25

# Metadata verification
✅ dwarfsck can read and validate FlatBuffers images
✅ JSON output working correctly
✅ Metadata format: "FlatBuffers" (confirmed)
✅ Version: 2.5 [2] (correct)
```

**Validation Test**:
```bash
./build-fb/mkdwarfs -i /tmp/test-data -o test.dwarfs
# SUCCESS: Created 20M image from 30M data
# Dedup: 10M saved, 2 blocks, 3 chunks

./build-fb/dwarfsck test.dwarfs --json
# SUCCESS: Metadata readable, format=FlatBuffers
```

### ❌ Thrift/Dual-Format Builds

**Status**: Build Failed  
**Blocker**: Missing header file

**Error**:
```
src/writer/internal/metadata_builder.cpp:46: 
  fatal error: 'thrift_metadata_builder_impl.h' file not found

src/writer/internal/metadata_builder_factory.cpp:43:
  fatal error: 'thrift_metadata_builder_impl.h' file not found
```

**Root Cause**:
The `thrift_metadata_builder` class exists only in anonymous namespace within `thrift_metadata_builder.cpp`. It was never properly extracted into a header file during OOP refactoring (unlike the FlatBuffers version).

**Files Affected**:
- ❌ Missing: `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h`
- ✅ Exists: `include/dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h` (232 lines)
- ✅ Exists: `src/writer/internal/thrift_metadata_builder.cpp` (~1280 lines)

**Impact**:
- Cannot build with `DWARFS_WITH_THRIFT=ON`
- Cannot test backward compatibility at runtime
- Cannot validate recompression workflows
- Pre-existing technical debt (not a regression)

---

## Tool Functionality Assessment

### ✅ mkdwarfs (Image Creation)

**Status**: Fully Functional

**Tested**:
- Created FlatBuffers image successfully
- Deduplication working correctly
- Compression working correctly  
- Progress output working
- Log levels working

**Output Sample**:
```
1 dirs, 0/0 soft/hard links, 3/3 files, 0 other
original size: 30 MiB, hashed: 20 MiB (2 files, 869 MiB/s)  
saved by deduplication: 10 MiB (1 files)
filesystem: 20 MiB in 2 blocks (3 chunks, 2/2 fragments, 2 inodes)
```

### ✅ dwarfsck (Image Verification)

**Status**: Fully Functional

**Tested**:
- Can read FlatBuffers images
- Can output JSON metadata
- Can detect format correctly
- **Cannot** read Thrift images (expected - Thrift support not compiled)

**JSON Output Working**:
```json
{
  "metadata_format": "FlatBuffers",
  "version": {"header": 2, "major": 2, "minor": 5},
  "created_by": "libdwarfs v0.14.1-158-g685cb8045f-dirty"
}
```

### ❌ dwarfsextract (Image Extraction) - CRITICAL BUG

**Status**: Non-Functional (Segfault/Directory Error)

**Bug #1: Directory Extraction Fails**
```bash
./build-fb/dwarfsextract -i test.dwarfs -o /tmp/extract
# ERROR: filesystem error: in current_path: 
#        No such file or directory ["/tmp/extract"]
```

**Bug #2: Crashes with Segfault**
```bash
mkdir -p /tmp/extract
./build-fb/dwarfsextract -i test.dwarfs -o /tmp/extract
# ERROR: Segmentation fault: 11 (exit code 139)
```

**Root Cause**: Unknown - appears to be `current_path` handling issue in libarchive or filesystem code

**Impact**:
- Cannot verify extracted content matches original
- Cannot complete extraction benchmarks
- **BLOCKS release validation** (extraction is core functionality)

**Workaround**: None discovered - tar/archive extraction also fails

---

## Format Compatibility Testing

### FlatBuffers Image Reading

✅ **FlatBuffers-only build can read FlatBuffers images**
```bash
./build-fb/dwarfsck benchmark-results/flatbuffers-validation/test.dwarfs
# SUCCESS
```

### Thrift Image Reading  

❌ **FlatBuffers-only build cannot read Thrift images** (Expected)
```bash
./build-fb/dwarfsck benchmark-results/perl-thrift.dwarfs
# ERROR: FlatBuffers metadata verification failed
```

This is **expected behavior** - backward compatibility requires dual-format build.

### Existing Test Images

We have pre-built images for comparison:
```
benchmark-results/perl-flatbuffers.dwarfs  15M (Nov 20, FlatBuffers format)
benchmark-results/perl-thrift.dwarfs       15M (Nov 20, Thrift format)
```

**Size Comparison**: Both ~15M (similar compression ratio)

---

## Critical Issues Summary

### Issue #1: Missing Thrift Header (Pre-existing Debt)

**Severity**: High (blocks Thrift/dual builds)  
**Type**: Pre-existing technical debt  
**Fix Effort**: 1-2 hours

**Required Actions**:
1. Create `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h`
2. Extract `thrift_metadata_builder` class from anonymous namespace
3. Separate into header (declarations) + cpp (definitions)
4. Update includes in metadata_builder.cpp and metadata_builder_factory.cpp

**Files to Modify**:
- `src/writer/internal/thrift_metadata_builder.cpp` (extract class to header)
- CREATE: `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h`  
- `src/writer/internal/metadata_builder.cpp` (update include)
- `src/writer/internal/metadata_builder_factory.cpp` (update include)

### Issue #2: dwarfsextract Crash (Critical Bug)

**Severity**: Critical (blocks release validation)  
**Type**: Regression or platform-specific bug  
**Fix Effort**: Unknown (requires debugging)

**Error Patterns**:
1. `filesystem error: current_path: No such file or directory`
2. `Segmentation fault: 11` when output dir pre-created

**Debugging Steps Needed**:
1. Run with debugger (lldb) to get stack trace
2. Check if regression from recent changes
3. Test on other platforms (Linux, Windows)
4. Verify libarchive version compatibility

**Workaround**: None available

---

## Test Matrix Results

| Test Case | Status | Notes |
|-----------|--------|-------|
| Create FlatBuffers image | ✅ Pass | mkdwarfs works perfectly |
| Verify FlatBuffers image | ✅ Pass | dwarfsck reads metadata |
| Extract FlatBuffers image | ❌ Fail | dwarfsextract crashes |
| Create Thrift image | ⏸️ Blocked | Cannot build with Thrift |
| Verify Thrift image (FB build) | ✅ Pass (Expected Fail) | Correctly rejects Thrift |
| Create Dual-format build | ⏸️ Blocked | Missing header |
| Cross-format compatibility | ⏸️ Blocked | Need dual build |

---

## Recommendations for v0.16.0 Release

### Option 1: Fix Critical Issues First (Recommended)

**Approach**: Fix blocking issues before release

**Steps**:
1. **Fix dwarfsextract crash** (Critical - 2-4 hours debugging + fix)
2. Validate extraction works across platforms
3. **Optionally fix Thrift header** (1-2 hours) for complete testing
4. Run comprehensive benchmarks
5. Release with confidence

**Timeline**: +1 day  
**Risk**: Low (validates all functionality)

### Option 2: Release with Known Limitations

**Approach**: Document issues, release FlatBuffers-only

**Caveats**:
- ⚠️ **dwarfsextract broken** - users cannot extract images
- ⚠️ **No backward compatibility** - cannot read Thrift images  
- ⚠️ **Incomplete validation** - extraction not tested

**Timeline**: Immediate  
**Risk**: **HIGH** - extraction is core functionality

### Option 3: Hybrid - Fix Extract, Defer Thrift

**Approach**: Fix critical extract bug, defer Thrift for later

**Steps**:
1. Fix dwarfsextract crash (must have)
2. Test extraction thoroughly
3. Release FlatBuffers-only build
4. Defer Thrift support to future release

**Timeline**: +4-6 hours  
**Risk**: Medium (core functionality works, but no backward compat)

---

## Final Recommendation

**DO NOT RELEASE v0.16.0 until dwarfsextract is fixed.**

Extraction is a **core feature** - users must be able to extract images. A release with broken extraction would be unusable.

**Recommended Path**:
1. **Immediate**: Debug and fix dwarfsextract crash (Priority 1)
2. **Short-term**: Create comprehensive extraction tests
3. **Medium-term**: Fix Thrift header for complete testing
4. **Release**: When extraction verified working

---

## Technical Details

### Build Configuration

```cmake
# FlatBuffers-only build (build-fb/)
cmake -B build-fb -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=OFF

# Attempted Dual-format build (failed)
cmake -B build-dual -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=OFF
# Result: Build failed (missing thrift_metadata_builder_impl.h)
```

### Test Data

```bash
# Created test data
/tmp/test-data/
  file1.bin  10M (random)
  file2.bin  10M (random)
  file3.bin  10M (duplicate of file1)

# Created image
benchmark-results/flatbuffers-validation/test.dwarfs  20M
```

### Library Versions

```
FlatBuffers: 24.3.25
boost: 1.89.0
crypto (OpenSSL): 3.5.2
fmt: 11.2.0
xxhash: 0.8.3
zstd: 1.5.7
```

---

## Next Steps

### Immediate Actions

1. ✅ Document findings (this report)
2. ⏸️ Debug dwarfsextract crash
3. ⏸️ Create reproducer for extraction bug
4. ⏸️ Test on other platforms (Linux priority)

### Future Work

1. Create `thrift_metadata_builder_impl.h` header
2. Complete dual-format build testing
3. Validate backward compatibility
4. Run comprehensive benchmark matrix
5. Update CI/CD for format testing

---

## Conclusion

While FlatBuffers format validation shows the **core metadata system works perfectly**, the discovery of a **critical dwarfsextract bug blocks release**. 

**The v0.16.0 release should be delayed** until extraction functionality is restored and validated.

**Estimated Fix Time**: 4-8 hours (debugging + fix + validation)

---

**Status**: 🔴 **BLOCKED - Critical Bug**  
**Priority**: **P0 - Release Blocker**  
**Next Action**: Debug dwarfsextract crash