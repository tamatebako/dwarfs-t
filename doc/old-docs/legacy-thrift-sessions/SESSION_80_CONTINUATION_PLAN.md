# Session 80: Complete Frozen2 Implementation - Continuation Plan

**Created**: 2026-01-05
**Prerequisite**: Session 79 (modular architecture created, compilation errors remain)
**Goal**: Fix template errors, build successfully, add tests, achieve Homebrew compatibility
**Estimated Time**: 4-5 hours

---

## Mission

Complete the Frozen2 serialization implementation by:
1. Fixing all template deduction errors (lambda vs std::function)
2. Building successfully without errors
3. Porting and passing all tests from dwarfs-rs
4. Achieving byte-for-byte compatibility with Homebrew v0.14.1
5. Updating official documentation

---

## Current State (Session 79 Output)

### ✅ Completed
- **9 new files created** (~3,000 lines total)
- **Modular OOP architecture** (4 components, all <800 lines)
- **CMake integration** (all files added to build)

### ❌ Blocking Issues
- **Template deduction errors**: 40+ compilation errors
- **Root cause**: `std::function<>` cannot deduce from lambdas
- **Affected files**: All builder and serializer files

---

## Phase 1: Fix Template Deduction Errors (1.5 hours)

### Task 1.1: Update Layout Builder Templates (30 min)

**File**: `include/dwarfs/metadata/legacy/frozen2_layout_builder.h`

Change all template signatures from:
```cpp
template<typename T>
std::unique_ptr<Layout> build_optional(
    std::optional<T> const& opt,
    std::function<std::unique_ptr<Layout>(T const&)> builder);
```

To accept any callable:
```cpp
template<typename T, typename Builder>
std::unique_ptr<Layout> build_optional(
    std::optional<T> const& opt,
    Builder&& builder);
```

Apply to: `build_optional`, `build_vector`, `build_set`, `build_map`

### Task 1.2: Update Value Serializer Templates (30 min)

**File**: `include/dwarfs/metadata/legacy/frozen2_value_serializer.h`

Change all member function templates from:
```cpp
template<typename T>
void serialize_optional(
    std::optional<T> const& opt,
    std::function<void(Serializer&, T const&)> serializer);
```

To:
```cpp
template<typename T, typename Func>
void serialize_optional(
    std::optional<T> const& opt,
    Func&& serializer);
```

Apply to: `serialize_optional`, `serialize_vector`, `serialize_set`, `serialize_map`

### Task 1.3: Update StructSerializer Template (30 min)

**File**: `include/dwarfs/metadata/legacy/frozen2_value_serializer.h`

Change:
```cpp
template<typename T>
void StructSerializer::serialize_field(
    T const& value,
    std::function<void(Serializer&, T const&)> serializer);
```

To:
```cpp
template<typename T, typename Func>
void StructSerializer::serialize_field(
    T const& value,
    Func&& serializer);
```

Update implementation to use `std::forward<Func>(serializer)`.

---

## Phase 2: Fix Remaining Compilation Errors (1 hour)

### Task 2.1: Fix DenseMap Constructor (15 min)

**File**: `src/metadata/legacy/frozen2_serializer.cpp`

Change line 67:
```cpp
schema.layouts = DenseMap<SchemaLayout>(std::move(layout_vec));
```

To:
```cpp
std::vector<std::optional<SchemaLayout>> opt_vec;
for (auto& layout : layout_vec) {
  opt_vec.push_back(std::move(layout));
}
schema.layouts.raw_data() = std::move(opt_vec);
```

### Task 2.2: Fix Schema Root Access (15 min)

**File**: `src/metadata/legacy/frozen2_serializer.cpp`

Change lines 73-74:
```cpp
auto& root = schema.layouts.get(*root_id);
root.size = (root.bits + 7) / 8;
```

To:
```cpp
auto root_opt = schema.layouts.get(*root_id);
if (root_opt) {
  root_opt->size = (root_opt->bits + 7) / 8;
}
```

### Task 2.3: Verify Build (30 min)

Run:
```bash
ninja -C build-test dwarfs_metadata_legacy
```

Fix any remaining errors systematically.

---

## Phase 3: Port Tests from dwarfs-rs (1.5 hours)

### Task 3.1: Create Test File (30 min)

**File**: `test/metadata/legacy/frozen2_serializer_test.cpp` (NEW)

Port all 3 tests from `dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs:859-961`:

