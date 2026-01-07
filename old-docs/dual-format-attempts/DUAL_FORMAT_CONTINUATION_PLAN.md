# Dual-Format Implementation - Continuation Plan

**Branch**: feature/multi-format-serialization-fuse
**Goal**: Complete functional dual-format support with native FlatBuffers backend
**Estimated Time**: 6-8 hours

## Current State Summary

**What Works** ✅:
- All libraries compile
- All tools compile
- FlatBuffers serialization creates valid images
- Thrift serialization creates valid images
- Thrift backend reads/extracts perfectly
- Format auto-detection via magic bytes
- `--format` option in mkdwarfs

**What's Broken** ❌:
- FlatBuffers images cannot be extracted (empty pathnames)
- Only Thrift backend gets compiled (CMake if/elseif)
- FlatBuffers→Thrift conversion loses compact_names data
- FlatBuffers native backend never used

## Root Cause Analysis

### Problem 1: CMake Only Compiles One Backend

**File**: [`cmake/libdwarfs.cmake:331-349`](../cmake/libdwarfs.cmake)

```cmake
if(DWARFS_HAVE_THRIFT)
  target_sources(dwarfs_reader PRIVATE metadata_v2_thrift.cpp)
elseif(DWARFS_HAVE_FLATBUFFERS)  # ← NEVER runs when Thrift enabled
  target_sources(dwarfs_reader PRIVATE metadata_v2_flatbuffers.cpp)
```

**Fix Required**:
```cmake
# Compile BOTH backends in dual-format builds
if(DWARFS_HAVE_THRIFT)
  target_sources(dwarfs_reader PRIVATE
    src/reader/internal/metadata_types_thrift.cpp
    src/reader/internal/metadata_v2_thrift.cpp
    src/reader/internal/time_resolution_handler.cpp)
endif()

if(DWARFS_HAVE_FLATBUFFERS)
  target_sources(dwarfs_reader PRIVATE
    src/reader/internal/metadata_types_flatbuffers.cpp
    src/reader/internal/metadata_v2_flatbuffers.cpp
    src/reader/internal/flatbuffer_metadata_views.cpp)
  if(NOT DWARFS_HAVE_THRIFT)
    target_sources(dwarfs_reader PRIVATE src/reader/internal/time_resolution_handler.cpp)
  endif()
endif()
```

### Problem 2: No Runtime Backend Selection

**Current**: Single `metadata_v2` constructor always uses Thrift backend
**Needed**: Factory that detects format and instantiates correct backend

**Architecture**:
```
metadata_v2::metadata_v2(schema, data, ...)
  ↓
Format detection (SerializerRegistry)
  ↓
┌─────────────────┬──────────────────┐
│ FLATBUFFERS     │ THRIFT_COMPACT   │
↓                 ↓                  │
make_metadata_v2_  make_metadata_v2_thrift()
flatbuffers()       ↓
↓                 Creates metadata_<LoggerPolicy>
Creates metadata_  with MappedFrozen<thrift::...>
<LoggerPolicy>
with flatbuffers::
Metadata const*
```

### Problem 3: Conversion Layer is Buggy

**File**: [`src/reader/internal/metadata_v2_thrift.cpp:649-693`](../src/reader/internal/metadata_v2_thrift.cpp)

**Current Flow**:
```
FlatBuffers image
  ↓
FlatBuffersSerializer::deserialize() → domain::metadata
  ↓
DomainThriftConverter::to_thrift() → thrift::metadata::metadata
  ↓
Thrift Frozen2 freeze
  ↓
MappedFrozen<thrift::metadata::metadata>
```

**Problem**: `compact_names` field gets corrupted/lost in conversion

**Solution**: DON'T CONVERT - use FlatBuffers backend directly!

## Implementation Roadmap

### Phase 1: Enable Dual-Backend Compilation (2 hours)

