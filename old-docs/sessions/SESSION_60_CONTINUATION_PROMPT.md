# Session 60 Continuation Prompt

**Date**: 2025-12-31
**Status**: 🟡 **BUILD IN PROGRESS** - vcpkg compiling Folly/wangle/fbthrift
**Goal**: Fix Thrift converter, verify Homebrew v0.14.1 compatibility

---

## QUICK START

```bash
cd /Users/mulgogi/src/external/dwarfs

# 1. Check build status
ps aux | grep 87524 | grep -v grep  # If running, wait
tail -50 /tmp/cmake-session60-final.log  # Check progress

# 2. If build complete, build test binaries
ninja -C build-converter-test dwarfs_unit_tests mkdwarfs dwarfsck

# 3. Run converter round-trip tests
./build-converter-test/dwarfs_unit_tests --gtest_filter="*ConverterRoundTrip*"

# 4. Fix bugs in src/metadata/converters/cpp_thrift_converter.cpp
# 5. Re-test until all 7 tests pass
# 6. Run Homebrew compatibility tests
```

---

## CONTEXT

### What Was Done

**Session 59**:
- Fixed wangle TFO typo patch
- Added RULE 4 (vcpkg-only for Folly/Thrift)
- Started vcpkg build (failed)

**Session 60** (this session):
- Fixed fbthrift SHA512 checksum
- Fixed wangle CMake config directory issue
- Restarted vcpkg build (currently running)

### Current State

**vcpkg Build**:
- PID: 87524 (check if running: `ps aux | grep 87524`)
- Log: `/tmp/cmake-session60-final.log`
- Progress: ~50% (package 101/204)
- Started: 2025-12-31 23:04 HKT
- ETA: 23:20-23:30 HKT (15-20 min remaining)

**Build Directory**: `build-converter-test/`

**Modified Files**:
1. `vcpkg_ports/fbthrift/portfile.cmake` - SHA512 fix
2. `vcpkg_ports/wangle/portfile.cmake` - Added CMake dir patch
3. `vcpkg_ports/wangle/fix-cmake-dir.patch` - New patch file

---

## CRITICAL PROBLEM

**Issue**: Thrift converter has asymmetric field conversions
- **Symptom**: Files created by our build cannot be read by Homebrew dwarfs v0.14.1
- **Root Cause**: [`src/metadata/converters/cpp_thrift_converter.cpp`](../src/metadata/converters/cpp_thrift_converter.cpp) missing field mappings
- **Evidence**: 48-byte corruption, 26-byte metadata loss
- **Solution**: Fix converter to be fully symmetric (both directions)

---

## STEP-BY-STEP EXECUTION

### Step 1: Wait for Build Completion

```bash
# Monitor
tail -f /tmp/cmake-session60-final.log

# Check if still running
ps aux | grep 87524 | grep -v grep

# If complete, check exit status
# Success: Log ends with "-- Build files written to..."
# Failure: Log ends with "CMake Error" or "BUILD_FAILED"
```

**If build fails**:
- Check last 100 lines: `tail -100 /tmp/cmake-session60-final.log`
- Debug issue
- Fix and rebuild

### Step 2: Build Test Binaries

```bash
cd /Users/mulgogi/src/external/dwarfs
ninja -C build-converter-test dwarfs_unit_tests mkdwarfs dwarfsck
```

**Expected**: Binaries created successfully

### Step 3: Run Converter Round-Trip Tests

```bash
./build-converter-test/dwarfs_unit_tests --gtest_filter="*ConverterRoundTrip*"
```

**Expected Output**:
```
[==========] Running 7 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 7 tests from ConverterRoundTrip
[ RUN      ] ConverterRoundTrip.BasicMetadata
...
```

**Expected Result**: Some/all tests FAIL (this identifies bugs)

### Step 4: Analyze Test Failures

**For each failing test**:
1. Note which domain fields are corrupted
2. Find corresponding code in `cpp_thrift_converter.cpp`
3. Check BOTH directions:
   - `domain_to_thrift()` (write path)
   - `thrift_to_domain()` (read path)
4. Identify missing/incorrect field mappings

**Example Analysis**:
```
Test: ConverterRoundTrip.StringTables
Failure: Expected index=42, got index=0

Location: cpp_thrift_converter.cpp:110
Issue: Missing write-side conversion
Fix: Add thrift.index = domain.string_table_index;
```

### Step 5: Fix Converter Bugs

**File**: [`src/metadata/converters/cpp_thrift_converter.cpp`](../src/metadata/converters/cpp_thrift_converter.cpp)

**Known Issue Pattern** (Lines 101-120):
```cpp
// WRONG - Asymmetric
void thrift_to_domain(ThriftType const& thrift, DomainType& domain) {
  domain.string_table_index = thrift.index;  // Read ✓
}

void domain_to_thrift(DomainType const& domain, ThriftType& thrift) {
  // Missing write! ✗
}

// RIGHT - Symmetric
void thrift_to_domain(ThriftType const& thrift, DomainType& domain) {
  domain.string_table_index = thrift.index;  // Read ✓
}

void domain_to_thrift(DomainType const& domain, ThriftType& thrift) {
  thrift.index = domain.string_table_index;  // Write ✓
}
```

**Fix Strategy**:
1. For each corrupted field, find its converter functions
2. Ensure BOTH directions exist and are correct
3. Pay attention to:
   - Index calculations
   - Optional field handling
   - Default values
   - Packed structure offsets

### Step 6: Iterative Testing

```bash
# After each fix
ninja -C build-converter-test dwarfs_unit_tests
./build-converter-test/dwarfs_unit_tests --gtest_filter="*ConverterRoundTrip*"
```

