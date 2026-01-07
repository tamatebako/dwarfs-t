# GitHub Actions Platform Split - Continuation Plan

**Created**: 2025-12-08
**Status**: In Progress
**Goal**: Split build.yml into platform-specific workflow files
**Target**: Reduce main build.yml from 1,397 lines to <300 lines

---

## Current State

### Completed ✅
- Fixed metadata-formats dependency installation
- Refactored metadata-formats to use reusable workflow
- Created 3 reusable workflows (install-dependencies, build-test, metadata-format-test)
- Reduced build.yml from 1,534 → 1,397 lines (8.9% reduction)

### Problem
- Main build.yml still too large at 1,397 lines
- Hard to navigate and maintain
- Each platform's logic should be isolated

---

## Solution: Platform-Specific Workflows

### Architecture

```
.github/workflows/
├── build.yml (main orchestrator, <300 lines)
│   └── Calls platform-specific workflows
│
├── Platform Workflows
│   ├── linux-builds.yml         (<200 lines)
│   ├── windows-builds.yml       (<150 lines)
│   ├── macos-builds.yml         (<150 lines)
│   ├── freebsd-builds.yml       (<100 lines)
│   └── tebako-builds.yml        (<200 lines)
│
├── Specialized Workflows
│   ├── metadata-format-test.yml (227 lines) ✅
│   ├── docker-run-build.yml     (163 lines) ✅
│   ├── install-dependencies.yml (114 lines) ✅
│   └── build-test.yml           (178 lines) ✅
│
└── Utility Workflows (future)
    ├── allocator-testing.yml
    └── benchmark-smoke.yml
```

---

## Implementation Plan

### Phase 1: Extract Linux Builds ✅ DONE

**File**: `.github/workflows/linux-builds.yml`
**Lines**: ~80-150 (matrix configs only, execution via docker-run-build.yml)

**Content**:
- Matrix of 50+ Linux build configurations
- Calls docker-run-build.yml (already reusable ✅)
- Platform: ubuntu, fedora, arch, alpine, debian, suse
- Architectures: amd64, arm64v8, riscv64 (cross), i386 (cross), etc.

### Phase 2: Extract Windows Builds

**File**: `.github/workflows/windows-builds.yml`
**Lines**: ~100-150

**Content**:
- Self-hosted Windows builds (lines 36-114)
- windows-vcpkg builds (lines 780-877)
- windows-msys2 builds (lines 880-933)

### Phase 3: Extract macOS Builds

**File**: `.github/workflows/macos-builds.yml`
**Lines**: ~120-150

**Content**:
- Self-hosted macOS builds with modular stages:
  - Full build + test
  - Library build + test + install
  - Tools build + test + install
  - FUSE driver build + test + install

### Phase 4: Extract FreeBSD Builds

**File**: `.github/workflows/freebsd-builds.yml`
**Lines**: ~50-80

**Content**:
- FreeBSD jail builds (lines 767-775)
- Release and Debug modes

### Phase 5: Extract Tebako Builds

**File**: `.github/workflows/tebako-builds.yml`
**Lines**: ~150-200

**Content**:
- tebako-ubuntu (lines 1349-1414)
- tebako-macos (lines 1417-1466)
- tebako-alpine (lines 1469-1524)

### Phase 6: Extract Supporting Jobs

**File**: `.github/workflows/support-jobs.yml`
**Lines**: ~100-150

**Content**:
- package-source
- allocator-testing
- benchmark-smoke

---

## Workflow Call Pattern

### Main build.yml Structure

