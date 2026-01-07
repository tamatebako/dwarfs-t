# Test Suite Refactoring - Session 7 Continuation Plan

**Created**: 2025-12-14
**For Session**: 7 (Final)
**Estimated Time**: 1-2 hours (Option A) OR 10-15 hours (Option B)
**Prerequisites**: Read [`TEST_SUITE_SESSION_6_ARCHITECTURAL_ISSUE.md`](TEST_SUITE_SESSION_6_ARCHITECTURAL_ISSUE.md)

---

## CRITICAL: Decision Required Before Starting

Session 6 discovered that the extraction approach from Session 5 was flawed. You must choose between:

- **Option A**: Ship minimal (filesystem tests only) for v0.16.0 ✅ Recommended
- **Option B**: Full refactoring before v0.16.0 (10-15 hours) ⏸️ Defer to v0.17.0

**Read the architectural issue document first**, then proceed with the chosen option below.

---

## Option A: Minimal Extraction (RECOMMENDED) ✅

**Goal**: Ship working filesystem tests for v0.16.0, defer full extraction to v0.17.0

**Estimated Time**: 1-2 hours

### Phase A.1: Build & Test Filesystem Tests (30min)

1. **Build the filesystem tests**:
   ```bash
   cd build-test
   ninja dwarfs_filesystem_tests
   ```

2. **Run tests**:
   ```bash
   ctest -R filesystem --output-on-failure
   ```

3. **Verify**:
   - Tests compile ✓
   - Tests execute ✓
   - Pass rate matches expectations

**Success Criteria**: Both filesystem test files compile and pass

### Phase A.2: Documentation Updates (30min)

1. **Update README.adoc** - Add section on modular test suite:
   ```adoc
   == Testing

   DwarFS has a comprehensive test suite organized into modular components:

   * `dwarfs_filesystem_tests` - Core filesystem operations (modular)
   * `dwarfs_unit_tests` - Unit tests for individual components
   * `dwarfs_expensive_tests` - Full integration tests
   * Additional specialized test suites

   To run tests:
   [source,bash]
   ----
   cmake -B build -DWITH_TESTS=ON
   ninja -C build
   ctest --test-dir build
   ----
   ```

2. **Create TEST_ARCHITECTURE.md** - Document test organization
3. **Update SESSION_6 status** - Mark as complete

### Phase A.3: Cleanup & Commit (30min)

1. **Remove Session 5 temporary files**:
   ```bash
   rm -f doc/TEST_SUITE_SESSION_5_*.md
   rm -f doc/TEST_SUITE_SESSION_4_*.md
   ```

2. **Update memory bank** - Document current state

3. **Git commit**:
   ```bash
   git add test/filesystem/ cmake/tests.cmake doc/
   git commit -m "refactor(tests): extract filesystem tests to modular suite

   - Add dwarfs_filesystem_tests target with 2 test files
   - Document architectural limitation preventing full extraction
   - Defer scanner/packing test extraction to v0.17.0

   Closes #XXX"
   ```

**Total Time**: ~1.5 hours

**Deliverables**:
- ✅ Filesystem tests building and passing
- ✅ Documentation updated
- ✅ Clean commit ready for PR
- ✅ v0.16.0 release ready

---

## Option B: Full Refactoring (DEFER TO v0.17.0) ⏸️

**Goal**: Make ALL tests extractable by refactoring `basic_end_to_end_test()`

**Estimated Time**: 10-15 hours

**⚠️ NOT RECOMMENDED FOR v0.16.0** - Too much risk and effort

<details>
<summary>Click to expand Option B details (for v0.17.0)</summary>

### Phase B.1: Refactor basic_end_to_end_test (4-6 hours)

1. **Move function to test_common**:
   - Declare in `test/test_common.h`
   - Implement in `test/test_common.cpp`
   - Update all 50+ call sites in `dwarfs_test.cpp`

2. **Update compressions handling**:
   - Ensure `test::compressions` is properly accessible
   - Remove anonymous namespace version
   - Update references

3. **Fix default_file_hash_algo**:
   - Clarify variable vs function usage
   - Update incorrect call sites

### Phase B.2: Extract Scanner Tests (2-3 hours)

1. Create `test/scanner/` with Session 5 files
2. Update namespace references
3. Update CMake
4. Build and test

### Phase B.3: Extract Packing Tests (2-3 hours)

1. Create `test/metadata/packing_test.cpp`
2. Update namespace references
3. Update CMake
4. Build and test

### Phase B.4: Comprehensive Testing (2-3 hours)

1. Run full test suite
2. Fix any regressions
3. Verify all tests pass
4. Performance testing

**Total Time**: 10-15 hours

**Risks**:
- High probability of test failures requiring investigation
- May discover additional anonymous namespace dependencies
- Could delay v0.16.0 release

