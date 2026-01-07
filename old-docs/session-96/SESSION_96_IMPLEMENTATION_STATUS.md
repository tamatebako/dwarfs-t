# Session 96: v0.17.0 Release Preparation - Implementation Status

**Date**: TBD
**Prerequisite**: Session 95 complete ✅
**Duration**: ~3 hours (estimated)
**Status**: ⏳ **PENDING** - Ready to start

---

## Overall Progress: 0% Complete (0/11 tasks)

| Task | Status | Completion | Time Spent | Notes |
|------|--------|------------|------------|-------|
| Task 1.1: Clean Build | ⏳ PENDING | 0% | 0 min | Test permanent CMake fix |
| Task 1.2: Test Suite | ⏳ PENDING | 0% | 0 min | Validate integration |
| Task 1.3: FB-Only Build | ⏳ PENDING | 0% | 0 min | Verify compatibility |
| Task 2.1: Test Images | ⏳ PENDING | 0% | 0 min | Create all formats |
| Task 2.2: Interoperability | ⏳ PENDING | 0% | 0 min | Test all tools |
| Task 2.3: Conversion | ⏳ PENDING | 0% | 0 min | Cross-format |
| Task 3.1: Doc Verification | ⏳ PENDING | 0% | 0 min | Accuracy check |
| Task 3.2: Move Docs | ⏳ PENDING | 0% | 0 min | Cleanup |
| Task 4.1: Version Numbers | ⏳ PENDING | 0% | 0 min | Update versions |
| Task 4.2: Release Notes | ⏳ PENDING | 0% | 0 min | Create RELEASE_NOTES |
| Task 4.3: Tag RC | ⏳ PENDING | 0% | 0 min | Git tag |

---

## Phase 1: Build Validation (0% complete)

### Task 1.1: Test Clean Build ⏳

**Status**: Not started
**Validation Checklist**:
- [ ] Build completes without errors
- [ ] No generator expressions in link.txt
- [ ] No manual fixes needed
- [ ] All libraries built

---

### Task 1.2: Run Full Test Suite ⏳

**Status**: Not started
**Validation Checklist**:
- [ ] All tests PASS
- [ ] Modern Thrift: 15/15 PASSED
- [ ] No regressions
- [ ] dwarfs_unit_tests includes Modern Thrift

---

### Task 1.3: Test Build Without Thrift ⏳

**Status**: Not started
**Validation Checklist**:
- [ ] FlatBuffers-only build works
- [ ] Modern Thrift files not compiled
- [ ] Tests pass without Modern Thrift
- [ ] Legacy Thrift still works

---

## Phase 2: Functional Validation (0% complete)

### Task 2.1: Create Test Images ⏳

**Status**: Not started
**Expected Outputs**:
- [ ] test.dff (FlatBuffers)
- [ ] test.dtc (Modern Thrift)
- [ ] test.dth (Legacy Thrift)

**Validation Checklist**:
- [ ] All formats create successfully
- [ ] Format detection correct
- [ ] Magic bytes verified
- [ ] Size ordering: Modern < FlatBuffers < Legacy

---

### Task 2.2: Test Format Interoperability ⏳

**Status**: Not started
**Validation Checklist**:
- [ ] dwarfsck works on all formats
- [ ] dwarfsextract works on all formats
- [ ] Extracted content identical
- [ ] No format-specific errors

---

### Task 2.3: Test Cross-Format Conversion ⏳

**Status**: Not started
**Validation Checklist**:
- [ ] FlatBuffers → Modern Thrift works
- [ ] Modern Thrift → FlatBuffers works
- [ ] Format detection after conversion
- [ ] Content preserved

---

## Phase 3: Documentation Validation (0% complete)

### Task 3.1: Verify Documentation Accuracy ⏳

**Status**: Not started
**Files to Verify**:
- [ ] MODERN_THRIFT_GUIDE.md examples
- [ ] mkdwarfs.md options
- [ ] dwarfs-format.md specification
- [ ] CHANGES.md v0.17.0 entry
- [ ] README.md tables
- [ ] vcpkg-integration.md commands

---

### Task 3.2: Move Remaining Temporary Docs ⏳

**Status**: Not started
**Files to Move**:
- [ ] SESSION_94_COMPLETION_SUMMARY.md → old-docs/sessions/
- [ ] SESSION_95_CONTINUATION_PLAN.md → old-docs/sessions/

**Files to Keep**:
- SESSION_95_COMPLETION_SUMMARY.md (current)
- SESSION_95_IMPLEMENTATION_STATUS.md (current)
- SESSION_96_CONTINUATION_PLAN.md (this session)

---

## Phase 4: Release Preparation (0% complete)

### Task 4.1: Update Version Numbers ⏳

**Status**: Not started
**Files to Check**:
- [ ] cmake/version.cmake
- [ ] PRJ_VERSION_FULL

---

### Task 4.2: Create Release Notes ⏳

**Status**: Not started
**File to Create**: RELEASE_NOTES_v0.17.0.md

**Sections**:
- [ ] Overview
- [ ] New Features
- [ ] Breaking Changes
- [ ] Migration Guide
- [ ] Known Issues
- [ ] Acknowledgments

---

### Task 4.3: Tag Release Candidate ⏳

**Status**: Not started
**Actions**:
- [ ] git tag v0.17.0-rc1
- [ ] git push origin v0.17.0-rc1
- [ ] Monitor CI/CD

---

## Metrics

### Code Changes
- **Files modified**: 0
- **Lines added**: 0
- **Net change**: 0 lines

### Testing
- **Builds run**: 0
- **Tests executed**: 0
- **Formats validated**: 0/3

### Documentation
- **Docs verified**: 0/6
- **Docs moved**: 0/2
- **Release notes**: Not started

### Time Investment
- **Task 1.1**: 0 min
- **Task 1.2**: 0 min
- **Task 1.3**: 0 min
- **Task 2.1**: 0 min
- **Task 2.2**: 0 min
- **Task 2.3**: 0 min
- **Task 3.1**: 0 min
- **Task 3.2**: 0 min
- **Task 4.1**: 0 min
- **Task 4.2**: 0 min
- **Task 4.3**: 0 min
- **Total**: 0 min / 180 min

---

## Completion Criteria

### Must Complete
- [ ] Clean build works without manual fixes
- [ ] All tests pass (15/15 Modern Thrift)
- [ ] All three formats functional
- [ ] Documentation accurate
- [ ] v0.17.0-rc1 tagged

### Should Complete
- [ ] Cross-format conversion verified
- [ ] Release notes complete
- [ ] CI/CD passes

### Nice to Have
- [ ] Performance benchmarks
- [ ] Cross-platform validation
- [ ] Community feedback

---

## Dependencies

### Completed
- ✅ Session 86: Architecture Design
- ✅ Session 87-92: Schema + Implementation
- ✅ Session 93: Compilation Fixes
- ✅ Session 94: Testing & Validation
- ✅ Session 95: Build Integration + Documentation

### Pending
- ⏳ Session 96: Release Preparation (this session)
- ⏳ v0.17.0: Final release

---

## Next Session Actions

### If Session 96 Completes Successfully
1. Monitor CI/CD for v0.17.0-rc1
2. Cross-platform testing
3. Gather community feedback
4. Tag v0.17.0 final

### If Issues Found
1. Document blockers
2. Create Session 97 for fixes
3. Re-tag RC2 after fixes

---

**Last Updated**: 2026-01-06 (created)
**Status**: PENDING - Ready to start
**Next**: Begin Task 1.1 (Test clean build)