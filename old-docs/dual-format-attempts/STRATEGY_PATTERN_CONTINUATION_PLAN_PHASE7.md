# Strategy Pattern Implementation - Phase 7 Continuation Plan

**Status**: FlatBuffers-only COMPLETE (99.1% tests passing)  
**Next**: Enable dual-format builds for benchmark comparison

**Date Created**: 2025-11-19  
**Last Updated**: 2025-11-19  
**Current Phase**: 7 - Dual-Format Build Fixes

---

## Current State Summary

### ✅ COMPLETE (Phases 1-6)
1. **FlatBuffers Strategy** - All 9 methods implemented, validated
2. **Domain Model Integration** - 32 files modified, ~3,800 lines
3. **Serialization Layer** - 6 critical bugs fixed
4. **Test Suite** - Format-agnostic, 99.1% pass rate
5. **CMake Build System** - Include ordering fixed
6. **Header Guards** - Duplicate definitions resolved

### ⏸️ IN PROGRESS (Phase 7)
**Dual-Format Build** - 90% complete, blocked by FLAC compression

---

## Phase 7: Dual-Format Build Completion

### Goal
Enable builds with BOTH Thrift and FlatBuffers for benchmark comparisons

### Current Blocker: FLAC Compression

**Issue**: FLAC compressor hard-coded to use Thrift serialization for metadata

**Location**: `src/compression/flac.cpp`

**Errors**: 19 compilation errors
- Uses `CompactSerializer` (Thrift-specific)
- Uses `folly::ByteRange` (Folly-specific)
- `flac_block_decompressor` doesn't inherit from `block_decompressor::impl` properly

### Architecture Problem

**Current Design** (Problematic):
```
FLAC Compressor → Thrift CompactSerializer → Serialized Metadata
```

**Issues**:
1. **Tight Coupling**: FLAC directly depends on Thrift types
2. **No Abstraction**: No interface for metadata serialization
3. **Violates OCP**: Can't extend to new formats without modifying FLAC

### Proposed Solution (Clean Architecture)

**Design Pattern**: Strategy + Dependency Inversion

```
┌────────────────────────────────────────────────┐
│         FLAC Compressor                        │
│  (depends on interface, not implementation)    │
└────────────────┬───────────────────────────────┘
                 │ uses
                 ▼
┌────────────────────────────────────────────────┐
│    ICompressionMetadataSerializer (interface)  │
│  - serialize(metadata) → bytes                 │
│  - deserialize(bytes) → metadata               │
└────────────────┬───────────────────────────────┘
                 │ implements
          ┌──────┴──────┐
          ▼             ▼
┌──────────────┐  ┌──────────────┐
│ Thrift       │  │ FlatBuffers  │
│ Metadata     │  │ Metadata     │
│ Serializer   │  │ Serializer   │
└──────────────┘  └──────────────┘
```

**Benefits**:
- ✅ **Separation of Concerns**: FLAC doesn't know about Thrift/FlatBuffers
- ✅ **Open/Closed**: Can add new formats without modifying FLAC
- ✅ **Dependency Inversion**: FLAC depends on interface, not concrete types
- ✅ **Single Responsibility**: Each serializer handles one format

### Implementation Steps

#### Step 1: Create Compression Metadata Interface

**File**: `include/dwarfs/compression/metadata_serializer.h`

```cpp
namespace dwarfs::compression {

// Abstract interface for compression metadata serialization
class IMetadataSerializer {
public:
  virtual ~IMetadataSerializer() = default;
  
  // Serialize metadata to bytes
  virtual std::vector<uint8_t> serialize(void const* metadata) const = 0;
  
  // Deserialize bytes to metadata
  virtual std::unique_ptr<void, void(*)(void*)> deserialize(
      std::vector<uint8_t> const& data) const = 0;
  
  // Get format name for logging
  virtual std::string_view get_format_name() const = 0;
};

// Factory to create appropriate serializer based on build configuration
class MetadataSerializerFactory {
public:
  static std::unique_ptr<IMetadataSerializer> create_default();
};

} // namespace dwarfs::compression
```

#### Step 2: Implement Thrift Serializer

**File**: `src/compression/metadata_serializer_thrift.cpp`

```cpp
#ifdef DWARFS_HAVE_THRIFT

class ThriftMetadataSerializer : public IMetadataSerializer {
  std::vector<uint8_t> serialize(void const* metadata) const override {
    auto* thrift_meta = static_cast<pcm_sample_transformer const*>(metadata);
    // Use CompactSerializer
  }
  
  std::unique_ptr<void, void(*)(void*)> deserialize(
      std::vector<uint8_t> const& data) const override {
    // Use CompactSerializer::deserialize
  }
};

#endif
```

#### Step 3: Refactor FLAC to Use Interface

**Changes in** `src/compression/flac.cpp`:

**Before** (Tight Coupling):
```cpp
using ::apache::thrift::CompactSerializer;
folly::ByteRange br{...};
CompactSerializer::deserialize(br, hdr);
```

