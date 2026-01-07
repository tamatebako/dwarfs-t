# Session 7 Continuation Prompt - OOP Test Architecture

**Read First**: [`TEST_SUITE_OOP_ARCHITECTURE_PLAN.md`](TEST_SUITE_OOP_ARCHITECTURE_PLAN.md)

---

## Context

Session 6 discovered that the test extraction approach was fundamentally flawed - tests were extracted procedurally instead of following **proper OOP principles**. The current `dwarfs_test.cpp` has a monolithic procedural function that violates:

- ❌ Single Responsibility Principle
- ❌ Open/Closed Principle
- ❌ Separation of Concerns
- ❌ Reusability

We need to **redesign the entire test architecture** using proper OOP with test fixtures.

---

## Objective

Create a **proper OOP test fixture hierarchy** following these principles:

1. **Single Responsibility**: Each test class tests ONE feature
2. **Inheritance**: Base fixtures provide common functionality
3. **Factory Pattern**: Fixtures create objects via factory methods
4. **Template Method**: SetUp/TearDown in base, extended by children
5. **CMake Native**: NO autotools, pure CMake build

---

## Architecture (MUST READ)

```
DwarfsTestFixture (abstract base - common setup)
    │
    ├── FilesystemTestFixture (filesystem operations)
    │   ├── FilesystemBasicTest (path finding, basic ops)
    │   ├── FilesystemUidGidTest (UID/GID handling)
    │   └── FilesystemAccessTest (permission checks)
    │
    ├── ScannerTestFixture (scanner operations)
    │   ├── ScannerParameterTest (parameter combinations)
    │   └── InodeOrderingTest (ordering algorithms)
    │
    └── MetadataTestFixture (metadata operations)
        ├── PackingTest (packing parameters)
        └── SerializationTest (serialization formats)
```

---

## Implementation Plan

### Phase 1: Create Base Fixture (2 hours)

**File**: `test/fixtures/dwarfs_test_fixture.h/cpp`

**Requirements**:
- Abstract base class inheriting from `::testing::Test`
- Template Method Pattern for SetUp/TearDown
- Factory Methods for creating objects:
  - `create_input()` → `shared_ptr<os_access_mock>`
  - `create_scanner_options()` → `writer::scanner_options`
  - `create_segmenter_config()` → `writer::segmenter::config`
- Builder Methods:
  - `build_filesystem(compression)` → `string` (filesystem image)
  - `create_file_view(fsimage)` → `file_view`
  - `create_reader(file_view)` → `reader::filesystem_v2`
- Common state:
  - `test_logger logger_`
  - `shared_ptr<os_access_mock> input_`
  - `writer::writer_progress progress_`

**Example Structure**:
```cpp
// test/fixtures/dwarfs_test_fixture.h
namespace dwarfs::test {

class DwarfsTestFixture : public ::testing::Test {
protected:
  void SetUp() override;
  void TearDown() override;

  virtual std::shared_ptr<os_access_mock> create_input();
  virtual writer::scanner_options create_scanner_options();

  std::string build_filesystem(std::string const& compression = "null");

  test_logger logger_;
  std::shared_ptr<os_access_mock> input_;
  writer::writer_progress progress_;
};

} // namespace dwarfs::test
```

**Action**: Create these files with proper OOP structure

### Phase 2: Create Filesystem Fixture (1 hour)

**File**: `test/fixtures/filesystem_test_fixture.h/cpp`

**Requirements**:
- Inherits from `DwarfsTestFixture`
- Overrides SetUp for filesystem-specific initialization
- Provides filesystem-specific helpers:
  - `verify_file(path, expected_content)`
  - `verify_uid_gid(path, uid, gid)`
  - `verify_access(path, mode, uid, gid)`
- Manages filesystem instance:
  - `unique_ptr<reader::filesystem_v2> filesystem_`

**Example**:
```cpp
// test/fixtures/filesystem_test_fixture.h
namespace dwarfs::test {

class FilesystemTestFixture : public DwarfsTestFixture {
protected:
  void SetUp() override;

  void verify_file(std::string const& path,
                   std::string const& expected);
  void verify_uid_gid(std::string const& path,
                      uint32_t uid, uint32_t gid);

  std::unique_ptr<reader::filesystem_v2> filesystem_;
};

} // namespace dwarfs::test
```

**Action**: Create filesystem-specific fixture

### Phase 3: Migrate One Test to OOP (1 hour)

**File**: `test/filesystem/filesystem_uid_gid_test.cpp`

**Requirements**:
- Test class inherits from `FilesystemTestFixture`
- Uses `TEST_F` (fixture-based) instead of `TEST`
- Uses fixture helpers instead of inline code
- Clear, focused tests with single responsibility

