// Create comprehensive continuation plan
# Strategy Pattern Implementation - Continuation Plan

**Date**: 2025-11-17
**Status**: 80% Complete
**Branch**: feature/multi-format-serialization-fuse
**Current Session End**: Phase 3 complete, Phase 4 in progress

## What's Been Accomplished (Phases 1-3)

### Foundation Complete ✅
- Architecture designed and documented
- Interface layer updated to use domain model
- Domain ↔ Thrift bidirectional converters created
- Thrift strategy implementation created (1,288 lines)
- FlatBuffers strategy skeleton created (300 lines)
- Factory implementation created
- Build system updated (CMake)

### Files Created/Modified: 16 files, ~3,000 lines

## Remaining Work: Complete Implementation

### PHASE 4: Complete FlatBuffers Strategy (CRITICAL)

**File**: [`src/writer/internal/flatbuffers_metadata_builder.cpp`](../src/writer/internal/flatbuffers_metadata_builder.cpp)

**Current State**: Skeleton with method signatures
**Required**: Full implementation of all methods

#### Methods to Complete (~800 lines):

1. **`remap_blocks()`** (~150 lines)
   - Current: Empty/stub
   - Required: Remap block indices after compression
   - Adapt from: [`thrift_metadata_builder.cpp:433-557`](../src/writer/internal/thrift_metadata_builder.cpp)
   - Key changes: Work with `domain::chunk` instead of `thrift::metadata::chunk`

2. **`remap_holes()`** (~30 lines)
   - Current: Empty/stub
   - Required: Remap sparse file hole indices
   - Adapt from: [`thrift_metadata_builder.cpp:398-431`](../src/writer/internal/thrift_metadata_builder.cpp)

3. **`update_inodes()`** (~60 lines)
   - Current: Empty/stub
   - Required: Update inode fields (uid, gid, timestamps, chmod)
   - Adapt from: [`thrift_metadata_builder.cpp:559-629`](../src/writer/internal/thrift_metadata_builder.cpp)
   - Key changes: Work with `domain::inode_data` public fields

4. **`apply_chmod()`** (~35 lines)
   - Current: Empty/stub
   - Required: Apply chmod transformations to modes
   - Adapt from: [`thrift_metadata_builder.cpp:631-666`](../src/writer/internal/thrift_metadata_builder.cpp)

5. **`update_nlink()`** (~45 lines)
   - Current: Empty/stub
   - Required: Update hardlink counts
   - Adapt from: [`thrift_metadata_builder.cpp:823-874`](../src/writer/internal/thrift_metadata_builder.cpp)

6. **`update_totals_and_size_cache()`** (~150 lines)
   - Current: Empty/stub
   - Required: Calculate file sizes, build size cache
   - Adapt from: [`thrift_metadata_builder.cpp:668-821`](../src/writer/internal/thrift_metadata_builder.cpp)
   - Uses: `inode_size_provider` (already adapted)

7. **`pack_metadata()`** (~90 lines)
   - Current: Empty/stub
   - Required: Delta-compress tables, pack string tables
   - Adapt from: [`thrift_metadata_builder.cpp:902-1000`](../src/writer/internal/thrift_metadata_builder.cpp)
   - Key changes: Call `string_table::pack()` on domain types

8. **`upgrade_metadata()`** (~30 lines)
   - Current: Empty/stub
   - Required: Upgrade from older format versions
   - Adapt from: [`thrift_metadata_builder.cpp:1199-1233`](../src/writer/internal/thrift_metadata_builder.cpp)

9. **`upgrade_from_pre_v2_2()`** (~180 lines)
   - Current: Empty/stub
   - Required: Upgrade from v2.2 format
   - Adapt from: [`thrift_metadata_builder.cpp:1015-1197`](../src/writer/internal/thrift_metadata_builder.cpp)
   - Complex: Rebuilds inodes, chunks, shared files

