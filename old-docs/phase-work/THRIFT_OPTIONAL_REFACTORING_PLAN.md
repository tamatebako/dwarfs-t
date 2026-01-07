# Plan: Make Thrift/Folly Entirely Optional in DwarFS

**Last Updated:** 2025-11-12
**Current Status:** 🟢 Metadata serialization complete, 🟡 Remaining work identified

## Executive Summary

**Goal:** Enable builds without Thrift/Folly on platforms where they cannot be compiled (MSys2/MinGW, some embedded systems, etc.)

**Current Achievement:** ✅ Metadata serialization is now **fully multi-format** with Thrift/Cereal/Bitsery support
- Thrift is **optional at build time** (`-DDWARFS_WITH_THRIFT=OFF`)
- Cereal and Bitsery work as complete alternatives
- Format auto-detection functional
- All formats tested in CI

**Remaining Scope:** Make **all non-metadata Thrift/Folly usage** optional (History system, Feature management, utility functions)

**Estimated Effort:** 1-2 weeks for remaining work + 1 week for memory allocator testing

## Current Status: What Works

### ✅ Metadata Serialization Multi-Format Support (COMPLETE)

**Achievement:** Three serialization formats fully operational:
- **Thrift Compact** (legacy, optional) - via `thrift/metadata.thrift`
- **Cereal Binary** (modern default) - via `include/dwarfs/metadata/domain/cereal_support.h`
- **Bitsery** (performance) - via `include/dwarfs/metadata/domain/bitsery_support.h`

**Implementation:**
```
Domain Model (format-agnostic)
      ↓
include/dwarfs/metadata/domain/metadata.h
      ↓
├─→ Thrift Frozen2 (optional)
├─→ Cereal Binary (required)
└─→ Bitsery (required)
      ↓
Facade Pattern (src/metadata/serialization/serialization_facade.cpp)
      ↓
Registry with Auto-Detection (src/metadata/serialization/serializer_registry.cpp)
```

**Key Files:**
- Domain model: `include/dwarfs/metadata/domain/metadata.h`
- Serializers: `src/metadata/serialization/*_serializer.cpp` (3 files)
- Facade: `src/metadata/serialization/serialization_facade.cpp`
- Registry: `src/metadata/serialization/serializer_registry.cpp`
- Configuration: `cmake/metadata_serialization.cmake`
- Tests: `test/metadata/serialization_test.cpp`

**Build Configuration:**
```cmake
# In cmake/metadata_serialization.cmake
-DDWARFS_WITH_THRIFT=OFF   # Thrift optional
-DDWARFS_WITH_CEREAL=ON    # Cereal required
-DDWARFS_WITH_BITSERY=ON   # Bitsery required
```

**Tebako Builds:**
- Automatically force `DWARFS_WITH_THRIFT=OFF` (static linking incompatible)
- Use Cereal + Bitsery formats
- Fully functional without Thrift

### ✅ Platform Compatibility Achieved

**Working Platforms Without Thrift:**
- ✅ Tebako builds (Ubuntu, macOS, Alpine) - MKD and ALL scopes
- ✅ MSys2/MinGW (with `DWARFS_WITH_THRIFT=OFF`)
- ✅ Windows ARM64 (Thrift/Folly optional)
- ✅ All architectures (x86_64, aarch64, riscv64, etc.)

## Remaining Work

### 1. History System (2 files)
**Status:** ⚠️ PARTIALLY COMPLETE - Needs testing

**Problem:** History uses Thrift format for serialization

**Files:**
- `include/dwarfs/history.h` - History class interface
- `src/history.cpp` - History implementation (lines 46+ use Thrift)

**Solution Approach:**
```cpp
// Current (Thrift-only):
#ifdef DWARFS_HAVE_THRIFT
  thrift::history::history hist;
  // ... serialize with CompactSerializer
#endif

// Needed (Multi-format):
#ifdef DWARFS_HAVE_CEREAL
  // Cereal serialization
#elif DWARFS_HAVE_BITSERY
  // Bitsery serialization
#elif DWARFS_HAVE_THRIFT
  // Thrift serialization (legacy)
#endif
```

