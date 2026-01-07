# Metadata Strategy Pattern - Implementation Roadmap

## Quick Reference

**Main Document**: [`METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`](METADATA_ARCHITECTURE_STRATEGY_PATTERN.md)

**Goal**: Eliminate hard Thrift dependencies by introducing abstract interfaces that both Thrift and FlatBuffers implement.

**Key Insight**: We need a HIGHER-LEVEL interface, not format conversion.

---

## Critical Architecture Shift

### ❌ LOW-LEVEL THINKING (Wrong Approach)
```
Trying to convert Thrift code → domain model
Creating parallel implementations
Mixing concerns in one place
```

### ✅ HIGH-LEVEL THINKING (Correct Approach - Strategy Pattern)
```
┌──────────────────────────────────┐
│   Abstract Interface             │
│   (Format Agnostic)              │
│                                  │
│   - get_chunk()                  │
│   - get_directory()              │
│   - build()                      │
└────────┬───────────────┬─────────┘
         │               │
    implements      implements
         │               │
         ▼               ▼
   ┌─────────┐    ┌──────────┐
   │ Thrift  │    │FlatBuf   │
   │  Impl   │    │  Impl    │
   └─────────┘    └──────────┘
```

---

## Implementation Phases

### Phase 1: Define Abstract Interfaces (Week 1)

**Reader Interface** - [`include/dwarfs/reader/metadata_provider.h`](../include/dwarfs/reader/)
```cpp
class metadata_provider {
  virtual chunk get_chunk(inode, offset) = 0;
  virtual directory get_directory(inode) = 0;
  virtual inode_data get_inode(inode_num) = 0;
  // ... all read operations as pure virtual

  static unique_ptr<metadata_provider> create(format, data);
};
```

**Writer Interface** - Update [`include/dwarfs/writer/internal/metadata_builder.h`](../include/dwarfs/writer/internal/metadata_builder.h)
```cpp
class metadata_builder {
  virtual void gather_chunks(...) = 0;
  virtual void gather_entries(...) = 0;

  // KEY CHANGE: Returns domain model, not Thrift type!
  virtual metadata::domain::metadata build() = 0;

  static unique_ptr<metadata_builder> create(format, options);
};
```

### Phase 2: Thrift Implementation (Week 2)

**Files to Create:**
- `src/writer/internal/thrift_metadata_builder.cpp`
- Guard with `#ifdef DWARFS_HAVE_THRIFT`
- Move code from existing `metadata_builder.cpp`
- Convert Thrift → domain in `build()` method

**Key Pattern:**
```cpp
metadata::domain::metadata thrift_metadata_builder::build() {
  // Build Thrift structures (existing code)
  thrift::metadata::metadata thrift_md = /* ... */;

  // Convert to domain model
  return metadata::converters::from_thrift(thrift_md);
}
```

### Phase 3: FlatBuffers Implementation (Week 2-3)

