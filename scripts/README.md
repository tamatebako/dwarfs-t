# Scripts Directory

This directory contains convenience scripts for building, testing, and benchmarking DwarFS across different configurations.

## Quick Reference

| Script | Purpose | When to Use |
|--------|---------|-------------|
| [`build-all-and-test.sh`](#build-all-and-testsh) | Build + test all 3 configs | Complete validation |
| [`benchmark-all.sh`](#benchmark-allsh) | Run comprehensive benchmarks | Performance testing |
| [`run-all.sh`](#run-allsh) | Clean → Build → Test → Benchmark | Complete workflow |
| [`clean-build.sh`](#clean-buildsh) | Clean build single config | Fresh development build |
| [`test-all-configs.sh`](#test-all-configssh) | Test all 3 configs | Quick validation |
| [`verify_benchmark_setup.sh`](#verify_benchmark_setupsh) | Check benchmark readiness | Before benchmarking |
| [`test_vcpkg_install.sh`](#test_vcpkg_installsh) | Test vcpkg integration | vcpkg development |
| [`extract_blocks.py`](#extract_blockspy) | Extract DwarFS blocks | Low-level analysis |

---

## Core Workflow Scripts

### `build-all-and-test.sh`

**Purpose**: Build and test all three metadata serialization configurations.

**Usage**:
```bash
./scripts/build-all-and-test.sh [--vcpkg]
```

**Options**:
- `--vcpkg`: Use vcpkg for dependencies (requires `VCPKG_ROOT`)

**Configurations Built**:
1. **FlatBuffers-only** (`build-fb-only/`) - Modern default
2. **Both formats** (`build-both/`) - FlatBuffers + Thrift
3. **Thrift-only** (`build-thrift-only/`) - Legacy format

**Environment Variables**:
- `JOBS`: Parallel build jobs (default: 8)
- `BUILD_TYPE`: Release/Debug (default: Release)
- `CMAKE_GENERATOR`: Ninja/Make (default: Ninja)
- `VCPKG_ROOT`: vcpkg installation path
- `VCPKG_TRIPLET`: vcpkg triplet (auto-detected if not set)

**Output**:
- `build-fb-only/`: FlatBuffers-only build
- `build-both/`: Both-formats build
- `build-thrift-only/`: Thrift-only build

**Exit Codes**:
- `0`: All builds and tests passed
- `1`: One or more builds/tests failed

---

### `benchmark-all.sh`

**Purpose**: Run comprehensive benchmarks (delegates to `benchmarks/run_comprehensive_benchmark.sh`).

**Usage**:
```bash
./scripts/benchmark-all.sh [options]
```

**What It Does**:
- Delegates to `benchmarks/run_comprehensive_benchmark.sh`
- Runs FUSE extraction and libdwarfs API benchmarks
- Tests all 3 build configurations
- Generates comprehensive reports

**See Also**:
- [`benchmarks/README.md`](../benchmarks/README.md) - Detailed benchmark documentation
- [`benchmarks/run_comprehensive_benchmark.sh`](../benchmarks/run_comprehensive_benchmark.sh) - Actual implementation

---

### `run-all.sh`

**Purpose**: Complete workflow - clean, build, test, and benchmark all configurations.

**Usage**:
```bash
./scripts/run-all.sh [dataset_path]
```

**Workflow**:
1. **Clean**: Remove all `build-*` directories
2. **Build**: Run `build-all-and-test.sh`
3. **Benchmark**: Run `benchmark-all.sh`

**When to Use**:
- Complete validation before commits
- CI/CD-like local testing
- Performance regression testing

**Estimated Time**: 30-60 minutes (build + test + benchmark)

---

## Development Scripts

### `clean-build.sh`

**Purpose**: Clean and configure a single build directory for development.

**Usage**:
```bash
./scripts/clean-build.sh [-y]
```

**Options**:
- `-y`: Skip confirmation prompt

**Environment Variables**:
- `BUILD_DIR`: Target directory (default: `build-test`)
- `BUILD_TYPE`: Release/Debug (default: Release)
- `WITH_TESTS`: ON/OFF (default: ON)
- `WITH_TOOLS`: ON/OFF (default: ON)
- `WITH_LIBDWARFS`: ON/OFF (default: ON)
- `WITH_FLATBUFFERS`: ON/OFF (default: ON)
- `WITH_THRIFT`: ON/OFF (default: OFF)

**Example**:
```bash
# FlatBuffers-only development build
BUILD_DIR=build-dev WITH_THRIFT=OFF ./scripts/clean-build.sh -y

# Debug build with Thrift
BUILD_TYPE=Debug WITH_THRIFT=ON ./scripts/clean-build.sh
```

---

### `test-all-configs.sh`

**Purpose**: Quick test of all three configurations (build + test only, no benchmarks).

**Usage**:
```bash
./scripts/test-all-configs.sh
```

**What It Tests**:
- FlatBuffers-only (18 tests expected)
- Thrift-only (11 tests expected)
- Both-formats (18 tests expected)

**Output**: Test logs in `build-test-*-test.log`

**When to Use**:
- Quick validation during development
- Pre-commit checks
- CI/CD fast path

---

## Utility Scripts

### `verify_benchmark_setup.sh`

**Purpose**: Verify benchmark infrastructure is ready.

**Usage**:
```bash
./scripts/verify_benchmark_setup.sh
```

**Checks**:
- ✓ Python 3 available
- ✓ Build directories exist
- ✓ Test executables present
- ✓ Benchmark scripts available
- ✓ Benchmark libraries present
- ✓ Datasets downloaded
- ✓ CI workflows configured
- ✓ Quick functional test

**Exit Codes**:
- `0`: All checks passed
- `1`: One or more checks failed

---

### `test_vcpkg_install.sh`

**Purpose**: Test vcpkg port installation and CMake integration.

**Usage**:
```bash
./scripts/test_vcpkg_install.sh
```

**What It Tests**:
1. `libdwarfs` port installation
2. `dwarfs` tools port installation
3. CMake `find_package(dwarfs)` integration
4. Linking and execution of test programs

**Requirements**:
- vcpkg installed at `~/vcpkg`
- Overlay ports in `ports/` directory

**When to Use**:
- Verifying vcpkg port changes
- Testing installation procedures
- CI/CD vcpkg validation

---

### `extract_blocks.py`

**Purpose**: Extract individual compressed blocks from DwarFS images for low-level analysis.

**Usage**:
```bash
./scripts/extract_blocks.py <image> <basename>
```

**Example**:
```bash
./scripts/extract_blocks.py test.dff test_blocks/block_
# Creates: test_blocks/block_block0, test_blocks/block_metadata8, etc.
```

**Output Files**:
- `<basename>block<N>`: Compressed block data (without headers)
- `<basename>schema<N>`: Schema sections
- `<basename>metadata<N>`: Metadata sections
- `<basename>index<N>`: Index sections
- `<basename>history<N>`: History sections

**When to Use**:
- Experimenting with compression algorithms externally
- Analyzing block-level compression ratios
- Debugging compression issues
- Research and development

**Limitations**:
- Cannot handle DwarFS image offsets
- Extracts raw blocks without headers
- Python 3 required

---

## Script Organization (MECE)

The scripts are organized into **Mutually Exclusive, Collectively Exhaustive** categories:

### Core Workflows (Complete Tasks)
- **`run-all.sh`**: Clean → Build → Test → Benchmark (everything)
- **`build-all-and-test.sh`**: Build → Test (no benchmarking)
- **`benchmark-all.sh`**: Benchmark only (requires existing builds)

### Development Tools (Single Purpose)
- **`clean-build.sh`**: Configure a single build
- **`test-all-configs.sh`**: Test existing builds
- **`verify_benchmark_setup.sh`**: Verify infrastructure

### Integration Testing
- **`test_vcpkg_install.sh`**: vcpkg port validation

### Analysis Tools
- **`extract_blocks.py`**: Low-level DwarFS analysis

---

## Common Workflows

### Complete Validation
```bash
# Full clean build and test
./scripts/run-all.sh

# Or step-by-step:
./scripts/build-all-and-test.sh
./scripts/verify_benchmark_setup.sh
./scripts/benchmark-all.sh
```

### Quick Development Cycle
```bash
# Single config build and test
BUILD_DIR=build-dev ./scripts/clean-build.sh -y
cd build-dev && ninja && ctest

# Or test all configs quickly
./scripts/test-all-configs.sh
```

### Benchmark Testing
```bash
# Verify setup first
./scripts/verify_benchmark_setup.sh

# Run benchmarks
./scripts/benchmark-all.sh
```

### vcpkg Development
```bash
# Test vcpkg ports
./scripts/test_vcpkg_install.sh

# Build with vcpkg
./scripts/build-all-and-test.sh --vcpkg
```

---

## Environment Setup

### Required Tools
- **CMake** ≥3.28
- **Ninja** (recommended) or Make
- **C++20 compiler** (GCC 10+, Clang 12+, AppleClang 14+)
- **Python 3** (for benchmarks and utilities)

### Optional Tools
- **vcpkg** (for `--vcpkg` builds and `test_vcpkg_install.sh`)
- **FUSE** libraries (for `dwarfs` FUSE driver)

### Environment Variables
```bash
# Build configuration
export JOBS=8                    # Parallel build jobs
export BUILD_TYPE=Release        # or Debug
export CMAKE_GENERATOR=Ninja     # or "Unix Makefiles"

# vcpkg (if using)
export VCPKG_ROOT=~/vcpkg
export VCPKG_TRIPLET=arm64-osx-static  # or auto-detect

# Custom paths
export BUILD_DIR=build-custom
```

---

## See Also

- **[`benchmarks/README.md`](../benchmarks/README.md)**: Detailed benchmarking documentation
- **[`example/static-site-server/README.md`](../example/static-site-server/README.md)**: Example application
- **[`doc/vcpkg-build-guide.md`](../doc/vcpkg-build-guide.md)**: vcpkg integration guide
- **[`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md)**: System architecture

---

**Last Updated**: 2025-12-30
**Status**: Production-ready scripts