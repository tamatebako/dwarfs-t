# Session 43: Tool Support Library Implementation - Start Here

**Date**: 2025-12-27
**Previous Session**: Session 42 (discovered blocker)
**Estimated Time**: 4 hours
**Status**: Ready to start

---

## Quick Context

You are continuing work on the Tebako fork of DwarFS. Session 42 discovered that CLI tools cannot be built separately via vcpkg because they depend on ~24 implementation files in `tools/src/tool/` that aren't included in the installed libraries.

**Solution**: Create `libdwarfs_tool_support` library containing all tool utilities.

---

## Your Task

Implement the tool support library so CLI tools can be built separately from the main project.

---

## Before You Start

### Read These Files First (in order):
1. [`doc/SESSION_43_CONTINUATION_PLAN.md`](SESSION_43_CONTINUATION_PLAN.md) - Full implementation plan
2. [`doc/SESSION_43_IMPLEMENTATION_STATUS.md`](SESSION_43_IMPLEMENTATION_STATUS.md) - Task checklist
3. [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md) - System architecture

### Verify Current State:
```bash
# Should exist from Session 42:
ls -la tools/CMakeLists.txt
ls -la tools/vcpkg.json
ls -la vcpkg_ports/dwarfs/portfile.cmake

# Tool source files (24 files):
find tools/src/tool -name "*.cpp" | wc -l  # Should be ~9-10
find tools/src/mkdwarfs -name "*.cpp" | wc -l  # Should be ~4
find tools/src/dwarfs -name "*.cpp" | wc -l  # Should be ~2
```

---

## Implementation Steps

### Phase 1: Create Tool Support Library (2 hours) ⏰ START HERE

**1.1 Define CMake Target** (30 min):

Create or update `cmake/tool_support.cmake`:
```cmake
# DwarFS Tool Support Library
# Contains CLI tool utilities (main_adapter, iolayer, etc.)

add_library(dwarfs_tool_support STATIC
  # Main adapter and safe entry point
  ${CMAKE_SOURCE_DIR}/tools/src/tool/main_adapter.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/safe_main.cpp

  # System utilities
  ${CMAKE_SOURCE_DIR}/tools/src/tool/iolayer.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/sys_char.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/sysinfo.cpp

  # Tool utilities
  ${CMAKE_SOURCE_DIR}/tools/src/tool/tool.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/pager.cpp
  ${CMAKE_SOURCE_DIR}/tools/src/tool/render_manpage.cpp

  # Add other files from find tools/src/tool -name "*.cpp"
)

target_include_directories(dwarfs_tool_support
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tools/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dwarfs_tool_support
  PUBLIC
    dwarfs_common
    Boost::program_options
    Boost::process
)

# Install library
install(TARGETS dwarfs_tool_support
  EXPORT dwarfs-targets
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
)

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/tools/include/dwarfs
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/tools/include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
  PATTERN "dwarfs" EXCLUDE
)
```

Then include in `cmake/libdwarfs.cmake`:
```cmake
# Add after other libraries
if(WITH_LIBDWARFS)
  include(tool_support)
endif()
```

**1.2-1.5**: Follow plan in SESSION_43_CONTINUATION_PLAN.md

---

### Phase 2: Update Tools Build (1 hour)

Update `tools/CMakeLists.txt` to link against `dwarfs::dwarfs_tool_support`:

```cmake
target_link_libraries(mkdwarfs PRIVATE
  dwarfs::dwarfs_common
  dwarfs::dwarfs_writer
  dwarfs::dwarfs_tool_support  # ADD THIS
)

# Same for dwarfsck and dwarfsextract
```

---

### Phase 3: Integration Testing (30 min)

```bash
# Clean build
rm -rf build-* vcpkg_installed

# Rebuild vcpkg
vcpkg install dwarfs --overlay-ports=vcpkg_ports

# Build tools
cmake -B build-vcpkg -S tools \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static

cmake --build build-vcpkg

# Test
./build-vcpkg/mkdwarfs --help
./build-vcpkg/dwarfsck --help
./build-vcpkg/dwarfsextract --help
```

---

### Phase 4: Documentation (30 min)

1. Update README.md with vcpkg build section
2. Create doc/vcpkg-build-guide.md
3. Move temporary docs:
```bash
mkdir -p old-docs/session-42
mv doc/SESSION_41_*.md old-docs/session-42/
mv doc/SESSION_42_*.md old-docs/session-42/
```

---

## Key Files to Modify

**Create**:
- `cmake/tool_support.cmake` - Tool support library definition

**Modify**:
- `cmake/libdwarfs.cmake` - Include tool_support.cmake
- `cmake/dwarfs-config.cmake.in` - Export dwarfs_tool_support target
- `vcpkg_ports/dwarfs/portfile.cmake` - Build and install library
- `tools/CMakeLists.txt` - Link to tool support library
- `README.md` - Document vcpkg build mode

**Move to old-docs/session-42/**:
- `doc/SESSION_41_*.md`
- `doc/SESSION_42_*.md`

---

## Success Criteria

All must pass ✅:
1. mkdwarfs, dwarfsck, dwarfsextract build via vcpkg
2. Tools run correctly (test with --help and basic operations)
3. Main build still works: `cmake -B build && cmake --build build`
4. example/static-site-server still works: `cd example/static-site-server && ./build.sh`
5. Documentation updated

---

## If You Get Stuck

**Common Issues**:

1. **"Undefined symbols"** → Missing source files in tool_support library, add them
2. **"Header not found"** → Check install commands in cmake/tool_support.cmake
3. **"Target not found"** → Check export in dwarfs-config.cmake.in
4. **vcpkg install fails** → Check portfile.cmake build commands

**Debugging**:
```bash
# Check what's installed
ls -la /path/to/vcpkg/packages/dwarfs_*/lib/
ls -la /path/to/vcpkg/packages/dwarfs_*/include/

# Check what's being linked
cmake --build build-vcpkg --verbose | grep "link"
```

---

## Timeline

- **Phase 1**: 2 hours (library creation) ⏰
- **Phase 2**: 1 hour (tools update)
- **Phase 3**: 30 minutes (testing)
- **Phase 4**: 30 minutes (documentation)

**Total**: 4 hours (stay on schedule!)

---

## References

- **Main Plan**: [`SESSION_43_CONTINUATION_PLAN.md`](SESSION_43_CONTINUATION_PLAN.md)
- **Status Tracker**: [`SESSION_43_IMPLEMENTATION_STATUS.md`](SESSION_43_IMPLEMENTATION_STATUS.md)
- **Architecture**: [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md)
- **Session 42 Summary**: [`SESSION_42_CONTINUATION_PROMPT.md`](SESSION_42_CONTINUATION_PROMPT.md)

---

**Ready? Start with Phase 1.1: Create cmake/tool_support.cmake**

Mark tasks complete in SESSION_43_IMPLEMENTATION_STATUS.md as you go.