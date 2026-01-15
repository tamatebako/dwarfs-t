# DwarFS Metadata Deserialization & Writer Fix - Implementation Plan

**Date**: 2026-01-10
**Status**: CRITICAL - Both reader and writer broken
**Goal**: Fix static-site-server to serve files correctly with proper text/binary differentiation

---

## Executive Summary

### Critical Issues Identified

1. **Legacy Thrift Deserializer is Wrong Format** ⚠️
   - Current: `legacy_metadata_serializer.cpp` tries to deserialize **Thrift Compact** format
   - Actual: Homebrew mkdwarfs v0.14.1 creates **Frozen2** (bit-packed) format
   - Impact: Cannot read ANY Homebrew-generated images (100% failure rate)
   - Error: "Expected LIST for uids" at `legacy_metadata_serializer.cpp:326`

2. **Local mkdwarfs Crashes** 🔥
   - Assertion: `(fsb->size() <= worst_case_block_size_)` at `filesystem_writer.cpp:188`
   - Impact: Cannot create test images with local build
   - Root cause: Recent refactor broke writer-side logic

3. **Chunk Size Bug** (Secondary issue, documented in CHUNK_SIZE_FIX_PLAN.md)
   - Files truncated: 11339-cover.png returns 3518 bytes instead of 77263
   - Cause: Chunk.size stores compressed sizes instead of uncompressed

### What We Have vs. What We Need

#### ✅ Already Implemented

| Component | Location | Status |
|-----------|----------|--------|
| Frozen2 **Serialization** | `frozen2_serializer.cpp` | ✅ Working |
| Frozen2 Schema Ser/Deser | `frozen_schema_serializer.cpp` | ✅ Working |
| Frozen2 Bit Operations | `frozen_bits.cpp` (load_bits, store_bits) | ✅ Working |
| Thrift Compact Reader | `thrift_compact_reader.h` | ✅ Working |
| Flatbuffers Ser/Deser | `flatbuffers_serializer.cpp` | ✅ Working |

#### ❌ Missing Implementation

| Component | Needed For | Priority |
|-----------|------------|----------|
| **Frozen2 Deserializer** | Reading Homebrew images | 🔥 CRITICAL |
| **Writer Fix** | Creating test images | 🔥 CRITICAL |

#### 📚 Reference Implementation Available

**dwarfs-rs has complete Frozen2 deserializer**:
- File: `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/de_frozen.rs`
- Lines: ~600 lines of well-documented Rust code
- Features:
  - Full bit-level deserialization
  - Schema-driven layout interpretation
  - Handles all DwarFS metadata types
  - Production-tested (used by Homebrew v0.14.1)

---

## Phase 1: Implement Frozen2 Deserializer (PRIORITY 1)

### Goal
Enable reading Homebrew mkdwarfs v0.14.1 images by implementing Frozen2 deserialization

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Legacy Thrift Format (Homebrew v0.14.1)                   │
│                                                             │
│  ┌──────────────────┐        ┌───────────────────────┐    │
│  │ Schema Section   │        │ Metadata Section      │    │
│  │ (Thrift Compact) │        │ (Frozen2 Bit-Packed)  │    │
│  └──────────────────┘        └───────────────────────┘    │
│          ↓                            ↓                     │
│  ┌──────────────────┐        ┌───────────────────────┐    │
│  │ Schema           │───────>│ Frozen2Deserializer   │    │
│  │ Deserializer     │ schema │                       │    │
│  │ (EXISTING ✅)    │        │ (TO IMPLEMENT ❌)     │    │
│  └──────────────────┘        └───────────────────────┘    │
│                                        ↓                    │
│                               ┌────────────────────┐       │
│                               │ domain::metadata   │       │
│                               └────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

### Implementation Steps

#### Step 1.1: Create Frozen2 Deserializer Header

**File**: `include/dwarfs/metadata/legacy/frozen2_deserializer.h`

**Reference**: `dwarfs-rs/dwarfs/src/metadata/de_frozen.rs:1-200`

