# DwarFS CI/CD Architecture - Visual Summary

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           DwarFS CI/CD Architecture                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        WORKFLOW LAYER (5 files)                     │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │                                                                      │   │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐   │   │
│  │  │ pr-validation    │  │ ci-main          │  │ release          │   │   │
│  │  │ (Fast feedback)  │  │ (Comprehensive)  │  │ (Artifacts)      │   │   │
│  │  │ • 4 jobs, ~15min │  │ • 10 jobs, ~45min│  │ • 5 platforms    │   │   │
│  │  │ • PR trigger     │  │ • Main branch    │  │ • Tag trigger    │   │   │
│  │  └────────┬─────────┘  └────────┬─────────┘  └────────┬─────────┘   │   │
│  │           │                     │                     │             │   │
│  │  ┌────────▼─────────┐  ┌────────▼─────────┐  ┌────────▼─────────┐   │   │
│  │  │ scheduled        │  │ manual           │  │                  │   │   │
│  │  │ (Weekly)         │  │ (On-demand)      │  │                  │   │   │
│  │  │ • 24+ jobs       │  │ • Flexible       │  │                  │   │   │
│  │  │ • Benchmarks     │  │ • Specific tests │  │                  │   │   │
│  │  └──────────────────┘  └──────────────────┘  └──────────────────┘   │   │
│  │                                                                      │   │
│  └──────────────────────────────┬───────────────────────────────────────┘   │
│                                   │                                         │
│                                   ▼                                         │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                   REUSABLE WORKFLOW LAYER (4 files)                │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │                                                                      │   │
│  │  ┌──────────────────────────────────────────────────────────────┐   │   │
│  │  │ _build-test-reusable.yml (ENHANCED)                         │   │   │
│  │  │ • Standard build + test workflow                            │   │   │
│  │  │ • Used by: pr-validation, ci-main, scheduled, manual       │   │   │
│  │  └──────────────────────────────────────────────────────────────┘   │   │
│  │                                                                      │   │
│  │  ┌──────────────────────────────────────────────────────────────┐   │   │
│  │  │ _build-release-reusable.yml (NEW)                            │   │   │
│  │  │ • Release build + packaging workflow                        │   │   │
│  │  │ • Used by: release                                           │   │   │
│  │  └──────────────────────────────────────────────────────────────┘   │   │
│  │                                                                      │   │
│  │  ┌──────────────────────────────────────────────────────────────┐   │   │
│  │  │ _compat-test-reusable.yml (NEW)                              │   │   │
│  │  │ • Homebrew compatibility test workflow                       │   │   │
│  │  │ • Used by: ci-main, scheduled                                │   │   │
│  │  └──────────────────────────────────────────────────────────────┘   │   │
│  │                                                                      │   │
│  │  ┌──────────────────────────────────────────────────────────────┐   │   │
│  │  │ _benchmark-reusable.yml (NEW)                                │   │   │
│  │  │ • Comprehensive benchmark workflow                           │   │   │
│  │  │ • Used by: scheduled, manual                                  │   │   │
│  │  └──────────────────────────────────────────────────────────────┘   │   │
│  │                                                                      │   │
│  └──────────────────────────────┬───────────────────────────────────────┘   │
│                                   │                                         │
│                                   ▼                                         │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     COMPOSITE ACTION LAYER (5 actions)              │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │                                                                      │   │
│  │  setup-vcpkg         configure-cmake      setup-build-deps         │   │
│  │       │                     │                    │                 │   │
│  │       └─────────────────────┴────────────────────┘                 │   │
│  │                             │                                      │   │
│  │                             ▼                                      │   │
│  │                    run-ctest    setup-homebrew-dwarfs              │   │
│  │                                                                      │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## MECE Categories

