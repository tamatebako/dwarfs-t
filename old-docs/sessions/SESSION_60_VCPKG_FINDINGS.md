# Session 60: vcpkg Build Findings & Analysis

**Date**: 2025-12-31
**Duration**: 4+ hours (22:00 HKT Session 59 end → 02:30 HKT Session 60)
**Build Attempts**: 15
**Status**: 🔴 **BLOCKED** - Fundamental version incompatibility

---

## Executive Summary

**Problem**: Building dwarfs with Thrift support using vcpkg overlay ports
**Root Cause**: Cascading version incompatibilities in Folly/fbthrift/wangle dependency chain
**Outcome**: Successfully built wangle, but fbthrift incompatible with both old and new Folly

---

## Build Attempt Timeline

| Attempt | Issue | Fix Applied | Result |
|---------|-------|-------------|--------|
| v1-v4 | wangle CMake config path | Removed broken patches | ❌ Path still wrong |
| v5 | wangle WANGLE_CMAKE_DIR wrong | fix-cmake-config.patch | ✅ wangle builds! |
| v6-v10 | fbthrift ProtocolBench target | no-benchmarks.patch | ✅ Patch applies |
| v11-v13 | FOLLY_NODISCARD missing | Attempted patches | ❌ Patches don't apply |
| v14 | FOLLY_NODISCARD missing | Compiler flag -DCMAKE_CXX_FLAGS | ✅ Flag works |
| v15 | FOLLY_DETAIL_LITE_TUPLE_ADJUST_WARNINGS | Upgraded Folly to v2025.12.29.00 | ❌ Still incompatible |

---

## Successful Fixes

### ✅ wangle (Package 191/204)
**Issues Resolved**:
1. TFO typo in TCP_FASTOPEN constant
2. WANGLE_CMAKE_DIR hardcoded to wrong path

**Patches**:
- [`vcpkg_ports/wangle/fix-tfo-typo.patch`](../vcpkg_ports/wangle/fix-tfo-typo.patch)
- [`vcpkg_ports/wangle/fix-cmake-config.patch`](../vcpkg_ports/wangle/fix-cmake-config.patch)

**Portfile**: [`vcpkg_ports/wangle/portfile.cmake`](../vcpkg_ports/wangle/portfile.cmake)
**Result**: Builds successfully with both old and new Folly

### ✅ Folly (Package 187/204)
**Upgrade**: mhx/folly (outdated fork) → facebook/folly v2025.12.29.00 (latest official)
**SHA512**: `845e47618ad57a0fab5e29dd1a4f61980659e7e0f42497c4b54c5ac8e19707d288c2846409866843813367daf009ec4ec0d6c22dd005514a07f0df07a1990212`
**Build Time**: ~60 seconds
**Portfile**: [`vcpkg_ports/folly/portfile.cmake`](../vcpkg_ports/folly/portfile.cmake)
**Result**: Builds successfully

### ✅ fizz (Package 190/204)
**Status**: Automatically rebuilt with new Folly
**Build Time**: ~18 seconds
**Result**: Builds successfully (compatible with latest Folly)

---

## Remaining Issues

### 🔴 fbthrift (Package 192/204) - BLOCKED

**Repository**: mhx/fbthrift (fork of facebook/fbthrift)
**Branch**: main (rolling, no version tag)
**Compilation Progress**: 211/391 files before failure

**Errors Encountered**:

1. **ProtocolBench target** (v6-v10):
   - Generator expression bug in link libraries
   - **Fix**: Comment out in test/CMakeLists.txt ✅
   - **Patch**: `no-benchmarks.patch` ✅

2. **FOLLY_NODISCARD macro missing** (v11-v14):
   - Old Folly (2024.01.15.00) missing macro
   - **Fix**: Compiler flag `-DCMAKE_CXX_FLAGS="-DFOLLY_NODISCARD=[[nodiscard]]"` ✅
   - **Result**: Works with compiler flag

3. **FOLLY_DETAIL_LITE_TUPLE_ADJUST_WARNINGS missing** (v15):
   - Latest Folly (v2025.12.29.00) has this macro
   - fbthrift main branch still fails
   - **Location**: `folly/lang/MustUseImmediately.h:28`
   - **Root Cause**: fbthrift main branch incompatible with latest Folly

