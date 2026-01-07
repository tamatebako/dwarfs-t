# DwarFS Option C: Complete Backend Separation - Continuation Prompt

**Branch**: `feature/multi-format-serialization-fuse`
**Current Progress**: 25% (Foundation complete, type system separation needed)
**Next Phase**: Phase 1 - Create FlatBuffers-Specific Type System
**Cost So Far**: $21.79 USD

## Quick Context

DwarFS is implementing dual-format metadata support (FlatBuffers + Thrift). We chose **Option C: Complete Separation** for zero performance overhead. Currently have factory pattern working but backends share Thrift-centric types, causing linker errors.

## What Works ✅

1. **Build System** - Both backends compile ([`cmake/libdwarfs.cmake:330-360`](../cmake/libdwarfs.cmake))
2. **Runtime Factory** - Format detection and dispatch ([`src/reader/internal/metadata_v2_factory.cpp`](../src/reader/internal/metadata_v2_factory.cpp))
3. **Serialization** - Both formats can CREATE images perfectly
4. **Thrift Backend** - Fully functional (reads native Thrift images)

## What's Broken ❌

**FlatBuffers backend can't link** because it tries to use Thrift types:

```
Undefined symbols:
  string_table::string_table(std::span<std::string const>)
  inode_view_impl::inode_view_impl(InodeData const*, ...)
```

**Root Cause**: [`metadata_types.h`](../include/dwarfs/reader/internal/metadata_types.h) is Thrift-centric with these type assumptions:
- `inode_view_impl` wraps Thrift `View<InodeData>`
- `global_metadata` uses Thrift `MappedFrozen<metadata>`
- `string_table` expects Thrift `View<vector<string>>`

## The Solution (Option C)

Create **completely independent type systems** for each backend:

```
Thrift Backend                FlatBuffers Backend
└── thrift_backend::          └── flatbuffers_backend::
    ├── inode_view_impl           ├── inode_view_impl
    ├── dir_entry_view_impl       ├── dir_entry_view_impl
    ├── directory_view            ├── directory_view
    ├── chunk_view                ├── chunk_view
    └── global_metadata           └── global_metadata
```

**No shared types** = No conflicts = Zero virtual dispatch overhead

## Implementation Plan

**Full details**: [`doc/OPTION_C_COMPLETE_SEPARATION_PLAN.md`](OPTION_C_COMPLETE_SEPARATION_PLAN.md)
**Status tracker**: [`doc/OPTION_C_IMPLEMENTATION_STATUS.md`](OPTION_C_IMPLEMENTATION_STATUS.md)

### Phase 1: FlatBuffers Type System (NEXT - 4 hours)

**Step 1.1**: Create `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`

Copy structure from [`metadata_types_thrift.cpp`](../src/reader/internal/metadata_types_thrift.cpp) and replace:
- `View<InodeData>` → `::dwarfs::flatbuffers::InodeData const*`
- `View<Directory>` → `::dwarfs::flatbuffers::Directory const*`
- `MappedFrozen<metadata>` → `::dwarfs::flatbuffers::Metadata const*`

**Step 1.2**: Implement `src/reader/internal/metadata_types_flatbuffers.cpp`

Use FlatBuffers accessors:
- `inode->mode()` instead of `view.mode()`
- `dir->first_entry()` instead of `view.first_entry()`
- `meta->chunks()->Get(i)` instead of `view.chunks()[i]`

**Step 1.3**: Update `metadata_v2_flatbuffers.cpp`

Change namespace:
```cpp
// OLD:
using namespace dwarfs::reader::internal;

// NEW:
using namespace dwarfs::reader::internal::flatbuffers_backend;
```

**Step 1.4**: Test FlatBuffers-only build

```bash
cmake -B build-fb-only -DDWARFS_WITH_THRIFT=OFF -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build-fb-only
./build-fb-only/mkdwarfs -i testdata -o test.dff --format=flatbuffers
./build-fb-only/dwarfsextract -i test.dff -o out/
```

### Phase 2: Thrift Backend Isolation (PARALLEL - 2 hours)

**Step 2.1**: Wrap Thrift types in namespace

