# Next Session Prompt - Test Conversion (Session 3)

**Date**: 2025-12-14
**Session**: 3 of 5
**Estimated Time**: 8 hours (compressed to 4h)
**Focus**: Convert tests to use cached fixtures

---

## Session Context

You are continuing the DwarFS Thrift Optional Refactoring project. The test fixture caching infrastructure is COMPLETE and ready to use. Your task is to convert existing tests to use the caching system.

**What's Ready**:
- ✅ `CachedTestFixtures` singleton class implemented
- ✅ 4 standard fixtures auto-registered
- ✅ Build system integration complete
- ✅ Clean compilation verified

**What's Next**:
Convert ~200 tests in [`test/dwarfs_test.cpp`](../test/dwarfs_test.cpp) to use cached fixtures instead of dynamic image creation.

---

## Quick Start

### 1. Read Memory Bank
```
Memory bank location: .kilocode/rules/memory-bank/
```

**MUST READ**:
- `context.md` - Current project state
- `architecture.md` - Test infrastructure architecture
- All other memory bank files

### 2. Review Status Documents

**Read in order**:
1. [`doc/THRIFT_OPTIONAL_FULL_CONTINUATION_PLAN.md`](THRIFT_OPTIONAL_FULL_CONTINUATION_PLAN.md) - Complete roadmap
2. [`doc/THRIFT_OPTIONAL_IMPLEMENTATION_STATUS.md`](THRIFT_OPTIONAL_IMPLEMENTATION_STATUS.md) - Progress tracker
3. [`doc/THRIFT_OPTIONAL_TEST_CACHING_STATUS.md`](THRIFT_OPTIONAL_TEST_CACHING_STATUS.md) - Infrastructure details

### 3. Verify Build
```bash
cd /Users/mulgogi/src/external/dwarfs
cmake --build build
# Should complete successfully
```

---

## Session 3 Tasks

### Task 1: Convert dwarfs_test.cpp Tests (4 hours)

**File**: [`test/dwarfs_test.cpp`](../test/dwarfs_test.cpp)
**Lines**: 2,237 total

**Conversion Pattern**:

```cpp
// BEFORE:
TEST(filesystem, read) {
  test::test_logger lgr;
  std::independent_bits_engine<std::mt19937_64,
                               std::numeric_limits<uint8_t>::digits, uint16_t>
      rng;
  std::string contents;
  contents.resize(76543);
  std::generate(begin(contents), end(contents), std::ref(rng));

  auto input = std::make_shared<test::os_access_mock>();
  input->add_dir("");
  input->add_file("random", contents);

  auto fsimage = build_dwarfs(lgr, input, "null", {.block_size_bits = 8});
  auto mm = test::make_mock_file_view(std::move(fsimage));

  reader::filesystem_v2 fs(lgr, *input, mm,
                           {.inode_reader = {.readahead = 64}});
  // ... test logic ...
}

// AFTER (if custom data needed, keep dynamic):
TEST(filesystem, read) {
  test::test_logger lgr;
  std::independent_bits_engine<std::mt19937_64,
                               std::numeric_limits<uint8_t>::digits, uint16_t>
      rng;
  std::string contents;
  contents.resize(76543);
  std::generate(begin(contents), end(contents), std::ref(rng));

  auto input = std::make_shared<test::os_access_mock>();
  input->add_dir("");
  input->add_file("random", contents);

  // STILL dynamic because random content varies per run
  auto fsimage = build_dwarfs(lgr, input, "null", {.block_size_bits = 8});
  auto mm = test::make_mock_file_view(std::move(fsimage));

  reader::filesystem_v2 fs(lgr, *input, mm,
                           {.inode_reader = {.readahead = 64}});
  // ... test logic ...
}

// AFTER (if standard data, use cache):
TEST(filesystem, uid_gid_32bit) {
  test::test_logger lgr;
  auto input = std::make_shared<test::os_access_mock>();

  input->add("", {1, 040755, 1, 0, 0, 10, 42, 0, 0, 0});
  input->add("foo16.txt", {2, 0100755, 1, 60000, 65535, 5, 42, 0, 0, 0},
             "hello");
  input->add("foo32.txt", {3, 0100755, 1, 65536, 4294967295, 5, 42, 0, 0, 0},
             "world");

  // USE CACHED FIXTURE
  auto& fixtures = test::CachedTestFixtures::instance();
  auto image_path = fixtures.get_image("uid_gid_test");
  auto mm = test::make_real_file_view(image_path);

  reader::filesystem_v2 fs(lgr, *input, mm);
  // ... test logic ...
}
```

