# Session 45: Quick Start Prompt

**Read This First**: [`doc/SESSION_45_CONTINUATION_PLAN.md`](SESSION_45_CONTINUATION_PLAN.md)

---

## Context

Session 44 completed **Phases 1-2** (modular CMake structure). Remaining work: Phases 3-6.

**Critical Blocker**: parallel_hashmap dependency error prevents build.

---

## Your Task

Complete all remaining phases (3-6) in ~2.5 hours:

1. **Phase 3**: Fix phmap dependency, verify full build & vcpkg install
2. **Phase 4**: Create minimal `tools/CMakeLists.txt`, build tools separately
3. **Phase 5**: Run end-to-end integration tests
4. **Phase 6**: Update docs, archive session materials

---

## Start Here

**Step 1**: Fix blocking dependency
```bash
# Check error
cmake --build build-quick 2>&1 | grep phmap

# Expected: 'parallel_hashmap/phmap_config.h' file not found
```

**Step 2**: Verify `cmake/vcpkg/phmap.cmake` exists and is included

**Step 3**: Fix and test
```bash
# After fix:
rm -rf build-quick
cmake -B build-quick -DWITH_LIBDWARFS=ON -DWITH_TOOLS=OFF
cmake --build build-quick -j8
ls build-quick/libdwarfs_tool_support.a  # MUST exist
```

---

## Key Documents

1. **Session 44 Summary**: [`doc/SESSION_44_COMPLETION_SUMMARY.md`](SESSION_44_COMPLETION_SUMMARY.md)
2. **Full Plan**: [`doc/SESSION_45_CONTINUATION_PLAN.md`](SESSION_45_CONTINUATION_PLAN.md)
3. **Status Tracker**: [`doc/SESSION_45_IMPLEMENTATION_STATUS.md`](SESSION_45_IMPLEMENTATION_STATUS.md)

---

## Success Criteria

Complete workflow must work end-to-end:
```bash
rm -rf build* vcpkg_installed
cd example/static-site-server && ./build.sh
cd ../.. && cmake -B build-tools -S tools -DCMAKE_TOOLCHAIN_FILE=...
cmake --build build-tools
./build-tools/mkdwarfs -i /usr/share/dict -o test.dff
./build-tools/dwarfsck test.dff
./build-tools/dwarfsextract -i test.dff -o test-out
diff -r /usr/share/dict test-out  # Exit code 0
```

---

## Quick Reference

**Files Modified in Session 44**:
- ✅ Created: `cmake/tool_support.cmake`
- ✅ Modified: `cmake/libdwarfs.cmake` (removed inline def)
- ✅ Modified: `CMakeLists.txt` (added include)
- ✅ Modified: `vcpkg_ports/dwarfs/portfile.cmake` (cleanup)

**Files to Create in Session 45**:
- ⏳ `tools/CMakeLists.txt` (minimal, for separate builds)
- ⏳ `doc/vcpkg-integration.md` (comprehensive guide)
- ⏳ `doc/TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md` (summary)

**Files to Update in Session 45**:
- ⏳ `README.md` (add vcpkg section)
- ⏳ Archive: Move `doc/SESSION_4[1-4]_*.md` → `old-docs/sessions-41-44/`

---

**Timeline**: 2.5 hours | **Status**: Ready to start | **Blocker**: Fix phmap first