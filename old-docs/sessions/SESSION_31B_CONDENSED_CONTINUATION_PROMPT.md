// ... existing code ...

# Session 31B Continuation Prompt (CONDENSED)

**Date**: 2025-12-22+
**Status**: Ready for Phase 1 + 2 combined sprint
**Deadline**: AGGRESSIVE - Must finish in 2-3 sessions

## Quick Context

You're continuing Session 31 of the DwarFS metadata architecture migration. Session 31A completed the critical blocker (domain view implementations). Now we're doing an AGGRESSIVE sprint to finish faster.

### What's Complete (Session 31A - 3 hours)

✅ **Domain View Implementations** (~700 lines)
- `include/dwarfs/reader/internal/domain_metadata_views.h` (230 lines)
- `src/reader/internal/domain_metadata_views.cpp` (470 lines)
- All 5 view classes implemented and working

### Current State

**File Ready**: `src/reader/internal/common_metadata_operations.cpp`
- Status: 14/40 methods complete (426 lines)
- Blocker resolved: Can now use domain views
- Target: Complete all 40 methods

## Mission: Session 31B (4-5 hours)

**COMBINE Phases 1 + 2** in ONE session:
1. Complete `common_metadata_operations.cpp` (26 remaining methods, 2-3h)
2. Create both format adapters (2 files, 1-2h)

### Part A: Complete Phase 1 (2-3 hours)

**File**: `src/reader/internal/common_metadata_operations.cpp`

**Reference Architecture Doc**: `doc/SESSION_31_ARCHITECTURE_CORRECT.md`

**Implementation Order** (26 methods):

1. **Lookup Methods** (5 methods, 30 min):
   ```cpp
   dir_entry_view root() const override;
   std::optional<dir_entry_view> find(std::string_view path) const override;
   std::optional<inode_view> find(int inode) const override;
   std::optional<dir_entry_view> find(int inode, std::string_view name) const override;
   ```

2. **File Operations** (6 methods, 45 min):
   ```cpp
   file_stat getattr(inode_view iv, std::error_code& ec) const override;
   file_stat getattr(inode_view iv, getattr_options const& opts, std::error_code& ec) const override;
   std::optional<directory_view> opendir(inode_view iv) const override;
   std::optional<dir_entry_view> readdir(directory_view dir, size_t offset) const override;
   size_t dirsize(directory_view dir) const override;
   void access(inode_view iv, int mode, file_stat::uid_type uid, file_stat::gid_type gid, std::error_code& ec) const override;
   int open(inode_view iv, std::error_code& ec) const override;
   ```

3. **Chunk & Link Operations** (2 methods, 30 min):
   ```cpp
   chunk_range get_chunks(int inode, std::error_code& ec) const override;
   std::string readlink(inode_view iv, readlink_mode mode, std::error_code& ec) const override;
   ```

4. **Traversal Methods** (2 methods, 45 min):
   ```cpp
   void walk(std::function<void(dir_entry_view)> const& func) const override;
   void walk_data_order(std::function<void(dir_entry_view)> const& func) const override;
   ```

5. **Serialization Methods** (4 methods, 30 min):
   ```cpp
   void dump(std::ostream& os, fsinfo_options const& opts, filesystem_info const* fsinfo, ...) const override;
   nlohmann::json info_as_json(fsinfo_options const& opts, filesystem_info const* fsinfo) const override;
   nlohmann::json as_json() const override;
   std::string serialize_as_json(bool simple) const override;
   ```

6. **Thrift Legacy Methods** (3 methods, 15 min):
   ```cpp
   std::unique_ptr<thrift::metadata::metadata> thaw() const override;
   std::unique_ptr<thrift::metadata::metadata> unpack() const override;
   std::unique_ptr<thrift::metadata::fs_options> thaw_fs_options() const override;
   ```

7. **Sparse File Operations** (1 method, 15 min):
   ```cpp
   file_off_t seek(uint32_t inode, file_off_t offset, seek_whence whence, std::error_code& ec) const override;
   ```

**Implementation Strategy**:
- Use domain views from Phase 1A
- Reference `src/reader/internal/metadata_v2_thrift.cpp` for logic patterns (lines 299-2400)
- ALL code must be format-agnostic
- Work ONLY with `domain_meta_` member variable
- Compile after each group to catch errors early