**Work Required:**
1. Define C++ history structures (mirror `thrift/history.thrift`)
2. Add Cereal/Bitsery serialization support
3. Test round-trip serialization
4. Update tests in `test/metadata/converters/thrift_metadata_converter_test.cpp`

**Estimate:** 2-3 days

### 2. Feature Management (2 files)
**Status:** ❌ NOT STARTED - Blocked pending History completion

**Problem:** Uses Thrift enum utilities for feature tracking

**Files:**
- `include/dwarfs/internal/features.h` - Feature enumeration
- `src/internal/features.cpp` - Uses `apache::thrift::util::enumNameOrThrow`

**Solution:**
```cpp
// Replace thrift/features.thrift with plain C++ enum:
namespace dwarfs {

enum class feature : uint32_t {
  sparsefiles = 1,
  symlinks = 2,
  hardlinks = 3,
  // ... copy all from thrift/features.thrift
};

std::string feature_to_string(feature f);
std::optional<feature> string_to_feature(std::string_view name);

} // namespace dwarfs
```

**Work Required:**
1. Create `include/dwarfs/internal/feature_types.h` with enum
2. Implement manual string<->enum mapping in `features.cpp`
3. Guard Thrift enum code with `#ifdef DWARFS_HAVE_THRIFT`
4. Update all references throughout codebase

**Estimate:** 1-2 days

### 3. Utility Functions (5+ files)
**Status:** ❌ NOT STARTED - Low priority

**Problem:** Some utility code uses Folly-specific functions

**Files:**
- `src/conv.cpp` - Uses `folly::to<>()`
- `src/detail/scoped_env.cpp` - Uses Folly environment utilities
- Potentially others

**Solution:**
- Abstract Folly usage behind compatibility layer
- Provide STL-based alternatives
- Guard with `#ifdef DWARFS_HAVE_FOLLY`

**Work Required:**
1. Audit all Folly usage outside metadata
2. Create `include/dwarfs/internal/compat.h` with alternatives
3. Implement STL fallbacks
4. Test without Folly

**Estimate:** 2-3 days

## New Requirement: Memory Allocator Testing Strategy

### Overview

**Goal:** Test both jemalloc (from tamatebako/jemalloc fork) and mimalloc across all platforms in CI

**Rationale:**
- Performance comparison between allocators
- Validate tamatebako/jemalloc fork functionality
- Platform-specific allocator compatibility testing
- Ensure mimalloc works as alternative

### Current State

**jemalloc Configuration:**
```cmake
# CMakeLists.txt:263-265
if(USE_JEMALLOC)
  pkg_check_modules(JEMALLOC IMPORTED_TARGET jemalloc>=${JEMALLOC_REQUIRED_VERSION})
endif()
```

**mimalloc Configuration:**
```cmake
# CMakeLists.txt:267-269
if(USE_MIMALLOC)
  find_package(mimalloc ${MIMALLOC_REQUIRED_VERSION} REQUIRED CONFIG)
endif()
```

**Mutual Exclusion:**
```cmake
# CMakeLists.txt:86-88
if(USE_JEMALLOC AND USE_MIMALLOC)
  message(FATAL_ERROR "USE_JEMALLOC and USE_MIMALLOC are mutually exclusive")
endif()
```

**Platform Restrictions:**
- **Windows ARM64:** jemalloc NOT supported (needs auto-disable)
- **All other platforms:** Both allocators should work

### Implementation Plan

#### Step 1: Update jemalloc Source to Tebako Fork

**File:** `CMakeLists.txt` (or new `cmake/need_jemalloc.cmake`)

```cmake
if(USE_JEMALLOC)
  # Windows ARM64 - jemalloc not supported
  if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|AARCH|arm64")
    message(STATUS "jemalloc not supported on Windows ARM64, disabling")
    set(USE_JEMALLOC OFF CACHE BOOL "jemalloc disabled" FORCE)
  else()
    # Use tamatebako fork with Tebako-specific improvements
    if(DEFINED ENV{TEBAKO_BUILD} OR TEBAKO_BUILD)
      # Tebako builds use pre-built jemalloc from Tebako environment
      pkg_check_modules(JEMALLOC IMPORTED_TARGET jemalloc>=${JEMALLOC_REQUIRED_VERSION})
    else()
      # Standard builds can use system jemalloc or tamatebako fork
      pkg_check_modules(JEMALLOC IMPORTED_TARGET jemalloc>=${JEMALLOC_REQUIRED_VERSION})
      if(NOT JEMALLOC_FOUND)
        message(STATUS "System jemalloc not found, consider tamatebako/jemalloc fork")
      endif()
    endif()
  endif()
endif()
```

