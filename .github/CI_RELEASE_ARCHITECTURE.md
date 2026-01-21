# CI and Release Workflow Architecture

## Overview

This document describes the comprehensive CI/CD architecture for DwarFS, designed to be:
- **MECE**: Mutually Exclusive, Collectively Exhaustive - every scenario covered exactly once
- **DRY**: Don't Repeat Yourself - shared logic, no duplication
- **Comprehensive**: Covers all platforms, configurations, and quality gates
- **Pipelined**: Parallel execution where possible, gated by dependencies

## Architecture Principles

### 1. Pipeline Stages

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CI/CD Pipeline                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────┐ │
│  │   Lint &     │ -> │   Build &    │ -> │   Test &     │ -> │  Release │ │
│  │   Format    │    │   Compile   │    │   Verify    │    │          │ │
│  └──────────────┘    └──────────────┘    └──────────────┘    └──────────┘ │
│         │                   │                   │                │         │
│         v                   v                   v                v         │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │                  Quality Gates (All Must Pass)                   │ │
│  └────────────────────────────────────────────────────────────────────┘ │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2. Configuration Matrix

The CI system tests all valid combinations of:

| Dimension | Values |
|-----------|--------|
| Platform | Linux (x64, arm64), macOS (x64, arm64), Windows (x64) |
| Compiler | GCC, Clang, MSVC |
| Build Type | Debug, Release, RelWithDebInfo |
| Serialization | flatbuffers, flatbuffers-with-thrift, legacy-only |
| Vcpkg | static, dynamic |

**Total:** 3 platforms × 3-4 compilers × 2-3 build types × 3 serialization × 2 vcpkg = **~150 unique configurations**

### 3. MECE Organization

To keep the pipeline MECE, we organize jobs into **parallelizable groups**:

```
CI/
├── lint-format/          # Fast checks (no compilation)
├── build-matrix/         # Main build verification
│   ├── linux/
│   │   ├── gcc/
│   │   │   ├── debug/
│   │   │   └── release/
│   │   └── clang/
│   │       ├── debug/
│   │       └── release/
│   ├── macos/
│   │   └── ...
│   └── windows/
│       └── ...
├── test-coverage/        # Coverage & quality metrics
├── integration/          # Cross-platform integration tests
└── release/              # Release artifact generation
```

## Workflow Files

### 1. Main CI Workflow (`ci.yml`)

**Purpose:** Run on every push and PR - validates all configurations

```yaml
name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]

# Cancel in-progress runs for same branch/PR
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true

jobs:
  # ========== STAGE 1: Fast Checks ==========
  lint-format:
    name: Lint & Format Check
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Run clang-format
        run: |
          ./scripts/check-format.sh
      - name: Run clang-tidy
        run: |
          ./scripts/run-clang-tidy.sh

  # ========== STAGE 2: Build Matrix ==========
  # Note: Only run if lint passed
  build-linux:
    name: Build Linux (Linux)
    needs: lint-format
    strategy:
      matrix:
        compiler: [gcc, clang]
        build_type: [Debug, Release]
        serialization: [flatbuffers, flatbuffers-with-thrift]
        include:
          # Static linking is default
          - vcpkg_triplet: x64-linux
            vcpkg_dynamic: false
          # Test dynamic linking
          - compiler: clang
            build_type: Release
            serialization: flatbuffers
            vcpkg_triplet: x64-linux-dynamic
            vcpkg_dynamic: true
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: |
          ./scripts/build-in-docker.sh \
            --compiler ${{ matrix.compiler }} \
            --build-type ${{ matrix.build_type }} \
            --serialization ${{ matrix.serialization }} \
            --vcpkg-triplet ${{ matrix.vcpkg_triplet }}

  build-macos:
    name: Build macOS
    needs: lint-format
    strategy:
      matrix:
        compiler: [clang]
        build_type: [Debug, Release]
        serialization: [flatbuffers, flatbuffers-with-thrift]
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: |
          ./scripts/build-with-homebrew.sh \
            --build-type ${{ matrix.build_type }} \
            --serialization ${{ matrix.serialization }}

  build-windows:
    name: Build Windows
    needs: lint-format
    strategy:
      matrix:
        build_type: [Release]
        serialization: [flatbuffers]
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build
        run: |
          .\scripts\build-with-vcpkg.ps1 \
            -BuildType ${{ matrix.build_type }} \
            -Serialization ${{ matrix.serialization }}

  # ========== STAGE 3: Test Suite ==========
  # Note: Only run if builds passed
  test-suite:
    name: Test Suite
    needs: [build-linux, build-macos, build-windows]
    runs-on: ubuntu-latest
    strategy:
      matrix:
        test_type: [unit, integration, fuzz, benchmark]
    steps:
      - uses: actions/checkout@v4
      - name: Run ${{ matrix.test_type }} tests
        run: |
          ./scripts/run-tests.sh --type ${{ matrix.test_type }}

  # ========== STAGE 4: Coverage ==========
  coverage:
    name: Code Coverage
    needs: build-linux
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Generate Coverage
        run: |
          ./scripts/generate-coverage.sh
      - name: Upload to Codecov
        uses: codecov/codecov-action@v3
```

