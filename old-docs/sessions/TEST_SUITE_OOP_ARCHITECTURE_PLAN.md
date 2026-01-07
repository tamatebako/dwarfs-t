# DwarFS Test Suite OOP Architecture Plan

**Created**: 2025-12-14
**Purpose**: Redesign test suite with proper OOP hierarchy
**Build System**: CMake (native) - NO autotools

---

## Problem Statement

Current `dwarfs_test.cpp` has a **monolithic procedural function** in an anonymous namespace:

```cpp
namespace {  // WRONG - procedural, not OOP
  void basic_end_to_end_test(17 parameters...) {
    // 400+ lines of procedural code
    // Hard to extend, hard to test specific scenarios
  }
}
```

This violates:
- ❌ **Single Responsibility Principle** - Does everything
- ❌ **Open/Closed Principle** - Must modify to extend
- ❌ **Separation of Concerns** - Mixed setup/execution/verification
- ❌ **Testability** - Cannot test parts independently
- ❌ **Reusability** - Locked in anonymous namespace

---

## Correct OOP Architecture

### Test Fixture Hierarchy

```
DwarfsTestFixture (abstract base)
    │
    ├── FilesystemTestFixture
    │   ├── FilesystemBasicTest
    │   ├── FilesystemUidGidTest
    │   └── FilesystemAccessTest
    │
    ├── ScannerTestFixture
    │   ├── ScannerParameterTest
    │   ├── InodeOrderingTest
    │   └── InputListTest
    │
    ├── MetadataTestFixture
    │   ├── PackingTest
    │   ├── SerializationTest
    │   └── FormatConversionTest
    │
    └── CompressionTestFixture
        ├── CompressionAlgorithmTest
        └── CompressionRegressionTest
```

### Base Fixture (OOP Foundation)

```cpp
// test/fixtures/dwarfs_test_fixture.h
namespace dwarfs::test {

class DwarfsTestFixture : public ::testing::Test {
protected:
  // Setup/Teardown (Template Method Pattern)
  void SetUp() override;
  void TearDown() override;

  // Factory Methods (Factory Pattern)
  virtual std::shared_ptr<os_access_mock> create_input();
  virtual writer::scanner_options create_scanner_options();
  virtual writer::segmenter::config create_segmenter_config();

  // Builder Methods (Builder Pattern)
  std::string build_filesystem(
      std::string const& compression = "null");
  file_view create_file_view(std::string fsimage);
  reader::filesystem_v2 create_reader(file_view mm);

  // Common state
  test_logger logger_;
  std::shared_ptr<os_access_mock> input_;
  writer::writer_progress progress_;
};

} // namespace dwarfs::test
```

### Feature-Specific Fixtures (Inheritance)

```cpp
// test/fixtures/filesystem_test_fixture.h
namespace dwarfs::test {

class FilesystemTestFixture : public DwarfsTestFixture {
protected:
  // Filesystem-specific setup
  void SetUp() override;

  // Filesystem-specific helpers
  void verify_file(std::string const& path,
                   std::string const& expected_content);
  void verify_uid_gid(std::string const& path,
                      uint32_t expected_uid,
                      uint32_t expected_gid);

  // Filesystem-specific state
  std::unique_ptr<reader::filesystem_v2> filesystem_;
};

} // namespace dwarfs::test
```

### Concrete Test Classes (One per feature)

```cpp
// test/filesystem/filesystem_uid_gid_test.cpp
namespace dwarfs::test {

class FilesystemUidGidTest : public FilesystemTestFixture {
protected:
  void SetUp() override {
    FilesystemTestFixture::SetUp();
    // UID/GID-specific setup
  }
};

TEST_F(FilesystemUidGidTest, handles_32_bit_uid_gid) {
  input_->add("foo16.txt", {2, 0100755, 1, 60000, 65535, 5, 42, 0, 0, 0}, "hello");
  input_->add("foo32.txt", {3, 0100755, 1, 65536, 4294967295, 5, 42, 0, 0, 0}, "world");

  auto fsimage = build_filesystem();
  auto fs = create_reader(create_file_view(fsimage));

  verify_uid_gid("/foo16.txt", 60000, 65535);
  verify_uid_gid("/foo32.txt", 65536, 4294967295);
}

} // namespace dwarfs::test
```

---

## Directory Structure (OOP Hierarchy)

