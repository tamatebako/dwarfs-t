# Strategy Pattern Implementation - Continuation Prompt for Next Session

## Quick Context (Fresh Session Start)

DwarFS Strategy Pattern implementation is **85% complete**. FlatBuffers-only builds work perfectly (99.1% tests passing). Dual-format builds are 90% done but blocked by FLAC compression coupling issue.

**Current Branch**: `feature/multi-format-serialization-fuse`  
**Base Commit**: TBD (check git log)

## IMMEDIATE TASK: Fix FLAC/Thrift Coupling

**Goal**: Enable dual-format builds (THRIFT=ON + FLATBUFFERS=ON) to run benchmarks

**Current Blocker**: FLAC compressor is tightly coupled to Thrift serialization

**Location**: `src/compression/flac.cpp` (19 compilation errors)

### Problem Analysis

**Architecture Violation**:
```cpp
// Current (BAD - violates Dependency Inversion Principle)
FLAC Compressor → apache::thrift::CompactSerializer (concrete dependency)
                 → folly::ByteRange (concrete dependency)
```

**Errors**:
1. Uses `CompactSerializer` directly (Thrift-specific)
2. Uses `folly::ByteRange` (Folly-specific)
3. `flac_block_decompressor` return type incompatible

### Clean Architecture Solution

**Apply**: Strategy Pattern + Dependency Inversion

```
┌──────────────────────────────┐
│   FLAC Compressor            │
│ (depends on abstraction)     │
└──────────────┬───────────────┘
               │ uses
               ▼
┌──────────────────────────────┐
│ ICompressionMetadataSerializer │  ← INTERFACE (new)
│ - serialize() → bytes         │
│ - deserialize() → metadata    │
└──────────────┬───────────────┘
               │ implements
        ┌──────┴──────┐
        ▼             ▼
┌────────────┐  ┌────────────┐
│  Thrift    │  │ FlatBuffers│
│ Serializer │  │ Serializer │
└────────────┘  └────────────┘
```

### Implementation Checklist

#### 1. Create Interface (30 min)

**File**: `include/dwarfs/compression/metadata_serializer.h`

```cpp
#pragma once

#include <memory>
#include <string_view>
#include <vector>

namespace dwarfs::compression {

class ICompressionMetadataSerializer {
public:
  virtual ~ICompressionMetadataSerializer() = default;
  
  virtual std::vector<uint8_t> serialize(void const* metadata) const = 0;
  virtual std::unique_ptr<void, void(*)(void*)> deserialize(
      std::vector<uint8_t> const& data) const = 0;
  virtual std::string_view get_format_name() const = 0;
};

class MetadataSerializerFactory {
public:
  static std::unique_ptr<ICompressionMetadataSerializer> create_default();
};

} // namespace dwarfs::compression
```

#### 2. Implement Thrift Strategy (1 hour)

**File**: `src/compression/metadata_serializer_thrift.cpp`

```cpp
#ifdef DWARFS_HAVE_THRIFT

#include <dwarfs/compression/metadata_serializer.h>
#include <thrift/lib/cpp2/protocol/Serializer.h>
// Include PCM metadata type
#include "pcm_metadata.h"  // Or wherever it's defined

namespace dwarfs::compression {

class ThriftMetadataSerializer : public ICompressionMetadataSerializer {
  std::vector<uint8_t> serialize(void const* metadata) const override {
    auto const* pcm = static_cast<PCMMetadata const*>(metadata);
    std::string result;
    ::apache::thrift::CompactSerializer::serialize(*pcm, &result);
    return std::vector<uint8_t>(result.begin(), result.end());
  }
  
  std::unique_ptr<void, void(*)(void*)> deserialize(
      std::vector<uint8_t> const& data) const override {
    auto pcm = std::make_unique<PCMMetadata>();
    ::apache::thrift::CompactSerializer::deserialize(data, *pcm);
    return std::unique_ptr<void, void(*)(void*)>(
      pcm.release(),
      [](void* p) { delete static_cast<PCMMetadata*>(p); }
    );
  }
  
  std::string_view get_format_name() const override {
    return "Thrift Compact";
  }
};

std::unique_ptr<ICompressionMetadataSerializer> 
MetadataSerializerFactory::create_default() {
  return std::make_unique<ThriftMetadataSerializer>();
}

} // namespace dwarfs::compression

#endif // DWARFS_HAVE_THRIFT
```

#### 3. Refactor FLAC (1.5 hours)

**File**: `src/compression/flac.cpp`

**Changes**:

1. Remove direct Thrift includes:
```cpp
// DELETE these:
// #include <thrift/lib/cpp2/protocol/Serializer.h>
// using ::apache::thrift::CompactSerializer;
```

2. Add interface include:
```cpp
#include <dwarfs/compression/metadata_serializer.h>
```

