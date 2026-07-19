# DwarFS CI/CD Guide

This directory contains the GitHub Actions workflows for continuous integration, testing, and release management.

## Quick Overview

DwarFS uses a **unified CI/CD architecture** with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Workflow Categories                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  CONTINUOUS INTEGRATION (CI)                                   │
│  ├─ pr-validation.yml    → Fast PR feedback (4 jobs, ~15min)    │
│  └─ ci-main.yml         → Comprehensive CI (10 jobs, ~45min)     │
│                                                                  │
│  CONTINUOUS DELIVERY (CD)                                     │
│  └─ release.yml         → Release artifacts (5 platforms)       │
│                                                                  │
│  SCHEDULED & MANUAL                                             │
│  ├─ scheduled.yml       → Weekly comprehensive (24+ jobs)      │
│  └─ manual.yml          → On-demand testing (flexible)         │
│                                                                  │
│  REUSABLE WORKFLOWS                                           │
│  ├─ _build-test-reusable.yml       → Core build/test           │
│  ├─ _build-release-reusable.yml    → Release builds          │
│  ├─ _compat-test-reusable.yml      → Compatibility          │
│  └─ _benchmark-reusable.yml        → Benchmarks             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

## Workflow Reference

### Main CI Workflows

| Workflow | When It Runs | What It Does | Duration |
|----------|-------------|--------------|----------|
| **pr-validation.yml** | Pull requests to main/feature* | Fast validation on 2 platforms × 2 configs | ~15 min |
| **ci-main.yml** | Pushes to main | Comprehensive testing on 5 platforms × 2 configs | ~45 min |
| **release.yml** | Git tags v* | Builds release artifacts for all platforms | ~30 min |
| **scheduled.yml** | Weekly (Sun 2AM UTC) + manual | Full triplet matrix + optional benchmarks | 2-3 hours |
| **manual.yml** | Manual dispatch | Flexible on-demand testing | Variable |

### Reusable Workflows

These are internal workflows called by other workflows:

| Workflow | Purpose | Key Features |
|----------|---------|-------------|
| `_build-test-reusable.yml` | Standardized build and test | vcpkg, artifact upload, test results |
| `_build-release-reusable.yml` | Release packaging | Stripped binaries, tarballs |
| `_compat-test-reusable.yml` | Homebrew compatibility | Cross-version testing |
| `_benchmark-reusable.yml` | Performance testing | Perl dataset, comprehensive reports |

## Platform Coverage

| Platform | Runner | Triplet | CI Main | PR Validation | Release |
|----------|--------|---------|---------|---------------|--------|
| **Linux x64** | ubuntu-latest | `x64-linux` | ✅ | ✅ | ✅ |
| **Linux ARM64** | ubuntu-24.04-arm64 | `arm64-linux` | ✅ | ❌ | ✅ |
| **macOS x64** | macos-13 | `x64-osx` | ✅ | ❌ | ✅ |
| **macOS ARM64** | macos-14 | `arm64-osx` | ✅ | ✅ | ✅ |
| **Windows x64** | windows-latest | `x64-windows-static` | ✅ | ❌ | ✅ |

**Note**: PR validation only tests on Ubuntu x64 and macOS ARM64 for fast feedback.

## Build Configurations

| Configuration | Description | PR Validation | CI Main | Release |
|---------------|-------------|---------------|---------|---------|
| **flatbuffers-only** | FlatBuffers metadata only | ✅ | ✅ | ✅ (production) |
| **both-formats** | FlatBuffers + Thrift (Legacy + Modern) | ✅ | ✅ | ❌ |

## Quick Start

### For Developers

**After making changes:**

1. **Push your changes** → PR validation runs automatically
2. **Review results** in the Actions tab
3. **Fix any failures** and push updates

**To trigger manually:**
```bash
gh workflow run pr-validation.yml
```

### For Release Managers

**Before creating a release:**

1. Run comprehensive tests:
   ```bash
   gh workflow run ci-main.yml
   ```

2. Tag and push:
   ```bash
   git tag v0.17.0
   git push origin v0.17.0
   ```
   This triggers `release.yml` automatically.

3. Verify release on GitHub Releases page

### For CI/CD Maintenance

**Run comprehensive tests:**
```bash
gh workflow run scheduled.yml -f run-benchmarks=true
```

**Test specific triplet:**
```bash
gh workflow run manual.yml -f triplet=arm64-osx -f config=both-formats
```

**Run benchmarks:**
```bash
gh workflow run manual.yml -f workflow-type=benchmark
```

## Directory Structure

