# DwarFS Unified Build System Architecture

## Overview

This document describes the unified build/test/benchmark/release system for Tebako DwarFS. The system is designed to be:

- **Extensible**: Easy to add new platforms, configurations, and tests
- **MECE**: Mutually Exclusive, Collectively Exhaustive organization
- **DRY**: Don't Repeat Yourself - centralized configuration and logic
- **One-Step**: Single command execution for developers and release managers

## Architecture Principles

### 1. Model-Driven Configuration

All build and test configurations are defined in a central model, not scattered across multiple files. This ensures:

- Single source of truth
- Easy to add new configurations
- Consistent behavior across all environments

### 2. Layered Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Developer Interface                       │
│  One-step scripts: test-everything.sh, build-all-and-test.sh │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│                 Orchestration Layer                          │
│  scripts/orchestrator/ - Build/test/benchmark coordination  │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│                   Build Configuration                        │
│  cmake/configurations.cmake - Centralized CMake config       │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│                    Platform Layer                            │
│  vcpkg/ - Triplet definitions for all platforms             │
└────────────────────────┬────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────┐
│                   CI/CD Layer                                │
│  .github/workflows/ - Reusable workflows and actions        │
└─────────────────────────────────────────────────────────────┘
```

### 3. Configuration Model

```yaml
# Build Configuration Model
build_configurations:
  flatbuffers-only:
    description: "FlatBuffers metadata only (stable, recommended)"
    options:
      DWARFS_WITH_FLATBUFFERS: ON
      DWARFS_WITH_THRIFT: OFF
    supported: true
    metadata_formats: [flatbuffers]
    file_extensions: [.dff]

  both-formats:
    description: "All metadata formats (FlatBuffers + Thrift)"
    options:
      DWARFS_WITH_FLATBUFFERS: ON
      DWARFS_WITH_THRIFT: ON
    supported: true
    metadata_formats: [flatbuffers, legacy-thrift, modern-thrift]
    file_extensions: [.dff, .dft]
```

```yaml
# Platform Triplet Model
platforms:
  macos:
    architectures: [x64, arm64]
    triplets:
      - x64-osx
      - arm64-osx
      - x64-osx-dynamic
      - arm64-osx-dynamic

  linux:
    architectures: [x64, arm64]
    triplets:
      - x64-linux
      - arm64-linux
      - x64-linux-dynamic
      - arm64-linux-dynamic

  windows:
    architectures: [x64, arm64]
    toolchains: [msvc, mingw]
    link_types: [static, dynamic]
    triplets:
      - x64-windows-static
      - arm64-windows-static
      - x64-windows-dynamic
      - arm64-windows-dynamic
      - x64-mingw-static
      - arm64-mingw-static
```

## Directory Structure

```
dwarfs/
├── scripts/
│   ├── orchestrator/           # NEW: Unified orchestration
│   │   ├── build.sh           # Core build logic
│   │   ├── test.sh            # Core test logic
│   │   ├── benchmark.sh       # Core benchmark logic
│   │   └── release.sh         # Core release logic
│   │
│   ├── lib/                    # NEW: Shared libraries
│   │   ├── build_env.sh       # Environment detection
│   │   ├── vcpkg_helper.sh    # vcpkg utilities
│   │   ├── triplet_helper.sh  # Triplet detection/mapping
│   │   └── output.sh          # Formatted output
│   │
│   ├── one-step/               # NEW: One-step entry points
│   │   ├── test-everything.sh
│   │   ├── build-all-and-test.sh
│   │   └── benchmark-all.sh
│   │
│   └── legacy/                 # Deprecated scripts (to be removed)
│       ├── clean.sh
│       ├── clean-all.sh
│       └── ...
│
├── .github/
│   ├── workflows/
│   │   ├── ci.yml             # Main CI (replaces build.yml)
│   │   ├── release.yml        # NEW: Release workflow
│   │   └── benchmark.yml      # NEW: Benchmark workflow
│   │
│   └── actions/
│       ├── setup-vcpkg/       # Composite action
│       ├── setup-build-deps/  # Composite action
│       ├── configure-cmake/   # Composite action
│       ├── run-ctest/         # Composite action
│       └── run-benchmark/     # NEW: Composite action
│
├── vcpkg_ports/               # vcpkg overlay ports
│   ├── jemalloc/              # Tebako's jemalloc 5.5.0
│   └── ...
│
└── vcpkg_triplets/            # NEW: Custom triplets
    ├── arm64-osx.cmake
    ├── x64-osx-dynamic.cmake
    └── ...
```

## One-Step Interface

### For Developers

```bash
# Clone and setup
git clone --recurse-submodules https://github.com/tamatebako/dwarfs-t.git
cd dwarfs-t
export VCPKG_ROOT="$HOME/vcpkg"  # One-time setup

