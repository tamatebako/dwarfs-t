# DwarFS Developer & Release Manager Workflow

This document provides step-by-step workflows for developers and release managers working on DwarFS.

## Table of Contents

- [Quick Reference](#quick-reference)
- [Developer Workflow](#developer-workflow)
  - [Initial Setup](#initial-setup)
  - [Daily Development](#daily-development)
  - [Before Submitting PR](#before-submitting-pr)
  - [After PR is Merged](#after-pr-is-merged)
- [Release Manager Workflow](#release-manager-workflow)
  - [Pre-Release Checklist](#pre-release-checklist)
  - [Creating a Release](#creating-a-release)
  - [Post-Release Tasks](#post-release-tasks)
- [CI/CD Architecture](#cicd-architecture)
- [Tebako-DwarFS Integration](#tebako-dwarfs-integration)

## Quick Reference

### For Developers

```bash
# After code changes, run this ONE command:
./scripts/one-step/test-everything.sh --quick

# If it passes, commit and push:
git add .
git commit -m "feat(scope): description"
git push origin feature/my-feature
```

### For Release Managers

```bash
# Full release process (ONE command):
./scripts/orchestrator/release.sh --version X.Y.Z

# Or step-by-step (see below)
```

## Build Configurations

DwarFS supports two build configurations:

| Config | Description | Metadata Formats | Stability | Use Case |
|-------|-------------|------------------|----------|----------|
| `production` | Production build | FlatBuffers + Legacy Thrift | ✅ Stable | Production use, releases |
| `experimental` | Experimental build | FlatBuffers + Legacy Thrift + Modern Thrift | ⚠️ Experimental | Development, testing |

**Key Differences:**
- **Production**: FlatBuffers + Legacy Thrift (hand-coded, no external deps). Stable, tested, recommended for production use.
- **Experimental**: Adds Modern Thrift (fbthrift) support. May have instability or breaking changes.

**Note**: Legacy Thrift (hand-coded Frozen2 in `src/metadata/legacy/`) is always available in both configurations.

## Developer Workflow

### Initial Setup

**One-Time Setup (per machine):**

```bash
# 1. Clone repository
git clone --recurse-submodules https://github.com/tamatebako/dwarfs-t.git
cd dwarfs-t

# 2. Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"

# 3. Add to shell profile (~/.bashrc, ~/.zshrc, etc.)
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc
source ~/.bashrc

# 4. Verify setup
./scripts/one-step/test-everything.sh --quick
```

**Expected output:**
```
╔═══════════════════════════════════════════════════════════════╗
║           DwarFS One-Step Test Suite                         ║
╚═══════════════════════════════════════════════════════════════╝

✓ FlatBuffers-only (stable, recommended)
✓ Both-formats (FlatBuffers + Thrift)

╔═══════════════════════════════════════════════════════════════╗
║                   ALL TESTS PASSED!                           ║
╚═══════════════════════════════════════════════════════════════╝
```

### Daily Development

#### 1. Create Feature Branch

```bash
git checkout main
git pull origin main
git checkout -b feature/my-feature
```

#### 2. Make Code Changes

Edit files as needed. The project uses:
- **C++** for core code
- **CMake** for build system
- **Bash** for scripts
- **YAML** for CI/CD

#### 3. Test Your Changes

**Quick validation (recommended for iterative development):**

```bash
# Quick test (2-5 minutes)
./scripts/one-step/test-everything.sh --quick
```

**Full validation (before submitting PR):**

```bash
# Full test suite (10-30 minutes depending on platform)
./scripts/one-step/test-everything.sh
```

**Manual testing (if needed):**

```bash
# Clean build
BUILD_DIR=build ./scripts/clean-build.sh -y

# Configure for production (FlatBuffers + Legacy Thrift)
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=$PWD/vcpkg_ports \
  -DVCPKG_OVERLAY_TRIPLETS=$PWD/vcpkg_triplets \
  -DVCPKG_TARGET_TRIPLET=$(scripts/lib/vcpkg_helper.sh auto_triplet) \
  -DDWARFS_WITH_EXPERIMENTAL_THRIFT=OFF \
  -DWITH_TESTS=ON

# Build
cmake --build build --parallel

# Test
ctest --test-dir build --output-on-failure -j$(nproc)
```

#### 4. Commit Your Changes

```bash
# Stage changes
git add .

# Commit with conventional commit format
git commit -m "feat(metadata): add new serialization option

This adds support for XYZ format which improves performance by 20%."

# Common commit types:
# feat:     New feature
# fix:      Bug fix
# docs:     Documentation only
# style:    Code style (formatting, etc.)
# refactor: Code change that neither fixes bug nor adds feature
# test:     Adding or updating tests
# chore:    Build process or auxiliary tool changes
```

#### 5. Push to GitHub

```bash
# Push to feature branch
git push origin feature/my-feature

# Or set upstream and push
git push -u origin feature/my-feature
```

### Before Submitting PR

#### 1. Final Validation

```bash
# Run full test suite
./scripts/one-step/test-everything.sh
```

#### 2. Update Documentation

If your change affects:
- **User-facing features**: Update README.md
- **Build system**: Update BUILD_SYSTEM_ARCHITECTURE.md
- **Testing**: Update TESTING.md
- **CI/CD**: Update .github/workflows/

#### 3. Create Pull Request

```bash
# Or use GitHub web UI
gh pr create --title "feat(metadata): add new serialization option" \
  --body "## Summary
- Adds XYZ format support
- Improves performance by 20%

## Test plan
- [x] All unit tests pass (Linux x64 production)
- [x] All unit tests pass (Linux x64 experimental)
- [x] Compatibility tests pass
- [ ] Manual testing on other platforms (if applicable)

## Checklist
- [x] Documentation updated
- [x] Tests added
- [x] No breaking changes"
```

### After PR is Merged

```bash
# 1. Delete local feature branch
git branch -d feature/my-feature

# 2. Update main branch
git checkout main
git pull origin main

# 3. Clean up build directories
./scripts/one-step/clean.sh --all --yes
```

## Release Manager Workflow

### Pre-Release Checklist

Run this checklist before every release:

```bash
# 1. Checkout main branch
git checkout main
git pull origin main

# 2. Verify no uncommitted changes
git status

# 3. Run full test suite
./scripts/one-step/test-everything.sh

# 4. Run benchmarks (optional but recommended)
./scripts/one-step/benchmark-quick.sh
```

**Checklist:**

- [ ] All tests pass (`./scripts/one-step/test-everything.sh`)
- [ ] No compiler warnings
- [ ] Documentation is up to date
- [ ] CHANGELOG.md is updated
- [ ] Version number is updated in CMakeLists.txt
- [ ] Git tag is ready (vX.Y.Z format)

### Creating a Release

#### Option 1: Automated Release (Recommended)

```bash
# ONE command does everything:
./scripts/orchestrator/release.sh --version 0.9.0 --auto

# This will:
# 1. Run all tests
# 2. Run benchmarks
# 3. Create release builds for all platforms
# 4. Generate release artifacts
# 5. Create git tag
# 6. Push to GitHub
# 7. Create GitHub release
```

#### Option 2: Manual Release (Step-by-Step)

**Step 1: Update Version**

```bash
# Edit version in CMakeLists.txt
vim CMakeLists.txt

# Edit version in vcpkg.json
vim vcpkg.json

# Update CHANGELOG.md
vim CHANGELOG.md
```

**Step 2: Create Release Branch**

```bash
git checkout -b release/0.9.0
```

**Step 3: Run Full Test Suite**

```bash
# Full validation
./scripts/one-step/test-everything.sh
```

**Step 4: Build Release Artifacts**

```bash
# Build all configurations
./scripts/one-step/build-all.sh
```

**Step 5: Create Git Tag**

```bash
git add .
git commit -m "chore(release): prepare release v0.9.0"

git tag -a v0.9.0 -m "Release v0.9.0

## Highlights
- Feature 1
- Feature 2

## Fixes
- Bug fix 1
- Bug fix 2

## Downloads
See GitHub Assets for binaries"

git push origin main
git push origin v0.9.0
```

**Step 6: Trigger GitHub Actions Release**

The `release.yml` workflow will automatically:
- Build release binaries for all platforms
- Run full test suite
- Upload artifacts to GitHub release

**Step 7: Verify Release**

```bash
# Check release status
gh run list --workflow=release.yml --limit 1

# View release on GitHub
gh release view v0.9.0
```

### Post-Release Tasks

#### 1. Update Homebrew Formula (if applicable)

```bash
# Update Homebrew dwarfs formula
brew bump-formula-pr --url=https://github.com/tamatebako/dwarfs-t/archive/refs/tags/v0.9.0.tar.gz
```

#### 2. Update Documentation

```bash
# Update version in README.md
sed -i '' 's/v0.8.0/v0.9.0/g' README.md

# Update documentation links
git add README.md
git commit -m "docs(readme): update version to v0.9.0"
git push origin main
```

#### 3. Announce Release

Create announcement in:
- GitHub Releases (already done by workflow)
- Project README
- Relevant mailing lists/discords

#### 4. Cleanup

```bash
# Clean build directories
./scripts/one-step/clean.sh --all --yes

# Delete release branch
git branch -d release/0.9.0
```

## CI/CD Architecture

### Workflow Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                       GitHub Actions                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │  PR          │    │  CI          │    │  CI Matrix   │     │
│  │  Validation  │    │  (Linux x64) │    │  (All Plats) │     │
│  └──────────────┘    └──────────────┘    └──────────────┘     │
│         │                     │                     │           │
│         ▼                     ▼                     ▼           │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │  Fast (15m)  │    │  Main (60m)  │    │  Full (2-3h) │     │
│  │  Production  │    │  3 configs   │    │  17 configs  │     │
│  └──────────────┘    └──────────────┘    └──────────────┘     │
│         │                     │                     │           │
│         └─────────────────────┴─────────────────────┘           │
│                               │                                 │
│                               ▼                                 │
│                    ┌──────────────────┐                         │
│                    │  _build.yml      │                         │
│                    │  (Reusable Core) │                         │
│                    └──────────────────┘                         │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Workflow Architecture (2025-01-24)

DwarFS uses a **MECE** (Mutually Exclusive, Collectively Exhaustive) workflow structure:

| Workflow | Purpose | Trigger | Jobs | Runtime |
|----------|---------|---------|------|--------|
| `pr-validation.yml` | Fast PR feedback | Pull requests | 1 | ~15 min |
| `ci.yml` | Main CI validation | Push to main, feature/** | 3 | ~60 min |
| `ci-matrix.yml` | Full cross-platform matrix | Push to main, manual | 17 | ~2-3 hours |
| `release.yml` | Release artifacts | Git tags (v*) | Variable | ~30 min |

**Reusable Core:**
| Workflow | Purpose | Used By |
|----------|---------|---------|
| `_build.yml` | Core build/test logic | All CI workflows |

### CI Matrix Coverage

**ci-matrix.yml** tests all supported triplets across platforms:

| Platform | Jobs | Configurations |
|----------|------|----------------|
| **Linux** | 6 | x64 (production/experimental/dynamic), ARM64 (production/experimental) |
| **macOS** | 5 | x64 (production/dynamic), ARM64 (production/experimental/dynamic) |
| **Windows** | 6 | x64 (static/dynamic/MSYS/MinGW), ARM64 (static) |

**Total: 17 jobs** in full matrix (manual or push to main)

### CI Workflow Files

| File | Purpose | Trigger | Runtime |
|------|---------|---------|---------|
| `_build.yml` | Reusable core workflow | Called by others | N/A |
| `pr-validation.yml` | Fast PR feedback (Linux x64 production) | Pull request | ~15 min |
| `ci.yml` | Main CI (Linux x64 all configs) | Push to main, feature/** | ~60 min |
| `ci-matrix.yml` | Full cross-platform matrix | Push to main, workflow_dispatch | ~2-3 hours |
| `release.yml` | Release builds | Git tag (v*) | ~30 min |

### Experimental Build Issues

**Issue**: Experimental build fails
**Explanation**: The experimental configuration builds Modern Thrift (fbthrift) which depends on folly, fizz, mvfst, and wangle. These are complex dependencies that may fail to compile.

**Status**: Experimental jobs are marked with `continue-on-error: true` so they don't block CI. The production build must pass.

**Affected Jobs**:
- `ci.yml` → `experimental` job
- `ci-matrix.yml` → All jobs with "Experimental" in name

## Tebako-DwarFS Integration

### Architecture

Tebako-DwarFS extends DwarFS with Ruby integration:

```
┌─────────────────────────────────────────────────────────────────┐
│                    Tebako-DwarFS                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │  Ruby FFI    │    │  Ruby Gem   │    │  Tebako VM   │     │
│  │  Bindings    │───▶│  Wrapper    │───▶│  Integration │     │
│  └──────────────┘    └──────────────┘    └──────────────┘     │
│         │                                       │              │
│         ▼                                       ▼              │
│  ┌──────────────┐                      ┌──────────────┐       │
│  │  libdwarfs   │◀─────────────────────│  Embedded    │       │
│  │  (static)    │                      │  Filesystem  │       │
│  └──────────────┘                      └──────────────┘       │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Extending to Tebako-DwarFS

**Key Differences:**

| Aspect | DwarFS | Tebako-DwarFS |
|--------|--------|---------------|
| Build Type | Shared/Dynamic | Static only |
| Ruby FFI | No | Yes |
| Embedding | No | Yes |
| vcpkg Triplets | All | Static only |

**Integration Steps:**

1. **Use DwarFS as submodule:**
```bash
cd tebako
git submodule add https://github.com/tamatebako/dwarfs-t.git ext/dwarfs
```

2. **Build static library:**
```bash
cd ext/dwarfs
cmake -B build-static \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=$PWD/vcpkg_ports \
  -DVCPKG_OVERLAY_TRIPLETS=$PWD/vcpkg_triplets \
  -DVCPKG_TARGET_TRIPLET=$(scripts/lib/vcpkg_helper.sh auto_triplet)-static \
  -DBUILD_SHARED_LIBS=OFF \
  -DDWARFS_WITH_EXPERIMENTAL_THRIFT=OFF \
  -DWITH_TESTS=OFF \
  -DWITH_TOOLS=OFF \
  -DWITH_LIBDWARFS=ON
cmake --build build-static --parallel
```

3. **Link to Ruby extension:**
```ruby
# ext/tebako_dwarfs/extconf.rb
require 'mkmf'

$libs << " -L#{dwarfs_build_dir} -ldwarfs"
$libs << " -L#{vcpkg_install_dir}/lib -ljemalloc"
$libs << " -L#{vcpkg_install_dir}/lib -lfmt"

create_makefile('tebako_dwarfs')
```

4. **Use same CI workflows:**
```yaml
# .github/workflows/tebako-ci.yml
jobs:
  test:
    uses: tamatebako/dwarfs-t/.github/workflows/_build-test-reusable.yml@main
    with:
      triplet: x64-linux-static
      config: flatbuffers
```

### Shared Scripts

Both projects can use the same scripts:

```bash
# In tebako-dwarfs
source ../dwarfs/scripts/lib/build_env.sh
source ../dwarfs/scripts/lib/vcpkg_helper.sh

# Use same build functions
dwarfs_build_configuration "production" \
  "build-tebako" \
  "Release" \
  "vcpkg" \
  "$VCPKG_ROOT" \
  "x64-linux-static"
```

### CI/CD Reuse

Tebako-DwarFS can use DwarFS CI workflows:

```yaml
# tebako-dwarfs/.github/workflows/ci.yml
jobs:
  test:
    uses: tamatebako/dwarfs-t/.github/workflows/_build-test-reusable.yml@main
    with:
      runner: ubuntu-latest
      triplet: ${{ matrix.triplet }}
      config: flatbuffers
      build-type: Release
      with-tests: true
      with-libdwarfs: true
      with-tools: false  # No tools needed for embedding
    strategy:
      matrix:
        triplet:
          - x64-linux-static
          - arm64-linux-static
          - x64-osx-static
          - arm64-osx-static
```

## Troubleshooting

### Common Developer Issues

**Issue**: Tests fail locally but pass on CI
**Solution**: Ensure you're using vcpkg mode, not system packages

**Issue**: "jemalloc not found"
**Solution**: The project requires Tebako jemalloc 5.5.0, not upstream 5.3.0

**Issue**: CI timeout
**Solution**: Use `--quick` mode for faster iteration

### Common Release Manager Issues

**Issue**: Release artifacts not uploaded
**Solution**: Check GitHub Actions logs, verify git tag format (vX.Y.Z)

**Issue**: Version mismatch
**Solution**: Ensure version is updated in both CMakeLists.txt and vcpkg.json

## Resources

- **TESTING.md**: Comprehensive testing guide
- **BUILD_SYSTEM_ARCHITECTURE.md**: Build system details
- **README.md**: Project overview
- **CHANGELOG.md**: Version history
- **GitHub**: https://github.com/tamatebako/dwarfs-t

## Appendix

### Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Code style
- `refactor`: Code change
- `test`: Tests
- `chore`: Build/auxiliary

**Example:**
```
feat(metadata): add compression level option

This allows users to specify compression level (1-9) when
creating DwarFS images.

Closes #123
```

### Version Numbering

- **Major**: Breaking changes (0.x.x → 1.0.0)
- **Minor**: New features, backward compatible (0.8.x → 0.9.0)
- **Patch**: Bug fixes (0.8.0 → 0.8.1)

### Branch Naming

- `feature/` - New features
- `fix/` - Bug fixes
- `refactor/` - Code refactoring
- `docs/` - Documentation
- `release/` - Release preparation
