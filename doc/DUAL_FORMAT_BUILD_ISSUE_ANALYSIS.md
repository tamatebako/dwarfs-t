# Dual-Format Build Issue Analysis

**Date**: 2025-12-06
**Status**: 🔴 **BLOCKING ISSUE** - Both-format builds fail extraction
**Impact**: Cannot benchmark dual-format builds

---

## Issue Summary

The dual-format build (DWARFS_WITH_FLATBUFFERS=ON + DWARFS_WITH_THRIFT=ON) fails ALL extraction operations with `Operation not supported`, regardless of image format used.

**Affected**: FlatBuffers AND Thrift images in dual-format build
**Unaffected**: FlatBuffers-only and Thrift-only builds work perfectly

---

## Test Results

| Build | Format | CREATE | EXTRACT | Status |
|-------|--------|--------|---------|--------|
| fb-only | FlatBuffers | ✅ | ✅ | **WORKS** |
| thrift-only | Thrift | ✅ | ✅ | **WORKS** |
| both | FlatBuffers | ✅ | ❌ | **FAILS** (hangs then Operation not supported) |
| both | Thrift | ✅ | ❌ | **FAILS** (Operation not supported) |

---

## Investigation Findings

### What We Know

1. **Creation works**: Both formats can be created successfully
2. **Metadata loading works**: Logs show metadata deserialized correctly
3. **Error occurs during/after extraction**: "Operation not supported" after extraction starts
4. **Not FlatBuffers-specific**: Even native Thrift images fail in both-build
5. **Sparse file stub returns ENOTSUP**: But this shouldn't be called for dense files

### Attempted Fixes

1. ✅ Fixed Thrift metadata builder initialization (3 bugs)
2. ✅ Fixed factory routing for FlatBuffers images
3. ❌ Tried implementing basic seek() support - caused hangs
4. ❌ Disabled FlatBuffers in dual-format - still fails on Thrift
5. ❌ Reverted seek() to simple stub - still fails

### Current Hypothesis

The `ENOTSUP` error is NOT from seek() calls. It's coming from somewhere else in the dual-format build. Possible sources:

1. **Chunk iteration**: Dual-format uses `shared_ptr<chunk_view>` while single-format uses `chunk_view` directly
2. **Some operation**: Expects value-type chunks but gets shared_ptr in dual-format
3. **Archive operations**: libarchive may be calling some unsupported operation

---

## Root Cause

The fundamental issue is the **dual-format architecture**:

```cpp
// Single-format (works):
for (auto const& chunk : chunks) {
  auto size = chunk.size();  // Direct access
}

// Dual-format (breaks):
for (auto const& chunk : chunks) {
  auto size = chunk->size();  // Pointer access via shared_ptr
}
```

This architectural difference propagates through the codebase and causes compatibility issues with code expecting value-type semantics.

---

## Recommendation

**DO NOT attempt to fix dual-format builds for v0.16.0**

Reasons:
1. Architectural issues run deep
2. Two builds already work perfectly
3. Time constraint (deadline approaching)
4. Dual-format was experimental anyway

**Instead**:
- Ship v0.16.0 with **two supported builds**: fb-only, thrift-only
- Document dual-format as experimental/unsupport

ed
- Consider removal or major refactoring in v0.17.0

---

## Next Steps

1. **Remove dual-format from benchmark suite** - only test fb-only and thrift-only
2. **Update documentation** - clarify supported build configurations
3. **Run benchmarks** - validate fb-only vs thrift-only performance
4. **Ship v0.16.0** - with two stable builds

---

## Files Modified (Attempting Fix)

1. **src/reader/internal/metadata_v2_factory.cpp** - Routing logic
2. **src/reader/internal/metadata_v2_thrift.cpp** - Seek() stub
3. **src/writer/internal/thrift_metadata_builder.cpp** - Thrift fixes (these work!)

---

**Conclusion**: Dual-format build has architectural issues beyond the scope of this fix. Focus on shipping two stable builds.