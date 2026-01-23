# CI Architecture Implementation Plan

**Created:** 2026-01-23
**Status:** In Progress
**Objective:** Implement modern CI architecture with CMakePresets, run-cmake, and proper caching per triplet

---

## Part 1: Warnings Analysis (from CI run 21270972546)

### Critical Warnings (Need Fixing)

| Category | Count | Location | Impact |
|----------|-------|----------|--------|
| **Unused variables** | 30+ | `frozen2_deserializer.cpp` | Code cleanliness |
| **Unused parameters** | 5+ | `value_encoders.cpp`, `metadata_*` | Code cleanliness |
| **Float comparison** | ~20 | `folly_compat.h:372,447` | Potential safety issues |
| **Unused function** | 1 | `features.cpp:68` | Dead code |
| **CMake deprecation** | 1 | `CMakeLists.txt:43` | Future compatibility |

### Medium Priority Warnings

| Category | Count | Location | Action |
|----------|-------|----------|--------|
| **Man page generator not found** | 4 | `manpage.cmake:25` | Install `ronn` or suppress warning |
| **Git tags not found** | 1 | `version.cmake:99` | Expected in CI |
| **CMake syntax warning** | 1 | `packaging.cmake:148` | Fix column 90 issue |
| **vcpkg unused variables** | 3 | vcpkg.cmake | Not our code (vcpkg issue) |

### Low Priority (Can Ignore)

| Category | Location | Reason |
|----------|----------|--------|
| **xxhash not found** | setup-build-deps | Optional dependency, already handled |
| **Package manager detection** | actions/cache | GitHub Actions internal |
| **x-gha backend removed** | vcpkg | Warning only, still works |
| **vcpkg port warnings** | jemalloc, fbthrift | Overlay ports, not critical |

---

## Part 2: New CI Architecture

### 2.1 Overview

**Current Architecture Problems:**
1. Not using CMakePresets.json - using custom composite actions
2. Binary cache not fully utilized - coarse-grained caching
3. Dependencies rebuilt unnecessarily - no shared deps between production/experimental
4. CMake/Ninja not cached - downloaded each time

**New Architecture:**
```
┌─────────────────────────────────────────────────────────────┐
│                    GitHub Actions Workflow                  │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  get-cmake   │  │  run-vcpkg   │  │  run-cmake   │      │
│  │              │  │              │  │              │      │
│  │ • CMake      │  │ • vcpkg      │  │ • Configure  │      │
│  │   cached     │  │   cached     │  │ • Build      │      │
│  │ • Ninja      │  │ • Binary     │  │ • Test       │      │
│  │   cached     │  │   cache      │  │              │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│         │                  │                  │              │
│         └──────────────────┴──────────────────┘              │
│                            │                                 │
│                     ┌──────▼──────┐                          │
│                     │ CMakePresets│                          │
│                     │   .json     │                          │
│                     └─────────────┘                          │
└─────────────────────────────────────────────────────────────┘

Binary Cache (per triplet):
  ┌─────────────────────────────────────────────┐
  │ x64-linux-production  │ x64-linux-experimental│
  │ x64-osx-production    │ x64-osx-experimental  │
  │ x64-windows-production│ ...                  │
  └─────────────────────────────────────────────┘
```

### 2.2 CMakePresets.json Structure

**Key Design Principles:**
1. **Per-triplet presets** - Each OS/arch combination has its own preset
2. **Layered configuration** - Base → Production/Experimental
3. **Cache variables** - Enable CMake caching for faster reconfigure
4. **Local reproducibility** - Works on developer machines

```json
{
  "version": 3,
  "cmakeMinimumRequired": "3.28",

  "include": [
    "cmake/presets/base.json",
    "cmake/presets/triplets.json"
  ]
}
```

**Directory Structure:**
```
cmake/presets/
├── CMakePresets.json       # Main entry point
├── base.json               # Base common settings
├── triplets/               # Per-OS/arch configurations
│   ├── linux-x64.json
│   ├── linux-x64-dynamic.json
│   ├── osx-x64.json
│   ├── osx-arm64.json
│   └── windows-x64.json
└── configs/                # Production/Experimental overlay
    ├── production.json
    └── experimental.json
```

### 2.3 vcpkg Dependency Split

**vcpkg.json (Base dependencies - shared by all configs):**
```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "dwarfs",
  "version-string": "0.0.0",
  "dependencies": [
    {"name": "fmt", "version>=": "10.0"},
    {"name": "flatbuffers", "version>=": "23.5"},
    {"name": "xxhash", "version>=": "0.8.1"},
    {"name": "boost-system", "version>=": "1.67.0"},
    {"name": "boost-filesystem", "version>=": "1.67.0"},
    {"name": "boost-program-options", "version>=": "1.67.0"},
    {"name": "boost-iostreams", "version>=": "1.67.0"},
    {"name": "libarchive", "version>=": "3.6.0"},
    {"name": "sqlite3", "version>=": "3.0.0"},
    {"name": "libxml2", "version>=": "2.9.0"},
    {"name": "zstd", "version>=": "1.4.8"},
    {"name": "lz4", "version>=": "1.9.3"},
    {"name": "liblzma", "version>=": "5.2.5"},
    {"name": "brotli", "version>=": "1.0.9"},
    {"name": "libmagic", "version>=": "5.38"},
    {"name": "flac", "version>=": "1.4.2"},
    {"name": "googletest", "version>=": "1.13.0"}
  ]
}
```