**Step 1.1**: Fix CMake to compile both backends
```cmake
# cmake/libdwarfs.cmake
if(DWARFS_HAVE_THRIFT)
  target_sources(dwarfs_reader PRIVATE
    src/reader/internal/metadata_types_thrift.cpp
    src/reader/internal/metadata_v2_thrift.cpp
    src/reader/internal/time_resolution_handler.cpp)
endif()

if(DWARFS_HAVE_FLATBUFFERS)
  target_sources(dwarfs_reader PRIVATE
    src/reader/internal/metadata_types_flatbuffers.cpp
    src/reader/internal/metadata_v2_flatbuffers.cpp
    src/reader/internal/flatbuffer_metadata_views.cpp)
  if(NOT DWARFS_HAVE_THRIFT)
    target_sources(dwarfs_reader PRIVATE src/reader/internal/time_resolution_handler.cpp)
  endif()
endif()
```

**Step 1.2**: Remove Thrift-only guards from metadata_v2_thrift.cpp
- Currently wrapped in `#ifdef DWARFS_HAVE_THRIFT` at line 1
- Remove this - CMake already conditionally compiles the file

**Step 1.3**: Move metadata_v2_utils to shared location
- Currently only in Thrift backend
- Needed by both backends
- Extract to separate file or keep one copy

**Step 1.4**: Test compilation
```bash
ninja -C build-dual
# Should compile BOTH backends
```

### Phase 2: Implement Runtime Factory (3 hours)

**Step 2.1**: Create factory in metadata_v2 constructor

**File**: [`src/reader/internal/metadata_v2_factory.cpp`](../src/reader/internal/metadata_v2_factory.cpp) (new file)

```cpp
#include <dwarfs/reader/internal/metadata_v2.h>
#include <dwarfs/metadata/serialization/serializer_registry.h>

namespace dwarfs::reader::internal {

// Forward declarations
#ifdef DWARFS_HAVE_THRIFT
extern metadata_v2 make_thrift_backend(...);
#endif
#ifdef DWARFS_HAVE_FLATBUFFERS
extern metadata_v2 make_flatbuffers_backend(...);
#endif

metadata_v2::metadata_v2(logger& lgr, ...) {
  // Detect format
  auto format = SerializerRegistry::instance().detect_format(data);

  // Dispatch to correct backend
#ifdef DWARFS_HAVE_FLATBUFFERS
  if (format == FLATBUFFERS) {
    *this = make_flatbuffers_backend(lgr, schema, data, ...);
    return;
  }
#endif
#ifdef DWARFS_HAVE_THRIFT
  if (format == THRIFT_COMPACT) {
    *this = make_thrift_backend(lgr, schema, data, ...);
    return;
  }
#endif

  throw runtime_error("Unsupported format");
}

} // namespace
```

**Step 2.2**: Export factory functions from backends

**In [`metadata_v2_thrift.cpp`](../src/reader/internal/metadata_v2_thrift.cpp)**:
```cpp
// At end of file, OUTSIDE any class
metadata_v2 make_thrift_backend(logger& lgr, ...) {
  metadata_v2 result;
  result.impl_ = make_unique_logging_object<...>(...);
  return result;
}
```

**In [`metadata_v2_flatbuffers.cpp`](../src/reader/internal/metadata_v2_flatbuffers.cpp)**:
```cpp
// At end of file
metadata_v2 make_flatbuffers_backend(logger& lgr, ...) {
  metadata_v2 result;
  result.impl_ = make_unique_logging_object<...>(...);
  return result;
}
```

**Step 2.3**: Update metadata_v2.h

**File**: [`include/dwarfs/reader/internal/metadata_v2.h`](../include/dwarfs/reader/internal/metadata_v2.h)

Add to class:
```cpp
class metadata_v2 {
 public:
  // Make impl_ accessible to factory functions
  friend metadata_v2 make_thrift_backend(...);
  friend metadata_v2 make_flatbuffers_backend(...);

 private:
  std::unique_ptr<impl> impl_;
};
```

**Step 2.4**: Remove conversion code from metadata_v2_thrift.cpp

**File**: src/reader/internal/metadata_v2_thrift.cpp:649-693

Delete the entire block:
```cpp
// For FlatBuffers format, deserialize and re-freeze to Thrift
if (detected.has_value() &&
    *detected == metadata::serialization::SerializationFormat::FLATBUFFERS) {
  // ... 40 lines of conversion code ...
}
```

Replace with:
```cpp
// Native Thrift frozen format - use directly
return check_frozen(map_frozen<thrift::metadata::metadata>(schema_, data_));
```

