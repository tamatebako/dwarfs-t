# Phase 1 Continuation - Quick Start Guide
**Date**: 2025-11-22 | **Branch**: `feature/multi-format-serialization-fuse` | **Progress**: 95% Phase 1

## 🎯 Immediate Goal

Fix 3 remaining compile errors in `metadata_v2_flatbuffers.cpp` to complete FlatBuffers backend implementation.

## 📁 Essential Context Files

**Read First** (in order):
1. [`doc/PHASE_1_STATUS_2025-11-22.md`](PHASE_1_STATUS_2025-11-22.md) - Detailed status & fixes
2. [`doc/PHASE_1_CONTINUATION_2025-11-22.md`](PHASE_1_CONTINUATION_2025-11-22.md) - Complete roadmap
3. [`doc/OPTION_C_COMPLETE_SEPARATION_PLAN.md`](OPTION_C_COMPLETE_SEPARATION_PLAN.md) - Original plan

## 🚀 Quick Start Commands

```bash
cd /Users/mulgogi/src/external/dwarfs
git status  # Verify on feature/multi-format-serialization-fuse branch

# Verify current file state
wc -l src/reader/internal/metadata_v2_flatbuffers.cpp
# Should be 2381 lines (if different, restore from backup)

# Read status
cat doc/PHASE_1_STATUS_2025-11-22.md
```

## 🔧 Three Fixes Needed

All in [`src/reader/internal/metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp):

### Fix #1: Add Missing `dump()` Wrapper (Line ~1705)
```cpp
void metadata_v2_data::dump(
    std::ostream& os, fsinfo_options const& opts, filesystem_info const* fsinfo,
    std::function<void(std::string const&, uint32_t)> const& icb) const {
  dump(os, "", root_, opts, icb);
}
```
**Insert After**: End of previous `dump()` method (~line 1695)

### Fix #2: Update `global_` Member Type (Line ~716)
```cpp
// Change from:
global_metadata const global_;

// To:
fb::global_metadata const global_;
```

### Fix #3: Add `get_chunks()` Implementation (Before namespace closing)
```cpp
fb::chunk_range metadata_v2::get_chunks(int inode, std::error_code& ec) const {
  return impl_->get_chunks(inode, ec);
}
```
**Insert Before**: `} // namespace dwarfs::reader::internal`

## ✅ Verification Steps

```bash
# Apply fixes using edit_file tool (safer than sed for critical changes)

# Build
cd build-fb-test
ninja mkdwarfs dwarfsck dwarfsextract

# Test
./mkdwarfs -i testdata -o test.dff --format=flatbuffers --file-hash=xxh3-128 -l1
./dwarfsck test.dff
./dwarfsextract -i test.dff -o out/
diff -r testdata/ out/  # Must be identical
```

## 📊 Complete Work Scope

**Phase 1** (Current): FlatBuffers backend - 95% done, 1-2 hours remaining
**Phase 2** (Next): Thrift backend isolation - 2 hours
**Phase 3** (Then): Dual-format integration - 2 hours
**Phase 4** (After): Remove conversion - 1 hour
**Phase 5** (Final): Testing + Documentation - 3 hours

**Total Remaining**: ~9 hours, ~$60-80 USD

See [PHASE_1_CONTINUATION_2025-11-22.md](PHASE_1_CONTINUATION_2025-11-22.md) for complete details.

## 🎓 Key Insights

1. **Namespace Separation Works**: `fb::` prefixes compile cleanly
2. **Architecture is Sound**: Option C proving correct
3. **Almost There**: Just 3 small fixes needed
4. **Testing Critical**: Don't skip functional verification
5. **Documentation Essential**: Update official docs (Phase 5)

## 🔄 Restore Command (If Needed)

```bash
cp src/reader/internal/metadata_v2_flatbuffers.cpp.orig \
   src/reader/internal/metadata_v2_flatbuffers.cpp
```

## 📝 TODO Summary

- [x] Create FlatBuffers backend namespace
- [ ] Fix 3 compile errors
- [ ] Test FlatBuffers-only build end-to-end
- [ ] Implement Thrift backend namespace (Phase 2)
- [ ] Integrate both backends (Phase 3)
- [ ] Remove conversion code (Phase 4)
- [ ] Write tests + update all documentation (Phase 5)

**Next Action**: Apply the 3 fixes documented in PHASE_1_STATUS_2025-11-22.md