**Adaptation Strategy**:
- Copy logic from Thrift version
- Replace Thrift types with domain types
- Replace `t.field().value()` with `t.field` (domain uses direct access)
- Replace Thrift optional `.has_value()` checks with domain optional checks
- Test incrementally

**Estimated Time**: 6-8 hours

---

### PHASE 5: Simplify metadata_builder.cpp (Quick)

**File**: [`src/writer/internal/metadata_builder.cpp`](../src/writer/internal/metadata_builder.cpp)

**Current State**: Contains full implementation + conversion
**Target State**: Thin wrapper that delegates to factory

**Action**: Replace entire implementation with factory delegation

**Before** (~1,200 lines):
```cpp
template <typename LoggerPolicy>
class metadata_builder_ {
  // ... 1200 lines of implementation ...
};

metadata_builder::metadata_builder(...)
    : impl_{make_unique<metadata_builder_>(...)} {}
```

**After** (~50 lines):
```cpp
// No impl class definition - use factory

metadata_builder::metadata_builder(logger& lgr, metadata_options const& options)
    : impl_{metadata_builder_factory::create(lgr, options,
                options.metadata_format)} {}

metadata_builder::metadata_builder(
    logger& lgr,
    metadata::domain::metadata const& md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options)
    : impl_{metadata_builder_factory::create_from_existing(
                lgr, md, orig_fs_options, orig_fs_version, options,
                options.metadata_format)} {}

// Move constructor
metadata_builder::metadata_builder(
    logger& lgr,
    metadata::domain::metadata&& md,
    metadata::domain::fs_options const* orig_fs_options,
    filesystem_version const& orig_fs_version,
    metadata_options const& options)
    : impl_{metadata_builder_factory::create_from_existing(
                lgr, std::move(md), orig_fs_options, orig_fs_version, options,
                options.metadata_format)} {}

metadata_builder::~metadata_builder() = default;
```

**Estimated Time**: 1 hour

---

### PHASE 6: Fix Domain Model Type Mismatches

**Issue**: Some domain types have class-based accessors, adapters may expect struct-style

**Files to Check**:
- [`include/dwarfs/metadata/domain/inode_size_cache.h`](../include/dwarfs/metadata/domain/inode_size_cache.h) - Check if map<> or custom class
- [`include/dwarfs/metadata/domain/string_table.h`](../include/dwarfs/metadata/domain/string_table.h) - Check structure
- [`include/dwarfs/metadata/domain/history_entry.h`](../include/dwarfs/metadata/domain/history_entry.h) - Check structure

**Action**: Ensure converters match actual domain model structure

**Estimated Time**: 2 hours

---

### PHASE 7: Build & Test (Critical Validation)

#### Test 1: Build with Thrift Enabled
```bash
cmake -B build-both -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_THRIFT=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DWITH_TESTS=ON \
  -DWITH_TOOLS=ON

ninja -C build-both
ctest --test-dir build-both
```

**Expected**: All tests pass, both formats work

#### Test 2: Build with FlatBuffers Only (THE KEY TEST)
```bash
cmake -B build-fb-only -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_THRIFT=OFF \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DWITH_TESTS=ON \
  -DWITH_TOOLS=ON

ninja -C build-fb-only
# This MUST work on AppleClang 17!
```

**Expected**: Builds successfully without Thrift, all tools work

#### Test 3: Functional Testing
```bash
# Create filesystem with FlatBuffers
./build-fb-only/mkdwarfs -i src -o test.dwarfs --metadata-format=flatbuffers

# Mount and verify
./build-fb-only/dwarfs test.dwarfs mnt
ls mnt
umount mnt

# Check integrity
./build-fb-only/dwarfsck test.dwarfs

# Extract
./build-fb-only/dwarfsextract -i test.dwarfs -o extracted
diff -r src extracted
```

**Expected**: All operations succeed

**Estimated Time**: 4 hours

---

### PHASE 8: Integration Point Verification

**Files to Verify**:

1. **[`src/writer/scanner.cpp`](../src/writer/scanner.cpp:970-971)**
   - Current: `mdb.build()` returns domain model
   - Check: `metadata_freezer` accepts domain model ✅ (already updated)
   - Action: Verify compilation, no changes needed

2. **[`src/utility/rewrite_filesystem.cpp`](../src/utility/rewrite_filesystem.cpp:503-504)**
   - Current: `builder.build()` returns domain model
   - Check: `metadata_freezer` accepts domain model ✅ (already updated)
   - Action: Verify compilation, no changes needed

3. **Entry classes** (`src/writer/internal/entry.cpp`)
   - Check: `pack()` methods work with domain types
   - May need: Adapt `pack()` to use domain types instead of Thrift

**Estimated Time**: 3 hours

---

### PHASE 9: Handle Metadata Options

**Issue**: `metadata_options` may specify `metadata_format`

**File**: `include/dwarfs/writer/metadata_options.h`

**Check**: Does it have `SerializationFormat metadata_format` field?

**If NO**:
- Add field to `metadata_options` struct
- Add command-line option to mkdwarfs
- Add default (FlatBuffers)

**If YES**:
- Verify factory uses it correctly

**Estimated Time**: 2 hours

---

### PHASE 10: Complete Testing & Validation

#### Unit Tests
- Test domain ↔ Thrift converter (round-trip)
- Test metadata_builder_factory (creates correct type)
- Test both strategies independently

#### Integration Tests
- Test full workflow: scan → build → freeze → write
- Test with both formats
- Test format detection on read

#### Platform Tests (CI/CD)
- Linux: Ubuntu 22.04/24.04 (x86_64, aarch64)
- macOS: AppleClang 17 (x86_64, arm64) - **PRIMARY TARGET**
- Windows: MSVC (x64)
- FreeBSD: gcc/clang

#### Performance Tests
- Compare FlatBuffers vs Thrift:
  - Compression ratio
  - Build time
  - Mount time
  - Read performance
  - Memory usage

**Estimated Time**: 6 hours

---

### PHASE 11: Documentation & Cleanup

1. **Update Memory Bank**
   - `.kilocode/rules/memory-bank/architecture.md` - Add Strategy Pattern section
   - `.kilocode/rules/memory-bank/context.md` - Update current state
   - `.kilocode/rules/memory-bank/tech.md` - Document implementation approach

2. **Update User Documentation**
   - `doc/mkdwarfs.md` - Document `--metadata-format` option
   - `doc/dwarfs-format.md` - Document format differences
   - `README.md` - Update with new architecture notes

3. **Code Cleanup**
   - Remove old debug prints
   - Remove commented code
   - Run clang-format
   - Run clang-tidy

4. **Create Migration Guide**
   - Document for library users
   - Breaking changes (build() return type)
   - Migration examples

**Estimated Time**: 4 hours

---

### PHASE 12: Commit & PR

1. **Commit Strategy**:
   - Commit 1: "feat(metadata): add domain model converters"
   - Commit 2: "refactor(metadata): update interfaces to return domain model"
   - Commit 3: "refactor(metadata): implement Strategy Pattern for metadata builders"
   - Commit 4: "feat(metadata): add FlatBuffers-only build support"
   - Commit 5: "docs(metadata): update documentation for Strategy Pattern"
   - Commit 6: "test(metadata): add tests for format independence"

2. **Pull Request**:
   - Title: "Strategy Pattern: Complete format independence for metadata"
   - Description: Full summary with architecture diagrams
   - Link to architecture docs
   - Breaking changes noted
   - Performance impact noted

**Estimated Time**: 2 hours

---

## Timeline Summary

