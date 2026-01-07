# Session 31I Continuation Prompt

**Date**: 2025-12-23
**Context**: Complete domain-based metadata migration validation
**Previous Session**: 31H (Architectural fixes complete, core libraries build successfully)

## Quick Context

### What Was Accomplished (Session 31H)

**Architectural Purity Achieved** ✅
- Fixed 3 critical architectural violations:
  1. Removed `reinterpret_cast` in directory_view construction
  2. Fixed iterator type mismatch in `std::distance`
  3. Added iterator support to `domain_chunk_range_impl`

**Build Success** ✅
- Core libraries build cleanly with **0 errors**:
  - `libdwarfs_common.a` 
  - `libdwarfs_reader.a`
- FlatBuffers-only build works perfectly
- All type safety violations eliminated

**Files Modified** (4 files, 5 fixes):
- [`src/reader/internal/common_metadata_operations.cpp`](../src/reader/internal/common_metadata_operations.cpp)
- [`src/reader/metadata_types.cpp`](../src/reader/metadata_types.cpp)
- [`src/reader/internal/inode_reader_v2.cpp`](../src/reader/internal/inode_reader_v2.cpp)
- [`include/dwarfs/reader/internal/domain_metadata_views.h`](../include/dwarfs/reader/internal/domain_metadata_views.h)

**Documentation Created**:
- [`doc/SESSION_31H_STATUS.md`](SESSION_31H_STATUS.md) - Complete status
- [`doc/SESSION_31I_CONTINUATION_PLAN.md`](SESSION_31I_CONTINUATION_PLAN.md) - This session's plan
- [`doc/SESSION_31I_STATUS_TRACKER.md`](SESSION_31I_STATUS_TRACKER.md) - Progress tracker

## Your Mission (Session 31I)

Complete the validation and integration of the domain-based metadata architecture.

### Phase-by-Phase Execution

**Read these files FIRST**:
1. [`doc/SESSION_31I_CONTINUATION_PLAN.md`](SESSION_31I_CONTINUATION_PLAN.md) - Detailed plan
2. [`doc/SESSION_31I_STATUS_TRACKER.md`](SESSION_31I_STATUS_TRACKER.md) - Track progress here

### Phase 1: Build All Tools (20 min)

```bash
cd /Users/mulgogi/src/external/dwarfs
ninja -C build-fb-clean mkdwarfs dwarfsck dwarfsextract dwarfs
```

**Goal**: Verify all tools compile with the architectural fixes
**Success**: All 4 tools build with 0 errors

### Phase 2: Run Unit Tests (30 min)

```bash
# Build tests
ninja -C build-fb-clean dwarfs_unit_tests

# Run all tests
ctest --test-dir build-fb-clean --output-on-failure

# Run metadata tests specifically
ctest --test-dir build-fb-clean -R metadata --verbose
```

**Goal**: Validate correctness of architectural changes
**Success**: All tests pass with no regressions

### Phase 3: Integration Testing (45 min)

**Create & Verify**:
```bash
./build-fb-clean/mkdwarfs -i /usr/bin -o test-31i.dff --compression=zstd:level=3
./build-fb-clean/dwarfsck test-31i.dff --check-integrity
```

**Extract & Validate**:
```bash
mkdir -p extracted-31i
./build-fb-clean/dwarfsextract -i test-31i.dff -o extracted-31i/
# Verify byte-for-byte correctness with sample files
```

**Goal**: Confirm byte-for-byte correctness in real-world usage
**Success**: No data corruption, files match originals

### Phase 4: Performance Validation (15 min)

```bash
time ./build-fb-clean/dwarfsextract -i test-31i.dff -o /tmp/bench-extract/
```

**Goal**: Ensure no significant performance regression
**Success**: Performance within 5% of baseline

### Phase 5: Delete Legacy Code (20 min)

⚠️ **CRITICAL**: Only proceed if ALL previous phases passed ✅

```bash
# Remove 7,288 lines of duplicate backend code
git rm src/reader/internal/metadata_v2_flatbuffers.cpp
git rm src/reader/internal/metadata_v2_thrift.cpp
git rm src/reader/internal/metadata_types_flatbuffers.cpp
git rm src/reader/internal/metadata_types_thrift.cpp
```