**vcpkg-experimental.json (Modern Thrift dependencies):**
```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "dwarfs-experimental",
  "version-string": "0.0.0",
  "dependencies": [
    {"name": "jemalloc", "version>=": "5.5.0"},
    {"name": "folly", "version>=": "2025.01.06.00"},
    {"name": "fizz", "version>=": "2024.12.01.00"},
    {"name": "wangle", "version>=": "2024.12.02.00"},
    {"name": "fbthrift", "version>=": "2025.01.06.00"}
  ]
}
```

### 2.4 CI Workflow Structure

**_build-test-reusable.yml (New):**
```yaml
jobs:
  build-and-test:
    steps:
      - uses: lukka/get-cmake@latest

      - name: Setup vcpkg (production)
        if: inputs.config == 'production'
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgJsonGlob: '**/vcpkg.json'

      - name: Setup vcpkg (experimental)
        if: inputs.config == 'experimental'
        uses: lukka/run-vcpkg@v11
        with:
          vcpkgJsonGlob: '**/vcpkg-experimental.json'

      - uses: lukka/run-cmake@v10
        with:
          configurePreset: ${{ inputs.triplet }}-${{ inputs.config }}
          buildPreset: ${{ inputs.triplet }}-${{ inputs.config }}
          testPreset: ${{ inputs.triplet }}-${{ inputs.config }}
```

---

## Part 3: Per-Triplet Configurations

### 3.1 Supported Triplets Matrix

| OS | Architecture | Triplet | Presets |
|----|-------------|---------|---------|
| Linux | x64 | `x64-linux` | `x64-linux-production`, `x64-linux-experimental` |
| Linux | x64 | `x64-linux-dynamic` | `x64-linux-dynamic-production`, `x64-linux-dynamic-experimental` |
| Linux | x64 | `x64-linux-release-dbg` | `x64-linux-release-dbg-production`, `x64-linux-release-dbg-experimental` |
| macOS | x64 | `x64-osx` | `x64-osx-production`, `x64-osx-experimental` |
| macOS | ARM64 | `arm64-osx` | `arm64-osx-production`, `arm64-osx-experimental` |
| Windows | x64 | `x64-windows` | `x64-windows-production`, `x64-windows-experimental` |
| Windows | x64 | `x64-windows-static` | `x64-windows-static-production`, `x64-windows-static-experimental` |
| Windows | x64 | `x64-windows-static-md` | `x64-windows-static-md-production`, `x64-windows-static-md-experimental` |

### 3.2 Binary Cache Strategy

**Cache Key Composition:**
```
vcpkg-${VCPKG_COMMIT}-${OS}-${ARCH}-${VCPKG_JSON_HASH}-${TRIPLET}
```

**Shared Dependencies:**
- `fmt`, `boost-*`, `libarchive`, etc. are shared across ALL presets
- Only difference is `DWARFS_WITH_THRIFT` variable

**Cache Layout:**
```
.github/cache/
├── vcpkg-x64-linux-production/
├── vcpkg-x64-linux-experimental/
├── vcpkg-x64-osx-production/
├── vcpkg-arm64-osx-production/
└── ... (one per triplet × config)
```

---

## Part 4: Implementation Checklist

### Phase 1: CMakePresets Infrastructure
- [ ] Create `cmake/presets/` directory structure
- [ ] Create `cmake/presets/base.json` with common settings
- [ ] Create `cmake/presets/triplets/linux-x64.json`
- [ ] Create `cmake/presets/triplets/linux-x64-dynamic.json`
- [ ] Create `cmake/presets/triplets/linux-x64-release-dbg.json`
- [ ] Create `cmake/presets/triplets/osx-x64.json`
- [ ] Create `cmake/presets/triplets/osx-arm64.json`
- [ ] Create `cmake/presets/triplets/windows-x64.json`
- [ ] Create `cmake/presets/triplets/windows-x64-static.json`
- [ ] Create `cmake/presets/triplets/windows-x64-static-md.json`
- [ ] Create `cmake/presets/configs/production.json`
- [ ] Create `cmake/presets/configs/experimental.json`
- [ ] Create root `CMakePresets.json` that includes all
- [ ] Test locally: `cmake --list-presets`

