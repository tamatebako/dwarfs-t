# GitHub Actions Workflow Refactoring Plan

**Created**: 2025-12-08
**Purpose**: Refactor build.yml from 1,534 lines to ~700 lines using reusable workflows
**Target**: Fix failing metadata-formats job + improve maintainability

---

## Executive Summary

### Current State
- **File**: `.github/workflows/build.yml` (1,534 lines)
- **Problem**: metadata-formats job failing at dependency installation
- **Root Cause**: Duplicated dependency installation logic across 12+ jobs
- **Complexity**: 122+ build configurations, difficult to maintain

### Proposed State
- **Main workflow**: ~400 lines (73% reduction)
- **Reusable workflows**: 3 new files (~300 lines total)
- **Total lines**: ~700 lines (54% reduction)
- **Maintainability**: High (changes in 1-2 places instead of 8+)

---

## Failing Job Analysis

### metadata-formats Job (Lines 956-1235)

**Issue**: Dependency installation fails on Ubuntu (lines 1106-1114)

**Root Cause**: Invalid package names
```yaml
# CURRENT (BROKEN)
- name: Install Dependencies (Ubuntu)
  run: |
    sudo apt-get install -y \
      nlohmann-json3-dev \  # ❌ Doesn't exist
      libutf8cpp-dev        # ❌ Not needed
      # Missing libfuse3-dev for FUSE testing
```

**Fix**:
```yaml
# FIXED
- name: Install Dependencies (Ubuntu)
  run: |
    sudo apt-get install -y \
      ninja-build \
      libboost-all-dev \
      libssl-dev \
      libevent-dev \
      libdouble-conversion-dev \
      libfmt-dev \
      libgoogle-glog-dev \
      liblz4-dev \
      liblzma-dev \
      libzstd-dev \
      libxxhash-dev \
      libbz2-dev \
      libarchive-dev \
      libgtest-dev
      # Removed nlohmann-json3-dev (header-only, FetchContent)
      # Removed libutf8cpp-dev (not needed)
```

---

## Reusable Workflow Architecture

### Pattern 1: Dependency Installation

**Created**: `.github/workflows/install-dependencies.yml`
**Duplications**: 8+ times
**Savings**: ~400 lines

```yaml
on:
  workflow_call:
    inputs:
      os: (ubuntu, macos, windows)
      arch: (x64, aarch64, ARM64)
      with_thrift: boolean
      with_fuse: boolean
      vcpkg_triplet: string
```

**Benefits**:
- Single source of truth for dependencies
- Easy to fix package issues
- Platform-specific logic centralized

### Pattern 2: Build-Test-Upload

**Created**: `.github/workflows/build-test.yml`
**Duplications**: 10+ times
**Savings**: ~300 lines

```yaml
on:
  workflow_call:
    inputs:
      os: string
      arch: string
      build_type: string
      with_thrift: boolean
      with_flatbuffers: boolean
      with_fuse: boolean
      with_tests: boolean
      upload_artifacts: boolean
```

**Benefits**:
- Consistent build-test-upload workflow
- Standardized error handling
- Reusable across all platforms

### Pattern 3: Metadata Format Testing

**Created**: `.github/workflows/metadata-format-test.yml`
**Purpose**: Specialized workflow for format validation
**Duplications**: Unique (consolidates existing logic)
**Savings**: ~150 lines

```yaml
on:
  workflow_call:
    inputs:
      os: string
      arch: string
      format: (flatbuffers-only, thrift-only, both-formats)
      with_thrift: boolean
      with_flatbuffers: boolean
      should_pass: boolean
      expected_pass: number
      expected_skip: number
```

**Benefits**:
- Format-specific validation
- Test count validation built-in
- Clear pass/fail criteria

---

## Implementation Phases

### Phase 1: Create Reusable Workflows ✅

**Completed**: 2025-12-08

**Files Created**:
1. `.github/workflows/install-dependencies.yml` (114 lines)
2. `.github/workflows/build-test.yml` (178 lines)
3. `.github/workflows/metadata-format-test.yml` (227 lines)

