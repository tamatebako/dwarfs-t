# Session 31L Continuation Plan: Complete Backend File Cleanup

**Date**: 2025-12-23
**Previous Session**: 31K - Timestamp implementation complete
**Objective**: Remove old backend files safely after proper verification
**Estimated Duration**: 2 hours

## Executive Summary

Session 31K implemented timestamp handling successfully. Now we need to:
1. Identify which files are truly obsolete
2. Remove them from CMake build system FIRST
3. Delete the actual files
4. Verify builds work correctly
5. Update documentation

## Current State Analysis

### Files Still Being Compiled (KEEP)
From [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake):
- ✅ `metadata_types_flatbuffers.cpp` (line 151) - **STILL COMPILED** conditionally
- ✅ `metadata_types_thrift.cpp` (line 150) - **STILL COMPILED** conditionally
- ✅ `metadata_v2.cpp` (line 155) - **ACTIVE** constructor/factory
- ✅ `metadata_v2_factory.cpp` (line 166) - **ACTIVE** in dual-format builds
- ✅ `common_metadata_operations.cpp` (line 158) - **NEW** domain-based impl
- ✅ `domain_metadata_views.cpp` (line 159) - **NEW** view wrappers
- ✅ `flatbuffers_metadata_adapter.cpp` (line 163) - **NEW** adapter
- ✅ `thrift_metadata_adapter.cpp` (line 162) - **NEW** adapter

### Files NOT Being Compiled (DELETE)

**Large Backend Implementation Files** (165KB total):
1. `src/reader/internal/metadata_v2_flatbuffers.cpp` (83,632 bytes)
   - Referenced in set_source_files_properties (line 182) but NOT in source list
   - Replaced by `flatbuffers_metadata_adapter.cpp`
   - **DELETE**: Safe to remove

2. `src/reader/internal/metadata_v2_thrift.cpp` (81,800 bytes)
   - Not referenced anywhere in cmake
   - Replaced by `thrift_metadata_adapter.cpp`
   - **DELETE**: Safe to remove

**Legacy Split Files** (79KB total):
3. `src/reader/internal/metadata_v2_thrift_part1.cpp` (48,090 bytes)
4. `src/reader/internal/metadata_v2_thrift_part2.cpp` (28,307 bytes)
5. `src/reader/internal/metadata_v2_thrift_getters.cpp` (3,317 bytes)
   - Old split files from before refactoring
   - Never referenced in cmake
   - **DELETE**: Safe to remove

**Obsolete Factory** (2.6KB):
6. `src/reader/internal/metadata_v2_flatbuffers_factory.cpp` (2,603 bytes)
   - Replaced by `flatbuffers_metadata_adapter.cpp`
   - Not in cmake source list
   - **DELETE**: Safe to remove

**Upstream Copy** (84KB):
7. `src/reader/internal/metadata_v2_thrift_upstream.cpp` (84,238 bytes)
   - Copy of original upstream implementation
   - Never compiled
   - **DELETE**: Safe to remove (keep in backup)

**Total to Delete**: ~337KB, **7 files**

### Incorrect CMake Reference (FIX)

Line 182-183 in cmake/lib

dwarfs.cmake:
```cmake
set_source_files_properties(
  src/reader/internal/metadata_types_flatbuffers.cpp
  src/reader/internal/metadata_v2_flatbuffers.cpp  # <-- NOT COMPILED, REMOVE
  PROPERTIES
  COMPILE_OPTIONS "-DDWARFS_FLATBUFFERS_BACKEND"
)
```

## Phase 1: Remove from CMake (20 min)

### Step 1.1: Clean up set_source_files_properties

**File**: `cmake/libdwarfs.cmake:179-186`

**Current**:
```cmake
# In dual-format mode, ensure FlatBuffers implementation uses FlatBuffers types
if(DWARFS_HAVE_THRIFT AND DWARFS_HAVE_FLATBUFFERS)
  set_source_files_properties(
    src/reader/internal/metadata_types_flatbuffers.cpp
    src/reader/internal/metadata_v2_flatbuffers.cpp
    PROPERTIES
    COMPILE_OPTIONS "-DDWARFS_FLATBUFFERS_BACKEND"
  )
endif()
```

**Change to**:
```cmake
# In dual-format mode, ensure FlatBuffers types use correct namespace
if(DWARFS_HAVE_THRIFT AND DWARFS_HAVE_FLATBUFFERS)
  set_source_files_properties(
    src/reader/internal/metadata_types_flatbuffers.cpp
    PROPERTIES
    COMPILE_OPTIONS "-DDWARFS_FLATBUFFERS_BACKEND"
  )
endif()
```