### Phase 2: vcpkg Dependency Split
- [ ] Create `vcpkg.json` with base dependencies
- [ ] Create `vcpkg-experimental.json` with Modern Thrift deps
- [ ] Verify `vcpkg.json` works with production build
- [ ] Verify `vcpkg-experimental.json` works with experimental build
- [ ] Document dependency split in DEVELOPER_WORKFLOW.md

### Phase 3: CI Workflow Updates
- [ ] Update `.github/workflows/_build-test-reusable.yml`
  - [ ] Add `lukka/get-cmake@latest` step
  - [ ] Replace custom configure-cmake action with run-cmake
  - [ ] Remove custom build step (handled by run-cmake)
  - [ ] Remove custom test step (handled by run-cmake)
  - [ ] Update artifact paths if needed
- [ ] Update `.github/workflows/ci-main.yml`
- [ ] Update `.github/workflows/ci-matrix.yml` with all triplets
- [ ] Update `.github/workflows/pr-validation.yml`

### Phase 4: Warning Fixes
- [ ] Fix unused variables in `frozen2_deserializer.cpp` (30+ warnings)
  - [ ] Use `[[maybe_unused]]` attribute or remove
- [ ] Fix unused parameters in `value_encoders.cpp`
  - [ ] Use `[[maybe_unused]]` attribute
- [ ] Fix float comparison warnings in `folly_compat.h`
  - [ ] Use `std::memcmp` or epsilon comparison
- [ ] Fix unused function in `features.cpp`
  - [ ] Remove or mark `[[maybe_unused]]`
- [ ] Fix CMake deprecation warning at `CMakeLists.txt:43`
  - [ ] Remove deprecated policy or update
- [ ] Fix CMake syntax warning in `packaging.cmake:148`
  - [ ] Break long line
- [ ] Suppress or fix "ronn not found" warnings
  - [ ] Add ronn to CI or make truly optional

### Phase 5: Testing & Validation
- [ ] Test build locally with production preset
- [ ] Test build locally with experimental preset
- [ ] Verify CI runs with all triplets
- [ ] Verify binary cache is populated
- [ ] Verify second run is faster (80-90% improvement)
- [ ] Verify test results upload correctly
- [ ] Verify no regressions in build output

### Phase 6: Documentation Updates
- [ ] Update `DEVELOPER_WORKFLOW.md` with CMakePresets instructions
- [ ] Update `CLAUDE.md` with new build commands
- [ ] Update `TODO.pre-release-checks.md` with CI status
- [ ] Document CMakePresets in README.adoc if needed

### Phase 7: Cleanup
- [ ] Remove deprecated composite actions:
  - [ ] `.github/actions/configure-cmake/`
  - [ ] (Keep `run-ctest` and `setup-build-deps` if still useful)
- [ ] Remove old build scripts if replaced
- [ ] Update any references in documentation

---

## Part 5: Expected Performance Improvements

### First Run (Cold Cache)
| Configuration | Time |
|--------------|------|
| Production | ~30 min |
| Experimental | ~70 min |

### Subsequent Runs (Warm Cache)
| Configuration | Time | Improvement |
|--------------|------|-------------|
| Production | ~5-10 min | 80-85% faster |
| Experimental | ~15-20 min | 75-80% faster |

### Key Factors
1. **Shared base dependencies** - Only built once per triplet
2. **CMake/Ninja cached** - No download overhead
3. **CMake cache** - Faster reconfigure
4. **Parallel jobs** - All triplets build independently

---

## Part 6: Monitoring Strategy

### CI Metrics to Track
- [ ] First run time per triplet
- [ ] Cached run time per triplet
- [ ] Binary cache hit rate
- [ ] Test pass rate
- [ ] Warning count

### Commands to Monitor
```bash
# Check CI run times
gh run list --workflow=ci-matrix.yml --json databaseId,displayTitle,conclusion,updatedAt --jq '.[]'

# Check specific job logs
gh job view <job-id> --log

# Check cache status
gh api repos/tamatebako/dwarfs/actions/caches
```

---

## Part 7: Rollback Plan

If the new architecture fails:
1. Revert `_build-test-reusable.yml` to use composite actions
2. Keep CMakePresets.json for local development
3. Document issues encountered
4. Iterate and try again

---

## Appendix: Reference Commands

### Local Testing
```bash
# List presets
cmake --list-presets

# Configure with preset
cmake --preset x64-linux-production

# Build with preset
cmake --build --preset x64-linux-production

# Test with preset
ctest --preset x64-linux-production
```

### vcpkg Commands
```bash
# Install base dependencies
vcpkg install --triplet=x64-linux

# Install experimental dependencies
vcpkg install --triplet=x64-linux --x-feature=experimental

# List installed packages
vcpkg list --triplet=x64-linux
```

### CI Debugging
```bash
# Trigger workflow manually
gh workflow run ci-matrix.yml

# Watch workflow run
gh run watch

# View job logs
gh job view <job-id> --log
```