```cpp
// metadata_types_thrift.cpp
namespace dwarfs::reader::internal::thrift_backend {
  // All existing types here
}
```

**Step 2.2**: Update references in `metadata_v2_thrift.cpp`

### Phase 3-5: See full plan

## Files Modified So Far

**Working changes** (keep):
1. [`cmake/libdwarfs.cmake`](../cmake/libdwarfs.cmake) - Dual backend compilation
2. [`src/reader/internal/metadata_v2_factory.cpp`](../src/reader/internal/metadata_v2_factory.cpp) - Factory
3. [`include/dwarfs/reader/internal/metadata_v2.h`](../include/dwarfs/reader/internal/metadata_v2.h) - Friend declarations

**Problematic changes** (will be superseded by Phase 1):
4. [`src/reader/internal/metadata_v2_thrift.cpp`](../src/reader/internal/metadata_v2_thrift.cpp) - Conditional constructor (keep)
5. [`src/reader/internal/metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp) - Uses wrong types (will fix in Phase 1)

## Key Reference Files

**Template for FlatBuffers types**:
- [`src/reader/internal/metadata_types_thrift.cpp`](../src/reader/internal/metadata_types_thrift.cpp) - Copy structure from here

**FlatBuffers schema**:
- [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs) - Source of truth for FlatBuffers structures

**Existing FlatBuffers helpers**:
- [`include/dwarfs/reader/internal/flatbuffer_metadata_views.h`](../include/dwarfs/reader/internal/flatbuffer_metadata_views.h) - View helpers (already exist!)
- [`src/reader/internal/flatbuffer_metadata_views.cpp`](../src/reader/internal/flatbuffer_metadata_views.cpp) - Implementation

## Build Commands

```bash
# Current state (fails at link)
ninja -C build-dual mkdwarfs dwarfsck dwarfsextract

# After Phase 1 (should work)
cmake -B build-fb-only -DDWARFS_WITH_THRIFT=OFF -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build-fb-only

# After Phase 3 (full dual-format)
cmake -B build-dual -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build-dual
```

## Testing Protocol

After each phase:

```bash
# Phase 1 test:
./build-fb-only/mkdwarfs -i testdata -o test.dff --format=flatbuffers --file-hash=xxh3-128 -l1
./build-fb-only/dwarfsck test.dff
./build-fb-only/dwarfsextract -i test.dff -o out/
ls -R out/  # Should list all files

# Phase 2 test:
./build-thrift-only/mkdwarfs -i testdata -o test.dft --format=thrift --file-hash=xxh3-128 -l1
./build-thrift-only/dwarfsextract -i test.dft -o out/

# Phase 3 test:
./build-dual/mkdwarfs -i testdata -o test.dff --format=flatbuffers
./build-dual/mkdwarfs -i testdata -o test.dft --format=thrift
./build-dual/dwarfsextract -i test.dff -o out-fb/
./build-dual/dwarfsextract -i test.dft -o out-thrift/
diff -r out-fb/ out-thrift/  # Must be identical

# Phase 4 test (verify no conversion):
./build-dual/dwarfsck test.dff 2>&1 | grep -i "convert"
# Expected: No output (no conversion messages)
```

## Critical Design Decisions

### 1. Namespace Strategy

**Decision**: Use nested namespaces to isolate backends
```cpp
namespace dwarfs::reader::internal {
  namespace thrift_backend { /* Thrift types */ }
  namespace flatbuffers_backend { /* FlatBuffers types */ }
}
```

**Rationale**: Clear separation, prevents naming conflicts, easy to reason about

### 2. Type Independence

**Decision**: Zero shared type dependencies between backends

**Rationale**: Each backend optimized for its format, no abstraction penalty

### 3. Code Duplication

**Decision**: Accept ~2000 lines of duplicated view/accessor code

**Rationale**: Performance > maintainability for this hot path, formats won't change often

## Common Pitfalls to Avoid

1. **Don't try to share types** - Leads back to current problem
2. **Don't use type erasure** - Adds virtual dispatch overhead
3. **Don't mix Thrift/FlatBuffers includes** in same .cpp without guards
4. **DO use namespaces** to isolate types completely
5. **DO keep string_table generic** - Works with both via span/view

## Session Handoff Checklist

- [x] Factory pattern implemented
- [x] Both backends export factory functions
- [x] CMake compiles both backends
- [x] Implementation plan created
- [x] Status tracker created
- [x] Old docs moved
- [x] CHANGES.md updated
- [ ] **NEXT**: Create flatbuffers_backend namespace and types
- [ ] **THEN**: Test FlatBuffers-only build
- [ ] **FINALLY**: Integrate and test dual-format

## Estimated Remaining Work

| Phase | Time | Can Parallelize |
|-------|------|-----------------|
| 1. FlatBuffers types | 4h | Phase 2 |
| 2. Thrift isolation | 2h | Phase 1 |
| 3. Factory integration | 2h | - |
| 4. Remove conversion | 1h | - |
| 5. Docs & cleanup | 2h | - |
| **Total** | **~9h** | (with parallelization) |

## Quick Start for Next Session

```bash
cd /Users/mulgogi/src/external/dwarfs
git status  # Verify on feature/multi-format-serialization-fuse branch

# Read these files first:
cat doc/OPTION_C_COMPLETE_SEPARATION_PLAN.md  # Full implementation plan
cat doc/OPTION_C_IMPLEMENTATION_STATUS.md      # Current status

# Start Phase 1.1:
# Copy Thrift types as FlatBuffers template
cp src/reader/internal/metadata_types_thrift.cpp \
   src/reader/internal/metadata_types_flatbuffers_new.cpp

# Edit to use FlatBuffers types instead of Thrift types
# Then integrate into build system
```

## Success Criteria

- [ ] FlatBuffers-only build: compiles, creates images, extracts correctly
- [ ] Thrift-only build: compiles, creates images, extracts correctly
- [ ] Dual-format build: compiles, both formats work, no conversion
- [ ] Performance: No regression in benchmarks
- [ ] Tests: All unit/integration tests pass
- [ ] Docs: README.md, doc/*.md updated with format info
- [ ] Clean: No temporary files, clear git history

## Documentation Status

**Updated**:
- ✅ [`CHANGES.md`](../CHANGES.md) - v0.16.0 entry added
- ✅ [`doc/OPTION_C_COMPLETE_SEPARATION_PLAN.md`](OPTION_C_COMPLETE_SEPARATION_PLAN.md) - Full plan
- ✅ [`doc/OPTION_C_IMPLEMENTATION_STATUS.md`](OPTION_C_IMPLEMENTATION_STATUS.md) - Live status

**TODO after completion**:
- [ ] README.md - Add metadata formats section
- [ ] doc/mkdwarfs.md - Document --format option
- [ ] doc/dwarfs-format.md - FlatBuffers specification

**Archived**:
- ✅ Moved 18 old planning docs to [`old-docs/dual-format-attempts/`](../old-docs/dual-format-attempts/)

## Key Insights from This Session

1. **Factory pattern works** - Runtime dispatch implemented correctly
2. **Shared types don't work** - CMake `-UDWARFS_HAVE_THRIFT` breaks string_table
3. **Conversion is buggy** - FlatBuffers→Thrift loses compact_names data
4. **Need type independence** - Each backend must have its own complete type system
5. **Performance is critical** - DwarFS users expect zero-copy, high-speed metadata access

## Contact Points for Questions

- **Architecture**: See memory bank [`architecture.md`](../.kilocode/rules/memory-bank/architecture.md)
- **Metadata types**: Study [`metadata_types_thrift.cpp`](../src/reader/internal/metadata_types_thrift.cpp)
- **FlatBuffers schema**: Reference [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs)
- **Factory pattern**: Review [`metadata_v2_factory.cpp`](../src/reader/internal/metadata_v2_factory.cpp)

## Expected Timeline

- **Session 2** (4-5 hours): Complete Phase 1 (FlatBuffers types) + Phase 2 (Thrift isolation)
- **Session 3** (3-4 hours): Phase 3 (Factory integration) + Phase 4 (Remove conversion)
- **Session 4** (2-3 hours): Phase 5 (Documentation & cleanup) + Final testing

**Total**: 3 sessions, ~10-12 hours of focused work

---

**Continuation command**: Open this file and review the plan, then start with Phase 1.1