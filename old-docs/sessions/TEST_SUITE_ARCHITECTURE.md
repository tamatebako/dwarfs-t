# DwarFS Test Suite Architecture

**Created**: 2025-12-14
**Status**: Design Phase
**Purpose**: Refactor monolithic `dwarfs_test.cpp` into modular, maintainable test suite

---

## Current State Analysis

**Monolithic File**: `test/dwarfs_test.cpp` (2,237 lines)

**Problems**:
- Single massive file violates **Single Responsibility Principle**
- Difficult to maintain and extend
- Not MECE (overlapping concerns mixed together)
- Hard to use cached fixtures consistently
- Poor separation of concerns

**Test Categories Identified**:
1. **Compression Tests** (~200 lines) - Algorithm and compression regression
2. **Scanner Tests** (~300 lines) - File scanning, hashing, ordering
3. **Metadata/Packing Tests** (~150 lines) - Metadata packing options
4. **Filesystem Tests** (~800 lines) - Core filesystem operations
5. **Filter Tests** (~400 lines) - Entry filtering and rules
6. **Segmenter Tests** (~100 lines) - Block segmentation

---

## Target Architecture (MECE Structure)

```
test/
├── test_helpers.h              # Shared test utilities
├── test_fixtures.h             # Test fixture caching (✓ DONE)
├── test_fixtures.cpp           # Test fixture impl (✓ DONE)
│
├── compression/
│   ├── compression_test.cpp    # Algorithm tests
│   └── compression_regression_test.cpp
│
├── scanner/
│   ├── scanner_options_test.cpp
│   ├── file_hashing_test.cpp
│   └── inode_ordering_test.cpp
│
├── metadata/
│   ├── packing_test.cpp
│   └── plain_tables_test.cpp
│
├── filesystem/
│   ├── filesystem_basic_test.cpp      # Basic FS ops
│   ├── filesystem_uid_gid_test.cpp    # UID/GID handling
│   ├── filesystem_access_test.cpp     # Access control
│   ├── filesystem_read_test.cpp       # Read operations
│   ├── filesystem_lookup_test.cpp     # Path lookup
│   └── filesystem_multi_image_test.cpp
│
├── filter/
│   └── filter_test.cpp
│
└── segmenter/
    └── segmenter_test.cpp
```

### Architectural Principles Applied

1. **Single Responsibility**: Each test file focuses on ONE component
2. **MECE**: No overlap between test files, complete coverage
3. **Open/Closed**: Easy to add new test files without modifying existing
4. **Separation of Concerns**: Test categories cleanly separated
5. **DRY**: Shared utilities in test_helpers.h, fixtures in test_fixtures

---

## Module Breakdown

### 1. Compression Module (`test/compression/`)

**compression_test.cpp**:
- Test classes: `compression_test`
- Focus: Algorithm correctness across compressions
- Lines: ~150
- Uses fixtures: `basic_test_data`

**compression_regression_test.cpp**:
- Test classes: `compression_regression`
- Focus: Specific bug regressions (github45, etc.)
- Lines: ~100
- Uses fixtures: Dynamic (random data)

### 2. Scanner Module (`test/scanner/`)

**scanner_options_test.cpp**:
- Test classes: `scanner_test`
- Focus: Scanner options (devices, specials, uid/gid)
- Lines: ~150
- Uses fixtures: `basic_test_data`

**file_hashing_test.cpp**:
- Test classes: `hashing_test`
- Focus: File hashing algorithms
- Lines: ~50
- Uses fixtures: `basic_test_data`

**inode_ordering_test.cpp**:
- Test classes: `file_scanner`
- Focus: Inode ordering modes (path, similarity, nilsimsa)
- Lines: ~150
- Uses fixtures: Dynamic (large trees)

### 3. Metadata Module (`test/metadata/`)

**packing_test.cpp**:
- Test classes: `packing_test`
- Focus: Metadata packing options
- Lines: ~100
- Uses fixtures: `basic_test_data`, `empty_fs`

**plain_tables_test.cpp**:
- Test classes: `plain_tables_test`
- Focus: Plain vs packed string tables
- Lines: ~50
- Uses fixtures: `basic_test_data`

### 4. Filesystem Module (`test/filesystem/`)

**filesystem_basic_test.cpp**:
- Test functions: `find_by_path`, `root_access_github204`
- Focus: Basic filesystem operations
- Lines: ~150
- Uses fixtures: `basic_test_data`

