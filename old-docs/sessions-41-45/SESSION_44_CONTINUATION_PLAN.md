# Session 44: Tool Support Library - Root Cause Fix

**Created**: 2025-12-27
**Previous Session**: Session 43 (defined library but not building)
**Status**: Root cause identified, clean solution required
**Estimated Time**: 3 hours

---

## Problem Analysis

### What Session 43 Achieved ✅
1. Created `dwarfs_tool_support` library target in `cmake/libdwarfs.cmake`
2. Target successfully appears in CMake help output
3. Headers installation configured
4. Export to dwarfs-config.cmake working

### Root Cause Identified 🔍
**Issue**: Library defined but NOT building because:
1. Source files are in `tools/src/tool/` and `tools/src/{mkdwarfs,dwarfs}/`
2. These directories are part of the tools build tree
3. vcpkg has `-DWITH_TOOLS=ON` but library isn't being compiled
4. Library target exists but compilation isn't triggered

**Architectural Problem**: Tool support files are scattered across tool-specific directories instead of being organized as a proper library module.

---

## Clean Solution: Modular CMake Architecture

### Principle: Separation of Concerns
Following DwarFS architecture patterns (filesystem_loader, fuse_driver), we need:

1. **Separate CMake Module**: `cmake/tool_support.cmake`
2. **Proper Source Organization**: All tool utilities in one place
3. **Independent Build**: Library builds with `-DWITH_LIBDWARFS=ON` regardless of `-DWITH_TOOLS`
4. **Clean vcpkg Integration**: Reproducible, testable builds

### Proposed Structure

```
dwarfs/
├── cmake/
│   ├── libdwarfs.cmake         # Core libraries
│   └── tool_support.cmake      # Tool support library (NEW - separate module)
├── tools/
│   ├── src/
│   │   ├── tool/               # Tool utilities (library source)
│   │   ├── mkdwarfs/           # mkdwarfs handlers (library source)
│   │   ├── dwarfs/             # dwarfs handlers (library source)
│   │   ├── mkdwarfs_main.cpp   # Executable entry point
│   │   ├── dwarfs_main.cpp     # Executable entry point
│   │   └── ...
│   └── include/dwarfs/tool/    # Public headers
└── vcpkg_ports/dwarfs/
    └── portfile.cmake          # Clean, minimal port
```

---

## Implementation Plan

### Phase 1: Create Modular cmake/tool_support.cmake (1 hour)

**1.1 Extract to Separate Module** (20 min)
- [ ] Create `cmake/tool_support.cmake`
- [ ] Move `dwarfs_tool_support` definition from `cmake/libdwarfs.cmake`
- [ ] Include module in main CMakeLists.txt
- [ ] Verify no circular dependencies

**1.2 Fix Source File Paths** (20 min)
- [ ] Use `${CMAKE_SOURCE_DIR}/tools/src/tool/*.cpp` (absolute paths)
- [ ] Use `${CMAKE_SOURCE_DIR}/tools/src/mkdwarfs/*.cpp`
- [ ] Use `${CMAKE_SOURCE_DIR}/tools/src/dwarfs/*.cpp` (conditional)
- [ ] Verify paths work in both main build and vcpkg build

**1.3 Configure Dependencies** (20 min)
- [ ] PUBLIC: dwarfs_common, Boost::program_options
- [ ] PUBLIC: dwarfs_reader, dwarfs_writer (for handlers)
- [ ] CONDITIONAL: dwarfs_rewrite (if Thrift enabled)
- [ ] CONDITIONAL: FUSE libraries (if enabled)

### Phase 2: Clean vcpkg Integration (45 min)

**2.1 Simplify Portfile** (15 min)
- [ ] Verify `-DWITH_LIBDWARFS=ON` is sufficient
- [ ] Remove redundant header installs (handled by CMake)
- [ ] Add comment explaining tool_support library
- [ ] Test reproducibility

**2.2 Test Vcpkg Build** (30 min)
- [ ] Clean: `rm -rf vcpkg_installed build-vcpkg`
- [ ] Install: `vcpkg install dwarfs:arm64-osx-static --overlay-ports=vcpkg_ports`
- [ ] Verify: `ls vcpkg_installed/*/lib/libdwarfs_tool_support.a`
- [ ] Check headers: `ls vcpkg_installed/*/include/dwarfs/tool/`

### Phase 3: Tools Build & Integration (45 min)

**3.1 Update tools/CMakeLists.txt** (15 min)
- [ ] Verify `find_package(dwarfs CONFIG REQUIRED)` finds tool_support
- [ ] Confirm link line: `dwarfs::dwarfs_tool_support` only (transitive deps)
- [ ] Remove any duplicate source includes

