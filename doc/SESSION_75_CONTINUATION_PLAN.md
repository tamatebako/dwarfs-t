# Session 75: Three-Format Testing & Documentation

**Prerequisites**: Session 74 complete (Modern Thrift serializer working)
**Goal**: Validate three-format system and update official documentation

---

## Overview

Session 74 completed the Modern Thrift serializer implementation. All three metadata formats (FlatBuffers, Modern Thrift, Legacy Thrift) are now functional and ready for testing. Session 75 will validate the three-format system and update official documentation for v0.17.0 release.

### Current State

✅ **Complete**:
- Custom jemalloc with unprefixed symbols
- Folly v2025.12.29.00 integration
- FBThrift v2025.12.29.00 integration
- Modern Thrift serializer implementation
- All 5 libraries built successfully

❌ **Remaining**:
- Three-format validation testing
- Official documentation updates
- CMake linker fixes (optional)
- v0.17.0 release preparation

---

## Phase 1: Three-Format Validation (2 hours)

### Objective
Verify all three metadata formats work correctly and produce byte-for-byte identical outputs when extracting.

### Tasks

**1.1: Create Test Images** (30 min)
```bash
# Prepare test data
mkdir -p /tmp/test-3formats
echo "test content" > /tmp/test-3formats/file1.txt
mkdir /tmp/test-3formats/subdir
echo "more content" > /tmp/test-3formats/subdir/file2.txt

# FlatBuffers (modern default)
./mkdwarfs -i /tmp/test-3formats -o /tmp/test.dff

# Modern Thrift (CompactProtocol)
./mkdwarfs -i /tmp/test-3formats -o /tmp/test.dtc \
  --metadata-format=thrift-compact

# Legacy Thrift (hand-coded)
./mkdwarfs -i /tmp/test-3formats -o /tmp/test.dth \
  --metadata-format=legacy-thrift
```

**1.2: Verify Magic Bytes** (15 min)
```bash
# FlatBuffers: "DFBF" at offset 0 or 8
xxd /tmp/test.dff | head -2

# Modern Thrift: 0x82 0x21 at offset 0
xxd /tmp/test.dtc | head -2

# Legacy Thrift: No magic bytes
xxd /tmp/test.dth | head -2
```

**1.3: Integrity Checks** (30 min)
```bash
# Check all formats
./dwarfsck /tmp/test.dff --check-integrity
./dwarfsck /tmp/test.dtc --check-integrity
./dwarfsck /tmp/test.dth --check-integrity

# Verify metadata
./dwarfsck /tmp/test.dff --print-metadata
./dwarfsck /tmp/test.dtc --print-metadata
./dwarfsck /tmp/test.dth --print-metadata
```

**1.4: Extract and Compare** (45 min)
```bash
# Extract all formats
./dwarfsextract -i /tmp/test.dff -o /tmp/extracted-fb
./dwarfsextract -i /tmp/test.dtc -o /tmp/extracted-tc
./dwarfsextract -i /tmp/test.dth -o /tmp/extracted-th

# Verify byte-for-byte identity
diff -r /tmp/test-3formats /tmp/extracted-fb
diff -r /tmp/test-3formats /tmp/extracted-tc
diff -r /tmp/test-3formats /tmp/extracted-th
```

**Success Criteria**:
- All 3 formats create successfully
- Magic bytes correct for each format
- Integrity checks pass for all formats
- Extracted contents are byte-for-byte identical

---

## Phase 2: Official Documentation Updates (3 hours)

### Objective
Update README.adoc and official documentation to reflect three-format support.

### Tasks

**2.1: Update README.adoc** (60 min)

Add section on metadata formats:
```adoc
== Metadata Formats

DwarFS supports three metadata serialization formats:

=== FlatBuffers (Modern Default)
* *File Extension*: `.dff`
* *Magic Bytes*: "DFBF" (0x44 0x46 0x42 0x46)
* *Dependencies*: Header-only FlatBuffers library
* *Advantages*: Excellent portability, zero-copy access, forward/backward compatible
* *Use Case*: Default for all new images

=== Modern Thrift Compact
* *File Extension*: `.dtc`
* *Magic Bytes*: 0x82 0x21 (CompactProtocol)
* *Dependencies*: Folly + fbthrift (optional)
* *Advantages*: Smallest metadata size, fast serialization
* *Use Case*: When building with Thrift support and minimum size required

=== Legacy Thrift
* *File Extension*: `.dth`
* *Magic Bytes*: None (fallback)
* *Dependencies*: None (hand-coded)
* *Advantages*: No external dependencies, backward compatible with v0.14.1
* *Use Case*: Compatibility with older DwarFS images
```

