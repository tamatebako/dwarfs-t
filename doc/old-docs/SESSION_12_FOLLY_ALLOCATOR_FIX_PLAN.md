# Session 12: Fix Folly Allocator Linking on macOS ARM64

**Created**: 2025-12-17
**Priority**: CRITICAL
**Estimated Time**: 2-3 hours
**Objective**: Make Thrift/Folly work with static builds on macOS ARM64

---

## Problem Statement

### Current Issue

Thrift-only and dual-format builds fail on macOS ARM64 with:
```
Undefined symbols for architecture arm64:
  "folly::detail::UsingJEMallocInitializer::operator()() const"
  "folly::detail::UsingTCMallocInitializer::operator()() const"
ld: symbol(s) not found for architecture arm64
```

### Root Cause Analysis

**Symptom**: Folly expects jemalloc/tcmalloc symbols even when `USE_JEMALLOC=OFF`

**Likely Causes**:
1. Folly's allocator detection not respecting CMake flags
2. Missing Folly compile definitions (`FOLLY_USE_JEMALLOC=0`)
3. Incomplete Folly build configuration in `cmake/folly.cmake`
4. Platform-specific allocator handling needed for macOS ARM64

**Architecture Issue**: Allocator selection should be:
- **Configurable** via CMake options
- **Consistent** across all Folly compilation units
- **Platform-aware** (macOS ARM64 has different defaults)
- **Zero-dependency** when disabled (no allocator libs needed)

---

## Solution Architecture

### Design Principles

1. **Separation of Concerns**:
   - Allocator selection (CMake configuration layer)
   - Folly compilation (build system layer)
   - Symbol resolution (linker layer)

2. **Platform Abstraction**:
   - Default allocator per platform
   - Override mechanism via CMake options
   - Automatic fallback to system malloc

3. **MECE Structure**:
   - One source of truth for allocator choice
   - All Folly targets use same configuration
   - No conflicting definitions

### Solution Approach

**NOT**: Code guards or conditional compilation scattered across files
**YES**: Centralized configuration in `cmake/folly.cmake` with proper propagation

---

## Implementation Plan

### Phase A: Investigate Current Folly Configuration (30 min)

**Objective**: Understand how Folly is currently built

**Steps**:
1. Read `cmake/folly.cmake` completely
2. Identify where allocator is configured
3. Check if `FOLLY_USE_JEMALLOC` is set
4. Verify compile definitions propagate to all targets

**Deliverables**:
- Analysis document of current configuration
- List of missing/incorrect settings

### Phase B: Fix Folly Allocator Configuration (45 min)

**Objective**: Ensure Folly builds without jemalloc when `USE_JEMALLOC=OFF`

**Required Changes**:

1. **In `cmake/folly.cmake`**:
   ```cmake
   # When USE_JEMALLOC=OFF, explicitly disable Folly allocator detection
   if(NOT USE_JEMALLOC)
     target_compile_definitions(folly_base PUBLIC
       FOLLY_USE_JEMALLOC=0
       FOLLY_USE_TCMALLOC=0
     )
   endif()
   ```

2. **Verify Target Propagation**:
   - Ensure all Folly targets inherit these definitions
   - Check both PUBLIC and INTERFACE link dependencies

3. **Platform-Specific Handling**:
   ```cmake
   if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
     # macOS ARM64: Force system malloc
     target_compile_definitions(folly_base PUBLIC
       FOLLY_HAVE_MALLOC_USABLE_SIZE=0
     )
   endif()
   ```

**Architecture**: Centralized configuration, explicit overrides, platform-aware defaults

### Phase C: Verify Build and Linking (30 min)

**Objective**: Confirm fix works for all configurations

**Test Matrix**:
1. **Thrift-only build**:
   ```bash
   cmake -B build-thrift -DDWARFS_WITH_FLATBUFFERS=OFF \
         -DDWARFS_WITH_THRIFT=ON -DUSE_JEMALLOC=OFF
   ninja -C build-thrift dwarfs_filesystem_tests
   ```

2. **Dual-format build**:
   ```bash
   cmake -B build-both -DDWARFS_WITH_FLATBUFFERS=ON \
         -DDWARFS_WITH_THRIFT=ON -DUSE_JEMALLOC=OFF
   ninja -C build-both dwarfs_filesystem_tests
   ```

**Success Criteria**:
- Builds complete without undefined symbols
- Tests pass (18/18 expected)
- No jemalloc/tcmalloc libraries linked