```
.github/
├── workflows/
│   ├── pr-validation.yml           # Fast PR feedback
│   ├── ci-main.yml                # Comprehensive CI
│   ├── release.yml                # Release artifacts
│   ├── scheduled.yml              # Weekly comprehensive
│   ├── manual.yml                 # On-demand testing
│   │
│   ├── _build-test-reusable.yml   # Core build/test (DRY!)
│   ├── _build-release-reusable.yml # Release builds
│   ├── _compat-test-reusable.yml  # Compatibility tests
│   └── _benchmark-reusable.yml    # Benchmarks
│   │
│   └── [legacy workflows...]      # Being deprecated
│
├── actions/
│   ├── setup-vcpkg/               # vcpkg installation
│   ├── configure-cmake/           # CMake configuration
│   ├── setup-build-deps/          # Build dependencies
│   ├── run-ctest/                 # Test execution
│   └── setup-homebrew-dwarfs/     # Homebrew setup
│
├── CI_CD_ARCHITECTURE_PROPOSAL.md   # Complete architecture
├── CI_CD_ARCHITECTURE_VISUAL.md     # Visual diagrams
├── IMPLEMENTATION_STATUS.md          # Progress tracking
├── MATRIX_INVENTORY.md              # Matrix reference
└── README.md                         # This file
```

## Architecture Principles

### MECE (Mutually Exclusive, Collectively Exhaustive)

Each workflow has a **single, clear purpose**:

- **PR validation**: Fast feedback for contributors
- **CI main**: Full validation for main branch
- **Release**: Artifact creation for distribution
- **Scheduled**: Comprehensive testing (benchmarks, all triplets)
- **Manual**: Flexible on-demand testing

### DRY (Don't Repeat Yourself)

**Reusable workflows** eliminate duplication:

- `_build-test-reusable.yml` is used by ALL CI workflows
- Single source of truth for build/test logic
- Change once, affects all workflows

### Comprehensive Coverage

- **Platforms**: 5 platforms (Linux x64/ARM64, macOS x64/ARM64, Windows x64)
- **Configurations**: 2 build types (flatbuffers-only, both-formats)
- **Test types**: Unit, integration, compatibility, benchmarks

## Workflow Triggers

| Event | Workflow | Purpose |
|-------|----------|---------|
| **PR opened** | `pr-validation.yml` | Fast feedback, merge decision |
| **PR updated** | `pr-validation.yml` | Re-validate changes |
| **Push to main** | `ci-main.yml` | Full validation |
| **Tag v\*** | `release.yml` | Create release |
| **Weekly (Sun 2AM)** | `scheduled.yml` | Nightly comprehensive |
| **Manual** | `manual.yml` | On-demand testing |

## Matrix Strategy

### Platform Matrix

The CI system uses a **2×5 platform matrix**:

```
┌─────────────────────────────────────────────────────────────┐
│                     Platform Matrix                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ubuntu-latest      (Linux x64)                             │
│  ubuntu-24.04-arm64 (Linux ARM64)                          │
│  macos-13           (macOS x64)                             │
│  macos-14           (macOS ARM64)                           │
│  windows-latest     (Windows x64)                           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Configuration Matrix

```
┌─────────────────────────────────────────────────────────────┐
│                   Configuration Matrix                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  flatbuffers-only    (FlatBuffers metadata)                  │
│  both-formats       (FlatBuffers + Thrift)                  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Combined Matrix

**Total CI jobs**: 5 platforms × 2 configurations = **10 jobs**

For `scheduled.yml`, the matrix expands to **24+ jobs** with dynamic triplet variants.

## Deprecated Workflows

The following workflows are deprecated and will be removed:

- ❌ `build.yml` - Superseded by `ci-main.yml` and `pr-validation.yml`
- ❌ `build-test.yml` - Superseded by `_build-test-reusable.yml`

**Migration**: Update any references to use the new workflows.

## Troubleshooting

### Workflow Failures

**Check workflow logs:**
```bash
gh run list --workflow=pr-validation.yml
gh run view <run-id>
```

**Common issues:**
- **vcpkg build failures**: Check Tebako jemalloc overlay port
- **Test failures**: View test results in artifacts
- **Timeout failures**: May indicate resource issues

### Manual Testing

**Test locally before pushing:**
```bash
./scripts/one-step/test-everything.sh --quick
```

**Test specific configuration:**
```bash
BUILD_DIR=build-test ./scripts/clean-build.sh -y
cd build-test && ninja && ninja test
```

## Documentation

- **[CI_CD_ARCHITECTURE_PROPOSAL.md](CI_CD_ARCHITECTURE_PROPOSAL.md)** - Complete architecture
- **[CI_CD_ARCHITECTURE_VISUAL.md](CI_CD_ARCHITECTURE_VISUAL.md)** - Visual diagrams
- **[IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)** - Progress tracking
- **[MATRIX_INVENTORY.md](MATRIX_INVENTORY.md)** - Matrix reference
- **[../../TESTING.md](../TESTING.md)** - Testing guide
- **[../../WORKFLOW_GUIDE.md](../WORKFLOW_GUIDE.md)** - Developer workflows

## Contributing

When adding new tests or workflows:

1. **Use reusable workflows** - Don't duplicate build logic
2. **Follow MECE principles** - Clear, single purpose
3. **Document your changes** - Update relevant docs
4. **Test thoroughly** - Use `manual.yml` for validation

## Support

For CI/CD issues:
1. Check workflow logs in the Actions tab
2. Review troubleshooting section above
3. Open an issue with workflow run ID
