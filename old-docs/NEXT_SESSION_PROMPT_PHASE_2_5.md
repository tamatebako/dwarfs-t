# Next Session: Complete Phase 2.5+ Multi-Format Metadata Implementation

**For AI Session Starting After**: 2025-11-23 15:15 HKT
**Estimated Duration**: 8-12 hours (can be split across multiple sessions)
**Branch**: `feature/multi-format-serialization-fuse`

---

## Quick Context

You're completing DwarFS multi-format metadata serialization. **Phase 2.5 is architecturally correct** but implementation needs alignment with existing interfaces. An existing file `include/dwarfs/reader/internal/metadata_view_interface.h` already defines the required abstract interfaces. Your task: align backends with existing interfaces and complete remaining work.

**What's Done** (Architecturally Sound):
- ✅ Code isolation (FlatBuffers + Thrift backends separated)
- ✅ Factory pattern implemented
- ✅ Strategy Pattern architecture valid
- ✅ Abstract interfaces exist (pre-existing file)

**What Remains** (Implementation Alignment):
- ⬜ Remove 4 duplicate interface files
- ⬜ Align backend signatures with existing interface
- ⬜ Fix factory to use existing interface
- ⬜ Complete comprehensive testing
- ⬜ Functional validation
- ⬜ Update official documentation

---

## Critical Issues to Fix Immediately

### 🔴 Issue 1: Duplicate Interface Definitions (BLOCKING BUILD)

**Problem**: Created 4 new interface files that duplicate existing `metadata_view_interface.h`

**Files to Delete**:
```bash
rm include/dwarfs/reader/internal/inode_view_interface.h
rm include/dwarfs/reader/internal/dir_entry_view_interface.h
rm include/dwarfs/reader/internal/global_metadata_interface.h
rm include/dwarfs/reader/internal/chunk_view_interface.h
```

**Why**: These files duplicate interfaces already in `metadata_view_interface.h` (lines 53, 120, 170, 246)

### 🔴 Issue 2: Interface Signature Mismatch (BLOCKING BUILD)

**Problem**: Backend methods return `unique_ptr`, existing interface expects `shared_ptr`

**Fix Required**:
- FlatBuffers backend: `inode()` method signature
- Thrift backend: `inode()` method signature
- Both: Remove extra methods not in existing interface
- Both: Keep only methods declared in `metadata_view_interface.h`

### 🔴 Issue 3: CMake References Wrong Files

**File**: `cmake/libdwarfs.cmake`

**Problem**: References 4 non-existent interface headers

**Fix**: Remove these lines (around line 126-129):
```cmake
include/dwarfs/reader/internal/inode_view_interface.h
include/dwarfs/reader/internal/dir_entry_view_interface.h
include/dwarfs/reader/internal/global_metadata_interface.h
include/dwarfs/reader/internal/chunk_view_interface.h
```

Keep: `include/dwarfs/reader/internal/metadata_factory.h`

---

## Your Complete Task: Phases 2.5-A through 2.9

### Phase 2.5-A: Remove Duplicates & Align Interfaces (2-3h)

**Goal**: Get builds working by aligning with existing interfaces

#### Step 1: Delete Duplicate Files (5min)
Delete 4 duplicate interface headers (listed above)

#### Step 2: Read Existing Interface (15min)
Thoroughly read `include/dwarfs/reader/internal/metadata_view_interface.h` to understand:
- Exact method signatures required
- Return types (mostly `shared_ptr`, not `unique_ptr`)
- Which methods are in interface vs backend-specific

#### Step 3: Update FlatBuffers Backend (45min)

**File**: `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`
- Change includes: Remove duplicate headers, include `metadata_view_interface.h`
- Update `global_metadata` inheritance
- Update `inode_view_impl` inheritance
- Update `dir_entry_view_impl` inheritance
- Fix `inode()` method: `unique_ptr` →

 `shared_ptr`
- Remove methods not in interface
- Keep `override` keywords only for virtual methods

**File**: `src/reader/internal/metadata_types_flatbuffers.cpp`
- Update `inode()` implementation to return `shared_ptr`
- Update `parent()` if signature changed
- Remove helper methods not needed

#### Step 4: Update Thrift Backend (45min)

