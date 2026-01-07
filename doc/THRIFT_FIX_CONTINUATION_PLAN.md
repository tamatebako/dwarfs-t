# Thrift-Only Complete Fix - Continuation Plan

**Date**: 2025-12-02  
**Status**: Ready to Execute  
**Current**: Tools build ✅, Tests fail ❌  
**Goal**: All 3 builds pass all tests

---

## Quick Context

**Achieved So Far**:
- ✅ Benchmark infrastructure complete (all 3 priorities)
- ✅ Thrift-only **tools build and work**
- ✅ Fixed 16+ compilation errors
- ✅ Fixed linking errors

**Current Blocker**:
- ❌ Tests fail: `"Failed to create serializer for format: FlatBuffers"`
- Root cause: Default format hardcoded to FlatBuffers

**Time Needed**: 3-4 hours focused debugging

---

## Phase 1: Fix Default Format Selection (1 hour)

### Root Cause

Tests create `filesystem_writer` with empty options `{}`, which defaults to FlatBuffers format. When FlatBuffers is disabled, this fails.

### Solution Approach

**Make default format conditional**:
```cpp
// In filesystem_writer or metadata_builder
SerializationFormat get_default_format() {
#ifdef DWARFS_HAVE_FLATBUFFERS
  return SerializationFormat::FlatBuffers;
#elif defined(DWARFS_HAVE_THRIFT)
  return SerializationFormat::Thrift;
#endif
}
```

### Files to Check:

1. **`src/writer/internal/filesystem_writer_detail.cpp`** or similar:
   - Find where serializer is created
   - Make format selection conditional

2. **`src/writer/internal/metadata_builder.cpp`**:
   - Check if builder defaults to FlatBuffers
   - Make conditional

3. **`src/metadata/serialization/facade_factory.cpp`**:
   - Ensure creates correct serializer based on format
   - Default selection logic

### Fix Pattern:

Search for patterns like:
```cpp
auto format = SerializationFormat::FlatBuffers;  // WRONG

// Should be:
auto format = get_default_available_format();
```

---

## Phase 2: Fix Test Infrastructure (1 hour)

### File: `test/metadata_test.cpp`

**Line 139**: `filesystem_writer fsw(oss, lgr, pool, prog, {});`

**Fix**: Explicitly set format to Thrift:
```cpp
writer::filesystem_writer_options fs_opts;
// Set format to Thrift explicitly for this test
writer::filesystem_writer fsw(oss, lgr, pool, prog, fs_opts, 
                               metadata::serialization::SerializationFormat::Thrift);
```

### Other Test Files

Search for similar patterns:
```bash
grep -r "filesystem_writer.*{}" test/*.cpp
```

---

## Phase 3: Fix Serializer Registry (1 hour)

### File: `src/metadata/serialization/serializer_registry.cpp`

**Ensure**:
1. Thrift serializer registers when `DWARFS_HAVE_THRIFT` defined
2. FlatBuffers serializer skipped when not available
3. Registry returns available serializers correctly

**Check Functions**:
- `register_all()` - Must register Thrift
- `get_serializer(format)` - Must handle Thrift
- `detect_format(data)` - Must detect Thrift correctly

###File: `src/metadata/serialization/facade_factory.cpp`

**Ensure**:
- `create(format)` works for Thrift when FlatBuffers disabled
- Error messages mention available formats
- Default format is appropriate

---

## Phase 4: Guard FlatBuffers-Specific Tests (30 min)

### Files to Guard:

**Pattern**:
```cpp
#ifdef DWARFS_HAVE_FLATBUFFERS
TEST(SerializationTest, FlatBuffersSpecific) {
  // FlatBuffers-specific test
}
#endif
```

**Files**:
- `test/metadata/serialization_test.cpp`
- `test/metadata/serialization_benchmark_test.cpp`
- `test/metadata/serialization/serialization_facade_test.cpp`

---

## Phase 5: Test All Builds (30 min)

### 5.1 Test Flatbuffers-only
```bash
cd build-fb && ./dwarfs_unit_tests
# Expected: 1,600/1,613 passing
```

### 5.2 Test Thrift-only
```bash
cd build-tb && ./dwarfs_unit_tests
# Target: 1,600/1,613 passing (13 FlatBuffers tests skipped)
```

### 5.3 Build Dual-format
```bash
cmake -B build-dual -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
cmake --build build-dual
cd build-dual && ./dwarfs_unit_tests
# Target: 1,613/1,613 passing (all tests)
```

---

## Debugging Commands

### Find Which Test Segfaults:
```bash
cd build-tb
./dwarfs_unit_tests --gtest_list_tests > all-tests.txt

# Binary search
./dwarfs_unit_tests --gtest_filter="*" --gtest_break_on_failure
```

### Use Debugger:
```bash
lldb ./dwarfs_unit_tests
(lldb) run --gtest_filter="metadata_test.basic"
# On crash:
(lldb) bt
(lldb) frame select 0
(lldb) p this
```

### Check Specific Failing Tests:
```bash
# Run one failing test
./dwarfs_unit_tests --gtest_filter="metadata_factory_test.create_with_explicit_thrift_format"

# See what failed
./dwarfs_unit_tests --gtest_filter="SerializationTest.*" 2>&1 | grep -A5 "FAILED"
```

---

## Success Criteria

- [ ] No segfaults in test suite
- [ ] `metadata_test.basic` passes
- [ ] `metadata_factory_test.*` passes  
- [ ] Serialization tests pass or skip appropriately
- [ ] FlatBuffers-only: 1,600/1,613 ✅
- [ ] Thrift-only: 1,600/1,613 ✅
- [ ] Dual-format: 1,613/1,613 ✅

---

## Quick Wins to Try First

### 1. Set Default Format Conditionally

Find this pattern:
```cpp
constexpr auto DEFAULT_FORMAT = SerializationFormat::FlatBuffers;
```

Change to:
```cpp
#ifdef DWARFS_HAVE_FLATBUFFERS
constexpr auto DEFAULT_FORMAT = SerializationFormat::FlatBuffers;
#elif defined(DWARFS_HAVE_THRIFT)
constexpr auto DEFAULT_FORMAT = SerializationFormat::Thrift;
#endif
```

### 2. Fix Test to Use Thrift

In `test/metadata_test.cpp` line 139:
```cpp
writer::metadata_options meta_opts;
meta_opts.format = metadata::serialization::SerializationFormat::Thrift;

writer::filesystem_writer_options fs_opts;
writer::filesystem_writer fsw(oss, lgr, pool, prog, fs_opts, meta_opts);
```

### 3. Check Serializer Registry

Ensure `serializer_registry::register_all()` registers Thrift serializer

---

## Timeline

| Phase | Duration | Total |
|-------|----------|-------|
| 1. Fix default format | 1h | 1h |
| 2. Fix test infrastructure | 1h | 2h |
| 3. Fix serializer registry | 1h | 3h |
| 4. Guard tests | 0.5h | 3.5h |
| 5. Validate all builds | 0.5h | 4h |

**Total**: 4 hours compressed timeline

---

**Status**: Ready to execute  
**Next**: Phase 1 - Find and fix default format selection  
**Files to Start**: `src/writer/internal/filesystem_writer_detail.cpp`, `src/metadata/serialization/`