```cpp
#include <gtest/gtest.h>
#include "dwarfs/metadata/legacy/frozen2_serializer.h"
#include "dwarfs/metadata/domain/metadata.h"

namespace dwarfs::metadata::legacy::test {

// Port ser_frozen.rs:864-886
TEST(Frozen2SerializerTest, SmokeTest) {
  domain::metadata meta;
  auto opts = domain::fs_options{};
  opts.mtime_only = true;
  opts.time_resolution_sec = 42;
  meta.options = opts;

  auto [schema, out] = Frozen2Serializer::serialize(meta);

  // Verify exact byte output
  std::vector<uint8_t> expected = {
    1,              // options.is_some = true
    1,              // options.inner.mtime_only = true
    1,              // options.inner.time_resolution.is_some = true
    42, 0, 0, 0,    // options.inner.time_resolution.inner = 42
  };
  EXPECT_EQ(out, expected);
}

// Port ser_frozen.rs:888-915
TEST(Frozen2SerializerTest, BytesTest) {
  domain::metadata meta;
  meta.dwarfs_version = "abc";

  auto [schema, out] = Frozen2Serializer::serialize(meta);

  std::vector<uint8_t> expected = {
    1,          // dwarfs_version.is_some
    9, 0, 0, 0, // dwarfs_version.inner.distance
    3, 0, 0, 0, // dwarfs_version.inner.count
    // Outlined
    'a', 'b', 'c',
  };
  EXPECT_EQ(out, expected);

  // Test all-zeros case
  meta.dwarfs_version = "\0\0";
  auto [schema2, out2] = Frozen2Serializer::serialize(meta);

  std::vector<uint8_t> expected2 = {
    1,          // dwarfs_version.is_some
    2, 0, 0, 0, // dwarfs_version.inner.count
  };
  EXPECT_EQ(out2, expected2);
}

// Port ser_frozen.rs:917-960
TEST(Frozen2SerializerTest, CollectionTest) {
  domain::metadata meta;
  meta.chunks = {
    domain::chunk(0, 0, 42),
    domain::chunk(0, 100, 42),
  };
  meta.symlink_table = {0, 0, 0};

  auto [schema, out] = Frozen2Serializer::serialize(meta);

  std::vector<uint8_t> expected = {
    12, 0, 0, 0,    // chunks.distance = 12
    2, 0, 0, 0,     // chunks.count = 2
    3, 0, 0, 0,     // symlink_table.count = 3
    // Outlined
    0, 0, 0, 0,     // chunks[0].offset = 0
    42, 0, 0, 0,    // chunks[0].size = 42
    100, 0, 0, 0,   // chunks[1].offset = 100
    42, 0, 0, 0,    // chunks[1].size = 42
  };
  EXPECT_EQ(out, expected);
}

} // namespace
```

### Task 3.2: Add Test to CMake (15 min)

**File**: `cmake/metadata_serialization.cmake`

Add after line 283:
```cmake
# Frozen2 serializer tests
add_executable(frozen2_serializer_tests
  test/metadata/legacy/frozen2_serializer_test.cpp
)

target_link_libraries(frozen2_serializer_tests
  PRIVATE
    dwarfs_metadata_legacy
    GTest::gtest_main
    GTest::gmock
)

add_test(
  NAME frozen2_serializer_tests
  COMMAND frozen2_serializer_tests
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set_tests_properties(frozen2_serializer_tests
  PROPERTIES LABELS "legacy;metadata;frozen2;serializer"
)
```

### Task 3.3: Run and Fix Tests (45 min)

Build and run tests:
```bash
ninja -C build-test frozen2_serializer_tests
./build-test/frozen2_serializer_tests
```

Fix any test failures by comparing byte output with dwarfs-rs.

---

## Phase 4: Integration Testing (30 min)

### Task 4.1: Test with mkdwarfs (15 min)

Create test filesystem:
```bash
mkdir -p /tmp/test-frozen2
echo "test content" > /tmp/test-frozen2/file.txt

./build-test/mkdwarfs \
  -i /tmp/test-frozen2 \
  -o /tmp/test.dth \
  --metadata-format=legacy-thrift
```

### Task 4.2: Verify Homebrew Compatibility (15 min)

If Homebrew v0.14.1 is available:
```bash
# Mount with Homebrew dwarfs
dwarfs /tmp/test.dth /tmp/mnt

# Verify contents
ls -la /tmp/mnt
cat /tmp/mnt/file.txt

# Unmount
umount /tmp/mnt
```

---

## Phase 5: Documentation (30 min)

### Task 5.1: Update Memory Bank (15 min)

**File**: `.kilocode/rules/memory-bank/context.md`

Update current status to reflect Frozen2 completion.

### Task 5.2: Create Completion Summary (15 min)

**File**: `doc/SESSION_80_COMPLETION_SUMMARY.md`

Document:
- What was completed
- Files created/modified
- Test results
- Performance metrics
- Known issues (if any)

---

## Success Criteria

### Build ✓
- [ ] All files compile without errors
- [ ] All files compile without warnings
- [ ] Library links successfully

### Tests ✓
- [ ] smoke test PASSES
- [ ] bytes test PASSES
- [ ] collection test PASSES
- [ ] Byte output matches dwarfs-rs exactly

### Integration ✓
- [ ] mkdwarfs creates legacy Thrift image
- [ ] Homebrew v0.14.1 can mount and read
- [ ] File extraction works correctly
- [ ] Checksums match

### Documentation ✓
- [ ] Memory bank updated
- [ ] Completion summary created
- [ ] Session 77-79 docs moved to old-docs/

---

## Time Budget

| Phase | Tasks | Estimate | Cumulative |
|-------|-------|----------|------------|
| 1 | Fix templates | 1.5h | 1.5h |
| 2 | Fix build errors | 1h | 2.5h |
| 3 | Port tests | 1.5h | 4h |
| 4 | Integration | 0.5h | 4.5h |
| 5 | Documentation | 0.5h | 5h |

**Total**: 5 hours for complete Frozen2 implementation

---

## Common Pitfalls to Avoid

- ❌ Don't use `std::function<>` in template parameters
- ❌ Don't forget `std::forward<>` for perfect forwarding
- ❌ Don't skip byte-for-byte test verification
- ❌ Don't forget to test Homebrew compatibility
- ✅ Use `auto&&` or template parameters for callables
- ✅ Use `std::invoke` if needed for generic callables
- ✅ Verify every byte in test output

---

**Created**: 2026-01-05
**Status**: Ready to start
**Next**: Begin Phase 1 - Fix template deduction errors