```cpp
#pragma once

#include <span>
#include <memory>
#include "dwarfs/metadata/domain/metadata.h"
#include "dwarfs/metadata/legacy/frozen_schema.h"

namespace dwarfs::metadata::legacy {

/**
 * Frozen2 Deserializer - Reads bit-packed metadata using schema
 *
 * Port of dwarfs-rs de_frozen.rs deserializer
 *
 * Architecture:
 *   1. Takes Schema (parsed from Thrift Compact)
 *   2. Takes frozen bytes (bit-packed data)
 *   3. Uses schema layouts to interpret bit positions
 *   4. Returns domain::metadata
 *
 * Bit Layout Example:
 *   Schema says field "block_size" is at offset 0, size 18 bits
 *   Deserializer:
 *     - Reads bits [0..18) from frozen bytes
 *     - Interprets as uint32_t
 *     - Assigns to metadata.block_size
 */
class Frozen2Deserializer {
public:
  /**
   * Deserialize Frozen2 format to domain::metadata
   *
   * @param schema Parsed schema (from Schema section)
   * @param data Frozen bytes (from Metadata section)
   * @return Deserialized metadata
   * @throws std::runtime_error on validation or parse failure
   *
   * Port from: de_frozen.rs:38-47
   */
  static domain::metadata deserialize(
      Schema const& schema,
      std::span<uint8_t const> data);

private:
  class Reader;  // Implements serde-like deserialization logic
};

} // namespace dwarfs::metadata::legacy
```

**Estimated Lines**: ~50 lines

#### Step 1.2: Implement Frozen2 Deserializer Core

**File**: `src/metadata/legacy/frozen2_deserializer.cpp`

**Reference**: `dwarfs-rs/dwarfs/src/metadata/de_frozen.rs:48-600`

**Key Components**:

1. **Reader Class** (port of Rust `Deserializer` struct)
   - Tracks current schema layout
   - Tracks bit offset in frozen data
   - Provides field access methods

2. **Deserialization Methods**:
   - `deserialize_u32()` - Read uint32 from bit-packed data
   - `deserialize_u64()` - Read uint64 from bit-packed data
   - `deserialize_bool()` - Read single bit
   - `deserialize_string()` - Read string (with length prefix)
   - `deserialize_vector()` - Read list/vector of values
   - `deserialize_struct()` - Read nested struct

3. **Integration with frozen_bits**:
   - Use existing `frozen_bits::load_bits()` for bit operations
   - Use existing `frozen_bits::load_bit()` for boolean fields

**Example Implementation** (core structure):

```cpp
domain::metadata Frozen2Deserializer::deserialize(
    Schema const& schema,
    std::span<uint8_t const> data) {

  auto root_layout = schema.layouts.get(schema.root_layout);
  if (!root_layout) {
    throw std::runtime_error("Invalid root layout");
  }

  Reader reader(schema, data, root_layout, 0, 0);
  return reader.deserialize_metadata();
}

class Frozen2Deserializer::Reader {
  Schema const& schema_;
  std::span<uint8_t const> data_;
  SchemaLayout const* layout_;
  uint32_t bit_offset_;
  uint32_t storage_start_;

public:
  // Port field_deserializer from de_frozen.rs:123-138
  Reader field_reader(int16_t field_id) const;

  // Port deserialize_u32 from de_frozen.rs:171-176
  uint32_t read_u32();

  // Port deserialize_u64 from de_frozen.rs:178-199
  uint64_t read_u64();

  // Port deserialize_bool from de_frozen.rs:160-169
  bool read_bool();

  // Port deserialize_seq from de_frozen.rs:247-292
  template<typename T>
  std::vector<T> read_vector();

  // Port deserialize_struct from de_frozen.rs:201-245
  domain::metadata deserialize_metadata();
};
```

**Estimated Lines**: ~500-600 lines

#### Step 1.3: Integrate Frozen2Deserializer into LegacyThriftSerializer

**File**: `src/metadata/serialization/legacy_thrift_serializer.cpp`

