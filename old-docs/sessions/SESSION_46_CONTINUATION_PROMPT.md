# Session 46: Quick Start Prompt

**Read This First**: [`doc/SESSION_46_CONTINUATION_PLAN.md`](SESSION_46_CONTINUATION_PLAN.md)

---

## Context

Session 45 completed tool_support library infrastructure but left 2 **CRITICAL BUGS**:

1. ❌ **Manpage support broken** - `--man` option doesn't work
2. ❌ **vcpkg tools crash** - SIGILL on execution

These are NOT optional - they MUST be fixed.

---

## Your Task

Fix both critical bugs in ~4 hours:

1. **Implement proper manpage support** in tool_support library
2. **Fix vcpkg architecture optimization** causing crashes
3. **Validate** full end-to-end in all build modes
4. **Update docs** to remove "Known Limitations"

---

## Start Here

**Phase 1.1**: Understand manpage generation
```bash
# Read these files
cat cmake/render_manpage.cmake
cat cmake/manpage.cmake
cat cmake/render_manpage.py

# Find where manpages are generated
find build-* -name "*_manpage.cpp" 2>/dev/null | head -5
```

**Key Question**: How do we generate manpages during library build instead of tool build?

**Phase 2.1**: Identify SIGILL cause
```bash
# Compare compiler flags
cmake -DCMAKE_VERBOSE_MAKEFILE=ON ... 2>&1 | grep CXX_FLAGS

# Check architecture
otool -l build-vcpkg/mkdwarfs | grep -A 5 LC_BUILD_VERSION
```

**Key Question**: What optimization flag causes illegal instruction on ARM64?

---

## Success Criteria

This MUST work without errors:
```bash
# vcpkg workflow
./build-vcpkg/mkdwarfs --man | head -20  # Shows manpage ✅
./build-vcpkg/mkdwarfs -i dir -o test.dff  # No crash ✅
./build-vcpkg/dwarfsck test.dff  # Validates ✅
./build-vcpkg/dwarfsextract -i test.dff -o out  # Extracts ✅
```

---

## Key Documents

1. **Full Plan**: [`doc/SESSION_46_CONTINUATION_PLAN.md`](SESSION_46_CONTINUATION_PLAN.md)
2. **Status Tracker**: [`doc/SESSION_46_IMPLEMENTATION_STATUS.md`](SESSION_46_IMPLEMENTATION_STATUS.md)
3. **Session 45 Summary**: [`doc/TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md`](TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md)

---

## Quick Reference

**Session 45 Achievements**:
- ✅ Library builds via vcpkg
- ✅ Tools build separately
- ✅ Basic functionality works

**Session 46 Goals**:
- ✅ Fix manpage support completely
- ✅ Fix vcpkg crash bugs completely
- ✅ Remove all "Known Limitations"

---

**Timeline**: 4 hours | **Status**: Ready to start | **Priority**: CRITICAL BUGS