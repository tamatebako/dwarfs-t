# Both-Format Build Fix - Implementation Status

**Started**: 2025-12-06  
**Last Updated**: 2025-12-07 17:23 HKT  
**Status**: 🟡 IN PROGRESS  

---

## Session 1: 2025-12-06 (5 hours)

### Completed ✅
1. **FlatBuffers Verification Investigation**
   - Discovered false alarm (old buggy images)
   - Verified verification code correct
   - Deleted old benchmark images
   - Status: ✅ RESOLVED

2. **Thrift Metadata Builder Fixes** (3 critical bugs)
   - Fixed uninitialized `inodes` vector (line 546)
   - Fixed uninitialized `directories` vector (line 548)
   - Fixed uninitialized `chunk_table` and `chunks` vectors (line 489-490)
   - Status: ✅ FIXED - Thrift-only build now works 100%

3. **Factory Routing Fix**
   - Updated `metadata_v2_factory.cpp` routing
   - Attempted FlatBuffers support in both-build
   - Status: ⚠️ PARTIAL - routing implemented but extraction fails

4. **Dual-Format Seek() Investigation**
   - Attempted sparse file seek() implementation
   - Caused hangs - reverted
   - Simplified to stub
   - Status: ⚠️ Still causes ENOTSUP errors

### Current Blockers 🔴
- Both-build extraction fails with `Operation not supported`
- Affects BOTH FlatBuffers AND native Thrift images
- Error occurs during/after extraction, not during metadata loading
- Extraction directory created but files not written

---

## Test Results Matrix

| Build | Format | Create | Extract | Files Written |
|-------|--------|--------|---------|---------------|
| fb-only | FlatBuffers | ✅ | ✅ | ✅ |
| thrift-only | Thrift | ✅ | ✅ | ✅ |
| both | FlatBuffers | ✅ | ❌ | ❌ |
| both | Thrift | ✅ | ❌ | ❌ |

---

## Files Modified (Session 1)

1. **src/writer/internal/thrift_metadata_builder.cpp**
   - Lines 546-549: Initialize inodes + directories
   - Lines 489-491: Initialize chunk_table + chunks
   - Status: ✅ WORKING (thrift-only build fixed)

2. **src/reader/internal/metadata_v2_factory.cpp**
   - Lines 72-86: Updated routing logic
   - Status: ⚠️ PARTIAL (routes correctly but extraction fails)

3. **src/reader/internal/metadata_v2_thrift.cpp**
   - Lines 1216-1230: seek() stub
   - Status: ⚠️ Returns ENOTSUP (correct per spec, but something calls it)

4. **benchmark-data/images/**
   - Deleted old buggy images
   - Status: ✅ COMPLETE

---

## Next Session Tasks

### Phase 1: Find Error Source (PRIORITY 1)

**Task**: Add comprehensive error logging to identify exact ENOTSUP source

**Files to instrument**:
1. `src/utility/filesystem_extractor.cpp`
   - Add logging around every `std::error_code` check
   - Log before archive_write_close()
   
2. `src/reader/internal/metadata_v2_thrift.cpp`
   - Log all method calls in dual-format paths
   - Especially seek(), get_chunks(), readdir()

3. `src/reader/filesystem_v2.cpp`
   - Log filesystem operations

**Commands**:
```bash
# Rebuild with logging
ninja -C build-both-bench

# Test with full logging
./build-both-bench/dwarfsextract -i /tmp/both-thrift.dwarfs \
  -o /tmp/test --log-level=trace 2>&1 | tee /tmp/both-debug.log

# Analyze
grep -n "not_supported\|ENOTSUP\|ERROR\|Operation not" /tmp/both-debug.log
```

### Phase 2: Fix Root Cause (PRIORITY 2)

Based on Phase 1 findings, apply appropriate fix from scenarios in continuation plan.

### Phase 3: Validate (PRIORITY 3)

Test all scenarios:
- Both-build + FlatBuffers image
- Both-build + Thrift image  
- Both-build + small dataset
- Both-build + compressed names (FSST)

---

## Known Issues to Check

1. **Chunk Iteration**: Does dual-format code properly use `chunk->` instead of `chunk.`?
2. **Archive Options**: Does libarchive call seek() even for dense files?
3. **Sparse File Flags**: Are sparse file archive flags being set incorrectly?
4. **String Table**: Are compact_names/symlinks (FSST) preserved correctly?

---

## Debug Commands

```bash
# Check if sparse features flag is set in image
./build-both-bench/dwarfsck -i /tmp/both-thrift.dwarfs --json | \
  grep -i sparse

# Try extraction without sparse support
./build-both-bench/dwarfsextract -i /tmp/both-thrift.dwarfs \
  -o /tmp/test 2>&1

# Check what archive options are used
# (Add debug output to filesystem_extractor.cpp)
```

---

## Success Criteria

- [ ] Both-build can extract FlatBuffers images
- [ ] Both-build can extract Thrift images
- [ ] No ENOTSUP errors
- [ ] No hangs or timeouts
- [ ] Files written correctly
- [ ] Ready for benchmarking

---

**Current Task**: Begin Phase 1 - Add logging to find exact error source

**ETA**: 4 hours to completion (1h debug + 2h fix + 1h validate)