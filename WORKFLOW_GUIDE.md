# DwarFS Developer & Release Manager Workflow Guide

This guide provides the complete workflow for developers and release managers working on Tebako DwarFS.

## Quick Reference

| Role | Command | Purpose |
|------|---------|---------|
| **Developer** | `./scripts/one-step/test-everything.sh --quick` | Quick validation before commit |
| **Developer** | `./scripts/one-step/test-everything.sh` | Full test suite |
| **Developer** | `./scripts/one-step/build-all.sh` | Build all configurations |
| **Release Manager** | `./scripts/orchestrator/release.sh --dry-run` | Pre-release validation |
| **Release Manager** | `./scripts/orchestrator/release.sh --version X.Y.Z` | Create release |

---

## Developer Workflow

### Initial Setup (One-Time)

```bash
# 1. Clone repository with submodules
git clone --recurse-submodules https://github.com/tamatebako/dwarfs.git
cd dwarfs

# 2. Install vcpkg (one-time)
git clone https://github.com/Microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh

# 3. Set environment (add to ~/.bashrc or ~/.zshrc)
export VCPKG_ROOT="$HOME/vcpkg"

# 4. Verify setup
./scripts/one-step/test-everything.sh --quick
```

### Daily Development Workflow

#### 1. Before Starting Work

```bash
# Pull latest changes
git pull origin main

# Clean build artifacts from previous session
./scripts/one-step/clean.sh --yes
```

#### 2. During Development

```bash
# Option A: Work with single configuration (faster)
BUILD_DIR=build-dev ./scripts/clean-build.sh -y
cd build-dev
ninja
ninja test

# Option B: Work with all configurations
./scripts/one-step/build-all.sh
```

#### 3. Before Committing

```bash
# Quick validation (2-5 minutes)
./scripts/one-step/test-everything.sh --quick

# If tests pass, commit
git add .
git commit -m "feat(description): description of changes"
```

#### 4. Before Pushing / Creating PR

```bash
# Full validation (10-20 minutes)
./scripts/one-step/test-everything.sh

# If all tests pass, push
git push origin feature-branch
```

### After Implementing New Features

Use this checklist:

```bash
# 1. Update tests
# - Add unit tests for new functionality
# - Update integration tests if needed
# - Add compatibility tests if format changed

# 2. Run full test suite
./scripts/one-step/test-everything.sh

# 3. Update documentation
# - Update relevant .md files
# - Update BUILD_SYSTEM_ARCHITECTURE.md if architectural change
# - Update TESTING.md if new tests added

# 4. Run benchmarks (if performance-critical)
./scripts/one-step/benchmark-quick.sh

# 5. Create PR with template
```

### Pull Request Template

```markdown
## Description
Brief description of changes.

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Performance improvement
- [ ] Refactoring
- [ ] Documentation
- [ ] Other (please describe)

## Testing
- [ ] Unit tests pass locally: `./scripts/one-step/test-everything.sh --quick`
- [ ] Full test suite passes: `./scripts/one-step/test-everything.sh`
- [ ] Added new tests for this change
- [ ] All existing tests still pass

## Compatibility
- [ ] Homebrew dwarfs can read images I create
- [ ] I can read Homebrew dwarfs images
- [ ] File extensions are correct (.dff, .dft)

## Performance
- [ ] Benchmarks run (if applicable)
- [ ] No significant performance regression

## Documentation
- [ ] Updated BUILD_SYSTEM_ARCHITECTURE.md (if architectural change)
- [ ] Updated TESTING.md (if new tests)
- [ ] Updated relevant documentation
```

---

## Release Manager Workflow

### Pre-Release Checklist

#### 1. Environment Verification

```bash
# Verify vcpkg is set up
echo $VCPKG_ROOT  # Should be set to vcpkg path

# Verify Tebako jemalloc
source scripts/lib/vcpkg_helper.sh
dwarfs_verify_tebako_jemalloc "$PWD"
```

