# Session 43: Tool Support Library Implementation - Continuation Plan

**Created**: 2025-12-27
**Previous Session**: Session 42 - Discovered architectural blocker
**Status**: Ready to start

---

## Problem Summary

**Issue**: CLI tools cannot be built separately via vcpkg because they depend on ~24 implementation files in `tools/src/tool/` that provide essential utilities not included in libdwarfs libraries.

**Root Cause**: No separation between library code and tool support code.

**Solution**: Create `libdwarfs_tool_support` library containing all tool-specific utilities.

---

## Architectural Design

### Proposed Structure

```
dwarfs/
├── include/dwarfs/         # Library headers (PUBLIC)
│   ├── reader/
│   ├── writer/
│   └── utility/
├── include/dwarfs/tool/    # Tool support headers (PUBLIC)
│   ├── main_adapter.h
│   ├── iolayer.h
│   ├── sys_char.h
│   └── ...
├── src/                    # Library implementation
│   ├── reader/
│   ├── writer/
│   └── ...
├── src/tool/              # Tool support implementation
│   ├── main_adapter.cpp
│   ├── iolayer.cpp
│   ├── sys_char.cpp
│   └── ...
└── tools/                 # CLI tool entry points only
    └── src/
        ├── mkdwarfs.cpp      (thin wrapper)
        ├── mkdwarfs_main.cpp (main logic)
        └── ...
```

### Library Organization

**Current** (5 libraries):
1. `dwarfs_common` - Core utilities
2. `dwarfs_reader` - Filesystem reading
3. `dwarfs_writer` - Filesystem writing
4. `dwarfs_extractor` - Filesystem extraction
5. `dwarfs_rewrite` - Filesystem recompression

**Proposed** (6 libraries):
1-5. (Same as above)
6. **`dwarfs_tool_support`** - CLI tool utilities (NEW)

### dwarfs_tool_support Contents

**Source Files** (~24 files in `tools/src/tool/`):
- `main_adapter.cpp` - Safe main() entry point
- `iolayer.cpp` - Cross-platform I/O abstraction
- `sys_char.cpp` - Platform string encoding
- `sysinfo.cpp` - System information (memory, CPU)
- `tool.cpp` - Common tool utilities
- `pager.cpp` - Manpage pager
- `render_manpage.cpp` - Manpage rendering
- `safe_main.cpp` - Exception handling wrapper
- Plus tool-specific handlers (mkdwarfs/*, dwarfs/*)

**Dependencies**:
- `dwarfs_common` (required)
- `Boost::program_options` (required)
- `Boost::process` (required)

---

## Implementation Plan

### Phase 1: Create Tool Support Library (2 hours)

**1.1 Define CMake Target** (30 min)
- [ ] Add `dwarfs_tool_support` target in `cmake/libdwarfs.cmake`
- [ ] Set source files from `tools/src/tool/*.cpp`
- [ ] Set include directories
- [ ] Link dependencies (dwarfs_common, Boost)
- [ ] Mark as OBJECT library for static linking

**1.2 Install Headers** (15 min)
- [ ] Install `tools/include/dwarfs/tool/*.h` to `include/dwarfs/tool/`
- [ ] Install `tools/include/*.h` to `include/`
- [ ] Update install commands in CMakeLists.txt

**1.3 Export CMake Targets** (15 min)
- [ ] Add `dwarfs_tool_support` to `dwarfs-config.cmake.in`
- [ ] Create `dwarfs::dwarfs_tool_support` target alias
- [ ] Test find_package() detection

**1.4 Update vcpkg Port** (30 min)
- [ ] Build `dwarfs_tool_support` in portfile
- [ ] Install library to `lib/libdwarfs_tool_support.a`
- [ ] Install all tool headers
- [ ] Update vcpkg.json dependencies

**1.5 Test Library Build** (30 min)
- [ ] Build with main CMake: `cmake -B build -DWITH_LIBDWARFS=ON`
- [ ] Verify `libdwarfs_tool_support.a` created
- [ ] Verify headers installed correctly
- [ ] Test vcpkg installation

### Phase 2: Update Tools Build (1 hour)

**2.1 Update tools/CMakeLists.txt** (30 min)
- [ ] Add `find_package(dwarfs CONFIG REQUIRED)` requirement
- [ ] Link all tools to `dwarfs::dwarfs_tool_support`
- [ ] Remove duplicate source file includes
- [ ] Verify dependencies are transitive

**2.2 Verify Clean Build** (30 min)
- [ ] Clear caches: `rm -rf build-* vcpkg_installed`
- [ ] Rebuild vcpkg: `vcpkg install dwarfs`
- [ ] Build tools: `cmake -B build-vcpkg -S tools && cmake --build build-vcpkg`
- [ ] Test all 3 tools: mkdwarfs --help, dwarfsck --help, dwarfsextract --help

### Phase 3: Integration Testing (30 min)

**3.1 Test Workflows**
- [ ] Create filesystem: `mkdwarfs -i input -o test.dff -l3`
- [ ] Verify filesystem: `dwarfsck test.dff`
- [ ] Extract filesystem: `dwarfsextract -i test.dff -o output`
- [ ] Compare input vs output: `diff -r input output`

**3.2 Test Main Build Unaffected**
- [ ] Build main project: `cmake -B build && cmake --build build`
- [ ] Run tests: `ctest --test-dir build`
- [ ] Verify no regressions

**3.3 Test example/static-site-server**
- [ ] Rebuild: `cd example/static-site-server && ./build.sh`
- [ ] Verify still works

### Phase 4: Documentation (30 min)

**4.1 Update README.md**
- [ ] Document two build modes: main vs vcpkg
- [ ] Add "Building Tools Separately" section
- [ ] Link to detailed guide

**4.2 Create doc/vcpkg-build-guide.md**
- [ ] Prerequisites
- [ ] Installation steps
- [ ] Usage examples
- [ ] Troubleshooting

**4.3 Move Temporary Docs**
```bash
mkdir -p old-docs/session-42
mv doc/SESSION_41_*.md old-docs/session-42/
mv doc/SESSION_42_*.md old-docs/session-42/
# Keep SESSION_43_* active
```

---

## Success Criteria

### Must Have ✅
1. All 3 tools build successfully via vcpkg
2. Tools run and produce correct output
3. Main build still works
4. example/static-site-server still works
5. Documentation updated

### Nice to Have 🎯
1. FUSE driver working (fix compile flags)
2. Benchmark script created
3. Performance comparison data

---

## Estimated Timeline

- **Phase 1**: 2 hours (library creation)
- **Phase 2**: 1 hour (tools update)
- **Phase 3**: 30 minutes (testing)
- **Phase 4**: 30 minutes (documentation)

**Total**: 4 hours (compressed schedule)

---

## Rollback Plan

If implementation fails:
1. Revert vcpkg_ports/dwarfs/portfile.cmake
2. Revert tools/CMakeLists.txt
3. Revert cmake/libdwarfs.cmake
4. Document issue for future session

---

## Next Session Start

Read: [`doc/SESSION_43_CONTINUATION_PROMPT.md`](SESSION_43_CONTINUATION_PROMPT.md)

Start with Phase 1.1: Define CMake Target