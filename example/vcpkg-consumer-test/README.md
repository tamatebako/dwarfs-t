# DwarFS Header Installation Test

**Purpose**: Verify that DwarFS installation provides ALL headers required by consumer projects.

**Created**: 2025-12-30
**Context**: Response to libtfs vcpkg header installation issues

---

## What This Test Does

This test ensures that the DwarFS build system correctly installs **all** headers that consumer projects need, including:

1. ✅ Public API headers (`dwarfs/reader/*.h`, `dwarfs/utility/*.h`)
2. ✅ Internal headers required by public headers (`dwarfs/internal/*.h`)
3. ✅ Detail headers (`dwarfs/detail/*.h`)
4. ✅ Top-level utility headers (`dwarfs/logger.h`, etc.)

## Critical Headers Tested

These headers were **missing** in the original installation and caused libtfs compilation failures:

- `dwarfs/logger.h`
- `dwarfs/internal/packed_ptr.h`
- `dwarfs/internal/string_table.h`
- `dwarfs/detail/file_view_impl.h`
- `dwarfs/detail/file_segment_impl.h`

## How to Run Locally

### Quick Automated Tests

We provide two automated test scripts that verify the header test works correctly:

#### 1. Test That Missing Headers Are Detected (EXPECT-FAIL)

```bash
cd example/vcpkg-consumer-test
./test-expect-fail.sh
```

**What it does**:
- Temporarily breaks header installation (simulates the original bug)
- Builds DwarFS with broken installation
- Verifies critical headers are MISSING
- Attempts to build header test (expects FAILURE)
- Restores original configuration

**Expected output**:
```
==================================================
✅ EXPECT-FAIL TEST PASSED
==================================================

Summary:
  - Built DwarFS with broken header installation
  - Verified critical headers were missing
  - Header test correctly FAILED to build

This proves the header test can detect missing headers!
```

#### 2. Test That Fix Works Correctly (EXPECT-PASS)

```bash
cd example/vcpkg-consumer-test
./test-expect-pass.sh
```

**What it does**:
- Builds DwarFS with FIXED header installation
- Verifies all critical headers are PRESENT
- Builds header test (expects SUCCESS)
- Runs test executable

**Expected output**:
```
==================================================
✅ EXPECT-PASS TEST PASSED
==================================================

Summary:
  - Built DwarFS with fixed header installation
  - Verified all critical headers were present
  - Header test built successfully
  - Test executable ran successfully

This proves the header installation fix works correctly!
```

### Manual Test

#### Prerequisites

1. Build and install DwarFS:
   ```bash
   cd /path/to/dwarfs
   mkdir build && cd build
   cmake .. -DCMAKE_INSTALL_PREFIX=/tmp/dwarfs-install
   cmake --build .
   cmake --install .
   ```

#### Run Test

```bash
cd example/vcpkg-consumer-test

# Configure
cmake -B build \
  -DCMAKE_PREFIX_PATH=/tmp/dwarfs-install

# Build (will FAIL if headers are missing)
cmake --build build

# Run
./build/header_test
```

#### Expected Output

```
DwarFS Header Installation Test
================================

✅ PASSED: All critical headers compiled successfully

Verified headers:
  ✅ dwarfs/logger.h
  ✅ dwarfs/internal/packed_ptr.h
  ✅ dwarfs/internal/string_table.h
  ✅ dwarfs/detail/file_view_impl.h
  ✅ dwarfs/detail/file_segment_impl.h
  ✅ dwarfs/reader/metadata_types.h
  ✅ dwarfs/reader/filesystem_v2.h
  ✅ dwarfs/reader/filesystem_loader.h
  ✅ dwarfs/utility/filesystem_extractor.h
  ✅ + 5 additional common headers

This test ensures that the DwarFS installation provides
all headers required by consumer projects like libtfs.
```

## CI Integration

This test is integrated into the GitHub Actions CI workflow to automatically verify header installation on every build.

See: `.github/workflows/installed-headers-test.yml`

## Troubleshooting

### Build Fails with "file not found"

If the test fails to compile with errors like:
```
fatal error: 'dwarfs/internal/packed_ptr.h' file not found
```

This means the header installation is incomplete. Check:

1. ✅ [`cmake/libdwarfs.cmake`](../../cmake/libdwarfs.cmake) line 543-569
2. ✅ Installation rules use `FILES_MATCHING PATTERN "*.h"`
3. ✅ Only `writer/internal` should be excluded

### Test Links but Doesn't Run

If compilation succeeds but linking fails, check that:

1. ✅ DwarFS libraries are installed
2. ✅ CMake can find the DwarFS package
3. ✅ `CMAKE_PREFIX_PATH` points to installation directory

## For Maintainers

### When to Update This Test

Add new headers to `header_test.cpp` when:

1. A new public API header is added
2. An internal header becomes required by a public header
3. A consumer project reports missing headers

### Test Philosophy

This test follows the **"Include Everything a Consumer Might Need"** philosophy:

- If a public header includes it, we test it
- If a consumer might transitively depend on it, we test it
- Better to test too much than too little

## Related Issues

- **libtfs vcpkg integration** (2025-12-30): Missing headers blocking compilation
- **Original bug report**: Headers excluded by pattern-based installation

## See Also

- [Main example](../example.cpp) - Basic DwarFS usage
- [Header installation fix](../../cmake/libdwarfs.cmake) - Implementation
- [CI workflow](.github/workflows/installed-headers-test.yml) - Automated testing