#### 2. Full Test Suite Execution

```bash
# Clean all builds
./scripts/one-step/clean.sh --all --yes

# Run full test suite
./scripts/one-step/test-everything.sh

# Expected: All configurations pass
```

#### 3. Benchmark Execution (Optional but Recommended)

```bash
# Download test dataset
wget -O benchmark-files/perl-5.43.3.tar.gz \
  https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz
mkdir -p benchmark-files
tar -xzf benchmark-files/perl-5.43.3.tar.gz -C benchmark-files

# Run comprehensive benchmarks
export VCPKG_ROOT="$HOME/vcpkg"
./benchmarks/run_comprehensive_benchmark.sh

# Update README.md with results
```

#### 4. Compatibility Testing

```bash
# Ensure Homebrew dwarfs compatibility
# (Requires Homebrew dwarfs installation)
brew install dwarfs

# Test reading Homebrew images
./build-both-formats/mkdwarfs --list /path/to/homebrew/image.dft

# Test creating Homebrew-compatible images
./build-both-formats/mkdwarfs -i test/ -o test.dft --format=legacy-thrift
/homebrew/bin/dwarfs test.dft ls /
```

#### 5. Documentation Updates

```bash
# Update version numbers
# 1. Update CMakeLists.txt version
# 2. Update README.md version
# 3. Update CITATION.cff version
# 4. Update CHANGELOG.md

# Run release orchestrator (dry-run first)
./scripts/orchestrator/release.sh --dry-run
```

### Release Process

#### Step 1: Pre-Release Validation

```bash
#!/bin/bash
# pre-release-check.sh

set -e

echo "=== Pre-Release Validation ==="

# 1. Clean everything
echo "Cleaning build artifacts..."
./scripts/one-step/clean.sh --all --yes

# 2. Run full test suite
echo "Running full test suite..."
./scripts/one-step/test-everything.sh

# 3. Verify all builds
echo "Verifying builds..."
for dir in build-flatbuffers-only build-both-formats; do
  if [[ ! -x "$dir/mkdwarfs" ]]; then
    echo "ERROR: $dir/mkdwarfs not found or not executable"
    exit 1
  fi
done

# 4. Check documentation
echo "Checking documentation..."
# (Add checks for documentation completeness)

echo "=== Pre-Release Validation Complete ==="
```

#### Step 2: Create Release

```bash
# Update version
VERSION="0.14.2"

# Update version in files
sed -i '' "s/PROJECT_VERSION \"[^\"]*\"/PROJECT_VERSION \"$VERSION\"/" CMakeLists.txt
sed -i '' "s/version: \"[^\"]*\"/version: \"$VERSION\"/" CITATION.cff

# Commit version changes
git add CMakeLists.txt CITATION.cff CHANGELOG.md
git commit -m "chore(release): bump version to $VERSION"

# Create git tag
git tag -a "v$VERSION" -m "Release v$VERSION"

# Push to GitHub
git push origin main
git push origin "v$VERSION"
```

#### Step 3: GitHub Release

```bash
# Create GitHub release using gh CLI
gh release create "v$VERSION" \
  --title "DwarFS v$VERSION" \
  --notes "Release notes for v$VERSION" \
  --draft
```

### Post-Release Tasks

```bash
# 1. Announce release
# - Update website
# - Send announcement emails
# - Post to social media

# 2. Monitor issues
# - Watch for bug reports
# - Track adoption metrics

# 3. Begin next version
# - Create release/X.y.z branch
# - Update version to X.Y.Z-dev
```

---

## CI/CD Matrix Testing

### GitHub Actions Configuration

The `.github/workflows/ci.yml` tests across:

**Platforms:**
- macOS: arm64 (M1/M2/M3), x64 (Intel)
- Linux: arm64, x64
- Windows: x64 (planned)

**Configurations:**
- `flatbuffers-only` - FlatBuffers metadata only
- `both-formats` - FlatBuffers + Thrift

