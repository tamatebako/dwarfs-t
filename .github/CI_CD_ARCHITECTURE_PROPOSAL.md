# DwarFS CI/CD Architecture Proposal

## Executive Summary

This proposal defines a **MECE, DRY, comprehensive, and pipelined** CI/CD architecture for DwarFS. The current state has fragmented workflows with significant duplication. This architecture consolidates everything into a clear, maintainable structure.

**Current Issues:**
- 19 workflow files with significant duplication
- `ci.yml` and `vcpkg-triplet-matrix.yml` duplicate build logic
- `encoding-format-test.yml` uses different vcpkg setup (`lukka/run-vcpkg`)
- `build.yml` is 90% disabled/deprecated
- Inconsistent matrix strategies across workflows

**Target State:**
- 5 core workflow files (MECE categories)
- 1 reusable build workflow (DRY)
- 5 composite actions (already exist, need standardization)
- Clear pipelined flow from PR → Merge → Release

---

## Architecture Overview

### MECE Workflow Categories

```
┌─────────────────────────────────────────────────────────────────┐
│                     CI/CD Architecture                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  CONTINUOUS INTEGRATION (CI)                               │ │
│  │  • pr-validation.yml    (PR validation, fast feedback)    │ │
│  │  • ci-main.yml         (Main branch, comprehensive)       │ │
│  └────────────────────────────────────────────────────────────┘ │
│                         ↓                                       │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  CONTINUOUS DELIVERY (CD)                                  │ │
│  │  • release.yml         (Release artifacts & publishing)   │ │
│  └────────────────────────────────────────────────────────────┘ │
│                         ↓                                       │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │  SCHEDULED & MANUAL                                        │ │
│  │  • scheduled.yml       (Nightly/weekly comprehensive)     │ │
│  │  • manual.yml          (On-demand workflows)              │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### DRY Implementation Layers

```
┌─────────────────────────────────────────────────────────────────┐
│                    Workflow Layer (5 files)                     │
│  pr-validation.yml, ci-main.yml, release.yml,                  │
│  scheduled.yml, manual.yml                                      │
└────────────────────────────┬────────────────────────────────────┘
                             │
┌────────────────────────────┴────────────────────────────────────┐
│                    Reusable Workflow Layer (1 file)             │
│  _build-test-reusable.yml (UNIFIED build/test workflow)        │
└────────────────────────────┬────────────────────────────────────┘
                             │
┌────────────────────────────┴────────────────────────────────────┐
│                    Composite Action Layer (5 actions)           │
│  setup-vcpkg, configure-cmake, setup-build-deps,               │
│  run-ctest, setup-homebrew-dwarfs                              │
└─────────────────────────────────────────────────────────────────┘
```

---

## MECE Workflow Specifications

### 1. CONTINUOUS INTEGRATION (CI)

#### 1.1 PR Validation Workflow (`pr-validation.yml`)

**Purpose**: Fast feedback on pull requests

**Trigger**: Pull request to main/feature branches

**Scope**: **Minimal but critical** tests only (target: < 15 minutes)

**Matrix**:
- **Platforms**: Ubuntu-latest (x64), macos-14 (arm64)
- **Configs**: flatbuffers-only, both-formats
- **Total Jobs**: 4

**Tests**:
- ✅ Build verification
- ✅ Unit tests (metadata, serialization)
- ✅ Basic smoke tests

**Does NOT include** (for speed):
- ❌ Full compatibility tests
- ❌ Benchmarks
- ❌ Windows builds (slower)
- ❌ All platform variants

**File**: `.github/workflows/pr-validation.yml`

```yaml
name: PR Validation