**Decision Criteria**:
- **Use Caching**: Test uses standard, predictable data
- **Keep Dynamic**: Test uses random data, per-test configuration, or edge cases

**Tests to Convert** (14 identified):

| Test | Line | Fixture | Priority |
|------|------|---------|----------|
| `uid_gid_32bit` | 1154 | uid_gid_test | HIGH |
| `uid_gid_count` | 1186 | uid_gid_test | HIGH |
| `uid_gid_override` | 1225 | uid_gid_test | HIGH |
| `find_by_path` | 1374 | basic_test_data | HIGH |
| `root_access_github204` | 1428 | basic_test_data | MEDIUM |
| `regression_empty_fs` | 679 | empty_fs | HIGH |
| `file_start_hash` | 1395 | basic_test_data | MEDIUM |
| `input_list` | 1117 | basic_test_data | MEDIUM |
| `regression_block_boundary` | 761 | DYNAMIC | SKIP |
| `github183` | 1282 | basic_test_data | LOW |
| `read` | 1597 | DYNAMIC | SKIP |
| `inode_size_cache` | 1917 | DYNAMIC | SKIP |
| `multi_image` | 1989 | DYNAMIC | SKIP |
| `case_insensitive_lookup` | 2059 | DYNAMIC | SKIP |

**Strategy**:
1. Convert HIGH priority tests first (6 tests)
2. Convert MEDIUM priority tests (4 tests)
3. SKIP tests that require dynamic creation (4 tests)

**Expected Outcome**: 10 tests converted, 4 remain dynamic

### Task 2: Verify Conversions (1 hour)

**Steps**:
```bash
# Rebuild
cmake --build build

# Run converted tests
ctest --test-dir build -R "filesystem.(uid_gid|find_by_path)" --output-on-failure

# Run all tests
ctest --test-dir build --output-on-failure

# Verify pass rate
```

**Success Criteria**:
- All converted tests pass
- No behavior changes
- No new failures introduced

### Task 3: Create Image Script (1 hour)

**File**: `scripts/create_flatbuffers_test_images.sh`

**Content**:
```bash
#!/bin/bash
# Create FlatBuffers versions of existing test images

set -e

BUILD_DIR="${BUILD_DIR:-build}"
MKDWARFS="${BUILD_DIR}/mkdwarfs"
DWARFSEXTRACT="${BUILD_DIR}/dwarfsextract"
DWARFSCK="${BUILD_DIR}/dwarfsck"

# Verify tools exist
for tool in "$MKDWARFS" "$DWARFSEXTRACT" "$DWARFSCK"; do
  if [[ ! -x "$tool" ]]; then
    echo "Error: $tool not found or not executable"
    echo "Run: cmake --build $BUILD_DIR"
    exit 1
  fi
done

echo "==> Creating FlatBuffers test images..."

for img in test/*.dwarfs; do
  base=$(basename "$img" .dwarfs)
  fb_img="test/${base}.fb.dwarfs"

  # Skip if already exists
  if [[ -f "$fb_img" ]]; then
    echo "  - Skipping $base (already exists)"
    continue
  fi

  echo "  - Processing $base..."

  # Extract to temporary directory
  tmp_dir=$(mktemp -d)
  trap "rm -rf $tmp_dir" EXIT

  "$DWARFSEXTRACT" -i "$img" -o "$tmp_dir" --no-progress 2>/dev/null

  # Recreate with FlatBuffers
  "$MKDWARFS" \
    -i "$tmp_dir" \
    -o "$fb_img" \
    --metadata-format=flatbuffers \
    --no-progress \
    --log-level=error

  # Verify integrity
  if ! "$DWARFSCK" -i "$fb_img" --check-integrity >/dev/null 2>&1; then
    echo "  ✗ FAILED: $fb_img"
    rm -f "$fb_img"
    exit 1
  fi

  # Cleanup
  rm -rf "$tmp_dir"

  echo "  ✓ Created $fb_img"
done

echo "==> Done! Created $(ls test/*.fb.dwarfs 2>/dev/null | wc -l) images"
```

