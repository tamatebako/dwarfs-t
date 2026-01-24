# DwarFS Pre-Release Checklist

## Completed Items ✅

### 1. ✅ Metadata Format Support (COMPLETED)

Tebako DwarFS supports 3 metadata formats in 2 builds:

- **FlatBuffers** (modern default, stable)
- **Legacy Thrift** (hand-coded Frozen2, Homebrew compatible)
- **Modern Thrift** (fbthrift, optional, experimental)

**Build Configurations:**
- `production`: FlatBuffers + Legacy Thrift (stable, recommended)
- `experimental`: FlatBuffers + Legacy Thrift + Modern Thrift (experimental)

**Note:** Legacy Thrift (hand-coded Frozen2) is always available in both configurations.

### 2. ✅ Legacy Thrift Implementation (COMPLETED)

Hand-written serialization code in `src/metadata/legacy/` that does not depend on external Thrift libraries.

**Compatibility Test Suite** (`test/compat/`):
- Homebrew detection (`test/compat/lib/homebrew_detector.h/cpp`)
- Fixture generation (`test/compat/lib/fixture_generator.h/cpp`)
- Fixture caching (`test/compat/lib/fixture_cache.h/cpp`)
- Test executables (read, write, round-trip tests)

**Format Usage:**
```bash
# Create Homebrew-compatible Legacy Thrift images
mkdwarfs -i input/ -o output.dft --format=legacy-thrift

# Create Production images (recommended)
mkdwarfs -i input/ -o output.dff --format=flatbuffers

# Create Experimental images (with Modern Thrift)
mkdwarfs -i input/ -o output.dft --format=thrift
```

### 3. ✅ Unified Build System (COMPLETED)

Created a comprehensive unified build/test/benchmark/release system:

**Architecture Documentation:**
- `BUILD_SYSTEM_ARCHITECTURE.md` - Complete system architecture
- `scripts/README.md` - Scripts directory documentation
- `vcpkg_triplets/README.md` - Triplet documentation

**Shared Libraries** (`scripts/lib/`):
- `build_env.sh` - Environment detection, colors, output functions
- `vcpkg_helper.sh` - vcpkg utilities, triplet management, Tebako jemalloc verification

**Orchestrator Modules** (`scripts/orchestrator/`):
- `build.sh` - Core build logic with configuration model
- `release.sh` - Release orchestration (prereqs, tests, benchmarks, artifacts)

**One-Step Entry Points** (`scripts/one-step/`):
- `test-everything.sh` - Run all tests (auto-detects vcpkg or system packages)
- `build-all.sh` - Build all configurations
- `clean.sh` - Clean build artifacts
- `benchmark-quick.sh` - Quick benchmark validation

**Utility Scripts** (`scripts/utils/`):
- `clean.sh` - Core clean utility with --all and --yes options

**Usage:**
```bash
# One-step testing
./scripts/one-step/test-everything.sh
./scripts/one-step/test-everything.sh --quick   # Skip benchmarks
./scripts/one-step/test-everything.sh --vcpkg   # Force vcpkg mode

# Build all configurations
./scripts/one-step/build-all.sh

# Clean build artifacts
./scripts/one-step/clean.sh --all --yes
```

### 4. ✅ vcpkg Triplet Support (COMPLETED)

Created custom vcpkg triplets in `vcpkg_triplets/`:

**macOS:**
- `arm64-osx` (static, recommended for M1/M2/M3)
- `x64-osx` (static, recommended for Intel)
- `arm64-osx-dynamic` (dynamic)
- `x64-osx-dynamic` (dynamic)

**Linux:**
- `arm64-linux` (static, exists)
- `x64-linux` (static, exists)
- `arm64-linux-dynamic` (dynamic)
- `x64-linux-dynamic` (dynamic)

**Windows:**
- `arm64-windows-static` (static, exists)
- `x64-windows-static` (static, exists)
- `arm64-windows-dynamic` (dynamic)
- `x64-windows-dynamic` (dynamic)
- `arm64-mingw-static` (exists)
- `x64-mingw-static` (exists)

**Total: 12+ triplets supported**

### 5. ✅ Scripts Directory MECE Organization (COMPLETED)

Reorganized scripts directory into:
- `lib/` - Shared library functions
- `orchestrator/` - Core build/test/benchmark/release logic
- `one-step/` - User-facing one-step entry points
- `utils/` - Utility scripts
- Legacy scripts with backward compatibility wrappers

### 6. ✅ CI/CD Reusable Workflows (COMPLETED)

Created reusable workflow templates:
- `.github/workflows/_build-test-reusable.yml` - Reusable build and test workflow
- `.github/workflows/ci.yml` - Main CI workflow using reusable workflow