**Step 2.5**: Test both backends work
```bash
# Test FlatBuffers backend is actually used
./build-dual/dwarfsextract -i test-fb.dwarfs -o out/
# Should NOT see "converting to internal Thrift format"
# Should extract successfully

# Test Thrift backend still works
./build-dual/dwarfsextract -i test-thrift.dwarfs -o out/
# Should extract successfully
```

### Phase 3: OOP Refactoring of mkdwarfs_main.cpp (2 hours)

**Current Problem**: 1585-line monolithic file

**Target Architecture**:
```
tools/
├── src/
│   ├── mkdwarfs_main.cpp           (100 lines - main entry point)
│   └── mkdwarfs/
│       ├── options_parser.h/cpp    (parse command-line options)
│       ├── level_config.h/cpp      (compression level defaults)
│       ├── categorizer_config.h/cpp (categorizer setup)
│       ├── filesystem_creator.h/cpp (create filesystem)
│       └── filesystem_rewriter.h/cpp (recompress)
```

**Step 3.1**: Extract Options Parser Class

**New File**: `tools/src/mkdwarfs/options_parser.h`
```cpp
namespace dwarfs::tool::mkdwarfs {

class OptionsParser {
 public:
  OptionsParser(iolayer const& iol);

  struct ParsedOptions {
    // All the variables currently scattered in main()
    writer::scanner_options scanner_opts;
    writer::segmenter_factory::config segmenter_config;
    std::string output_path;
    unsigned compression_level;
    // ... etc
  };

  std::optional<ParsedOptions> parse(int argc, sys_char** argv);

 private:
  iolayer const& iol_;
  // Helper methods for parsing specific option groups
  void add_basic_options(po::options_description& opts);
  void add_advanced_options(po::options_description& opts);
  // ...
};

} // namespace
```

**Step 3.2**: Extract Level Configuration

**New File**: `tools/src/mkdwarfs/level_config.h`
```cpp
namespace dwarfs::tool::mkdwarfs {

struct LevelDefaults {
  unsigned block_size_bits;
  std::string data_compression;
  std::string schema_compression;
  std::string metadata_compression;
  unsigned window_size;
  unsigned window_step;
  std::string order;
};

class LevelConfig {
 public:
  static LevelDefaults const& get(unsigned level);
  static constexpr unsigned DEFAULT_LEVEL = 7;
  static constexpr unsigned MAX_LEVEL = 9;

 private:
  static std::array<LevelDefaults, 10> const levels_;
};

} // namespace
```

**Step 3.3**: Extract Filesystem Creator

**New File**: `tools/src/mkdwarfs/filesystem_creator.h`
```cpp
namespace dwarfs::tool::mkdwarfs {

class FilesystemCreator {
 public:
  FilesystemCreator(logger& lgr, OptionsParser::ParsedOptions const& opts);

  int create(std::filesystem::path const& input_path,
             std::ostream& output,
             writer::writer_progress& progress);

 private:
  logger& lgr_;
  OptionsParser::ParsedOptions const& opts_;

  void setup_scanner();
  void setup_segmenter();
  void setup_compressors();
};

} // namespace
```

**Step 3.4**: Update CMakeLists.txt
```cmake
add_executable(mkdwarfs
  tools/src/mkdwarfs_main.cpp
  tools/src/mkdwarfs/options_parser.cpp
  tools/src/mkdwarfs/level_config.cpp
  tools/src/mkdwarfs/filesystem_creator.cpp
  tools/src/mkdwarfs/filesystem_rewriter.cpp
)
```

**Step 3.5**: Refactor main() to use classes
```cpp
int mkdwarfs_main(int argc, sys_char** argv, iolayer const& iol) {
  OptionsParser parser(iol);
  auto opts = parser.parse(argc, argv);
  if (!opts) return opts.error();

  writer::console_writer lgr(...);
  writer::writer_progress prog(...);

  if (opts->recompress) {
    FilesystemRe writer rewriter(lgr, *opts);
    return rewriter.run(prog);
  } else {
    FilesystemCreator creator(lgr, *opts);
    return creator.run(opts->input_path, opts->output, prog);
  }
}
```

