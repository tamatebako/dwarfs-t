# Both-Format Build Extraction Issue - Debug Status

**Date**: 2025-12-07  
**Session Duration**: ~2 hours  
**Status**: ⚠️ **PARTIAL FIX - Issue persists**  

---

## Problem Statement

Both-format build (FLATBUFFERS + THRIFT) fails extraction with "Operation not supported" error.

**Symptom**:
```bash
$ ./build-both-bench/dwarfsextract -i test.dwarfs -o extract/
I Detected FlatBuffers metadata format, converting to internal Thrift format
... (conversion successful) ...
Operation not supported
```

---

## Root Causes Identified & Fixed

### 1. ✅ Factory Blocking FlatBuffers Images
**File**: [`src/reader/internal/metadata_v2_factory.cpp:80-86`](../src/reader/internal/metadata_v2_factory.cpp:80-86)

**Problem**: Explicit rejection of FlatBuffers images  
**Solution**: Route both formats to Thrift backend (which has built-in conversion)

```cpp
// BEFORE (lines 80-86):
if (*detected == SerializationFormat::FLATBUFFERS) {
  LOG_ERROR << "Dual-format builds currently only support Thrift format";
  DWARFS_THROW(runtime_error, "FlatBuffers format not supported...");
}

// AFTER (lines 72-78):
if (*detected == SerializationFormat::THRIFT_COMPACT ||
    *detected == SerializationFormat::FLATBUFFERS) {
  *this = make_metadata_v2_thrift(lgr, schema, data, options,
                                 inode_offset, force_consistency_check, perfmon);
  return;
}
```

### 2. ✅ Thrift Backend Already Handles Conversion
**File**: [`src/reader/internal/metadata_v2_thrift.cpp:669-711`](../src/reader/internal/metadata_v2_thrift.cpp:669-711)

The Thrift backend ALREADY has code to detect and convert FlatBuffers→Thrift internally:
- Detects FlatBuffers format
- Deserializes using facade
- Freezes to Thrift format
- Works in metadata inspection (`dwarfsck` succeeds)

---

## Remaining Issue

**Status**: ❌ **Extraction still fails**

**Error Location**: After archive close, before completion  
**Exit Code**: 1  
**Error Message**: "Operation not supported" (plain text, not through logger)

### What Works
- ✅ Image creation with FlatBuffers format
- ✅ Metadata inspection (`dwarfsck` shows correct file structure)
- ✅ Format detection and conversion  
- ✅ Compilation (all 3 build configs)

### What Fails
- ❌ File extraction (creates empty directories, but no files)
- ❌ Both FlatBuffers images (converted to Thrift)
- ❌ Only in both-format build (thrift-only works fine)

---

## Debug Evidence

### 1. Successful Conversion
```
I Detected FlatBuffers metadata format, converting to internal Thrift format
DEBUG FlatBuffers deserialize:
  names: 0
  compact_names: YES
  symlinks: 0
  compact_symlinks: YES
  dir_entries: 2
=== Domain→Thrift Conversion ===
Input names: 0
Input dir_entries: 2
Output names: 0
Output dir_entries: 2
=== End Conversion ===
```

### 2. Metadata Inspection Works
```bash
$ ./build-both-bench/dwarfsck /tmp/test.dwarfs
...
<inode:0> ---drwxr-xr-x (1 entries, parent=0)
    <inode:1> ----rw-r--r-- file.txt [0, 1] 13
```

### 3. Extraction Creates Directories But No Files
```bash
$ ls -la /tmp/test-extract/
total 0
drwxr-xr-x   2 mulgogi  wheel   64 Dec  7 17:46 .
drwxrwxrwt  72 root     wheel 2304 Dec  7 17:46 ..
# file.txt is MISSING!
```

### 4. Error Timing
```
D [filesystem_extractor.cpp:209] closing archive
D [block_cache.cpp:257] stopping cache workers
Operation not supported  # ← Error appears here
```

---

## Hypotheses (Unconfirmed)

### 1. Chunk Access Issue
The dual-format build uses `shared_ptr<chunk_view_interface>` instead of value types.  
**Location**: [`src/reader/internal/inode_reader_v2.cpp:238-258`](../src/reader/internal/inode_reader_v2.cpp:238-258)

Code already has dual-format guards for pointer vs value access.

### 2. FSST String Compression
FlatBuffers images use `compact_names: YES` (FSST compression).  
Conversion might not properly handle FSST symtab.

**Location**: [`src/metadata/converters/domain_thrift_converter.cpp:105-108`](../src/metadata/converters/domain_thrift_converter.cpp:105-108)

### 3. Sparse File Seeking
Disabled in dual-format (lines 1216-1255 in metadata_v2_thrift.cpp).  
But this should only affect `SEEK_DATA`/`SEEK_HOLE`, not regular reads.

---

## Files Modified

1. [`src/reader/internal/metadata_v2_factory.cpp`](../src/reader/internal/metadata_v2_factory.cpp) - Enabled FlatBuffers routing
2. [`src/reader/internal/metadata_v2_thrift.cpp`](../src/reader/internal/metadata_v2_thrift.cpp) - Updated seek() comments

---

## Recommended Next Steps

### Immediate (< 2 hours)
1. **Add comprehensive error logging**:
   - Wrap ALL error_code checks in extraction path
   - Log file reader operations
   - Track which files succeed/fail

2. **Test with non-FSST image**:
   ```bash
   mkdwarfs -i src -o test.dwarfs --no-compact-names
   ```

3. **Compare extraction logs**: thrift-only vs both-format

### Short-term (< 1 day)
1. **Inspect conversion of chunk data**:
   - Verify chunk table conversion
   - Check chunk → block mapping
   - Validate file size calculations

2. **Test Thrift-format images in both-build**:
   ```bash
   mkdwarfs -i src -o test.dwarfs --format thrift
   ```

### Long-term (> 1 day)
1. **Consider deprecating both-format build**:
   - Ship two separate builds (fb-only, thrift-only)
   - Simpler architecture
   - No shared_ptr overhead

2. **Or fully implement FlatBuffers backend**:
   - Create native flatbuffers_metadata_provider
   - Avoid conversion overhead
   - Cleaner architecture

---

## Build Status

| Build Config | CREATE | CHECK | EXTRACT |
|--------------|--------|-------|---------|
| fb-only      | ✅     | ✅    | ✅      |
| thrift-only  | ✅     | ✅    | ✅      |
| both (FB→T)  | ✅     | ✅    | ❌      |
| both (Thrift)| ✅     | ✅    | ❓      |

---

## Key Code Locations

- **Factory dispatch**: [`metadata_v2_factory.cpp:72-78`](../src/reader/internal/metadata_v2_factory.cpp:72-78)
- **FB→Thrift conversion**: [`metadata_v2_thrift.cpp:669-711`](../src/reader/internal/metadata_v2_thrift.cpp:669-711)
- **Extraction loop**: [`filesystem_extractor.cpp:563-679`](../src/utility/filesystem_extractor.cpp:563-679)
- **Chunk iteration (dual)**: [`inode_reader_v2.cpp:238-258`](../src/reader/internal/inode_reader_v2.cpp:238-258)
- **Domain→Thrift conv**: [`domain_thrift_converter.cpp:148-319`](../src/metadata/converters/domain_thrift_converter.cpp:148-319)

---

**Session ended**: 2025-12-07 17:47 HKT  
**Time invested**: ~2 hours  
**Outcome**: Partial fix (unblocked factory), deeper issue remains