**3.2 Build Tools Separately** (20 min)
- [ ] Configure: `cmake -B build-tools -S tools -DCMAKE_TOOLCHAIN_FILE=...`
- [ ] Build: `cmake --build build-tools`
- [ ] Verify: `./build-tools/mkdwarfs --version`

**3.3 Integration Testing** (10 min)
- [ ] Create test filesystem
- [ ] Verify with dwarfsck
- [ ] Extract with dwarfsextract
- [ ] Compare roundtrip integrity

### Phase 4: Documentation & Cleanup (30 min)

**4.1 Update Official Docs** (20 min)
- [ ] README.md: Add "Building Tools Separately" section
- [ ] doc/vcpkg-integration.md: Document library usage
- [ ] Architecture diagram: Show tool_support layer

**4.2 Move Temporary Docs** (10 min)
```bash
mkdir -p old-docs/sessions-41-43
mv doc/SESSION_41_*.md old-docs/sessions-41-43/
mv doc/SESSION_42_*.md old-docs/sessions-41-43/
mv doc/SESSION_43_*.md old-docs/sessions-41-43/
```

---

## Key Architectural Principles

### 1. Modular CMake Design
Each library should have its own CMake module:
- `cmake/libdwarfs.cmake` - Core 5 libraries
- `cmake/tool_support.cmake` - Tool utilities library
- `cmake/metadata_serialization.cmake` - Format configuration
- `cmake/tebako.cmake` - Tebako integration

### 2. Source File Organization
Tool support sources must be:
- In `tools/src/tool/` (core utilities)
- In `tools/src/{tool_name}/` (tool-specific handlers)
- Accessed via absolute paths: `${CMAKE_SOURCE_DIR}/tools/src/...`

### 3. Clean Dependency Chain
```
CLI Tools
    ↓
dwarfs_tool_support (handlers, utilities)
    ↓
dwarfs_{reader,writer,extractor} (operations)
    ↓
dwarfs_common (foundation)
```

### 4. vcpkg Reproducibility
Portfile must:
- Use minimal, explicit CMake options
- Let CMake handle all installation
- Be testable in isolation
- Work across all platforms

---

## Success Criteria

### Must Have ✅
1. `libdwarfs_tool_support.a` appears in vcpkg install
2. All tool headers installed correctly
3. Tools build successfully via `cmake -S tools`
4. Tools run and produce correct output
5. Main build unaffected
6. example/static-site-server still works

### Build Validation
```bash
# 1. vcpkg install
vcpkg remove dwarfs:arm64-osx-static
vcpkg install dwarfs:arm64-osx-static --overlay-ports=vcpkg_ports

# 2. Verify library
ls vcpkg_installed/arm64-osx-static/lib/libdwarfs_tool_support.a

# 3. Build tools
cmake -B build-tools -S tools \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static

cmake --build build-tools

# 4. Test
./build-tools/mkdwarfs --help
./build-tools/mkdwarfs -i test-input -o test.dff -l1
./build-tools/dwarfsck test.dff
./build-tools/dwarfsextract -i test.dff -o test-output
diff -r test-input test-output
```

---

## Files to Modify

### Create
- [ ] `cmake/tool_support.cmake` - Modular tool support library definition

### Modify
- [ ] `cmake/libdwarfs.cmake` - Remove dwarfs_tool_support, add include(tool_support)
- [ ] `CMakeLists.txt` - Include tool_support.cmake at correct point
- [ ] `tools/CMakeLists.txt` - Simplified linking (already done)
- [ ] `vcpkg_ports/dwarfs/portfile.cmake` - Clean up redundant installs

### Document
- [ ] `README.md` - Building tools separately section
- [ ] `doc/vcpkg-integration.md` - Full integration guide

### Move to old-docs/sessions-41-43/
- [ ] `doc/SESSION_41_*.md`
- [ ] `doc/SESSION_42_*.md`
- [ ] `doc/SESSION_43_*.md`

---

## Timeline

- **Phase 1**: 1 hour (modular cmake)
- **Phase 2**: 45 minutes (vcpkg integration)
- **Phase 3**: 45 minutes (tools build & test)
- **Phase 4**: 30 minutes (documentation)

**Total**: 3 hours (focused, clean implementation)

---

## Next Session Start

1. Read this continuation plan
2. Read [`doc/SESSION_44_IMPLEMENTATION_STATUS.md`](SESSION_44_IMPLEMENTATION_STATUS.md)
3. Start with Phase 1.1: Create `cmake/tool_support.cmake`

**Key Focus**: Modular, clean, reproducible CMake architecture