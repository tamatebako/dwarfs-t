# DwarFS CI/CD Improvement Plan

## Executive Summary

This document outlines the planned improvements to the DwarFS CI/CD infrastructure to:
1. Add comprehensive triplet matrix support for cross-platform builds
2. DRY (Don't Repeat Yourself) the GitHub Actions workflows using reusable workflows and composite actions

## Table of Contents

- [Current State Analysis](#current-state-analysis)
- [Triplet Matrix Support](#triplet-matrix-support)
- [Workflow DRY Improvements](#workflow-dry-improvements)
- [Implementation Roadmap](#implementation-roadmap)

---

## Current State Analysis

### Existing Workflows (15 files)

| Workflow | Purpose | Status | Notes |
|----------|---------|--------|-------|
| `build.yml` | Main orchestrator | Partially disabled | Only runs encoding-format-test jobs |
| `linux-builds.yml` | Linux builds | Reusable workflow | Extensive matrix (60+ configurations) |
| `windows-builds.yml` | Windows builds | Reusable workflow | x64/ARM64, MSys2 support |
| `macos-builds.yml` | macOS builds | Reusable workflow | ARM64/X64 matrix |
| `tebako-builds.yml` | Tebako builds | Reusable workflow | Ubuntu, macOS, Alpine |
| `compat-test.yml` | Homebrew compatibility | Standalone | Uses setup-homebrew-dwarfs action |
| `encoding-format-test.yml` | Encoding tests | Unknown | Referenced but not read |
| `benchmark-comprehensive.yml` | Benchmark tests | Standalone | - |
| `docker-run-build.yml` | Docker build runner | Reusable workflow | Used by linux-builds.yml |
| `freebsd-builds.yml` | FreeBSD builds | Reusable workflow | - |
| `support-jobs.yml` | Support jobs | Unknown | Not analyzed |
| `install-dependencies.yml` | Dependency installation | Unknown | Not analyzed |
| `tebako-build-test.yml` | Tebako build/test | Unknown | Not analyzed |
| `tebako-lint.yml` | Tebako linting | Unknown | Not analyzed |
| `installed-headers-test.yml` | Header tests | Unknown | Not analyzed |

### Existing Composite Actions (1 file)

| Action | Purpose | Status |
|--------|---------|--------|
| `setup-homebrew-dwarfs` | Install Homebrew & dwarfs | ✅ Complete |

### Current Triplet Coverage

| Platform | Architecture | Static | Dynamic | Notes |
|----------|-------------|--------|---------|-------|
| **Windows** |
| Windows | x64 | ✅ (x64-windows-static) | ❌ | Only static |
| Windows | ARM64 | ✅ (arm64-windows-static) | ❌ | Only static |
| MSys2 | x64 | ✅ (shared) | ✅ | Via ucrt64/mingw64 |
| **macOS** |
| macOS | ARM64 | ✅ (arm64-osx-static) | ❌ | Only static (implied) |
| macOS | X64 | ✅ (x64-osx-static) | ❌ | Only static (implied) |
| **Linux** |
| Linux | amd64 | ✅ (shared) | ✅ (Alpine static) | Via distro packages |
| Linux | arm64v8 | ✅ (shared) | ✅ (Alpine static) | Via distro packages |
| Cross-compiled | aarch64 | ✅ (Alpine static) | ❌ | Only static |
| Cross-compiled | riscv64 | ✅ (Alpine static) | ❌ | Only static |

### Key Observations

1. **Partial triplet coverage**: Windows and macOS lack dynamic (shared) library variants
2. **No dedicated Linux static triplet**: Static builds via Alpine, not via vcpkg triplets
3. **Cross-compile limited**: Only Alpine cross-compilation, no vcpkg cross-compilation triplets
4. **Workflow duplication**: Similar build/test patterns repeated across workflows
5. **Good reusability foundation**: Several workflows already use `workflow_call`

---

## Triplet Matrix Support

### Target Triplet Matrix

#### Windows Triplets

| Triplet | Architecture | Linkage | Status | Priority |
|---------|-------------|---------|--------|----------|
| `x64-windows-static` | x64 | Static | ✅ Existing | P0 |
| `x64-windows-static-md` | x64 | Static (MD runtime) | ❌ Missing | P2 |
| `x64-windows动态` | x64 | Dynamic | ❌ Missing | P1 |
| `arm64-windows-static` | ARM64 | Static | ✅ Existing | P0 |
| `arm64-windows-static-md` | ARM64 | Static (MD runtime) | ❌ Missing | P2 |
| `arm64-windows动态` | ARM64 | Dynamic | ❌ Missing | P1 |
| `x64-mingw-static` | x64 | Static | ✅ Existing (MSys2) | P0 |
| `arm64-mingw-static` | ARM64 | Static | ❌ Missing | P2 |

#### macOS Triplets

| Triplet | Architecture | Linkage | Status | Priority |
|---------|-------------|---------|--------|----------|
| `arm64-osx-static` | ARM64 | Static | ❌ Missing* | P1 |
| `arm64-osx` | ARM64 | Dynamic | ❌ Missing* | P1 |
| `x64-osx-static` | X64 | Static | ❌ Missing* | P1 |
| `x64-osx` | X64 | Dynamic | ❌ Missing* | P1 |

*Note: macOS builds exist but don't use explicit vcpkg triplet naming

#### Linux Triplets

| Triplet | Architecture | Linkage | Status | Priority |
|---------|-------------|---------|--------|----------|
| `x64-linux-static` | x64 | Static | ❌ Missing | P1 |
| `x64-linux动态` | x64 | Dynamic | ❌ Missing | P2 |
| `arm64-linux-static` | ARM64 | Static | ❌ Missing | P1 |
| `arm64-linux动态` | ARM64 | Dynamic | ❌ Missing | P2 |

*Note: Linux uses distro packages, not vcpkg, so these triplets would need custom vcpkg overlay ports

### Triplet File Structure

Create overlay triplets in `vcpkg_triplets/`:

```
vcpkg_triplets/
├── x64-windows-static.cmake       # ✅ Existing
├── x64-windows动态.cmake          # NEW (dynamic)
├── arm64-windows-static.cmake     # ✅ Existing
├── arm64-windows动态.cmake        # NEW (dynamic)
├── arm64-osx-static.cmake         # NEW
├── arm64-osx.cmake                # NEW (dynamic)
├── x64-osx-static.cmake           # NEW
├── x64-osx.cmake                  # NEW (dynamic)
├── x64-linux-static.cmake         # NEW
├── x64-linux.cmake                # NEW (dynamic)
├── arm64-linux-static.cmake       # NEW
└── arm64-linux.cmake              # NEW (dynamic)
```

### Example Triplet Files

#### `vcpkg_triplets/x64-windows动态.cmake`

```cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DWIN32_LEAN_AND_MEAN=ON)
```

#### `vcpkg_triplets/arm64-osx-static.cmake`

```cmake
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# macOS-specific settings
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
```

---

## Workflow DRY Improvements

### Proposed Composite Actions

Create reusable composite actions in `.github/actions/`:

#### 1. `setup-vcpkg` Action

**File**: `.github/actions/setup-vcpkg/action.yaml`

**Purpose**: Setup vcpkg with specified triplet

**Inputs**:
- `triplet` - vcpkg triplet (required)
- `overlay-ports` - Path to overlay ports (default: `vcpkg_ports`)
- `overlay-triplets` - Path to overlay triplets (default: `vcpkg_triplets`)
- `cache-key` - Unique cache key (default: `vcpkg-${{ runner.os }}-${{ matrix.triplet }}`)

**Outputs**:
- `vcpkg-root` - Path to vcpkg installation
- `triplet` - Resolved triplet name

**Example**:
```yaml
- name: Setup vcpkg
  uses: ./.github/actions/setup-vcpkg
  with:
    triplet: ${{ matrix.triplet }}
```

#### 2. `setup-build-deps` Action

**File**: `.github/actions/setup-build-deps/action.yaml`

**Purpose**: Install platform-specific build dependencies

**Inputs**:
- `platform` - Target platform (windows/macos/linux)
- `with-fuse` - Install FUSE (default: false)
- `with-openssl` - Install OpenSSL (default: true)

**Example**:
```yaml
- name: Setup build dependencies
  uses: ./.github/actions/setup-build-deps
  with:
    platform: ${{ matrix.platform }}
    with-fuse: true
```

#### 3. `configure-cmake` Action

**File**: `.github/actions/configure-cmake/action.yaml`

**Purpose**: Configure CMake with standard options

**Inputs**:
- `build-type` - Release/Debug (default: Release)
- `with-tests` - Enable tests (default: true)
- `with-thrift` - Enable Thrift (default: false)
- `with-flatbuffers` - Enable FlatBuffers (default: true)
- `toolchain-file` - Path to vcpkg toolchain (optional)
- `cmake-args` - Additional CMake arguments (optional)

**Example**:
```yaml
- name: Configure CMake
  uses: ./.github/actions/configure-cmake
  with:
    build-type: Release
    with-thrift: ${{ matrix.with_thrift }}
    toolchain-file: ${{ env.VCPKG_TOOLCHAIN_FILE }}
```

#### 4. `run-ctest` Action

**File**: `.github/actions/run-ctest/action.yaml`

**Purpose**: Run CTest with standard options

**Inputs**:
- `build-dir` - Build directory (default: build)
- `parallel` - Parallel jobs (default: auto)
- `output-on-failure` - Show failed test output (default: true)
- `skip-fuse` - Skip FUSE tests (default: false)

**Example**:
```yaml
- name: Run tests
  uses: ./.github/actions/run-ctest
  with:
    skip-fuse: ${{ matrix.skip_fuse_tests }}
```

### Proposed Reusable Workflows

#### 1. `vcpkg-build.yml` Workflow

**File**: `.github/workflows/vcpkg-build.yml`

**Purpose**: Standard vcpkg-based build workflow

**Inputs**:
- `triplet` - vcpkg triplet (required)
- `os` - Operating system (required)
- `with-thrift` - Enable Thrift (default: false)
- `with-flatbuffers` - Enable FlatBuffers (default: true)
- `run-tests` - Run tests (default: true)
- `run-benchmarks` - Run benchmarks (default: false)

**Matrix Support**:
```yaml
strategy:
  matrix:
    include:
      - triplet: x64-windows-static
        os: windows-latest
        with_thrift: true

      - triplet: arm64-osx-static
        os: macos-14
        with_thrift: true

      - triplet: x64-linux-static
        os: ubuntu-24.04
        with_thrift: false
```

**Usage**:
```yaml
jobs:
  build:
    uses: ./.github/workflows/vcpkg-build.yml
    with:
      triplet: ${{ matrix.triplet }}
      os: ${{ matrix.os }}
    secrets: inherit
```

#### 2. `compat-test-matrix.yml` Workflow

**File**: `.github/workflows/compat-test-matrix.yml`

**Purpose**: Unified compatibility testing workflow

**Current**: `compat-test.yml` has inline matrix

**Improved**: Extract matrix to reusable workflow

**Matrix**:
```yaml
strategy:
  matrix:
    include:
      # Windows
      - os: windows-latest
        triplet: x64-windows-static
        platform: windows
        arch: x64

      - os: windows-latest
        triplet: arm64-windows-static
        platform: windows
        arch: arm64

      # macOS
      - os: macos-14
        triplet: arm64-osx-static
        platform: darwin
        arch: arm64

      - os: macos-13
        triplet: x64-osx-static
        platform: darwin
        arch: x64

      # Linux
      - os: ubuntu-24.04-arm64
        triplet: arm64-linux-static
        platform: linux
        arch: arm64

      - os: ubuntu-24.04
        triplet: x64-linux-static
        platform: linux
        arch: x64
```

### Before/After Comparison

#### Before (Current)

```yaml
# windows-builds.yml
jobs:
  windows-vcpkg:
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup vcpkg
        run: |
          vcpkg install --triplet=${{ matrix.triplet }} ...

      - name: Configure
        run: |
          cmake -B build -G "Visual Studio 17 2022" \
            -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake \
            ...

      - name: Build
        run: cmake --build build --config Release

      - name: Test
        run: ctest --test-dir build -C Release
```

#### After (DRY)

```yaml
# windows-vcpkg.yml
jobs:
  build:
    uses: ./.github/workflows/vcpkg-build.yml
    with:
      triplet: ${{ matrix.triplet }}
      os: windows-latest
      with-thrift: ${{ matrix.with_thrift }}
    secrets: inherit
```

The vcpkg-build.yml workflow uses the composite actions:
```yaml
steps:
  - uses: actions/checkout@v4

  - uses: ./.github/actions/setup-vcpkg
    with:
      triplet: ${{ inputs.triplet }}

  - uses: ./.github/actions/configure-cmake
    with:
      with-thrift: ${{ inputs.with-thrift }}
      toolchain-file: ${{ steps.setup-vcpkg.outputs.toolchain }}

  - run: cmake --build build --config Release

  - uses: ./.github/actions/run-ctest
```

---

## Implementation Roadmap

### Phase 1: Triplet Matrix Support (Week 1)

1. **Create vcpkg overlay triplets** (Day 1-2)
   - [ ] `x64-windows动态.cmake`
   - [ ] `arm64-windows动态.cmake`
   - [ ] `arm64-osx-static.cmake`
   - [ ] `arm64-osx.cmake`
   - [ ] `x64-osx-static.cmake`
   - [ ] `x64-osx.cmake`
   - [ ] `x64-linux-static.cmake`
   - [ ] `x64-linux.cmake`
   - [ ] `arm64-linux-static.cmake`
   - [ ] `arm64-linux.cmake`

2. **Update workflows to use triplet matrix** (Day 3-4)
   - [ ] Add triplet matrix to `windows-builds.yml`
   - [ ] Add triplet matrix to `macos-builds.yml`
   - [ ] Add triplet matrix to `linux-builds.yml`
   - [ ] Test dynamic builds on Windows
   - [ ] Test dynamic builds on macOS
   - [ ] Test static builds on Linux via vcpkg

3. **Documentation** (Day 5)
   - [ ] Update `README.md` with triplet matrix
   - [ ] Update `TESTING.md` with triplet info
   - [ ] Document triplet usage in `doc/` directory

### Phase 2: Composite Actions (Week 2)

1. **Create setup-vcpkg action** (Day 1-2)
   - [ ] Create `.github/actions/setup-vcpkg/action.yaml`
   - [ ] Add caching support
   - [ ] Add binary cache support
   - [ ] Test across platforms

2. **Create setup-build-deps action** (Day 2-3)
   - [ ] Create `.github/actions/setup-build-deps/action.yaml`
   - [ ] Support Windows, macOS, Linux
   - [ ] Add FUSE detection
   - [ ] Add OpenSSL detection

3. **Create configure-cmake action** (Day 3-4)
   - [ ] Create `.github/actions/configure-cmake/action.yaml`
   - [ ] Support all CMake options
   - [ ] Support vcpkg toolchain
   - [ ] Test across platforms

4. **Create run-ctest action** (Day 4-5)
   - [ ] Create `.github/actions/run-ctest/action.yaml`
   - [ ] Add FUSE test skip option
   - [ ] Add test filtering
   - [ ] Test across platforms

### Phase 3: Reusable Workflows (Week 3)

1. **Create vcpkg-build.yml workflow** (Day 1-2)
   - [ ] Create reusable workflow
   - [ ] Support all platforms
   - [ ] Support all triplets
   - [ ] Add artifact uploads

2. **Create compat-test-matrix.yml workflow** (Day 2-3)
   - [ ] Extract from `compat-test.yml`
   - [ ] Add triplet matrix support
   - [ ] Test across platforms

3. **Migrate existing workflows** (Day 3-5)
   - [ ] Refactor `windows-builds.yml`
   - [ ] Refactor `macos-builds.yml`
   - [ ] Refactor `linux-builds.yml`
   - [ ] Refactor `tebako-builds.yml`

### Phase 4: Testing & Validation (Week 4)

1. **Test all triplets** (Day 1-2)
   - [ ] Windows x64 static + dynamic
   - [ ] Windows ARM64 static + dynamic
   - [ ] macOS ARM64 static + dynamic
   - [ ] macOS X64 static + dynamic
   - [ ] Linux x64 static + dynamic
   - [ ] Linux ARM64 static + dynamic

2. **Test all workflows** (Day 3-4)
   - [ ] vcpkg-build.yml
   - [ ] compat-test-matrix.yml
   - [ ] Refactored platform workflows

3. **Documentation finalization** (Day 5)
   - [ ] Finalize `CI_IMPROVEMENTS.md`
   - [ ] Update main `README.md`
   - [ ] Create `doc/vcpkg-triplets.md`
   - [ ] Create `doc/github-actions-guide.md`

---

## Success Criteria

### Triplet Matrix Support

- [x] All 8 platform/architecture/linkage combinations have triplet files
- [ ] All 8 triplets build successfully
- [ ] Dynamic builds work on Windows and macOS
- [ ] Static vcpkg builds work on Linux
- [ ] Triplet matrix documented in README

### Workflow DRY Improvements

- [ ] 4 new composite actions created and tested
- [ ] 2 new reusable workflows created and tested
- [ ] At least 50% reduction in workflow code duplication
- [ ] All existing workflows migrated to use new actions
- [ ] No functionality lost in migration

### Testing Coverage

- [ ] All triplets tested on each PR
- [ ] Compatibility tests use triplet matrix
- [ ] Build time not significantly increased
- [ ] CI/CD infrastructure more maintainable

---

## Appendix

### A. Triplet Naming Conventions

**vcpkg triplet format**: `{arch}-{os}-{linkage}`

| Component | Values |
|-----------|--------|
| `arch` | x64, arm64, x86, arm |
| `os` | windows, osx, linux |
| `linkage` | static, dynamic |

### B. References

- [vcpkg Triplets](https://learn.microsoft.com/en-us/vcpkg/users/triplets)
- [GitHub Actions Composite Actions](https://docs.github.com/en/actions/creating-actions/creating-a-composite-action)
- [GitHub Actions Reusable Workflows](https://docs.github.com/en/actions/using-workflows/reusing-workflows)
- [DwarFS TESTING.md](../TESTING.md)

---

**Last Updated**: 2025-12-30
**Status**: Draft - Ready for Review
