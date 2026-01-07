# Session 31J Continuation Plan: Complete metadata_v2 Implementation

**Date**: 2025-12-23 (planned)
**Previous Session**: 31I - Blocked by incomplete migration
**Objective**: Implement domain-based metadata_v2::impl to unblock tool builds
**Estimated Duration**: 4-6 hours

## Executive Summary

Session 31I discovered that the domain-based migration (31E-31H) is incomplete. The old backend implementations were disabled but never replaced with domain-based equivalents. This session will complete the migration by implementing:

1. `metadata_v2::impl` subclass using domain operations
2. `metadata_v2_utils` delegation methods
3. Full tool build and validation

## Background

### What Sessions 31E-31H Accomplished

✅ **Created domain-based common operations**:
- `common_metadata_operations.cpp` (1,325 lines)
- `domain_metadata_views.cpp` (350 lines)
- Format adapters (FlatBuffers, Thrift)

✅ **Architectural purity**:
- No type casts
- Clean iterator types
- Proper const-correctness

### What's Missing

❌ **`metadata_v2::impl` concrete implementation**
❌ **`metadata_v2` constructor**
❌ **`metadata_v2_utils` methods**

### Why Old Backends Don't Work

Session 31H's architectural changes broke compatibility:
- Return type changed: `flatbuffers_backend::chunk_range` → `domain_chunk_range_impl`
- Const-correctness: `shared_ptr<T>` → `shared_ptr<const T>`

## Session 31J Work Plan

### Phase 1: Design Domain-Based Implementation (30 min)

**Objective**: Design `metadata_v2::impl` subclass architecture

**Key Decisions**:
1. **Storage**: How to store domain metadata + adapter
2. **Delegation**: How to delegate to `common_metadata_operations`
3. **Format handling**: Single vs dual-format support

**Design Pattern**:
```cpp
class domain_metadata_impl : public metadata_v2::impl {
  // Store domain model
  metadata::domain::metadata meta_;

  // Store adapter for format-specific operations
  std::unique_ptr<metadata_adapter_interface> adapter_;

  // Store common operations instance
  std::unique_ptr<common_metadata_operations> ops_;

  // Implement all virtual methods by delegating to ops_
};
```

**Files to Create**:
- `include/dwarfs/reader/internal/domain_metadata_impl.h`
- `src/reader/internal/domain_metadata_impl.cpp`

### Phase 2: Implement Core Methods (2 hours)

**Objective**: Implement all metadata_v2::impl virtual methods

**Method Categories**:

1. **Navigation** (8 methods):
   - `walk()`, `walk_data_order()`
   - `root()`, `find()` (3 overloads)
   - `readdir()`, `dirsize()`

2. **Attributes** (4 methods):
   - `getattr()` (2 overloads)
   - `access()`, `open()`

3. **File Operations** (4 methods):
   - `seek()`, `readlink()`
   - `get_chunks()`, `opendir()`

4. **Statistics** (6 methods):
   - `size()`, `block_size()`, `statvfs()`
   - `has_symlinks()`, `has_sparse_files()`
   - `check_consistency()`

5. **Metadata Queries** (7 methods):
   - `get_inode_info()`
   - `get_block_category()`, `get_block_category_metadata()`
   - `get_all_block_categories()`, `get_block_numbers_by_category()`
   - `get_all_uids()`, `get_all_gids()`

6. **Serialization** (5 methods):
   - `dump()`, `info_as_json()`, `as_json()`
   - `serialize_as_json()`, `thaw()`, `unpack()`, `thaw_fs_options()`

**Implementation Strategy**:
- Most methods delegate to `common_metadata_operations`
- Format-specific operations delegate to adapter
- JSON/serialization may need adapter-specific handling

### Phase 3: Implement metadata_v2_utils (1 hour)

**Objective**: Implement metadata_v2_utils delegation class

**Files to Modify**:
- `src/reader/internal/metadata_v2.cpp`