**Note:** The tamatebako/jemalloc fork should be:
- Referenced in documentation
- Used in Docker images
- Pre-installed in Tebako dev containers

#### Step 2: Add CI Matrix for Allocator Testing

**File:** `.github/workflows/build.yml`

Add new job section after existing tests:

```yaml
  allocator-testing:
    name: Allocator Test - ${{ matrix.platform }} - ${{ matrix.allocator }}
    runs-on: ${{ matrix.runner }}
    needs: package-source

    strategy:
      fail-fast: false
      matrix:
        include:
          # Linux x86_64
          - platform: linux-amd64
            runner: ubuntu-latest
            arch: amd64
            dist: ubuntu
            allocator: jemalloc
            build_type: gcc-release-jemalloc-ninja

          - platform: linux-amd64
            runner: ubuntu-latest
            arch: amd64
            dist: ubuntu
            allocator: mimalloc
            build_type: gcc-release-mimalloc-ninja

          # Linux ARM64
          - platform: linux-arm64
            runner: ubuntu-24.04-arm64
            arch: arm64v8
            dist: ubuntu
            allocator: jemalloc
            build_type: gcc-release-jemalloc-ninja

          - platform: linux-arm64
            runner: ubuntu-24.04-arm64
            arch: arm64v8
            dist: ubuntu
            allocator: mimalloc
            build_type: gcc-release-mimalloc-ninja

          # macOS ARM64
          - platform: macos-arm64
            runner: macos-14
            allocator: jemalloc

          - platform: macos-arm64
            runner: macos-14
            allocator: mimalloc

          # macOS x86_64
          - platform: macos-x64
            runner: macos-13
            allocator: jemalloc

          - platform: macos-x64
            runner: macos-13
            allocator: mimalloc

    uses: ./.github/workflows/docker-run-build.yml
    with:
      build_type: ${{ matrix.build_type }}
      build_arch: ${{ matrix.arch }}
      build_dist: ${{ matrix.dist }}
      build_from_tarball: true
      upload_artifacts: false
```

#### Step 3: Create Build Type Configurations

**Files:** Need build scripts to recognize allocator types

For Docker builds, add to build scripts:
```bash
# Extract allocator from build_type
if [[ "$BUILD_TYPE" =~ -jemalloc- ]]; then
  CMAKE_ARGS="$CMAKE_ARGS -DUSE_JEMALLOC=ON -DUSE_MIMALLOC=OFF"
elif [[ "$BUILD_TYPE" =~ -mimalloc- ]]; then
  CMAKE_ARGS="$CMAKE_ARGS -DUSE_JEMALLOC=OFF -DUSE_MIMALLOC=ON"
fi
```

#### Step 4: Update Docker Images

**Files:** `.docker/Dockerfile.*` (all distributions)

Add jemalloc from tamatebako fork:

```dockerfile
# Ubuntu/Debian
RUN git clone https://github.com/tamatebako/jemalloc.git /tmp/jemalloc && \
    cd /tmp/jemalloc && \
    ./autogen.sh && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/jemalloc

# Install mimalloc
RUN apt-get install -y libmimalloc-dev || \
    (git clone https://github.com/microsoft/mimalloc.git /tmp/mimalloc && \
     cd /tmp/mimalloc && mkdir build && cd build && \
     cmake .. -DCMAKE_BUILD_TYPE=Release && \
     make -j$(nproc) && make install && \
     rm -rf /tmp/mimalloc)
```

#### Step 5: Performance Benchmarking

