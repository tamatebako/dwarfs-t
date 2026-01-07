# Session 47: Quick Start Prompt

**Read This First**: [`doc/SESSION_47_CONTINUATION_PLAN.md`](SESSION_47_CONTINUATION_PLAN.md)

---

## Context

Session 46 completed manpage implementation infrastructure but left 2 **CRITICAL ITEMS** incomplete:

1. ⚠️ **Manpage testing incomplete** - Generated but not validated end-to-end
2. 🔴 **vcpkg tools crash with SIGILL** - Critical blocker, makes vcpkg builds unusable

---

## Your Task

Complete both critical items in ~4 hours:

1. **Validate manpage functionality** across all 3 build modes
2. **Fix vcpkg SIGILL crashes** on ARM64 (architecture optimization issue)
3. **Comprehensive testing** of all workflows
4. **Update documentation** to remove "Known Limitations"

---

## Start Here

**Phase 1.1**: Test manpage functionality in full build
```bash
cd /Users/mulgogi/src/external/dwarfs

# Build with manpages
cmake -B build-full -DWITH_LIBDWARFS=ON -DWITH_TOOLS=ON \
  -DWITH_MAN_OPTION=ON -DWITH_FUSE_DRIVER=ON
cmake --build build-full -j8

# Test all 4 tools (MUST display formatted manpages)
./build-full/mkdwarfs --man | head -50
./build-full/dwarfs --man | head -50
./build-full/dwarfsck --man | head -50
./build-full/dwarfsextract --man | head -50

# Verify non-empty (>100 lines)
test $(./build-full/mkdwarfs --man | wc -l) -gt 100 && echo "✅ Manpage OK"
```

**Phase 2.1**: Diagnose vcpkg SIGILL (CRITICAL)
```bash
# Try to run vcpkg-built tool (will likely crash)
./build-vcpkg-tools/mkdwarfs --help
# If exit code is 132 → SIGILL crash

# Investigate cause
cmake -B build-vcpkg-debug -S tools \
  -DCMAKE_VERBOSE_MAKEFILE=ON \
  -DCMAKE_PREFIX_PATH=<vcpkg_path> \
  2>&1 | grep -E "(march|mcpu|mtune)" > vcpkg-flags.log

# Compare with working build
cmake -B build-sys -DWITH_TOOLS=ON \
  -DCMAKE_VERBOSE_MAKEFILE=ON \
  2>&1 | grep -E "(march|mcpu|mtune)" > sys-flags.log

diff vcpkg-flags.log sys-flags.log
```

**Key Question**: What compiler flag difference causes illegal instruction?

---

## Success Criteria

This MUST work without errors:
```bash
# Manpage test
./build-vcpkg-tools/mkdwarfs --man | head -20  # Shows manpage ✅

# Execution test (CRITICAL - must not crash)
./build-vcpkg-tools/mkdwarfs -i /usr/share/dict -o test.dff -l1
echo $?  # MUST be 0, not 132 ✅

# Full workflow
./build-vcpkg-tools/dwarfsck test.dff  # Validates ✅
./build-vcpkg-tools/dwarfsextract -i test.dff -o out  # Extracts ✅
diff -r /usr/share/dict out  # Exit 0 ✅
```

---

## Key Documents

1. **Full Plan**: [`doc/SESSION_47_CONTINUATION_PLAN.md`](SESSION_47_CONTINUATION_PLAN.md) - Complete 4-phase approach
2. **Status Tracker**: [`doc/SESSION_47_IMPLEMENTATION_STATUS.md`](SESSION_47_IMPLEMENTATION_STATUS.md) - Task checklist
3. **Session 46 Summary**: [`doc/TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md`](TOOL_SUPPORT_LIBRARY_IMPLEMENTATION.md) - What was done

---

## Quick Reference

**Session 46 Achievements**:
- ✅ Manpages generate in tool_support library
- ✅ FUSE compilation fixed
- ✅ need_fuse.cmake duplicate target fixed
- ✅ tools/CMakeLists.txt updated

**Session 47 Goals**:
- ✅ Test manpages in all 3 build modes
- ✅ Fix vcpkg SIGILL (conservative ARM64 flags likely solution)
- ✅ Comprehensive validation
- ✅ Remove all "Known Limitations" from docs

---

**Timeline**: 4 hours | **Priority**: CRITICAL BUGS | **Status**: Ready to start