**2.2: Update mkdwarfs.adoc** (60 min)

Document `--metadata-format` option:
```adoc
== Metadata Format Options

`--metadata-format=FORMAT`::
  Specify metadata serialization format. Valid values:
  * `flatbuffers` (default): Modern FlatBuffers format (.dff)
  * `thrift-compact`: Modern Thrift CompactProtocol (.dtc)
  * `legacy-thrift`: Hand-coded Thrift format (.dth)
```

**2.3: Create doc/metadata-formats.adoc** (60 min)

New document detailing:
- Format comparison table (size, speed, dependencies)
- When to use each format
- Cross-format compatibility
- Migration guide

**Success Criteria**:
- README.adoc updated with format information
- mkdwarfs.adoc documents --metadata-format option
- New metadata-formats.adoc guide created

---

## Phase 3: Move Temporary Documentation (30 min)

### Objective
Organize documentation by moving completed session docs to old-docs/sessions/.

### Tasks

**3.1: Move Session Documentation**
```bash
mkdir -p old-docs/sessions/session-72-74
mv doc/SESSION_72_*.md old-docs/sessions/session-72-74/
mv doc/SESSION_73_*.md old-docs/sessions/session-72-74/
mv doc/SESSION_74_*.md old-docs/sessions/session-72-74/
```

**3.2: Update Index**
Create `old-docs/sessions/README.md` with links to archived sessions.

**Success Criteria**:
- Temporary session docs moved to old-docs/
- Index created for easy reference

---

## Phase 4: Optional - Fix CMake Linker Issues (1-2 hours)

### Objective
Resolve CMake generator expression bug causing linker failures.

### Issue
```bash
/bin/sh: LINK_ONLY:Threads::Threads: No such file or directory
```

### Solution
Update CMakeLists.txt to properly expand Threads::Threads target.

**Files to Modify**:
- `CMakeLists.txt` - Fix linker flags for tools

**Success Criteria**:
- All 150/150 files compile and link successfully
- All 4 tools (mkdwarfs, dwarfs, dwarfsck, dwarfsextract) build

---

## Phase 5: v0.17.0 Release Preparation (Optional)

### Objective
Prepare for v0.17.0 release with three-format support.

### Tasks

**5.1: Update CHANGES.md**
Document new features:
- Three metadata formats support
- Modern Thrift CompactProtocol serializer
- FlatBuffers as modern default
- vcpkg custom ports for jemalloc/Folly/fbthrift

**5.2: Version Bump**
- Update version to 0.17.0 in cmake/version.cmake
- Tag release: `git tag -a v0.17.0 -m "Three metadata formats"`

**Success Criteria**:
- CHANGES.md updated
- Version bumped to 0.17.0
- Git tag created

---

## Success Criteria (Overall)

✅ **Phase 1**: All 3 formats validated and working
✅ **Phase 2**: Official documentation complete
✅ **Phase 3**: Temporary docs organized
✅ **Phase 4** (Optional): CMake linker issues resolved
✅ **Phase 5** (Optional): v0.17.0 release ready

---

## Time Estimate

| Phase | Time | Cumulative |
|-------|------|------------|
| 1. Validation | 2 hours | 2:00 |
| 2. Documentation | 3 hours | 5:00 |
| 3. Organization | 0.5 hours | 5:30 |
| 4. CMake Fix (opt) | 1-2 hours | 6:30-7:30 |
| 5. Release Prep (opt) | 1 hour | 7:30-8:30 |

**Total**: 5.5-8.5 hours

---

**Plan Created**: 2026-01-05 11:01 HKT
**Prerequisites**: Session 74 complete
**Next Session**: Validate three-format system and update documentation