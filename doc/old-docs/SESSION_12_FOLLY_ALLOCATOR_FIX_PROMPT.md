# Session 12 Continuation Prompt - Fix Folly Allocator Linking

**Created**: 2025-12-17
**Priority**: CRITICAL
**Estimated Time**: 2-3 hours
**Context**: Session 11 revealed Folly allocator linking issues on macOS ARM64

---

## Current Situation

Session 11 completed cross-format testing and created automated test infrastructure. However, a critical issue was identified:

**Problem**: Thrift-only and dual-format builds fail on macOS ARM64 with undefined allocator symbols:
```
Undefined symbols for architecture arm64:
  "folly::detail::UsingJEMallocInitializer::operator()() const"
  "folly::detail::UsingTCMallocInitializer::operator()() const"
ld: symbol(s) not found for architecture arm64
```

**Status**: This is NOT acceptable as a platform limitation - it must be fixed.

---

## Objective

Fix Folly allocator configuration so that Thrift-only and dual-format builds work on macOS ARM64 without requiring jemalloc/tcmalloc.

**Success Criteria**:
- ✅ Thrift-only builds successfully on macOS ARM64
- ✅ Dual-format builds successfully on macOS ARM64
- ✅ All 3 configurations (FlatBuffers-only, Thrift-only, both-formats) pass 18/18 tests
- ✅ Automated test script (`scripts/test-all-configs.sh`) runs all configs on macOS

---

## Architecture Principle

**CRITICAL**: This must be fixed with a proper architectural solution, not workarounds:

✅ **YES**: Centralized configuration in `cmake/folly.cmake`
✅ **YES**: Explicit compile definitions propagated to all targets
✅ **YES**: Platform-aware defaults

❌ **NO**: Code guards scattered across source files
❌ **NO**: Platform-specific workarounds in test scripts
❌ **NO**: "Known limitation" documentation

---

## Implementation Guide

### Phase A: Investigate Current Configuration (30 min)

**Read and analyze**:
1. [`cmake/folly.cmake`](../cmake/folly.cmake) - Complete file
2. Look for allocator detection logic
3. Check if `FOLLY_USE_JEMALLOC` compile definition is set anywhere
4. Verify how `USE_JEMALLOC` CMake option affects Folly build

**Expected findings**:
- Folly's allocator auto-detection runs regardless of `USE_JEMALLOC` setting
- Missing explicit `FOLLY_USE_JEMALLOC=0` compile definition
- No platform-specific handling for macOS ARM64

**Deliverable**: Create analysis notes documenting:
- Current allocator configuration approach
- Missing settings
- Proposed fix location

### Phase B: Implement Fix (45 min)

**File to modify**: [`cmake/folly.cmake`](../cmake/folly.cmake)

**Changes required**:

1. **Add explicit allocator disabling** when `USE_JEMALLOC=OFF`:
   ```cmake
   # After folly_base target is defined, add:
   
   # Explicit allocator control - disable when not using jemalloc
   if(NOT USE_JEMALLOC)
     target_compile_definitions(folly_base PUBLIC
       FOLLY_USE_JEMALLOC=0
       FOLLY_USE_TCMALLOC=0
     )
   endif()
   ```

2. **Add macOS ARM64-specific handling** (if needed):
   ```cmake
   # Platform-specific allocator settings
   if(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
     target_compile_definitions(folly_base PUBLIC
       FOLLY_HAVE_MALLOC_USABLE_SIZE=0
     )
   endif()
   ```

3. **Verify propagation**:
   - Ensure `PUBLIC` visibility so definitions propagate to dependent targets
   - Check that all Folly libraries (folly_base, folly, etc.) get these definitions

**Critical**: Use `target_compile_definitions` with `PUBLIC` scope, not `add_definitions` or `PRIVATE` scope.

### Phase C: Verify Fix (30 min)

**Test 1: Thrift-Only Build**:
```bash
rm -rf build-thrift-verify
cmake -B build-thrift-verify -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DUSE_JEMALLOC=OFF \
  -DENABLE_RICEPP=OFF \
  -DWITH_TESTS=ON \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=OFF

ninja -C build-thrift-verify dwarfs_filesystem_tests
```

**Expected**: ✅ Build succeeds without undefined symbols

**Test 2: Dual-Format Build**:
```bash
rm -rf build-both-verify
cmake -B build-both-verify -GNinja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DUSE_JEMALLOC=OFF \
  -DENABLE_RICEPP=OFF \
  -DWITH_TESTS=ON \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=OFF

ninja -C build-both-verify dwarfs_filesystem_tests
```

**Expected**: ✅ Build succeeds without undefined symbols

**Test 3: Run Tests**:
```bash
./build-thrift-verify/dwarfs_filesystem_tests --gtest_color=yes
./build-both-verify/dwarfs_filesystem_tests --gtest_color=yes
```

**Expected**: ✅ 18/18 tests passing in both configurations

### Phase D: Update Test Script (15 min)

**File to modify**: [`scripts/test-all-configs.sh`](../scripts/test-all-configs.sh)