**Goal**: Clean up legacy backend implementations
**Success**: 4 files deleted, 85.6% code reduction achieved

### Phase 6: Update CMake (15 min)

Edit [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake) - Remove 4 deleted files

**Goal**: Clean build system
**Success**: Rebuild works with 0 errors

### Phase 7: Git Commit (20 min)

Commit message template provided in continuation plan.

**Goal**: Finalize migration with comprehensive commit
**Success**: All changes committed with proper documentation

### Phase 8: Update Documentation (30 min)

Update official docs (README.adoc, etc.) and move temporary docs to old-docs/

**Goal**: Reflect architecture in official documentation
**Success**: Docs updated, temporary docs archived

## Critical Reminders

### Architectural Principles
- Maintain **separation of concerns** throughout
- All changes must be **MECE** (Mutually Exclusive, Collectively Exhaustive)
- Prioritize **correctness over passing tests** - if tests fail, analyze whether:
  1. Implementation needs fixing, OR
  2. Test expectations need updating to match new (correct) architecture
- **NEVER** lower pass thresholds or cut corners

### Quality Gates
- **DO NOT** proceed to Phase 5 unless Phases 1-4 ALL pass
- **DO NOT** commit if integration tests show data corruption
- **DO NOT** ignore test failures - analyze and fix properly

### Build Location
- Working directory: `/Users/mulgogi/src/external/dwarfs`
- Build directory: `build-fb-clean`
- All tools are in `build-fb-clean/`

## Expected Outcomes

### Must Achieve
- ✅ All tools build successfully
- ✅ All tests pass
- ✅ Integration tests confirm byte-for-byte correctness
- ✅ Legacy code deleted (7,288 lines)
- ✅ Changes committed

### Success Metrics
- **Code reduction**: 85.6% (7,288 → 1,675 lines)
- **Architectural violations**: 0
- **Build errors**: 0
- **Test failures**: 0
- **Data corruption**: 0

## How to Start

1. **Read** [`doc/SESSION_31I_CONTINUATION_PLAN.md`](SESSION_31I_CONTINUATION_PLAN.md) thoroughly
2. **Update** [`doc/SESSION_31I_STATUS_TRACKER.md`](SESSION_31I_STATUS_TRACKER.md) as you progress
3. **Begin** with Phase 1 (Build All Tools)
4. **Proceed** sequentially through phases
5. **Stop** immediately if any phase fails - analyze and fix before continuing

## If You Encounter Issues

### Tools Fail to Build
- Check linker errors carefully
- Apply same architectural patterns used for libraries in Session 31H
- May need additional interface implementations
- Document in SESSION_31I_STATUS_TRACKER.md

### Tests Fail
- **Analyze** root cause first
- Determine if implementation OR test expectations need updating
- Maintain correctness - NEVER compromise on quality
- Document rationale for any changes

### Integration Shows Data Issues
- **STOP** immediately
- Debug metadata round-trip carefully
- Fix data transformation before proceeding
- Do NOT delete legacy code until this is resolved

## Reference Documents

**Session History**:
- Session 31E: Initial domain migration architecture
- Session 31F: Domain view implementations
- Session 31G: Type system alignment
- Session 31H: **Architectural purity fixes** ← You are here
- Session 31I: **Validation & completion** ← What you'll do

**Key Files**:
- [`doc/SESSION_31H_STATUS.md`](SESSION_31H_STATUS.md) - What was just accomplished
- [`doc/SESSION_31I_CONTINUATION_PLAN.md`](SESSION_31I_CONTINUATION_PLAN.md) - Detailed execution plan
- [`doc/SESSION_31I_STATUS_TRACKER.md`](SESSION_31I_STATUS_TRACKER.md) - Track your progress

## Ready?

You have everything you need:
- ✅ Clean architectural foundation (Session 31H)
- ✅ Detailed execution plan
- ✅ Status tracker for progress
- ✅ Clear success criteria
- ✅ Contingency plans

**Start with Phase 1 and work systematically through each phase.**

Good luck! 🚀

---

**Last Updated**: 2025-12-23 14:13 HKT
**Status**: Ready to begin
**Estimated Duration**: 3 hours 15 minutes