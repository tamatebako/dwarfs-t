# FlatBuffers Metadata Fix - Implementation Status

**Date**: 2025-11-30  
**Branch**: `refactor/dwarfs-mkdwarfs-complete`  
**Status**: Phase A Complete ✅

---

## Architecture Overview

The FlatBuffers metadata serialization follows a clean **Strategy Pattern** with complete separation of concerns:

```
┌─────────────────────────────────────────────────────────┐
│           Serialization Strategy Pattern                │
└─────────────────────────────────────────────────────────┘
                          │
              ┌───────────┴───────────┐
              ▼                       ▼
    ┌──────────────────┐    ┌──────────────────┐
    │ Writer Strategy  │    │ Reader Strategy  │
    │                  │    │                  │
    │ Serializer       │    │ Deserializer     │
    │ (domain → bytes) │    │ (bytes → domain) │
    └──────────────────┘    └──────────────────┘
              │                       │
              ▼                       ▼
    ┌──────────────────┐    ┌──────────────────┐
    │ FlatBuffers API  │    │ FlatBuffers API  │
    │ FinishSizePfx()  │    │ VerifySizePfx()  │
    │ + file ID "DFBF" │    │ GetSizePfxRoot() │
    └──────────────────┘    └──────────────────┘
```

---

## Phase A: Verification Fix - Complete ✅

### Issue #1: Format Mismatch (Line 127)

**Problem**: Writer and reader used incompatible FlatBuffers verification methods

**Root Cause Analysis**:
- **Writer** ([`flatbuffers_serializer.cpp:343`](../src/metadata/serialization/flatbuffers_serializer.cpp:343)):
  ```cpp
  builder.FinishSizePrefixed(metadata_offset, "DFBF");
  ```
  Creates: `[4-byte size][DFBF identifier][flatbuffers data]`

- **Reader** ([`metadata_v2_flatbuffers.cpp:126`](../src/reader/internal/metadata_v2_flatbuffers.cpp:126) - BEFORE):
  ```cpp
  if (!::dwarfs::flatbuffers::VerifyMetadataBuffer(verifier)) {
  ```
  Expected: `[DFBF identifier][flatbuffers data]` (no size prefix)

**OOP Solution**: Fixed reader to match writer's format contract:
```cpp
// AFTER (Line 126-129)
if (!verifier.VerifySizePrefixedBuffer<::dwarfs::flatbuffers::Metadata>("DFBF")) {
  DWARFS_THROW(runtime_error, "FlatBuffers metadata verification failed");
}
return ::flatbuffers::GetSizePrefixedRoot<::dwarfs::flatbuffers::Metadata>(data.data());
```