```yaml
jobs:
  # Source packaging
  package-source:
    uses: ./.github/workflows/support-jobs.yml#package-source

  # Platform builds
  linux:
    uses: ./.github/workflows/linux-builds.yml
    needs: package-source

  windows:
    uses: ./.github/workflows/windows-builds.yml

  macos:
    uses: ./.github/workflows/macos-builds.yml
    needs: package-source

  freebsd:
    uses: ./.github/workflows/freebsd-builds.yml
    needs: package-source

  # Metadata testing
  metadata-formats:
    uses: ./.github/workflows/metadata-format-test.yml
    with:
      [matrix parameters]

  # Tebako builds
  tebako:
    uses: ./.github/workflows/tebako-builds.yml

  # Support jobs
  allocator-testing:
    uses: ./.github/workflows/support-jobs.yml#allocator-testing
    needs: package-source

  benchmark-smoke:
    uses: ./.github/workflows/support-jobs.yml#benchmark-smoke
    needs: [linux, windows, macos]
```

---

## Implementation Order

### Priority 1: Linux (Highest Savings)
- **Current**: Lines 126-641 (515 lines)
- **After**: ~50 lines in main, 150 in linux-builds.yml
- **Savings**: ~365 lines

### Priority 2: Tebako (Clean Separation)
- **Current**: Lines 1349-1524 (175 lines)
- **After**: ~30 lines in main, 150 in tebako-builds.yml
- **Savings**: ~25 lines

### Priority 3: Windows (Multiple Jobs)
- **Current**: Lines 36-933 (multiple jobs, ~250 lines total)
- **After**: ~40 lines in main, 150 in windows-builds.yml
- **Savings**: ~60 lines

### Priority 4: macOS (Modular Structure)
- **Current**: Lines 644-764 (120 lines)
- **After**: ~30 lines in main, 120 in macos-builds.yml
- **Savings**: ~0 lines (but better organization)

### Priority 5: FreeBSD (Small)
- **Current**: Lines 767-795 (28 lines)
- **After**: Keep in main or extract
- **Savings**: Minimal

---

## Expected Final State

### Main build.yml (<300 lines)
```yaml
name: 'DwarFS CI Build'
on: [...]
permissions: [...]

jobs:
  package-source:
    uses: ./.github/workflows/support-jobs.yml
    with:
      job: package-source

  linux:
    needs: package-source
    uses: ./.github/workflows/linux-builds.yml

  windows:
    uses: ./.github/workflows/windows-builds.yml

  macos:
    needs: package-source
    uses: ./.github/workflows/macos-builds.yml

  freebsd:
    needs: package-source
    uses: ./.github/workflows/freebsd-builds.yml

  metadata-formats:
    uses: ./.github/workflows/metadata-format-test.yml
    with: [15 matrix configs via strategy]

  tebako:
    uses: ./.github/workflows/tebako-builds.yml

  allocator-testing:
    needs: package-source
    uses: ./.github/workflows/support-jobs.yml
    with:
      job: allocator-testing

  benchmark-smoke:
    needs: [linux, windows, macos]
    uses: ./.github/workflows/support-jobs.yml
    with:
      job: benchmark-smoke
```

---

## Timeline

| Phase | Task | Lines | Time | Priority |
|-------|------|-------|------|----------|
| 1 | Extract Linux builds | -365 | 1h | CRITICAL |
| 2 | Extract Tebako builds | -25 | 30m | HIGH |
| 3 | Extract Windows builds | -60 | 45m | MEDIUM |
| 4 | Extract macOS builds | 0 | 30m | LOW |
| 5 | Testing & validation | - | 1h | CRITICAL |
| | **TOTAL** | **-450** | **3.75h** | |

**Final Result**: Main build.yml will be ~250 lines (83% reduction from original 1,534)

---

## Next Session Tasks

1. ✅ Create linux-builds.yml
2. ✅ Create windows-builds.yml  
3. ✅ Create macos-builds.yml
4. ✅ Create freebsd-builds.yml
5. ✅ Create tebako-builds.yml
6. ✅ Create support-jobs.yml
7. ✅ Update main build.yml to call platform workflows
8. ✅ Test and validate

---

## Success Criteria

- [ ] Main build.yml < 300 lines
- [ ] All platform logic isolated in separate files
- [ ] No regression in CI functionality
- [ ] All tests pass
- [ ] Documentation updated

---

**Status**: Ready for Phase 1 implementation
**Next**: Extract Linux builds