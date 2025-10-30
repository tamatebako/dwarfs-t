# Tebako Patches Restoration Guide

**Generated:** 2025-10-28
**Purpose:** Guide for reapplying tebako-specific patches to updated upstream dwarfs

---

## Overview

This archive contains 139 commits worth of tebako-specific modifications to the dwarfs filesystem. The patches are organized by category for systematic reapplication after merging/rebasing with upstream.

**Common Ancestor:** `6214ccbe8c0c615224087bf97c9b700c590acab7` (2023-11-20)
**Tebako commits ahead:** 139
**Upstream commits ahead:** 2,290
**Time divergence:** ~24 months

---

## Directory Structure

```
tebako-patches/
├── 01-build-system/          # CMake, build configuration, dependencies
├── 02-platform-support/      # Windows/MSys, macOS, source code changes
├── 03-ci-cd/                 # GitHub Actions, CI workflows
├── 04-ruby-integration/      # Ruby-specific compatibility (currently empty)
├── 05-dependencies/          # Submodule updates, fsst changes
└── metadata/                 # Complete diffs, commit lists, statistics
```

---

## Patch Application Order

### CRITICAL: Apply patches in this exact order to minimize conflicts

### Phase 1: Build System Foundation (HIGH PRIORITY)
**Order:** 1st
**Risk Level:** ⚠️ VERY HIGH - Heavy conflicts expected

1. **cmake-changes.patch**
   - Main CMakeLists.txt modifications
   - Contains ~537 lines of changes
   - Key features:
     * Windows/MSys build support
     * CMake 4.x compatibility (CMP0167)
     * Custom compiler flags (FOLLY_ASSUME_NO_TCMALLOC, GLOG_USE_GLOG_EXPORT)
     * Dependency path prioritization
     * tebako-tools integration
   - **Application strategy:**
     ```bash
     # Do NOT apply directly - use as reference
     # Manually re-implement changes after reviewing upstream CMakeLists.txt
     git apply --check tebako-patches/01-build-system/cmake-changes.patch
     ```
   - **Expected conflicts:** MANY - CMakeLists.txt is heavily modified in upstream
   - **Resolution approach:**
     1. Start with upstream CMakeLists.txt
     2. Add tebako-specific sections one by one
     3. Test build after each addition
     4. Key sections to preserve:
        - MSys/MinGW detection
        - PREFER_SYSTEM_LIBFMT option
        - USE_JEMALLOC propagation
        - Boost policy CMP0167 handling

2. **cmake-modules-gitmodules.patch**
   - `.gitmodules` changes (tools submodule)
   - `cmake/` module updates
   - **Application strategy:**
     ```bash
     git apply tebako-patches/01-build-system/cmake-modules-gitmodules.patch
     ```
   - **Expected conflicts:** LOW
   - **Dependencies:** None

3. **documentation-config.patch**
   - `README.md` and `.gitignore` updates
   - **Application strategy:**
     ```bash
     git apply tebako-patches/01-build-system/documentation-config.patch
     ```
   - **Expected conflicts:** LOW-MEDIUM
   - **Resolution:** Prefer tebako README, merge .gitignore entries

### Phase 2: Dependencies & Submodules
**Order:** 2nd
**Risk Level:** ⚠️ MEDIUM-HIGH

4. **folly-fbthrift.patch**
   - Submodule pointer updates
   - **Versions:** folly/fbthrift 07.2024
   - **Application strategy:**
     ```bash
     # Check submodule status first
     git submodule status

     # Apply if safe, otherwise update manually
     git apply tebako-patches/05-dependencies/folly-fbthrift.patch

     # Or manually:
     cd folly && git checkout <tebako-commit-hash>
     cd ../fbthrift && git checkout <tebako-commit-hash>
     ```
   - **Expected conflicts:** MEDIUM
   - **Note:** Upstream may have updated to newer versions
   - **Testing required:** Build test immediately after applying

5. **fsst-changes.patch**
   - FSST library modifications
   - **Application strategy:**
     ```bash
     git apply tebako-patches/05-dependencies/fsst-changes.patch
     ```
   - **Expected conflicts:** LOW
   - **Contains:** Potential AVX512 or compression optimizations

### Phase 3: Platform-Specific Code
**Order:** 3rd
**Risk Level:** ⚠️ MEDIUM

6. **source-code-changes.patch**
   - All source file modifications (`.cpp`, `.h`)
   - **Includes:**
     * Windows/MSys compatibility
     * Ruby integration support
     * Platform-specific fixes
   - **Application strategy:**
     ```bash
     # Review first - this is comprehensive
     git apply --check tebako-patches/02-platform-support/source-code-changes.patch

     # Apply with three-way merge if conflicts
     git apply -3 tebako-patches/02-platform-support/source-code-changes.patch
     ```
   - **Expected conflicts:** MEDIUM-HIGH
   - **Key files to watch:**
     * `src/dwarfs/file_stat.cpp` - Windows compatibility
     * `src/dwarfs/checksum.cpp` - Algorithm restrictions
     * `src/dwarfs/console_writer.cpp` - Output handling
     * `src/internal/worker_group.cpp` - Threading changes
   - **Resolution:** Prefer tebako for Windows-specific code