### 2. Reusable Workflows

**Purpose:** DRY - define common build/test logic once, reuse everywhere

#### `.github/workflows/_build.yml`

```yaml
name: Reusable Build

on:
  workflow_call:
    inputs:
      os:
        required: true
        type: string
      compiler:
        required: true
        type: string
      build_type:
        required: true
        type: string
      serialization:
        required: true
        type: string
      vcpkg_triplet:
        type: string

jobs:
  build:
    runs-on: ${{ inputs.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Setup vcpkg
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgGitCommitId: '7c11b72a6f810293d0ce234de126b99e9ab6fa7a'
          vcpkgJsonGlob: '**/vcpkg.json'
          vcpkgDirectory: '${{ github.workspace }}/../vcpkg'
      - name: Setup CMake
        uses: jwlawson/actions-setup-cmake@v2
      - name: Configure & Build
        run: |
          cmake -B build \
            -DCMAKE_BUILD_TYPE=${{ inputs.build_type }} \
            -DDWARFS_WITH_${{ inputs.serialization }}=ON \
            ${{ inputs.os == 'ubuntu-latest' && '-DVCPKG_TARGET_TRIPLET=' || '' }}
          cmake --build build --config ${{ inputs.build_type }}
      - name: Test
        run: |
          ctest --test-dir build --output-on-failure
```

### 3. Release Workflow (`release.yml`)

**Purpose:** Automated releases on version tags

```yaml
name: Release

on:
  push:
    tags:
      - 'v*.*.*'

# Non-concurrent: releases must be sequential
concurrency:
  group: release
  cancel-in-progress: false

jobs:
  # ========== STAGE 1: Verify ==========
  verify-release:
    name: Verify Release Readiness
    runs-on: ubuntu-latest
    outputs:
      version: ${{ steps.get_version.outputs.version }}
      is_valid: ${{ steps.check.outputs.valid }}
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0
      - name: Get Version
        id: get_version
        run: |
          VERSION=${{GITHUB_REF#refs/tags/v}
          echo "version=$VERSION" >> $GITHUB_OUTPUT
      - name: Check Version Format
        id: check
        run: |
          VERSION=${{GITHUB_REF#refs/tags/v}}
          if [[ ! $VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            echo "Invalid version format: $VERSION"
            echo "valid=false" >> $GITHUB_OUTPUT
            exit 1
          fi
          echo "valid=true" >> $GITHUB_OUTPUT
      - name: Check Changelog
        run: |
          VERSION=${{GITHUB_REF#refs/tags/v}}
          if ! grep -q "^## \[$VERSION\]" CHANGELOG.md; then
            echo "ERROR: Changelog entry for $VERSION not found"
            exit 1
          fi

  # ========== STAGE 2: Build Release Artifacts ==========
  build-release:
    name: Build Release Artifacts
    needs: verify-release
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
        include:
          - os: ubuntu-latest
            artifact_name: dwarfs-linux-x64
            binary_name: mkdwarfs
          - os: macos-latest
            artifact_name: dwarfs-macos-x64
            binary_name: mkdwarfs
          - os: windows-latest
            artifact_name: dwarfs-windows-x64
            binary_name: mkdwarfs.exe
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Build Release
        run: |
          ./scripts/build-release.sh ${{ needs.verify-release.outputs.version }}
      - name: Upload Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: ${{ matrix.artifact_name }}
          path: |
            build/mkdwarfs*
            build/dwarfs*
          retention-days: 90

  # ========== STAGE 3: Create GitHub Release ==========
  create-release:
    name: Create GitHub Release
    needs: [verify-release, build-release]
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Download All Artifacts
        uses: actions/download-artifact@v4
        with:
          path: artifacts
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          tag_name: ${{ needs.verify-release.outputs.version }}
          name: ${{ needs.verify-release.outputs.version }}
          body: |
            ## DwarFS ${{ needs.verify-release.outputs.version }}

            ### Installation
            See [INSTALL.md](INSTALL.md) for details.

            ### What's Changed
            See [CHANGELOG.md](CHANGELOG.md) for details.
          files: artifacts/**/*
          generate_release_notes: true
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}

  # ========== STAGE 4: Publish to Package Managers ==========
  publish-packages:
    name: Publish to Package Managers
    needs: create-release
    strategy:
      matrix:
        manager: [homebrew, vcpkg, conda-forge]
    runs-on: ubuntu-latest
    steps:
      - name: Publish to ${{ matrix.manager }}
        run: |
          ./scripts/publish-to-${{ matrix.manager }}.sh \
            ${{ needs.create-release.outputs.version }}
```

