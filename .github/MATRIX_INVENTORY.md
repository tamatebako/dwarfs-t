# GitHub Actions Matrix Inventory

## Current State (Existing Workflows)

### 1. ci.yml - Main CI Workflow

**Matrix Location**: Inline in `ci.yml`

**Matrix Definition**:
```yaml
strategy:
  fail-fast: false
  matrix:
    include:
      # Linux x64 (2 jobs)
      - runner: ubuntu-latest
        os: linux
        arch: x64
        triplet: x64-linux
        config: flatbuffers-only
      - runner: ubuntu-latest
        os: linux
        arch: x64
        triplet: x64-linux
        config: both-formats

      # Linux ARM64 (2 jobs)
      - runner: ubuntu-24.04-arm64
        os: linux
        arch: arm64
        triplet: arm64-linux
        config: flatbuffers-only
      - runner: ubuntu-24.04-arm64
        os: linux
        arch: arm64
        triplet: arm64-linux
        config: both-formats

      # macOS x64 (2 jobs)
      - runner: macos-13
        os: macos
        arch: x64
        triplet: x64-osx
        config: flatbuffers-only
      - runner: macos-13
        os: macos
        arch: x64
        triplet: x64-osx
        config: both-formats

      # macOS ARM64 (2 jobs)
      - runner: macos-14
        os: macos
        arch: arm64
        triplet: arm64-osx
        config: flatbuffers-only
      - runner: macos-14
        os: macos
        arch: arm64
        triplet: arm64-osx
        config: both-formats

      # Windows x64 (2 jobs)
      - runner: windows-latest
        os: windows
        arch: x64
        triplet: x64-windows-static
        config: flatbuffers-only
      - runner: windows-latest
        os: windows
        arch: x64
        triplet: x64-windows-static
        config: both-formats
```

**Total Jobs**: 10

---

### 2. vcpkg-triplet-matrix.yml - Comprehensive Triplet Testing

**Matrix Location**: Dynamic via `matrix-setup` job

**Matrix Setup Job**:
```yaml
matrix-setup:
  runs-on: ubuntu-latest
  outputs:
    matrix: ${{ steps.set-matrix.outputs.matrix }}
  steps:
    - id: set-matrix
      run: |
        MATRIX='[
          {"triplet": "x64-linux-static", "os": "ubuntu-latest", "arch": "x64"},
          {"triplet": "arm64-linux-static", "os": "ubuntu-24.04-arm64", "arch": "arm64"},
          {"triplet": "x64-osx-static", "os": "macos-13", "arch": "x64"},
          {"triplet": "arm64-osx-static", "os": "macos-14", "arch": "arm64"},
          {"triplet": "x64-windows-static", "os": "windows-latest", "arch": "x64"},
          {"triplet": "arm64-windows-static", "os": "windows-latest", "arch": "arm64"}
        ]'
        echo "matrix=$MATRIX" >> $GITHUB_OUTPUT
```

**Matrix Usage**:
```yaml
strategy:
  fail-fast: false
  matrix:
    include: ${{ fromJson(needs.matrix-setup.outputs.matrix) }}
    config: [flatbuffers-only, both-formats]
```

**Total Jobs**: 6 triplets × 2 configs = **12 jobs**

---

### 3. compat-test.yml - Compatibility Testing

**Matrix Location**: Inline in `compat-test.yml`

**Matrix Definition**:
```yaml
strategy:
  fail-fast: false
  matrix:
    include:
      - runner: macos-14
        os: macos
        arch: arm64
        triplet: arm64-osx
      - runner: macos-13
        os: macos
        arch: x64
        triplet: x64-osx
      - runner: ubuntu-latest
        os: linux
        arch: x64
        triplet: x64-linux
      - runner: ubuntu-24.04-arm64
        os: linux
        arch: arm64
        triplet: arm64-linux
```

**Total Jobs**: 4

---

### 4. benchmark-comprehensive.yml - Benchmark Suite

**Matrix Location**: Inline in `benchmark-comprehensive.yml`

**Matrix Definition**:
```yaml
strategy:
  fail-fast: false
  matrix:
    include:
      - runner: ubuntu-latest
        triplet: x64-linux
        config: flatbuffers-only
        build_dir: build-fb-bench
      - runner: ubuntu-latest
        triplet: x64-linux
        config: both-formats
        build_dir: build-both-bench
```

**Total Jobs**: 2

---

## Proposal: Consolidated Matrix Definitions

### Problem: Matrix Duplication

Currently, matrices are **duplicated across workflows**:

```
ci.yml                    → inline matrix (10 jobs)
vcpkg-triplet-matrix.yml  → dynamic matrix (12 jobs)
compat-test.yml           → inline matrix (4 jobs)
benchmark-comprehensive.yml → inline matrix (2 jobs)

TOTAL: 4 different matrix definitions
```

### Solution: Centralized Matrix Configuration

**Option 1: Matrix in Reusable Workflow (RECOMMENDED)**

```yaml
# .github/workflows/_build-test-reusable.yml
on:
  workflow_call:
    inputs:
      runner: { type: string, required: true }
      triplet: { type: string, required: true }
      config: { type: string, required: true }

jobs:
  build:
    runs-on: ${{ inputs.runner }}
    # No matrix here - parameters passed from caller
```

**Calling workflow defines matrix**:

```yaml
# .github/workflows/ci-main.yml
jobs:
  test:
    strategy:
      matrix:
        include:
          - runner: ubuntu-latest
            triplet: x64-linux
            config: flatbuffers-only
          # ... more matrix entries
    uses: ./.github/workflows/_build-test-reusable.yml
    with:
      runner: ${{ matrix.runner }}
      triplet: ${{ matrix.triplet }}
      config: ${{ matrix.config }}
```

**Option 2: Dynamic Matrix Generation (for scheduled.yml)**

```yaml
# .github/workflows/scheduled.yml
jobs:
  setup-matrix:
    runs-on: ubuntu-latest
    outputs:
      matrix: ${{ steps.set-matrix.outputs.matrix }}
    steps:
      - id: set-matrix
        run: |
          # Define ALL triplets (including dynamic variants)
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
          echo "matrix=$MATRIX" >> $GITHUB_OUTPUT
```

---

## Complete Matrix Reference

### All Supported Triplets

| Platform | Runner | Architecture | Static Triplet | Dynamic Triplet |
|----------|--------|--------------|----------------|-----------------|
| **Linux** | ubuntu-latest | x64 | `x64-linux` | `x64-linux-dynamic` |
| **Linux** | ubuntu-24.04-arm64 | ARM64 | `arm64-linux` | `arm64-linux-dynamic` |
| **macOS** | macos-13 | x64 | `x64-osx` | `x64-osx-dynamic` |
| **macOS** | macos-14 | ARM64 | `arm64-osx` | `arm64-osx-dynamic` |
| **Windows** | windows-latest | x64 | `x64-windows-static` | `x64-windows-dynamic` |
| **Windows** | windows-latest | ARM64 | `arm64-windows-static` | `arm64-windows-dynamic` |

### Build Configurations

| Config | Description | Use Case |
|--------|-------------|----------|
| `flatbuffers-only` | FlatBuffers only (recommended) | Production, new deployments |
| `both-formats` | FlatBuffers + Thrift (Legacy + Modern) | Development, compatibility testing |

### Workflow → Matrix Mapping

| Workflow | Matrix Source | Triplets | Configs | Jobs |
|----------|---------------|----------|---------|------|
| **pr-validation.yml** (NEW) | Inline | 2 (ubuntu-latest, macos-14) | 2 | **4** |
| **ci-main.yml** (NEW) | Inline | 5 (all platforms) | 2 | **10** |
| **release.yml** (NEW) | Inline | 5 (all platforms) | 1 | **5** |
| **scheduled.yml** (NEW) | Dynamic | 12 (all variants) | 2 | **24** |
| **compat-test.yml** (EXISTING) | Inline | 4 (Linux + macOS) | N/A | **4** |

---

## Matrix Location Summary

### Current State (Fragmented)

```
.github/workflows/
├── ci.yml
│   └── matrix: inline (10 entries)
├── vcpkg-triplet-matrix.yml
│   └── matrix: dynamic via setup job (6 entries)
├── compat-test.yml
│   └── matrix: inline (4 entries)
├── benchmark-comprehensive.yml
│   └── matrix: inline (2 entries)
└── ... (other workflows with inline matrices)
```

**Problem**: Duplicated matrix definitions across multiple files

### Proposed State (Consolidated)

```
.github/workflows/
├── pr-validation.yml
│   └── matrix: inline FAST subset (2 platforms × 2 configs = 4 jobs)
├── ci-main.yml
│   └── matrix: inline FULL (5 platforms × 2 configs = 10 jobs)
├── release.yml
│   └── matrix: inline release (5 platforms × 1 config = 5 jobs)
├── scheduled.yml
│   └── matrix: dynamic COMPREHENSIVE (12 triplets × 2 configs = 24 jobs)
└── _build-test-reusable.yml
    └── NO matrix (receives parameters from caller)
```

**Benefit**: Each workflow has ONE clear matrix definition

---

## How to Find Matrices in Workflows

**Search command**:
```bash
# Find all workflows with matrices
grep -l "strategy:" .github/workflows/*.yml

# Show matrix for specific workflow
grep -A 30 "strategy:" .github/workflows/ci.yml
```

**Key patterns to look for**:
1. `strategy:` - Indicates matrix configuration
2. `matrix:` - Contains matrix variables
3. `include:` - Lists matrix combinations
4. `fromJson()` - Dynamic matrix from JSON

---

## Recommendation

**Keep matrices in calling workflows**, NOT in reusable workflows.

**Why?**
1. ✅ **Flexibility**: Each workflow can define its own matrix scope
2. ✅ **Clarity**: Matrix is visible in the workflow file
3. ✅ **DRY**: Reusable workflow contains build logic, not matrix definitions
4. ✅ **MECE**: Each workflow has a clear, distinct matrix

**Pattern**:
```yaml
# Calling workflow (ci-main.yml)
jobs:
  test:
    strategy:
      matrix:
        include:
          - runner: ubuntu-latest
            triplet: x64-linux
            config: flatbuffers-only
    uses: ./.github/workflows/_build-test-reusable.yml
    with:
      runner: ${{ matrix.runner }}
      triplet: ${{ matrix.triplet }}
      config: ${{ matrix.config }}

# Reusable workflow (_build-test-reusable.yml)
# NO MATRIX - receives parameters via inputs
```