### Phase 4: File Extension Updates (1 hour)

**Step 4.1**: Update default extensions in mkdwarfs

**File**: [`tools/src/mkdwarfs_main.cpp`](../tools/src/mkdwarfs_main.cpp) (or new options_parser.cpp)

```cpp
std::string get_default_extension(SerializationFormat format) {
  switch (format) {
    case FLATBUFFERS: return ".dff";  // DwarFS FlatBuffer
    case THRIFT_COMPACT: return ".dft";  // DwarFS Thrift
    default: return ".dwarfs";  // Fallback
  }
}

// In output path logic:
if (output_str.empty()) {
  output = path_str + get_default_extension(opts.metadata_format);
}
```

**Step 4.2**: Update documentation mentions

Search and replace in:
- README.adoc
- doc/mkdwarfs.md
- doc/dwarfs.md
- doc/dwarfsck.md
- doc/dwarfsextract.md
- doc/dwarfs-format.md

**Step 4.3**: Update examples

- example/ directory
- Test scripts
- Benchmark scripts

### Phase 5: Testing & Validation (1 hour)

**Test Matrix**:

| Operation | FlatBuffers | Thrift | Cross-Format |
|-----------|-------------|--------|--------------|
| Create    | ✅ Test    | ✅ Test | N/A |
| Verify    | ✅ Test    | ✅ Test | N/A |
| Extract   | ✅ Test    | ✅ Test | N/A |
| Read (FUSE) | ✅ Test  | ✅ Test | N/A |
| Recompress | ✅ Test  | ✅ Test | ✅ FB→Thrift, Thrift→FB |

**Test Commands**:
```bash
# Create

 test filesystems
./build-dual/mkdwarfs -i testdata -o test.dff --format=flatbuffers
./build-dual/mkdwarfs -i testdata -o test.dft --format=thrift

# Verify both
./build-dual/dwarfsck test.dff
./build-dual/dwarfsck test.dft

# Extract both
./build-dual/dwarfsextract -i test.dff -o out-fb/
./build-dual/dwarfsextract -i test.dft -o out-thrift/

# Compare extracted content
diff -r out-fb/ out-thrift/ # Should be identical

# Test recompression (format conversion)
./build-dual/mkdwarfs --recompress=metadata --format=thrift -i test.dff -o test-converted.dft
./build-dual/mkdwarfs --recompress=metadata --format=flatbuffers -i test.dft -o test-converted.dff
```

### Phase 6: Benchmarking (1 hour)

**Once extraction works**, run performance comparison:

```bash
# Download or prepare test dataset
python3 benchmarks/download_datasets.py --download perl

# Run comparison
python3 benchmarks/run_complete_comparison.py \
  --dataset benchmark-files/perl-5.43.3 \
  --runs 3

# Expected results: DUAL_FORMAT_BENCHMARK_RESULTS.md
# - Format sizes (Thrift ~10% smaller)
# - Compression speed (similar)
# - Read latency (FlatBuffers slightly slower - no Frozen2)
# - Memory usage
```

### Phase 7: Documentation Updates (1 hour)

**Files to Update**:

1. **README.adoc**:
   - Add "Metadata Formats" section
   - Document --format option
   - Explain .dff vs .dft extensions
   - Migration guide from .dwarfs

2. **doc/mkdwarfs.md**:
   - Document --format option
   - Add format comparison table
   - Update examples with .dff/.dft

3. **doc/dwarfs-format.md**:
   - Update format version table
   - Document FlatBuffers format (v2.5)
   - Keep Thrift format docs (legacy)

4. **CHANGES.md**:
   ```markdown
   ## v0.16.0 (Unreleased)

   ### New Features
   - **Dual-format metadata support**: FlatBuffers (default) and Thrift (legacy)
   - New `--format` option in mkdwarfs
   - File extensions: .dff (FlatBuffers), .dft (Thrift)
   - Automatic format detection

   ### Breaking Changes
   - Removed Cereal and Bitsery formats (replaced by FlatBuffers)

   ### Migration
   - Existing .dwarfs images remain compatible
   - Recommended: Recompress to FlatBuffers format
   - Command: `mkdwarfs --recompress=metadata --format=flatbuffers -i old.dwarfs -o new.dff`
   ```