## Configuration Management

### 1. Matrix Configuration File (`.github/configs/build-matrix.yml`)

```yaml
# Build matrix configuration - single source of truth
build_matrix:
  platforms:
    ubuntu-latest:
      compilers: [gcc, clang]
      vcpkg_triplets: [x64-linux, x64-linux-dynamic]
      docker_images:
        gcc: ubuntu:22.04
        clang: ubuntu:22.04

    macos-latest:
      compilers: [clang]
      vcpkg_triplets: [x64-osx, x64-osx-dynamic, arm64-osx]

    windows-latest:
      compilers: [msvc]
      vcpkg_triplets: [x64-windows, x64-windows-dynamic]

  build_types:
    Debug:
      flags: -DCMAKE_BUILD_TYPE=Debug -DWITH_TESTS=ON
      tests: true

    Release:
      flags: -DCMAKE_BUILD_TYPE=Release -DWITH_TESTS=ON
      tests: true

    RelWithDebInfo:
      flags: -DCMAKE_BUILD_TYPE=RelWithDebInfo
      tests: false

  serializations:
    flatbuffers:
      options: -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF

    flatbuffers-with-thrift:
      options: -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON

    legacy-only:
      options: -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=OFF

  # Critical builds that MUST pass
  critical:
    - platform: ubuntu-latest
      compiler: clang
      build_type: Release
      serialization: flatbuffers

    - platform: macos-latest
      compiler: clang
      build_type: Release
      serialization: flatbuffers
```

### 2. Quality Gates

Define which checks must pass before proceeding:

```yaml
quality_gates:
  # Must pass before release
  required_for_release:
    - lint-format
    - critical-builds
    - test-coverage-min-80
    - no-memory-leaks
    - no-security-vulnerabilities

  # Must pass before merge to main
  required_for_merge:
    - lint-format
    - one-build-per-serialization
    - unit-tests
    - integration-tests

  # Optional: informational only
  optional:
    - benchmarks
    - fuzz-tests
    - coverage-report
```

## Artifacts and Caching

### 1. CCache Configuration

```yaml
- name: Setup ccache
  uses: hendrikmuhs/ccache-action@v1.2
  with:
    key: ${{ runner.os }}-${{ matrix.compiler }}-${{ matrix.build_type }}
    max-size: 5G
```

### 2. vcpkg Binary Cache

```yaml
- name: Setup vcpkg binary cache
  uses: actions/cache@v4
  with:
    path: |
      ~/.cache/vcpkg
      ${{ github.workspace }}/../vcpkg
    key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}
    restore-keys: |
      vcpkg-${{ runner.os }}-
```

### 3. Build Artifacts

```yaml
# Upload test results
- name: Upload Test Results
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: test-results-${{ matrix.os }}-${{ matrix.compiler }}
    path: |
      build/Testing/
      build/**/*.log
```