**Current Code** (lines 46-62):
```cpp
std::unique_ptr<void, void(*)(void*)> LegacyThriftSerializer::deserialize(
    const std::vector<uint8_t>& data) const {

  auto domain_meta = std::make_unique<domain::metadata>();

  // WRONG: This uses Thrift Compact deserialization
  legacy::LegacyMetadataSerializer::deserialize(data, *domain_meta);

  return std::unique_ptr<void, void(*)(void*)>(
      domain_meta.release(),
      [](void* ptr) { delete static_cast<domain::metadata*>(ptr); }
  );
}
```

**New Code** (CORRECT implementation):
```cpp
std::unique_ptr<void, void(*)(void*)> LegacyThriftSerializer::deserialize(
    const std::vector<uint8_t>& data) const {

  // DwarFS v0.14.1 format (Homebrew):
  //   Byte 0-7:    Size prefix (little-endian uint64)
  //   Byte 8+:     Schema (Thrift Compact) + Metadata (Frozen2)

  if (data.size() < 16) {
    throw std::runtime_error("Legacy Thrift data too small");
  }

  // Read size prefix (8 bytes)
  uint64_t size = 0;
  std::memcpy(&size, data.data(), 8);

  // Skip size prefix, get actual metadata
  std::span<uint8_t const> metadata_bytes(data.data() + 8, data.size() - 8);

  // Step 1: Parse Schema section (Thrift Compact format)
  // Schema comes first in the metadata bytes
  // We need to find where schema ends and frozen data begins

  // Parse schema (FrozenSchemaSerializer already handles this)
  auto schema = legacy::FrozenSchemaSerializer::deserialize(metadata_bytes);

  // Step 2: Calculate frozen data offset
  // After schema is parsed, remaining bytes are frozen data
  // This requires knowing schema serialized size
  // For now, we can serialize schema back to get size (inefficient but correct)
  auto schema_bytes = legacy::FrozenSchemaSerializer::serialize(schema);
  size_t schema_size = schema_bytes.size();

  if (metadata_bytes.size() <= schema_size) {
    throw std::runtime_error("No frozen data after schema");
  }

  // Step 3: Extract frozen bytes
  std::span<uint8_t const> frozen_bytes(
      metadata_bytes.data() + schema_size,
      metadata_bytes.size() - schema_size);

  // Step 4: Deserialize using Frozen2
  auto domain_meta = std::make_unique<domain::metadata>(
      legacy::Frozen2Deserializer::deserialize(schema, frozen_bytes));

  return std::unique_ptr<void, void(*)(void*)>(
      domain_meta.release(),
      [](void* ptr) { delete static_cast<domain::metadata*>(ptr); }
  );
}
```

**Estimated Lines**: ~50 lines modified

#### Step 1.4: Port Rust de_frozen.rs Methods

**Mapping Table**: Rust → C++

| Rust Method (de_frozen.rs) | C++ Method | Lines | Complexity |
|----------------------------|------------|-------|------------|
| `deserialize()` (38-47) | `Frozen2Deserializer::deserialize()` | ~15 | Low |
| `Source::load_bit()` (60-67) | Use `frozen_bits::load_bit()` | ✅ Exists | - |
| `Source::load_bits()` (73-111) | Use `frozen_bits::load_bits()` | ✅ Exists | - |
| `Deserializer::field_deserializer()` (123-138) | `Reader::field_reader()` | ~20 | Low |
| `deserialize_bool()` (160-169) | `Reader::read_bool()` | ~10 | Low |
| `deserialize_u32/u64()` (171-199) | `Reader::read_u32/u64()` | ~30 | Low |
| `deserialize_struct()` (201-245) | `Reader::deserialize_metadata()` | ~50 | Medium |
| `deserialize_seq()` (247-292) | `Reader::read_vector<T>()` | ~50 | Medium |
| `deserialize_string()` (318-343) | `Reader::read_string()` | ~30 | Low |
| `deserialize_map()` (350-428) | `Reader::read_map<K,V>()` | ~80 | High |