**Total Jobs:** 10+ test matrix combinations

### Adding New Platform to CI

1. Add triplet to `vcpkg_triplets/`
2. Add to `scripts/lib/vcpkg_helper.sh:dwarfs_list_triplets()`
3. Update `.github/workflows/ci.yml` matrix:

```yaml
strategy:
  matrix:
    include:
      - runner: ubuntu-latest
        os: linux
        arch: x64
        triplet: x64-linux
        config: flatbuffers-only
```

---

## Troubleshooting

### Build Failures

**Problem:** `jemalloc version mismatch`

```bash
# Solution: Always use vcpkg mode
export VCPKG_ROOT="$HOME/vcpkg"
./scripts/one-step/test-everything.sh --vcpkg
```

**Problem:** `CMake configuration failed`

```bash
# Solution: Clean and retry
./scripts/one-step/clean.sh --yes
./scripts/one-step/test-everything.sh
```

### Test Failures

**Problem:** Tests timeout

```bash
# Solution: Run tests with fewer jobs
JOBS=2 ./scripts/one-step/test-everything.sh --quick
```

**Problem:** Compatibility tests skipped

```bash
# Solution: Install Homebrew dwarfs
brew install dwarfs
```

---

## Remaining Work

### 1. Full Benchmarks

**Status:** Infrastructure ready, requires execution.

**Steps:**
```bash
# 1. Build with vcpkg (30-60 min first time)
export VCPKG_ROOT="$HOME/vcpkg"
./scripts/one-step/build-all.sh

# 2. Download dataset (~200MB)
wget -O benchmark-files/perl-5.43.3.tar.gz \
  https://www.cpan.org/src/5.0/perl-5.43.3.tar.gz
mkdir -p benchmark-files
tar -xzf benchmark-files/perl-5.43.3.tar.gz -C benchmark-files

# 3. Run benchmarks (2-3 hours)
./benchmarks/run_comprehensive_benchmark.sh

# 4. Update README.md
# Extract results and update performance section
```

### 2. Directory Structure Cleanup

**Status:** Scripts organized, source code pending.

**Reference:** `/Users/mulgogi/src/external/xz`

**Tasks:**
- [ ] Review `include/dwarfs/` structure
- [ ] Consolidate duplicate headers
- [ ] Remove deprecated code
- [ ] Organize by functionality (metadata/, reader/, writer/, etc.)

### 3. Legacy Workflow Conversion

**Status:** Core reusable workflows created.

**Remaining Workflows:**
- [ ] `benchmark-comprehensive.yml` → use reusable workflow
- [ ] `tebako-builds.yml` → use reusable workflow
- [ ] `linux-builds.yml` → use reusable workflow
- [ ] `macos-builds.yml` → use reusable workflow

**Template:**
```yaml
jobs:
  test:
    uses: ./.github/workflows/_build-test-reusable.yml
    with:
      triplet: ${{ matrix.triplet }}
      config: ${{ matrix.config }}
      runner: ${{ matrix.runner }}
```

---

## Summary: What To Do After Implementing Features

### For Developers:

1. **Run quick tests:** `./scripts/one-step/test-everything.sh --quick`
2. **Update tests** for new functionality
3. **Update documentation** (BUILD_SYSTEM_ARCHITECTURE.md, TESTING.md)
4. **Create PR** with the template provided
5. **Address review feedback**

### For Release Managers:

1. **Run pre-release validation:** `./scripts/orchestrator/release.sh --dry-run`
2. **Execute full test suite:** `./scripts/one-step/test-everything.sh`
3. **Run benchmarks** (if needed): `./benchmarks/run_comprehensive_benchmark.sh`
4. **Verify compatibility** with Homebrew dwarfs
5. **Update version numbers** and documentation
6. **Create git tag** and GitHub release
7. **Monitor post-release** issues

---

**Last Updated:** 2025-01-18
**Maintained By:** @tamatebako/dwarfs
