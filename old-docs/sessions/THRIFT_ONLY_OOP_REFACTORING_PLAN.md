# Thrift-Only Build - OOP Refactoring Plan

**Date**: 2025-12-02 23:40 HKT  
**Status**: Plan Created  
**Goal**: Fix dual-format build through proper OOP architecture  
**Estimated Time**: 3-4 hours

---

## Problem Analysis

### Current Issues

1. **Template Visibility**: Template classes defined in `.cpp` files cannot be instantiated from other translation units
2. **Large Files**: Implementation files exceed 1,200 lines (target: <800 lines each)
3. **Mixed Concerns**: Single classes handle multiple responsibilities
4. **Code Duplication**: Similar logic exists in both FlatBuffers and Thrift builders

### Root Cause

The previous refactoring (Phase A, 2025-12-02) reduced `metadata_builder.cpp` from 1,315 to 154 lines by extracting strategy implementations, but:
- Strategy classes remained as monolithic templates in `.cpp` files
- C++ templates require definitions in header files for visibility
- No proper separation of concerns within strategy classes

---

## Solution: Multi-Layer OOP Architecture

### Design Principles

1. **Single Responsibility**: Each class handles ONE aspect of metadata building
2. **Template Visibility**: Template classes declared in headers, defined inline or in `.tpp` files
3. **Dependency Inversion**: High-level modules depend on abstractions
4. **Open/Closed**: Extensible without modifying existing code
5. **File Size**: Target <800 lines per file, ideally <500 lines

### Proposed Architecture

```
metadata_builder (facade)
    ↓
strategy factory
    ↓
metadata_builder_strategy (abstract)
    ↙                    ↘
flatbuffers_strategy    thrift_strategy
    ↓                        ↓
    ├─ chunk_processor      ├─ chunk_processor
    ├─ entry_processor      ├─ entry_processor
    ├─ packing_processor    ├─ packing_processor
    ├─ upgrade_processor    ├─ upgrade_processor
    └─ validation_helper    └─ validation_helper
```

---

## Refactoring Tasks

### Phase 1: Extract Common Interfaces (1 hour)

**Goal**: Define abstract interfaces for shared responsibilities

#### 1.1 Create `metadata_chunk_processor.h` (250 lines)
```cpp
// Abstract interface for chunk processing
class metadata_chunk_processor {
public:
  virtual ~metadata_chunk_processor() = default;
  
  virtual void gather_chunks(
      inode_manager const&,
      block_manager const&,
      size_t chunk_count) = 0;
      
  virtual void remap_blocks(
      std::span<block_mapping const>,
      size_t new_block_count) = 0;
};
```

**Files to create**:
- `include/dwarfs/writer/internal/metadata_chunk_processor.h`
- `include/dwarfs/writer/internal/metadata_entry_processor.h`
- `include/dwarfs/writer/internal/metadata_packing_processor.h`
- `include/dwarfs/writer/internal/metadata_upgrade_processor.h`

#### 1.2 Extract Utility Classes (200 lines)
```cpp
// Inode size calculation
class inode_size_calculator {
public:
  struct size_info {
    size_t num_chunks;
    uint64_t size;
    uint64_t allocated_size;
  };
  
  explicit inode_size_calculator(metadata const&);
  size_info calculate(size_t inode_index) const;
};
```

**Files to create**:
- `include/dwarfs/writer/internal/inode_size_calculator.h`
- `src/writer/internal/inode_size_calculator.cpp`
- `include/dwarfs/writer/internal/metadata_validator.h`
- `src/writer/internal/metadata_validator.cpp`

### Phase 2: Refactor FlatBuffers Strategy (1.5 hours)

**Goal**: Break down 1,264-line `flatbuffers_metadata_builder.cpp` into focused classes

#### 2.1 Create Header with Template Declaration (150 lines)
**File**: `include/dwarfs/writer/internal/flatbuffers_metadata_builder_impl.h`

```cpp
template <typename LoggerPolicy>
class flatbuffers_metadata_builder final 
    : public metadata_builder::impl {
public:
  // Constructors
  flatbuffers_metadata_builder(logger&, metadata_options const&);
  
  // Interface implementation
  void gather_chunks(...) override;
  metadata::domain::metadata build() override;
  
private:
  // Delegate to processors
  std::unique_ptr<metadata_chunk_processor> chunk_proc_;
  std::unique_ptr<metadata_entry_processor> entry_proc_;
  std::unique_ptr<metadata_packing_processor> pack_proc_;
  std::unique_ptr<metadata_upgrade_processor> upgrade_proc_;
  
  LOG_PROXY_DECL(LoggerPolicy);
  metadata::domain::metadata md_;
};
```