on:
  pull_request:
    branches: [main, feature/*]
  workflow_dispatch:

permissions:
  contents: read

jobs:
  validate:
    strategy:
      fail-fast: true  # Fail fast on PRs
      matrix:
        include:
          # Fast x64 builds only
          - runner: ubuntu-latest
            triplet: x64-linux
            config: flatbuffers-only
          - runner: ubuntu-latest
            triplet: x64-linux
            config: both-formats
          - runner: macos-14
            triplet: arm64-osx
            config: flatbuffers-only
          - runner: macos-14
            triplet: arm64-osx
            config: both-formats

    uses: ./.github/workflows/_build-test-reusable.yml
    with:
      runner: ${{ matrix.runner }}
      triplet: ${{ matrix.triplet }}
      config: ${{ matrix.config }}
      build-type: Release
      cache-key: pr-validation-${{ matrix.runner }}
```

#### 1.2 Main CI Workflow (`ci-main.yml`)

**Purpose**: Comprehensive validation for main branch

**Trigger**: Push to main, manual dispatch

**Scope**: **Full matrix** across all platforms and configurations

**Matrix**:
- **Platforms**: 5 (Ubuntu x64/ARM64, macOS x64/ARM64, Windows x64)
- **Configs**: flatbuffers-only, both-formats
- **Total Jobs**: 10

**Tests**:
- ✅ All unit tests
- ✅ Integration tests
- ✅ Compatibility tests (macOS only)

**File**: `.github/workflows/ci-main.yml`

```yaml
name: CI Main

on:
  push:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: read

jobs:
  # Full platform matrix
  build-and-test:
    strategy:
      fail-fast: false
      matrix:
        include:
          - runner: ubuntu-latest
            triplet: x64-linux
            config: flatbuffers-only
          - runner: ubuntu-latest
            triplet: x64-linux
            config: both-formats
          - runner: ubuntu-24.04-arm64
            triplet: arm64-linux
            config: flatbuffers-only
          - runner: ubuntu-24.04-arm64
            triplet: arm64-linux
            config: both-formats
          - runner: macos-13
            triplet: x64-osx
            config: flatbuffers-only
          - runner: macos-13
            triplet: x64-osx
            config: both-formats
          - runner: macos-14
            triplet: arm64-osx
            config: flatbuffers-only
          - runner: macos-14
            triplet: arm64-osx
            config: both-formats
          - runner: windows-latest
            triplet: x64-windows-static
            config: flatbuffers-only
          - runner: windows-latest
            triplet: x64-windows-static
            config: both-formats

    uses: ./.github/workflows/_build-test-reusable.yml
    with:
      runner: ${{ matrix.runner }}
      triplet: ${{ matrix.triplet }}
      config: ${{ matrix.config }}
      build-type: Release
      cache-key: ci-main-${{ matrix.runner }}-${{ matrix.triplet }}

  # Compatibility tests (after main builds)
  compat-tests:
    needs: build-and-test
    uses: ./.github/workflows/_compat-test-reusable.yml
```

### 2. CONTINUOUS DELIVERY (CD)

#### 2.1 Release Workflow (`release.yml`)

**Purpose**: Create and publish release artifacts

**Trigger**: Git tags (v*), manual dispatch

**Scope**: **Release artifact creation** for all platforms

**Matrix**:
- **Platforms**: 5 (Ubuntu x64/ARM64, macOS x64/ARM64, Windows x64)
- **Configs**: flatbuffers-only (for production)
- **Total Jobs**: 5 (build) + 1 (publish)

**Artifacts**:
- ✅ Static binaries (mkdwarfs, dwarfsck, dwarfsextract, dwarfs)
- ✅ Libraries (static + shared)
- ✅ Headers
- ✅ Release notes

**File**: `.github/workflows/release.yml`

```yaml
name: Release

on:
  push:
    tags:
      - 'v*'
  workflow_dispatch:
    inputs:
      version:
        description: 'Release version (e.g., v0.17.0)'
        required: true

permissions:
  contents: write

jobs:
  # Build release binaries for all platforms
  build-release:
    strategy:
      fail-fast: false
      matrix:
        include:
          - runner: ubuntu-latest
            triplet: x64-linux
            artifact: dwarfs-linux-x64
          - runner: ubuntu-24.04-arm64
            triplet: arm64-linux
            artifact: dwarfs-linux-arm64
          - runner: macos-13
            triplet: x64-osx
            artifact: dwarfs-macos-x64
          - runner: macos-14
            triplet: arm64-osx
            artifact: dwarfs-macos-arm64
          - runner: windows-latest
            triplet: x64-windows-static
            artifact: dwarfs-windows-x64

    uses: ./.github/workflows/_build-release-reusable.yml
    with:
      runner: ${{ matrix.runner }}
      triplet: ${{ matrix.triplet }}
      artifact-name: ${{ matrix.artifact }}
      config: flatbuffers-only

  # Create GitHub release
  publish-release:
    needs: build-release
    runs-on: ubuntu-latest
    steps:
      - name: Download all artifacts
        uses: actions/download-artifact@v4
        with:
          path: release-artifacts

      - name: Create GitHub Release
        uses: softprops/action-gh-release@v2
        with:
          files: release-artifacts/**/*.tar.gz
          generate_release_notes: true
```

### 3. SCHEDULED & MANUAL WORKFLOWS

#### 3.1 Scheduled Workflow (`scheduled.yml`)

**Purpose**: Comprehensive tests that take too long for PR validation

**Trigger**: Weekly schedule (Sundays 2 AM UTC), manual dispatch

**Scope**: **Comprehensive testing** including benchmarks and compatibility

**Matrix**:
- **All 12 vcpkg triplets** (static + dynamic variants)
- **2 configurations** (flatbuffers-only, both-formats)
- **Total Jobs**: 24+

**Tests**:
- ✅ Comprehensive unit tests
- ✅ Compatibility tests (Homebrew)
- ✅ Benchmarks (optional, controlled by input)

**File**: `.github/workflows/scheduled.yml`

```yaml
name: Scheduled Comprehensive Tests

on:
  schedule:
    - cron: '0 2 * * 0'  # Weekly Sunday 2 AM UTC
  workflow_dispatch:
    inputs:
      run-benchmarks:
        description: 'Run benchmarks (2-3 hours)'
        type: boolean
        default: false
      triplet-filter:
        description: 'Specific triplet to test (empty = all)'
        type: string

permissions:
  contents: read

jobs:
  setup-matrix:
    runs-on: ubuntu-latest
    outputs:
      matrix: ${{ steps.set-matrix.outputs.matrix }}
    steps:
      - id: set-matrix
        run: |
          if [ -n "${{ inputs.triplet-filter }}" ]; then
            MATRIX='[{"triplet":"${{ inputs.triplet-filter }}"}]'
          else
            MATRIX='[
              {"triplet":"x64-linux","os":"ubuntu-latest"},
              {"triplet":"arm64-linux","os":"ubuntu-24.04-arm64"},
              {"triplet":"x64-linux-dynamic","os":"ubuntu-latest"},
              {"triplet":"arm64-linux-dynamic","os":"ubuntu-24.04-arm64"},
              {"triplet":"x64-osx","os":"macos-13"},
              {"triplet":"arm64-osx","os":"macos-14"},
              {"triplet":"x64-osx-dynamic","os":"macos-13"},
              {"triplet":"arm64-osx-dynamic","os":"macos-14"},
              {"triplet":"x64-windows-static","os":"windows-latest"},
              {"triplet":"x64-windows-dynamic","os":"windows-latest"},
              {"triplet":"arm64-windows-static","os":"windows-latest"},
              {"triplet":"arm64-windows-dynamic","os":"windows-latest"}
            ]'
          fi
          echo "matrix=$MATRIX" >> $GITHUB_OUTPUT

  comprehensive-test:
    needs: setup-matrix
    strategy:
      fail-fast: false
      matrix:
        include: ${{ fromJson(needs.setup-matrix.outputs.matrix) }}
        config: [flatbuffers-only, both-formats]

    uses: ./.github/workflows/_build-test-reusable.yml
    with:
      runner: ${{ matrix.os }}
      triplet: ${{ matrix.triplet }}
      config: ${{ matrix.config }}

  benchmarks:
    if: inputs.run-benchmarks == true
    needs: comprehensive-test
    uses: ./.github/workflows/_benchmark-reusable.yml
