# Session 24 Continuation Prompt

**Date**: 2025-12-22
**Blocked By**: Session 22/23 architectural error
**Read First**: [`SESSION_24_OOP_ARCHITECTURE_FIX_PLAN.md`](SESSION_24_OOP_ARCHITECTURE_FIX_PLAN.md)

## Critical Context

Session 22 successfully implemented FlatBuffers direct reader but left a **critical architectural flaw**: both [`metadata_v2_thrift.cpp`](../src/reader/internal/metadata_v2_thrift.cpp:298) and [`metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp:311) define the same class `metadata_v2_data` in namespace `dwarfs::reader::internal`, causing 20 duplicate symbol errors at link time.

## Your Mission

Fix the duplicate symbol issue using **proper OOP architecture** (Strategy + Bridge patterns with anonymous namespaces), then complete component refactoring.

## Phase A: Isolate Thrift Backend (START HERE)

### Step 1: Wrap in Anonymous Namespace

Edit [`src/reader/internal/metadata_v2_thrift.cpp`](../src/reader/internal/metadata_v2_thrift.cpp):

**Before line 104** (before first anonymous namespace):
```cpp
namespace dwarfs::reader::internal {

using namespace dwarfs::internal;
namespace fs = std::filesystem;
namespace tb = thrift_backend;

namespace {
```

**After line 296** (after existing anonymous namespace closes):
```cpp
} // namespace

namespace {  // Backend-specific anonymous namespace

class metadata_v2_data {
  // ... entire class ...
};

template <typename LoggerPolicy>
class metadata_ final : public metadata_v2::impl {
  // ... entire class ...
};

} // anonymous namespace (metadata_v2_data, metadata_)
```

**Keep in outer namespace** (lines 2371-2437):
```cpp
// MUST stay in dwarfs::reader::internal namespace
metadata_v2_utils::metadata_v2_utils(...) { ... }
// ... other metadata_v2_utils methods ...

#ifdef DWARFS_HAVE_THRIFT
// Factory function - MUST be in outer namespace for factory to call
#if defined(DWARFS_HAVE_FLATBUFFERS) && defined(DWARFS_HAVE_THRIFT)
metadata_v2 make_metadata_v2_thrift(...) { ... }
#else
metadata_v2::metadata_v2(...) { ... }
#endif
#endif
```

### Step 2: Build and Verify

```bash
cd build && ninja -j4 dwarfsck 2>&1 | grep -E "(duplicate|error)"
```

**Expected**: Fewer duplicate symbols (FlatBuffers still duplicates)

## Phase B: Isolate FlatBuffers Backend

Apply same pattern to [`src/reader/internal/metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp):

1. Wrap `metadata_v2_data` (line 311) in anonymous namespace
2. Wrap `metadata_` template (line 2260) in anonymous namespace
3. Keep `make_metadata_v2_flatbuffers()` (line 2460) in outer namespace
4. Keep conditional `metadata_v2_utils` (line 2435) in outer namespace

## Phase C: Verify No Duplicate Symbols

```bash
cd build
ninja clean
ninja -j4 mkdwarfs dwarfsck dwarfsextract
```

**Success Criteria**:
- ✅ Clean compilation
- ✅ All 3 tools built
- ✅ No "duplicate symbol" errors
- ✅ `nm libdwarfs_reader.a | grep metadata_v2_data` → Empty (symbols are local)

## Phase D: Test FlatBuffers Fix

### Test 1: dwarfsck
```bash
./build/dwarfsck -l example/static-site-server/aesop.dff
```

**Expected**:
- ✅ Lists all 117 files
- ✅ **NO** "converting to internal Thrift format" message
- ✅ **NO** "data size mismatch" error

### Test 2: static-site-server
```bash
cd example/static-site-server
./build/static-site-server --image aesop.dff --port 8080 &
curl http://localhost:8080/
kill %1
```

**Expected**:
- ✅ Server starts without errors
- ✅ Returns HTML content
- ✅ All files accessible

### Test 3: Extraction
```bash
./build/dwarfsextract -i example/static-site-server/aesop.dff -o /tmp/test-extract
diff -r /tmp/test-extract example/pg11339-h/
```

**Expected**: Files match exactly

## Phase E: Component Refactoring (OOP)

### Objective

Break monolithic `metadata_v2_data` classes (2400+ lines) into focused OOP components following **Single Responsibility Principle**.

### Thrift Backend Components

Create these files in `src/reader/internal/`:

#### 1. `thrift_consistency_checker.h` + `.cpp` (~350 lines total)
```cpp
namespace dwarfs::reader::internal::thrift_backend {

class consistency_checker {
public:
  explicit consistency_checker(logger& lgr, Meta const& meta);

  void check_metadata() const;
  void check_inode_size_cache(...) const;

private:
  logger& lgr_;
  Meta const& meta_;
};

} // namespace
```

**Responsibility**: Validate metadata integrity only

#### 2. `thrift_cache_builder.h` + `.cpp` (~350 lines total)
```cpp
namespace dwarfs::reader::internal::thrift_backend {

class cache_builder {
public:
  static packed_int_vector<uint32_t> unpack_chunk_table(...);
  static packed_int_vector<uint32_t> unpack_shared_files(...);
  static std::optional<packed_int_vector<uint32_t>> build_nlinks(...);
  static std::vector<packed_int_vector<uint32_t>> build_dir_icase_cache(...);
};

} // namespace
```

**Responsibility**: Build runtime caches from packed metadata

#### 3. `thrift_file_operations.h` + `.cpp` (~350 lines total)
```cpp
namespace dwarfs::reader::internal::thrift_backend {

class file_operations {
public:
  // Constructor
  file_operations(Meta const& meta, ...);

  // Operations
  chunk_range get_chunks(int inode, std::error_code& ec) const;
  file_size_result file_size(...) const;
  file_off_t seek(...) const;

private:
  Meta const& meta_;
  // ... caches ...
};

} // namespace
```

**Responsibility**: File content access operations only

#### 4. `thrift_directory_navigator.h` + `.cpp` (~300 lines total)
```cpp
namespace dwarfs::reader::internal::thrift_backend {

class directory_navigator {
public:
  directory_navigator(Meta const& meta, ...);

  std::optional<dir_entry_view> find(std::string_view path) const;
  std::optional<dir_entry_view> find(directory_view, string_view) const;
  std::optional<dir_entry_view> readdir(directory_view, size_t) const;
  void walk(...) const;

private:
  Meta const& meta_;
  // ... caches ...
};

} // namespace
```

**Responsibility**: Directory traversal and lookup only

#### 5. `thrift_metadata_formatter.h` + `.cpp` (~300 lines total)
```cpp
namespace dwarfs::reader::internal::thrift_backend {

class metadata_formatter {
public:
  metadata_formatter(Meta const& meta);

  nlohmann::json as_json() const;
  nlohmann::json info_as_json(...) const;
  void dump(std::ostream&, ...) const;
  std::string serialize_as_json(bool simple) const;

private:
  Meta const& meta_;
};

} // namespace
```

**Responsibility**: Metadata serialization and formatting only

#### 6. Updated `metadata_v2_data` (~600 lines, down from 2439)
```cpp
namespace {  // Anonymous

class metadata_v2_data {
public:
  // Constructor (delegates to components)
  metadata_v2_data(...);

  // Delegating methods
  void check_consistency() const { checker_.check_metadata(); }
  auto find(string_view p) const { return navigator_.find(p); }
  auto get_chunks(int i, error_code& e) const { return file_ops_.get_chunks(i, e); }
  auto as_json() const { return formatter_.as_json(); }

private:
  // Components (Composition over Inheritance)
  consistency_checker checker_;
  cache_builder caches_;
  file_operations file_ops_;
  directory_navigator navigator_;
  metadata_formatter formatter_;

  // Metadata reference
  Meta const& meta_;
};

} // anonymous
```

**Benefits**:
- Main class is now orchestrator only
- Each component independently testable
- Clear separation of concerns
- Easy to extend (add component = new file)

### FlatBuffers Backend

Mirror the same structure with `flatbuffers_backend::` prefix.

## Testing Strategy

### After Each Component Extraction

1. Build: `ninja -j4 dwarfsck`
2. Test: `./dwarfsck -l example/static-site-server/aesop.dff`
3. Verify: Output unchanged

### Integration Testing

After all refactoring:
```bash
# Run full test suite
cd build
ctest --output-on-failure

# Benchmark
../benchmarks/run_metadata_format_benchmark.py
```

## Success Criteria

### Technical
- ✅ No duplicate symbols
- ✅ Clean compilation
- ✅ All tests pass
- ✅ Zero performance regression

### Architectural
- ✅ Strategy pattern properly applied
- ✅ Bridge pattern separates interface/implementation
- ✅ Anonymous namespaces enforce encapsulation
- ✅ Each class <800 lines
- ✅ Single responsibility throughout

### Functional
- ✅ FlatBuffers images work
- ✅ Thrift images work
- ✅ Factory dispatch correct
- ✅ No conversion overhead

## Documentation Requirements

Update these files in final phase:

1. **Architecture**: Add "Anonymous Namespace Isolation" section
2. **Context**: Update current work status
3. **Session docs**: Archive Sessions 22, 23, 24
4. **Memory bank**: Reflect new architecture

## Key Constraints

1. **NEVER lower pass thresholds** - If tests fail, fix the code
2. **NEVER skip testing** - Test after each major change
3. **ALWAYS use OOP** - Strategy, Bridge, Single Responsibility
4. **ALWAYS separate concerns** - Each file has ONE purpose

---

**Next Session**: Execute Phase A, then B, then C, then D, then E, then F
**Estimated Total**: 9.5-10.5 hours
**Priority**: CRITICAL - Blocks all FlatBuffers work