### Step 1.2: Verify No Other References

```bash
grep -r "metadata_v2_flatbuffers\|metadata_v2_thrift" cmake/ --include="*.cmake"
grep -r "metadata_v2_flatbuffers\|metadata_v2_thrift" CMakeLists.txt
```

Expected: Only factory function names, no file references.

### Step 1.3: Test Build After CMake Changes

```bash
rm -rf build-verify
cmake -B build-verify -DCMAKE_BUILD_TYPE=Release -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
cmake --build build-verify --target mkdwarfs dwarfsck -j8
```

**Success Criteria**: Build completes without errors.

## Phase 2: Move Files to Backup (10 min)

**Important**: Move, don't delete immediately. Keep backup for reference.

```bash
# Create backup directory
mkdir -p .backup/session31-obsolete-backend

# Move old backend implementations
mv src/reader/internal/metadata_v2_flatbuffers.cpp .backup/session31-obsolete-backend/
mv src/reader/internal/metadata_v2_thrift.cpp .backup/session31-obsolete-backend/

# Move legacy split files
mv src/reader/internal/metadata_v2_thrift_part1.cpp .backup/session31-obsolete-backend/
mv src/reader/internal/metadata_v2_thrift_part2.cpp .backup/session31-obsolete-backend/
mv src/reader/internal/metadata_v2_thrift_getters.cpp .backup/session31-obsolete-backend/

# Move obsolete factory
mv src/reader/internal/metadata_v2_flatbuffers_factory.cpp .backup/session31-obsolete-backend/

# Move upstream copy
mv src/reader/internal/metadata_v2_thrift_upstream.cpp .backup/session31-obsolete-backend/
```

## Phase 3: Verify Builds Work (30 min)

### Test 1: FlatBuffers-Only Build
```bash
rm -rf build-fb-only
cmake -B build-fb-only -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
cmake --build build-fb-only --target all -j8
./build-fb-only/mkdwarfs -i example/pg11339-h -o /tmp/test-fb.dff
./build-fb-only/dwarfsck /tmp/test-fb.dff
```

### Test 2: Both Formats Build
```bash
rm -rf build-both
cmake -B build-both -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
cmake --build build-both --target all -j8
./build-both/mkdwarfs -i example/pg11339-h -o /tmp/test-both.dff
./build-both/dwarfsck /tmp/test-both.dff
```

### Test 3: Thrift-Only Build (If Supported)
```bash
rm -rf build-thrift-only
cmake -B build-thrift-only -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
# Expected: May fail config (FlatBuffers required)
```

**Success Criteria**:
- ✅ FlatBuffers-only: Builds and runs
- ✅ Both formats: Builds and runs
- ⚠️ Thrift-only: May not be supported anymore

## Phase 4: Git Commit (10 min)

```bash
# Stage cmake changes
git add cmake/libdwarfs.cmake

# Stage moved files (shows as deletions)
git add src/reader/internal/metadata_v2_flatbuffers.cpp
git add src/reader/internal/metadata_v2_thrift.cpp
git add src/reader/internal/metadata_v2_thrift_part*.cpp
git add src/reader/internal/metadata_v2_thrift_getters.cpp
git add src/reader/internal/metadata_v2_flatbuffers_factory.cpp
git add src/reader/internal/metadata_v2_thrift_upstream.cpp

# Commit
git commit -m "refactor(metadata): remove obsolete backend implementation files

Removed 7 obsolete files (337KB total):
- metadata_v2_flatbuffers.cpp (83KB)
- metadata_v2_thrift.cpp (81KB)
- metadata_v2_thrift_part1.cpp (48KB)
- metadata_v2_thrift_part2.cpp (28KB)
- metadata_v2_thrift_getters.cpp (3KB)
- metadata_v2_flatbuffers_factory.cpp (2.6KB)
- metadata_v2_thrift_upstream.cpp (84KB)

These files were replaced by the domain-based architecture:
- common_metadata_operations.cpp (domain-agnostic implementation)
- domain_metadata_views.cpp (view wrappers)
- flatbuffers_metadata_adapter.cpp (FlatBuffers adapter)
- thrift_metadata_adapter.cpp (Thrift adapter)

Files moved to .backup/session31-obsolete-backend/ for reference.

Refs: Session 31K (timestamp implementation), Session 31J (blocker resolution)
"
```