```
test/
├── fixtures/                           # Base fixtures (abstract)
│   ├── dwarfs_test_fixture.h          # Root base class
│   ├── dwarfs_test_fixture.cpp
│   ├── filesystem_test_fixture.h      # Filesystem base
│   ├── filesystem_test_fixture.cpp
│   ├── scanner_test_fixture.h         # Scanner base
│   ├── scanner_test_fixture.cpp
│   ├── metadata_test_fixture.h        # Metadata base
│   ├── metadata_test_fixture.cpp
│   └── compression_test_fixture.h     # Compression base
│
├── filesystem/                         # Filesystem tests (concrete)
│   ├── filesystem_basic_test.cpp      # Basic operations
│   ├── filesystem_uid_gid_test.cpp    # UID/GID handling
│   ├── filesystem_access_test.cpp     # Access permissions
│   ├── filesystem_find_test.cpp       # Path finding
│   └── filesystem_read_test.cpp       # Read operations
│
├── scanner/                            # Scanner tests (concrete)
│   ├── scanner_parameter_test.cpp     # Parameter combinations
│   ├── inode_ordering_test.cpp        # Ordering algorithms
│   ├── input_list_test.cpp            # Input list handling
│   └── file_hash_test.cpp             # Hash algorithms
│
├── metadata/                           # Metadata tests (concrete)
│   ├── packing_test.cpp               # Packing parameters
│   ├── serialization_test.cpp         # Serialization formats
│   ├── format_conversion_test.cpp     # Format conversion
│   └── converters/                    # Format-specific
│       ├── thrift_converter_test.cpp
│       └── flatbuffers_converter_test.cpp
│
├── compression/                        # Compression tests (concrete)
│   ├── compression_algorithm_test.cpp # Algorithm testing
│   └── compression_regression_test.cpp # Regression testing
│
├── unit/                              # Unit tests (no fixtures)
│   ├── checksum_test.cpp
│   ├── string_test.cpp
│   └── ...
│
└── CMakeLists.txt or tests.cmake      # CMake build (NATIVE)
```

---

## CMake Structure (Native, No Autotools)

```cmake
# cmake/tests.cmake

# ============================================================================
# Test Fixtures Library (Shared OOP Base)
# ============================================================================

add_library(dwarfs_test_fixtures
  test/fixtures/dwarfs_test_fixture.cpp
  test/fixtures/filesystem_test_fixture.cpp
  test/fixtures/scanner_test_fixture.cpp
  test/fixtures/metadata_test_fixture.cpp
  test/fixtures/compression_test_fixture.cpp
)

target_link_libraries(dwarfs_test_fixtures
  PUBLIC
    dwarfs_test_helpers
    GTest::gtest
    dwarfs_reader
    dwarfs_writer
)

# ============================================================================
# Modular Test Executables (OOP-Based)
# ============================================================================

# Filesystem tests (inherits from FilesystemTestFixture)
add_executable(dwarfs_filesystem_tests
  test/filesystem/filesystem_basic_test.cpp
  test/filesystem/filesystem_uid_gid_test.cpp
  test/filesystem/filesystem_access_test.cpp
  test/filesystem/filesystem_find_test.cpp
  test/filesystem/filesystem_read_test.cpp
)
target_link_libraries(dwarfs_filesystem_tests
  PRIVATE dwarfs_test_fixtures GTest::gtest_main
)
gtest_discover_tests(dwarfs_filesystem_tests)

# Scanner tests (inherits from ScannerTestFixture)
add_executable(dwarfs_scanner_tests
  test/scanner/scanner_parameter_test.cpp
  test/scanner/inode_ordering_test.cpp
  test/scanner/input_list_test.cpp
  test/scanner/file_hash_test.cpp
)
target_link_libraries(dwarfs_scanner_tests
  PRIVATE dwarfs_test_fixtures GTest::gtest_main
)
gtest_discover_tests(dwarfs_scanner_tests)

# Metadata tests (inherits from MetadataTestFixture)
add_executable(dwarfs_metadata_tests
  test/metadata/packing_test.cpp
  test/metadata/serialization_test.cpp
  test/metadata/format_conversion_test.cpp
)
target_link_libraries(dwarfs_metadata_tests
  PRIVATE dwarfs_test_fixtures GTest::gtest_main
)
gtest_discover_tests(dwarfs_metadata_tests)

# Compression tests (inherits from CompressionTestFixture)
add_executable(dwarfs_compression_tests
  test/compression/compression_algorithm_test.cpp
  test/compression/compression_regression_test.cpp
)
target_link_libraries(dwarfs_compression_tests
  PRIVATE dwarfs_test_fixtures GTest::gtest_main
)
gtest_discover_tests(dwarfs_compression_tests)

# Unit tests (no fixtures, standalone)
add_executable(dwarfs_unit_tests
  test/unit/checksum_test.cpp
  test/unit/string_test.cpp
  # ... all unit tests
)
target_link_libraries(dwarfs_unit_tests
  PRIVATE dwarfs_test_helpers GTest::gtest_main
)
gtest_discover_tests(dwarfs_unit_tests)
```

---

## Migration Strategy

### Phase 1: Create Base Fixtures (2-3 hours)

1. **Create `test/fixtures/dwarfs_test_fixture.h/cpp`**:
   - Extract common setup from anonymous namespace
   - Use Template Method Pattern for setup/teardown
   - Provide Factory Methods for creating objects

2. **Create feature-specific fixtures**:
   - `FilesystemTestFixture` - Filesystem operations
   - `ScannerTestFixture` - Scanner operations
   - `MetadataTestFixture` - Metadata operations
   - `CompressionTestFixture` - Compression operations

