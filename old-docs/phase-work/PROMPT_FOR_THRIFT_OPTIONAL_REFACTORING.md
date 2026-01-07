# Prompt for Continuing Thrift-Optional Refactoring

## Context

I need to make Thrift/Folly entirely optional in the DwarFS codebase to support platforms where they cannot be built (Windows MSys2/MinGW, some embedded systems, restrictive build environments).

## Current Situation

**Repository:** https://github.com/tamatebako/dwarfs
**Branch:** `feature/multi-format-serialization-fuse`
**Latest Stable Commit:** 13f273c2 (before incomplete refactoring attempts)

**Project:** DwarFS - A high compression read-only file system
**Language:** C++ (C++20)
**Build System:** CMake
**Serialization:** Currently Thrift frozen layouts (mandatory), want to make it optional

## Problem Statement

The entire DwarFS metadata system is architecturally dependent on Apache Thrift frozen layouts for efficient memory-mapped metadata access. This dependency affects ~50+ files across the reader, writer, and filesystem subsystems.

**What Needs to Happen:**
Make Thrift/Folly completely optional while maintaining:
- Full backward compatibility with existing Thrift-format filesystems
- Efficient metadata access (memory-mapped when possible)
- Support for Cereal and Bitsery as complete alternatives
- All functionality working in both Thrift and non-Thrift modes

## Detailed Plan

A comprehensive refactoring plan has been created: **`doc/THRIFT_OPTIONAL_REFACTORING_PLAN.md`**

Please read this file first - it contains:
- 6-phase refactoring plan (Weeks 1-4 timeline)
- Detailed task breakdowns for each phase
- Architecture analysis of current Thrift usage
- Platform-specific considerations
- Risk assessment and mitigation strategies

## Key Files Requiring Changes

### Core Metadata System (~50+ files)
```
src/reader/internal/metadata_v2.cpp          - Primary metadata reader
src/reader/filesystem_v2.cpp                  - Filesystem operations
src/writer/internal/metadata_builder.cpp      - Metadata construction
src/writer/internal/metadata_freezer.cpp      - Thrift freezing
src/internal/features.cpp                     - Feature management
src/history.cpp                               - History tracking
```

### Headers Needing Abstraction
```
include/dwarfs/internal/features.h
include/dwarfs/history.h
include/dwarfs/internal/metadata_*
```

## Current Build Configuration

**CMake Options:**
- `DWARFS_WITH_THRIFT` - Enable Thrift support (ON by default)
- `DWARFS_WITH_CEREAL` - Enable Cereal support (ON by default)
- `DWARFS_WITH_BITSERY` - Enable Bitsery support (ON by default)

**Currently:** All builds require Thrift regardless of these options.
**Goal:** Make DWARFS_WITH_THRIFT=OFF actually work.

## Specific Technical Requirements

### 1. Feature System Refactoring (HIGH PRIORITY)
**Current:** Uses `apache::thrift::util::enumNameOrThrow` and `TEnumTraits`
**Needed:** Plain C++ enum in `include/dwarfs/internal/feature_types.h`

```cpp
enum class feature : uint32_t {
  sparsefiles = 1,
  symlinks = 2,
  // ... copy all from thrift/features.thrift
};

// Implement without Thrift utilities
std::string feature_to_string(feature f);
std::optional<feature> string_to_feature(std::string_view name);
```

### 2. Metadata Accessor Abstraction (CORE)
**Current:** Direct usage of `MappedFrozen<thrift::metadata::metadata>`
**Needed:** Interface for metadata access

```cpp
class metadata_accessor {
public:
  virtual ~metadata_accessor() = default;
  virtual size_t inode_count() const = 0;
  virtual std::string_view get_name(size_t idx) const = 0;
  // ... abstract ALL metadata access
};
```

### 3. Serialization Strategy
- **Thrift:** Keep frozen layouts for performance (when available)
- **Cereal:** Binary archives for structured data (fallback)
- **Bitsery:** High-performance binary serialization (alternative fallback)

Without frozen layouts, may need custom memory-mapped structures for performance.

## Platform-Specific Notes

### Windows ARM64
jemalloc doesn't support Windows ARM64. Add conditional:
```cmake
if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|AARCH")
  set(USE_JEMALLOC OFF)
endif()
```

### MSys2/MinGW
Folly has fundamental type conflicts (pid_t, mode_t) with MinGW. Even with Thrift optional, folly itself is still required for some non-metadata code (conv.cpp, scoped_env.cpp).

Options:
1. Patch folly for MinGW compatibility
2. Abstract folly usage to allow alternative implementations
3. Document MSys2 as unsupported platform

## Testing Requirements

Must pass in BOTH configurations:
1. **With Thrift:** `cmake . -DDWARFS_WITH_THRIFT=ON -DWITH_TESTS=ON && cmake --build . && ctest`
2. **Without Thrift:** `cmake . -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=ON && cmake --build . && ctest`

Functionality to verify:
- Create filesystem (mkdwarfs)
- Read filesystem (dwarfs, dwarfsck)
- Extract files (dwarfsextract)
- Metadata serialization round-trips
- All unit tests pass

## Starting Point

**Recommended Approach:**

1. **Phase 1 - Features (Day 1-2):**
   Start with feature enum refactoring - it's isolated and needed everywhere
   - Create `include/dwarfs/internal/feature_types.h`
   - Update `src/internal/features.cpp`
   - Test builds with/without Thrift

2. **Phase 2 - History (Day 3-4):**
   Attempt history refactoring was made but incomplete
   - Review previous attempt in commits 247b6fe0-264186c3 (reverted)
   - Learn from failures: need to also handle features.cpp
   - Implement complete solution with tests

3. **Phase 3 - Metadata Core (Week 2):**
   The big one - abstract metadata access
   - Start with metadata_accessor interface
   - Gradually migrate reader code
   - Leave writer for later if needed

## Questions to Consider

1. **Performance:** How critical is frozen layout performance? Can Cereal/Bitsery be memory-mapped?
2. **Compatibility:** Do we need to read old Thrift-format filesystems without Thrift? (Likely yes)
3. **Scope:** Can we ship with "Thrift required for reading old formats" initially?

## Success Metrics

1. ✅ `cmake . -DDWARFS_WITH_THRIFT=OFF` - configures successfully
2. ✅ Build completes without Thrift headers
3. ✅ Can create new filesystems using Cereal/Bitsery
4. ✅ Basic functionality works (create, mount, extract)
5. ✅ CI builds pass on MSys2, Windows ARM64, other restricted platforms

## Your Task

Execute the refactoring plan in `doc/THRIFT_OPTIONAL_REFACTORING_PLAN.md` to make Thrift/Folly entirely optional. Start with Phase 1 (feature enum) as it's the most isolated and provides immediate value. Test thoroughly at each phase before proceeding.

Good luck! This is a significant architectural improvement that will greatly expand DwarFS platform support.