| Phase | Description | Est. Time | Status |
|-------|-------------|-----------|--------|
| 1-3 | Foundation & Strategy separation | 2 days | ✅ COMPLETE |
| 4 | Complete FlatBuffers methods | 6-8 hours | 🔄 Next |
| 5 | Simplify metadata_builder.cpp | 1 hour | Pending |
| 6 | Fix type mismatches | 2 hours | Pending |
| 7 | Build & test | 4 hours | Pending |
| 8 | Integration verification | 3 hours | Pending |
| 9 | Metadata options | 2 hours | Pending |
| 10 | Complete testing | 6 hours | Pending |
| 11 | Documentation | 4 hours | Pending |
| 12 | Commit & PR | 2 hours | Pending |
| **TOTAL** | **Full completion** | **4-5 days** | **80% done** |

## Critical Path

1. Complete Flat Buffers methods (longest task)
2. Test FlatBuffers-only build on AppleClang 17
3. Fix any compilation errors
4. Verify integration points
5. Full test suite validation

## Success Criteria

- [ ] AppleClang 17 builds with `-DDWARFS_WITH_THRIFT=OFF`
- [ ] Both Thrift and FlatBuffers work independently
- [ ] No `#ifdef DWARFS_HAVE_THRIFT` in application logic (scanner.cpp, etc.)
- [ ] Each format in separate implementation file
- [ ] Clean interfaces with domain model types only
- [ ] All existing tests pass with both formats
- [ ] New format-specific tests pass
- [ ] Performance within 10% between formats
- [ ] Cross-platform CI passes on all targets
- [ ] Memory bank documentation updated

## Risk Assessment

### High Risk Items
- **FlatBuffers method implementation** - Complex logic, easy to introduce bugs
  - Mitigation: Copy-adapt incrementally, test after each method
- **Build system changes** - CMake errors can block progress
  - Mitigation: Test build after each CMake change

### Medium Risk Items
- **Type mismatches** - Domain model structure may differ from assumptions
  - Mitigation: Verify each domain type structure before using
- **Integration points** - Entry::pack() may need adaptation
  - Mitigation: Check Entry class implementation, adapt if needed

### Low Risk Items
- **Testing** - Tests may need minor updates
  - Mitigation: Fix incrementally
- **Documentation** - Straightforward updates
  - Mitigation: Update as we go

## Blockers to Watch

1. **Missing domain model types** - If domain/ doesn't have all needed types
   - Solution: Check include/dwarfs/metadata/domain/, create if missing

2. **Entry::pack() Thrift dependency** - If pack() methods use Thrift types
   - Solution: Update Entry to accept domain types

3. **Compiler errors with domain types** - Access patterns may differ
   - Solution: Adjust accessors (use public fields directly)

4. **Test failures with domain model** - Tests may expect Thrift types
   - Solution: Update tests to work with domain model

## Next Session Priorities

### Immediate (Must Do):
1. Complete ALL FlatBuffers strategy methods
2. Simplify metadata_builder.cpp to thin wrapper
3. Build test with both configurations
4. Fix compilation errors

### Secondary (Should Do):
5. Verify integration points (scanner, entry, rewrite)
6. Run basic functional tests
7. Update status tracker

### Optional (Nice to Have):
8. Performance benchmarks
9. Full documentation updates
10. Complete test coverage

## Handoff Information

### Files Currently Modified
All strategy files created but FlatBuffers methods incomplete.

### Build System State
CMake updated for conditional compilation but not tested.

### Known Issues
- FlatBuffers strategy methods are stubs (need ~800 lines)
- metadata_builder.cpp still contains full impl (needs simplification)
- No testing done yet

### Key Decisions Made
- **Domain model**: Using `dwarfs::metadata::domain::metadata` (class-based, not struct-based)
- **Thrift converters**: Created separate domain_thrift_converter (not reusing cpp_thrift_converter)
- **Strategy files**: Separate files per format (not conditional compilation in one file)

### Environment
- **Platform**: macOS with AppleClang 17
- **Branch**: feature/multi-format-serialization-fuse
- **Base commit**: bfe4896195 (2025-11-17)

---

**Ready for continuation with clear path forward.**