**Changes**:
- Remove the platform detection workaround (lines ~24-30)
- Allow all 3 configurations to run on macOS

**Before** (remove these lines):
```bash
# Skip problematic configs on macOS
if [[ "$PLATFORM" == "Darwin" ]]; then
  echo "⚠️  macOS detected: Skipping Thrift/dual-format (Folly linking issues)"
  echo ""
  CONFIGS=("flatbuffers-only")
  FLATBUFFERS_FLAGS=("ON")
  THRIFT_FLAGS=("OFF")
fi
```

**After**: Let the normal configuration array run on all platforms.

**Verify**:
```bash
./scripts/test-all-configs.sh
```

**Expected output**:
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

**Create**:
1. `doc/SESSION_12_COMPLETE_SUMMARY.md` - Document the fix
   - Problem statement
   - Root cause analysis
   - Solution implemented
   - Test results
   - Architecture validation

**Update**:
1. `.kilocode/rules/memory-bank/context.md` - Mark Session 12 complete
2. `doc/SESSION_11_COMPLETE_SUMMARY.md` - Add note that fix came in Session 12

---

## Files to Work With

### Read First
- `cmake/folly.cmake` - Folly build configuration (PRIMARY FIX LOCATION)
- `doc/SESSION_11_COMPLETE_SUMMARY.md` - Context from previous session
- `scripts/test-all-configs.sh` - Automated test script

### Modify
- `cmake/folly.cmake` - Add allocator compile definitions
- `scripts/test-all-configs.sh` - Remove platform workaround

### Create
- `doc/SESSION_12_COMPLETE_SUMMARY.md` - Session documentation

### Update
- `.kilocode/rules/memory-bank/context.md` - Update current status
- `doc/SESSION_12_FOLLY_ALLOCATOR_FIX_STATUS.md` - Mark phases complete

---

## Debugging Help

### If Build Still Fails

**Check 1**: Verify compile definitions are set:
```bash
# Look at compiler invocation
ninja -C build-thrift-verify -v | grep FOLLY_USE_JEMALLOC
```

**Check 2**: Verify no conflicting definitions:
```bash
# Search for other FOLLY_USE_JEMALLOC settings
grep -r "FOLLY_USE_JEMALLOC" cmake/ folly/
```

**Check 3**: Check link line for allocator libraries:
```bash
# Look at linker invocation
ninja -C build-thrift-verify -v | grep "c++ .*dwarfs_filesystem_tests"
# Should NOT contain -ljemalloc or -ltcmalloc
```

### If Tests Fail

**Scenario**: Build succeeds but tests fail

**Diagnosis**:
1. Check if it's format-specific (Thrift vs FlatBuffers issue)
2. Compare test output between working FlatBuffers-only and Thrift builds
3. Look for allocator-related errors in test output

**Fix**: Likely unrelated to allocator fix - address separately

---

## Success Indicators

### During Implementation

✅ **Phase A Complete**: Clear understanding of current Folly configuration
✅ **Phase B Complete**: CMake changes applied, no syntax errors
✅ **Phase C Complete**: All 3 configs build and test successfully
✅ **Phase D Complete**: Test script runs all configs
✅ **Phase E Complete**: Documentation updated

### Final Validation

✅ **No undefined symbols**: All builds link successfully
✅ **No allocator libraries**: Link line is clean
✅ **All tests pass**: 18/18 in all 3 configurations
✅ **Automation works**: Script tests all configs
✅ **Clean architecture**: Centralized configuration, no scattered fixes

---

## Common Pitfalls to Avoid

❌ **Adding code guards**: Don't modify source files, fix in CMake
❌ **Platform-specific scripts**: Fix the build, not the test infrastructure
❌ **Incomplete propagation**: Ensure all Folly targets get definitions
❌ **Conflicting definitions**: Remove old allocator settings if present
❌ **Documentation shortcuts**: Properly document the architectural solution

---

## Reference Materials

**Folly Allocator Documentation**:
- Folly uses jemalloc/tcmalloc for performance
- Falls back to system malloc when not available
- Requires compile-time configuration

**CMake Best Practices**:
- Use `target_compile_definitions` not `add_definitions`
- Use `PUBLIC` scope for propagation
- Platform detection via `CMAKE_SYSTEM_PROCESSOR`

**DwarFS Principles**:
- Centralized configuration (MECE)
- Platform abstraction
- No code guards unless absolutely necessary

---

## Post-Session Checklist

After completing Session 12:

- [ ] All 3 configurations build on macOS ARM64
- [ ] All 3 configurations pass 18/18 tests
- [ ] Test automation script works without platform skipping
- [ ] SESSION_12_COMPLETE_SUMMARY.md created
- [ ] Memory bank context.md updated
- [ ] Session 12 status tracker updated
- [ ] Fix validated on at least macOS ARM64
- [ ] CI/CD implications documented

---

**Status**: READY TO START
**Estimated Duration**: 2-3 hours
**Next Action**: Read [`cmake/folly.cmake`](../cmake/folly.cmake) and analyze current allocator configuration