Add benchmarks comparing allocators:
```bash
# In CI or locally
for ALLOC in system jemalloc mimalloc; do
  cmake -B build-$ALLOC -DUSE_JEMALLOC=$([ "$ALLOC" = "jemalloc" ] && echo ON || echo OFF) \
        -DUSE_MIMALLOC=$([ "$ALLOC" = "mimalloc" ] && echo ON || echo OFF)
  cmake --build build-$ALLOC
  ./build-$ALLOC/dwarfs_benchmark --benchmark_out=results-$ALLOC.json
done
```

### Memory Allocator Platform Matrix

| Platform | jemalloc (tamatebako) | mimalloc | Default (system malloc) |
|----------|:---------------------:|:--------:|:-----------------------:|
| Linux x86_64 | ✅ Test | ✅ Test | ✅ Control |
| Linux ARM64 | ✅ Test | ✅ Test | ✅ Control |
| Linux riscv64 | ✅ Test | ✅ Test | ✅ Control |
| Linux i386 | ✅ Test | ✅ Test | ✅ Control |
| Linux ppc64le | ✅ Test | ✅ Test | ✅ Control |
| macOS ARM64 | ✅ Test | ✅ Test | ✅ Control |
| macOS x86_64 | ✅ Test | ✅ Test | ✅ Control |
| Windows x64 | ❌ Skip | ✅ Test | ✅ Control |
| Windows ARM64 | ❌ Not supported | ❌ Unknown | ✅ Control |
| FreeBSD | ✅ Test | ⚠️ TBD | ✅ Control |

**Testing Strategy:**
1. Run full test suite with each allocator
2. Run performance benchmarks (creation, decompression, random access)
3. Monitor memory usage patterns
4. Validate stability under load

## Architecture Analysis (Updated)

### Current Thrift/Folly Usage Categories

#### 1. Metadata Storage & Serialization ✅ COMPLETE
**Status:** Fully abstracted, all 3 formats working

**Implementation:**
- Domain model in `include/dwarfs/metadata/domain/`
- Format support via `cereal_support.h` and `bitsery_support.h`
- Thrift support in `thrift_compact_serializer.cpp` (guarded)
- Facade pattern abstracts all format differences
- Format detection via magic bytes

**No further work needed**

#### 2. History System ⚠️ PARTIALLY DONE
**Status:** Basic structure exists, needs completion

**Files:**
- `include/dwarfs/history.h` - History class interface
- `src/history.cpp` - Implementation (lines 46+ use Thrift)

**Current Issue:**
```cpp
// Line 52-116 in src/history.cpp
#ifdef DWARFS_HAVE_THRIFT
  apache::thrift::CompactSerializer serializer;
  thrift::history::history hist;
  // ...serialization code...
#endif
```

**Remaining Work:**
1. ✅ Define C++ history structures (already exists in domain model?)
2. ⚠️ Add Cereal serialization (needs validation)
3. ⚠️ Add Bitsery serialization (needs validation)
4. ❌ Write comprehensive tests
5. ❌ Validate parsing/serialization round-trips

**Estimate:** 2-3 days

#### 3. Feature Management ❌ NOT STARTED
**Status:** Still depends on Thrift enum utilities

**Files:**
- `include/dwarfs/internal/features.h` - Feature enumeration
- `src/internal/features.cpp` - Uses `apache::thrift::util::enumNameOrThrow`

**Problem:**
```cpp
// Current code depends on Thrift:
#include <thrift/lib/cpp2/util/EnumNameOrThrow.h>
auto name = apache::thrift::util::enumNameOrThrow(feature);
```

**Solution:**
1. Define plain C++ enum (not in Thrift IDL):
```cpp
namespace dwarfs {

enum class feature : uint32_t {
  sparsefiles = 1,
  symlinks = 2,
  hardlinks = 3,
  devices = 4,
  // ... copy all from thrift/features.thrift
};

std::string feature_to_string(feature f);
std::optional<feature> string_to_feature(std::string_view name);

} // namespace dwarfs
```

2. Implement manual mapping without Thrift utilities
3. Guard existing Thrift code with `#ifdef DWARFS_HAVE_THRIFT`

**Impact:** Used throughout codebase for feature detection
**Risk:** Medium - many files reference features

**Estimate:** 1-2 days