**Total Estimated Lines**: ~600 lines

#### Step 1.5: Testing & Validation

**Test Files**:
1. `test/metadata/legacy/frozen2_deserializer_test.cpp` (new)
   - Unit tests for each deserialization method
   - Round-trip tests (serialize → deserialize)
   - Compatibility tests with dwarfs-rs generated images

2. Integration test:
   ```bash
   # Generate image with Homebrew mkdwarfs
   /opt/homebrew/bin/mkdwarfs -i example/pg11339-h -o /tmp/test-homebrew.dff

   # Try to read with our tools
   ./build/dwarfsck -i /tmp/test-homebrew.dff --export-metadata=/tmp/meta.json

   # Serve with static-site-server
   ./build/static-site-server --image /tmp/test-homebrew.dff --port 8080
   curl http://localhost:8080/11339-cover.png > /tmp/downloaded.png
   diff example/pg11339-h/11339-cover.png /tmp/downloaded.png
   ```

**Success Criteria**:
- ✅ All unit tests pass
- ✅ Can read Homebrew-generated images
- ✅ dwarfsck exports metadata correctly
- ✅ static-site-server serves files byte-for-byte identical to originals

---

## Phase 2: Fix mkdwarfs Writer Crash (PRIORITY 2)

### Goal
Fix local mkdwarfs so it can create test images for development

### Issue Details

**Error**:
```
Assertion failed: (fsb->size() <= worst_case_block_size_)
File: filesystem_writer.cpp, line 188
```

**Context**:
- Triggered when creating even empty filesystems
- Recent refactor changed block size calculation
- Likely related to multi-format metadata support changes

### Investigation Steps

#### Step 2.1: Reproduce and Analyze

**Commands**:
```bash
# Try to create minimal filesystem
./build/mkdwarfs -i example/pg11339-h -o /tmp/test.dff -l 0 2>&1 | tee /tmp/mkdwarfs-debug.log

# Check with verbose logging
./build/mkdwarfs -i example/pg11339-h -o /tmp/test.dff --log-level=debug 2>&1 | tee /tmp/mkdwarfs-verbose.log
```

**Analysis Questions**:
1. What is `fsb->size()` returning?
2. What is `worst_case_block_size_` set to?
3. When was `worst_case_block_size_` calculation changed?
4. Does this affect all formats or just certain configurations?

#### Step 2.2: Git Bisect the Issue

**Commits to Check**:
```bash
git log --oneline --all | grep -E "block|size|writer|metadata" | head -20
```

**Known Good**: Before commit `fcf6c651` (feat: generalize metadata formats...)
**Known Bad**: Current HEAD

**Bisect Process**:
```bash
git bisect start
git bisect bad HEAD
git bisect good 203c837e  # Last known working commit
# Test each commit:
./build.sh && ./build/mkdwarfs -i example/pg11339-h -o /tmp/test.dff
```

#### Step 2.3: Examine filesystem_writer.cpp

**File**: `src/writer/filesystem_writer.cpp` (or similar)

**Key Areas**:
1. `worst_case_block_size_` initialization
2. Block size calculation during metadata serialization
3. Changes related to multi-format support

**Look for**:
- Metadata size estimation changes
- Block size overhead calculations
- Format-specific size differences (Flatbuffers vs Thrift)

#### Step 2.4: Fix Implementation

**Likely Fixes** (hypothesis):

**Option A**: Metadata size overhead increased
```cpp
// Before (Thrift-only):
worst_case_block_size_ = block_size + thrift_overhead;

// After (Multi-format):
worst_case_block_size_ = block_size + std::max({
    flatbuffers_overhead,
    legacy_thrift_overhead,
    modern_thrift_overhead
});
```

**Option B**: Size calculation happens too early
```cpp
// Move worst_case_block_size_ calculation to AFTER
// metadata format is determined
```