```
┌─────────────────────────────────────────────────────────────┐
│                    CONTINUOUS INTEGRATION (CI)              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  pr-validation.yml        ci-main.yml                       │
│  ├─ Trigger: PR          ├─ Trigger: Push to main          │
│  ├─ Scope: Fast          ├─ Scope: Comprehensive           │
│  ├─ Jobs: 4              ├─ Jobs: 10                       │
│  ├─ Time: ~15min         ├─ Time: ~45min                   │
│  └─ Purpose: Feedback    └─ Purpose: Full validation       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   CONTINUOUS DELIVERY (CD)                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  release.yml                                                 │
│  ├─ Trigger: Git tag (v*)                                  │
│  ├─ Scope: Release artifacts                                │
│  ├─ Jobs: 5 (one per platform)                             │
│  ├─ Time: ~30min                                            │
│  └─ Purpose: Create and publish releases                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    SCHEDULED & MANUAL                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  scheduled.yml            manual.yml                        │
│  ├─ Trigger: Weekly      ├─ Trigger: Manual dispatch        │
│  ├─ Scope: All tests     ├─ Scope: Flexible                │
│  ├─ Jobs: 24+            ├─ Jobs: Variable                 │
│  ├─ Time: 2-3 hours      ├─ Time: Variable                 │
│  └─ Purpose: Nightly     └─ Purpose: On-demand             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## DRY Principle: Single Source of Truth

```
BEFORE (Current State):
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   ci.yml        │     │  vcpkg-triplet  │     │ encoding-format │
│                 │     │    -matrix.yml  │     │    -test.yml    │
│ • Build steps   │     │                 │     │                 │
│ • CMake config  │     │ • Build steps   │     │ • Build steps   │
│ • Test setup    │     │ • CMake config  │     │ • Different vcpkg│
│                 │     │ • Test setup    │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
        │                       │                       │
        └───────────────────────┴───────────────────────┘
                                │
                    DUPLICATION: 3+ locations
                    MAINTENANCE BURDEN: High

AFTER (Target State):
┌─────────────────────────────────────────────────────────────────┐
│                  _build-test-reusable.yml                        │
│                                                                  │
│  • Build steps (ONE implementation)                             │
│  • CMake configuration (ONE implementation)                      │
│  • Test execution (ONE implementation)                           │
│                                                                  │
└──────────────────────────┬─────────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
┌─────────────┐   ┌─────────────┐   ┌─────────────┐
│pr-validation│   │  ci-main    │   │  scheduled  │
│    .yml     │   │    .yml     │   │    .yml     │
│             │   │             │   │             │
│ Just calls  │   │ Just calls  │   │ Just calls  │
│ reusable    │   │ reusable    │   │ reusable    │
│ with params │   │ with params │   │ with params │
└─────────────┘   └─────────────┘   └─────────────┘

MAINTENANCE BURDEN: Low (change ONE file)
```

## Comprehensive Coverage

### Platform Matrix

```
                    PR Validation    CI Main    Release    Scheduled

Ubuntu x64                ✅            ✅         ✅         ✅
Ubuntu ARM64              ❌            ✅         ✅         ✅
macOS x64                 ❌            ✅         ✅         ✅
macOS ARM64               ✅            ✅         ✅         ✅
Windows x64               ❌            ✅         ✅         ✅

Dynamic variants          ❌            ❌         ❌         ✅
```

### Configuration Matrix

```
                    PR Validation    CI Main    Release    Scheduled

FlatBuffers-only         ✅            ✅         ✅         ✅
Both-formats             ✅            ✅         ❌         ✅
```

### Test Type Matrix

```
                    PR Validation    CI Main    Scheduled

