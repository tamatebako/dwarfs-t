# Session 29: Compressed Phase 4+5 - Complete OOP Migration

**Date**: 2025-12-22+
**Goal**: Delete old backends, integrate clean OOP interfaces, test
**Timeline**: 6-8 hours (compressed from 11-14h)
**Approach**: AGGRESSIVE - Remove old code first, force clean implementation

## Critical Decision: DELETE OLD CODE FIRST

**Principle**: Force ourselves to implement cleanly by removing old crutches.

### Files to DELETE (6,800 lines of old code)

**Backup first**, then delete:
```bash
mkdir -p .backup/session29-deleted
cp src/reader/internal/metadata_v2_flatbuffers.cpp .backup/session29-deleted/
cp src/reader/internal/metadata_v2_thrift.cpp .backup/session29-deleted/
cp src/reader/internal/metadata_types_flatbuffers.cpp .backup/session29-deleted/
cp src/reader/internal/metadata_types_thrift.cpp .backup/session29-deleted/

# DELETE the old implementations
rm src/reader/internal/metadata_v2_flatbuffers.cpp
rm src/reader/internal/metadata_v2_thrift.cpp
rm src/reader/internal/metadata_types_flatbuffers.cpp
rm src/reader/internal/metadata_types_thrift.cpp
```

### What We Have (READY TO USE)

**Phase 1 - Converters** ✅:
- Domain ↔ Thrift converter (789 lines)
- Domain ↔ FlatBuffers converter (946 lines)
- **WORKS**: All 3 build configurations pass

**Phase 2 - Reader Interfaces** ✅:
- Abstract reader interface (87 lines)
- FlatBuffers reader (105 lines)
- Thrift reader (94 lines)
- Factory with format detection (95 lines)

**Phase 3 - Writer Interfaces** ✅:
- Abstract writer interface (61 lines)
- FlatBuffers writer (55 lines)
- Thrift writer (52 lines)

**Total Ready**: ~2,300 lines of clean OOP code

## Compressed Phase 4+5 Plan (6-8 hours)

### Part A: Integration (4-5 hours)

#### Step 1: Update metadata_v2.cpp Core (2h)

**File**: [`src/reader/internal/metadata_v2.cpp`](../src/reader/internal/metadata_v2.cpp)

**Old Approach** (delete this):
```cpp
class metadata_v2::impl {
  // Complex backend-specific code
  thrift_backend::metadata_ or flatbuffers_backend::metadata_
};
```

**New Approach** (implement this):
```cpp
class metadata_v2::impl {
  std::unique_ptr<metadata_reader_interface> reader_;
  metadata::domain::metadata cached_domain_;

  // Use reader interface + converters
  impl(logger& lgr, std::span<uint8_t const> data) {
    reader_ = create_metadata_reader(data.data(), data.size());
    cached_domain_ = reader_->read();
  }

  // All access methods use cached_domain_
  chunk get_chunk(size_t i) const { return cached_domain_.chunks[i]; }
  // etc.
};
```

**Key Points**:
- Use our reader interface exclusively
- Cache domain model for performance
- NO backend namespaces
- NO format-specific code

#### Step 2: Update metadata_builder (1-2h)

**File**: [`src/writer/internal/metadata_builder.cpp`](../src/writer/internal/metadata_builder.cpp)

**Implementation**:
```cpp
// Build domain model during construction
metadata::domain::metadata domain_meta;
// ... accumulate chunks, inodes, etc. into domain_meta

// At serialize time:
auto writer = create_metadata_writer(format);
return writer->serialize(domain_meta);
```

#### Step 3: Update CMake (30min)

Remove old file references from [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake):
- Remove metadata_v2_flatbuffers.cpp
- Remove metadata_v2_thrift.cpp
- Remove metadata_types_flatbuffers.cpp
- Remove metadata_types_thrift.cpp
- Keep only: metadata_v2.cpp + our new interface files

#### Step 4: Fix Compilation Errors (1h)

After deleting old code, there will be compilation errors. Fix by:
- Updating include paths
- Removing backend namespace references
- Using domain model access patterns

### Part B: Testing (2-3 hours)

#### Test 1: Build Verification (30min)
```bash
# Test all 3 configurations still build
cmake -B build-test-both ...
cmake -B build-test-fb ...
cmake -B build-test-thrift ...
```

#### Test 2: Functional Tests (1h)
```bash
# Create test filesystem
./build/mkdwarfs -i test/data -o test.dff --format=flatbuffers
./build/mkdwarfs -i test/data -o test.dft --format=thrift

# Mount and verify
./build/dwarfs test.dff mnt/ &
ls -laR mnt/
diff -r test/data mnt/
umount mnt/

# Extract and verify
./build/dwarfsextract -i test.dff -o extracted/
diff -r test/data extracted/
```

#### Test 3: Unit Tests (30min)
- Add tests for reader/writer interfaces
- Test round-trip: domain → serialize → deserialize → domain
- Verify both formats work

#### Test 4: Performance Check (30min)
- Verify no regression in read/write speed
- Check memory usage
- Compare with previous benchmarks

## Deliverables

### Files Created (Session 28 + 29)
1. ✅ Phase 1 converters (validated)
2. ✅ Phase 2 reader interfaces
3. ✅ Phase 3 writer interfaces
4. **Phase 4** integration (rewritten metadata_v2)
5. **Phase 5** tests passing

### Files Deleted
1. `metadata_v2_flatbuffers.cpp` (2516 lines)
2. `metadata_v2_thrift.cpp` (1959 lines)
3. `metadata_types_flatbuffers.cpp` (1151 lines)
4. `metadata_types_thrift.cpp` (1151 lines)

**Total Deletion**: 6,777 lines of complex backend code
**Total New Code**: ~2,300 lines of clean OOP interfaces

**Net Result**: -4,477 lines, +100% clarity, +100% maintainability

## Success Criteria

✅ All 3 build configurations pass
✅ All unit tests pass
✅ Functional tests pass (mount, extract)
✅ NO preprocessor guards in interface code
✅ FULLY OOP architecture
✅ Performance equivalent or better

## Timeline Compression Strategy

**Time Saved**:
- No gradual migration: -2h
- Delete first, implement clean: -2h
- Combined testing: -1h
- **Total**: 6-8h instead of 11-14h

**Risk**: More aggressive, but cleaner result and our interfaces are ready.

---

**Ready to execute in Session 29**: Yes - all preparation complete