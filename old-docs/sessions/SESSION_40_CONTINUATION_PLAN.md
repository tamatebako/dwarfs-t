# Session 40: Continuation Plan - Fix parallel-hashmap vcpkg Integration

**Previous Session**: Session 39 - jemalloc vcpkg Resolution (COMPLETE)
**Status**: READY TO START
**Estimated Time**: 30-60 minutes
**Priority**: HIGH - Complete --build-deps functionality

---

## Context

Session 39 successfully fixed the jemalloc issue. The build now progresses to DwarFS configuration but fails on parallel-hashmap CMake target resolution:

```
CMake Error:
  Target "phmap" not found.
```

**Root Cause**: vcpkg's parallel-hashmap 2.0.0 is header-only without CMake targets, but DwarFS expects a `phmap` target.

---

## Objectives

1. Create cmake/vcpkg/parallel-hashmap.cmake to provide phmap INTERFACE target
2. Complete DwarFS build in _deps/
3. Build and test static-site-server example
4. Document and commit

---

## Implementation Plan

### Phase 1: Fix parallel-hashmap Integration (15 min)

**Task 1.1**: Create cmake/vcpkg/parallel-hashmap.cmake
```cmake
# parallel-hashmap dependency configuration
# Header-only library for high-performance hash maps

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: parallel-hashmap is header-only without CMake targets
  # Create phmap INTERFACE target from find_path
  
  find_path(PARALLEL_HASHMAP_INCLUDE_DIR
    NAMES parallel_hashmap/phmap.h
    REQUIRED
  )
  
  if(NOT TARGET phmap)
    add_library(phmap INTERFACE IMPORTED)
    set_target_properties(phmap PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${PARALLEL_HASHMAP_INCLUDE_DIR}"
    )
  endif()
  
  message(STATUS "Using parallel-hashmap from vcpkg (header-only): ${PARALLEL_HASHMAP_INCLUDE_DIR}")
else()
  # Non-vcpkg mode: try system or FetchContent
  find_path(PARALLEL_HASHMAP_INCLUDE_DIR
    NAMES parallel_hashmap/phmap.h
  )
  
  if(PARALLEL_HASHMAP_INCLUDE_DIR)
    if(NOT TARGET phmap)
      add_library(phmap INTERFACE IMPORTED)
      set_target_properties(phmap PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PARALLEL_HASHMAP_INCLUDE_DIR}"
      )
    endif()
    message(STATUS "Using system parallel-hashmap: ${PARALLEL_HASHMAP_INCLUDE_DIR}")
  else()
    # Fall back to FetchContent
    message(STATUS "parallel-hashmap not found, using FetchContent")
    include(FetchContent)
    
    FetchContent_Declare(
      parallel-hashmap
      GIT_REPOSITORY https://github.com/greg7mdp/parallel-hashmap.git
      GIT_TAG v2.0.0
    )
    
    FetchContent_MakeAvailable(parallel-hashmap)
    
    if(NOT TARGET phmap)
      add_library(phmap INTERFACE IMPORTED)
      set_target_properties(phmap PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${parallel-hashmap_SOURCE_DIR}"
      )
    endif()
  endif()
endif()
```

**Task 1.2**: Include in main CMakeLists.txt
Check if `cmake/vcpkg/parallel-hashmap.cmake` is already included, if not add it.

### Phase 2: Test Complete Build (30 min)

**Task 2.1**: Clean and rebuild
```bash
cd example/static-site-server
rm -rf _deps/ build/
./build.sh --build-deps 2>&1 | tee build-complete.log
```

**Task 2.2**: Verify build stages
- ✅ vcpkg installs all packages (14/14)
- ✅ jemalloc builds successfully  
- ✅ parallel-hashmap resolves via new cmake file
- ✅ DwarFS configures successfully
- ✅ DwarFS compiles (all tools)
- ✅ DwarFS installs to _deps/dwarfs-install/
- ✅ Example configures successfully
- ✅ Example compiles successfully

### Phase 3: Runtime Test (5 min)

**Task 3.1**: Test static-site-server
```bash
cd example/static-site-server
./build/static-site-server --image *.dff --port 8080 &
SERVER_PID=$!
sleep 2
curl http://localhost:8080/
kill $SERVER_PID
```

### Phase 4: Documentation & Commit (15 min)

**Task 4.1**: Create completion materials
- doc/SESSION_39_COMPLETION_SUMMARY.md (final)
- doc/SESSION_40_COMPLETION_SUMMARY.md
- doc/SESSION_40_GIT_COMMIT_MESSAGE.txt

**Task 4.2**: Commit changes
```bash
git add cmake/vcpkg/parallel-hashmap.cmake \
  doc/SESSION_39_COMPLETION_SUMMARY.md \
  doc/SESSION_40_COMPLETION_SUMMARY.md \
  doc/SESSION_40_GIT_COMMIT_MESSAGE.txt
git commit -F doc/SESSION_40_GIT_COMMIT_MESSAGE.txt
```

---

## Success Criteria

- [ ] cmake/vcpkg/parallel-hashmap.cmake created
- [ ] phmap target resolves in vcpkg mode
- [ ] DwarFS builds completely in _deps/
- [ ] DwarFS tools installed to _deps/dwarfs-install/
- [ ] Example server builds successfully
- [ ] Server runs and serves content
- [ ] All from one command: `./build.sh --build-deps`

---

## Expected Issues

### Issue 1: Wrong include path
**If**: parallel_hashmap/phmap.h not found
**Fix**: Check actual vcpkg install location, adjust find_path

### Issue 2: Target already exists
**If**: phmap target conflicts
**Fix**: Use `if(NOT TARGET phmap)` guard

---

## Files to Create/Modify

**New**:
- cmake/vcpkg/parallel-hashmap.cmake

**Update**:
- doc/SESSION_39_COMPLETION_SUMMARY.md (finalize)
- doc/SESSION_40_COMPLETION_SUMMARY.md (create)
- doc/SESSION_40_GIT_COMMIT_MESSAGE.txt (create)

---

## Quick Start

1. Read this plan
2. Create cmake/vcpkg/parallel-hashmap.cmake 
3. Test build
4. Document and commit

---

**Created**: 2025-12-27
**Session**: 40 (parallel-hashmap + complete build)
**Previous**: Session 39 (jemalloc resolution - COMPLETE)
**Estimated Duration**: 30-60 minutes