**Run**:
```bash
chmod +x scripts/create_flatbuffers_test_images.sh
./scripts/create_flatbuffers_test_images.sh
```

### Task 4: Update Status (30 minutes)

Update [`doc/THRIFT_OPTIONAL_IMPLEMENTATION_STATUS.md`](THRIFT_OPTIONAL_IMPLEMENTATION_STATUS.md) with:
- Session 3 completion
- Tests converted count
- Images created
- Any issues encountered

---

## Important Reminders

### Architecture Principles

1. **Object-Oriented**: Use classes, not functions where appropriate
2. **MECE**: Each fixture tests one thing, no overlap
3. **Separation of Concerns**: Generation ≠ Testing ≠ Caching
4. **Open/Closed**: Add fixtures without modifying core
5. **Single Responsibility**: Each component has one job

### Code Quality

- **No code guards**: Use architecture (strategy pattern, not #ifdef)
- **No shortcuts**: Tests must verify correct behavior
- **No threshold lowering**: 100% pass rate or fix the test
- **Clean builds**: Zero warnings tolerated

### Test Conversion Rules

**DO Convert** when:
- Test uses standard, predictable data
- Test reuses common patterns
- Test benefits from caching

**DON'T Convert** when:
- Test requires random/dynamic data
- Test has custom configuration per run
- Test verifies edge cases with specific setup

---

## Expected Outcome

### End of Session 3

**Delivered**:
- 10 tests converted to use caching
- 7 FlatBuffers test images created
- Image generation script
- Build still clean
- Tests still pass

**Time Spent**: 4 hours (compressed from 8h)

**Ready For**: Session 4 (Validation + Documentation)

---

## Troubleshooting

### If Build Fails

```bash
# Clean and rebuild
rm -rf build
cmake -B build -DUSE_JEMALLOC=OFF -DWITH_TESTS=ON
cmake --build build
```

### If Tests Fail After Conversion

1. **Check fixture matches test expectations**
2. **Verify input mock still needed** (some tests may not need it)
3. **Compare behavior**: Run test with dynamic vs cached
4. **Rollback if needed**: Keep dynamic creation

### If Can't Find Test Target

```bash
# List all test targets
cmake --build build --target help | grep test
```

---

## Files You'll Modify

### Primary
- `test/dwarfs_test.cpp` - Convert tests

### Create
- `scripts/create_flatbuffers_test_images.sh` - Image generator
- `test/*.fb.dwarfs` - 7 test images

### Update
- `doc/THRIFT_OPTIONAL_IMPLEMENTATION_STATUS.md` - Progress

---

## Success Criteria

### Must Achieve
- ✅ 10 tests converted successfully
- ✅ 7 FlatBuffers images created
- ✅ Build remains clean (0 errors)
- ✅ Test pass rate unchanged

### Nice to Have
- More than 10 tests converted
- Performance improvement measurable
- Additional fixtures identified

---

**Ready to Start**: Yes ✅
**Blockers**: None
**Estimated Duration**: 4 hours
**Confidence**: Very High