**Recommendation**: ⏸️ **DEFER** to v0.17.0 after stable release

</details>

---

## Quick Start for Session 7

### If Choosing Option A (Recommended)

```bash
# 1. Read the architectural issue doc
cat doc/TEST_SUITE_SESSION_6_ARCHITECTURAL_ISSUE.md

# 2. Build filesystem tests
cd build-test
ninja dwarfs_filesystem_tests

# 3. Run tests
ctest -R filesystem --output-on-failure

# 4. If successful, proceed with documentation updates
```

### If Choosing Option B (Not Recommended)

```bash
# 1. Discuss with team - this is a big effort
# 2. Plan full refactoring approach
# 3. Create RFC for basic_end_to_end_test refactoring
# 4. Follow Phase B.1-B.4 above
```

---

## Files to Work With

### Option A Files

**To Modify**:
- `README.adoc` - Add testing section
- `doc/TEST_ARCHITECTURE.md` - Create new doc
- `doc/TEST_SUITE_SESSION_6_ARCHITECTURAL_ISSUE.md` - Mark as reviewed

**To Remove**:
- `doc/TEST_SUITE_SESSION_5_STATUS.md`
- `doc/TEST_SUITE_SESSION_4_COMPLETE.md`
- Old session planning docs

**To Keep**:
- `test/filesystem/filesystem_basic_test.cpp`
- `test/filesystem/filesystem_uid_gid_test.cpp`
- `cmake/tests.cmake`

### Option B Files

**To Create**:
- `doc/RFC_BASIC_END_TO_END_TEST_REFACTORING.md`
- `test/scanner/*.cpp` (restore from Session 5 backup if needed)
- `test/metadata/packing_test.cpp`

**To Modify**:
- `test/test_common.h` - Add `basic_end_to_end_test()` declaration
- `test/test_common.cpp` - Add `basic_end_to_end_test()` implementation
- `test/dwarfs_test.cpp` - Update all call sites
- `cmake/tests.cmake` - Add scanner and metadata targets

---

## Success Criteria

### Option A Success

- [x] Filesystem tests compile
- [x] Filesystem tests pass
- [ ] README.adoc updated with testing section
- [ ] TEST_ARCHITECTURE.md created
- [ ] Old session docs moved to archive/
- [ ] Clean git commit ready
- [ ] v0.16.0 release ready

### Option B Success

- [ ] `basic_end_to_end_test()` in test_common
- [ ] All call sites updated
- [ ] Scanner tests extracted and passing
- [ ] Packing tests extracted and passing
- [ ] All test suites passing
- [ ] No performance regression
- [ ] Documentation complete

---

## Rollback Plan

If either option encounters issues:

1. **Immediate**: `git reset --hard HEAD` to revert uncommitted changes
2. **Verify**: Ensure build still works: `cd build-test && ninja && ctest`
3. **Document**: Update continuation plan with findings
4. **Reassess**: Determine if approach needs revision

---

## Timeline

### Option A (Recommended)

| Phase | Time | Cumulative |
|-------|------|------------|
| Build & Test | 30min | 30min |
| Documentation | 30min | 1h |
| Cleanup | 30min | 1.5h |
| **Total** | **1.5h** | **1.5h** |

**Ready for v0.16.0**: ✅ YES

### Option B (Deferred)

| Phase | Time | Cumulative |
|-------|------|------------|
| Refactor basic_end_to_end_test | 4-6h | 6h |
| Extract Scanner | 2-3h | 9h |
| Extract Packing | 2-3h | 12h |
| Testing | 2-3h | 15h |
| **Total** | **10-15h** | **15h** |

**Ready for v0.16.0**: ❌ NO (too risky)

---

## Recommendation

✅ **PROCEED WITH OPTION A**

**Rationale**:
1. Gets value to users faster (modular filesystem tests)
2. Low risk - only touches working code
3. Keeps v0.16.0 on schedule
4. Allows proper planning for full refactoring in v0.17.0
5. Documents the limitation clearly

**Next Release (v0.17.0)**:
- Plan full test suite refactoring
- Implement Option B properly
- Complete modular test architecture

---

## Questions to Answer

Before starting Session 7, answer:

1. **Do we ship v0.16.0 now or delay?** → If now, choose Option A
2. **Is full test refactoring critical?** → If no, choose Option A
3. **Can we afford 10-15 hours?** → If no, choose Option A
4. **Is partial value acceptable?** → If yes, choose Option A

**Default Choice**: Option A (unless strong reason for Option B)

---

**Document Status**: Ready for Session 7
**Created**: 2025-12-14 22:48 HKT
**Updated**: 2025-12-14 22:48 HKT
**Next Action**: Choose Option A or B, then begin work