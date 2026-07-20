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
3. ✅ Tests production configuration (FlatBuffers + Legacy Thrift)
4. ✅ Runs all unit tests
5. ⏭️  Optionally runs benchmarks (prompts first)

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

### One-Step Process for Developers

```bash
# Clone repository (with submodules)
git clone --recurse-submodules https://github.com/tamatebako/dwarfs-t.git
cd dwarfs-t

# Install vcpkg (one-time setup)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# Run all tests (one command!)
./scripts/one-step/test-everything.sh
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
./scripts/one-step/test-everything.sh --quick
```

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

## CI/CD Testing

### Workflow Architecture (2025-01-24)

DwarFS uses a **MECE** (Mutually Exclusive, Collectively Exhaustive) workflow structure:

| Workflow | Purpose | Trigger | Jobs | Runtime |
|----------|---------|--------|------|--------|
| `pr-validation.yml` | Fast PR feedback | Pull requests | 1 | ~15 min |
| `ci.yml` | Main CI validation | Push to main, feature/** | 3 | ~60 min |
| `ci-matrix.yml` | Full cross-platform matrix | Push to main, manual | 17 | ~2-3 hours |
| `release.yml` | Release artifacts | Git tags (v*) | Variable | ~30 min |

**Reusable Core:**
| Workflow | Purpose | Used By |
|----------|---------|---------|
| `_build.yml` | Core build/test logic | All CI workflows |

### Workflow Details

#### pr-validation.yml - Fast PR Feedback

**Trigger:** Pull requests to main or feature branches

**Jobs:**
- Linux x64 Production (1 job)

**Purpose:** Provide quick feedback to developers before merging. Tests only the stable production configuration.

#### ci.yml - Main CI

**Trigger:** Push to main or feature branches

**Jobs:**
- 📦 Linux x64 Production (must pass)
- 🧪 Linux x64 Experimental (may fail - known folly issues)
- 🔧 Linux x64 Dynamic (must pass)

**Purpose:** Validate every commit to main branches. Production builds must pass. Experimental builds are allowed to fail due to known issues with folly/fbthrift dependencies.

#### ci-matrix.yml - Full Cross-Platform Matrix

**Trigger:** Push to main branch, or manual dispatch

**Platform Coverage:**
- **Linux** (6 jobs): x64 (production/experimental/dynamic), ARM64 (production/experimental)
- **macOS** (5 jobs): x64 (production/dynamic), ARM64 (production/experimental/dynamic)
- **Windows** (5 jobs): x64 (static/dynamic/MSYS/MinGW), ARM64 (static)

**Total: 17 jobs**

**Purpose:** Comprehensive cross-platform validation. Tests all supported triplets to ensure DwarFS works everywhere.

**Manual Usage:**
```bash
# Trigger via GitHub CLI
gh workflow run ci-matrix.yml

# With platform filter
gh workflow run ci-matrix.yml -f platform=macos
```

#### _build.yml - Reusable Core

**Purpose:** Single-source-of-truth for build logic. Called by all other workflows.

**Features:**
- Uses lukka/run-vcpkg for vcpkg setup with binary caching
- Uses lukka/run-cmake for CMakePresets-based building
- Supports all triplets and configurations
- Shared caching across all workflows

### GitHub Actions Usage

Tests run automatically on:
- **Pull requests** → `pr-validation.yml` (fast feedback)
- **Pushes to main** → `ci.yml` (production + experimental)
- **Pushes to main** → `ci-matrix.yml` (full matrix)
- **Git tags** → `release.yml` (create release)

**Manual Trigger:**
```bash
# Run specific workflow
gh workflow run pr-validation.yml
gh workflow run ci.yml
gh workflow run ci-matrix.yml

# With platform filter
gh workflow run ci-matrix.yml -f platform=windows
```

### CI Triplet Coverage

| Platform | Triplet | Config | CI Status |
|----------|---------|--------|-----------|
| **Linux x64** | x64-linux | production | ✅ pr-validation, ci, ci-matrix |
| **Linux x64** | x64-linux | experimental | ✅ ci, ci-matrix |
| **Linux x64** | x64-linux-dynamic | production | ✅ ci, ci-matrix |
| **Linux x64** | x64-linux-dynamic | experimental | ✅ ci-matrix |
| **Linux ARM64** | arm64-linux | production | ✅ ci-matrix |
| **Linux ARM64** | arm64-linux | experimental | ✅ ci-matrix |
| **macOS x64** | x64-osx | production | ✅ ci-matrix |
| **macOS x64** | x64-osx-dynamic | production | ✅ ci-matrix |
| **macOS ARM64** | arm64-osx | production | ✅ ci-matrix |
| **macOS ARM64** | arm64-osx | experimental | ✅ ci-matrix |
| **macOS ARM64** | arm64-osx-dynamic | production | ✅ ci-matrix |
| **Windows x64** | x64-windows-static | production | ✅ ci-matrix |
| **Windows x64** | x64-windows-dynamic | production | ✅ ci-matrix |
| **Windows x64** | x64-windows-msys | production | ✅ ci-matrix |
| **Windows x64** | x64-windows-mingw | production | ✅ ci-matrix |
| **Windows ARM64** | arm64-windows-static | production | ✅ ci-matrix |

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

### Common Developer Issues

**Issue**: Tests fail locally but pass on CI
**Solution**: Ensure you're using vcpkg mode, not system packages

**Issue**: "jemalloc not found"
**Solution**: The project requires Tebako jemalloc 5.5.0, not upstream 5.3.0

**Issue**: CI timeout
**Solution**: Use `--quick` mode for faster iteration

### Experimental Build Issues

**Issue**: Experimental build fails
**Explanation**: The experimental configuration builds Modern Thrift (fbthrift) which depends on folly, fizz, mvfst, and wangle. These are complex dependencies that may fail to compile.

**Status**: Experimental jobs are marked with `continue-on-error: true` so they don't block CI. The production build must pass.

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
- **DEVELOPER_WORKFLOW.md**: Developer and release manager workflows
- **BUILD_SYSTEM_ARCHITECTURE.md**: Build system details
- **README.md**: Project overview