**Methods to Implement** (6 total):
```cpp
metadata_v2_utils::metadata_v2_utils(metadata_v2 const& meta)
  : impl_(*meta.impl_) {}

void metadata_v2_utils::dump(...) const {
  impl_.dump(...);
}

// ... 5 more delegation methods
```

### Phase 4: Implement metadata_v2 Constructor (30 min)

**Objective**: Create metadata_v2 constructor that instantiates domain_metadata_impl

**Files to Modify**:
- `src/reader/internal/metadata_v2.cpp`

**Implementation**:
```cpp
metadata_v2::metadata_v2(
    logger& lgr, span<uint8_t const> schema,
    span<uint8_t const> data, metadata_options const& options,
    int inode_offset, bool force_consistency_check,
    shared_ptr<performance_monitor const> const& perfmon)
  : impl_(make_unique<domain_metadata_impl>(
        lgr, schema, data, options, inode_offset,
        force_consistency_check, perfmon)) {}
```

### Phase 5: Build & Test Tools (1 hour)

**Objective**: Verify all tools build successfully

**Commands**:
```bash
ninja -C build-fb-clean mkdwarfs dwarfsck dwarfsextract dwarfs
```

**Expected**: 0 compilation errors, 0 link errors

**If Failures**:
- Analyze linker errors
- Implement missing methods
- Fix type mismatches

### Phase 6: Unit Testing (1 hour)

**Objective**: Validate correctness of domain-based implementation

**Commands**:
```bash
ninja -C build-fb-clean dwarfs_unit_tests
ctest --test-dir build-fb-clean --output-on-failure
ctest --test-dir build-fb-clean -R metadata --verbose
```

**Success Criteria**:
- All tests pass
- No regressions
- Metadata round-trip works

### Phase 7: Integration Testing (1 hour)

**Objective**: Verify byte-for-byte correctness

**Test 1**: Create FlatBuffers image
```bash
./build-fb-clean/mkdwarfs -i /usr/bin -o test-31j.dff \
  --compression=zstd:level=3
```

**Test 2**: Validate
```bash
./build-fb-clean/dwarfsck test-31j.dff --check-integrity
```

**Test 3**: Extract & verify
```bash
mkdir -p extracted-31j
./build-fb-clean/dwarfsextract -i test-31j.dff -o extracted-31j/
# Verify files match originals
```

**Success Criteria**:
- Image creates successfully
- Integrity check passes
- Extracted files match originals byte-for-byte

### Phase 8: Cleanup & Commit (30 min)

**Delete Old Backends** (if all tests pass):
```bash
git rm src/reader/internal/metadata_v2_flatbuffers.cpp  # 2,516 lines
git rm src/reader/internal/metadata_v2_thrift.cpp       # 2,470 lines
git rm src/reader/internal/metadata_types_flatbuffers.cpp  # 1,151 lines
git rm src/reader/internal/metadata_types_thrift.cpp    # 1,151 lines
```

**Update CMake**:
- Remove old backend files from `cmake/libdwarfs.cmake`

**Git Commit**:
```bash
git add -A
git commit -m "feat(metadata): Complete domain-based metadata_v2 implementation

ARCHITECTURAL COMPLETION:
- Implemented domain_metadata_impl as metadata_v2::impl
- Implemented metadata_v2_utils delegation
- Deleted 7,288 lines of backend-specific code
- Unified implementation via common_metadata_operations

IMPLEMENTATION:
- Added: domain_metadata_impl.{h,cpp}
- Modified: metadata_v2.cpp (constructor + utils)
- Deleted: 4 backend files (7,288 lines)
- Net reduction: ~5,600 lines (-79%)

TESTING:
- All tools build: ✅ PASS
- Unit tests: ✅ PASS
- Integration tests: ✅ PASS
- Byte-for-byte verification: ✅ PASS

This completes the domain-based metadata migration started in
Sessions 31E-31H, delivering a clean, maintainable architecture.

Sessions: 31E-31J (Domain Migration Complete)
Ref: doc/SESSION_31I_CRITICAL_BLOCKER_STATUS.md
"
```