```

#### 3.2 Manual Workflow (`manual.yml`)

**Purpose**: On-demand workflows for special cases

**Trigger**: Manual dispatch only

**Capabilities**:
- Run specific triplet
- Run specific configuration
- Run benchmarks only
- Run compatibility tests only

**File**: `.github/workflows/manual.yml`

```yaml
name: Manual Workflows

on:
  workflow_dispatch:
    inputs:
      workflow:
        description: 'Workflow type'
        required: true
        type: choice
        options:
          - benchmark
          - compat
          - triplet-test
      triplet:
        description: 'vcpkg triplet'
        type: string
      config:
        description: 'Build configuration'
        type: choice
        options:
          - flatbuffers-only
          - both-formats
        default: flatbuffers-only

jobs:
  manual:
    uses: ./.github/workflows/_build-test-reusable.yml
    with:
      runner: ubuntu-latest
      triplet: ${{ inputs.triplet || 'x64-linux' }}
      config: ${{ inputs.config }}
```

---

## DRY Implementation: Reusable Workflows

### Unified Build/Test Reusable Workflow

**File**: `.github/workflows/_build-test-reusable.yml` (ENHANCED)

**Current State**: Exists but not fully utilized

**Enhancements**:
1. Add better caching strategy
2. Add artifact upload options
3. Add test result reporting

```yaml
name: Reusable Build and Test