### Part B: Create Format Adapters (1-2 hours)

**Goal**: Thin deserializers (ONLY deserialize to domain, nothing else)

#### File 1: `src/reader/internal/thrift_metadata_adapter.cpp` (30 min)

```cpp
#include "common_metadata_operations.h"
#include <dwarfs/metadata/converters/domain_thrift_converter.h>

namespace dwarfs::reader::internal {

metadata_v2 make_metadata_v2_thrift(
    logger& lgr, std::span<uint8_t const> schema,
    std::span<uint8_t const> data, metadata_options const& options,
    int inode_offset, bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon) {

  // 1. Map frozen Thrift
  auto frozen = map_frozen<thrift::metadata::metadata>(schema, data);

  // 2. Convert to domain using EXISTING converter (Session 28)
  auto domain = metadata::converters::from_thrift(frozen.thaw());

  // 3. Create common operations
  metadata_v2 result;
  result.impl_ = std::make_unique<common_metadata_operations>(
      lgr, std::move(domain), options, inode_offset,
      force_consistency_check, perfmon);
  return result;
}

} // namespace
```

#### File 2: `src/reader/internal/flatbuffers_metadata_adapter.cpp` (30 min)

```cpp
#include "common_metadata_operations.h"
#include <dwarfs/metadata/converters/domain_flatbuffers_converter.h>

namespace dwarfs::reader::internal {

metadata_v2 make_metadata_v2_flatbuffers(
    logger& lgr, std::span<uint8_t const> data,
    metadata_options const& options, int inode_offset,
    bool force_consistency_check,
    std::shared_ptr<performance_monitor const> const& perfmon) {

  // 1. Deserialize FlatBuffers
  auto root = GetSizePrefixedMetadata(data.data());

  // 2. Convert to domain using EXISTING converter (Session 28)
  auto domain = metadata::converters::from_flatbuffers(root);

  // 3. Create common operations
  metadata_v2 result;
  result.impl_ = std::make_unique<common_metadata_operations>(
      lgr, std::move(domain), options, inode_offset,
      force_consistency_check, perfmon);
  return result;
}

} // namespace
```

**Critical**: Use EXISTING converters from Session 28! Don't recreate them.

## Next Session: 31C (3-4 hours)

Will handle:
- Phase 3: Wire up factory (30 min)
- Phase 4: Update build system (30 min)
- Phase 5: Test all configurations (2-3 hours)

## Critical Reminders

1. **Domain Model Only**: ALL operations work on `metadata::domain::metadata`
2. **No Format Access**: Do NOT access frozen Thrift or FlatBuffers in common operations
3. **Use Existing Converters**: Session 28 created domain ↔ format converters
4. **Thin Adapters**: Adapters ONLY deserialize, nothing else
5. **Test Incrementally**: Compile after each method group

## Files Reference

**To Modify**:
- `src/reader/internal/common_metadata_operations.cpp` (currently 426 lines → target ~700 lines)

**To Create**:
- `src/reader/internal/thrift_metadata_adapter.cpp` (~100 lines)
- `src/reader/internal/flatbuffers_metadata_adapter.cpp` (~100 lines)

**Reference Files**:
- `src/reader/internal/metadata_v2_thrift.cpp` (lines 299-2400) - for logic patterns
- `doc/SESSION_31_ARCHITECTURE_CORRECT.md` - architecture reference
- `include/dwarfs/reader/internal/domain_metadata_views.h` - view interfaces
- `include/dwarfs/metadata/domain/metadata.h` - domain model structure

## Time Budget

- Session 31A: ✅ 3 hours (domain views)
- **Session 31B**: 4-5 hours (THIS SESSION - common ops + adapters)
- Session 31C: 3-4 hours (wire up + test)
- Session 31D: 1 hour (delete + commit)
- **Total**: 11-13 hours

## Success Criteria for Session 31B

- ✅ All 40 methods in `common_metadata_operations.cpp` implemented
- ✅ Both adapter files created and working
- ✅ Code compiles (basic test)
- ✅ Ready for Session 31C integration

---

**Read `doc/SESSION_31B_CONDENSED_CONTINUATION_PLAN.md` for full details.**

**Start by implementing the lookup methods (5 methods, 30 min) first.**

// ... existing code ...