### Phase D: Update Test Script (15 min)

**Objective**: Remove macOS workaround from automated script

**Changes to `scripts/test-all-configs.sh`**:
- Remove platform detection skip logic
- Test all 3 configs on macOS
- Verify all pass

**Expected Output**:
```
Platform: Darwin arm64

Testing: flatbuffers-only
✅ PASSED: flatbuffers-only (18 tests)

Testing: thrift-only
✅ PASSED: thrift-only (18 tests)

Testing: both-formats
✅ PASSED: both-formats (18 tests)

✅ ALL TESTED CONFIGURATIONS PASSED
```

### Phase E: Documentation (15 min)

**Objective**: Document the fix and update status

**Deliverables**:
1. **SESSION_12_COMPLETE_SUMMARY.md**: Implementation details
2. **Update context.md**: Mark issue resolved
3. **Update README.adoc**: Remove platform limitation note

---

## Technical Details

### Folly Allocator Detection Mechanism

**How Folly Detects Allocators**:
1. CMake probes for jemalloc/tcmalloc libraries
2. Sets `FOLLY_USE_JEMALLOC` and `FOLLY_USE_TCMALLOC` accordingly
3. Generates code that calls allocator-specific APIs

**Problem on macOS ARM64**:
- Folly finds jemalloc headers during configuration
- Generates code expecting jemalloc symbols
- But library not linked due to `USE_JEMALLOC=OFF`
- Result: Undefined symbols at link time

**Solution**:
- **Override** Folly's detection with explicit compile definitions
- **Propagate** to all downstream targets
- **Verify** no allocator libraries in link line

### Symbol Resolution Flow

```
CMake Configuration
    ↓
Folly Allocator Detection (AUTO)
    ↓
[OVERRIDE HERE] Force FOLLY_USE_JEMALLOC=0
    ↓
Folly Compilation (no allocator code)
    ↓
Linking (no allocator symbols needed)
    ↓
Success ✅
```

### Platform-Specific Considerations

**macOS ARM64**:
- System malloc is efficient (no need for jemalloc)
- Static linking has strict symbol requirements
- Requires explicit allocator disabling

**Linux x86_64**:
- jemalloc commonly available
- Dynamic linking more tolerant
- May work without explicit overrides

**Windows**:
- tcmalloc not typically used
- Uses system allocator by default
- Should work without changes

---

## Alternative Solutions (Rejected)

### Alternative 1: Use jemalloc
**Rejected**: Adds unnecessary dependency, complicates static builds

### Alternative 2: Dynamic linking
**Rejected**: Defeats purpose of static builds for Tebako

### Alternative 3: Patch Folly source
**Rejected**: Not maintainable, upstream changes break builds

### Alternative 4: Conditional compilation guards
**Rejected**: Violates architectural principle of centralized configuration

---

## Success Metrics

1. **Build Success**:
   - ✅ Thrift-only builds on macOS ARM64
   - ✅ Dual-format builds on macOS ARM64
   - ✅ No undefined symbols

2. **Test Success**:
   - ✅ 18/18 tests pass in Thrift-only
   - ✅ 18/18 tests pass in dual-format
   - ✅ All configs tested by automation script

3. **Architecture Quality**:
   - ✅ Centralized allocator configuration
   - ✅ Platform-aware defaults
   - ✅ No code guards needed
   - ✅ Extensible for new platforms

4. **Documentation**:
   - ✅ Fix documented
   - ✅ Platform limitation removed
   - ✅ Test matrix updated

---

## Risk Mitigation

### Risk 1: Folly Requires Allocator

**Probability**: Low
**Impact**: High
**Mitigation**: System malloc is always available

### Risk 2: Performance Degradation

**Probability**: Low
**Impact**: Medium
**Mitigation**: macOS ARM64 system malloc is efficient

### Risk 3: Other Platforms Affected

**Probability**: Medium
**Impact**: Medium
**Mitigation**: Test on Linux before finalizing

---

## Next Steps After This Session

1. **CI/CD Integration**: Ensure all platforms test all configs
2. **Performance Comparison**: Benchmark with/without jemalloc
3. **Documentation**: Update user-facing docs with platform matrix
4. **Release**: Include in v0.16.0 release notes

---

**Status**: Ready to implement
**Blocker**: None
**Dependencies**: None (uses existing Folly submodule)