## Implementation Details

###domain_metadata_impl Constructor

```cpp
domain_metadata_impl::domain_metadata_impl(
    logger& lgr,
    span<uint8_t const> schema,
    span<uint8_t const> data,
    metadata_options const& options,
    int inode_offset,
    bool force_consistency_check,
    shared_ptr<performance_monitor const> const& perfmon) {

  // 1. Detect format (FlatBuffers or Thrift)
  auto format = detect_format(data);

  // 2. Create appropriate adapter
  if (format == Format::FlatBuffers) {
    adapter_ = make_flatbuffers_adapter(lgr, data, options);
  } else {
    adapter_ = make_thrift_adapter(lgr, schema, data, options);
  }

  // 3. Load domain model via adapter
  meta_ = adapter_->load_metadata();

  // 4. Create common operations
  ops_ = make_unique<common_metadata_operations>(
      meta_, inode_offset, force_consistency_check);

  // 5. Consistency check if requested
  if (force_consistency_check) {
    check_consistency();
  }
}
```

### Method Delegation Pattern

```cpp
dir_entry_view domain_metadata_impl::root() const {
  return ops_->root();
}

optional<dir_entry_view> domain_metadata_impl::find(string_view path) const {
  return ops_->find(path);
}

chunk_range domain_metadata_impl::get_chunks(int inode, error_code& ec) const {
  return ops_->get_chunks(inode, ec);
}

// ... repeat for all methods
```

## Risk Mitigation

### High Risk: Incorrect delegation
**Mitigation**: Comprehensive testing at each step
**Fallback**: Fix delegation logic before proceeding

### Medium Risk: Type mismatches
**Mitigation**: Careful type checking during implementation
**Fallback**: Update type aliases if needed

### Low Risk: Performance regression
**Mitigation**: Profile if issues arise
**Fallback**: Optimize hot paths

## Success Criteria

**Must-Have**:
- [ ] All tools build with 0 errors
- [ ] All unit tests pass
- [ ] Integration tests show byte-for-byte correctness
- [ ] Old backends deleted

**Should-Have**:
- [ ] Performance within 5% of baseline
- [ ] Code coverage maintained
- [ ] All warnings addressed

**Nice-to-Have**:
- [ ] Performance improvements observed
- [ ] Memory usage reduced

## Timeline Estimates

| Phase | Estimated | Notes |
|-------|-----------|-------|
| 1. Design | 30 min | Architecture decisions |
| 2. Core Methods | 2 hours | 30+ method implementations |
| 3. Utils | 1 hour | 6 delegation methods |
| 4. Constructor | 30 min | Instantiation logic |
| 5. Build/Test Tools | 1 hour | Build + fix issues |
| 6. Unit Tests | 1 hour | Run + fix failures |
| 7. Integration | 1 hour | Full workflow testing |
| 8. Cleanup | 30 min | Delete + commit |
| **Total** | **6 hours** | With buffer for issues |

## Dependencies

**Prerequisites** (from Session 31H):
- ✅ `common_metadata_operations.cpp` exists (1,325 lines)
- ✅ `domain_metadata_views.cpp` exists (350 lines)
- ✅ Format adapters exist
- ✅ Type system aligned to domain types

**New Files to Create**:
1. `include/dwarfs/reader/internal/domain_metadata_impl.h`
2. `src/reader/internal/domain_metadata_impl.cpp`

**Files to Modify**:
1. `src/reader/internal/metadata_v2.cpp` - Add constructor + utils
2. `cmake/libdwarfs.cmake` - Remove old backend entries (after validation)

## Next Session Start

Read this document and begin with Phase 1: Design Domain-Based Implementation.

---

**Last Updated**: 2025-12-23 14:34 HKT
**Status**: Ready for execution
**Estimated Duration**: 6 hours
**Prerequisites**: Session 31I blocker documented