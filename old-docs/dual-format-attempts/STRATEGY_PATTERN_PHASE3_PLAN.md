// Create Phase 3 implementation plan
# Phase 3: Strategy Pattern Implementation Plan

**Date**: 2025-11-17
**Status**: Planning
**Goal**: Proper Strategy Pattern with separate implementations per format

## SOLID Principles Compliance

### Single Responsibility ✅
- **One class = One format implementation**
- `thrift_metadata_builder.cpp` - Thrift format only
- `flatbuffers_metadata_builder.cpp` - FlatBuffers format only
- `metadata_builder_factory.cpp` - Creation logic only

### Open/Closed ✅
- Add new format = add new strategy file
- No modifications to existing strategies
- No modifications to interface

### Liskov Substitution ✅
- Both strategies implement same interface
- Can be used interchangeably
- Same behavior contract

### Interface Segregation ✅
- Single `metadata_builder::impl` interface
- No format-specific methods in interface

### Dependency Inversion ✅
- Depend on `metadata_builder::impl` (abstraction)
- Not on `thrift_metadata_builder` (concrete)

## Current File Structure

```
include/dwarfs/writer/internal/
  metadata_builder.h                 # Public interface (PIMPL)

src/writer/internal/
  metadata_builder.cpp               # Monolithic Thrift implementation
                                     # Contains: constructors + metadata_builder_ class
```

## Target File Structure (MECE)

```
include/dwarfs/writer/internal/
  metadata_builder.h                 # Public interface (unchanged)

src/writer/internal/
  metadata_builder.cpp               # Thin wrapper (constructors only)
                                     # Delegates to factory

  thrift_metadata_builder.cpp        # Thrift strategy (NEW)
                                     # Contains: thrift_metadata_builder class
                                     # Guarded: #ifdef DWARFS_HAVE_THRIFT

  flatbuffers_metadata_builder.cpp   # FlatBuffers strategy (NEW)
                                     # Contains: flatbuffers_metadata_builder class
                                     # Always compiled

  metadata_builder_factory.cpp       # Factory (NEW)
                                     # Creates correct strategy based on format
```

## Implementation Steps

### Step 1: Create Thrift Strategy
**File**: `src/writer/internal/thrift_metadata_builder.cpp`

**Action**: Move existing `metadata_builder_` class from `metadata_builder.cpp`

**Changes**:
1. Rename `metadata_builder_` → `thrift_metadata_builder`
2. Keep ALL existing logic (proven code)
3. Wrap entire file with `#ifdef DWARFS_HAVE_THRIFT`
4. `build()` method converts Thrift → domain (already done)

**Result**: Thrift strategy isolated, optional

### Step 2: Create FlatBuffers Strategy
**File**: `src/writer/internal/flatbuffers_metadata_builder.cpp`

**Action**: Create NEW implementation that builds domain model directly

**Key Difference from Thrift**:
```cpp
// Thrift strategy (2-step):
class thrift_metadata_builder {
  thrift::metadata::metadata md_;  // Build Thrift

  domain::metadata build() {
    auto thrift_md = build_thrift_internal();
    return converters::from_thrift(thrift_md);  // Convert
  }
};

// FlatBuffers strategy (direct):
class flatbuffers_metadata_builder {
  domain::metadata md_;  // Build domain directly!

  domain::metadata build() {
    update_nlink();
    update_totals();
    pack_metadata();
    return std::move(md_);  // No conversion!
  }
};
```

**Implementation**:
- Copy logic from `thrift_metadata_builder`
- Adapt to work directly with domain types
- No `#ifdef` guards needed (always available)

**Result**: FlatBuffers strategy, no Thrift dependency

### Step 3: Create Factory
**File**: `src/writer/internal/metadata_builder_factory.cpp`

**Responsibility**: Create appropriate strategy based on format