**Goal**: All 7 tests PASS

### Step 7: Homebrew Compatibility Tests

**Create Test Data**:
```bash
mkdir -p /tmp/test-data
echo "test file content" > /tmp/test-data/file.txt
mkdir /tmp/test-data/subdir
echo "nested file" > /tmp/test-data/subdir/nested.txt
```

**Test 1: Our Build → Homebrew Read**:
```bash
# Create with our build (Thrift format, level 1)
./build-converter-test/mkdwarfs -i /tmp/test-data -o /tmp/our.dft \
  --format=thrift -l1

# Verify with Homebrew v0.14.1
/opt/homebrew/bin/dwarfsck -i /tmp/our.dft

# Extract with Homebrew
/opt/homebrew/bin/dwarfsextract -i /tmp/our.dft -o /tmp/extracted-by-hb

# Compare
diff -r /tmp/test-data /tmp/extracted-by-hb
```

**Expected**: No errors, identical files

**Test 2: Homebrew → Our Build Read**:
```bash
# Create with Homebrew
/opt/homebrew/bin/mkdwarfs -i /tmp/test-data -o /tmp/hb.dft -l1

# Verify with our build
./build-converter-test/dwarfsck -i /tmp/hb.dft

# Extract with our build
./build-converter-test/dwarfsextract -i /tmp/hb.dft -o /tmp/extracted-by-us

# Compare
diff -r /tmp/test-data /tmp/extracted-by-us
```

**Expected**: No errors, identical files

**Test 3: FlatBuffers Format**:
```bash
# Create with FlatBuffers
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

### Step 8: Update Documentation

```bash
# Update memory bank
# Edit: .kilocode/rules/memory-bank/context.md
# - Mark Session 60 complete
# - Update component status (Thrift converters: ✅)

# Archive session docs
mkdir -p old-docs/session-60
mv doc/SESSION_60_*.md old-docs/session-60/

# Create completion summary
# Create: doc/SESSION_60_COMPLETION_SUMMARY.md
```

---

## CRITICAL REMINDERS

### RULE 3: Dual-Mode is MANDATORY

**NEVER**:
- Remove Thrift support
- Remove FlatBuffers support
- Make one format "second-class"
- Skip cross-format testing

**ALWAYS**:
- Fix both formats
- Test both formats
- Maintain backward compatibility

### RULE 4: ALWAYS Use vcpkg for Folly/Thrift

**NEVER**:
- Use Homebrew Folly/Thrift
- Mix Homebrew and vcpkg dependencies

**ALWAYS**:
- Use vcpkg with overlay ports
- Static linking for Tebako compatibility

### Architecture Principles

**Converter Pattern**:
```
Domain Model (C++ structs)
    ↕️ BIDIRECTIONAL - MUST be symmetric
Thrift Types (frozen2)
```

**If conversion is asymmetric** → Data corruption → Incompatibility

---

## SUCCESS CRITERIA

- [ ] vcpkg build completes successfully
- [ ] All 7 converter round-trip tests PASS
- [ ] Homebrew v0.14.1 can read our Thrift files
- [ ] We can read Homebrew v0.14.1 Thrift files
- [ ] FlatBuffers format works correctly
- [ ] Memory bank updated
- [ ] Session documentation archived

---

## TIME ESTIMATES

- Build completion: **15-20 min** (if not already done)
- Testing: **30 min**
- Fixes: **1-2 hours**
- Compatibility: **30 min**
- Documentation: **30 min**
- **Total**: **3-4 hours**

---

## DOCUMENTATION

**Read First**:
1. [`doc/SESSION_60_CONTINUATION_PLAN.md`](SESSION_60_CONTINUATION_PLAN.md) - Full plan
2. [`doc/SESSION_60_IMPLEMENTATION_STATUS.md`](SESSION_60_IMPLEMENTATION_STATUS.md) - Status tracker
3. [`.kilocode/rules/memory-bank/critical-rules.md`](../.kilocode/rules/memory-bank/critical-rules.md) - RULE 3 & 4

**Background**:
- [`doc/HOMEBREW_COMPATIBILITY_ISSUE.md`](HOMEBREW_COMPATIBILITY_ISSUE.md) - Problem investigation
- [`doc/SESSION_56_CONVERTER_FIX_PLAN.md`](SESSION_56_CONVERTER_FIX_PLAN.md) - Original analysis

---

## IF THINGS GO WRONG

### Build Fails

```bash
# Check logs
tail -200 /tmp/cmake-session60-final.log
grep -i "error\|failed" /tmp/cmake-session60-final.log

# Common issues:
# - vcpkg port error → Fix portfile
# - Missing dependency → Check vcpkg.json
# - Compile error → Fix source code
```

### Tests Fail to Compile

```bash
# Check ninja errors
ninja -C build-converter-test dwarfs_unit_tests 2>&1 | tee /tmp/test-build-error.log

# Common issues:
# - Missing header → Check includes
# - Linker error → Check library dependencies
```

### Converter Tests All Fail

**Likely Cause**: Fundamental converter architecture issue

**Action**:
1. Review converter pattern in [`doc/SESSION_56_CONVERTER_FIX_PLAN.md`](SESSION_56_CONVERTER_FIX_PLAN.md)
2. Check if domain model changed
3. Verify Thrift schema matches

### Homebrew Compatibility Fails

**Root Causes**:
- Converter still has bugs (re-run round-trip tests)
- Homebrew version mismatch (verify `/opt/homebrew/bin/mkdwarfs --version`)
- Format change (check metadata version)

---

**START HERE**: Check build status → Run tests → Fix bugs → Verify compatibility → Document