**After** (Clean Architecture):
```cpp
class flac_block_decompressor : public block_decompressor::impl {
  explicit flac_block_decompressor(
      std::span<uint8_t const> data,
      std::unique_ptr<compression::IMetadataSerializer> serializer)
    : serializer_(std::move(serializer)) {
    
    // Use interface, not concrete type
    auto metadata = serializer_->deserialize(std::vector(data.begin(), data.end()));
    // ...
  }
  
  std::unique_ptr<compression::IMetadataSerializer> serializer_;
};
```

#### Step 4: Update Factory
```cpp
std::unique_ptr<block_decompressor> 
flac_block_decompressor::create(std::span<uint8_t const> data) {
  auto serializer = compression::MetadataSerializerFactory::create_default();
  return std::make_unique<flac_block_decompressor>(data, std::move(serializer));
}
```

**Estimated Time**: 3-4 hours

---

## Phase 8: Benchmark Execution

### Prerequisites
- ✅ FlatBuffers-only build working
- ⏸️ Dual-format build (blocked by Phase 7)
- ⏸️ Thrift-only build (needs Phase 7)

### Benchmark Script Ready

**File**: [`benchmarks/run_complete_comparison.py`](../benchmarks/run_complete_comparison.py)

**Usage**:
```bash
# Download dataset
python3 benchmarks/download_datasets.py --download perl

# Run complete comparison (once Phase 7 done)
python3 benchmarks/run_complete_comparison.py \
  --dataset benchmark-files/perl-5.43.3 \
  --runs 3 \
  --output benchmarks/results/complete_comparison.json \
  --report benchmarks/results/flatbuffers_vs_thrift.md
```

**Dimensions Measured**:
1. Image build time (mkdwarfs)
2. Image size (compression ratio)
3. Extraction time (dwarfsextract)
4. Mount time (FUSE initialization)
5. Random access (find operations)
6. Memory usage (peak RSS)

**Estimated Execution Time**: 1 hour (3 builds × 3 runs × ~6 minutes)

---

## Phase 9: Documentation Updates

### Move Outdated Docs
```bash
mv doc/STRATEGY_PATTERN_*.md old-docs/
mv doc/PHASE_*.txt old-docs/
mv doc/THRIFT_OPTIONAL_*.md old-docs/
mv doc/WRITER_DOMAIN_MODEL_*.md old-docs/
mv doc/CMAKE_REFACTORING_*.md old-docs/
```

### Update Official Docs

**Files to Update**:
1. `README.adoc` - Add metadata format section
2. `doc/mkdwarfs.md` - Document `--metadata-format` option
3. `doc/dwarfs-format.md` - Explain FlatBuffers vs Thrift

**Content**:
- FlatBuffers is modern default
- Thrift for legacy compatibility
- Format auto-detection
- Performance characteristics

---

## Success Criteria

### Phase 7: Dual-Format Build
- [ ] FLAC uses metadata serializer interface
- [ ] All 3 build modes compile successfully
- [ ] Tests pass in all modes

### Phase 8: Benchmarks
- [ ] Perl dataset downloaded
- [ ] Benchmark script executes successfully
- [ ] JSON results generated
- [ ] Markdown report with comparisons

### Phase 9: Documentation
- [ ] Outdated docs moved to old-docs/
- [ ] Official docs updated with format information
- [ ] Architecture documented in README.adoc

---

## Risk Assessment

### High Risk
- **FLAC refactor complexity** - Compression metadata is intricate
- **Mitigation**: Incremental changes, test after each step

### Medium Risk
- **Benchmark execution time** - Could take 1-2 hours
- **Mitigation**: Use `--runs 1` for quick validation

### Low Risk
- **Documentation updates** - Straightforward
- **Mitigation**: Review before committing

---

## Timeline Estimate

| Phase | Task | Estimate | Priority |
|-------|------|----------|----------|
| 7 | FLAC refactor | 3-4 hours | HIGH |
| 8 | Benchmark execution | 1 hour | HIGH |
| 9 | Doc cleanup | 1 hour | MEDIUM |
| **Total** | **Complete** | **5-6 hours** | - |

---

## Handoff Checklist

### What's Working
- [x] FlatBuffers-only builds on AppleClang 17
- [x] Test suite 99.1% pass rate
- [x] Serialization validated
- [x] CMake build system fixed

### What Needs Work
- [ ] FLAC/Thrift coupling (Phase 7)
- [ ] Benchmark execution (Phase 8)
- [ ] Documentation cleanup (Phase 9)

### Key Files
- **Implementation**: `src/writer/internal/flatbuffers_metadata_builder.cpp`
- **Serialization**: `src/metadata/serialization/flatbuffers_serializer.cpp`
- **Tests**: `test/metadata/serialization_test.cpp`
- **Benchmarks**: `benchmarks/run_complete_comparison.py`
- **This Plan**: `doc/STRATEGY_PATTERN_CONTINUATION_PLAN_PHASE7.md`

---

**Ready for Phase 7 continuation**
