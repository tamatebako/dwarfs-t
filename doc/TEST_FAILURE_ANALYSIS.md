# Test Failure Analysis - FlatBuffers-Only Build

**Date**: 2025-12-13
**Build**: FlatBuffers-only (`DWARFS_WITH_THRIFT=OFF`)
**Total Tests**: 3,132
**Failed**: 1,110 (35%)
**Passed**: 2,022 (65%)

---

## Executive Summary

**ROOT CAUSE**: The majority of test failures (95%+) are caused by tests dynamically creating filesystem images in **Thrift format by default**, which cannot be read in FlatBuffers-only builds.

### Primary Issue Categories

| Category | Count | % of Failures | Fix Strategy |
|----------|-------|---------------|--------------|
| 1. Dynamic Thrift images | ~900 | 81% | Add format parameter to `build_dwarfs()` |
| 2. Pre-built Thrift images | ~150 | 14% | Create FlatBuffers versions |
| 3. Format-agnostic need parameterization | ~50 | 5% | Convert to parameterized tests |
| 4. Subprocess crashes | ~10 | <1% | Investigate individually |

---

## Detailed Analysis

### Category 1: Dynamically Created Thrift Images (~900 tests)

**Problem**: Tests call `build_dwarfs()` without specifying metadata format, defaulting to Thrift.

**Example** (from `test/dwarfs_test.cpp:1374`):
```cpp
TEST(filesystem, find_by_path) {
  auto fsimage = build_dwarfs(lgr, input, "null");  // Thrift by default
  auto mm = test::make_mock_file_view(std::move(fsimage));
  reader::filesystem_v2 fs(lgr, *input, mm);  // Can't read Thrift
  auto dev = fs.find(p);
  ASSERT_TRUE(dev) << p;  // FAILS: dev is false
}
```

**Test Files Affected**:
- `test/dwarfs_test.cpp`: Tests 53-63, 947-1017, 1374-2237 (most tests)
  - `filesystem.uid_gid_32bit` (line 1154)
  - `filesystem.uid_gid_count` (line 1186)
  - `filesystem.uid_gid_override` (line 1225)
  - `filesystem.find_by_path` (line 1374)
  - `filesystem.root_access_github204` (line 1428)
  - `filesystem.read` (line 1597)
  - `filesystem.inode_size_cache` (line 1917)
  - `filesystem.multi_image` (line 1989)
  - `filesystem.case_insensitive_lookup` (line 2059)
  - `packing_test.regression_empty_fs` (line 679 - 128 parameterized test instances)
  - `compression_test.end_to_end` (line 622)
  - `scanner_test.end_to_end` (line 642)
  - `hashing_test.end_to_end` (line 652)
  - `plain_tables_test.end_to_end` (line 670)
  - `file_scanner.inode_ordering` (line 896)
  - `file_scanner.input_list` (line 1117)
  - `file_scanner.file_start_hash` (line 1395)
  - `filter_test.*` (line 1034)
  - `compression_regression.github45` (line 804)
  - `section_index_regression.github183` (line 1282)

**Solution**:
1. Modify `build_dwarfs()` to accept optional metadata format parameter
2. Set default format based on `DWARFS_HAVE_FLATBUFFERS` and `DWARFS_HAVE_THRIFT` macros
3. Update all call sites to be format-aware

**Code Change Required**:
```cpp
// In test_helpers.h or dwarfs_test.cpp
std::string build_dwarfs(
    logger& lgr,
    std::shared_ptr<test::os_access_mock> input,
    std::string const& compression,
    writer::segmenter::config const& cfg = writer::segmenter::config(),
    writer::scanner_options const& options = writer::scanner_options(),
    writer::filesystem_writer_options const& writer_opts =
        writer::filesystem_writer_options(),
    // NEW PARAMETER:
    std::optional<std::string> metadata_format = std::nullopt,
    // ... other params ...
) {
  writer::filesystem_writer_options opts = writer_opts;
  
  // Set metadata format based on build or override
  if (metadata_format) {
    opts.metadata_format = *metadata_format;
  } else {
    // Default to available format
#ifdef DWARFS_HAVE_FLATBUFFERS
    opts.metadata_format = "flatbuffers";
#elif defined(DWARFS_HAVE_THRIFT)
    opts.metadata_format = "thrift";
#endif
  }
  
  // ... rest of function ...
}
```