**Option C**: Format-specific size estimation
```cpp
// Add per-format overhead calculation
size_t get_metadata_overhead(SerializationFormat format) {
  switch (format) {
    case FLATBUFFERS: return calculate_flatbuffers_overhead();
    case LEGACY_THRIFT: return calculate_legacy_thrift_overhead();
    // ...
  }
}
```

#### Step 2.5: Testing & Validation

**Test Commands**:
```bash
# Test 1: Create small filesystem
./build/mkdwarfs -i example/pg11339-h -o /tmp/test-small.dff

# Test 2: Create filesystem with different compression levels
./build/mkdwarfs -i example/pg11339-h -o /tmp/test-l0.dff -l 0
./build/mkdwarfs -i example/pg11339-h -o /tmp/test-l9.dff -l 9

# Test 3: Verify images work
./build/dwarfsck -i /tmp/test-small.dff
./build/dwarfsextract -i /tmp/test-small.dff -o /tmp/extracted/
diff -r example/pg11339-h /tmp/extracted/
```

**Success Criteria**:
- ✅ mkdwarfs creates images without assertion
- ✅ Images can be read by dwarfsck
- ✅ Images can be extracted byte-for-byte identical
- ✅ static-site-server can serve the images

---

## Phase 3: Integration & Testing

### Step 3.1: Comprehensive Test Script

Update `example/static-site-server/comprehensive_test.sh`:

```bash
#!/bin/bash
# Comprehensive DwarFS Multi-Format Test

# Generate images with BOTH tools:
# 1. Homebrew mkdwarfs (Frozen2/Legacy Thrift)
/opt/homebrew/bin/mkdwarfs -i example/pg11339-h -o aesop-homebrew.dff

# 2. Local mkdwarfs (Flatbuffers)
./build/mkdwarfs -i example/pg11339-h -o aesop-local.dff

# Test BOTH images produce identical results
for image in aesop-homebrew.dff aesop-local.dff; do
  echo "Testing $image..."

  # Start server
  ./build/static-site-server --image $image --port 8080 &
  SERVER_PID=$!
  sleep 2

  # Download files
  curl -s http://localhost:8080/11339-cover.png > /tmp/cover-$image.png
  curl -s http://localhost:8080/images/01hare.jpg > /tmp/01hare-$image.jpg

  # Verify
  diff example/pg11339-h/11339-cover.png /tmp/cover-$image.png
  diff example/pg11339-h/images/01hare.jpg /tmp/01hare-$image.jpg

  kill $SERVER_PID
done

# Compare outputs
diff /tmp/cover-aesop-homebrew.dff.png /tmp/cover-aesop-local.dff.png
diff /tmp/01hare-aesop-homebrew.dff.jpg /tmp/01hare-aesop-local.dff.jpg
```

### Step 3.2: Final Validation Checklist

- [ ] Frozen2Deserializer compiles without errors
- [ ] All unit tests pass
- [ ] mkdwarfs creates images successfully
- [ ] Can read Homebrew-generated images
- [ ] Can read locally-generated images
- [ ] static-site-server serves correct file sizes
- [ ] static-site-server serves correct file contents (byte-for-byte)
- [ ] static-site-server differentiates text vs binary files
- [ ] /ls/ endpoint shows correct file count
- [ ] No debug output in production builds

### Step 3.3: Performance Validation

**Benchmarks**:
```bash
# Compare deserialization performance
./build/dwarfs_benchmark_tests --benchmark_filter=Frozen2Deserialize
./build/dwarfs_benchmark_tests --benchmark_filter=FlatbuffersDeserialize

# Compare image sizes
ls -lh aesop-homebrew.dff aesop-local.dff

# Compare server response times
ab -n 1000 -c 10 http://localhost:8080/11339-cover.png
```

**Acceptance Criteria**:
- Frozen2 deserialization within 2x of Flatbuffers speed
- Image sizes within 10% of each other
- Server response times < 100ms for small files

---

## Phase 4: Chunk Size Fix (OPTIONAL - If Time Permits)

