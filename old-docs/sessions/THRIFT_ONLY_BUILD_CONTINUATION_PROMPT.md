# Thrift-Only Build Verification - Continuation Prompt

**Date**: 2025-12-02  
**Status**: Ready to Execute  
**Priority**: CRITICAL  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`

---

## Quick Context

You are continuing work on **DwarFS (Tebako Fork)** to ensure **Thrift-only builds work correctly**. The CI has been configured to expect Thrift-only builds to pass, but this has NOT been verified. Your task is to build Thrift-only locally, identify any issues, fix them, and ensure all tests pass.

---

## Session Startup Commands

```bash
# 1. Navigate to project
cd /Users/mulgogi/src/external/dwarfs

# 2. Verify branch
git branch --show-current

# 3. Read planning documents
cat doc/THRIFT_ONLY_BUILD_VERIFICATION_PLAN.md
cat doc/THRIFT_ONLY_BUILD_IMPLEMENTATION_STATUS.md

# 4. Review memory bank
cat .kilocode/rules/memory-bank/context.md
```

---

## Immediate First Step: Phase 1 Verification

**DO THIS FIRST** - Attempt Thrift-only build to see what breaks:

```bash
# Clean any existing Thrift-only build
rm -rf build-tb

# Configure Thrift-only (this may fail)
cmake -B build-tb -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TOOLS=ON \
  -DWITH_TESTS=ON \
  -DWITH_FUSE_DRIVER=ON 2>&1 | tee build-tb-config.log

# If config succeeds, build (this may fail)
cmake --build build-tb 2>&1 | tee build-tb-build.log

# If build succeeds, test (this may fail)
cd build-tb && ./dwarfs_unit_tests 2>&1 | tee ../build-tb-test.log
```

**Analysis**: Review logs and identify the failure point:
- **CMake fails**: Missing FlatBuffers dependency (Phase 3)
- **Build fails**: Unconditional FlatBuffers includes (Phase 4)
- **Tests fail**: FlatBuffers-specific tests not guarded (Phase 4)

---

## What This Session Must Accomplish

### Critical Path (5.5 hours):
1. **Phase 1** (30 min): Identify what breaks in Thrift-only build
2. **Phase 2** (30 min): Root cause analysis via code search
3. **Phase 3** (1 hour): Fix CMake to make FlatBuffers truly optional
4. **Phase 4** (2 hours): Fix source code - guard includes, registry, defaults
5. **Phase 5** (1 hour): Build all 3 configs, verify tests pass
6. **Phase 6** (30 min): Update documentation

---

## Key Technical Points

### Current Architecture Issue

The code was designed with "FlatBuffers required" assumption. To support Thrift-only:

1. **CMake must conditionally require FlatBuffers**:
   ```cmake
   if(DWARFS_WITH_FLATBUFFERS)
     find_package(FlatBuffers CONFIG)  # Remove REQUIRED
   endif()
   ```

2. **Source must guard FlatBuffers includes**:
   ```cpp
   #ifdef DWARFS_HAVE_FLATBUFFERS
   #include <flatbuffers/...>
   #endif
   ```

3. **Registry must conditionally register serializers**:
   ```cpp
   #ifdef DWARFS_HAVE_FLATBUFFERS
   register_serializer(make_unique<FlatBuffersSerializer>());
   #endif
   ```

4. **Default format must adapt**:
   ```cpp
   #ifdef DWARFS_HAVE_FLATBUFFERS
     default = "flatbuffers";
   #elif defined(DWARFS_HAVE_THRIFT)
     default = "thrift";
   #endif
   ```

### Files Most Likely to Need Changes

**CMake** (Phase 3):
- `cmake/metadata_serialization.cmake` - FlatBuffers find_package
- `cmake/libdwarfs.cmake` - Library dependencies

**Source** (Phase 4):
- `src/metadata/serialization/serializer_registry.cpp` - Registry
- `src/metadata/serialization/facade_factory.cpp` - Factory
- `src/reader/internal/*flatbuffers*` - Reader implementations
- `tools/src/*_main.cpp` - Tool default formats

**Tests** (Phase 4):
- `test/metadata/*flatbuffers*` - Guard with #ifdef

---

## Expected Issues & Solutions

### Issue 1: CMake "FlatBuffers not found"
**Solution**: Remove `REQUIRED`, make target creation conditional

### Issue 2: Compile error "flatbuffers.h not found"
**Solution**: Guard all FlatBuffers includes with `#ifdef DWARFS_HAVE_FLATBUFFERS`

### Issue 3: Runtime "No serializers registered"
**Solution**: Ensure Thrift serializer registers when THRIFT=ON

### Issue 4: Tests fail "Unknown format"
**Solution**: Make format defaults conditional on available formats

---

## Success Criteria (All Must Pass)

- [ ] CMake configuration succeeds with `FLATBUFFERS=OFF, THRIFT=ON`
- [ ] Build completes without errors
- [ ] Tests pass: **1,600/1,613** (13 FlatBuffers tests skipped)
- [ ] mkdwarfs creates Thrift images
- [ ] dwarfsck verifies Thrift images
- [ ] CI passes for Thrift-only configuration
- [ ] No regressions in FlatBuffers-only or Dual-format

---

## Quick Reference

**Current Builds**:
- `build-fb/` - FlatBuffers-only (✅ working, 1,600/1,613 tests)
- `build-tb/` - Thrift-only (❌ not verified, needs fixing)
- `build-dual/` - Not built yet (will test after fixes)

**Key Files Created This Session**:
1. `.github/workflows/benchmark-comprehensive.yml` - 3-build CI
2. `doc/BENCHMARK_CI_GUIDE.md` - Updated for 3 builds
3. `doc/THRIFT_ONLY_BUILD_VERIFICATION_PLAN.md` - This plan
4. `doc/THRIFT_ONLY_BUILD_IMPLEMENTATION_STATUS.md` - Status tracker
5. `scripts/verify_benchmark_setup.sh` - Verification script

---

## Communication Protocol

**When build fails**, report:
1. Failure point (CMake/Build/Test)
2. Exact error message
3. Affected files

**When asking questions**, provide:
1. Current phase
2. Specific issue encountered
3. Attempted solutions

---

## Timeline Pressure

**Deadline**: Need to compress work to finish ASAP

**Priority Order**:
1. GET THRIFT-ONLY BUILDING (even if tests fail initially)
2. GET TESTS PASSING
3. VALIDATE CI
4. UPDATE DOCS

**Acceptable Shortcuts**:
- Skip comprehensive benchmarking initially
- Focus on core functionality first
- Polish documentation later

---

## Ready to Start?

1. Run [session startup commands](#session-startup-commands)
2. Start with Phase 1 - Attempt Thrift-only build
3. Report results immediately
4. Follow plan based on failure point

---

**Status**: 🔴 Ready to Execute  
**Next Action**: Attempt Thrift-only build (Phase 1.1)  
**Expected**: Build likely fails, need to identify root cause  
**Estimated Time to Fix**: 5.5 hours (compressed timeline)