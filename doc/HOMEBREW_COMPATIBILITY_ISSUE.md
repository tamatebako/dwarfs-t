# Homebrew Compatibility Issue - CRITICAL BLOCKER

**Discovered**: 2025-12-30 (Session 55 Week 1)
**Severity**: CRITICAL - Blocks production use
**Status**: 🔴 BLOCKING

---

## Problem Summary

Our development branch (v0.14.1-98-g13159541fe on `feature/multi-format-serialization-fuse`) writes Thrift metadata that **Homebrew dwarfs 0.14.1 cannot read**, breaking backward compatibility.

### Compatibility Requirements

**Requirement 1**: **When we write Thrift, Homebrew 0.14.1 MUST be able to read it**
- Status: ❌ **BROKEN** (this is the blocker)
- Why: Ensures backward compatibility with production dwarfs deployments

**Requirement 2**: **When we write Thrift or FlatBuffers, we MUST be able to read them**
- Status: ✅ Expected to work (self-compatibility)
- Why: Ensures our own tools work correctly

**Note**: Homebrew 0.14.1 cannot read FlatBuffers (expected - it's a new format not in v0.14.1)

---

---

## Test Results

### Format Compatibility Matrix

| Test Case | Writer | Reader | Format | Status | Notes |
|-----------|--------|--------|--------|--------|-------|
| **Forward Compat** | Homebrew 0.14.1 | Our build | Thrift | ✅ PASS | We can read Homebrew files |
| **Backward Compat** | Our build | Homebrew 0.14.1 | **Thrift** | ❌ **FAIL** | **BLOCKER**: Homebrew cannot read our Thrift files |
| **Self Compat (Thrift)** | Our build | Our build | Thrift | ✅ PASS | Round-trip works |
| **Self Compat (FlatBuffers)** | Our build | Our build | FlatBuffers | ✅ PASS | Round-trip works |
| **New Format** | Our build | Homebrew 0.14.1 | FlatBuffers | ❌ EXPECTED | Homebrew doesn't support FlatBuffers |

**Key Finding**: The ONLY broken case is backward compatibility - Homebrew 0.14.1 cannot read Thrift files created by our build.

---

---

## Test Evidence

### Test 1: Homebrew 0.14.1 → Our Build (✅ WORKING)

```bash
# Homebrew creates file
$ /opt/homebrew/Cellar/dwarfs/0.14.1_3/bin/mkdwarfs \
    -i /tmp/test -o /tmp/homebrew.dwarfs -l7

# Our build reads it successfully
$ ./build/dwarfsck -i /tmp/homebrew.dwarfs
DwarFS version 2.5 [2]
History:
  1: [2025-12-30 18:46:13] libdwarfs v0.14.1 on Darwin [arm64], AppleClang 17.0.0.17000404
<inode:0> ---drwxr-xr-x (1 entries)
  <inode:1> ----rw-r--r--  [1 chunks] 22
```

**Result**: ✅ **Forward compatibility works** - We can read Homebrew files

### Test 2: Our Build (Thrift) → Homebrew 0.14.1 (❌ FAILED)

```bash
# Our build creates Thrift file
$ ./build/mkdwarfs --format=thrift \
    -i /tmp/test -o /tmp/local-thrift.dwarfs -l7
filesystem: 19 B in 1 blocks (1 chunks, 1/1 fragments, 1 inodes)
compressed filesystem: 1 blocks/1.03 KiB written

# Our build can read it
$ ./build/dwarfsck -i /tmp/local-thrift.dwarfs
DwarFS version 2.5 [2]
History:
  1: libdwarfs v0.14.1-98-g13159541fe-dirty (feature/multi-format-serialization-fuse)
<inode:0> ---drwxr-xr-x (1 entries)
  <inode:1> ----rw-r--r-- test.txt [1 chunks] 19

# Homebrew CANNOT read it
$ /opt/homebrew/Cellar/dwarfs/0.14.1_3/bin/dwarfsck -i /tmp/local-thrift.dwarfs
dwarfs::runtime_error: [metadata_types.cpp:583] unexpected data size in names: 8 != 4304355923
```

**Result**: ❌ **Backward compatibility BROKEN** - Homebrew cannot read our files

### Test 3: Our Build (FlatBuffers) → Homebrew 0.14.1 (❌ EXPECTED FAIL)

```bash
# Our build creates FlatBuffers file (default)
$ ./build/mkdwarfs -i /tmp/test -o /tmp/local-fb.dwarfs -l7

# Homebrew cannot read it (expected - new format)
$ /opt/homebrew/Cellar/dwarfs/0.14.1_3/bin/dwarfsck -i /tmp/local-fb.dwarfs
dwarfs::runtime_error: [metadata_v2.cpp:107] invalid schema size
```

**Result**: ❌ Expected - FlatBuffers is new format not supported by v0.14.1

---

## Root Cause Analysis

### Likely Causes (Priority Order)

1. **Thrift Schema Changes** 🔴 HIGH PROBABILITY
   - Changes in `thrift/metadata.thrift` since v0.14.1 release
   - Breaking changes in frozen2 serialization format
   - String table compression (`compact_names`) changes

2. **Metadata Version Mismatch** 🟡 MEDIUM PROBABILITY
   - Version history handling differences
   - Metadata packing options changed

3. **Dependency Version Drift** 🟢 LOW PROBABILITY
   - Different Folly/fbthrift versions
   - Different compiler optimizations

### Specific Error Analysis

**Error**: `unexpected data size in names: 8 != 4304355923`
**Location**: `metadata_types.cpp:583`
**Type**: Data size mismatch in names table

**Interpretation**:
- Expected size: 8 bytes
- Actual size: 4,304,355,923 bytes (4.3 GB - clearly corrupted/wrong)
- Suggests: Endianness issue, pointer corruption, or format version mismatch

---

## Investigation Plan

### Phase 1: Baseline Comparison (1 hour)

1. **Checkout v0.14.1 tag**:
   ```bash
   git checkout v0.14.1
   mkdir build-v0.14.1
   cmake -B build-v0.14.1 -GNinja -DCMAKE_BUILD_TYPE=Release
   ninja -C build-v0.14.1
   ```

2. **Test v0.14.1 → Homebrew compatibility**:
   ```bash
   ./build-v0.14.1/mkdwarfs --format=thrift -i /tmp/test -o /tmp/v0141.dwarfs -l7
   /opt/homebrew/.../dwarfsck -i /tmp/v0141.dwarfs
   ```
   - Expected: ✅ PASS (baseline compatibility)

3. **Compare Thrift schemas**:
   ```bash
   git diff v0.14.1..HEAD -- thrift/metadata.thrift
   ```
   - Identify ALL changes to Thrift schema

### Phase 2: Binary Diff Analysis (30 min)

1. **Hex dump comparison**:
   ```bash
   xxd /tmp/homebrew.dwarfs > homebrew.hex
   xxd /tmp/local-thrift.dwarfs > local.hex
   diff -u homebrew.hex local.hex | head -100
   ```

2. **Metadata section extraction**:
   ```bash
   # Use dwarfsck to export metadata as JSON
   ./build/dwarfsck --export-metadata=/tmp/homebrew-meta.json \
       -i /tmp/homebrew.dwarfs
   ./build/dwarfsck --export-metadata=/tmp/local-meta.json \
       -i /tmp/local-thrift.dwarfs
   diff -u homebrew-meta.json local-meta.json
   ```

### Phase 3: Code Analysis (1 hour)

1. **Review commits affecting Thrift**:
   ```bash
   git log v0.14.1..HEAD --oneline -- \
       thrift/ \
       src/metadata/ \
       src/reader/internal/metadata_v2_thrift.cpp \
       src/writer/internal/metadata_builder.cpp
   ```

2. **Identify breaking changes**:
   - Schema modifications
   - Serialization logic changes
   - Packing option changes

3. **Check frozen2 usage**:
   - Verify Folly frozen2 API compatibility
   - Check for frozen2 version-specific behavior

### Phase 4: Fix Implementation (2-4 hours)

**Option A: Revert Breaking Changes** (if minimal)
- Revert specific commits that broke compatibility
- Test with Homebrew

**Option B: Version Compatibility Layer** (if changes needed)
- Add v0.14.1 compatibility mode: `--compat-version=0.14.1`
- Write v0.14.1-compatible Thrift by default
- OR bump version to 0.15.0 and document breaking change

**Option C: FlatBuffers-Only** (nuclear option)
- Remove Thrift support entirely
- Document as breaking change requiring v0.15.0+
- NOT RECOMMENDED - breaks backward compatibility

---

## Success Criteria

✅ **Phase 1 Complete** when:
- v0.14.1 tag builds successfully
- v0.14.1 → Homebrew compatibility confirmed
- All Thrift schema changes documented

✅ **Phase 2 Complete** when:
- Metadata binary differences identified
- Specific breaking change location pinpointed

✅ **Phase 3 Complete** when:
- Root cause identified with specific commit/change
- Fix strategy decided

✅ **ISSUE RESOLVED** when:
1. **Backward compatibility restored**: Homebrew 0.14.1 can read Thrift files created by our build
   ```bash
   # Create with our build (Thrift format)
   ./build/mkdwarfs --format=thrift -i /tmp/test -o /tmp/compat.dwarfs -l7

   # Read with Homebrew (must succeed)
   /opt/homebrew/Cellar/dwarfs/0.14.1_3/bin/dwarfsck -i /tmp/compat.dwarfs
   /opt/homebrew/Cellar/dwarfs/0.14.1_3/bin/dwarfsextract \
       -i /tmp/compat.dwarfs -o /tmp/extracted
   diff -r /tmp/test /tmp/extracted
   ```

2. **Self-compatibility verified**: Our build can read files it creates (both formats)
   ```bash
   # Thrift round-trip
   ./build/mkdwarfs --format=thrift -i /tmp/test -o /tmp/test-thrift.dwarfs -l7
   ./build/dwarfsck -i /tmp/test-thrift.dwarfs
   ./build/dwarfsextract -i /tmp/test-thrift.dwarfs -o /tmp/verify-thrift
   diff -r /tmp/test /tmp/verify-thrift

   # FlatBuffers round-trip
   ./build/mkdwarfs -i /tmp/test -o /tmp/test-fb.dwarfs -l7
   ./build/dwarfsck -i /tmp/test-fb.dwarfs
   ./build/dwarfsextract -i /tmp/test-fb.dwarfs -o /tmp/verify-fb
   diff -r /tmp/test /tmp/verify-fb
   ```

3. **Regression test added**: Automated test prevents future backward compatibility breaks

---

## Impact Assessment

### Current State
- ❌ **Production**: BLOCKED - Cannot deploy to users with Homebrew dwarfs
- ❌ **Testing**: BLOCKED - Cannot validate against production format
- ✅ **Development**: OK - Self-compatible

### Affected Use Cases
1. **Users with Homebrew dwarfs 0.14.1**: Cannot use files created by our build
2. **Migration from 0.14.1**: Cannot upgrade in-place
3. **Shared filesystems**: Incompatible between versions

---

## Timeline

| Phase | Duration | Status |
|-------|----------|--------|
| Phase 1: Baseline | 1 hour | 🔴 NOT STARTED |
| Phase 2: Binary Diff | 30 min | 🔴 NOT STARTED |
| Phase 3: Code Analysis | 1 hour | 🔴 NOT STARTED |
| Phase 4: Fix | 2-4 hours | 🔴 NOT STARTED |
| Testing | 1 hour | 🔴 NOT STARTED |

**Total Estimated**: 5.5-7.5 hours

---

## Related Files

- `thrift/metadata.thrift` - Thrift schema definition
- `src/metadata/converters/thrift_metadata_converter.cpp` - Thrift serialization
- `src/reader/internal/metadata_v2_thrift.cpp` - Thrift deserialization
- `src/reader/internal/metadata_types_thrift.cpp` - Thrift type handling
- `src/writer/internal/metadata_builder.cpp` - Metadata construction

---

**Last Updated**: 2025-12-30 18:51 HKT
**Next Action**: Execute Phase 1 (Baseline Comparison)