3. Update class to use interface:
```cpp
class flac_block_decompressor : public block_decompressor_base {
public:
  explicit flac_block_decompressor(
      std::span<uint8_t const> data,
      std::unique_ptr<compression::ICompressionMetadataSerializer> serializer)
    : serializer_(std::move(serializer)) {
    
    // Use interface method
    auto metadata_ptr = serializer_->deserialize(
        std::vector<uint8_t>(data.begin(), data.end()));
    
    auto* pcm = static_cast<PCMMetadata*>(metadata_ptr.get());
    // ... rest of init ...
  }

private:
  std::unique_ptr<compression::ICompressionMetadataSerializer> serializer_;
};
```

4. Update factory:
```cpp
std::unique_ptr<block_decompressor> make_decompressor(...) {
  auto serializer = compression::MetadataSerializerFactory::create_default();
  return std::make_unique<flac_block_decompressor>(data, std::move(serializer));
}
```

#### 4. Update CMake (15 min)

**File**: `cmake/libdwarfs.cmake`

Add new serializer files:
```cmake
$<$<BOOL:${FLAC_FOUND}>:src/compression/metadata_serializer_thrift.cpp>
```

#### 5. Test Dual-Format Build (30 min)

```bash
cmake -B build-dual -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build-dual
```

**Expected**: All compile, mkdwarfs/dwarfsextract/dwarfs binaries created

---

## Phase 8: Run Benchmarks

Once Phase 7 complete:

```bash
# Download dataset (once)
python3 benchmarks/download_datasets.py --download perl

# Run complete comparison
python3 benchmarks/run_complete_comparison.py \
  --dataset benchmark-files/perl-5.43.3 \
  --runs 3

# Output:
# - benchmarks/results/complete_comparison.json
# - benchmarks/results/flatbuffers_vs_thrift.md
```

---

## Phase 9: Documentation Cleanup

### Move Outdated Docs

```bash
mkdir -p old-docs
mv doc/STRATEGY_PATTERN_ARCHITECTURE_DESIGN.md old-docs/
mv doc/STRATEGY_PATTERN_STATUS.md old-docs/
mv doc/STRATEGY_PATTERN_IMPLEMENTATION_STATUS.md old-docs/
mv doc/STRATEGY_PATTERN_PHASE3_PLAN.md old-docs/
mv doc/THRIFT_OPTIONAL_REFACTORING_PLAN.md old-docs/
mv doc/PHASE_6_BUILD_FIX_CONTINUATION.txt old-docs/
mv doc/PHASE_8_PROGRESS_SUMMARY.md old-docs/
mv doc/STRATEGY_PATTERN_IMPLEMENTATION_PROMPT.txt old-docs/
mv doc/WRITER_DOMAIN_MODEL_REFACTOR.md old-docs/
mv doc/CMAKE_REFACTORING_*.md old-docs/
mv doc/CI_TESTING_STRATEGY.md old-docs/
```

### Update Official Docs

**README.adoc** - Add section:
```asciidoc
== Metadata Formats

DwarFS supports two metadata serialization formats:

* *FlatBuffers* (default) - Modern, portable, header-only
* *Thrift Compact* (legacy) - Smallest size, backward compatibility

The format is auto-detected when reading. When creating:

[source,bash]
----
mkdwarfs -i src -o test.dwarfs --metadata-format=flatbuffers
----
```

**doc/mkdwarfs.md** - Document option:
```markdown
### --metadata-format=FORMAT

Selects metadata serialization format:
- `flatbuffers` (default): Modern portable format
- `thrift`: Legacy format for compatibility

Format is auto-detected when reading, so this only affects creation.
```

---

## Success Criteria

### Phase 7
- [ ] FLAC compiles in dual-format mode
- [ ] All tools build successfully
- [ ] No regression in FlatBuffers-only mode

### Phase 8  
- [ ] Benchmark script executes without errors
- [ ] JSON results file created
- [ ] Markdown report generated
- [ ] Comparison shows both formats working

### Phase 9
- [ ] Old docs moved
- [ ] Official docs updated
- [ ] Architecture clear in README

---

## Key Context Documents

**Read These First**:
1. `doc/STRATEGY_PATTERN_CONTINUATION_PLAN_PHASE7.md` - This file
2. `doc/STRATEGY_PATTERN_STATUS_TRACKER.md` - Detailed status
3. `.kilocode/rules/memory-bank/architecture.md` - Overall architecture

**Implementation Reference**:
- `src/writer/internal/flatbuffers_metadata_builder.cpp` - Example strategy
- `src/metadata/serialization/flatbuffers_serializer.cpp` - Serialization pattern
- `test/metadata/serialization_test.cpp` - Test patterns

---

## Handoff Information

### What's Working Perfectly
- FlatBuffers-only builds
- Test suite (99.1%)
- Writer creates valid images
- Reader loads images correctly

### What's Blocked
- Dual-format build (FLAC issue)
- Benchmark comparison (needs dual-format)

### Next Steps
1. Fix FLAC (~3-4 hours)
2. Run benchmarks (~1 hour)
3. Clean up docs (~1 hour)

**Total**: ~5-6 hours to 100% completion