**Existing Composite Actions** (`.github/actions/`):
- `setup-vcpkg/` - vcpkg setup
- `setup-build-deps/` - Build dependencies setup
- `configure-cmake/` - CMake configuration
- `run-ctest/` - Test execution

### 7. ✅ Tebako jemalloc Integration (COMPLETED)

**CRITICAL**: Project uses Tebako's fork of jemalloc 5.5.0, NOT upstream jemalloc!

**Implementation:**
- Overlay port at `vcpkg_ports/jemalloc/`
- Automatic verification in all build scripts
- vcpkg-first approach for all builds (system jemalloc 5.3.0 is incompatible)

## Remaining Items

### 1. ⚠️ CI Matrix (NOT COMPLETE - Only 2 of 40+ presets tested)

**Status**: CRITICAL GAP - CMakePresets.json defines 40+ configurations, but CI only tests 2.

**Current CI (build.yml)**:
- test-ubuntu-production: x64-linux production ✓
- test-ubuntu-experimental: x64-linux experimental ✓

**MISSING from CI** (defined in CMakePresets.json):
- Linux: arm64, musl, dynamic, release-dbg (8 presets)
- macOS: x64, arm64 (static/dynamic variants, 12 presets)
- Windows: x64, arm64 (static/dynamic/msys/mingw, 14 presets)

**Required Action**: Create `ci-matrix.yml` workflow to test all triplets.

### 2. ⚠️ Documentation Outdated (CRITICAL)

**TESTING.md claims workflows that DON'T exist**:
- `ci-main.yml` - Claims 10 jobs, doesn't exist
- `scheduled.yml` - Claims weekly benchmarks, doesn't exist
- `manual.yml` - Claims on-demand testing, doesn't exist

**What actually exists**:
- `pr-validation.yml` - 2 jobs (not 4 as documented)
- `build.yml` - 2 jobs (not `ci-main.yml`)
- `release.yml` - Release workflow

**Required Action**: Update TESTING.md to reflect actual CI configuration.

### 3. ⚠️ No macOS/Windows CI (CRITICAL)

Despite having CMakePresets for:
- macOS x64/arm64 (static/dynamic)
- Windows x64/arm64 (static/dynamic/msys/mingw)

**No CI jobs exist for these platforms.**

**Required Action**: Add macOS and Windows jobs to ci-matrix.yml.

### 4. ⚠️ No ARM64 Linux CI

Despite having presets for `arm64-linux-production` and `arm64-linux-experimental`, **no CI jobs exist**.

**Required Action**: Add ARM64 jobs to ci-matrix.yml.

### 5. ⚠️ No Dynamic Build CI

Linux dynamic presets (`x64-linux-dynamic-*`) exist but **not tested in CI**.

### 6. ⚠️ No Musl CI

Alpine Linux presets (`x64-linux-musl-*`) exist but **not tested in CI**.

### 7. ⚠️ No RelWithDebInfo CI

Release+Debug presets (`x64-linux-release-dbg-*`) exist but **not tested in CI**.

### 8. 📝 Benchmarks (PARTIAL - Infrastructure Ready)

**Status**: Benchmark infrastructure is in place, but requires vcpkg build with Tebako jemalloc.

**Requirements:**
- Build with vcpkg (for Tebako jemalloc 5.5.0)
- Download perl source dataset (~200MB)
- Run comprehensive benchmark suite (~2-3 hours)

**Quick Validation** (available now):
```bash
./scripts/one-step/benchmark-quick.sh
```

**Full Benchmarks** (requires complete build):
```bash
# Set up vcpkg
export VCPKG_ROOT=~/vcpkg

# Run full benchmark suite
./benchmarks/run_comprehensive_benchmark.sh
```

**To update README.md with benchmark results:**
1. Complete full benchmark suite
2. Extract key metrics from results
3. Update performance section in README.md
4. Add benchmark comparison tables

### 9. 📝 Directory Structure Cleanup (IN PROGRESS)

Reference: `/Users/mulgogi/src/external/xz` (clean directory structure)

**Completed:**
- Scripts directory MECE organization
- vcpkg triplets organized
- Documentation files created

**Remaining:**
- Consider reorganizing source code structure
- Consider consolidating header files
- Review and remove deprecated code

### 10. 📝 Additional DRY Workflows (OPTIONAL)

**Completed:**
- Reusable workflow template created
- Main CI workflow updated

**Remaining:**
- Convert all legacy workflows to use reusable templates
- Create additional composite actions for common operations
- Consolidate duplicate workflow logic

## Quick Start for Developers