# Run everything
./scripts/one-step/test-everything.sh
```

### For Release Managers

```bash
# Full release validation
./scripts/one-step/build-all-and-test.sh --vcpkg --all-triplets
./scripts/one-step/benchmark-all.sh --update-readme
./scripts/orchestrator/release.sh --dry-run
```

### For CI/CD

```yaml
# .github/workflows/ci.yml
jobs:
  test:
    uses: ./.github/workflows/_build-test.yml
    with:
      triplet: ${{ matrix.triplet }}
      config: ${{ matrix.config }}
```

## Extensibility

### Adding a New Platform

1. Create triplet in `vcpkg_triplets/`
2. Add to platform matrix in configuration model
3. CI automatically tests new platform

### Adding a New Test

1. Add test to `test/` directory
2. Register in CMakeLists.txt
3. Automatically runs in all configurations

### Adding a New Benchmark

1. Add benchmark to `benchmarks/`
2. Register in benchmark configuration
3. Automatically runs in CI and updates README

## Implementation Status

- [x] test-everything.sh (one-step testing)
- [x] ci.yml (streamlined CI)
- [ ] orchestrator/ modules
- [ ] vcpkg_triplets/ (8+ triplets)
- [ ] Reusable workflows (_build-test.yml)
- [ ] Benchmark automation with README update
- [ ] Release orchestration

## Migration Path

### Phase 1: Foundation (Current)
- [x] Create test-everything.sh
- [x] Create ci.yml
- [x] Document jemalloc dependency

### Phase 2: Orchestration (Next)
- [ ] Create scripts/lib/ for shared utilities
- [ ] Create scripts/orchestrator/ for core logic
- [ ] Refactor existing scripts to use orchestrator

### Phase 3: Triplets (Next)
- [ ] Create vcpkg_triplets/ directory
- [ ] Add 8+ custom triplet files
- [ ] Update CI to use all triplets

### Phase 4: DRY Workflows (Next)
- [ ] Create reusable workflow templates
- [ ] Convert all workflows to use templates
- [ ] Create composite actions for common operations

### Phase 5: Automation (Next)
- [ ] Automatic benchmark README updates
- [ ] Release automation
- [ ] Performance regression detection

## Configuration Files

### Central Build Configuration

`cmake/build_configurations.json`:
```json
{
  "configurations": {
    "flatbuffers-only": {
      "cmake_options": {
        "DWARFS_WITH_FLATBUFFERS": "ON",
        "DWARFS_WITH_THRIFT": "OFF"
      },
      "metadata_formats": ["flatbuffers"],
      "file_extensions": [".dff"]
    },
    "both-formats": {
      "cmake_options": {
        "DWARFS_WITH_FLATBUFFERS": "ON",
        "DWARFS_WITH_THRIFT": "ON"
      },
      "metadata_formats": ["flatbuffers", "legacy-thrift", "modern-thrift"],
      "file_extensions": [".dff", ".dft"]
    }
  }
}
```

### Platform Matrix

`cmake/platform_matrix.json`:
```json
{
  "platforms": {
    "macos": {
      "runners": ["macos-13", "macos-14"],
      "architectures": ["x64", "arm64"],
      "triplets": ["x64-osx", "arm64-osx"]
    },
    "linux": {
      "runners": ["ubuntu-latest", "ubuntu-24.04-arm64"],
      "architectures": ["x64", "arm64"],
      "triplets": ["x64-linux", "arm64-linux"]
    }
  }
}
```

## Testing Strategy

### Unit Tests
- Run on every commit
- All configurations
- All platforms

### Integration Tests
- Run on every PR
- Compatibility tests with Homebrew
- Round-trip serialization tests

### Benchmark Tests
- Run on main branch
- Performance regression detection
- Automatic README updates

## Release Process

### Pre-Release Checklist
1. All tests pass across all platforms
2. Benchmarks run and update README
3. Compatibility tests pass with Homebrew
4. Documentation updated

### Release Steps
```bash
# 1. Run full test suite
./scripts/one-step/test-everything.sh --all-triplets

# 2. Run benchmarks
./scripts/one-step/benchmark-all.sh --update-readme