**File**: `include/dwarfs/reader/internal/metadata_types_thrift.h`
- Same changes as FlatBuffers backend

**File**: `src/reader/internal/metadata_types_thrift.cpp`
- Same changes as FlatBuffers backend

#### Step 5: Update Factory (15min)

**File**: `include/dwarfs/reader/internal/metadata_factory.h`
- Include `metadata_view_interface.h` instead of duplicate headers
- Verify return types match interface

**File**: `src/reader/internal/metadata_factory.cpp`
- Update includes
- Verify creation returns correct interface types

#### Step 6: Update CMake (10min)

**File**: `cmake/libdwarfs.cmake`
- Remove references to 4 duplicate headers (around line 126-129)
- Keep `metadata_factory.h` and `src/reader/internal/metadata_factory.cpp`

### Phase 2.6: Verify Factory Integration (30min)

- Test format detection with sample FlatBuffers data
- Test format detection with sample Thrift data (if available)
- Verify error messages helpful
- Test factory independently

### Phase 2.7: Build System Validation (30min)

Test all configurations:

**FlatBuffers-only**:
```bash
cmake -B build-fb -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
cmake --build build-fb --target dwarfs_reader
```

**Dual-format** (if Thrift available):
```bash
cmake -B build-dual -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
cmake --build build-dual --target dwarfs_reader
```

**Tebako**:
```bash
cmake -B build-tebako -DTEBAKO_BUILD=ON -DTEBAKO_BUILD_SCOPE=ALL
cmake --build build-tebako
```

### Phase 2.8: Comprehensive Testing (3-4h)

Write 3 test files:

**File 1**: `test/metadata_view_interface_test.cpp`
- Test all interface methods work with both backends
- Verify polymorphism
- Test with real data
- 100% interface coverage

**File 2**: `test/metadata_factory_test.cpp`
- Test format detection (FlatBuffers vs Thrift)
- Test backend creation
- Test error handling
- Test with invalid data

**File 3**: `test/backend_compatibility_test.cpp`
- Test FlatBuffers backend compliance
- Test Thrift backend compliance (if dual-format)
- Test round-trip: serialize → deserialize → verify
- Compare results from both backends

### Phase 2.9: Validation & Documentation (2-3h)

#### Functional Validation
Test with real filesystem images:
```bash
# Create test image
./build-dual/mkdwarfs -i /usr/include -o test-fb.dwarfs

# Mount and verify
./build-dual/dwarfs test-fb.dwarfs mnt/ &
ls -la mnt/
fusermount -u mnt/

# Check integrity
./build-dual/dwarfsck test-fb.dwarfs

# Extract
./build-dual/dwarfsextract -i test-fb.dwarfs -o extract/
```

#### Update Official Documentation

**1. README.adoc** - Add metadata section:
```adoc
== Metadata Serialization

DwarFS supports two metadata serialization formats:

* *FlatBuffers* (modern, default, required) - Memory-mappable, zero-copy access, excellent portability
* *Thrift Compact* (legacy, optional) - Bit-packed layouts, backwards compatibility only

New filesystems use FlatBuffers by default. Thrift support is optional for reading old images.
```

**2. Create** `doc/metadata-formats.md`:
- Technical details of both formats
- Comparison table
- Migration guide
- Performance characteristics

**3. Update tool manuals**:
- `doc/mkdwarfs.md` - Document format (if selectable)
- `doc/dwarfs.md` - Document auto-detection
- `doc/dwarfsck.md` - Show format in output

#### Archive Temporary Documentation

Move to `old-docs/phase-work/`:
- `doc/PHASE_2_CONTINUATION_PROMPT_2025-11-22.md`
- `doc/PHASE_2_IMPLEMENTATION_STATUS_2025-11-22.md`
- `doc/PHASE_2_3_COMPLETION_ANALYSIS_2025-11-22.md`
- `doc/FULL_CONTINUATION_PLAN_2025-11-22.md`
- `doc/NEXT_SESSION_PROMPT_PHASE_2_4.md`

---

## Essential Reading (in order)

