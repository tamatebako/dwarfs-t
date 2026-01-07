# Thrift-Only Build Complete Fix Plan

**Created**: 2025-12-02 21:38 HKT  
**Goal**: Get Thrift-only build fully working with all tests passing  
**Priority**: CRITICAL  
**Estimated Time**: 4-6 hours

---

## Current Status

**Build Status**: ✅ All tools compile successfully  
**Test Status**: ❌ Segfault + 10+ test failures  
**Root Cause**: Test infrastructure and possibly core code assumes FlatBuffers

---

## Phase 1: Debug Segfault (Est: 2 hours)

### 1.1 Isolate Segfault Location

**Objective**: Identify which test causes segfault

**Method**:
```bash
cd build-tb

# Run tests one at a time to find culprit
./dwarfs_unit_tests --gtest_list_tests > all-tests.txt

# Binary search through tests
./dwarfs_unit_tests --gtest_filter="metadata_test.*"
./dwarfs_unit_tests --gtest_filter="serialization*"
```

### 1.2 Use Debugger

**With lldb**:
```bash
lldb ./dwarfs_unit_tests
(lldb) run --gtest_brief=1
# When it crashes:
(lldb) bt  # Backtrace
(lldb) frame select 0
(lldb) p variable_name  # Inspect variables
```

### 1.3 Identify Root Cause

**Common Issues**:
1. Null pointer dereference
2. Invalid memory access in Thrift frozen2 structures
3. Missing FlatBuffers guard in test code
4. Format detection returns invalid result

### 1.4 Fix Segfault

**Likely Fixes**:
- Add null checks
- Guard FlatBuffers-specific code
- Fix format detection logic
- Initialize pointers properly

---

## Phase 2: Fix Format Detection (Est: 1 hour)

### 2.1 Analyze SerializerRegistry

**File**: `src/metadata/serialization/serializer_registry.cpp`

**Check**:
- Does it register Thrift serializer when FLATBUFFERS=OFF?
- Is FlatBuffers checked first unconditionally?
- Does auto-detection work for Thrift-only?

### 2.2 Fix Facade Factory

**File**: `src/metadata/serialization/facade_factory.cpp`

**Ensure**:
- Format detection works with only Thrift
- Default format is Thrift when FlatBuffers not available
- Error messages are clear

### 2.3 Update Tests

**Files**: `test/metadata/serialization_test.cpp`, `test/metadata/serialization/serialization_facade_test.cpp`

**Fix**:
- Guard FlatBuffers-specific tests
- Add Thrift-specific test variants
- Fix format detection tests

---

## Phase 3: Fix Metadata Tests (Est: 1 hour)

### 3.1 Fix metadata_factory_test

**Error**: `create_with_explicit_thrift_format` fails

**Check**:
- Factory creates Thrift serializer correctly
- No FlatBuffers assumptions

### 3.2 Fix metadata_test.basic

**Error**: Basic metadata test fails

**Likely Cause**:
- Test data assumes FlatBuffers format
- Test uses FlatBuffers-specific operations

### 3.3 Fix Converter Tests

**Errors**:
- `RoundTripStringTableTest.PreservesNameIndicesAndTables`
- `ThriftMetadataConverterTest.DirectoryConversion`

**Action**: Guard these properly or fix Thrift paths

---

## Phase 4: Fix Serialization Benchmarks (Est: 30 min)

### 4.1 Guard FlatBuffers Benchmarks

**File**: `test/metadata/serialization_benchmark_test.cpp`

**Fix**: Wrap FlatBuffers benchmarks with:
```cpp
#ifdef DWARFS_HAVE_FLATBUFFERS
// FlatBuffers benchmarks
#endif
```

### 4.2 Add Thrift Benchmarks

Ensure Thrift benchmarks run when THRIFT=ON

---

## Phase 5: Validate All Builds (Est: 1 hour)

### 5.1 Build and Test FlatBuffers-only

**Verify**: Still 1,600/1,613 passing (no regressions)

### 5.2 Build and Test Thrift-only

**Target**: 1,600/1,613 passing (13 FlatBuffer tests skipped)

### 5.3 Build and Test Dual-format

**Target**: 1,613/1,613 passing (all tests)

---

## Phase 6: CI Validation (Est: 30 min)

### 6.1 Update CI

Remove `continue-on-error` for Thrift-only once tests pass

### 6.2 Push and Verify

Ensure all CI jobs pass for all 3 builds

---

## Success Criteria

- [ ] No segfaults in any build
- [ ] FlatBuffers-only: 1,600/1,613 tests ✅
- [ ] Thrift-only: 1,600/1,613 tests ✅  
- [ ] Dual-format: 1,613/1,613 tests ✅
- [ ] CI passes for all configurations
- [ ] Tools functional in all builds
- [ ] Documentation updated

---

## Files to Check/Modify

### High Priority (Likely Causes):
- `src/metadata/serialization/serializer_registry.cpp`
- `src/metadata/serialization/facade_factory.cpp`
- `test/metadata/serialization_test.cpp`
- `test/metadata/serialization_benchmark_test.cpp`

### Medium Priority:
- `test/metadata_factory_test.cpp`
- `test/metadata_test.cpp`
- `test/metadata/converters/*`

### May Need Guards:
- Any test file with FlatBuffers-specific logic
- Format detection code
- Default format selection

---

**Status**: Ready to execute systematic debugging  
**Next Action**: Start Phase 1.1 - Isolate segfault location  
**Timeline**: 4-6 hours focused work