```cpp
std::unique_ptr<metadata_builder::impl>
create_builder_impl(logger& lgr, metadata_options const& options,
                   SerializationFormat format) {
  switch (format) {
#ifdef DWARFS_HAVE_THRIFT
    case SerializationFormat::THRIFT_COMPACT:
      return make_unique<thrift_metadata_builder>(lgr, options);
#endif

    case SerializationFormat::FLATBUFFERS:
      return make_unique<flatbuffers_metadata_builder>(lgr, options);

    default:
      throw std::runtime_error("Unsupported format");
  }
}
```

**Result**: Encapsulated creation logic

### Step 4: Simplify metadata_builder.cpp
**File**: `src/writer/internal/metadata_builder.cpp`

**Responsibility**: Delegate to factory (thin wrapper)

**Before** (monolithic, ~1200 lines):
```cpp
// Entire implementation in this file
template <typename LoggerPolicy>
class metadata_builder_ {
  // ... 1200 lines of implementation ...
};
```

**After** (delegation, ~50 lines):
```cpp
// Just constructors that call factory
metadata_builder::metadata_builder(logger& lgr, metadata_options const& options)
    : impl_{metadata_builder_factory::create(lgr, options,
                                              options.metadata_format)} {}

// ... other constructors similar ...
```

**Result**: Thin interface implementation

### Step 5: Update Build System
**File**: `cmake/libdwarfs.cmake`

**Change**:
```cmake
# Before:
$<$<BOOL:${DWARFS_HAVE_THRIFT}>:src/writer/internal/metadata_builder.cpp>

# After:
src/writer/internal/metadata_builder.cpp  # Always compiled (thin wrapper)
src/writer/internal/metadata_builder_factory.cpp  # Always compiled
$<$<BOOL:${DWARFS_HAVE_THRIFT}>:src/writer/internal/thrift_metadata_builder.cpp>
src/writer/internal/flatbuffers_metadata_builder.cpp  # Always compiled
```

**Result**: Conditional compilation works correctly

## Architecture Benefits

### 1. True Format Independence
- Each format in separate file
- No cross-dependencies
- Clean compilation boundaries

### 2. Extensibility
- Add Protobuf? Create `protobuf_metadata_builder.cpp`
- Add Avro? Create `avro_metadata_builder.cpp`
- No changes to existing code

### 3. Testability
- Mock factory for unit tests
- Test each strategy independently
- Integration tests via factory

### 4. Maintainability
- Clear file organization
- Easy to locate format-specific code
- Minimal cognitive load

### 5. Build Flexibility
- Thrift optional at compile-time
- FlatBuffers always available
- Link only what you need

## Migration Risk Mitigation

### Risk 1: Breaking Existing Code
**Mitigation**: Keep Thrift logic unchanged, just move to new file

### Risk 2: Build Breaks
**Mitigation**: Update CMake carefully, test incrementally

### Risk 3: Runtime Failures
**Mitigation**: Factory validates request format, throws clear errors

### Risk 4: Performance Regression
**Mitigation**: Domain model conversion is lightweight, benchmark to verify

## Timeline

- **Step 1** (Thrift strategy): 4 hours - Move code, rename class
- **Step 2** (FlatBuffers strategy): 8 hours - Adapt Thrift logic to domain-direct
- **Step 3** (Factory): 2 hours - Simple factory implementation
- **Step 4** (Simplify wrapper): 1 hour - Thin delegation layer
- **Step 5** (CMake): 2 hours - Build system updates
- **Testing**: 4 hours - Both configurations

**Total**: ~3 days (including buffer)

## Success Criteria

- [ ] Two separate strategy files exist
- [ ] Factory creates correct implementation
- [ ] Builds with Thrift enabled
- [ ] Builds with Thrift disabled (FlatBuffers-only)
- [ ] All tests pass with both configurations
- [ ] AppleClang 17 builds successfully
- [ ] No `#ifdef` in application code (scanner, etc.)

## Ready to Execute

All planning complete. Architecture is sound. Ready to implement Step 1.

**Next Command**: Create `thrift_metadata_builder.cpp` by moving code from `metadata_builder.cpp`