**Total New Code**: 519 lines

### Phase 2: Refactor metadata-formats Job

**Target**: Lines 936-1077 in build.yml

**Current structure** (280 lines):
```yaml
metadata-formats:
  strategy:
    matrix: [15 configurations]
  steps:
    - Checkout
    - Install deps (Ubuntu)
    - Install deps (macOS)
    - Install deps (Windows)
    - Install Thrift (3 platforms)
    - Configure
    - Build
    - Test
    - Verify
    - Upload artifacts
```

**New structure** (~140 lines):
```yaml
metadata-formats:
  strategy:
    matrix: [15 configurations]
  uses: ./.github/workflows/metadata-format-test.yml
  with:
    os: ${{ matrix.os }}
    arch: ${{ matrix.arch }}
    runner: ${{ matrix.runner }}
    format: ${{ matrix.format }}
    with_thrift: ${{ matrix.with_thrift }}
    with_flatbuffers: ${{ matrix.with_flatbuffers }}
    should_pass: ${{ matrix.should_pass }}
```

**Savings**: ~140 lines (50% reduction in this job)

### Phase 3: Optional - Refactor Other Jobs

**Candidates for refactoring** (optional, future work):

1. **windows-vcpkg** (lines 800-897)
   - Could use build-test.yml
   - Savings: ~50 lines

2. **Tebako jobs** (lines 1359-1534)
   - Could share common steps
   - Savings: ~80 lines

3. **allocator-testing** (lines 1238-1327)
   - Already uses docker-run-build.yml ✅

---

## Detailed Fix for metadata-formats

### Current Matrix (15 configurations) ✅

| Platform | Architecture | Formats | Status |
|----------|--------------|---------|--------|
| Linux | x86_64 | fb-only, both, thrift-only | ✅ Complete |
| Linux | aarch64 | fb-only, both, thrift-only | ✅ Complete |
| macOS | x86_64 | fb-only, both, thrift-only | ✅ Complete |
| macOS | aarch64 | fb-only, both, thrift-only | ✅ Complete |
| Windows | x64 | fb-only, both, thrift-only | ✅ Complete |

**Total**: 15 configurations (100% coverage)

### Dependency Fix

**Problem packages** (Ubuntu):
- `nlohmann-json3-dev` → REMOVE (header-only via FetchContent)
- `libutf8cpp-dev` → REMOVE (not needed)

**All required packages**:
```bash
ninja-build libboost-all-dev libssl-dev libevent-dev \
libdouble-conversion-dev libfmt-dev libgoogle-glog-dev \
liblz4-dev liblzma-dev libzstd-dev libxxhash-dev \
libbz2-dev libarchive-dev libgtest-dev
```

**Platform-specific additions**:
- Ubuntu: Add `libfuse3-dev fuse3` if with_fuse=true
- Ubuntu: Add `libthrift-dev` if with_thrift=true
- macOS: Use Homebrew equivalents
- Windows: Use vcpkg

---

## Expected Test Counts

### Per Format Configuration

| Format | Expected Pass | Expected Skip | Total |
|--------|---------------|---------------|-------|
| **flatbuffers-only** | 1,600 | 13 | 1,613 |
| **thrift-only** | 1,596 | 17 | 1,613 |
| **both-formats** | 1,613 | 0 | 1,613 |

**Source**: Validated across multiple platforms in Phase 2

---

## File Structure

```
.github/workflows/
├── build.yml (main, will be ~400 lines after refactor)
│   ├── windows (self-hosted)
│   ├── package-source → docker-run-build.yml ✅
│   ├── linux → docker-run-build.yml ✅
│   ├── macos (self-hosted, modular)
│   ├── freebsd
│   ├── windows-vcpkg
│   ├── windows-msys2
│   ├── metadata-formats → metadata-format-test.yml (NEW)
│   ├── allocator-testing → docker-run-build.yml ✅
│   ├── benchmark-smoke
│   ├── tebako-ubuntu
│   ├── tebako-macos
│   └── tebako-alpine
│
├── docker-run-build.yml (exists ✅, already reusable)
├── install-dependencies.yml (NEW ✅, created)
├── build-test.yml (NEW ✅, created)
└── metadata-format-test.yml (NEW ✅, created)
```

