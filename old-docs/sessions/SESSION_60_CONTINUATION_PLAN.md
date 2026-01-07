# Session 60 Continuation Plan

**Date**: 2025-12-31
**Goal**: Complete vcpkg build, test and fix Thrift converters, verify Homebrew compatibility
**Deadline**: Complete all work in this session

---

## Overview

**Critical Blocker**: Thrift converter data corruption prevents backward compatibility with Homebrew dwarfs v0.14.1

**Root Cause**: [`src/metadata/converters/cpp_thrift_converter.cpp`](../src/metadata/converters/cpp_thrift_converter.cpp) has asymmetric conversion bugs causing 48-byte corruption and 26-byte metadata loss

**Solution Strategy**: Fix converter implementation, not bypass it. Both FlatBuffers and Thrift formats MUST work (RULE 3).

---

## Phase Summary (Compressed for Deadline)

| Phase | Tasks | Priority | Time Est. |
|-------|-------|----------|-----------|
| P1: Build Completion | vcpkg build monitoring | CRITICAL | In Progress |
| P2: Testing | Run converter round-trip tests | CRITICAL | 30 min |
| P3: Fixes | Fix all converter bugs | CRITICAL | 1-2 hours |
| P4: Compatibility | Verify Homebrew v0.14.1 | CRITICAL | 30 min |
| P5: Documentation | Update memory bank, archive docs | REQUIRED | 30 min |

**Total Estimated Time**: 3-4 hours remaining

---

## Phase 1: Build Completion (In Progress)

**Status**: vcpkg build running (PID 87524, ~50% complete at package 101/204)

### Tasks

1. **Monitor Build Progress**
   ```bash
   tail -f /tmp/cmake-session60-final.log
   ps aux | grep 87524 | grep -v grep
   ```

2. **On Build Success**
   ```bash
   cd /Users/mulgogi/src/external/dwarfs
   ninja -C build-converter-test dwarfs_unit_tests mkdwarfs dwarfsck
   ```

3. **On Build Failure**
   - Check logs: `tail -200 /tmp/cmake-session60-final.log`
   - Debug vcpkg errors
   - Fix port issues
   - Rebuild

**Completion Criteria**:
- ✅ vcpkg installs all 204 packages
- ✅ CMake configuration succeeds
- ✅ Test binaries compile

---

## Phase 2: Converter Testing (30 minutes)

**Objective**: Run round-trip tests to identify ALL converter bugs

### Tasks

1. **Run Round-Trip Tests**
   ```bash
   ./build-converter-test/dwarfs_unit_tests --gtest_filter="*ConverterRoundTrip*"
   ```

2. **Analyze Test Output**
   - Expected: 7 test cases total
   - Expected: Some/all may FAIL
   - Document: Which fields are corrupted
   - Document: Exact error messages

3. **Identify Bug Locations**
   - File: [`src/metadata/converters/cpp_thrift_converter.cpp`](../src/metadata/converters/cpp_thrift_converter.cpp)
   - Known issue area: Lines 101-120 (string_table index)
   - Look for: Index field asymmetries
   - Look for: Missing field conversions

**Completion Criteria**:
- ✅ All test cases executed
- ✅ Failure patterns documented
- ✅ Bug locations identified

**Expected Test Cases**:
1. Basic metadata round-trip
2. Inodes round-trip
3. Directories round-trip
4. Chunks round-trip
5. String tables round-trip
6. Symlinks round-trip
7. Devices round-trip

---

## Phase 3: Converter Fixes (1-2 hours)

**Objective**: Fix ALL asymmetric conversions in cpp_thrift_converter.cpp

### Architecture Review

**Converter Pattern**:
```
Domain Model (C++ structs)
    ↕️ (bidirectional conversion)
Thrift Types (frozen2)
```

**CRITICAL**: Both directions MUST be symmetric:
- `domain_to_thrift()` - Serialize (write)
- `thrift_to_domain()` - Deserialize (read)

### Known Issue Areas

**1. String Table Index Fields** (Lines 101-120)
```cpp
// WRONG - Asymmetric
domain.string_table_index = thrift.index;      // Read
// Missing write side!

// RIGHT - Symmetric
domain.string_table_index = thrift.index;      // Read
thrift.index = domain.string_table_index;      // Write
```

**2. Packed Structures**
- Ensure ALL domain fields map to Thrift fields
- Check chunk tables, directory tables, shared files
- Verify index calculations

**3. Optional Fields**
- Ensure presence checks on both sides
- Default values must match

### Fix Strategy

1. **For Each Failing Test**:
   - Identify which domain fields are corrupted
   - Find corresponding Thrift conversion code
   - Check BOTH directions (read AND write)
   - Fix asymmetry

2. **Iterative Testing**:
   ```bash
   # After each fix
   ninja -C build-converter-test dwarfs_unit_tests
   ./build-converter-test/dwarfs_unit_tests --gtest_filter="*ConverterRoundTrip*"
   ```

3. **Validation**:
   - All 7 tests MUST pass
   - No shortcuts or skipped tests
   - Complete symmetry required

**Completion Criteria**:
- ✅ All converter tests PASS (7/7)
- ✅ No corruption in round-trip
- ✅ Both directions symmetric

---

## Phase 4: Compatibility Testing (30 minutes)