Unit Tests               ✅            ✅         ✅
Integration Tests        ❌            ✅         ✅
Compatibility Tests      ❌            ✅         ✅
Benchmarks               ❌            ❌         ✅ (opt)
```

## Pipelined Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                         DEVELOPER FLOW                          │
└─────────────────────────────────────────────────────────────────┘

                              ┌─────────────┐
                              │ Open PR     │
                              └──────┬──────┘
                                     │
                                     ▼
                              ┌─────────────┐
                              │pr-validation │  ← Fast feedback (~15 min)
                              │  (4 jobs)   │
                              └──────┬──────┘
                                     │
                                     ▼
                              ┌─────────────┐
                              │   SUCCESS   │
                              └──────┬──────┘
                                     │
                                     ▼
                              ┌─────────────┐
                              │ Merge PR    │
                              └──────┬──────┘
                                     │
                                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                         MAIN BRANCH FLOW                         │
└─────────────────────────────────────────────────────────────────┘

                              ┌─────────────┐
                              │ Push to    │
                              │ main       │
                              └──────┬──────┘
                                     │
                                     ▼
                              ┌─────────────┐
                              │  ci-main    │  ← Full validation (~45 min)
                              │ (10 jobs)   │
                              └──────┬──────┘
                                     │
                                     ▼
                              ┌─────────────┐
                              │   SUCCESS   │
                              └──────┬──────┘
                                     │
                    ┌────────────────┴────────────────┐
                    │                                 │
                    ▼                                 ▼
┌───────────────────────┐             ┌───────────────────────────┐
│ Weekly (scheduled)    │             │ Tag for Release           │
│                       │             │                           │
│ • Full matrix (24+)   │             │ 1. Create tag v1.2.3      │
│ • Benchmarks (opt)    │             │ 2. release.yml runs       │
│ • Compatibility       │             │ 3. Build 5 artifacts      │
│                       │             │ 4. Publish GitHub release  │
└───────────────────────┘             └───────────────────────────┘
```

## File Structure (Before → After)

```
BEFORE (19 files):
.github/workflows/
├── build.yml                    (90% disabled, deprecate)
├── build-test.yml               (superseded, deprecate)
├── ci.yml                       (active, rename to ci-main.yml)
├── vcpkg-triplet-matrix.yml     (active, merge into scheduled.yml)
├── compat-test.yml              (active, merge into ci-main.yml)
├── benchmark-comprehensive.yml (keep for manual runs)
├── encoding-format-test.yml     (keep for validation)
├── linux-builds.yml             (deprecated, deprecate)
├── macos-builds.yml             (deprecated, deprecate)
├── windows-builds.yml           (deprecated, deprecate)
├── tebako-build*.yml            (keep, Tebako-specific)
├── support-jobs.yml             (keep, specialized)
├── install-dependencies.yml     (now composite action, deprecate)
├── _build-test-reusable.yml     (keep, enhance)
└── ... (10+ more files)

AFTER (9 core files):
.github/workflows/
├── pr-validation.yml            (NEW, fast PR feedback)
├── ci-main.yml                  (NEW, comprehensive CI)
├── release.yml                  (NEW, release artifacts)
├── scheduled.yml                (NEW, weekly comprehensive)
├── manual.yml                   (NEW, on-demand testing)
├── _build-test-reusable.yml     (ENHANCED, core build/test)
├── _build-release-reusable.yml  (NEW, release builds)
├── _compat-test-reusable.yml    (NEW, compatibility)
└── _benchmark-reusable.yml      (NEW, benchmarks)

.github/actions/
├── setup-vcpkg/                 (EXISTING, standardize usage)
├── configure-cmake/             (EXISTING, standardize usage)
├── setup-build-deps/            (EXISTING)
├── run-ctest/                   (EXISTING)
└── setup-homebrew-dwarfs/       (EXISTING)
```

## Key Benefits

1. **MECE**: Clear separation of concerns (CI, CD, Scheduled, Manual)
2. **DRY**: Single reusable workflow for all build/test operations
3. **Comprehensive**: All platforms, configs, and test types covered
4. **Pipelined**: Clear flow from PR → Merge → Release
5. **Maintainable**: 5 core workflows instead of 19 fragmented files
6. **Fast Feedback**: PR validation in ~15 minutes instead of ~45 minutes

## Success Metrics

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Core workflow files | 19 | 5 | 74% reduction |
| Code duplication | 3+ locations | 0 | 100% reduction |
| PR validation time | ~45 min | ~15 min | 67% faster |
| Maintenance burden | High | Low | Single source of truth |
| Platform coverage | Partial | Complete | All 5 platforms |
| Test type coverage | Partial | Complete | All test types |