7. **windows-msys-source.patch** (if exists)
   - Specific Windows/MSys source changes
   - Subset of source-code-changes.patch
   - **Application:** Only if not already in patch #6

8. **macos-compatibility.patch** (if exists)
   - macOS-specific changes
   - **Application:** Only if not already in patch #6

### Phase 4: CI/CD Infrastructure
**Order:** 4th (can be done in parallel with Phase 3)
**Risk Level:** ✅ LOW

9. **github-workflows.patch**
   - All `.github/workflows/*.yml` files
   - New workflows:
     * `alpine.yml` - Alpine Linux CI
     * `ubuntu.yml` - Ubuntu CI
     * `macos.yml` - macOS CI
     * `windows-msys.yml` - Windows MSys CI
     * `windows.yml` - Windows MSVC CI
   - Reusable action: `.github/actions/build-and-test/action.yml`
   - **Application strategy:**
     ```bash
     git apply tebako-patches/03-ci-cd/github-workflows.patch
     ```
   - **Expected conflicts:** NONE (new files)
   - **Post-apply:** Review if upstream has similar/better workflows

10. **cirrus-changes.patch** (if exists)
    - Cirrus CI configuration (deprecated)
    - **Note:** Tebako retired Cirrus CI in favor of GitHub Actions
    - **Application:** SKIP unless needed for historical reference

### Phase 5: Ruby Integration (if applicable)
**Order:** 5th
**Risk Level:** ⚠️ MEDIUM

**Note:** No separate Ruby patches found - likely integrated in source-code-changes.patch

**Ruby-specific features to verify:**
- MSys compatibility with Ruby's gmtime_r/localtime_r
- Ruby build integration
- Platform-specific Ruby workarounds

**Verification:**
```bash
# After applying all patches, verify Ruby integration
# (requires tebako environment)
cd /path/to/tebako
# Run tebako build tests
```

---

## Conflict Resolution Strategy

### For CMakeLists.txt (MOST CRITICAL)

1. **Start with upstream version**
   ```bash
   git checkout origin/main -- CMakeLists.txt
   ```

2. **Manually add tebako sections**
   - Reference: `tebako-patches/01-build-system/cmake-changes.patch`
   - Add sections in this order:
     1. MSys/MinGW platform detection
     2. Compiler flag definitions
     3. Dependency overrides (fmt, gtest, folly)
     4. tebako-tools integration
     5. Build options (PREFER_SYSTEM_LIBFMT, USE_JEMALLOC)
     6. Boost policy CMP0167

3. **Test build after each section**
   ```bash
   mkdir build && cd build
   cmake ..
   # Fix issues before proceeding
   ```

### For Source Files

1. **Use three-way merge**
   ```bash
   git apply -3 <patch-file>
   ```

2. **For conflicts:**
   - Prefer upstream: General improvements, bug fixes
   - Prefer tebako: Windows/MSys code, Ruby integration
   - Merge both: If changes are orthogonal

3. **Review carefully:**
   - `file_stat.cpp` - Windows stat handling
   - `checksum.cpp` - Algorithm availability
   - `console_writer.cpp` - Output formatting
   - `worker_group.cpp` - Threading behavior

### For Submodules

1. **Check both versions**
   ```bash
   # Tebako version
   grep folly tebako-patches/metadata/commit-details.txt

   # Upstream version
   git ls-tree origin/main folly fbthrift
   ```

2. **Choose version:**
   - If upstream is newer → test with upstream first
   - If tebako version has critical patches → use tebako version
   - Ideal: Use upstream + apply tebako patches if any

---

## Testing Protocol

### After Each Phase

1. **Build test**
   ```bash
   mkdir -p build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(nproc)
   ```

2. **Platform-specific tests**
   - Linux: Ubuntu 20.04, Alpine 3.17+
   - macOS: arm64 (M1), intel (if available)
   - Windows: MSVC, MSys/MinGW

### Full Integration Test

1. **Run test suite**
   ```bash
   cd build
   ctest --output-on-failure
   ```

2. **Tebako integration**
   ```bash
   # In tebako repository
   # Update dwarfs dependency to patched version
   # Run tebako build tests
   ```

3. **Regression tests**
   - Compare performance with pre-merge
   - Verify all tebako-specific features work
   - Check Ruby integration if applicable

---

## Rollback Procedures

### If Patches Fail

1. **Return to backup**
   ```bash
   git checkout tebako-main-backup
   ```

2. **Create fresh attempt branch**
   ```bash
   git checkout -b tebako-merge-attempt-2 origin/main
   ```

3. **Apply patches more granularly**
   - Split large patches into smaller chunks
   - Apply file-by-file if necessary

### If Build Fails

1. **Identify failing component**
   ```bash
   make VERBOSE=1
   ```

2. **Compare with working version**
   ```bash
   git diff tebako/main -- <failing-file>
   ```

