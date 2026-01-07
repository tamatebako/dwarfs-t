# Session 58: vcpkg Build - Critical Blocker Analysis

**Date**: 2025-12-31
**Stat us**: 🔴 **BLOCKED** - Dependency chain incompatibility

---

## Root Cause: Version Mismatch in Dependency Chain

The **mhx Folly fork** (main branch) is **incompatible** with vcpkg's dependency chain:

```
vcpkg wangle v2025.05.19.00
    ↓ expects
Folly with getTFOSucceded() (typo)
    ✗ conflicts with
mhx Folly fork with getTFOSucceeded() (correct spelling)
```

### Build Failures Encountered

1. **glog 0.7.1**: `GLOG_EXPORT` macro undefined → Fixed with glog 0.6.0 overlay
2. **jemalloc**: `nallocx`, `sdallocx`, `xallocx` undefined → Fixed by disabling jemalloc
3. **wangle**: Method name mismatch `getTFOSucceded` vs `getTFOSucceeded` → **BLOCKER**

---

## Why This Fails

**vcpkg's Philosophy**: All packages in vcpkg are version-locked and tested together

**Our Approach**: Using mhx fork of Folly (arbitrary `main` branch) + vcpkg's release versions of fizz/wangle/fbthrift

**Result**: **Version skew** - the dependency chain is broken

---

## Attempted Solutions (All Failed)

1. ✅ Use mhx fork REF `main` - works for folly alone
2. ✅ Downgrade glog to 0.6.0 - fixes GLOG_EXPORT
3. ✅ Disable jemalloc in Folly - fixes jemalloc functions
4. ❌ Build wangle - **fails due to API mismatch with Folly**

---

## Options Forward

### Option 1: Build ALL from mhx Fork (RECOMMENDED for Testing)

**Approach**: Use mhx forks for folly, fizz, wangle, fbthrift (if they exist)

**Pros**:
- Guaranteed version compatibility
- All components from same source

**Cons**:
- Must maintain 4 overlay ports
- May hit more compatibility issues
- mhx may not have all forks

**Time**: 2-4 hours to create overlay ports + test

### Option 2: Use Submodules (Session 57 Baseline)

**Approach**: Revert Session 57 changes, use git submodules

**Pros**:
- Known working state
- Already tested
- Session 56 fix still applies

**Cons**:
- Defeats vcpkg-only goal from Session 57
- Larger repository

**Time**: 30 minutes (revert + test)

### Option 3: Match vcpkg Versions Exactly

**Approach**: Find Folly version that matches vcpkg's wangle v2025.05.19.00

**Pros**:
- Full vcpkg compatibility
- Clean solution

**Cons**:
- May not be mhx fork (metadata format risk)
- Finding matching version complex

**Time**: 2-3 hours (research + test)

### Option 4: Patch wangle

**Approach**: Create wangle overlay port with typo fixed

**Pros**:
- Minimal change
- Keeps vcpkg ecosystem

**Cons**:
- May hit more mismatches
- Maintaining patches fragile

**Time**: 1-2 hours

---

## Recommendation

**For immediate testing of Session 56 converter fix**: Use **Option 2** (submodules)

**Rationale**:
1. Session 56 fix is **independent** of build system
2. Converter testing doesn't require vcpkg
3. Homebrew compatibility testing doesn't require vcpkg
4. vcpkg-only is a **nice-to-have**, not critical path

**For long-term vcpkg support**: Address in separate session after Session 56 validation

---

## Session 56 Testing Path (Option 2)

```bash
# Revert to submodule-based build (Session 57 baseline)
git checkout <commit-before-session-57>

# Build with submodules
mkdir build-test && cd build-test
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DWITH_TESTS=ON
ninja dwarfs_unit_tests mkdwarfs dwarfsck

# Test Session 56 fix
./dwarfs_unit_tests --gtest_filter="*ConverterRoundTrip*"

# Test Homebrew compatibility
mkdir -p /tmp/test-data && echo "test" > /tmp/test-data/file.txt
./mkdwarfs -i /tmp/test-data -o /tmp/our.dwarfs --format=thrift -l1
/opt/homebrew/bin/dwarfsck -i /tmp/our.dwarfs
```

**Estimated Time**: 45 minutes total

---

## Decision Needed

Which option do you want to pursue?

1. [ ] Option 1: Create mhx overlay ports for all (fizz, wangle, fbthrift)
2. [ ] **Option 2: Revert to submodules for Session 56 testing** (RECOMMENDED)
3. [ ] Option 3: Research matching Folly/wangle versions
4. [ ] Option 4: Patch wangle to fix typo

---

**Last Updated**: 2025-12-31 21:33 HKT
**Session 58 Time Invested**: ~90 minutes
**Blocker**: wangle API mismatch with mhx Folly fork