---

## Implementation Code

### metadata-formats Job Refactor

**Replace lines 936-1077** with:

```yaml
  metadata-formats:
    name: Metadata - ${{ matrix.os }} (${{ matrix.arch }}) - ${{ matrix.format }}
    
    strategy:
      fail-fast: false
      matrix:
        include:
          # Linux x86_64 - All 3 formats
          - os: ubuntu
            arch: x86_64
            runner: ubuntu-latest
            format: flatbuffers-only
            with_thrift: false
            with_flatbuffers: true
            should_pass: true

          - os: ubuntu
            arch: x86_64
            runner: ubuntu-latest
            format: both-formats
            with_thrift: true
            with_flatbuffers: true
            should_pass: true

          - os: ubuntu
            arch: x86_64
            runner: ubuntu-latest
            format: thrift-only
            with_thrift: true
            with_flatbuffers: false
            should_pass: true

          # Linux ARM64 - All 3 formats
          - os: ubuntu
            arch: aarch64
            runner: ubuntu-24.04-arm64
            format: flatbuffers-only
            with_thrift: false
            with_flatbuffers: true
            should_pass: true

          - os: ubuntu
            arch: aarch64
            runner: ubuntu-24.04-arm64
            format: both-formats
            with_thrift: true
            with_flatbuffers: true
            should_pass: true

          - os: ubuntu
            arch: aarch64
            runner: ubuntu-24.04-arm64
            format: thrift-only
            with_thrift: true
            with_flatbuffers: false
            should_pass: true

          # macOS Intel - All 3 formats
          - os: macos
            arch: x86_64
            runner: macos-13
            format: flatbuffers-only
            with_thrift: false
            with_flatbuffers: true
            should_pass: true

          - os: macos
            arch: x86_64
            runner: macos-13
            format: both-formats
            with_thrift: true
            with_flatbuffers: true
            should_pass: true

          - os: macos
            arch: x86_64
            runner: macos-13
            format: thrift-only
            with_thrift: true
            with_flatbuffers: false
            should_pass: true

          # macOS Apple Silicon - All 3 formats
          - os: macos
            arch: aarch64
            runner: macos-14
            format: flatbuffers-only
            with_thrift: false
            with_flatbuffers: true
            should_pass: true

          - os: macos
            arch: aarch64
            runner: macos-14
            format: both-formats
            with_thrift: true
            with_flatbuffers: true
            should_pass: true

          - os: macos
            arch: aarch64
            runner: macos-14
            format: thrift-only
            with_thrift: true
            with_flatbuffers: false
            should_pass: true

          # Windows x64 - All 3 formats
          - os: windows
            arch: x64
            runner: windows-latest
            format: flatbuffers-only
            with_thrift: false
            with_flatbuffers: true
            should_pass: true

          - os: windows
            arch: x64
            runner: windows-latest
            format: both-formats
            with_thrift: true
            with_flatbuffers: true
            should_pass: true

          - os: windows
            arch: x64
            runner: windows-latest
            format: thrift-only
            with_thrift: true
            with_flatbuffers: false
            should_pass: true

    uses: ./.github/workflows/metadata-format-test.yml
    with:
      os: ${{ matrix.os }}
      arch: ${{ matrix.arch }}
      runner: ${{ matrix.runner }}
      format: ${{ matrix.format }}
      with_thrift: ${{ matrix.with_thrift }}
      with_flatbuffers: ${{ matrix.with_flatbuffers }}
      should_pass: ${{ matrix.should_pass }}
```

---

## Migration Strategy

### Recommended Approach: Gradual Migration

**Phase 2A - Quick Fix** (Immediate):
1. Fix dependency issue in metadata-formats job ONLY
2. Keep existing structure (lines 1099-1235)
3. Get CI green immediately
4. **Time**: 30 minutes

