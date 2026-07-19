# Tebako Patches Archive

**Created:** 2025-10-28
**Purpose:** Comprehensive backup of all tebako-specific changes before upstream merge/rebase

---

## Archive Contents

This directory contains a complete archive of 139 commits of tebako-specific modifications to the dwarfs filesystem, diverged from upstream since November 20, 2023.

### Summary Statistics

- **Total commits preserved:** 139
- **Upstream commits missed:** 2,290
- **Time divergence:** ~24 months
- **Total changes:** 47 files changed, 1,373 insertions(+), 234 deletions(-)

---

## Directory Structure

```
tebako-patches/
├── README.md                          # This file
├── RESTORATION_GUIDE.md               # Detailed guide for applying patches
│
├── 01-build-system/                   # Build system changes (32K total)
│   ├── cmake-changes.patch            # 28K - Main CMakeLists.txt modifications
│   ├── cmake-modules-gitmodules.patch # 733B - Submodule and cmake module changes
│   └── documentation-config.patch     # 3.4K - README and .gitignore updates
│
├── 02-platform-support/               # Platform-specific code (18K total)
│   ├── source-code-changes.patch      # 17K - All C++ source file changes
│   ├── windows-msys-source.patch      # 1.1K - Windows/MSys specific changes
│   └── macos-compatibility.patch      # 0B - (no standalone macOS changes)
│
├── 03-ci-cd/                          # CI/CD infrastructure (31K total)
│   ├── github-workflows.patch         # 31K - GitHub Actions workflows
│   └── cirrus-changes.patch           # 0B - (Cirrus CI was retired)
│
├── 04-ruby-integration/               # Ruby-specific changes
│   └── (empty - Ruby changes are in source-code-changes.patch)
│
├── 05-dependencies/                   # Dependency updates (5.7K total)
│   ├── folly-fbthrift.patch           # 442B - Submodule pointer updates
│   └── fsst-changes.patch             # 5.3K - FSST library modifications
│
└── metadata/                          # Reference and metadata (115K total)
    ├── commit-list.txt                # 6.0K - Simple list of commits
    ├── commit-details.txt             # 15K - Detailed commit info
    ├── file-changes.txt               # 2.5K - File change statistics
    └── complete-changes.patch         # 92K - Complete unified diff
```

---

## Quick Start

### To Review Changes

```bash
# View summary of all changes
cat tebako-patches/metadata/file-changes.txt

# View commit history
cat tebako-patches/metadata/commit-list.txt

# See complete diff
less tebako-patches/metadata/complete-changes.patch
```

### To Apply Patches

**⚠️ IMPORTANT:** Read [`RESTORATION_GUIDE.md`](RESTORATION_GUIDE.md) first!

```bash
# Example: Apply build system changes
cd /path/to/dwarfs
git apply tebako-patches/01-build-system/cmake-changes.patch

# Check if patch applies cleanly
git apply --check tebako-patches/01-build-system/cmake-changes.patch

# Apply with three-way merge for conflicts
git apply -3 tebako-patches/02-platform-support/source-code-changes.patch
```

---

## Patch Categories Explained

### 01-build-system (HIGH PRIORITY)

**Purpose:** Core build system modifications
**Risk Level:** ⚠️ VERY HIGH - Heavy conflicts expected
**Contains:**
- CMake 4.x compatibility fixes
- Windows/MSys build support
- Boost policy CMP0167 handling
- Custom compiler flags
- Dependency management overrides
- tebako-tools integration

**Critical for:** All platforms to build successfully

### 02-platform-support (MEDIUM-HIGH PRIORITY)

**Purpose:** Platform-specific source code changes
**Risk Level:** ⚠️ MEDIUM
**Contains:**
- Windows/MSys compatibility code
- macOS M1/arm64 support
- Platform-specific file handling
- Ruby integration support
- Console output fixes

**Critical for:** Windows, macOS, Ruby integration

### 03-ci-cd (LOW PRIORITY)

**Purpose:** Continuous integration infrastructure
**Risk Level:** ✅ LOW - Separate files, no conflicts
**Contains:**
- GitHub Actions workflows for Alpine, Ubuntu, macOS, Windows
- Reusable build-and-test action
- Platform-specific CI configurations

**Critical for:** Automated testing and validation

### 04-ruby-integration

**Note:** This directory is empty because Ruby-specific changes are integrated into the source code patches (02-platform-support/source-code-changes.patch).

**Ruby features to verify:**
- MSys compatibility with Ruby's time functions
- Ruby build system integration
- Platform-specific Ruby workarounds

### 05-dependencies (MEDIUM PRIORITY)

**Purpose:** External dependency updates
**Risk Level:** ⚠️ MEDIUM
**Contains:**
- folly 07.2024 version
- fbthrift 07.2024 version
- FSST compression library changes

**Critical for:** Build compatibility, performance

---

## Key Files by Impact

### Highest Impact (Must Preserve)

1. **CMakeLists.txt** (28K patch)
   - Most critical file
   - Contains all Windows/MSys build logic
   - Extensive conflicts expected with upstream

2. **GitHub Actions workflows** (31K patch)
   - Complete CI/CD infrastructure
   - Separate files, low conflict risk
   - Enables multi-platform testing

3. **Source code changes** (17K patch)
   - Platform compatibility fixes
   - Windows/MSys support code
   - Ruby integration code

### Medium Impact

