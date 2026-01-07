# DwarFS CI Testing Strategy - Revised

## Test Matrix (Simplified & DRY)

### Format Configurations (2 Only)

**Rationale**: Thrift has platform limitations (folly/fbthrift dependencies), so we only need:

1. **all-formats**: Test default configuration with all formats enabled
2. **no-thrift**: Test future-proof configuration without thrift dependency

**Not needed**:
- `cereal-only`, `bitsery-only` - Overly specific, not real-world configs

### Platform Matrix (6 Configurations)

| Platform | Architecture | Runner | Formats |
|----------|-------------|---------|---------|
| **Ubuntu** | x86_64 | ubuntu-latest | all-formats, no-thrift |
| **Ubuntu** | aarch64  | ubuntu-24.04-arm64 | all-formats, no-thrift |
| **Windows** | x86_64 | windows-latest | all-formats, no-thrift |
| **Windows** | aarch64 | windows-11-arm64 | no-thrift only |
| **macOS** | x86_64 (Intel) | macos-13 | all-formats, no-thrift |
| **macOS** | aarch64 (Apple) | macos-14 | all-formats, no-thrift |

**Total**: 11 CI jobs (6 platforms × 2 formats - 1 Windows ARM with thrift)

---

## DRY Workflow Structure

### Single Reusable Workflow

**File**: `.github/workflows/metadata-format-ci.yml`

```yaml
name: Metadata Format CI

on:
  pull_request:
    paths:
      - 'src/metadata/**'
      - 'include/dwarfs/metadata/**'
      - 'test/**'
      - 'cmake/**'
      - '.github/workflows/metadata-format-ci.yml'
  push:
    branches: [main, develop]
  workflow_dispatch:

jobs:
  test-metadata-formats:
    name: ${{ matrix.os }} (${{ matrix.arch }}) / ${{ matrix.format }}
    runs-on: ${{ matrix.runner }}

    strategy:
      fail-fast: false
      matrix:
        include:
          # Ubuntu x86_64
          - os: ubuntu
            arch: x86_64
            runner: ubuntu-latest
            format: all-formats
            with_thrift: ON

          - os: ubuntu
            arch: x86_64
            runner: ubuntu-latest
            format: no-thrift
            with_thrift: OFF

          # Ubuntu ARM64
          - os: ubuntu
            arch: aarch64
            runner: ubuntu-24.04-arm64
            format: all-formats
            with_thrift: ON

          - os: ubuntu
            arch: aarch64
            runner: ubuntu-24.04-arm64
            format: no-thrift
            with_thrift: OFF

          # Windows x86_64
          - os: windows
            arch: x86_64
            runner: windows-latest
            format: all-formats
            with_thrift: ON

          - os: windows
            arch: x86_64
            runner: windows-latest
            format: no-thrift
            with_thrift: OFF

          # Windows ARM64 (no thrift only - limited folly support)
          - os: windows
            arch: aarch64
            runner: windows-11-arm64
            format: no-thrift
            with_thrift: OFF

          # macOS Intel
          - os: macos
            arch: x86_64
            runner: macos-13
            format: all-formats
            with_thrift: ON

          - os: macos
            arch: x86_64
            runner: macos-13
            format: no-thrift
            with_thrift: OFF

          # macOS Apple Silicon
          - os: macos
            arch: aarch64
            runner: macos-14
            format: all-formats
            with_thrift: ON

          - os: macos
            arch: aarch64
            runner: macos-14
            format: no-thrift
            with_thrift: OFF

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: true

      - name: Setup Dependencies
        uses: ./.github/actions/setup-deps
        with:
          os: ${{ matrix.os }}
          arch: ${{ matrix.arch }}

      - name: Configure
        run: |
          cmake -B build \
            -DCMAKE_BUILD_TYPE=Release \
            -DWITH_TESTS=ON \
            -DWITH_BENCHMARKS=ON \
            -DDWARFS_WITH_THRIFT=${{ matrix.with_thrift }} \
            -DDWARFS_WITH_CEREAL=ON \
            -DDWARFS_WITH_BITSERY=ON

      - name: Build
        run: cmake --build build --parallel

      - name: Test All
        run: ctest --test-dir build --output-on-failure

      - name: Test Metadata Serialization
        run: |
          ctest --test-dir build \
            --output-on-failure \
            --tests-regex "metadata.*serial"

      - name: Build Info
        run: |
          ./build/mkdwarfs 2>&1 | head -10
          ./build/dwarfsck --help | grep -i format

      - name: Upload Binaries
        if: matrix.format == 'all-formats'
        uses: actions/upload-artifact@v4
        with:
          name: dwarfs-${{ matrix.os }}-${{ matrix.arch }}-${{ matrix.format }}
          path: |
            build/mkdwarfs*
            build/dwarfs*
            build/dwarfsck*
            build/dwarfsextract*
```

