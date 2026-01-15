# Chunk Size Fix Plan for libdwarfs

## Problem Statement

Files served by static-site-server are truncated because `chunk.size` stores **compressed sizes** instead of **uncompressed sizes**.

**Evidence from dwarfs-rs**:
- In the Rust implementation, `Chunk.size` is documented as "size of chunk, in bytes" (uncompressed)
- File sizes are computed by summing `chunk.size` values
- The block cache handles decompression separately from chunk metadata

**Current behavior in libdwarfs**:
- `/11339-cover.png`: Uncompressed size = 77263 bytes, but `chunk[2].size = 3518` (compressed)
- `get_file_size()` sums chunk sizes → returns 3518 instead of 77263
- `read_string()` limits reads to chunk sizes → truncates files

## Root Cause Analysis

The issue is in the **writer's chunk creation code**. Chunks are being created with compressed sizes instead of uncompressed sizes.

### Key Files to Investigate

| File | Purpose |
|------|---------|
| `src/writer/internal/fragment_chunkable.cpp` | Creates chunks from file data |
| `src/writer/internal/flatbuffers_chunk_processor.cpp` | Processes chunks for FlatBuffers |
| `src/writer/internal/flatbuffers_metadata_builder.cpp` | Builds metadata with chunk info |
| `src/writer/internal/chunk.h` | Chunk structure definition |

### Code Path: File → Chunks → Metadata

```
scan_file() [fragment_chunkable.cpp]
    ↓
process_chunks() [flatbuffers_chunk_processor.cpp]
    ↓
create_chunk() [sets chunk.size = compressed_size ?]
    ↓
build() [flatbuffers_metadata_builder.cpp]
    ↓
serialize [flatbuffers_serializer.cpp]
```

## Fix Implementation Plan

### Phase 1: Verify Root Cause (Diagnostic)

1. Add debug output to `flatbuffers_chunk_processor.cpp` to log chunk sizes at creation time
2. Compare chunk.size with actual uncompressed file size
3. Verify whether the issue is in chunk creation or serialization

**Target file**: `src/writer/internal/flatbuffers_chunk_processor.cpp`

```cpp
// In process_data_chunk() or similar function:
LOG_DEBUG << "Chunk created: block=" << chunk.block
          << ", offset=" << chunk.offset
          << ", size=" << chunk.size  // Should be uncompressed!
          << ", compressed_size=" << compressed_size;
```

### Phase 2: Fix Chunk Size Assignment

The fix needs to ensure `chunk.size` stores the **uncompressed** size of the data.

**Location**: `src/writer/internal/flatbuffers_chunk_processor.cpp`

**Current (incorrect)**:
```cpp
chunk.size = compressed_data_size;  // WRONG
```

**Correct**:
```cpp
chunk.size = original_data_size;  // UNCOMPRESSED size
```

### Phase 3: Update Reader (if needed)

The reader code in `src/reader/internal/inode_reader_v2.cpp` should work correctly once chunks have proper uncompressed sizes:

1. `get_file_size()` - sums chunk sizes → returns correct uncompressed size
2. `read_string()` - reads chunk.size bytes from each chunk → gets full data
3. Block cache - handles decompression transparently

### Phase 4: Rebuild and Test

1. Rebuild mkdwarfs
2. Re-create aesop.dff with fixed mkdwarfs
3. Test file downloads match original files byte-for-byte

## Technical Details

### Chunk Structure (from flatbuffers/metadata.fbs)

```flatbuffers
table Chunk {
  block:uint32;    // filesystem block number
  offset:uint32;   // byte offset within decompressed block
  size:uint32;     // UNCOMPRESSED size of chunk data
}
```

### Expected Behavior

| Field | Value | Meaning |
|-------|-------|---------|
| `block` | Physical block number in DwarFS image | Where compressed data is stored |
| `offset` | Byte offset in decompressed block | Where chunk data starts |
| `size` | Uncompressed byte size | How many bytes the chunk decompresses to |

### How Reader Uses This

1. `get_file_size()`: Sum all `chunk.size` values for the file's chunks
2. `read_string()`: Read `chunk.size` bytes from each chunk
3. Block cache: Decompress block data, extract `offset..offset+size` range

## Files to Modify

### 1. `src/writer/internal/flatbuffers_chunk_processor.cpp`