#### 2.2 Create Processor Implementations (4 files × ~200 lines)

**Files to create**:
- `src/writer/internal/flatbuffers_chunk_processor.h` (150 lines)
- `src/writer/internal/flatbuffers_chunk_processor.cpp` (300 lines)
- `src/writer/internal/flatbuffers_entry_processor.h` (100 lines)
- `src/writer/internal/flatbuffers_entry_processor.cpp` (200 lines)
- `src/writer/internal/flatbuffers_packing_processor.h` (120 lines)
- `src/writer/internal/flatbuffers_packing_processor.cpp` (250 lines)
- `src/writer/internal/flatbuffers_upgrade_processor.h` (100 lines)
- `src/writer/internal/flatbuffers_upgrade_processor.cpp` (350 lines)

#### 2.3 Slim Down Main Strategy File (400 lines)
**File**: `src/writer/internal/flatbuffers_metadata_builder.cpp`

```cpp
// Constructor delegates to processors
template <typename LoggerPolicy>
flatbuffers_metadata_builder<LoggerPolicy>::flatbuffers_metadata_builder(
    logger& lgr, metadata_options const& options)
    : LOG_PROXY_INIT(lgr)
    , chunk_proc_{std::make_unique<flatbuffers_chunk_processor>(md_, options)}
    , entry_proc_{std::make_unique<flatbuffers_entry_processor>(md_, options)}
    , pack_proc_{std::make_unique<flatbuffers_packing_processor>(md_, options)}
    , upgrade_proc_{std::make_unique<flatbuffers_upgrade_processor>(md_)} {
}

// Interface methods delegate
template <typename LoggerPolicy>
void flatbuffers_metadata_builder<LoggerPolicy>::gather_chunks(
    inode_manager const& im, block_manager const& bm, size_t chunk_count) {
  chunk_proc_->gather_chunks(im, bm, chunk_count);
}

// Explicit instantiations
template class flatbuffers_metadata_builder<debug_logger_policy>;
template class flatbuffers_metadata_builder<prod_logger_policy>;
```

**Result**: Main file reduced from 1,264 lines to ~400 lines

### Phase 3: Refactor Thrift Strategy (1 hour)

**Goal**: Apply same processor pattern to Thrift strategy

#### 3.1 Create Thrift Processors (4 files × ~200 lines)

**Files to create** (parallel to FlatBuffers):
- `src/writer/internal/thrift_chunk_processor.{h,cpp}`
- `src/writer/internal/thrift_entry_processor.{h,cpp}`
- `src/writer/internal/thrift_packing_processor.{h,cpp}`
- `src/writer/internal/thrift_upgrade_processor.{h,cpp}`

#### 3.2 Create Thrift Strategy Header (150 lines)
**File**: `include/dwarfs/writer/internal/thrift_metadata_builder_impl.h`

Same pattern as FlatBuffers.

#### 3.3 Slim Down Thrift Strategy (400 lines)
**File**: `src/writer/internal/thrift_metadata_builder.cpp`

**Result**: Main file reduced from 1,285 lines to ~400 lines

### Phase 4: Fix Factory and Build System (30 minutes)

#### 4.1 Update Factory to Use Headers
**File**: `src/writer/internal/metadata_builder_factory.cpp`

```cpp
#ifdef DWARFS_HAVE_FLATBUFFERS
#include "flatbuffers_metadata_builder_impl.h"
#endif

#ifdef DWARFS_HAVE_THRIFT
#include "thrift_metadata_builder_impl.h"
#endif
```

#### 4.2 Update CMake
**File**: `cmake/libdwarfs.cmake`

Add all new processor files to build.

### Phase 5: Verification (30 minutes)

#### 5.1 Build All Configurations
```bash
# FlatBuffers-only
rm -rf build-fb && mkdir build-fb && cd build-fb
cmake .. -GNinja -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=ON
ninja

# Thrift-only  
rm -rf build-tb && mkdir build-tb && cd build-tb
cmake .. -GNinja -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON -DWITH_TESTS=ON
ninja

# Dual-format (KEY TEST!)
rm -rf build-dual && mkdir build-dual && cd build-dual
cmake .. -GNinja -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON -DWITH_TESTS=ON
ninja
```