3. **Update CMake**:
   - Add `dwarfs_test_fixtures` library target
   - Link all test executables to fixtures

### Phase 2: Migrate Tests to Fixtures (4-6 hours)

1. **Filesystem tests** (easiest - already extracted):
   - Create `FilesystemUidGidTest : FilesystemTestFixture`
   - Create `FilesystemBasicTest : FilesystemTestFixture`
   - Create `FilesystemAccessTest : FilesystemTestFixture`

2. **Scanner tests**:
   - Create `ScannerParameterTest : ScannerTestFixture`
   - Create `InodeOrderingTest : ScannerTestFixture`
   - Create `InputListTest : ScannerTestFixture`

3. **Metadata tests**:
   - Create `PackingTest : MetadataTestFixture`
   - Create `SerializationTest : MetadataTestFixture`

4. **Compression tests**:
   - Create `CompressionAlgorithmTest : CompressionTestFixture`
   - Create `CompressionRegressionTest : CompressionTestFixture`

### Phase 3: Remove Monolithic Tests (1-2 hours)

1. Verify all tests migrated
2. Remove `basic_end_to_end_test()` from anonymous namespace
3. Remove old parameterized tests
4. Clean up `dwarfs_test.cpp`

### Phase 4: Verification (1-2 hours)

1. Build all test suites
2. Run all tests
3. Verify coverage matches old suite
4. Fix any regressions

**Total Time**: 8-13 hours

---

## Benefits of OOP Architecture

### ✅ Single Responsibility
Each test class tests ONE feature:
- `FilesystemUidGidTest` → UID/GID handling
- `InodeOrderingTest` → Inode ordering
- `PackingTest` → Metadata packing

### ✅ Open/Closed Principle
Add new tests by **extending**, not modifying:
```cpp
class FilesystemSymlinkTest : public FilesystemTestFixture {
  // New test - no changes to base
};
```

### ✅ Dependency Inversion
Tests depend on **abstractions** (fixtures), not concrete implementations

### ✅ Reusability
Fixtur helpers reused across all tests:
```cpp
auto fs = create_reader(create_file_view(build_filesystem()));
verify_uid_gid("/path", uid, gid);  // Reusable helper
```

### ✅ Testability
Each layer independently testable:
- Unit test fixtures
- Unit test helpers
- Integration test full scenarios

### ✅ Clarity
Clear hierarchy shows relationships:
```
FilesystemUidGidTest inherits from FilesystemTestFixture
→ UID/GID testing is a type of filesystem testing
```

---

## Comparison: Procedural vs OOP

### ❌ Current (Procedural)

```cpp
namespace {
  void basic_end_to_end_test(bool with_devices, bool with_specials, ...) {
    // 400 lines of setup + execution + verification
    // Can't reuse parts
    // Can't extend without modifying
  }
}

TEST_P(scanner_test, end_to_end) {
  basic_end_to_end_test(GetParam()...);  // Black box
}
```

**Problems**:
- Mixed responsibilities
- Not extensible
- Not reusable
- Hard to test parts
- Locked in anonymous namespace

### ✅ Proposed (OOP)

```cpp
class FilesystemTestFixture : public DwarfsTestFixture {
protected:
  auto create_filesystem();  // Factory
  void verify_file(...);     // Single responsibility helper
};

class FilesystemUidGidTest : public FilesystemTestFixture {
  TEST_F(handles_32_bit) {
    // Clear, focused test
    // Reuses fixture helpers
    // Extensible via inheritance
  }
};
```

**Benefits**:
- Clear responsibilities
- Extensible via inheritance
- Reusable helpers
- Testable layers
- Proper namespace

---

## Next Steps

### Immediate (Session 7 - Option C: OOP Refactoring)

1. **Read this plan** ✋ STOP and understand OOP approach
2. **Create base fixture** (`DwarfsTestFixture`)
3. **Create one feature fixture** (`FilesystemTestFixture`)
4. **Migrate one test** (`FilesystemUidGidTest`)
5. **Verify** builds and passes
6. **Repeat** for other features

### Long Term (v0.17.0)

1. Complete all fixture hierarchies
2. Migrate all tests
3. Remove monolithic `basic_end_to_end_test()`
4. Document OOP test patterns

---

## Success Criteria

- [x] Tests organized by **feature**, not parameters
- [x] Each test class has **single responsibility**
- [x] **Inheritance hierarchy** reflects feature relationships
- [x] **Fixtures provide reusable** helpers
- [x] Tests are **independently extensible**
- [x] **Clear separation** of setup/execution/verification
- [x] **No anonymous namespace** dependencies
- [x] **CMake-only** build (NO autotools)

---

**Document Status**: OOP Architecture Plan
**Build System**: CMake (native, NO autotools)
**Architecture**: Proper OOP with fixtures
**Next Action**: Implement Phase 1 (base fixtures)