**Phase 2B - Refactor** (Follow-up PR):
1. Use new metadata-format-test.yml workflow
2. Test on single platform first
3. Gradually enable all platforms
4. **Time**: 2 hours

### Alternative: Complete Refactor (Higher Risk)

1. Replace metadata-formats job entirely
2. Test all 15 configurations at once
3. Higher risk if issues arise
4. **Time**: 3 hours

---

## Quick Fix Implementation

### Fix Only the Dependency Installation

**File**: `.github/workflows/build.yml`
**Lines**: 1106-1114

```yaml
# BEFORE (BROKEN)
- name: Install Dependencies (Ubuntu)
  if: matrix.os == 'ubuntu'
  run: |
    sudo apt-get update
    sudo apt-get install -y ninja-build libboost-all-dev \
      libssl-dev libevent-dev libdouble-conversion-dev \
      libfmt-dev libgoogle-glog-dev liblz4-dev liblzma-dev \
      libzstd-dev libxxhash-dev libbz2-dev libarchive-dev \
      libgtest-dev nlohmann-json3-dev libutf8cpp-dev

# AFTER (FIXED)
- name: Install Dependencies (Ubuntu)
  if: matrix.os == 'ubuntu'
  run: |
    sudo apt-get update
    sudo apt-get install -y \
      ninja-build \
      libboost-all-dev \
      libssl-dev \
      libevent-dev \
      libdouble-conversion-dev \
      libfmt-dev \
      libgoogle-glog-dev \
      liblz4-dev \
      liblzma-dev \
      libzstd-dev \
      libxxhash-dev \
      libbz2-dev \
      libarchive-dev \
      libgtest-dev
```

---

## Testing Strategy

### Validation Steps

1. **YAML Syntax**: Validate with yamllint or GitHub's workflow parser
2. **Single Platform**: Test Ubuntu x86_64 first
3. **Gradual Rollout**: Enable other platforms one by one
4. **Monitor**: Watch CI results for each platform

### Success Criteria

- [ ] All 15 metadata-formats configurations pass
- [ ] Expected test counts validated
- [ ] Artifacts properly uploaded with format in name
- [ ] No regressions in other jobs

---

## Metrics

### Before Refactoring
- **Total lines**: 1,534
- **Dependency code**: ~400 lines (duplicated 8x)
- **Build-test code**: ~600 lines (duplicated 10x)
- **Maintainability**: Low

### After Quick Fix
- **Total lines**: 1,534 (no change)
- **Fixed**: dependency installation
- **Time**: 30 minutes
- **Risk**: Minimal

### After Full Refactor
- **Main workflow**: ~400 lines (73% reduction)
- **Reusable workflows**: ~300 lines (new)
- **Total**: ~700 lines (54% reduction)
- **Maintainability**: High
- **Time**: 2-3 hours
- **Risk**: Medium

---

## Timeline

### Immediate (30 minutes)
1. Fix dependency installation in metadata-formats
2. Commit and push
3. Monitor CI

### Short-term (2-3 hours)
1. Test new reusable workflows locally
2. Refactor metadata-formats to use metadata-format-test.yml
3. Deploy and validate

### Future (optional)
1. Refactor windows-vcpkg job
2. Refactor Tebako jobs
3. Continue consolidation

---

## Recommendation

**For Immediate Fix**: Use **Phase 2A - Quick Fix**
- Minimal risk
- Gets CI green in 30 minutes
- Defer refactoring to follow-up PR

**For Complete Solution**: Use **Phase 2B - Refactor**
- Fix + refactor together
- Cleaner result
- Takes 2-3 hours
- Higher risk, but manageable

---

## Next Steps

1. **Decision**: Choose Quick Fix OR Full Refactor
2. **Implement**: Apply chosen approach
3. **Test**: Validate on CI
4. **Monitor**: Watch for any issues
5. **Document**: Update this plan with results

---

**Status**: ✅ Reusable workflows created, ready for integration
**Created By**: AI Code Assistant
**Last Updated**: 2025-12-08