```bash
# Clone and setup
git clone --recurse-submodules https://github.com/tamatebako/dwarfs.git
cd dwarfs

# One-time vcpkg setup
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# Run all tests (one command!)
./scripts/one-step/test-everything.sh
```

## Status Summary

| Item | Status | Notes |
|------|--------|-------|
| Metadata Formats | ✅ Complete | FB, Legacy Thrift, Modern Thrift |
| Legacy Thrift Tests | ✅ Complete | Compatibility test suite created |
| Unified Build System | ✅ Complete | Orchestrator + libs + one-step scripts |
| vcpkg Triplets | ✅ Complete | 40+ presets defined in CMakePresets.json |
| Scripts Organization | ✅ Complete | MECE structure with backward compat |
| Tebako jemalloc | ✅ Complete | Overlay port + auto-verification |
| TESTING Documentation | ⚠️ Outdated | Claims workflows that don't exist |
| DEVELOPER_WORKFLOW | ⚠️ Needs Review | May reference non-existent workflows |
| CI Matrix (All Triplets) | ❌ Incomplete | Only 2 of 40+ presets tested |
| Benchmarks | ⚠️ Partial | Infrastructure ready, requires full build |
| Directory Cleanup | 📝 In Progress | Scripts done, source code pending |
| Full DRY Workflows | 📝 Optional | Core done, legacy workflows pending |

## CI/CD Matrix Status (2025-01-24)

### Current CI Workflows (ACTUAL - Not Aspiration)

| Workflow | Purpose | Trigger | Platforms | Jobs | Status |
|----------|---------|---------|-----------|------|--------|
| `pr-validation.yml` | Fast PR feedback | Pull request | Linux x64 | 2 | ✅ Active |
| `build.yml` | Main CI (current branch) | Push to main | Linux x64 | 2 | ✅ Active |
| `release.yml` | Release builds | Git tag (v*) | Variable | 5 | ✅ Active |

### CI Matrix Coverage (ACTUAL - Not Aspiration)

**Currently tested (build.yml + pr-validation.yml):**

| Platform | Runner | Triplet | Config | Status |
|----------|--------|---------|--------|--------|
| **Linux x64** | ubuntu-latest | x64-linux | production | ✅ Active |
| **Linux x64** | ubuntu-latest | x64-linux | experimental | ✅ Active |

**Total: 2 CI jobs** (not 11 as previously claimed)

### Presets Available but NOT Tested in CI

| Platform | Triplet | Configs | CI Status |
|----------|---------|---------|-----------|
| Linux x64 dynamic | x64-linux-dynamic | production, experimental | ❌ Not tested |
| Linux x64 RelWithDebInfo | x64-linux-release-dbg | production, experimental | ❌ Not tested |
| Linux x64 musl | x64-linux-musl | production, experimental | ❌ Not tested |
| Linux ARM64 | arm64-linux | production, experimental | ❌ Not tested |
| macOS x64 | x64-osx | production, experimental | ❌ Not tested |
| macOS x64 dynamic | x64-osx-dynamic | production, experimental | ❌ Not tested |
| macOS x64 static | x64-osx-static | production, experimental | ❌ Not tested |
| macOS ARM64 | arm64-osx | production, experimental | ❌ Not tested |
| macOS ARM64 dynamic | arm64-osx-dynamic | production, experimental | ❌ Not tested |
| macOS ARM64 static | arm64-osx-static | production, experimental | ❌ Not tested |
| Windows x64 | x64-windows | production, experimental | ❌ Not tested |
| Windows x64 static | x64-windows-static | production, experimental | ❌ Not tested |
| Windows x64 static-md | x64-windows-static-md | production, experimental | ❌ Not tested |
| Windows x64 dynamic | x64-windows-dynamic | production, experimental | ❌ Not tested |
| Windows ARM64 static | arm64-windows-static | production, experimental | ❌ Not tested |
| Windows x64 msys | x64-windows-msys | production, experimental | ❌ Not tested |
| Windows x64 mingw | x64-windows-mingw | production, experimental | ❌ Not tested |

**Total presets defined: 40+ | Currently tested: 2 (5%)**

### Documentation

| Document | Purpose | Status |
|----------|---------|--------|
| `TESTING.md` | Comprehensive testing guide | ⚠️ Outdated - claims non-existent workflows |
| `DEVELOPER_WORKFLOW.md` | Developer & release manager workflows | ⚠️ Needs Review |
| `BUILD_SYSTEM_ARCHITECTURE.md` | Build system details | ✅ Complete |
| `README.md` | Project overview | ✅ Complete |

**Last Updated**: 2025-01-24
**Overall Status**: CI matrix is INCOMPLETE - only 2 of 40+ presets tested. Documentation needs updating.
