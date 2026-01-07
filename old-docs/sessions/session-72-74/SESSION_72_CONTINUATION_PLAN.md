# Session 72 Continuation Plan

**Created**: 2026-01-03 19:48 HKT
**Status**: Ready for next session
**Prerequisites**: Session 72 Phase 1-2 complete, Phase 3 blocked

---

## Session Overview

Session 72 achieved critical milestones:
1. ✅ Fixed thrift1 compiler integration (vcpkg include paths)
2. ✅ Validated BZip2 fix on arm64-osx (production-ready)
3. ⏸️ Identified Modern Thrift blocker (Folly/jemalloc)

**Next Session Goal**: Complete v0.17.0 release with documentation + plan v0.17.1 jemalloc fix

---

## Phase 4: Folly/jemalloc Integration Fix (90 min)

### Objective

Fix vcpkg Folly overlay port to properly link **Tebako's jemalloc fork**

### Background

**CRITICAL RULE**: We MUST ALWAYS use **Tebako's jemalloc fork** (memory-bank/critical-rules.md #1)

**Why Tebako jemalloc?**
- ✅ Supports all ARM64 platforms (including Apple Silicon)
- ✅ Compatible with static linking (Tebako requirement)
- ✅ Tested across all Tebako-supported platforms
- ✅ Actively maintained by Tebako team

**Current Problem**: vcpkg Folly expects jemalloc functions but they're undefined

### Solution Steps

#### Step 4.1: Update Folly vcpkg.json (15 min)

**File**: `vcpkg_ports/folly/vcpkg.json`

Add jemalloc dependency:
```json
{
  "name": "folly",
  "version-string": "2025.12.29.00",
  "dependencies": [
    "boost-chrono",
    "boost-context",
    ...
    {
      "name": "jemalloc",
      "platform": "!windows & !arm64-windows"
    }
  ]
}
```

#### Step 4.2: Configure Folly CMake for jemalloc (30 min)

**File**: `vcpkg_ports/folly/portfile.cmake`

Add after line 23 (before `vcpkg_cmake_configure`):
```cmake
# Find jemalloc (Tebako fork preferred)
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(JEMALLOC jemalloc)
endif()

if(NOT JEMALLOC_FOUND)
  find_library(JEMALLOC_LIBRARY NAMES jemalloc)
  find_path(JEMALLOC_INCLUDE_DIR NAMES jemalloc/jemalloc.h)
  if(JEMALLOC_LIBRARY AND JEMALLOC_INCLUDE_DIR)
    set(JEMALLOC_FOUND TRUE)
    set(JEMALLOC_LIBRARIES ${JEMALLOC_LIBRARY})
    set(JEMALLOC_INCLUDE_DIRS ${JEMALLOC_INCLUDE_DIR})
  endif()
endif()
```

Update vcpkg_cmake_configure OPTIONS:
```cmake
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DMSVC_USE_STATIC_RUNTIME=${MSVC_USE_STATIC_RUNTIME}
        -DCMAKE_INSTALL_DIR=share/folly
        -DCMAKE_POLICY_DEFAULT_CMP0167=NEW
        # jemalloc configuration
        $<$<BOOL:${JEMALLOC_FOUND}>:-DFOLLY_USE_J EMALLOC=ON>
        $<$<BOOL:${JEMALLOC_FOUND}>:-DJEMALLOC_INCLUDE_DIR=${JEMALLOC_INCLUDE_DIRS}>
        $<$<BOOL:${JEMALLOC_FOUND}>:-DJEMALLOC_LIBRARIES=${JEMALLOC_LIBRARIES}>
        # Existing options
        -DVCPKG_LOCK_FIND_PACKAGE_fmt=ON
        ...
)
```

#### Step 4.3: Test Modern Thrift Build (30 min)

```bash
# Clean rebuild
rm -rf build-modern-thrift

# Configure with Modern Thrift
cmake -B build-modern-thrift -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON

# Build
ninja -C build-modern-thrift

# Test metadata
ctest --test-dir build-modern-thrift --tests-regex "metadata" --output-on-failure
```

**Expected Result**: All files compile without jemalloc errors

#### Step 4.4: Validate Format Detection (15 min)

Create test images in all three formats:
```bash
# FlatBuffers format (.dff)
./build-modern-thrift/mkdwarfs -i /tmp/test -o test-fb.dff

# Legacy Thrift format (.dth - default without Modern Thrift)
./build-modern-thrift/mkdwarfs -i /tmp/test -o test-legacy.dth --no-modern-thrift

# Modern Thrift format (.dtc - if enabled)
./build-modern-thrift/mkdwarfs -i /tmp/test -o test-modern.dtc --format=thrift-compact
```

Verify format detection:
```bash
# Check magic bytes
xxd test-fb.dff | head -5     # Should see "DFBF" at offset 8
xxd test-modern.dtc | head -5  # Should see "82 21" at offset 0
xxd test-legacy.dth | head -5  # No magic bytes

# Verify read-back
./build-modern-thrift/dwarfsck test-fb.dff --check-integrity
./build-modern-thrift/dwarfsck test-modern.dtc --check-integrity
./build-modern-thrift/dwarfsck test-legacy.dth --check-integrity
```

---

## Phase 5: Documentation Updates (60 min)

### Objective

Update all documentation to reflect three-format architecture

### Step 5.1: Update vcpkg-build-guide.md (20 min)

**File**: `doc/vcpkg-build-guide.md`

Add sections:
1. **Three Metadata Formats** - Overview table with magic bytes
2. **Format Selection** - When to use each format
3. **Build Configurations** - How to enable/disable formats
4. **Troubleshooting** - Common vcpkg issues