#### 5.2 Run Tests
```bash
cd build-dual
./dwarfs_unit_tests
```

---

## File Structure After Refactoring

```
include/dwarfs/writer/internal/
├── metadata_builder.h                          (existing, ~100 lines)
├── metadata_chunk_processor.h                  (NEW, ~150 lines)
├── metadata_entry_processor.h                  (NEW, ~120 lines)
├── metadata_packing_processor.h                (NEW, ~130 lines)
├── metadata_upgrade_processor.h                (NEW, ~100 lines)
├── inode_size_calculator.h                     (NEW, ~80 lines)
├── metadata_validator.h                        (NEW, ~70 lines)
├── flatbuffers_metadata_builder_impl.h         (NEW, ~200 lines)
└── thrift_metadata_builder_impl.h              (NEW, ~200 lines)

src/writer/internal/
├── metadata_builder.cpp                        (existing, 154 lines)
├── metadata_builder_factory.cpp                (existing, ~100 lines)
├── inode_size_calculator.cpp                   (NEW, ~150 lines)
├── metadata_validator.cpp                      (NEW, ~120 lines)
├── flatbuffers_metadata_builder.cpp            (REFACTORED, ~400 lines)
├── flatbuffers_chunk_processor.h               (NEW, ~150 lines)
├── flatbuffers_chunk_processor.cpp             (NEW, ~300 lines)
├── flatbuffers_entry_processor.h               (NEW, ~100 lines)
├── flatbuffers_entry_processor.cpp             (NEW, ~200 lines)
├── flatbuffers_packing_processor.h             (NEW, ~120 lines)
├── flatbuffers_packing_processor.cpp           (NEW, ~250 lines)
├── flatbuffers_upgrade_processor.h             (NEW, ~100 lines)
├── flatbuffers_upgrade_processor.cpp           (NEW, ~350 lines)
├── thrift_metadata_builder.cpp                 (REFACTORED, ~400 lines)
├── thrift_chunk_processor.h                    (NEW, ~150 lines)
├── thrift_chunk_processor.cpp                  (NEW, ~300 lines)
├── thrift_entry_processor.h                    (NEW, ~100 lines)
├── thrift_entry_processor.cpp                  (NEW, ~200 lines)
├── thrift_packing_processor.h                  (NEW, ~120 lines)
├── thrift_packing_processor.cpp                (NEW, ~250 lines)
├── thrift_upgrade_processor.h                  (NEW, ~100 lines)
└── thrift_upgrade_processor.cpp                (NEW, ~350 lines)
```

**Total**: 
- 28 files (10 existing modified, 18 new)
- All files <500 lines (target achieved)
- Clear separation of concerns
- Proper template visibility

---

## Benefits of This Architecture

1. **Testability**: Each processor can be unit tested independently
2. **Maintainability**: Small, focused files are easier to understand
3. **Extensibility**: New processors can be added without modifying existing code
4. **Reusability**: Common processors can be shared between strategies
5. **Template Visibility**: Headers provide proper template instantiation points
6. **Build Performance**: Smaller files compile faster
7. **Code Review**: Smaller diffs are easier to review

---

## Success Criteria

- ✅ All files <800 lines (target: <500 lines)
- ✅ Dual-format build compiles successfully
- ✅ All three configurations pass tests
- ✅ No code duplication between strategies
- ✅ Clear separation of concerns
- ✅ Proper OOP architecture (SOLID principles)

---

## Timeline

| Phase | Task | Duration | Dependencies |
|-------|------|----------|--------------|
| 1 | Extract interfaces & utilities | 1h | None |
| 2 | Refactor FlatBuffers strategy | 1.5h | Phase 1 |
| 3 | Refactor Thrift strategy | 1h | Phase 1 |
| 4 | Fix factory & build system | 30m | Phases 2-3 |
| 5 | Verification & testing | 30m | Phase 4 |
| **Total** | **4.5 hours** | | |

---

## Next Steps

1. Start with Phase 1 (interfaces & utilities)
2. Implement FlatBuffers processors (Phase 2)
3. Apply same pattern to Thrift (Phase 3)
4. Update build system (Phase 4)
5. Verify all configurations (Phase 5)

**Start Here**: Phase 1.1 - Create `metadata_chunk_processor.h`