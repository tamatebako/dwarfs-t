# Unified CI/CD Workflow Documentation

## Overview

The [`ci-unified.yml`](workflows/ci-unified.yml:1) workflow provides a DRY (Don't Repeat Yourself), configuration-driven CI/CD pipeline that:

1. **Reads build configurations from [`ci-builds.yaml`](ci-builds.yaml:1)**
2. **Runs all Tier 1 and Tier 2 builds on every PR and merge to main**
3. **Tests all platforms**: Ubuntu, Windows, macOS (x86_64 + aarch64)
4. **Tests metadata formats**: All formats (Thrift+Cereal+Bitsery) and minimal (Bitsery-only)
5. **Includes Windows ARM64** with metadata format testing
6. **Runs benchmark smoke tests** after successful builds
7. **Uploads build artifacts** with proper naming

## Architecture

### Three-Job Pipeline

```
┌─────────────────────┐
│ prepare-matrix      │  Parse ci-builds.yaml into JSON matrices
└──────────┬──────────┘
           │
           ├─────────────────┬─────────────────┐
           ▼                 ▼                 ▼
  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐
  │ tier1-builds   │  │ tier2-builds   │  │                │
  │ (12 configs)   │  │ (4 configs)    │  │                │
  └────────┬───────┘  └────────┬───────┘  │                │
           │                   │           │                │
           └───────────────────┴───────────►  ci-success   │
                                           │  (gate check)  │
                                           └────────────────┘
```

### Configuration-Driven Design

All build logic resides in [`ci-builds.yaml`](ci-builds.yaml:1):

```yaml
tier1_builds:
  ubuntu-x86_64-all-formats:
    name: "Ubuntu x86_64 (All Metadata Formats)"
    runner: ubuntu-24.04
    arch: x86_64
    setup: |
      sudo apt-get update
      sudo apt-get install -y cmake ninja-build ...
    configure: |
      cmake -B build -GNinja ...
    build: |
      cmake --build build --parallel
    test: |
      cd build && ctest --output-on-failure
```

The workflow extracts and executes these commands dynamically.

## Build Matrix

### Tier 1 (12 Configurations - Always Run)

| Platform | Architecture | Metadata Formats | Configuration ID |
|----------|-------------|------------------|------------------|
| Ubuntu 24.04 | x86_64 | All (Thrift+Cereal+Bitsery) | `ubuntu-x86_64-all-formats` |
| Ubuntu 24.04 | x86_64 | Minimal (Bitsery only) | `ubuntu-x86_64-minimal` |
| Ubuntu 24.04 | ARM64 | All (Thrift+Cereal+Bitsery) | `ubuntu-aarch64-all-formats` |
| Ubuntu 24.04 | ARM64 | Minimal (Bitsery only) | `ubuntu-aarch64-minimal` |
| Windows Latest | x64 | All (Thrift+Cereal+Bitsery) | `windows-x86_64-all-formats` |
| Windows Latest | x64 | Minimal (Bitsery only) | `windows-x86_64-minimal` |
| Windows Latest | ARM64 | All (Thrift+Cereal+Bitsery) | `windows-aarch64-all-formats` |
| Windows Latest | ARM64 | Minimal (Bitsery only) | `windows-aarch64-minimal` |
| macOS 13 (Intel) | x86_64 | All (Thrift+Cereal+Bitsery) | `macos-x86_64-all-formats` |
| macOS 13 (Intel) | x86_64 | Minimal (Bitsery only) | `macos-x86_64-minimal` |
| macOS 14 (M-series) | ARM64 | All (Thrift+Cereal+Bitsery) | `macos-aarch64-all-formats` |
| macOS 14 (M-series) | ARM64 | Minimal (Bitsery only) | `macos-aarch64-minimal` |

### Tier 2 (4 Configurations - Always Run)

| Configuration | Purpose | Configuration ID |
|--------------|---------|------------------|
| Ubuntu x86_64 + AddressSanitizer | Memory error detection | `ubuntu-x86_64-asan` |
| Ubuntu x86_64 + Coverage | Code coverage analysis | `ubuntu-x86_64-coverage` |
| Alpine x86_64 Static | Fully static build for portability | `alpine-x86_64-static` |
| FreeBSD x86_64 | BSD compatibility testing | `freebsd-x86_64` |

**Total: 16 build configurations**

## Workflow Steps

### 1. Prepare Matrix Job

```yaml
prepare-matrix:
  - Checkout repository
  - Install yq (YAML parser)
  - Parse tier1_builds from ci-builds.yaml → JSON
  - Parse tier2_builds from ci-builds.yaml → JSON
  - Output matrices for next jobs
```

### 2. Tier 1/Tier 2 Build Jobs

For each configuration in the matrix:

```yaml
build-job:
  - Checkout repository (with submodules)
  - Install yq
  - Extract build scripts from ci-builds.yaml
  - Detect metadata format from build ID
  - Run setup commands
  - Run configure commands
  - Run build commands
  - Run test commands (skip Windows ARM64 cross-builds)
  - Run benchmark smoke test (optional)
  - Upload build artifacts
```

### 3. CI Success Job

```yaml
ci-success:
  - Check all build results
  - Fail if any build failed
  - Report success if all passed
```

## Benchmark Smoke Tests

Each successful build runs a quick benchmark smoke test:

```bash
# Linux/macOS
./build/benchmarks/dwarfs_benchmark \
  --benchmark_min_time=0.1s \
  --benchmark_filter=.*Creation.*

# Windows
./build/benchmarks/Release/dwarfs_benchmark.exe \
  --benchmark_min_time=0.1s \
  --benchmark_filter=.*Creation.*
```

This ensures benchmarks compile and can execute basic operations.

## Artifacts

Build artifacts are uploaded with the naming pattern:

```
dwarfs-{configuration-id}-{git-sha}
```

Examples:
- `dwarfs-ubuntu-x86_64-all-formats-a1b2c3d`
- `dwarfs-windows-aarch64-minimal-a1b2c3d`
- `dwarfs-macos-aarch64-all-formats-a1b2c3d`

Artifacts include:
- `dwarfs` / `dwarfs.exe` - Mount tool
- `mkdwarfs` / `mkdwarfs.exe` - Filesystem creator
- `dwarfsck` / `dwarfsck.exe` - Filesystem checker
- `dwarfsextract` / `dwarfsextract.exe` - Extraction tool

Retention: 7 days

## Trigger Events

The workflow runs on:

1. **Push to main branch**
2. **Pull requests to main branch**
3. **Manual workflow dispatch**

Both Tier 1 and Tier 2 run on all events (per requirements).

## Adding New Build Configurations

To add a new build configuration:

1. Edit [`ci-builds.yaml`](ci-builds.yaml:1)
2. Add entry under `tier1_builds` or `tier2_builds`
3. Specify: `name`, `runner`, `arch`, `setup`, `configure`, `build`, `test`
4. Optional: `container`, `vm`, `coverage`, `verify_static`
5. The workflow automatically picks up the new configuration

Example:

```yaml
tier1_builds:
  ubuntu-x86_64-debug:
    name: "Ubuntu x86_64 (Debug Build)"
    runner: ubuntu-24.04
    arch: x86_64
    setup: |
      sudo apt-get update
      sudo apt-get install -y cmake ninja-build ...
    configure: |
      cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Debug ...
    build: |
      cmake --build build --parallel
    test: |
      cd build && ctest --output-on-failure
```

## Metadata Format Testing

The workflow tests two metadata format configurations:

### All Formats (`*-all-formats`)
```cmake
-DDWARFS_WITH_THRIFT=ON
-DDWARFS_WITH_CEREAL=ON
-DDWARFS_WITH_BITSERY=ON
```

Tests interoperability with legacy Thrift format and modern Cereal/Bitsery formats.

### Minimal (`*-minimal`)
```cmake
-DDWARFS_WITH_THRIFT=OFF
-DDWARFS_WITH_CEREAL=OFF
-DDWARFS_WITH_BITSERY=ON
```

Tests minimal dependency build with fastest serialization (Bitsery only).

## Platform-Specific Notes

### Ubuntu / Linux
- Uses `apt-get` for dependencies
- Native builds on x86_64 and ARM64 runners
- Full test suite execution

### Windows
- Uses `vcpkg` for dependency management
- Visual Studio 2022 generator
- ARM64 builds are cross-compiled (tests skipped)

### macOS
- Uses Homebrew for dependencies
- macOS 13 for x86_64 (Intel)
- macOS 14 for ARM64 (Apple Silicon)
- Architecture specified via `-DCMAKE_OSX_ARCHITECTURES`

### Alpine (Tier 2)
- Container-based build
- Static linking for maximum portability
- Verification step ensures truly static binaries

### FreeBSD (Tier 2)
- VM-based build using `cross-platform-actions`
- Tests BSD compatibility

## Monitoring and Debugging

### View Build Status

GitHub Actions UI shows:
- Overall workflow status
- Individual job status for each configuration
- Real-time logs for each step

### Debug Failed Builds

1. Click on failed job in Actions UI
2. Expand failed step to see error output
3. Check extracted configuration in "Extract build configuration" step
4. Verify commands from ci-builds.yaml are correct

### Local Testing

To test a configuration locally:

```bash
# View configuration
yq eval '.tier1_builds.ubuntu-x86_64-all-formats' .github/ci-builds.yaml

# Extract and run setup
yq eval '.tier1_builds.ubuntu-x86_64-all-formats.setup' .github/ci-builds.yaml | bash

# Extract and run configure
yq eval '.tier1_builds.ubuntu-x86_64-all-formats.configure' .github/ci-builds.yaml | bash

# Extract and run build
yq eval '.tier1_builds.ubuntu-x86_64-all-formats.build' .github/ci-builds.yaml | bash

# Extract and run test
yq eval '.tier1_builds.ubuntu-x86_64-all-formats.test' .github/ci-builds.yaml | bash
```

## Migration from Old Workflows

This unified workflow replaces:
- `ubuntu.yml`
- `windows.yml`
- `macos.yml`
- `alpine.yml`
- `metadata-format-ci.yml`

Benefits:
1. **Single source of truth** - All build logic in ci-builds.yaml
2. **DRY principle** - No duplicated CI definitions
3. **Easy to extend** - Add configurations without touching workflow
4. **Better coverage** - Tests all platforms × architectures × formats
5. **Consistent** - Same steps across all builds

## Future Enhancements

Possible improvements:
1. Add caching for dependencies (vcpkg, Homebrew, apt)
2. Parallel artifact download/testing
3. Performance regression detection from benchmarks
4. Automatic release builds on tags
5. Cross-platform binary compatibility testing