## Implementation Checklist

### Phase 1: Dual-Backend Compilation
- [ ] Update cmake/libdwarfs.cmake (if → if/if)
- [ ] Remove #ifdef wrapper from metadata_v2_thrift.cpp
- [ ] Handle metadata_v2_utils (shared or duplicated)
- [ ] Test: Both .cpp files compile
- [ ] Test: No duplicate symbol errors

### Phase 2: Runtime Factory
- [ ] Create src/reader/internal/metadata_v2_factory.cpp
- [ ] Update metadata_v2.h with friend declarations
- [ ] Export make_*_backend() from both backends
- [ ] Remove FlatBuffers→Thrift conversion code
- [ ] Test: Format detection works
- [ ] Test: Correct backend instantiated
- [ ] Test: No "converting to internal Thrift format" message for .dff

### Phase 3: Extraction Validation
- [ ] Test: Extract .dff image successfully
- [ ] Test: Extract .dft image successfully
- [ ] Test: Extracted content identical
- [ ] Test: No "Invalid empty pathname" errors

### Phase 4: OOP Refactoring
- [ ] Create tools/src/mkdwarfs/ directory
- [ ] Extract OptionsParser class
- [ ] Extract LevelConfig class
- [ ] Extract FilesystemCreator class
- [ ] Extract FilesystemRewriter class
- [ ] Update CMakeLists.txt
- [ ] Test: mkdwarfs still works identically

### Phase 5: File Extensions
- [ ] Implement get_default_extension()
- [ ] Update mkdwarfs output path logic
- [ ] Search/replace .dwarfs → .dff/.dft in docs
- [ ] Update examples
- [ ] Test: Default extensions work

### Phase 6: Benchmarking
- [ ] Run benchmarks/run_complete_comparison.py
- [ ] Generate performance report
- [ ] Document size differences (~10%)
- [ ] Document speed differences

### Phase 7: Documentation
- [ ] Update README.adoc
- [ ] Update tool man pages
- [ ] Update format specification
- [ ] Update CHANGES.md
- [ ] Add migration guide

### Phase 8: Cleanup
- [ ] Move old docs to old-docs/
- [ ] Remove debug logging
- [ ] Remove temporary test files
- [ ] Final commit with summary

## Critical Dependencies

**Must Complete in Order**:
1. Phase 1 → Phase 2 (need both backends before factory)
2. Phase 2 → Phase 3 (need factory before extraction works)
3. Phase 3 → Phase 6 (need extraction before benchmarks)
4. Phases 3-6 → Phase 7 (need working system before docs)
5. Phase 4 can happen in parallel with Phases 5-7

## Estimated Timeline

| Phase | Time | Blocker |
|-------|------|---------|
| 1. Dual-Backend | 2h | - |
| 2. Runtime Factory | 3h | Phase 1 |
| 3. Validation | 1h | Phase 2 |
| 4. OOP Refactor | 2h | - (parallel) |
| 5. Extensions | 1h | - (parallel) |
| 6. Benchmarks | 1h | Phase 3 |
| 7. Documentation | 1h | Phases 3,6 |
| 8. Cleanup | 0.5h | All |
| **Total** | **6-8h** (with parallelization) | |

## Success Criteria

- [ ] Both .dff and .dft images extract successfully
- [ ] No conversion between formats during read
- [ ] Benchmark report generated
- [ ] All documentation updated
- [ ] mkdwarfs_main.cpp under 500 lines
- [ ] OOP classes in separate files
- [ ] All tests pass
- [ ] Ready to merge to main branch

## Known Risks

**Risk 1**: metadata_v2_utils duplication
- **Issue**: Currently only in Thrift backend
- **Impact**: Might need to duplicate or extract to shared file
- **Mitigation**: Choose one approach and implement consistently

**Risk 2**: FlatBuffers backend never tested independently
- **Issue**: Has been compiled but never actually used
- **Impact**: Might have bugs we haven't found
- **Mitigation**: Thorough testing in Phase 3

**Risk 3**: Large refactoring might break existing functionality
- **Issue**: mkdwarfs is complex with many edge cases
- **Impact**: Could introduce regressions
- **Mitigation**: Extensive testing after each change