4. **FSST changes** (5.3K patch)
   - Compression library optimizations
   - May conflict with upstream updates

5. **Documentation** (3.4K patch)
   - README updates for tebako fork
   - .gitignore additions

### Lower Impact

6. **Submodule pointers** (442B, 733B patches)
   - Version updates for folly/fbthrift
   - Easy to reapply manually

---

## Empty Patches (Expected)

### macos-compatibility.patch (0 bytes)
**Reason:** macOS changes are integrated into source-code-changes.patch
**Action:** Use source-code-changes.patch instead

### cirrus-changes.patch (0 bytes)
**Reason:** Cirrus CI was retired in favor of GitHub Actions
**Action:** No action needed, deprecated infrastructure

---

## Common Use Cases

### 1. Full Restoration After Upstream Merge

```bash
# Follow RESTORATION_GUIDE.md for complete step-by-step process
less tebako-patches/RESTORATION_GUIDE.md
```

### 2. Cherry-Pick Specific Features

```bash
# Example: Only apply Windows/MSys support
git apply tebako-patches/02-platform-support/windows-msys-source.patch

# Add MSys build options to CMakeLists.txt manually
# (Reference: 01-build-system/cmake-changes.patch)
```

### 3. Compare with Upstream

```bash
# See what changed in a specific file
git diff origin/main...tebako/main -- CMakeLists.txt

# View in patch format
git show tebako/main:CMakeLists.txt > /tmp/tebako-cmake.txt
git show origin/main:CMakeLists.txt > /tmp/upstream-cmake.txt
diff -u /tmp/upstream-cmake.txt /tmp/tebako-cmake.txt
```

### 4. Extract Specific Changes

```bash
# Get only Windows-related changes from source code
grep -A 5 -B 5 "WIN32\|MSYS\|MinGW" \
  tebako-patches/02-platform-support/source-code-changes.patch
```

---

## Verification

### Check Patch Integrity

```bash
# Verify all patches exist and have content
find tebako-patches -name "*.patch" -exec sh -c 'echo "$1: $(wc -l < "$1") lines"' _ {} \;

# Expected output (non-zero for active patches):
# cmake-changes.patch: 800+ lines
# source-code-changes.patch: 500+ lines
# github-workflows.patch: 900+ lines
# etc.
```

### Validate Patch Application

```bash
# Test if patches apply cleanly (without actually applying)
for patch in tebako-patches/*/*.patch; do
  echo "Checking: $patch"
  git apply --check "$patch" 2>&1 | head -5
done
```

---

## Important Notes

### About Conflicts

**Expected High-Conflict Files:**
- `CMakeLists.txt` - ⚠️ Manual intervention required
- Source files that upstream heavily modified
- Files where both tebako and upstream made changes

**Resolution Strategy:**
1. Use upstream as base
2. Manually re-apply tebako-specific sections
3. Test build after each section
4. Prefer tebako for Windows/MSys/Ruby code
5. Prefer upstream for general improvements

### About Submodules

The patches include pointer updates for:
- `folly` → version as of 07.2024
- `fbthrift` → version as of 07.2024

**Note:** Upstream may have updated to newer versions. Compare carefully:
```bash
# See current submodule commits
git ls-tree tebako/main folly fbthrift
git ls-tree origin/main folly fbthrift
```

### About Testing

**After applying ANY patches:**
1. Try to build immediately
2. Fix build errors before proceeding
3. Run tests if build succeeds
4. Verify platform-specific functionality

---

## Metadata Files Reference

### commit-list.txt
Simple one-line-per-commit format: `<hash> <message>`
Use for: Quick overview of changes

### commit-details.txt
Detailed format: `<hash>|<date>|<author>|<message>`
Use for: Analysis, finding specific commits

### file-changes.txt
Git diff --stat output
Use for: Understanding scope of changes per file

### complete-changes.patch
Full unified diff of ALL changes
Use for: Reference, searching for specific code

---

## Preservation Strategy

This archive serves multiple purposes:

1. **Backup:** Complete snapshot before risky merge/rebase
2. **Reference:** Understanding what tebako added
3. **Restoration:** Ability to reapply changes systematically
4. **Documentation:** Record of 2 years of tebako development
5. **Analysis:** Identifying conflicts and dependencies

---

## Next Steps

1. ✅ **Archive created** - This step is complete
2. 📋 **Read RESTORATION_GUIDE.md** - Before making any changes
3. 🔍 **Review upstream changes** - Check what changed in 2,290 commits
4. 🗂️ **Plan merge strategy** - Decide on rebase vs merge
5. 💾 **Create backup branch** - Before attempting restoration
6. 🔨 **Apply patches systematically** - Follow the guide
7. ✅ **Test thoroughly** - On all platforms
8. 📝 **Document** - Any issues or modifications made

---

## Support

For detailed restoration instructions, see [`RESTORATION_GUIDE.md`](RESTORATION_GUIDE.md)

For questions about specific patches:
- Review commit messages in `metadata/commit-details.txt`
- Examine the specific patch file
- Compare with upstream using `git diff`

**Repository References:**
- Tebako fork: `https://github.com/tamatebako/dwarfs`
- Upstream: `https://github.com/mhx/dwarfs`

**Key Maintainer:** maxirmx (Maxim Samsonov)

---

**Archive Status:** ✅ Complete and Verified
**Total Archive Size:** ~182 KB
**Files Archived:** 15 (10 patches + 4 metadata + 1 guide)