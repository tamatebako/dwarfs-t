# DwarFS Testing Guide

This document provides comprehensive information about testing DwarFS, including unit tests, compatibility tests, benchmarks, and CI/CD integration.

## Table of Contents

- [Overview](#overview)
- [Quick Start](#quick-start)
- [Test Suites](#test-suites)
  - [Unit Tests](#unit-tests)
  - [Compatibility Tests](#compatibility-tests)
  - [Benchmark Tests](#benchmark-tests)
- [Running Tests](#running-tests)
  - [Local Testing](#local-testing)
  - [CI/CD Testing](#cicd-testing)
- [Compatibility Test Framework](#compatibility-test-framework)
  - [Purpose](#purpose)
  - [Architecture](#architecture)
  - [Test Fixtures](#test-fixtures)
  - [Fixture Cache](#fixture-cache)
  - [Homebrew Detection](#homebrew-detection)
- [Platform Matrix Testing](#platform-matrix-testing)
- [Writing Tests](#writing-tests)
- [Troubleshooting](#troubleshooting)

## Overview

DwarFS uses a multi-tier testing approach:

1. **Unit Tests**: Fast, isolated tests for individual components
2. **Integration Tests**: Tests that verify interactions between components
3. **Compatibility Tests**: Cross-version and cross-platform compatibility verification
4. **Benchmark Tests**: Performance regression testing

## Quick Start

### Unified Build System Testing (Recommended)

The easiest way to test DwarFS is using the **unified build system** with one-step scripts:

```bash
# From the dwarfs source root
./scripts/one-step/test-everything.sh              # Auto-detect vcpkg or system packages
./scripts/one-step/test-everything.sh --vcpkg      # Force vcpkg mode
./scripts/one-step/test-everything.sh --quick      # Quick test (skip benchmarks)
```

**What it does:**
1. ✅ Detects build environment (vcpkg vs system packages)
2. ✅ Verifies Tebako jemalloc overlay port
3. ✅ Tests FlatBuffers-only configuration
4. ✅ Tests Both-formats configuration (FlatBuffers + Thrift)
5. ✅ Runs all unit tests
6. ⏭️  Optionally runs benchmarks (prompts first)

**For backward compatibility, the old paths still work:**
```bash
./scripts/test-everything.sh      # → redirects to scripts/one-step/test-everything.sh
```

### Traditional Testing

```bash
# From the dwarfs source root
cd build

# Run all unit tests
ctest

# Run specific test suite
ctest -R metadata
ctest -R compat

# Run with verbose output
ctest --verbose

# Run a specific test executable
./test/compat/read_homebrew_files_test
```

## Developer Workflow

### Prerequisites

**CRITICAL**: DwarFS requires **Tebako's fork of jemalloc 5.5.0**, NOT the upstream jemalloc!

| Build Mode | jemalloc Source | How to Get It |
|------------|----------------|--------------|
| **vcpkg (Recommended)** | Tebako 5.5.0 | Auto-installed via vcpkg overlay port |
| **System Packages** | ⚠️ Incompatible | Homebrew has 5.3.0 (won't work) |

**Recommended**: Use vcpkg mode for all builds.

### One-Step Process for Developers

```bash
# Clone repository (with submodules)
git clone --recurse-submodules https://github.com/tamatebako/dwarfs.git
cd dwarfs

# Install vcpkg (one-time setup)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# Run all tests (one command!)
./scripts/test-everything.sh
```

That's it! The script will:
- ✅ Detect vcpkg automatically
- ✅ Build both configurations
- ✅ Run all tests
- ✅ Show clear pass/fail results

### One-Step Process for Release Managers

```bash
# Full CI test suite (simulates GitHub Actions)
./scripts/build-all-and-test.sh --vcpkg

# Test specific configurations
./scripts/test-all-configs.sh --vcpkg

# Quick validation before release
./scripts/test-everything.sh --quick
```

### Available Test Scripts

| Script | Purpose | Usage |
|--------|---------|-------|
| `test-everything.sh` | **One-stop testing** | `./scripts/test-everything.sh` |
| `build-all-and-test.sh` | Full CI simulation | `./scripts/build-all-and-test.sh --vcpkg` |
| `test-all-configs.sh` | Configuration matrix | `./scripts/test-all-configs.sh --vcpkg` |
| `clean-build.sh` | Clean rebuild | `BUILD_DIR=build ./scripts/clean-build.sh -y` |
| `benchmark-all.sh` | Performance tests | `./scripts/benchmark-all.sh` |

## Test Suites

### Unit Tests

Unit tests are located in `test/` and use Google Test framework. They test individual components in isolation.

**Key unit test suites:**
- `metadata_test`: Metadata serialization and deserialization
- `utils_test`: Utility functions
- `compression_test`: Compression algorithms

### Compatibility Tests

Compatibility tests verify that DwarFS can read and write files compatible with other implementations (Homebrew dwarfs).

**Location:** `test/compat/`

**Test executables:**
- `read_homebrew_files_test`: Tests reading Homebrew-generated DFT files
- `write_compatible_files_test`: Tests writing files compatible with Homebrew
- `round_trip_test`: Tests write-read cycles

**Purpose:** Ensure Tebako DwarFS can interoperate with the upstream Homebrew dwarfs tool.

### Benchmark Tests

Benchmark tests measure performance and detect regressions.

**Location:** `benchmarks/`

**Key benchmark scripts:**
- `run_comprehensive_benchmark.sh`: Comprehensive performance testing
- `benchmark-all.sh`: Run all benchmarks

## Running Tests

### Local Testing

#### Build with Tests

```bash
# Using the clean-build script (RECOMMENDED)
BUILD_DIR=build ./scripts/clean-build.sh -y
cd build
ninja
```

#### Run All Tests

```bash
# From build directory
ninja test

# Or using ctest
ctest

# With verbose output
ctest --verbose
```

#### Run Specific Tests

```bash
# Run compatibility tests
./test/compat/read_homebrew_files_test
./test/compat/write_compatible_files_test
./test/compat/round_trip_test

# Run specific test with filter
./test/compat/read_homebrew_files_test --gtest_filter="ReadHomebrewFilesTest.ReadBasicFixture"

# Run metadata tests
ctest -R metadata

# Run with output
./test/compat/read_homebrew_files_test --gtest_print_time=1
```

### CI/CD Testing

#### Actual CI/CD Architecture (2025-01-24)

**WARNING**: Previous documentation claimed workflows that don't exist (`ci-main.yml`, `scheduled.yml`, `manual.yml`). This section reflects **actual** CI configuration.

**Workflows that actually exist:**

| Workflow | Purpose | Trigger | Jobs | Runtime |
|----------|---------|---------|------|--------|
| `pr-validation.yml` | Fast PR feedback | Pull request | 2 | ~15 min |
| `build.yml` | Main CI (not `ci-main.yml`) | Push to main | 2 | ~1h |
| `release.yml` | Release artifacts | Git tag (v*) | 5 | ~30 min |

**Reusable Workflows:**

| Workflow | Purpose | Used By |
|----------|---------|---------|
| `_build-test-reusable.yml` | Core build/test | `pr-validation.yml`, `build.yml` |
| `_build-release-reusable.yml` | Release builds | `release.yml` |

**CRITICAL GAP**: Only 2 of 40+ CMakePresets are tested in CI!

| Platform | Preset | CI Status |
|----------|--------|-----------|
| Linux x64 | x64-linux-production | ✅ Tested |
| Linux x64 | x64-linux-experimental | ✅ Tested |
| Linux x64 dynamic | x64-linux-dynamic-* | ❌ NOT tested |
| Linux ARM64 | arm64-linux-* | ❌ NOT tested |
| macOS | All macOS presets | ❌ NOT tested |
| Windows | All Windows presets | ❌ NOT tested |

#### GitHub Actions

Tests run automatically on:
- Pull requests → `pr-validation.yml` (fast feedback, 2 jobs)
- Pushes to main branch → `build.yml` (2 jobs)
- Git tags (v*) → `release.yml` (create release)

**TODO**: Expand CI to test all triplets (see `ci-matrix.yml` planned workflow).

#### CI/CD Matrix Testing

**build.yml** tests only **2 configurations**:

| Platform | Runner | Triplet | Config | Status |
|----------|--------|---------|--------|--------|
| **Linux** | ubuntu-latest | x64-linux | production | ✅ Active |
| **Linux** | ubuntu-latest | x64-linux | experimental | ✅ Active |

**Total: 2 CI jobs** (out of 40+ presets defined in CMakePresets.json)

**Configuration Matrix:**

| Configuration | Metadata Formats | File Extensions | Purpose |
|--------------|------------------|-----------------|---------|
| `production` | FlatBuffers + Legacy Thrift | .dff, .dft | Modern default, stable |
| `experimental` | FlatBuffers + Legacy Thrift + Modern Thrift | .dff, .dft | All formats, max compatibility |

**Reusable Workflow:**

The CI uses `.github/workflows/_build-test-reusable.yml` which can be called by other workflows:

```yaml
jobs:
  test:
    uses: ./.github/workflows/_build-test-reusable.yml
    with:
      triplet: ${{ matrix.triplet }}
      config: ${{ matrix.config }}
      runner: ${{ matrix.runner }}
      with-fuse: false
```

**Composite Actions:**

The CI uses reusable composite actions (`.github/actions/`):
- `setup-vcpkg/` - vcpkg setup with overlay ports
- `setup-build-deps/` - Build dependencies installation
- `configure-cmake/` - CMake configuration
- `run-ctest/` - Test execution with result upload

## Compatibility Test Framework

### Purpose

The compatibility test framework ensures that Tebako DwarFS can:

1. **Read** files created by Homebrew's mkdwarfs
2. **Write** files that Homebrew's dwarfs can read
3. **Maintain** compatibility across versions

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Compatibility Test Framework                │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │  Homebrew    │───▶│   Fixture    │───▶│   Fixture    │     │
│  │  Detector    │    │  Generator   │    │    Cache     │     │
│  └──────────────┘    └──────────────┘    └──────────────┘     │
│         │                                       │              │
│         ▼                                       ▼              │
│  Finds mkdwarfs                        Manages DFT files      │
│  installation                          with checksums        │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Test Fixtures

Test fixtures are DwarFS filesystem images (`.dft` files) used for compatibility testing.

**Naming Convention:**
```
dwarfs-v{version}-{platform}-{arch}.dft
```

Examples:
- `dwarfs-v0.14.1-darwin-arm64.dft`
- `dwarfs-vlatest-linux-x86_64.dft`

**Fixture Contents:**
- Test files (random data)
- Directories
- Symlinks
- Metadata (permissions, timestamps, etc.)

### Fixture Cache

The fixture cache manages test fixtures:

```cpp
// Create cache
auto cache = std::make_shared<FixtureCache>("/path/to/cache");

// Store fixture with checksum
cache->store(spec, data);

// Load fixture
auto data = cache->load(spec);

// Validate checksum
bool valid = cache->validate_checksum(spec);

// List fixtures
auto fixtures = cache->list_fixtures();
```

### Homebrew Detection

The framework automatically detects Homebrew dwarfs installation:

```cpp
// Detect Homebrew installation
auto detector = std::make_shared<HomebrewDetector>();
HomebrewInfo info = detector->detect();

// Check detection results
if (info.is_complete()) {
    std::cout << "mkdwarfs: " << info.mkdwarfs_path << std::endl;
    std::cout << "version: " << info.version << std::endl;
}
```

**Supported Platforms:**
- macOS: `/opt/homebrew` (arm64), `/usr/local` (x86_64)
- Linux: `/home/linuxbrew/.linuxbrew`, `~/.linuxbrew`

## Platform Matrix Testing

### Local Testing

To test on your current platform:

```bash
cd build
./test/compat/read_homebrew_files_test
```

### CI/CD Matrix

GitHub Actions runs tests across multiple platforms:

```yaml
strategy:
  matrix:
    os: [macos-latest, ubuntu-latest]
    arch: [arm64, x86_64]
```

### Cross-Platform Considerations

1. **macOS**: FUSE reader (`dwarfs`) not available, only `mkdwarfs`
2. **Linux**: Full toolchain available (`mkdwarfs`, `dwarfs`, `dwarfsextract`)
3. **Windows**: Requires WSL or native port (in development)

## Writing Tests

### Unit Test Example

```cpp
#include <gtest/gtest.h>

TEST(MyTestSuite, TestName) {
    // Arrange
    int expected = 42;

    // Act
    int actual = calculate_value();

    // Assert
    EXPECT_EQ(actual, expected);
}
```

### Compatibility Test Example

```cpp
TEST_F(ReadHomebrewFilesTest, ReadBasicFixture) {
    if (!info_.is_complete()) {
        GTEST_SKIP() << "Homebrew dwarfs not available";
    }

    auto spec = create_test_spec();

    // Generate fixture if not cached
    if (!cache_->has_valid_fixture(spec)) {
        std::string fixture_path = generator_->generate(spec);
        std::vector<uint8_t> data = read_file_to_bytes(fixture_path);
        cache_->store(spec, data);
    }

    // Load and verify
    std::vector<uint8_t> data = cache_->load(spec);
    EXPECT_FALSE(data.empty());
}
```

### Test Fixtures

Use test fixtures for common setup:

```cpp
class MyTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }

    // Test data members
    std::unique_ptr<MyClass> obj_;
};
```

## Troubleshooting

### Tests Being Skipped

**Problem:** "Homebrew dwarfs not available"

**Solution:**
1. Verify Homebrew is installed: `which brew`
2. Verify mkdwarfs is installed: `brew list dwarfs`
3. Check detection: `/opt/homebrew/bin/mkdwarfs --version`

### Checksum Mismatches

**Problem:** "Checksum validation failed"

**Solution:**
1. Clear fixture cache: `rm -rf /tmp/dwarfs-compat-*`
2. Regenerate fixtures
3. Verify mkdwarfs version matches expected

### mkdwarfs Errors

**Problem:** "mkdwarfs failed with exit code: 1"

**Solution:**
1. Check if output file exists (use `--force` flag)
2. Verify input directory exists
3. Check mkdwarfs logs

### Build Failures

**Problem:** "undefined reference to..."

**Solution:**
1. Clean build: `BUILD_DIR=build ./scripts/clean-build.sh -y`
2. Check dependencies: `vcpkg list`
3. Verify CMake configuration

## Continuous Integration

### GitHub Actions Workflows

**Location:** `.github/workflows/`

**Workflows that actually exist:**
- `pr-validation.yml`: Fast PR validation (2 jobs)
- `build.yml`: Main CI pipeline (2 jobs)
- `release.yml`: Release workflow
- `_build-test-reusable.yml`: Reusable build/test template

**WARNING**: `ci.yml` mentioned in some documentation **does not exist**.

### Triggering CI

```bash
# On PR
git push origin feature-branch

# Manual trigger
gh workflow run pr-validation.yml
gh workflow run build.yml
```

**Note**: There is no `ci.yml` or `scheduled.yml` workflow currently implemented.

### Test Artifacts

Test artifacts are stored for:
- Failed test logs
- Fixture files
- Coverage reports (if enabled)

## Best Practices

1. **Run tests locally** before pushing
2. **Use descriptive test names** that explain what is being tested
3. **Skip tests gracefully** when dependencies aren't available
4. **Clean up temporary files** in test teardown
5. **Use fixtures** for common test data
6. **Keep tests fast** - avoid I/O where possible
7. **Test error paths** as well as success paths

## Contributing

When adding new features:

1. Write unit tests for new code
2. Add compatibility tests if format changes
3. Update benchmarks if performance changes
4. Document test requirements in this file

## Resources

- **Google Test Documentation**: https://google.github.io/googletest/
- **CMake Documentation**: https://cmake.org/documentation/
- **vcpkg Documentation**: https://vcpkg.io/en/
- **Project ARCHITECTURE.md**: Detailed architecture documentation