**Example**:
```cpp
// test/filesystem/filesystem_uid_gid_test.cpp
namespace dwarfs::test {

class FilesystemUidGidTest : public FilesystemTestFixture {
  // No additional setup needed - uses base fixture
};

TEST_F(FilesystemUidGidTest, handles_32_bit_uid_gid) {
  // Setup test data using fixture factory
  input_->add("foo16.txt", {2, 0100755, 1, 60000, 65535, 5, 42, 0, 0, 0}, "hello");
  input_->add("foo32.txt", {3, 0100755, 1, 65536, 4294967295, 5, 42, 0, 0, 0}, "world");

  // Build filesystem using fixture builder
  auto fsimage = build_filesystem();
  filesystem_ = std::make_unique<reader::filesystem_v2>(
      logger_, *input_, create_file_view(fsimage));

  // Verify using fixture helpers
  verify_uid_gid("/foo16.txt", 60000, 65535);
  verify_uid_gid("/foo32.txt", 65536, 4294967295);
}

} // namespace dwarfs::test
```

**Action**: Convert existing test to use OOP fixture

### Phase 4: Update CMake (30 min)

**File**: `cmake/tests.cmake`

**Requirements**:
- Create `dwarfs_test_fixtures` library target
- Link all test executables to fixtures
- Use `gtest_discover_tests()` for test discovery
- **CMake native** - NO autotools, NO configure scripts

**Example**:
```cmake
# Test fixtures library
add_library(dwarfs_test_fixtures
  test/fixtures/dwarfs_test_fixture.cpp
  test/fixtures/filesystem_test_fixture.cpp
)
target_link_libraries(dwarfs_test_fixtures
  PUBLIC dwarfs_test_helpers GTest::gtest
)

# Filesystem tests using fixtures
add_executable(dwarfs_filesystem_tests
  test/filesystem/filesystem_uid_gid_test.cpp
)
target_link_libraries(dwarfs_filesystem_tests
  PRIVATE dwarfs_test_fixtures GTest::gtest_main
)
gtest_discover_tests(dwarfs_filesystem_tests)
```

**Action**: Update CMake with fixture library

### Phase 5: Build & Verify (30 min)

**Commands**:
```bash
cd build-test
cmake .. -GNinja
ninja dwarfs_filesystem_tests
ctest -R filesystem --output-on-failure
```

**Verification**:
- Fixtures compile ✓
- Tests compile ✓
- Tests pass ✓
- No regressions ✓

**Action**: Build and verify the OOP architecture works

---

## Success Criteria

After Session 7, you must have:

- [x] `test/fixtures/dwarfs_test_fixture.h/cpp` created
- [x] `test/fixtures/filesystem_test_fixture.h/cpp` created
- [x] At least one test migrated to OOP (`FilesystemUidGidTest`)
- [x] CMake updated with fixtures library
- [x] All tests compile and pass
- [x] Clear OOP hierarchy established

---

## DO NOT

❌ Create procedural functions in anonymous namespaces
❌ Use autotools or configure scripts
❌ Extract tests without OOP fixtures
❌ Mix responsibilities in test classes
❌ Hardcode setup logic in tests

## DO

✅ Create proper OOP fixture hierarchy
✅ Use inheritance for shared behavior
✅ Apply Single Responsibility Principle
✅ Use Factory and Builder patterns
✅ Use CMake native build

---

## Files to Create

```
test/fixtures/
├── dwarfs_test_fixture.h       # Base fixture
├── dwarfs_test_fixture.cpp
├── filesystem_test_fixture.h   # Filesystem fixture
└── filesystem_test_fixture.cpp

test/filesystem/
└── filesystem_uid_gid_test.cpp # Migrated to OOP

cmake/tests.cmake               # Updated with fixtures
```

---

## Estimated Time

| Phase | Duration |
|-------|----------|
| Base fixture | 2h |
| Filesystem fixture | 1h |
| Migrate one test | 1h |
| Update CMake | 30min |
| Build & verify | 30min |
| **Total** | **5 hours** |

---

## Next Session After This

Once base architecture is proven:

1. Create `ScannerTestFixture`
2. Create `MetadataTestFixture`
3. Migrate all scanner tests
4. Migrate all metadata tests
5. Remove monolithic `basic_end_to_end_test()`

---

## Critical Reminders

1. **READ** [`TEST_SUITE_OOP_ARCHITECTURE_PLAN.md`](TEST_SUITE_OOP_ARCHITECTURE_PLAN.md) FIRST
2. **FOLLOW OOP PRINCIPLES** - No procedural code
3. **USE CMAKE ONLY** - No autotools
4. **ONE RESPONSIBILITY** per test class
5. **INHERITANCE** for code reuse
6. **BUILD AFTER EACH STEP** - Verify incrementally

---

**Start Here**: Create `test/fixtures/dwarfs_test_fixture.h` with OOP base structure
**Build System**: CMake (native)
**Architecture**: OOP with proper fixtures
**Principles**: SOLID, DRY, MECE