## Notification Strategy

```yaml
# Notify on failure
- name: Notify on Failure
  if: failure()
  uses: 8398a7/action-slack@v3
  with:
    status: ${{ job.status }}
    text: |
      Build failed for ${{ github.repository }}@${{ github.sha }}
      Job: ${{ github.job }}
```

## Release Strategy

### 1. Semantic Versioning

- **Major (X.0.0)**: Breaking changes, API changes
- **Minor (0.X.0)**: New features, backward compatible
- **Patch (0.0.X)**: Bug fixes, internal changes

### 2. Release Branch Strategy

```
main (production)
  ^
  |
develop (integration)
  ^
  |
feature/* (feature branches)
```

### 3. Automated Release Process

1. Developer creates release branch: `release/x.y.x`
2. All CI checks must pass
3. Create annotated tag: `git tag -a vx.y.x -m "Release x.y.x"`
4. Push tag: `git push origin vx.y.x`
5. CI automatically:
   - Builds all release artifacts
   - Runs full test suite
   - Creates GitHub Release
   - Publishes to package managers

## Monitoring and Metrics

### 1. CI Dashboard

Track:
- Build success rate per configuration
- Average build time
- Test coverage trends
- Flaky test detection
- Resource utilization

### 2. SLA Targets

- **Lint/Format check**: < 2 minutes
- **Single build**: < 30 minutes
- **Full CI (parallel)**: < 45 minutes
- **Release pipeline**: < 2 hours

## Security Scanning

```yaml
security-scan:
  name: Security Scan
  runs-on: ubuntu-latest
  steps:
    - name: Run Trivy
      uses: aquasecurity/trivy-action@master
      with:
        scan-type: 'fs'
        scan-ref: '.'
        format: 'sarif'
        output: 'trivy-results.sarif'

    - name: Upload SARIF
      uses: github/codeql-action/upload-sarif@v2
      with:
        sarif_file: trivy-results.sarif
```

## Rollback Strategy

If a release has critical issues:

1. **Hotfix branch**: Create `hotfix/x.y.z+1`
2. **Quick CI**: Run only critical tests
3. **Patch release**: Tag as `vx.y.z+1`
4. **Post-mortem**: Document the issue and improve CI

## Continuous Improvement

### 1. CI Performance Monitoring

```bash
# Track build times over time
./scripts/analyze-ci-performance.sh
```

### 2. Flaky Test Detection

```yaml
- name: Detect Flaky Tests
  if: failure()
  uses: trmcnvn/flaky-action@v1
  with:
    repo-token: ${{ secrets.GITHUB_TOKEN }}
    label: "flaky"
```

### 3. Dependency Update Automation

```yaml
schedule:
  - cron: '0 0 * * 0'  # Weekly dependency update
    workflow: dependency-update
```

## Implementation Priority

### Phase 1: Foundation (Current)
- [x] Basic CI pipeline
- [x] Lint/format checks
- [ ] Build matrix for Linux/macOS
- [ ] Automated testing

### Phase 2: Expansion
- [ ] Windows support
- [ ] Coverage reporting
- [ ] Security scanning
- [ ] Performance benchmarking

### Phase 3: Automation
- [ ] Dependency updates
- [ ] Release automation
- [ ] Package manager publishing
- [ ] Documentation generation

### Phase 4: Optimization
- [ ] Advanced caching
- [ ] Parallel execution optimization
- [ ] Resource management
- [ ] Cost monitoring

## Summary

This architecture provides:

1. **MECE Coverage**: Every configuration tested exactly once
2. **DRY Implementation**: Reusable workflows, shared scripts
3. **Comprehensive Testing**: All platforms, compilers, build types
4. **Pipelined Execution**: Parallel where possible, gated where needed
5. **Quality Gates**: Ensures only quality code is released
6. **Automation**: Minimal manual intervention required

The pipeline is designed to be:
- **Fast**: Feedback in < 30 minutes for most changes
- **Reliable**: Caching, retries, idempotent operations
- **Transparent**: Clear status, logs, and notifications
- **Scalable**: Easy to add new platforms/configurations