**Diagnosis**: The mhx/fbthrift fork at main branch is in a transitional state - incompatible with both:
- Old Folly (2024.01.15.00): Missing FOLLY_NODISCARD
- New Folly (2025.12.29.00): Missing FOLLY_DETAIL_LITE_TUPLE_ADJUST_WARNINGS (though Folly has it)

---

## Technical Analysis

### Version Compatibility Matrix

| Component | Old Stack (Session 59) | New Stack (v15) | Compatibility |
|-----------|------------------------|-----------------|---------------|
| **Folly** | mhx/folly main (2024.01.15.00) | facebook/folly v2025.12.29.00 | ✅ Both build |
| **fizz** | v2025.05.19.00 | v2025.05.19.00 (rebuilt) | ✅ Works with both |
| **wangle** | v2025.05.19.00 | v2025.05.19.00 (rebuilt) | ✅ Works with both |
| **fbthrift** | mhx/fbthrift main | mhx/fbthrift main | ❌ Works with neither |

### Missing Macros Investigation

**FOLLY_NODISCARD** (old Folly):
- Defined in modern Folly as `[[nodiscard]]`
- **Workaround**: Compiler flag ✅
- **Status**: RESOLVED

**FOLLY_DETAIL_LITE_TUPLE_ADJUST_WARNINGS** (new Folly):
- Should be defined in `folly/lang/MustUseImmediately.h`  
- Present in latest Folly source
- **Actual Issue**: fbthrift's include structure can't find it
- **Status**: UNRESOLVED

---

## Root Cause Analysis

The fundamental problem is **using mhx/fbthrift fork at rolling main branch** instead of a stable tagged release.

**Evidence**:
1. mhx/fbthrift is a minimal fork for dwarfs' needs
2. Main branch is development/experimental
3. No version tags or stability guarantees
4. Incompatible with both old and new Folly versions

**Comparison**: 
- ✅ fizz uses **tagged release** v2025.05.19.00
- ✅ wangle uses **tagged release** v2025.05.19.00  
- ✅ Folly uses **tagged release** v2025.12.29.00
- ❌ fbthrift uses **rolling main branch** (no tag)

---

## Recommended Solution Strategy

### Option A: Use Stable fbthrift Release (RECOMMENDED)

**Approach**: Find a stable, tagged fbthrift release compatible with latest Folly

**Steps**:
1. Check facebook/fbthrift releases: https://github.com/facebook/fbthrift/tags
2. Find latest stable tag (e.g., v2025.12.29.00 if it exists)
3. Update `vcpkg_ports/fbthrift/portfile.cmake` to use facebook/fbthrift with tag
4. Test build compatibility

**Pros**:
- Stable, versioned dependencies
- Predictable behavior
- Better for CI/CD
- Matches fizz/wangle pattern

**Cons**:
- Official fbthrift may have features mhx/fbthrift removed
- May need additional patches

**Estimated Time**: 1-2 hours

### Option B: Fix Macro Compatibility (COMPLEX)

**Approach**: Add all missing macros via patches/compiler flags

**Required Fixes**:
1. FOLLY_NODISCARD ✅ (done via compiler flag)
2. FOLLY_DETAIL_LITE_TUPLE_ADJUST_WARNINGS ❌ (pending)
3. Unknown additional macros (likely more)

**Pros**:
- Keeps mhx/fbthrift fork
- Minimal changes to fbthrift

**Cons**:
- Whack-a-mole with macros
- Fragile, breaks on updates
- Not sustainable

**Estimated Time**: 3-4 hours (uncertain)

### Option C: Match All Versions (NUCLEAR)

**Approach**: Find a single date where Folly/fizz/wangle/fbthrift all have compatible releases

**Example**: Use all packages from 2024.01.15.00 era
- Folly: v2024.01.15.00
- fizz: v2024.01.15.00 (if exists)
- wangle: v2024.01.15.00 (if exists)
- fbthrift: v2024.01.15.00 (if exists)

**Pros**:
- Guaranteed version compatibility
- Stable snapshot

**Cons**:
- Very old (1 year)
- May have bugs/missing features
- Defeats purpose of latest Folly