3. **Revert specific changes**
   ```bash
   git checkout tebako/main -- <file>
   ```

---

## Critical Tebako Features to Preserve

### Windows/MSys Support
- [ ] MSys/MinGW detection in CMakeLists.txt
- [ ] Windows-specific file_stat implementation
- [ ] PREFER_SYSTEM_LIBFMT option
- [ ] Brotli library handling for MSys
- [ ] Path handling for Windows

### macOS Support
- [ ] macOS 14 (arm64 M1) support
- [ ] Xcode 15.4+ compatibility
- [ ] OpenSSL 1.1 handling
- [ ] Proper library linking

### Build System
- [ ] CMake 4.x compatibility
- [ ] Boost policy CMP0167 handling
- [ ] FOLLY_ASSUME_NO_TCMALLOC definition
- [ ] GLOG_USE_GLOG_EXPORT definition
- [ ] USE_JEMALLOC propagation

### Dependencies
- [ ] tebako-tools integration
- [ ] folly 07.2024 compatibility
- [ ] fbthrift 07.2024 compatibility
- [ ] fmt library override capability
- [ ] gtest override capability

### Ruby Integration
- [ ] MSys Ruby compatibility (gmtime_r, localtime_r)
- [ ] Ruby build integration

### CI/CD
- [ ] GitHub Actions workflows for all platforms
- [ ] Reusable build-and-test action
- [ ] Alpine, Ubuntu, macOS, Windows CI

---

## Dependencies Between Patches

```
cmake-changes.patch
    ├── Required by: ALL other patches
    └── Must be applied: FIRST

cmake-modules-gitmodules.patch
    ├── Depends on: cmake-changes.patch
    └── Required by: build system

folly-fbthrift.patch
    ├── Depends on: cmake-changes.patch
    └── Required by: source code builds

fsst-changes.patch
    ├── Independent
    └── Low priority

source-code-changes.patch
    ├── Depends on: cmake-changes.patch, folly-fbthrift.patch
    └── Required by: full functionality

github-workflows.patch
    ├── Independent (separate files)
    └── Can be applied anytime
```

---

## Known Issues & Workarounds

### CMake 4.x Compatibility
**Issue:** Boost policy CMP0167 warnings
**Workaround:** Set policy in CMakeLists.txt:
```cmake
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
```

### Folly/FBThrift Version Mismatch
**Issue:** API breakage between versions
**Workaround:** Lock to tebako versions (07.2024) initially, update later

### Windows Build Failures
**Issue:** Missing MSys-specific definitions
**Workaround:** Ensure all PREFER_SYSTEM_* options are set correctly

### macOS M1 Build Issues
**Issue:** Architecture-specific compilation
**Workaround:** Verify Xcode version, check arm64 flags

---

## Post-Application Checklist

- [ ] All patches applied successfully
- [ ] CMakeLists.txt builds on all platforms
- [ ] No critical warnings during build
- [ ] Test suite passes
- [ ] Tebako integration verified
- [ ] Performance comparable to previous version
- [ ] Documentation updated
- [ ] CI workflows running
- [ ] Tag created for merged version

---

## Reference Files

### Metadata Files
- `metadata/commit-list.txt` - Simple commit list
- `metadata/commit-details.txt` - Detailed commit info (hash|date|author|message)
- `metadata/file-changes.txt` - Statistics of changed files
- `metadata/complete-changes.patch` - Full unified diff

### Useful Commands

**Check what changed in a file:**
```bash
git diff origin/main...tebako/main -- <file>
```

**List all commits affecting a file:**
```bash
git log --oneline origin/main..tebako/main -- <file>
```

**Show commit details:**
```bash
git show <commit-hash>
```

**Apply patch with conflict markers:**
```bash
git apply -3 <patch-file>
```

**Check if patch applies cleanly:**
```bash
git apply --check <patch-file>
```

---

## Support & Resources

### Tebako Repository
- GitHub: `https://github.com/tamatebako/dwarfs`
- Branches: main, tebako-patch, update-upstream

### Upstream Repository
- GitHub: `https://github.com/mhx/dwarfs`
- Reference for latest changes

### Documentation
- See `TEBAKO_PATCHES_ANALYSIS.md` for detailed analysis
- Original commit messages in `metadata/commit-details.txt`

### Key Contributors
- maxirmx (Maxim Samsonov) - Primary tebako maintainer
- Marcus Holland-Moritz - Upstream maintainer

---

## Success Criteria

A successful restoration means:

1. ✅ All tebako-specific functionality preserved
2. ✅ Builds successfully on: Linux (Ubuntu, Alpine), macOS (arm64, intel), Windows (MSVC, MSys)
3. ✅ All tests pass
4. ✅ Tebako integration works without regressions
5. ✅ Performance is comparable or better
6. ✅ No critical build warnings
7. ✅ CI/CD pipelines operational
8. ✅ Documentation updated

---

**End of Restoration Guide**

For questions or issues during restoration, refer to the original analysis and commit history.