**Note**: The chunk size issue in CHUNK_SIZE_FIX_PLAN.md is a **secondary issue**. Once we can read images correctly, we may find this is already fixed in newer image formats.

**Decision Point**: After Phase 1-2 complete, test with fresh images:
```bash
# Create new image
./build/mkdwarfs -i example/pg11339-h -o /tmp/fresh.dff

# Test file sizes
./build/static-site-server --image /tmp/fresh.dff --port 8080 &
curl -I http://localhost:8080/11339-cover.png | grep Content-Length
# Expected: 77263
```

**If still broken**: Follow CHUNK_SIZE_FIX_PLAN.md
**If fixed**: Document as resolved by format implementation

---

## Implementation Timeline

| Phase | Task | Estimated Time | Priority |
|-------|------|----------------|----------|
| 1.1 | Create Frozen2Deserializer header | 1 hour | P1 |
| 1.2 | Implement core deserialization | 6-8 hours | P1 |
| 1.3 | Integrate into LegacyThriftSerializer | 2 hours | P1 |
| 1.4 | Port all Rust methods | 4-6 hours | P1 |
| 1.5 | Testing & validation | 2-3 hours | P1 |
| **Phase 1 Total** | **15-20 hours** | **P1** |
| 2.1 | Reproduce writer crash | 1 hour | P2 |
| 2.2 | Git bisect analysis | 2 hours | P2 |
| 2.3 | Examine filesystem_writer.cpp | 2 hours | P2 |
| 2.4 | Implement fix | 2-4 hours | P2 |
| 2.5 | Testing & validation | 2 hours | P2 |
| **Phase 2 Total** | **9-13 hours** | **P2** |
| 3.1 | Comprehensive test script | 2 hours | P3 |
| 3.2 | Final validation | 3 hours | P3 |
| 3.3 | Performance validation | 2 hours | P3 |
| **Phase 3 Total** | **7 hours** | **P3** |
| **GRAND TOTAL** | **31-40 hours** | |

---

## Success Criteria (Final)

When this plan is complete, the following MUST all be true:

### Reader (static-site-server)
- ✅ Can load Homebrew mkdwarfs v0.14.1 images (Legacy Thrift/Frozen2)
- ✅ Can load locally-built images (Flatbuffers)
- ✅ Serves correct file sizes (Content-Length headers)
- ✅ Serves correct file contents (byte-for-byte match with originals)
- ✅ /ls/ endpoint shows correct file count
- ✅ Differentiates text vs binary files correctly
- ✅ No "Expected LIST for uids" errors
- ✅ No truncated files

### Writer (mkdwarfs)
- ✅ Creates images without crashes
- ✅ Created images can be read by dwarfsck
- ✅ Created images can be served by static-site-server
- ✅ No assertion failures

### Testing
- ✅ comprehensive_test.sh passes 100%
- ✅ All unit tests pass
- ✅ Both Homebrew and local images produce identical HTTP responses

### Output "__RALPH_DONE__" when all above criteria are met.

---

## Risk Mitigation

### Risk 1: Frozen2 Deserialization Complexity
- **Mitigation**: Port line-by-line from dwarfs-rs (proven working)
- **Fallback**: Use Rust FFI to call dwarfs-rs deserializer temporarily

### Risk 2: Writer Fix Requires Architecture Changes
- **Mitigation**: Git bisect to find exact breaking commit
- **Fallback**: Revert to known-good commit, cherry-pick other changes

### Risk 3: Format Incompatibilities
- **Mitigation**: Test with multiple DwarFS versions
- **Fallback**: Support both old and new formats with version detection

---

## Notes

- **dwarfs-rs Reference**: `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/de_frozen.rs`
- **Existing Frozen2 Serializer**: `src/metadata/legacy/frozen2_serializer.cpp`
- **Existing Bit Operations**: `src/metadata/legacy/frozen_bits.cpp`
- **Test Images**: `example/static-site-server/*.dff`
- **Source Data**: `example/pg11339-h/` (Aesop), `example/pg19942-h/` (Candide)

---

**END OF PLAN**