on:
  workflow_call:
    inputs:
      runner:
        description: 'GitHub Actions runner'
        required: true
        type: string
      triplet:
        description: 'vcpkg triplet'
        required: true
        type: string
      config:
        description: 'Build configuration'
        required: true
        type: string
      build-type:
        description: 'CMake build type'
        type: string
        default: Release
      cache-key:
        description: 'Cache key prefix'
        type: string
        default: vcpkg
      upload-artifacts:
        description: 'Upload build artifacts'
        type: boolean
        default: false
      with-fuse:
        description: 'Enable FUSE driver'
        type: boolean
        default: false

    outputs:
      artifact-name:
        description: 'Generated artifact name'
        value: ${{ jobs.build.outputs.artifact-name }}

jobs:
  build:
    runs-on: ${{ inputs.runner }}
    outputs:
      artifact-name: ${{ steps.generate-artifact-name.outputs.name }}

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup vcpkg
        uses: ./.github/actions/setup-vcpkg
        with:
          triplet: ${{ inputs.triplet }}
          cache-key: ${{ inputs.cache-key }}

      - name: Setup build dependencies
        uses: ./.github/actions/setup-build-deps
        with:
          platform: ${{ inputs.runner }}
          with-fuse: ${{ inputs.with-fuse }}

      - name: Configure - FlatBuffers Only
        if: inputs.config == 'flatbuffers-only'
        uses: ./.github/actions/configure-cmake
        with:
          build-dir: build-${{ inputs.config }}
          build-type: ${{ inputs.build-type }}
          with-tests: true
          with-thrift: false
          with-flatbuffers: true

      - name: Configure - Both Formats
        if: inputs.config == 'both-formats'
        uses: ./.github/actions/configure-cmake
        with:
          build-dir: build-${{ inputs.config }}
          build-type: ${{ inputs.build-type }}
          with-tests: true
          with-thrift: true
          with-flatbuffers: true

      - name: Build
        shell: bash
        run: |
          if [[ "$RUNNER_OS" == "Windows" ]]; then
            cmake --build build-${{ inputs.config }} \
              --config ${{ inputs.build-type }} --parallel
          else
            cmake --build build-${{ inputs.config }} --parallel
          fi

      - name: Generate artifact name
        id: generate-artifact-name
        run: |
          ARTIFACT="dwarfs-${{ inputs.triplet }}-${{ inputs.config }}"
          echo "name=$ARTIFACT" >> $GITHUB_OUTPUT

      - name: Upload artifacts
        if: inputs.upload-artifacts == true
        uses: actions/upload-artifact@v4
        with:
          name: ${{ steps.generate-artifact-name.outputs.name }}
          path: |
            build-${{ inputs.config }}/mkdwarfs*
            build-${{ inputs.config }}/dwarfs*
            build-${{ inputs.config }}/dwarfsextract*
          if-no-files-found: warn

      - name: Run tests
        uses: ./.github/actions/run-ctest
        with:
          build-dir: build-${{ inputs.config }}
          config: ${{ inputs.build-type }}

      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-results-${{ inputs.triplet }}-${{ inputs.config }}
          path: |
            build-${{ inputs.config }}/Testing/
            build-${{ inputs.config }}/**/Test.xml
          if-no-files-found: warn
```

### Build Release Reusable Workflow (NEW)

**File**: `.github/workflows/_build-release-reusable.yml`

**Purpose**: Create release-ready binaries with packaging

```yaml
name: Reusable Release Build

on:
  workflow_call:
    inputs:
      runner:
        required: true
        type: string
      triplet:
        required: true
        type: string
      artifact-name:
        required: true
        type: string
      config:
        type: string
        default: flatbuffers-only

    outputs:
      artifact-path:
        value: ${{ jobs.package.outputs.artifact-path }}