**Add debug output**:
```cpp
#include <iostream>

// In the function that creates chunks (around line 100-150):
std::cerr << "DEBUG create_chunk: chunk.size=" << chunk.size
          << ", expected_uncompressed=" << expected_size << std::endl;
```

**Fix chunk.size assignment**:
Find where `chunk.size` is set and ensure it uses the uncompressed data size.

### 2. `src/reader/internal/domain_metadata_impl.cpp`

**Clean up debug output** (temporary, for verification):
Remove or reduce the verbose debug statements added during investigation.

### 3. `src/metadata/serialization/flatbuffers_serializer.cpp`

**Already fixed**: Added `reg_file_size_cache` deserialization (though not used in existing images).

## Verification Steps

1. Create test image with known file sizes
2. Use `dwarfsck` to inspect metadata:
   ```bash
   ./build/dwarfsck --export-metadata=/tmp/meta.json test.dff
   grep -A5 '"chunks"' /tmp/meta.json | head -20
   ```
3. Verify chunk sizes are uncompressed sizes
4. Download file and compare with original:
   ```bash
   diff original_file downloaded_file
   ```

## Alternative: Use Size Cache

If fixing chunk sizes is too risky, we could:

1. Populate `reg_file_size_cache` with correct uncompressed sizes during write
2. Reader uses cache instead of summing chunk sizes

This requires changes to:
- `src/writer/internal/flatbuffers_metadata_builder.cpp` - populate cache
- `src/reader/internal/domain_metadata_impl.cpp` - use cache when available

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| Breaking existing images | Chunk.size fix changes on-disk format; old images still work if reader handles both |
| Performance impact | Uncompressed sizes are same data volume, just different semantics |
| Build breakage | Test with existing unit tests before committing |

## Success Criteria

- [ ] Test 6 (`/11339-cover.png` diff) passes
- [ ] All 8 tests in test.sh pass
- [ ] Downloaded files match original files byte-for-byte
- [ ] Existing unit tests still pass

---

## Investigation Notes (2026-01-10)

### Code Path Analysis

After extensive tracing through the codebase, I found the writer code path **appears correct**:

1. **segmenter.cpp:finish_chunk()** - Creates chunks with uncompressed sizes:
   ```cpp
   chkable.add_chunk(block.num(), frames_to_bytes(chunk_.offset_in_frames),
                     frames_to_bytes(chunk_.size_in_frames));
   ```
   The `chunk_.size_in_frames` is based on data added to the block via `append_bytes()`.

2. **inode_manager.cpp:append_chunks_to()** - Copies chunks from fragments:
   ```cpp
   chk.set_block(src.block());
   chk.set_offset(src.offset());
   chk.set_size(src.size());  // Copies uncompressed size
   ```

3. **flatbuffers_serializer.cpp:create_chunk()** - Serializes directly:
   ```cpp
   return dwarfs::flatbuffers::CreateChunk(builder, c.block(), c.offset(), c.size());
   ```

### Where the Problem Occurs

The existing `aesop.dff` image was likely created with a buggy version of mkdwarfs, or there's a code path that transforms chunk sizes during block remapping.

**Key insight from block_cache.cpp**:
- `cached_block_` stores `uncompressed_size_ = decompressor_->uncompressed_size()`
- This is the **actual** decompressed size of the block
- But the chunk sizes in metadata are wrong

### Next Steps

1. **Rebuild mkdwarfs from clean state**
2. **Add debug output to writer** to trace chunk sizes during creation
3. **Re-create aesop.dff** with new mkdwarfs
4. **Verify chunk sizes** in new image using `dwarfsck --export-metadata`
5. **If new image has correct sizes** - problem was the old image
6. **If new image still has wrong sizes** - need to find the transformation code

### Files Involved in Chunk Size Flow

| File | Role |
|------|------|
| `src/writer/segmenter.cpp` | Creates chunks from file data |
| `src/writer/internal/inode_manager.cpp` | Copies chunks to metadata |
| `src/writer/internal/flatbuffers_chunk_processor.cpp` | Remaps blocks, preserves chunk sizes |
| `src/metadata/serialization/flatbuffers_serializer.cpp` | Serializes chunks to FlatBuffers |
| `src/reader/internal/domain_metadata_impl.cpp` | Deserializes chunks from FlatBuffers |
| `src/reader/internal/inode_reader_v2.cpp` | Uses chunk sizes for reading |