**filesystem_uid_gid_test.cpp**:
- Test functions: `uid_gid_32bit`, `uid_gid_count`, `uid_gid_override`
- Focus: UID/GID handling (16-bit, 32-bit, overrides)
- Lines: ~150
- Uses fixtures: `uid_gid_test`

**filesystem_access_test.cpp**:
- Test functions: `root_access_github204`
- Focus: Access control (permissions, root access)
- Lines: ~200
- Uses fixtures: Custom

**filesystem_read_test.cpp**:
- Test functions: `read`, `inode_size_cache`
- Focus: File reading (read, readv, read_string)
- Lines: ~400
- Uses fixtures: Dynamic (random data)

**filesystem_lookup_test.cpp**:
- Test functions: `case_insensitive_lookup`
- Focus: Path lookup (case sensitivity)
- Lines: ~200
- Uses fixtures: Dynamic (unicode)

**filesystem_multi_image_test.cpp**:
- Test functions: `multi_image`
- Focus: Multiple images in single file
- Lines: ~100
- Uses fixtures: Dynamic

### 5. Filter Module (`test/filter/`)

**filter_test.cpp**:
- Test classes: `filter_test`
- Focus: Entry filtering with rules
- Lines: ~400
- Uses fixtures: Custom test tree

### 6. Segmenter Module (`test/segmenter/`)

**segmenter_test.cpp**:
- Test functions: `regression_block_boundary`, `github183`
- Focus: Block segmentation
- Lines: ~150
- Uses fixtures: `basic_test_data`, dynamic

---

## Migration Strategy

### Phase 1: Extract Common Utilities (1 hour)

1. Create `test/test_common.h` with shared test utilities:
   - `build_dwarfs()` helper
   - Common test data generators
   - Shared constants

### Phase 2: Extract Modules (One at a Time) (8 hours)

**Order of extraction** (low risk → high risk):

1. **Segmenter** (simplest, 2 tests) → 30 min
2. **Filter** (self-contained) → 1 hour
3. **Compression** (well-isolated) → 1 hour
4. **Scanner** (moderate complexity) → 1.5 hours
5. **Metadata/Packing** (small, clear) → 1 hour
6. **Filesystem** (largest, most complex) → 3 hours

### Phase 3: Update Build System (1 hour)

Update `cmake/tests.cmake`:
```cmake
# Modular test suite
add_executable(dwarfs_compression_tests
  test/compression/compression_test.cpp
  test/compression/compression_regression_test.cpp
)

add_executable(dwarfs_scanner_tests
  test/scanner/scanner_options_test.cpp
  test/scanner/file_hashing_test.cpp
  test/scanner/inode_ordering_test.cpp
)

# ... etc for each module
```

### Phase 4: Verification (1 hour)

1. Build all test targets
2. Run full test suite
3. Verify 100% pass rate
4. Performance benchmark

---

## Benefits

### Maintainability
- **Before**: 2,237-line file, hard to navigate
- **After**: 12 focused files, <400 lines each

### Testability
- Each module independently testable
- Easier to debug failures
- Clear test ownership

### Extensibility
- Add new filesystem test? → `test/filesystem/filesystem_new_feature_test.cpp`
- Add new compression? → `test/compression/new_compression_test.cpp`
- No need to modify existing files

### Performance
- Parallel compilation of test modules
- Faster incremental builds
- Selective test execution

---

## Implementation Checklist

### Phase 1: Common Utilities
- [ ] Create `test/test_common.h`
- [ ] Extract `build_dwarfs()` helper
- [ ] Extract common test constants

### Phase 2: Module Extraction
- [ ] Create `test/segmenter/segmenter_test.cpp`
- [ ] Create `test/filter/filter_test.cpp`
- [ ] Create `test/compression/` tests
- [ ] Create `test/scanner/` tests
- [ ] Create `test/metadata/` tests
- [ ] Create `test/filesystem/` tests (6 files)

### Phase 3: Build System
- [ ] Update `cmake/tests.cmake`
- [ ] Add test targets for each module
- [ ] Remove old `dwarfs_test.cpp` from build

### Phase 4: Verification
- [ ] Build all modules
- [ ] Run test suite
- [ ] Verify pass rate
- [ ] Update documentation

---

## Success Criteria

1. ✅ All tests pass (3,132 tests)
2. ✅ Build time reduced (parallel compilation)
3. ✅ No test file >400 lines
4. ✅ Clear module boundaries (MECE)
5. ✅ Easy to add new tests
6. ✅ Cached fixtures used where appropriate

---

**Estimated Time**: 11 hours total
**Priority**: HIGH - Enables proper test fixture integration
**Status**: Ready to implement