jobs:
  build:
    runs-on: ${{ inputs.runner }}

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup vcpkg
        uses: ./.github/actions/setup-vcpkg
        with:
          triplet: ${{ inputs.triplet }}

      - name: Setup build dependencies
        uses: ./.github/actions/setup-build-deps
        with:
          platform: ${{ inputs.runner }}

      - name: Configure
        uses: ./.github/actions/configure-cmake
        with:
          build-dir: build-release
          build-type: Release
          with-tests: false
          with-libdwarfs: true
          with-tools: true
          with-fuse-driver: false
          with-thrift: ${{ inputs.config == 'both-formats' }}
          with-flatbuffers: true

      - name: Build
        shell: bash
        run: |
          if [[ "$RUNNER_OS" == "Windows" ]]; then
            cmake --build build-release --config Release --parallel
          else
            cmake --build build-release --parallel
          fi

      - name: Strip binaries (Linux/macOS)
        if: runner.os != 'Windows'
        run: |
          strip build-release/mkdwarfs/mkdwarfs || true
          strip build-release/dwarfs/dwarfs || true
          strip build-release/dwarfsextract/dwarfsextract || true
          strip build-release/dwarfs/dwarfsck || true

      - name: Package release
        shell: bash
        run: |
          mkdir -p package

          # Copy binaries
          cp build-release/tools/mkdwarfs/mkdwarfs* package/ 2>/dev/null || true
          cp build-release/tools/dwarfs/dwarfs* package/ 2>/dev/null || true
          cp build-release/tools/dwarfsextract/dwarfsextract* package/ 2>/dev/null || true
          cp build-release/tools/dwarfs/dwarfsck* package/ 2>/dev/null || true

          # Copy libraries (if built)
          cp build-release/libdwarfs/*.a package/ 2>/dev/null || true
          cp build-release/libdwarfs/*.so* package/ 2>/dev/null || true
          cp build-release/libdwarfs/*.dylib* package/ 2>/dev/null || true
          cp build-release/libdwarfs/*.dll* package/ 2>/dev/null || true

          # Copy headers
          if [[ -d build-release/include ]]; then
            cp -r build-release/include package/
          fi

          # Create tarball
          tar czf ${{ inputs.artifact-name }}.tar.gz -C package .

      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: ${{ inputs.artifact-name }}
          path: ${{ inputs.artifact-name }}.tar.gz

  package:
    needs: build
    runs-on: ubuntu-latest
    outputs:
      artifact-path: ${{ steps.output.outputs.path }}
    steps:
      - id: output
        run: echo "path=${{ inputs.artifact-name }}.tar.gz" >> $GITHUB_OUTPUT
```

### Compatibility Test Reusable Workflow (NEW)

**File**: `.github/workflows/_compat-test-reusable.yml`

**Purpose**: Test compatibility with Homebrew dwarfs

```yaml
name: Reusable Compatibility Test

on:
  workflow_call:
    inputs:
      runner:
        type: string
        default: macos-14

jobs:
  compat-test:
    runs-on: ${{ inputs.runner }}

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup Homebrew dwarfs
        uses: ./.github/actions/setup-homebrew-dwarfs

      - name: Setup vcpkg
        uses: ./.github/actions/setup-vcpkg
        with:
          triplet: ${{ runner.os == 'macOS-14' && 'arm64-osx' || 'x64-osx' }}

      - name: Setup build dependencies
        uses: ./.github/actions/setup-build-deps
        with:
          platform: ${{ inputs.runner }}

      - name: Configure with compat tests
        uses: ./.github/actions/configure-cmake
        with:
          build-dir: build-compat
          build-type: Release
          with-tests: true
          with-thrift: true
          with-flatbuffers: true
          cmake-args: -DWITH_COMPAT_TESTS=ON

      - name: Build compat tests
        run: cmake --build build-compat --target dwarfs-compat-lib -j$(sysctl -n hw.ncpu || nproc)

      - name: Run compat tests
        working-directory: test/compat
        run: |
          if [[ -f "./scripts/run_tests.sh" ]]; then
            ./scripts/run_tests.sh --all --format json
          else
            ../build-compat/test/compat/read_homebrew_files_test
          fi

      - name: Upload results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: compat-results-${{ inputs.runner }}
          path: |
            test/compat/results-*.json
            build-compat/Testing/
```

### Benchmark Reusable Workflow (NEW)

**File**: `.github/workflows/_benchmark-reusable.yml`

**Purpose**: Run comprehensive benchmarks

```yaml
name: Reusable Benchmark

on:
  workflow_call:
    inputs:
      runner:
        type: string
        default: ubuntu-latest
      triplet:
        type: string
        default: x64-linux

jobs:
  benchmark:
    runs-on: ${{ inputs.runner }}
    timeout-minutes: 180  # 3 hours

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Setup vcpkg
        uses: ./.github/actions/setup-vcpkg
        with:
          triplet: ${{ inputs.triplet }}

      - name: Setup build dependencies
        uses: ./.github/actions/setup-build-deps
        with:
          platform: ${{ inputs.runner }}

      - name: Build FlatBuffers-only
        uses: ./.github/actions/configure-cmake
        with:
          build-dir: build-fb-bench
          build-type: Release
          with-benchmarks: true
          with-thrift: false
          with-flatbuffers: true

      - name: Build Both-formats
        uses: ./.github/actions/configure-cmake
        with:
          build-dir: build-both-bench
          build-type: Release
          with-benchmarks: true
          with-thrift: true
          with-flatbuffers: true

      - name: Build
        run: |
          cmake --build build-fb-bench --parallel
          cmake --build build-both-bench --parallel

      - name: Download benchmark dataset
        run: |
          mkdir -p benchmark-files
          curl -L https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz | tar -xz -C benchmark-files

      - name: Run benchmarks
        run: ./benchmarks/run_comprehensive_benchmark.sh

      - name: Upload benchmark results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-results-${{ inputs.triplet }}
          path: results/
```

---

## Pipelined Flow

### Dependency Graph

```
PR Validation (Fast)        CI Main (Comprehensive)
       │                              │
       ├── Build + Test (4 jobs)      ├── Build + Test (10 jobs)
       │                              │
       └── Success → Merge allowed    ├── Compatibility Tests
                                      │
                                      │
                         Release (on tag)
                              │
                              ├── Build Release (5 jobs)
                              │
                              └── Publish Release

                         Scheduled (Weekly)
                              │
                              ├── Comprehensive Test (24 jobs)
                              │
                              └── Benchmarks (optional)
```

### Trigger Conditions

| Event | Workflow | Purpose | Runtime |
|-------|----------|---------|---------|
| **PR opened/updated** | `pr-validation.yml` | Fast feedback | ~15 min |
| **Push to main** | `ci-main.yml` | Full validation | ~45 min |
| **Tag pushed (v*)** | `release.yml` | Create release | ~30 min |
| **Weekly schedule** | `scheduled.yml` | Comprehensive + benchmarks | ~2-3 hours |
| **Manual dispatch** | `manual.yml` | On-demand testing | Variable |

---

## Migration Plan

### Phase 1: Create New Reusable Workflows (Week 1)

1. ✅ Create `_build-release-reusable.yml`
2. ✅ Create `_compat-test-reusable.yml`
3. ✅ Create `_benchmark-reusable.yml`
4. ✅ Enhance `_build-test-reusable.yml`

### Phase 2: Create New User-Facing Workflows (Week 1-2)

1. ✅ Create `pr-validation.yml`
2. ✅ Create `ci-main.yml`
3. ✅ Create `release.yml`
4. ✅ Create `scheduled.yml`
5. ✅ Create `manual.yml`

### Phase 3: Deprecate Old Workflows (Week 2)

**Files to DELETE:**
- ❌ `build.yml` (90% disabled, replace with ci-main.yml)
- ❌ `build-test.yml` (superseded by pr-validation.yml)
- ❌ `linux-builds.yml`, `macos-builds.yml`, `windows-builds.yml` (merged into ci-main.yml)
- ❌ `install-dependencies.yml` (now composite action)

**Files to KEEP (specialized):**
- ✅ `benchmark-comprehensive.yml` (for manual benchmark runs)
- ✅ `encoding-format-test.yml` (for encoding validation)
- ✅ `tebako-build*.yml` (Tebako-specific, keep separate)
- ✅ `support-jobs.yml` (specialized jobs)

### Phase 4: Update Documentation (Week 2)

1. Update `TESTING.md` with new workflow structure
2. Update `README.md` CI status badges
3. Create `.github/README.md` for CI/CD documentation

### Phase 5: Validation (Week 2)

1. Test all workflows with `workflow_dispatch`
2. Verify matrix coverage
3. Confirm artifact generation
4. Validate release flow

---

## Composite Actions Inventory

### Existing Actions (Keep & Standardize)

| Action | Purpose | Status |
|--------|---------|--------|
| `setup-vcpkg` | vcpkg installation with overlay ports | ✅ Active |
| `configure-cmake` | Standardized CMake configuration | ✅ Active |
| `setup-build-deps` | Platform-specific dependencies | ✅ Active |
| `run-ctest` | Test execution with CTest | ✅ Active |
| `setup-homebrew-dwarfs` | Homebrew dwarfs setup | ✅ Active |

### Standardization Requirements

1. **All workflows MUST use `setup-vcpkg` action** (no `lukka/run-vcpkg`)
2. **All workflows MUST use `configure-cmake` action** (no inline CMake)
3. **Consistent cache keys** across all workflows
4. **Consistent artifact naming** (dwarfs-{triplet}-{config})

---

## Comprehensive Coverage Matrix

### Platform Coverage

| Platform | Runner | PR Validation | CI Main | Release | Scheduled |
|----------|--------|---------------|---------|---------|-----------|
| Ubuntu x64 | ubuntu-latest | ✅ | ✅ | ✅ | ✅ |
| Ubuntu ARM64 | ubuntu-24.04-arm64 | ❌ | ✅ | ✅ | ✅ |
| macOS x64 | macos-13 | ❌ | ✅ | ✅ | ✅ |
| macOS ARM64 | macos-14 | ✅ | ✅ | ✅ | ✅ |
| Windows x64 | windows-latest | ❌ | ✅ | ✅ | ✅ |

### Configuration Coverage

| Config | PR Validation | CI Main | Release | Scheduled |
|--------|---------------|---------|---------|-----------|
| FlatBuffers-only | ✅ | ✅ | ✅ | ✅ |
| Both-formats | ✅ | ✅ | ❌ | ✅ |

### Test Coverage

| Test Type | PR Validation | CI Main | Scheduled |
|-----------|---------------|---------|-----------|
| Unit Tests | ✅ | ✅ | ✅ |
| Integration Tests | ❌ | ✅ | ✅ |
| Compatibility Tests | ❌ | ✅ | ✅ |
| Benchmarks | ❌ | ❌ | ✅ (optional) |

---

## Success Metrics

### Before (Current State)

| Metric | Value |
|--------|-------|
| Workflow Files | 19 |
| Duplicated Build Logic | 3+ locations |
| PR Validation Time | Unknown (full matrix) |
| Maintenance Burden | High |

### After (Target State)

| Metric | Value |
|--------|-------|
| Core Workflow Files | 5 |
| Reusable Workflows | 4 |
| Composite Actions | 5 |
| PR Validation Time | < 15 minutes |
| Code Duplication | 0% |
| Maintenance Burden | Low |

---

## Implementation Checklist

- [ ] Phase 1: Create reusable workflows
  - [ ] `_build-release-reusable.yml`
  - [ ] `_compat-test-reusable.yml`
  - [ ] `_benchmark-reusable.yml`
  - [ ] Enhance `_build-test-reusable.yml`
- [ ] Phase 2: Create user-facing workflows
  - [ ] `pr-validation.yml`
  - [ ] `ci-main.yml`
  - [ ] `release.yml`
  - [ ] `scheduled.yml`
  - [ ] `manual.yml`
- [ ] Phase 3: Deprecate old workflows
  - [ ] Delete `build.yml`
  - [ ] Delete `build-test.yml`
  - [ ] Delete platform-specific build workflows
- [ ] Phase 4: Update documentation
  - [ ] Update `TESTING.md`
  - [ ] Update `README.md`
  - [ ] Create `.github/README.md`
- [ ] Phase 5: Validate
  - [ ] Test all workflows
  - [ ] Verify matrix coverage
  - [ ] Validate release flow

---

## Conclusion

This architecture achieves:

1. **MECE**: Clear separation between CI, CD, Scheduled, and Manual workflows
2. **DRY**: Single reusable workflow for build/test, composite actions for setup
3. **Comprehensive**: All platforms, configurations, and test types covered
4. **Pipelined**: Clear flow from PR → Merge → Release with proper dependencies

The system is maintainable, scalable, and provides fast feedback for developers while ensuring comprehensive validation for the main branch.