1. [`doc/PHASE_2_5_IMPLEMENTATION_STATUS.md`](PHASE_2_5_IMPLEMENTATION_STATUS.md) - Current status
2. [`doc/PHASE_2_5_CONTINUATION_PLAN.md`](PHASE_2_5_CONTINUATION_PLAN.md) - Detailed plan
3. [`include/dwarfs/reader/internal/metadata_view_interface.h`](../include/dwarfs/reader/internal/metadata_view_interface.h) - Existing interface
4. [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md) - System architecture

---

## Success Criteria

You are done when ALL of the following are true:

✅ **Build Success**:
- FlatBuffers-only build compiles without errors
- Dual-format build compiles (if Thrift available)
- Tebako build compiles
- Zero compilation warnings

✅ **Test Success**:
- All existing tests pass
- New interface tests pass (100% coverage)
- Factory tests pass
- Backend compatibility tests pass
- No memory leaks (valgrind clean)

✅ **Functional Success**:
- Can create FlatBuffers images with mkdwarfs
- Can mount FlatBuffers images with dwarfs
- Can check images with dwarfsck
- Can extract images with dwarfsextract
- Can read Thrift images (if dual-format)
- Format auto-detection works

✅ **Documentation Success**:
- README.adoc updated with metadata section
- `doc/metadata-formats.md` created
- Tool manuals updated
- Temporary docs archived

---

## Architectural Principles to Uphold

### Object-Oriented Design
- Pure virtual interfaces enable polymorphism ✅
- Each backend implements complete contract ✅
- No conditional compilation in high-level code ✅

### MECE (Mutually Exclusive, Collectively Exhaustive)
- Each backend exclusively handles its format ✅
- No overlap between implementations ✅
- Factory covers all format cases ✅

### Separation of Concerns
- Interface: defines contract ✅
- Backend: implements format ✅
- Factory: creates instances ✅
- Wrapper: provides user API ✅

### Open/Closed Principle
- Open for extension: new format = new backend class ✅
- Closed for modification: existing code unchanged ✅

### Single Responsibility
- Each component has one clear purpose ✅

---

## Important Reminders

### If Tests Fail After Changes

**DO NOT** lower pass thresholds or skip tests to make them pass.

**DO**:
1. Analyze if test expectations are outdated
2. Update tests to match correct behavior
3. Document behavior changes
4. Ensure architecture remains correct

**Principle**: Architecture correctness > test compatibility

### If Build Breaks

**Expected**: Initial builds will fail (duplicate definitions)

**Resolution Path**:
1. Delete duplicates (fixes most errors)
2. Align signatures (fixes remaining errors)
3. Update CMake (enables clean build)
4. Test incrementally

### If Performance Degrades

**Principle**: Correctness first, then optimize

If benchmarks show issues:
1. Profile to find hotspots
2. Optimize while preserving architecture
3. Don't sacrifice correctness for speed

---

## Troubleshooting Common Issues

### "undefined reference to vtable"
- **Cause**: Missing override implementations
- **Fix**: Ensure ALL interface methods implemented

### "cannot convert unique_ptr to shared_ptr"
- **Cause**: Return type mismatch
- **Fix**: Change method return type to shared_ptr

### "redefinition of interface"
- **Cause**: Duplicate interface files not deleted
- **Fix**: Delete 4 duplicate headers

### "file not found: inode_view_interface.h"
- **Cause**: CMake references deleted file
- **Fix**: Remove reference from CMakeLists.cmake

---

## After Completion

1. Update [`doc/PHASE_2_5_IMPLEMENTATION_STATUS.md`](PHASE_2_5_IMPLEMENTATION_STATUS.md) with 100% complete
2. Create `doc/PHASE_2_COMPLETION_SUMMARY.md` with final results
3. Update [`.kilocode/rules/memory-bank/context.md`](../.kilocode/rules/memory-bank/context.md)
4. Archive phase docs to `old-docs/phase-work/`
5. Create PR with all changes

---

**Branch**: `feature/multi-format-serialization-fuse`
**Base Directory**: `/Users/mulgogi/src/external/dwarfs`
**Current Progress**: 42% (Architecture complete, implementation needs alignment)
**Estimated Time**: 8-12 hours total (can be split)

---

**IMPORTANT**: The architecture is CORRECT. We're only aligning implementation with existing interfaces, not redesigning. Focus on making builds work first, then tests, then documentation.