#### 4. Utility Functions (5+ files) ❌ NOT STARTED - Lower Priority
**Status:** Some code uses Folly-specific utilities

**Files:**
- `src/conv.cpp` - Uses `folly::to<T>()`
- `src/detail/scoped_env.cpp` - Uses Folly environment functions

**Solution:**
```cpp
// Create compatibility layer
namespace dwarfs::compat {

#ifdef DWARFS_HAVE_FOLLY
  using folly::to;
#else
  template<typename T>
  T to(std::string_view s) {
    // STL-based implementation
  }
#endif

} // namespace dwarfs::compat
```

**Estimate:** 2-3 days

## Revised Implementation Plan

### Phase 1: Complete History System (Week 1, Days 1-3)
**Goal:** Finish History multi-format support

#### Tasks:
1. Review existing History structure
2. Implement Cereal serialization if needed
3. Implement Bitsery serialization if needed
4. Write comprehensive tests
5. Validate against Thrift format (compatibility)

**Deliverable:** History system works without Thrift

### Phase 2: Feature System Refactoring (Week 1, Days 4-5)
**Goal:** Make feature management Thrift-independent

#### Tasks:
1. Create `include/dwarfs/internal/feature_types.h` with enum
2. Implement `feature_to_string()` and `string_to_feature()`
3. Update `src/internal/features.cpp` to use plain enum
4. Guard Thrift code with `#ifdef DWARFS_HAVE_THRIFT`
5. Test feature detection/validation

**Deliverable:** Features work without Thrift utilities

### Phase 3: Utility Functions (Week 2, Days 1-3)
**Goal:** Abstract remaining Folly usage

#### Tasks:
1. Audit all Folly usage in non-metadata code
2. Create compatibility layer `include/dwarfs/internal/compat.h`
3. Implement STL alternatives
4. Update code to use compat layer
5. Test builds without Folly

**Deliverable:** Can build entirely without Folly

### Phase 4: Memory Allocator Testing (Week 2-3)
**Goal:** Comprehensive allocator validation

#### Tasks:
1. Update Docker images with tamatebako/jemalloc + mimalloc
2. Add allocator testing matrix to CI (`.github/workflows/build.yml`)
3. Create build type configurations for allocators
4. Run performance benchmarks
5. Document results and recommendations

**Deliverable:** All platforms tested with both allocators

### Phase 5: Final Validation & Documentation (Week 3)
**Goal:** Ensure everything works, document thoroughly

#### Tasks:
1. Full CI test suite (with/without Thrift)
2. Performance regression testing
3. Update all documentation
4. Create migration guide for users
5. Update memory bank

**Deliverable:** Production-ready Thrift-optional DwarFS

## CMake Configuration Changes Needed

### 1. Enhanced jemalloc Detection
**File:** `CMakeLists.txt` or new `cmake/need_jemalloc.cmake`

```cmake
if(USE_JEMALLOC)
  # Platform check
  if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|AARCH|arm64")
    message(STATUS "jemalloc not supported on Windows ARM64")
    set(USE_JEMALLOC OFF CACHE BOOL "Disabled for platform" FORCE)
  else()
    # Prefer tamatebako fork in Tebako builds
    if(DEFINED ENV{TEBAKO_BUILD} OR TEBAKO_BUILD)
      # Tebako provides pre-built jemalloc
      pkg_check_modules(JEMALLOC IMPORTED_TARGET jemalloc>=${JEMALLOC_REQUIRED_VERSION})
      if(JEMALLOC_FOUND)
        message(STATUS "Using Tebako-provided jemalloc: ${JEMALLOC_VERSION}")
      endif()
    else()
      # Standard build - use system or build from source
      pkg_check_modules(JEMALLOC IMPORTED_TARGET jemalloc>=${JEMALLOC_REQUIRED_VERSION})
      if(JEMALLOC_FOUND)
        message(STATUS "Using system jemalloc: ${JEMALLOC_VERSION}")
      else()
        message(STATUS "jemalloc not found - consider installing from tamatebako/jemalloc")
      endif()
    endif()
  endif()
endif()
```

### 2. Build Type Naming Convention

Add to `cmake/compile.cmake` or relevant build configuration:

```cmake
# Artifact ID includes allocator information
if(JEMALLOC_FOUND)
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-jemalloc")
elseif(TARGET mimalloc)
  set(DWARFS_ARTIFACT_ID "${DWARFS_ARTIFACT_ID}-mimalloc")
endif()
```

## Platform-Specific Considerations (Updated)

### Windows ARM64
**Status:** ✅ FIXED

**jemalloc:** Not supported - automatically disabled
**mimalloc:** Should work - needs CI testing
**Thrift/Folly:** Optional, can build without

**Implementation:**
```cmake
# Already implemented in CMakeLists.txt (needs relocation to dedicated file)
if(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "ARM|AARCH")
  set(USE_JEMALLOC OFF CACHE BOOL "Not supported on Windows ARM64" FORCE)
endif()
```

### MSys2/MinGW
**Status:** ✅ WORKING (without Thrift)

**Folly:** Has type conflicts (pid_t, mode_t) with MinGW headers
**Solution:** Build with `-DDWARFS_WITH_THRIFT=OFF`
**Allocators:** Both should work - needs testing

**Current Workaround:** Document limitation, use Cereal/Bitsery
**Long-term:** Complete Phase 3 to eliminate Folly dependency

### FreeBSD
**Status:** ✅ WORKING

**jemalloc:** Should work with native FreeBSD jemalloc
**mimalloc:** Unknown - needs testing
**Recommendation:** Test both allocators in native FreeBSD builds

### Big-Endian Platforms (ppc64, s390x)
**Status:** ⚠️ EXPERIMENTAL

**jemalloc:** Likely works
**mimalloc:** Unknown
**Recommendation:** Test on at least ppc64le (little-endian) first

## Testing Requirements (Updated)

### Unit Tests
- ✅ Metadata serialization round-trips (all 3 formats) - DONE
- ⚠️ History serialization (Cereal/Bitsery) - NEEDS VALIDATION
- ❌ Feature enum without Thrift - TODO
- ✅ Format detection - DONE

### Integration Tests
- ✅ Create filesystems with each format - DONE
- ✅ Read filesystems with each format - DONE
- ⚠️ Cross-format compatibility - PARTIALLY TESTED
- ❌ Allocator comparison - TODO

### CI Validation
- ✅ Thrift enabled builds - WORKING
- ✅ Thrift disabled builds (Tebako) - WORKING
- ✅ All platforms build successfully - FIXED (fetch-depth issue)
- ❌ Allocator testing matrix - TODO

## Success Criteria (Updated)

### Build System
1. ✅ CMake configures with `DWARFS_WITH_THRIFT=OFF`
2. ✅ All non-Thrift code compiles without Thrift headers
3. ✅ Tebako builds work (MKD and ALL scopes)
4. ⚠️ MSys2 builds work (with Thrift=OFF) - NEEDS VALIDATION

### Functionality
5. ✅ Can create DwarFS filesystems using Cereal/Bitsery
6. ✅ Can read Cereal/Bitsery filesystems
7. ✅ Can read existing Thrift-format filesystems (with Thrift)
8. ⚠️ History system works without Thrift - NEEDS VALIDATION
9. ❌ Feature system works without Thrift - TODO

### Performance & Quality
10. ✅ All tests pass in Thrift mode
11. ✅ All tests pass in non-Thrift mode
12. ✅ CI builds succeed on all target platforms
13. ❌ Allocator testing complete - TODO
14. ⚠️ Performance acceptable for non-Thrift paths - NEEDS BENCHMARKING

## Timeline Estimate (Revised)

- **Week 1 (5 days):** Complete History + Features
  - Days 1-3: History system completion and testing
  - Days 4-5: Feature system refactoring
- **Week 2 (5 days):** Utility functions + Start allocator work
  - Days 1-3: Abstract Folly usage
  - Days 4-5: Setup allocator testing infrastructure
- **Week 3 (5 days):** Allocator testing + Documentation
  - Days 1-3: Run allocator CI tests, collect data
  - Days 4-5: Performance analysis, documentation

**Total:** 15 working days

##  Immediate Next Steps