**Files Modified**:
- [`src/reader/internal/metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp:123-130)

---

### Issue #2: Missing Compact Names Support (Line 74)

**Problem**: Reader expected uncompressed `names` but mkdwarfs writes compressed `compact_names`

**Root Cause Analysis**:
- mkdwarfs applies FSST compression to name strings
- Compressed names stored in `compact_names` field, plain `names` array is empty
- Consistency check only validated plain `names` array

**OOP Solution**: Applied **Adapter Pattern** with proper encapsulation:

1. **Consistency Check** - Accept both formats:
   ```cpp
   // Line 73-77
   auto compact_names = meta->compact_names();
   if ((!names || names->size() == 0) && !compact_names) {
     DWARFS_THROW(runtime_error, "no names in metadata");
   }
   ```

2. **Constructor** - Proper domain model construction:
   ```cpp
   // Line 35-68
   global_metadata::global_metadata(logger& lgr, Meta const& meta)
       : meta_{meta}
       , names_{[&]() -> dwarfs::internal::string_table {
           if (auto compact_names = meta->compact_names()) {
             // Build domain model string_table
             metadata::domain::string_table st;
             st.buffer = compact_names->buffer()->str();
             if (compact_names->symtab()) {
               // CRITICAL: Binary FSST data, preserve all bytes
               st.symtab = std::string(compact_names->symtab()->data(), 
                                       compact_names->symtab()->size());
             }
             st.index.assign(compact_names->index()->begin(), 
                            compact_names->index()->end());
             st.packed_index = compact_names->packed_index();
             return dwarfs::internal::string_table(lgr, "names", st);
           } else if (auto names = meta->names()) {
             // Plain names fallback
             return /* ... */;
           }
         }()} {
   }
   ```

**Files Modified**:
- [`src/reader/internal/metadata_types_flatbuffers.cpp`](../src/reader/internal/metadata_types_flatbuffers.cpp:35-80)

---

## Test Results

### Build Status
```bash
$ ninja -C build-fb mkdwarfs dwarfsck
[23/23] Linking CXX executable mkdwarfs
✅ SUCCESS - 0 errors, 0 warnings (except harmless rpath)
```

### Functional Tests
```bash
# Create filesystem image
$ ./build-fb/mkdwarfs -i /tmp/test-all-formats -o /tmp/test-fixed.dwarfs
✅ SUCCESS - Image created: 1.0K

# Verify filesystem
$ ./build-fb/dwarfsck /tmp/test-fixed.dwarfs
DwarFS version 2.5 [2]
History:
  1: libdwarfs v0.14.1-153-g931a24c89f-dirty
<inode:0> ---drwxr-xr-x (1 entries, parent=0)
    <inode:1> ----rw-r--r-- [file content]
✅ SUCCESS - Verification passed, filesystem readable
```

---

## Architecture Principles Applied

### 1. **Strategy Pattern** (Serialization)
- Pluggable serialization strategies (FlatBuffers, Thrift)
- Each strategy encapsulates its format-specific logic
- Clean separation: Writer strategy vs Reader strategy

### 2. **Adapter Pattern** (String Tables)
- Adapts FlatBuffers `compact_names` to `string_table` interface
- Transparent to client code using string_table API
- Handles plain vs compressed transparently

### 3. **Single Responsibility Principle**
- `flatbuffers_serializer.cpp`: Serialization only
- `metadata_v2_flatbuffers.cpp`: Deserialization only
- `metadata_types_flatbuffers.cpp`: Type conversions only
- `global_metadata`: Metadata access interface only

### 4. **Open/Closed Principle**
- Adding new serialization format = new strategy implementation
- No modification of existing serializer strategies
- Registry pattern for format detection and selection

### 5. **Dependency Inversion**
- High-level: domain model (`metadata::domain::*`)
- Low-level: FlatBuffers wire format
- Both depend on abstractions (strategy interfaces)

---

## Phase B: Size Optimization - Pending

**Next Steps**:
1. Investigate packing options application
2. Compare sizes: FlatBuffers vs Thrift
3. Verify FSST compression effectiveness
4. Ensure delta compression for chunk tables

**Target**: FlatBuffers images ≤110% of Thrift size (currently acceptable at 1KB for test data)

---

## Phase C: Documentation Update - Pending

**Tasks**:
1. Update [`README.md`](../README.md) metadata formats section
2. Create [`doc/flatbuffers-format.md`](../doc/) specification
3. Move temporary docs to [`doc/old-docs/`](../doc/old-docs/)
4. Update memory bank with final architecture

---

## Commits

### Phase A Completion
```bash
git add -A
git commit -m "fix(metadata): Resolve FlatBuffers verification failures

Fix size-prefixed buffer mismatch and compact_names support.

Phase A Complete:
- Use VerifySizePrefixedBuffer() to match FinishSizePrefixed()
- Add file identifier 'DFBF' verification
- Support compact_names deserialization with FSST
- Apply Adapter Pattern for string table construction

Verification now passes, filesystem images readable.

Fixes: #1, #2 in CRITICAL_FIXES_CONTINUATION_PLAN.md"
```

---

## References

- **Schema**: [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs:375) - File identifier "DFBF"
- **Serializer**: [`flatbuffers_serializer.cpp`](../src/metadata/serialization/flatbuffers_serializer.cpp:343)
- **Deserializer**: [`metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp:123-130)
- **Types**: [`metadata_types_flatbuffers.cpp`](../src/reader/internal/metadata_types_flatbuffers.cpp:35-80)