**Files to Create:**
- `src/writer/internal/flatbuffers_metadata_builder.cpp`
- Always available (no #ifdef needed)
- Build domain model DIRECTLY (no conversion!)

**Key Advantage:**
```cpp
metadata::domain::metadata flatbuffers_metadata_builder::build() {
  // Already building domain model!
  update_nlink();
  update_totals_and_size_cache();
  pack_metadata();

  return std::move(md_);  // No conversion needed!
}
```

### Phase 4: Factory Implementation (Week 3)

**Files to Create:**
- `src/writer/internal/metadata_builder_factory.cpp`
- `src/reader/metadata_provider_factory.cpp`

**Pattern:**
```cpp
unique_ptr<metadata_builder> metadata_builder::create(
    SerializationFormat format, ...) {
  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case THRIFT_COMPACT: return make_unique<thrift_metadata_builder>(...);
#endif
    case FLATBUFFERS: return make_unique<flatbuffers_metadata_builder>(...);
  }
}
```

### Phase 5: Integration (Week 4)

**Scanner** (`src/writer/scanner.cpp`):
```cpp
// Before:
#ifdef DWARFS_HAVE_THRIFT
metadata_builder mb(...);
auto const& md = mb.build();  // Returns Thrift type
#endif

// After:
auto mb = metadata_builder::create(format, ...);
auto md = mb->build();  // Returns domain model!
```

**Filesystem Writer** (`src/writer/filesystem_writer.cpp`):
```cpp
// Accept domain model
void write_metadata(metadata::domain::metadata md);
```

### Phase 6: Testing & Validation (Week 5-6)

**Build Tests:**
```bash
# Test 1: FlatBuffers only (NEW capability!)
cmake -B build -DDWARFS_WITH_THRIFT=OFF
ninja -C build
ctest --test-dir build

# Test 2: Both formats
cmake -B build -DDWARFS_WITH_THRIFT=ON -DDWARFS_WITH_FLATBUFFERS=ON
ninja -C build
ctest --test-dir build

# Test 3: Verify thrift-only fails (as expected)
cmake -B build -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON
# Should fail - FlatBuffers is required
```

**Functional Tests:**
```bash
# Create with FlatBuffers
./mkdwarfs -i src -o test-fb.dwarfs --metadata-format=flatbuffers

# Create with Thrift (if available)
./mkdwarfs -i src -o test-thrift.dwarfs --metadata-format=thrift

# Verify both can be mounted/checked/extracted
./dwarfs test-fb.dwarfs mnt
./dwarfsck test-fb.dwarfs
./dwarfsextract -i test-fb.dwarfs -o output
```

---

## Key Files Modified

### Headers (Public API Changes)
```
include/dwarfs/writer/internal/metadata_builder.h
  - Change build() return: thrift::metadata::metadata → metadata::domain::metadata
  - Remove Thrift forward declarations
  - Keep impl class pattern

include/dwarfs/reader/metadata_provider.h (NEW)
  - Pure virtual interface for all read operations
```

### Implementations
```
src/writer/internal/
  thrift_metadata_builder.cpp (NEW - moved from metadata_builder.cpp)
  flatbuffers_metadata_builder.cpp (NEW)
  metadata_builder_factory.cpp (NEW)

src/reader/
  thrift_metadata_provider.cpp (NEW)
  flatbuffers_metadata_provider.cpp (NEW)
  metadata_provider_factory.cpp (NEW)
```

### Integration Points
```
src/writer/scanner.cpp
  - Use factory instead of direct construction

src/writer/filesystem_writer.cpp
  - Accept domain model instead of Thrift type

src/reader/filesystem_v2.cpp
  - Use metadata_provider interface
```

---

## Success Metrics

### Compilation
- ✅ Builds with `-DDWARFS_WITH_THRIFT=OFF` (mkdwarfs, dwarfs, dwarfsck, dwarfsextract)
- ✅ Builds with `-DDWARFS_WITH_THRIFT=ON`
- ✅ No `#ifdef DWARFS_HAVE_THRIFT` in public headers

### Runtime
- ✅ Creates valid FlatBuffers images
- ✅ Creates valid Thrift images (if compiled with Thrift)
- ✅ Reads both formats correctly
- ✅ Performance within 10% between formats

### Code Quality
- ✅ No `thrift::metadata::` types in scanner.cpp, filesystem_writer.cpp
- ✅ Format-specific code isolated in implementations
- ✅ Domain model as universal interchange

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| **Complex migration** | Phased approach, one component at a time |
| **Performance regression** | Benchmark each phase, optimize if needed |
| **Breaking changes** | Keep Thrift as-is initially, extensive testing |
| **Time overrun** | 6-week timeline includes buffer for issues |

---

## Quick Start Checklist

- [ ] Read [`METADATA_ARCHITECTURE_STRATEGY_PATTERN.md`](METADATA_ARCHITECTURE_STRATEGY_PATTERN.md)
- [ ] Define `metadata_provider` interface
- [ ] Update `metadata_builder.h` to return domain model
- [ ] Create `thrift_metadata_builder.cpp` (move existing code)
- [ ] Create `flatbuffers_metadata_builder.cpp` (new implementation)
- [ ] Create factory implementations
- [ ] Update scanner.cpp integration
- [ ] Test both build configurations
- [ ] Run benchmarks

---

## Next Steps

1. **Review architecture document** - Understand Strategy Pattern design
2. **Create abstract interfaces** - Define pure virtual base classes
3. **Implement Thrift strategy** - Move existing code, add conversion
4. **Implement FlatBuffers strategy** - Build domain model directly
5. **Update integration points** - Scanner, filesystem_writer, etc.
6. **Comprehensive testing** - Both formats, all tools

**Estimated Timeline**: 6 weeks full-time development

**Current Status**: Architecture designed, ready for implementation