### Urgent (This Week)
1. ✅ Fix CI builds (`fetch-depth: 0` issue) - **COMPLETED 2025-11-12**
2. ⚠️ Validate History system without Thrift
3. ❌ Complete feature enum refactoring
4. ❌ Add jemalloc Windows ARM64 auto-disable

### Important (Next Week)
5. ❌ Setup allocator testing in CI
6. ❌ Update Docker images with tamatebako/jemalloc
7. ❌ Abstract Folly utility usage
8. ❌ Performance benchmarking

### Documentation
9. ❌ Update memory bank with migration status
10. ❌ Document allocator testing results
11. ❌ Create migration guide for users

## Risk Assessment (Updated)

### Technical Risks

**Low Risk:**
- ✅ Metadata serialization - Already complete, working well
- ✅ Build system changes - Well understood, tested

**Medium Risk:**
- ⚠️ History system - Structure exists, needs validation
- ⚠️ Feature system - Straightforward but affects many files
- ⚠️ Allocator testing - May reveal platform-specific issues

**Higher Risk:**
- ❌ Folly utility abstraction - May uncover hidden dependencies
- ❌ Performance regression - Non-frozen formats slower than Thrift
- ❌ Compatibility issues - Old archives might have edge cases

### Mitigation Strategies
1. Keep Thrift as primary/optimized path where available
2. Cereal/Bitsery as functional alternatives
3. Comprehensive testing at each phase
4. Performance benchmarking before release
5. Clear documentation of format trade-offs

## Long-term Vision (Updated)

### Minimal Configuration (Target)
```cmake
cmake -B build -GNinja \
  -DDWARFS_WITH_THRIFT=OFF \
  -DDWARFS_WITH_CEREAL=ON \
  -DDWARFS_WITH_BITSERY=ON \
  -DUSE_JEMALLOC=ON  # or USE_MIMALLOC=ON
```

**Features:**
- No Thrift/fbthrift (no complex dependencies)
- Minimal or no Folly (header-only utilities OK)
- Cereal + Bitsery for all serialization
- Choice of memory allocator
- Works on MSys2, embedded systems, all platforms

### All-Formats Configuration (Current)
```cmake
cmake -B build -GNinja \
  -DDWARFS_WITH_THRIFT=ON \
  -DDWARFS_WITH_CEREAL=ON \
  -DDWARFS_WITH_BITSERY=ON \
  -DUSE_JEMALLOC=ON
```

**Features:**
- Full Thrift support (maximum compatibility)
- All three serialization formats
- Full Folly (performance optimizations)
- Preferred memory allocator
- Can read all filesystem formats

## Progress Tracking

### Completed (v0.14.0 - v0.14.1)
- ✅ Domain model architecture
- ✅ Cereal serialization implementation
- ✅ Bitsery serialization implementation
- ✅ Serialization facade pattern
- ✅ Format detection registry
- ✅ Thrift optional at build time
- ✅ Tebako integration (MKD + ALL scopes)
- ✅ CI testing matrix for formats
- ✅ Build.yml fetch-depth fixes (2025-11-12)

### In Progress
- ⚠️ History system validation
- ⚠️ Allocator infrastructure setup

### Pending
- ❌ Feature system refactoring
- ❌ Folly utility abstraction
- ❌ Allocator CI testing matrix
- ❌ Performance benchmarking
- ❌ Documentation updates

## References

**Key Documentation:**
- Memory Bank: `.kilocode/rules/memory-bank/`
- Architecture: `.kilocode/rules/memory-bank/architecture.md`
- Metadata Design: `doc/METADATA_STRUCTURES_DESIGN.md`

**Key Implementation:**
- Metadata domain: `include/dwarfs/metadata/domain/metadata.h`
- Serializers: `src/metadata/serialization/*.cpp`
- Config: `cmake/metadata_serialization.cmake`

**External Resources:**
- tamatebako/jemalloc: https://github.com/tamatebako/jemalloc
- Cereal: https://uscilab.github.io/cereal/
- Bitsery: https://github.com/fraillt/bitsery

---

**Document Status:** Living document - updated as work progresses
**Last Major Update:** 2025-11-12 (CI fixes, allocator strategy, revised timeline)