---

## DRY Principles Applied

### 1. Reusable Setup Action

**File**: `.github/actions/setup-deps/action.yml`

```yaml
name: Setup DwarFS Dependencies
description: Install build dependencies for DwarFS

inputs:
  os:
    description: 'Operating system (ubuntu, windows, macos)'
    required: true
  arch:
    description: 'Architecture (x86_64, aarch64)'
    required: true

runs:
  using: composite
  steps:
    - name: Ubuntu Dependencies
      if: inputs.os == 'ubuntu'
      shell: bash
      run: |
        sudo apt-get update
        sudo apt-get install -y build-essential cmake ninja-build \
          libboost-all-dev libfmt-dev liblz4-dev libzstd-dev \
          libbrotli-dev liblzma-dev libxxhash-dev

    - name: macOS Dependencies
      if: inputs.os == 'macos'
      shell: bash
      run: |
        brew install cmake ninja boost fmt lz4 zstd brotli xz xxhash

    - name: Windows Dependencies
      if: inputs.os == 'windows'
      shell: pwsh
      run: |
        vcpkg install boost fmt lz4 zstd brotli lzma xxhash
```

### 2. Single Test Template

**No Duplication**: All platforms use same workflow with matrix parameters

**Benefits**:
- Change once, apply everywhere
- Consistent testing across platforms
- Easy to add new platforms
- Clear format configuration

### 3. Conditional Thrift

**Smart Skipping**:
- Windows ARM64: Skip thrift automatically (limited support)
- Other platforms: Test both configurations
- No redundant test combinations

---

## Simplified Test Matrix

### What We Test

```
Platform     Arch    all-formats  no-thrift
────────────────────────────────────────────
Ubuntu       x86_64      ✓           ✓
Ubuntu       aarch64     ✓           ✓
Windows      x86_64      ✓           ✓
Windows      aarch64     ✗           ✓
macOS        x86_64      ✓           ✓
macOS        aarch64     ✓           ✓
────────────────────────────────────────────
TOTAL: 11 jobs
```

### Why This Is Sufficient

**all-formats**:
- Tests default production configuration
- Verifies thrift compatibility where supported
- Ensures cereal and bitsery work

**no-thrift**:
- Tests future-proof configuration
- Validates minimal dependency build
- Proves cereal/bitsery sufficiency

**Not Testing**:
- `cereal-only`: Not a real-world Config (users would have bitsery too)
- `bitsery-only`: Not a real-world config (users would have cereal too)
- `thrift-only`: Legacy, not relevant

---

## Implementation Plan

### Step 1: Create Reusable Action
- `.github/actions/setup-deps/action.yml`
- Handles all three OS dependency installation
- Parameterized by OS and architecture

### Step 2: Create Metadata Format Workflow
- `.github/workflows/metadata-format-ci.yml`
- 11-job matrix covering all platform × format combinations
- Uses setup-deps action (DRY)

### Step 3: Integration Tests
- Add metadata format cross-compatibility tests
- Verify files created with one format can be read with another
- Test format detection and fallback

### Step 4: Update Build Workflow
- Add metadata format info to build artifacts
- Include format configuration in binary names
- Enable format-specific test execution

---

## Execution Checklist

- [ ] Create `.github/actions/setup-deps/action.yml`
- [ ] Create `.github/workflows/metadata-format-ci.yml`
- [ ] Test workflow locally (act tool)
- [ ] Verify all 11 matrix combinations
- [ ] Add metadata serialization tests
- [ ] Update artifact naming
- [ ] Document CI strategy in README
- [ ] Test Windows ARM64 runner availability

---

## Summary

**Simplified Strategy**:
- 2 format configs (not 4)
- 6 platform/arch combinations
- 11 total CI jobs (efficient)
- DRY via reusable action
- All platforms including Windows ARM64

**Ready to implement**: Clear plan, no redundancy, comprehensive coverage.