## Phase 5: Update Documentation (50 min)

### 5.1 Update README.adoc (20 min)

**File**: `README.adoc`

Add architecture section documenting the domain-based design:

```adoc
== Architecture

=== Domain-Based Metadata System

DwarFS uses a clean domain-based architecture for metadata handling:

.Metadata Architecture
[source]
----
            Domain Model (Format-Agnostic)
        metadata::domain::metadata
                      │
         ┌────────────┴────────────┐
         ▼                         ▼
  FlatBuffers Adapter       Thrift Adapter
  (flatbuffers_metadata_   (thrift_metadata_
   adapter.cpp)             adapter.cpp)
         │                         │
         └────────────┬────────────┘
                      ▼
        common_metadata_operations
            (All Filesystem Operations)
----

==== Key Benefits

* **Single Implementation**: All filesystem operations in one place
* **Format Independence**: No serialization knowledge in core operations
* **Easy Testing**: Domain model is pure C++ structures
* **Maintainability**: Eliminated ~337KB of duplicate code

==== Supported Formats

* **FlatBuffers** (`.dff`): Modern default
  - Header-only library
  - Memory-mappable, zero-copy
  - Excellent portability
  - Recommended for all new images

* **Thrift Compact** (`.dft`): Legacy format
  - Requires Folly + fbthrift
  - Memory-mappable, zero-copy
  - Smallest format (bit-packed)
  - Optional, for reading old images
```

### 5.2 Create Architecture Documentation (20 min)

**New File**: `doc/dwarfs-metadata-architecture.md`

Document the complete architecture with:
- Domain model structure
- Adapter pattern implementation
- Common operations layer
- View interfaces
- Performance characteristics

### 5.3 Move Completed Session Docs (10 min)

```bash
mkdir -p doc/old-sessions

# Move completed session docs
mv doc/SESSION_28_*.md doc/old-sessions/
mv doc/SESSION_29_*.md doc/old-sessions/
mv doc/SESSION_30_*.md doc/old-sessions/
mv doc/SESSION_31A_*.md doc/old-sessions/ 2>/dev/null || true
mv doc/SESSION_31B_*.md doc/old-sessions/ 2>/dev/null || true
mv doc/SESSION_31C_*.md doc/old-sessions/ 2>/dev/null || true
mv doc/SESSION_31D_*.md doc/old-sessions/ 2>/dev/null || true
mv doc/SESSION_31E_*.md doc/old-sessions/ 2>/dev/null || true
mv doc/SESSION_31F_*.md doc/old-sessions/ 2>/dev/null || true
mv doc/SESSION_31G_*.md doc/old-sessions/ 2>/dev/null || true
mv doc/SESSION_31H_*.md doc/old-sessions/ 2>/dev/null || true
mv doc/SESSION_31I_*.md doc/old-sessions/ 2>/dev/null || true

# Keep final summaries
# - SESSION_31J_COMPLETION_SUMMARY.md (architecture complete)
# - SESSION_31K_*.md (timestamp implementation)
# - SESSION_31L_*.md (this session)
```

## Success Criteria

**Must-Have**:
- [x] CMake cleaned (no references to deleted files)
- [ ] 7 files moved to backup (337KB)
- [ ] FlatBuffers-only build works
- [ ] Both-formats build works
- [ ] Tools create and read filesystems
- [ ] Git commit with clear message
- [ ] README.adoc updated with architecture

**Should-Have**:
- [ ] Architecture documentation created
- [ ] Old session docs moved to old-sessions/
- [ ] Performance validation (same speed)

## Timeline

| Phase | Duration | Cumulative |
|-------|----------|------------|
| 1. Remove from CMake | 20 min | 20 min |
| 2. Move files to backup | 10 min | 30 min |
| 3. Verify builds work | 30 min | 1 hour |
| 4. Git commit | 10 min | 1 hour 10 min |
| 5. Update documentation | 50 min | 2 hours |
| **Total** | **2 hours** | |

## Risk Assessment

**Low Risk**:
- Files are already not being compiled
- Moving to backup (not deleting)
- Can restore from backup if needed

**Mitigation**:
- Test each build configuration
- Keep backup in .backup/ directory
- Document what was removed and why

## Next Session Start

1. Read this continuation plan
2. Start with Phase 1: Remove from CMake
3. Test after each phase
4. Document any issues found

---

**Last Updated**: 2025-12-23 16:06 HKT
**Status**: Ready for execution
**Prerequisites**: Session 31K complete (timestamps working)