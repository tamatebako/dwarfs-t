# FlatBuffers Verification Issue

**Date**: 2025-12-05  
**Status**: 🔴 BLOCKING - Extraction Fails  
**Scope**: Affects ALL FlatBuffers images

---

## Problem

All extraction operations fail with:
```
[metadata_v2_flatbuffers.cpp:128] FlatBuffers metadata verification failed
```

### Symptoms

- ✅ CREATE operations: 100% success (images created correctly)
- ❌ EXTRACT operations: 100% failure (verification fails on read)
- Affects: Both `fb-only` and `both` builds
- Affects: Both FlatBuffers and Thrift formats (when read by FlatBuffers-enabled builds)

### Verification Code

**Location**: [`src/reader/internal/metadata_v2_flatbuffers.cpp:123-130`](../src/reader/internal/metadata_v2_flatbuffers.cpp:123)

```cpp
::dwarfs::flatbuffers::Metadata const* map_frozen(std::span<uint8_t const> data) {
  ::flatbuffers::Verifier verifier(data.data(), data.size());
  if (!verifier.VerifySizePrefixedBuffer<::dwarfs::flatbuffers::Metadata>("DFBF")) {
    DWARFS_THROW(runtime_error, "FlatBuffers metadata verification failed");
  }
  return ::flatbuffers::GetSizePrefixedRoot<::dwarfs::flatbuffers::Metadata>(data.data());
}
```

**Writer**: [`src/metadata/serialization/flatbuffers_serializer.cpp:343`](../src/metadata/serialization/flatbuffers_serializer.cpp:343)

```cpp
builder.FinishSizePrefixed(metadata_offset, "DFBF");
```

---

## Root Cause (Hypothesis)

The `data` span passed to `map_frozen()` may not include the size prefix and/or file identifier that `VerifySizePrefixedBuffer()` expects.

### Expected Format

When using `FinishSizePrefixed()`, the buffer layout is:
```
[4-byte size][4-byte "DFBF" identifier][flatbuffers data]
```

### Current Issue

The section extraction in [`filesystem_v2.cpp`](../src/reader/filesystem_v2.cpp) may be:
1. Skipping the size prefix before passing to verifier
2. Passing data starting after the "DFBF" identifier
3. Not including the full size-prefixed buffer

---

## Investigation Needed

### 1. Check Section Extraction

Find where section data is extracted and passed to metadata reader:
```bash
cd /Users/mulgogi/src/external/dwarfs
grep -n "METADATA" src/reader/filesystem_v2.cpp
grep -n "section.*data\|metadata_v2" src/reader/filesystem_v2.cpp
```

### 2. Verify Buffer Format

Dump the first bytes of metadata section:
```bash
# This would require adding debug output to see the actual bytes
# being passed to the verifier
```

### 3. Check for Format Mismatch

The verifier expects:
- `VerifySizePrefixedBuffer()` needs `[size][id][data]`
- But we might be passing just `[data]` or `[id][data]`

---

## Potential Solutions

### Option 1: Fix Verification Call

If section data doesn't include size prefix, use regular verify:
```cpp
::flatbuffers::Verifier verifier(data.data(), data.size());
if (!verifier.VerifyBuffer<::dwarfs::flatbuffers::Metadata>("DFBF")) {
  DWARFS_THROW(runtime_error, "FlatBuffers metadata verification failed");
}
return ::flatbuffers::GetRoot<::dwarfs::flatbuffers::Metadata>(data.data());
```

### Option 2: Fix Section Extraction

Ensure section includes the full size-prefixed buffer when extracting:
```cpp
// In filesystem_v2.cpp where section is extracted
auto metadata_section = sections[METADATA]; // Should include size prefix
```

### Option 3: Adjust Writer

Change writer to use `Finish()` instead of `FinishSizePrefixed()`:
```cpp
// In flatbuffers_serializer.cpp
builder.Finish(metadata_offset, "DFBF");  // No size prefix
```

---

## Next Steps

1. **Immediate**: Determine exact buffer format being passed to verifier
2. **Fix**: Apply appropriate solution (likely Option 1 or 2)
3. **Test**: Verify extraction works
4. **Validate**: Re-run benchmark suite

---

## Impact

- **Thrift-only build**: ✅ FIXED (unaffected by this issue)
- **Both build**: ❌ BLOCKED (cannot extract FlatBuffers images)
- **fb-only build**: ❌ BLOCKED (cannot extract any images)
- **Release v0.16.0**: ❌ BLOCKED (critical bug)

---

**Status**: 🔴 CRITICAL - Must fix before release  
**Priority**: URGENT  
**ETA**: 1-2 hours (after investigation)