**Objective**: Verify Homebrew dwarfs v0.14.1 compatibility in BOTH directions

### Test Setup

```bash
# Create test data
mkdir -p /tmp/test-data
echo "test file content" > /tmp/test-data/file.txt
mkdir /tmp/test-data/subdir
echo "nested file" > /tmp/test-data/subdir/nested.txt
```

### Test 1: Our Build → Homebrew Read

```bash
# Create with our build (Thrift format)
./build-converter-test/mkdwarfs -i /tmp/test-data -o /tmp/our.dft \
  --format=thrift -l1

# Verify with Homebrew v0.14.1
/opt/homebrew/bin/dwarfsck -i /tmp/our.dft

# Extract with Homebrew
/opt/homebrew/bin/dwarfsextract -i /tmp/our.dft -o /tmp/extracted-by-hb

# Compare content
diff -r /tmp/test-data /tmp/extracted-by-hb
```

**Expected**: No errors, identical files

### Test 2: Homebrew → Our Build Read

```bash
# Create with Homebrew
/opt/homebrew/bin/mkdwarfs -i /tmp/test-data -o /tmp/hb.dft -l1

# Verify with our build
./build-converter-test/dwarfsck -i /tmp/hb.dft

# Extract with our build
./build-converter-test/dwarfsextract -i /tmp/hb.dft -o /tmp/extracted-by-us

# Compare content
diff -r /tmp/test-data /tmp/extracted-by-us
```

**Expected**: No errors, identical files

### Test 3: FlatBuffers Format

```bash
# Create with FlatBuffers format
./build-converter-test/mkdwarfs -i /tmp/test-data -o /tmp/fb.dff \
  --format=flatbuffers -l1

# Verify
./build-converter-test/dwarfsck -i /tmp/fb.dff

# Extract
./build-converter-test/dwarfsextract -i /tmp/fb.dff -o /tmp/extracted-fb

# Compare
diff -r /tmp/test-data /tmp/extracted-fb
```

**Expected**: No errors, identical files

**Completion Criteria**:
- ✅ Homebrew can read our Thrift files
- ✅ We can read Homebrew Thrift files
- ✅ FlatBuffers format works correctly
- ✅ All extracted files identical

---

## Phase 5: Documentation (30 minutes)

### Tasks

1. **Update Memory Bank**
   - File: [`.kilocode/rules/memory-bank/context.md`](../.kilocode/rules/memory-bank/context.md)
   - Mark Session 60 complete
   - Update component status (Thrift converters: ✅)
   - Update current status

2. **Archive Session Documents**
   ```bash
   mkdir -p old-docs/session-60
   mv doc/SESSION_60_*.md old-docs/session-60/
   ```

3. **Create Completion Summary**
   - File: `doc/SESSION_60_COMPLETION_SUMMARY.md`
   - Document fixes made
   - Document test results
   - Document compatibility verification

4. **Update README.adoc** (if needed)
   - Document vcpkg requirement for Thrift builds
   - Document dual-format support

**Completion Criteria**:
- ✅ Memory bank updated
- ✅ Session docs archived
- ✅ Completion summary created
- ✅ README updated (if needed)

---

## Critical Files

### To Modify

1. [`src/metadata/converters/cpp_thrift_converter.cpp`](../src/metadata/converters/cpp_thrift_converter.cpp) - Fix converter bugs
2. [`.kilocode/rules/memory-bank/context.md`](../.kilocode/rules/memory-bank/context.md) - Update status
3. `README.adoc` - Document requirements (if needed)

### Already Modified (Session 59-60)

1. `vcpkg_ports/fbthrift/portfile.cmake` - SHA512 fix
2. `vcpkg_ports/wangle/portfile.cmake` - Added CMake dir patch
3. `vcpkg_ports/wangle/fix-cmake-dir.patch` - New patch file

---

## Dependencies

**Build Dependencies** (vcpkg):
- Folly
- wangle (with TFO typo fix + CMake dir fix)
- fbthrift (with correct SHA512)
- Boost, glog, gflags, etc.

**Test Dependencies**:
- Built test binaries
- Homebrew dwarfs v0.14.1 (`/opt/homebrew/bin/mkdwarfs`, `/opt/homebrew/bin/dwarfsck`)

---

## Risk Mitigation

### If Converter Fixes Take >2 Hours

**Escalation Plan**:
1. Document exact bug patterns
2. Create minimal reproduction case
3. Request upstream assistance
4. Continue with FlatBuffers-only if critical

**Note**: Per RULE 3, we MUST maintain dual-mode. This is non-negotiable for Homebrew compatibility.

### If Homebrew Compatibility Fails

**Root Cause Analysis**:
1. Check if it's converter bug or format change
2. Verify Homebrew version (must be v0.14.1)
3. Compare metadata schemas
4. Check for missing fields

---

## Success Metrics

- [x] vcpkg build succeeds (with corrected ports)
- [ ] All 7 converter round-trip tests PASS
- [ ] Homebrew v0.14.1 → our build works
- [ ] Our build → Homebrew v0.14.1 works
- [ ] FlatBuffers format functional
- [ ] Memory bank updated
- [ ] Documentation complete

---

**Last Updated**: 2025-12-31 23:05 HKT
**Current Phase**: P1 - Build in progress (50%)
**Next Action**: Monitor build completion, then proceed to P2