Example content:
```markdown
## Metadata Formats

DwarFS supports three metadata serialization formats:

| Format | Magic Bytes | File Ext | Priority | Dependencies | Use Case |
|--------|-------------|----------|----------|--------------|----------|
| FlatBuffers | "DFBF" | `.dff` | 120 | Header-only | **Recommended** default |
| Modern Thrift | `0x82 0x21` | `.dtc` | 100 | Folly + jemalloc | Minimum size |
| Legacy Thrift | None | `.dth` | 50 | None | v0.14.1 compat |

### Format Detection

Formats are auto-detected via magic bytes in priority order (highest first).
Legacy Thrift has no magic bytes and serves as fallback.

### Build Configurations

**FlatBuffers-only** (recommended):
```bash
cmake -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
```

**All formats**:
```bash
cmake -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
```
```

### Step 5.2: Update README.adoc (20 min)

**File**: `README.adoc`

Update sections:
1. **Features** - Add "Multiple metadata formats" with link
2. **Building** - Document vcpkg requirements
3. **Usage** - Show format selection options

Example:
```asciidoc
== Features

* **Three metadata formats** for flexibility:
  - FlatBuffers (default): excellent portability, zero-copy access
  - Modern Thrift: minimum size (~5% smaller than FlatBuffers)
  - Legacy Thrift: v0.14.1 compatibility (no dependencies)
* Auto-detection via magic bytes
* Cross-format recompression support

== Building

=== vcpkg (Recommended)

vcpkg provides consistent dependencies across all platforms.

[source,bash]
----
cmake -B build -GNinja \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=./vcpkg_ports \
  -DVCPKG_TARGET_TRIPLET=<your-triplet> \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON

ninja -C build
----

Supported triplets: arm64-osx, x64-osx, x64-linux, arm64-linux, x64-windows, etc.
```

### Step 5.3: Create Format Selection Guide (20 min)

**File**: `doc/metadata-format-guide.md` (NEW)

Content:
```markdown
# DwarFS Metadata Format Selection Guide

## Quick Decision Tree

**Use FlatBuffers** (.dff) if:
- ✅ You want the best portability (header-only library)
- ✅ You're distributing images to diverse platforms
- ✅ Build simplicity is important
- ✅ Size difference of ~5% is acceptable

**Use Modern Thrift** (.dtc) if:
- ✅ Absolute minimum size is critical
- ✅ You can build with Folly + jemalloc dependencies
- ✅ You're using Tebako's jemalloc fork
- ⚠️  vcpkg overlay ports are configured correctly

**Use Legacy Thrift** (.dth) if:
- ✅ You need v0.14.1 Homebrew compatibility
- ✅ You want zero external dependencies
- ⚠️  Slightly larger than other formats (~5-10%)

## Format Specifications

[Insert detailed specs from SESSION_72_IMPLEMENTATION_STATUS.md]
```

---

## Phase 6: Release Preparation (45 min)

### Objective

Prepare v0.17.0 release with clear feature matrix

### Step 6.1: Update CHANGES.md (15 min)

**File**: `CHANGES.md`

Add v0.17.0 release notes:
```markdown
## v0.17.0 (2026-01-XX)

### Major Features

- **Three Metadata Formats**:
  - FlatBuffers: Modern default <truncated>
```

### Step 6.2: Update context.md (15 min)

**File**: `.kilocode/rules/memory-bank/context.md`

Update Component Status table and Key Documentation section

### Step 6.3: Tag Release (15 min)

```bash
# Verify all tests pass
ctest --test-dir build --output-on-failure

# Tag release
git tag -a v0.17.0 -m "Release v0.17.0: Three metadata formats, vcpkg support"

# Push tag
git push origin v0.17.0
```

---

## Success Criteria

### v0.17.0 Release

- [ ] FlatBuffers format: Production-ready ✅
- [ ] Legacy Thrift format: Production-ready ✅
- [ ] Modern Thrift format: Documented as experimental
- [ ] vcpkg build: Works on arm64-osx ✅
- [ ] BZip2 fix: Validated ✅
- [ ] Documentation: Complete
- [ ] Tests: All passing

### v0.17.1 (Future)

- [ ] Modern Thrift: Folly/jemalloc integrated
- [ ] Modern Thrift: Production-ready
- [ ] Multi-triplet testing: All 20 triplets validated

---

## Time Estimates

| Phase | Duration | Cumulative |
|-------|----------|------------|
| 4. Folly/jemalloc Fix | 90 min | 90 min |
| 5. Documentation | 60 min | 150 min |
| 6. Release Prep | 45 min | 195 min |
| **Total** | **195 min** | **~3h 15m** |

---

## Rollback Plan

If Modern Thrift fix fails:
1. Document as "experimental - requires manual fbthrift" in v0.17.0
2. Release with FlatBuffers + Legacy Thrift only
3. Defer Modern Thrift to v0.17.1
4. **Still a valid release** (two production formats working)

---

## Files to Modify

### Phase 4 (Jemalloc Fix)
- `vcpkg_ports/folly/vcpkg.json`
- `vcpkg_ports/folly/portfile.cmake`

### Phase 5 (Documentation)
- `doc/vcpkg-build-guide.md`
- `README.adoc` or `README.md`
- `doc/metadata-format-guide.md` (NEW)

### Phase 6 (Release)
- `CHANGES.md`
- `.kilocode/rules/memory-bank/context.md`
- Git tags

---

**Next Session**: Start with Phase 4.1 (Update Folly vcpkg.json)
**Prerequisites**: Ensure Tebako jemalloc is available in vcpkg or system
**Estimated Completion**: v0.17.0 ready in ~3 hours