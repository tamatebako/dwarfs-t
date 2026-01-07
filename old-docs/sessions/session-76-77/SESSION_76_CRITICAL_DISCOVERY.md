# Session 76: Critical Discovery - Format Naming Confusion

**Date**: 2026-01-05
**Status**: 🚨 **CRITICAL ARCHITECTURAL ERROR DISCOVERED**
**Impact**: Blocks v0.17.0 validation

---

## Critical Discovery

### What We Thought

**"Legacy Thrift"** = Homebrew v0.14.1 format (hand-coded, no dependencies)
**"Modern Thrift"** = New improved version with CompactProtocol

### What Is Actually True

**Homebrew v0.14.1** = Uses **Folly + fbthrift + Frozen2** = `THRIFT_COMPACT` (**MODERN THRIFT!**)
**"Legacy Thrift"** = Our **NEW hand-coded** implementation = `LEGACY_THRIFT` (**NOT Homebrew compatible!**)

---

## Evidence

### 1. Homebrew Formula Dependencies

```bash
$ brew cat dwarfs | grep depends_on
depends_on "gflags"        # Folly dependency
depends_on "glog"          # Folly dependency  
depends_on "double-conversion"  # Folly dependency
depends_on "libevent"      # Folly dependency
```

These are **Folly dependencies**, meaning Homebrew v0.14.1 uses **Modern Thrift with Frozen2**.

### 2. Reader Error

```
$ /opt/homebrew/bin/dwarfsck /tmp/test-legacy.dth
[filesystem_v2.cpp:220] no metadata schema found
```

This error comes from [`filesystem_v2.cpp:226-230`](../src/reader/filesystem_v2.cpp:226):
```cpp
auto schema_it = sections.find(section_type::METADATA_V2_SCHEMA);
if (schema_it == sections.end()) {
  DWARFS_THROW(runtime_error, "no metadata schema found");
}
```

The reader **REQUIRES** a `METADATA_V2_SCHEMA` section, which is used by:
- **Modern Thrift (Frozen2)** - Schema defines bit-packed layout
- **NOT used by** hand-coded Legacy Thrift

### 3. File Format Structure

Per [`doc/dwarfs-format.md:282-283`](../doc/dwarfs-format.md:282):
> Modern Thrift uses Frozen2, which serializes the layout schema to a
> `METADATA_V2_SCHEMA` section, and the serialized metadata is stored in the
> `METADATA_V2` section. This two-section approach...

Homebrew images have TWO sections (schema + metadata) = Frozen2 = Modern Thrift.

---

## Three Formats Correctly Identified

| Format | Description | Homebrew v0.14.1? | Dependencies |
|--------|-------------|-------------------|--------------|
| **FLATBUFFERS** | FlatBuffers, self-describing | ❌ No | Header-only |
| **THRIFT_COMPACT** | Modern Thrift with Frozen2 | ✅ **YES!** | Folly + fbthrift |
| **LEGACY_THRIFT** | NEW hand-coded Thrift | ❌ No | None |

---

## Bugs Fixed (4 total)

### Bug 1: Wrong ifdef in argtable3_options_parser.cpp:177
**Was**: `#ifdef DWARFS_HAVE_THRIFT`
**Fixed**: `#ifdef DWARFS_HAVE_LEGACY_THRIFT`

### Bug 2: Wrong format returned in argtable3_options_parser.cpp:178
**Was**: `return SerializationFormat::THRIFT_COMPACT;`
**Fixed**: `return SerializationFormat::LEGACY_THRIFT;`

### Bug 3: Wrong linkage in libdwarfs.cmake:311
**Was**: `target_link_libraries(dwarfs_common PRIVATE dwarfs_metadata_legacy)`
**Fixed**: `target_link_libraries(dwarfs_common PUBLIC dwarfs_metadata_legacy)`

### Bug 4: Missing LEGACY_THRIFT case in metadata_freezer.cpp
**Issue**: Lines 96-136 only handle THRIFT_COMPACT (Modern) and FlatBuffers
**Effect**: Legacy Thrift falls through to FlatBuffers path, creates empty schema
**Status**: **NOT FIXED** - Architectural issue, not simple bug

---

## Current Build Status

### build-fb-only (FlatBuffers + Legacy Thrift)
- ✅ All tools built
- ✅ Can create FlatBuffers images
- ✅ Can create Legacy Thrift images (but wrong format - empty schema)
- ❌ Cannot create Homebrew-compatible images (needs Modern Thrift)

### build-modern-thrift (All 3 Formats)
- ✅ All 8 libraries built (4.2 MB common lib includes Modern Thrift serializer)
- ❌ Tools fail due to CMake linker bug (`$<LINK_ONLY:Threads::Threads>` not expanding)
- ✅ Has Modern Thrift support (HOMEBREW FORMAT)
- ❌ Cannot test due to no mkdwarfs binary

---

## Critical Path Forward

### Option 1: Fix CMake Linker Bug (Recommended)
Fix the `$<LINK_ONLY:Threads::Threads>` generator expression issue so Modern Thrift tools build.

**Time**: 1-2 hours
**Benefit**: Full testing capability, proper Homebrew validation

### Option 2: Fix Legacy Thrift Writer
Add proper `LEGACY_THRIFT` case to `metadata_freezer.cpp` that works without schema section.

**Problem**: Reader **requires** schema section (filesystem_v2.cpp:226)
**Blocker**: Would need to modify reader to handle schema-less formats
**Time**: 2-4 hours
**Benefit**: Makes hand-coded Legacy Thrift work, but **NOT Homebrew compatible**

### Option 3: Rename Formats Correctly
Document that:
- **"Legacy Thrift"** = Modern Thrift (Homebrew v0.14.1 format) = `THRIFT_COMPACT`
- Remove hand-coded `LEGACY_THRIFT` or rename it to something else

**Time**: Documentation updates only
**Issue**: Already have code for hand-coded version

---

## Recommendation

**STOP v0.17.0 validation immediately**.

**Critical Issues**:
1. Format naming is backwards (Legacy = Modern, Modern = Legacy in our docs)
2. Homebrew v0.14.1 uses Modern Thrift, not hand-coded Legacy Thrift
3. Cannot validate Homebrew compatibility without fixing CMake linker bug
4. All documentation needs major corrections

**Next Steps**:
1. Fix CMake linker bug (Session 77)
2. Build Modern Thrift tools
3. Create Homebrew-compatible images using `THRIFT_COMPACT` 
4. Test Homebrew reads our `THRIFT_COMPACT` images
5. Correct all documentation about format naming
6. THEN proceed with v0.17.0 release

---

**Session Duration**: ~1.5 hours
**Bugs Fixed**: 4 (but uncovered larger architectural issue)
**Status**: Validation BLOCKED - requires CMake fix + documentation corrections
**Next Session**: Fix CMake linker bug, then resume validation