# 3. Create release
./scripts/orchestrator/release.sh --version X.Y.Z
```

## Future Enhancements

1. **Performance Dashboard**: Track benchmarks over time
2. **Compatibility Matrix**: Visual matrix of format compatibility
3. **Release Notes Generation**: Automatic changelog from commits
4. **Binary Distribution**: Automated binary builds for all platforms
5. **Documentation Site**: Generated from source code comments

---

# Building DwarFS with vcpkg

## Overview

vcpkg provides a way to build fully static DwarFS binaries with all dependencies included. This is especially useful for:

- Creating portable binaries
- Static linking for distribution
- Reproducible builds across platforms
- Avoiding system dependency conflicts

**Important**: First-time vcpkg builds are **very slow** (30-60 minutes) as all dependencies are built from source. Subsequent builds are much faster due to caching.

## Prerequisites

- Git
- CMake >= 3.28
- Ninja or Make
- C++ compiler (GCC >= 10, Clang >= 12, MSVC >= 19.29)
- 4-8 GB disk space for vcpkg cache
- 30-60 minutes for first build

## Quick Start

### 1. Install vcpkg

```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg

# Bootstrap vcpkg
~/vcpkg/bootstrap-vcpkg.sh  # Linux/macOS
~/vcpkg/bootstrap-vcpkg.bat # Windows

# Set environment variable
export VCPKG_ROOT="$HOME/vcpkg"
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc  # Make permanent
```

### 2. Build DwarFS

```bash
cd /path/to/dwarfs

# Build all configurations (FlatBuffers-only, both-formats, thrift-only)
./scripts/build-all-and-test.sh --vcpkg

# Or build single configuration manually
export VCPKG_ROOT="$HOME/vcpkg"
cmake -B build-static -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON

cmake --build build-static --target mkdwarfs dwarfsck dwarfsextract -j8
```

## Platform-Specific Triplets

vcpkg uses "triplets" to specify target platform and linkage:

### Standard Triplets

| Triplet | Architecture | Linkage | Use Case |
|---------|--------------|---------|----------|
| `*-static` | All | Static | **Production deployment** (recommended) |
| `*-dynamic` | All | Dynamic | Library distribution |

### Triplet Selection Guide

| Scenario | Recommended Triplet | Reason |
|----------|---------------------|--------|
| **Production deployment** | `*-static` | No runtime dependencies |
| **macOS M1/M2/M3** | `arm64-osx` | Native Apple Silicon |
| **macOS Intel** | `x64-osx` | Intel Mac |
| **Windows standalone** | `x64-windows-static` | Self-contained executable |
| **Linux containers** | `x64-linux` | Static by default |
| **Embedded systems** | `arm64-linux` | ARM Linux static |

## Build Time Expectations

**First Build** (cold cache):
- Dependencies download: 5-10 minutes
- Boost compilation: 15-25 minutes
- Other dependencies: 5-10 minutes
- DwarFS compilation: 5-10 minutes
- **Total**: 30-60 minutes

**Subsequent Builds** (warm cache):
- DwarFS compilation only: 5-10 minutes

## Verifying Static Linking

### macOS
```bash
otool -L build-static/mkdwarfs | grep -v "/usr/lib/system"
# Should show only system libraries or none
```

### Linux
```bash
ldd build-static/mkdwarfs
# Should show only libc, libm, libdl, libpthread, linux-vdso, ld-linux
```

### Windows
```powershell
dumpbin /DEPENDENTS build-static\mkdwarfs.exe
# Should show only kernel32.dll, user32.dll, etc.
```

## Troubleshooting

### Build hangs or is very slow
**Cause**: vcpkg builds all dependencies from source on first run.
**Solution**: Be patient. First build takes 30-60 minutes.

### Out of disk space
```bash
# Clean vcpkg build trees (safe, won't affect installed packages)
$VCPKG_ROOT/vcpkg remove --outdated

# Clean downloads (will require re-download)
rm -rf $VCPKG_ROOT/downloads/*
```

### CMake can't find vcpkg
```bash
export VCPKG_ROOT="$HOME/vcpkg"
# Or specify directly:
cmake -DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" ...
```

## Using Libraries in Your Project

### Example: Minimal CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_dwarfs_app)

set(CMAKE_CXX_STANDARD 20)

# Find DwarFS
find_package(dwarfs REQUIRED CONFIG)

# Your application
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE
  dwarfs::dwarfs_reader
  dwarfs::dwarfs_extractor
)
```

### Building Your Project

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

cmake --build build
```

## Platform-Specific Notes

### macOS
- **Triplets**: `arm64-osx-static` (Apple Silicon), `x64-osx-static` (Intel)
- **Dependencies**: `brew install pkg-config` (required)

### Linux
- **Triplets**: `x64-linux-static`, `arm64-linux-static`
- **FUSE Support**: `sudo apt install libfuse3-dev`

### Windows
- **Triplets**: `x64-windows-static`, `arm64-windows-static`
- **WinFsp**: Required for FUSE driver - install separately at https://winfsp.dev/

## See Also

- [Developer Guide](developer-guide.md) - Development workflow
- [Testing](testing.md) - Test suite documentation
- [Architecture Reference](../reference/architecture.md) - System architecture