**Estimated Time**: 2-3 hours

### Option D: Use Homebrew for Testing (PRAGMATIC) ⭐

**Approach**: Keep vcpkg ports as CI work, use Homebrew for immediate converter testing

**Steps**:
1. Document all vcpkg findings
2. Build dwarfs with Homebrew Folly/Thrift: `brew install folly fbthrift`
3. Run converter tests
4. Fix `cpp_thrift_converter.cpp` bugs
5. Verify Homebrew v0.14.1 compatibility
6. Return to vcpkg ports in future session

**Pros**:
- Unblocks converter testing immediately
- Homebrew versions are curated/tested
- Separates concerns (testing vs build system)
- Can still improve vcpkg ports later

**Cons**:
- Violates RULE 4 (vcpkg-only for Folly/Thrift)
- Need to document exception clearly

**Estimated Time**: 30 minutes to working build

---

## Recommendation

**PRIMARY**: Option D (Homebrew for testing) + Option A (vcpkg stable releases) in parallel

**Rationale**:
1. Option D unblocks converter testing TODAY
2. Option A provides long-term vcpkg solution
3. Homebrew testing validates converter logic independent of build system
4. Can iterate on vcpkg ports without blocking progress

---

## Files Modified

### vcpkg Overlay Ports
1. [`vcpkg_ports/folly/portfile.cmake`](../vcpkg_ports/folly/portfile.cmake) - Upgraded to v2025.12.29.00
2. [`vcpkg_ports/wangle/portfile.cmake`](../vcpkg_ports/wangle/portfile.cmake) - Added 2 patches
3. [`vcpkg_ports/wangle/fix-tfo-typo.patch`](../vcpkg_ports/wangle/fix-tfo-typo.patch) - Corrects TCP_FASTOPEN typo
4. [`vcpkg_ports/wangle/fix-cmake-config.patch`](../vcpkg_ports/wangle/fix-cmake-config.patch) - Fixes WANGLE_CMAKE_DIR
5. [`vcpkg_ports/fbthrift/portfile.cmake`](../vcpkg_ports/fbthrift/portfile.cmake) - Added patches + compiler flags
6. [`vcpkg_ports/fbthrift/no-benchmarks.patch`](../vcpkg_ports/fbthrift/no-benchmarks.patch) - Comments out ProtocolBench

### Abandoned Files
- `vcpkg_ports/fbthrift/fix-folly-nodiscard.patch` - FAILED (patch format issues)

### Logs Created
- `/tmp/cmake-session60-v1.log` through `/tmp/cmake-session60-v15-LATEST-FOLLY.log`
- `/tmp/SESSION_60_STATUS.md`
- `/tmp/vcpkg-fixes-summary.md`

---

## Lessons Learned

### Build System Insights
1. **vcpkg overlay ports are powerful** - Can override any dependency
2. **Patch format matters** - git patches need exact line numbers, context
3. **Compiler flags work better** - For simple macro definitions
4. **Version pinning essential** - Rolling branches cause cascading issues

### Dependency Management
1. **Facebook repos version together** - Folly/fizz/wangle released on same dates
2. **mhx forks are minimal** - Only what dwarfs needs, may be outdated
3. **Homebrew curates compatibility** - Their versions are tested to work together
4. **vcpkg needs maintenance** - Overlay ports must track upstream

### Time Management
1. **vcpkg debugging is expensive** - Each build cycle 3-5 minutes
2. **Version research up front** - Would have saved hours
3. **Parallel paths valuable** - Test with Homebrew while fixing vcpkg

---

## Next Session Starter

```bash
# Check what Homebrew uses
brew info folly fbthrift | grep -E "stable|Revision"

# Build with Homebrew temporarily
cmake -B build-hb-test -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDWARFS_WITH_THRIFT=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DWITH_TESTS=ON

# Run converter tests
./build-hb-test/dwarfs_unit_tests --gtest_filter="*ConverterRoundTrip*"
```

---

**Created**: 2025-12-31 18:33 HKT
**Author**: Kilo Code (Architect Mode)
**Next Session**: Use Homebrew for converter testing, then architect stable vcpkg port strategy