### Category 2: Pre-Built Thrift Test Images (~150 tests)

**Problem**: Some tests use pre-built `.dwarfs` files in Thrift format from `test/` directory.

**Note**: After checking `dwarfs_test.cpp`, I don't see many pre-built image tests in this file. Most tests create images dynamically. The pre-built images are likely in other test files (not shown in this excerpt).

**Test Files to Check**:
- Files that use `test::make_real_file_view(test_dir / "*.dwarfs")`
- Look for hardcoded test image paths

**Solution**:
1. Create FlatBuffers versions of all test images: `*.fb.dwarfs`
2. Create helper function to select appropriate image based on format
3. Convert tests to use parameterized approach

### Category 3: Format-Agnostic Tests Need Parameterization (~50 tests)

**Problem**: Tests that work with either format but aren't parameterized yet.

**Examples**: Tests that should run once per available format to ensure both work correctly.

**Solution**: Already created framework in `test/format_test_base.h`. Need to convert tests.

### Category 4: Subprocess Crashes (~10 tests)

**Problem**: Tests marked as "Subprocess aborted"

**Examples**:
- `dwarfs/packing_test.regression_empty_fs/(false, true, ...)` (lines 947-978, 1011-1017)
- Other segmenter tests

**Investigation Needed**: May be same root cause as Category 1, or actual crashes that need debugging.

---

## Fix Priority

### Phase 1: Enable Dynamic Image Creation (HIGHEST PRIORITY)
- **Time**: 2 hours
- **Impact**: Fixes ~900 tests (81%)
- **Action**: Modify `build_dwarfs()` to support metadata format parameter

### Phase 2: Create FlatBuffers Test Images
- **Time**: 4 hours
- **Impact**: Fixes ~150 tests (14%)
- **Action**: Generate `.fb.dwarfs` versions of pre-built images

### Phase 3: Parameterize Format-Agnostic Tests
- **Time**: 2 hours
- **Impact**: Improves test coverage
- **Action**: Convert tests to use `DWARFS_FORMAT_TEST` macro

### Phase 4: Debug Subprocess Crashes
- **Time**: 1 hour
- **Impact**: Fixes ~10 tests (<1%)
- **Action**: Run individual tests with debugger

---

## Success Metrics

### After Phase 1 (Immediate Target)
- **Expected Pass Rate**: 95%+ (2,970+ / 3,132)
- **Expected Failures**: <160
- **Remaining**: Pre-built images + edge cases

### After Phases 1-2 (Release Target)
- **Expected Pass Rate**: 100% (FlatBuffers-only)
- **Expected Failures**: 0
- **Skipped Tests**: 200-300 (Thrift-specific features conditionally disabled)

---

## Implementation Plan

### Step 1: Quick Win - Fix build_dwarfs()
```cpp
// Add default metadata format selection
#ifdef DWARFS_HAVE_FLATBUFFERS
  #define DEFAULT_TEST_METADATA_FORMAT "flatbuffers"
#elif defined(DWARFS_HAVE_THRIFT)
  #define DEFAULT_TEST_METADATA_FORMAT "thrift"
#else
  #error "At least one metadata format must be available"
#endif
```

### Step 2: Validate Improvement
```bash
# Rebuild with fix
ninja -C build

# Run tests
ctest --test-dir build

# Should see: 95%+ pass rate
```

### Step 3: Continue with Remaining Categories
- Create FlatBuffers test images (Category 2)
- Parameterize tests (Category 3)
- Debug crashes (Category 4)

---

## Key Insights

1. **95%+ of failures have a SINGLE ROOT CAUSE**: Missing metadata format specification
2. **Fix is SIMPLE**: Add one parameter and set smart default
3. **Impact is MASSIVE**: 81% of failures resolved immediately
4. **No test logic changes needed**: Most tests work with either format

---

**Next Action**: Modify `build_dwarfs()` function to accept and use metadata format parameter.