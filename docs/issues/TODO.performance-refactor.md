# DwarFS Architecture Improvement Plan

> Generated: 2026-02-20
> Status: Proposed
> Estimated Timeline: 7-11 weeks

## Executive Summary

After deep analysis across 15 dimensions (file size, performance, API design, extensibility, dependencies, code patterns, testing, prioritization, and risk), this plan will transform DwarFS into:

- **Clean Architecture**: All files under 700 lines, clear separation of concerns
- **High Performance**: 2-4x throughput improvement via lock optimization
- **Easy to Use**: Symmetric reader/writer APIs, consistent error handling
- **Easy to Extend**: Plugin architecture for codecs and formats
- **No Technical Debt**: Test coverage, documentation, modern build

---

## Current State Analysis

### File Size Issues (Files exceeding 700 lines)

| File | Lines | Category |
|------|-------|----------|
| `unicode_case_folding.cpp` | 3036 | Generated data (acceptable) |
| `segmenter.cpp` | 1998 | Core algorithm |
| `filesystem_v2.cpp` | 1712 | Main filesystem |
| `domain_metadata_impl.cpp` | 1643 | Metadata operations |
| `common_metadata_operations.cpp` | 1378 | Shared operations |
| `thrift_metadata_builder.cpp` | 1353 | Metadata building |
| `fuse_driver.cpp` | 1263 | FUSE integration |
| `filesystem_writer.cpp` | 1245 | Writing logic |
| `pcmaudio_categorizer.cpp` | 1229 | Audio categorization |
| `frozen2_deserializer.cpp` | 1021 | Legacy format |
| `scanner.cpp` | 1048 | File scanning |

### Performance Hotspots

1. **Block Cache (`block_cache.cpp`)**
   - Single mutex for `cache_` and `active_` maps
   - `std::promise` allocation per read operation
   - Critical path for all read operations

2. **LRU Cache (`lru_cache.h`)**
   - `std::list` allocates on every insertion
   - Cache eviction causes memory churn

3. **Metadata Deserialization**
   - Frozen2 bit-packed parsing
   - FSST decompression (CPU intensive)

### API Gaps

1. **No simple_writer API** - asymmetric with simple_reader
2. **No streaming API** - large files must be fully loaded
3. **No async API** - blocking operations only
4. **Inconsistent error handling** - mix of exceptions and error codes

### Extensibility Issues

1. **Compression codecs hardcoded** - adding new codec requires core changes
2. **Metadata formats hardcoded** - 3 formats, not pluggable
3. **Categorizers not pluggable** - compile-time only

### Build Issues

1. **No minimal build target** - all compressors linked
2. **No CMake package exports** - no `find_package(dwarfs)`
3. **Complex dependency tree** - hard to customize

---

## Phase A: Performance Foundation (1-2 weeks)

### A.1 Block Cache Lock Optimization

**Problem**: Single mutex serializes all cache operations

**Solution**: Fine-grained locking

```cpp
// Before: Single mutex
mutable std::mutex mx_;  // Protects cache_, active_, lru_

// After: Separate locks by access pattern
mutable std::shared_mutex cache_mx_;  // Read-heavy (cache lookups)
mutable std::mutex active_mx_;        // Write-heavy (decompression)
mutable std::mutex lru_mx_;           // Eviction only
```

**Files to modify**:
- `src/reader/internal/block_cache.cpp`
- `include/dwarfs/reader/internal/block_cache.h`

**Expected Impact**: 2-4x throughput under concurrent read load

### A.2 LRU Cache Intrusive List

**Problem**: `std::list` allocates memory on every cache insertion

**Solution**: `boost::intrusive::list` with embedded hooks

```cpp
// Before
std::list<cache_entry> lru_;  // Allocates on insert

// After
#include <boost/intrusive/list.hpp>

struct cache_entry {
    // ... existing fields ...
    boost::intrusive::list_member_hook<> lru_hook_;
};

using lru_list = boost::intrusive::list<
    cache_entry,
    boost::intrusive::member_hook<
        cache_entry,
        boost::intrusive::list_member_hook<>,
        &cache_entry::lru_hook_
    >
>;
```

**Files to modify**:
- `include/dwarfs/reader/internal/lru_cache.h`
- `src/reader/internal/block_cache.cpp`

### A.3 Promise Pooling

**Problem**: `std::promise<block_range>` allocated per read

**Solution**: Object pool for promise reuse

```cpp
#include <boost/pool/object_pool.hpp>

class block_cache {
    // ...
    mutable boost::object_pool<std::promise<block_range>> promise_pool_{64};
};
```

**Files to modify**:
- `src/reader/internal/block_cache.cpp`

### A.4 Benchmark Suite

Create `benchmarks/block_cache_benchmark.cpp`:

```cpp
// Benchmark scenarios:
// 1. Single-threaded sequential read
// 2. Multi-threaded concurrent read (2, 4, 8, 16 threads)
// 3. Random access pattern
// 4. Sequential access pattern (with prefetch)
//
// Metrics:
// - Throughput (MB/s)
// - Latency (p50, p99, p999)
// - Memory allocation rate
// - Lock contention (via profiling)
```

**New files**:
- `benchmarks/block_cache_benchmark.cpp`
- `benchmarks/CMakeLists.txt`

---

## Phase B: API Completeness (1-2 weeks)

### B.1 simple_writer API

**Location**: `include/dwarfs/simple_writer.h`

```cpp
namespace dwarfs {

class simple_writer {
public:
    // One-shot: pack directory to image
    static bool pack(
        std::filesystem::path const& source_dir,
        std::filesystem::path const& output_image,
        std::string_view compression = "zstd");

    // Builder for options
    class builder {
    public:
        builder& compression(std::string_view codec, int level = 3);
        builder& block_size(size_t bytes);
        builder& exclude(std::vector<std::string> patterns);
        builder& include_hidden(bool include);
        builder& set_metadata_format(std::string_view format);
        simple_writer build();
    };

    // Incremental API
    bool add_file(std::filesystem::path const& src, std::string_view dest);
    bool add_directory(std::string_view path);
    bool add_symlink(std::string_view target, std::string_view link);
    bool write(std::filesystem::path const& output);

    // Error handling
    std::string last_error() const;
    bool has_error() const;
};

} // namespace dwarfs
```

**New files**:
- `include/dwarfs/simple_writer.h`
- `src/writer/simple_writer.cpp`

### B.2 C API Extension (Writer)

**Add to `include/dwarfs/c_api.h`**:

```cpp
// Writer handle
typedef struct dwarfs_writer_t* dwarfs_writer;

// Create/configure
dwarfs_writer dwarfs_writer_create(void);
int dwarfs_writer_set_compression(dwarfs_writer w, const char* codec, int level);
int dwarfs_writer_set_block_size(dwarfs_writer w, size_t bytes);

// Add content
int dwarfs_writer_add_file(dwarfs_writer w, const char* src, const char* dest);
int dwarfs_writer_add_directory(dwarfs_writer w, const char* path);

// Write image
int dwarfs_writer_write(dwarfs_writer w, const char* output_path);
void dwarfs_writer_close(dwarfs_writer w);
```

### B.3 Streaming Read API

**Location**: `include/dwarfs/stream_reader.h`

```cpp
namespace dwarfs {

class stream_reader {
public:
    explicit stream_reader(simple_reader const& reader, std::string_view path);

    // Read next chunk
    std::optional<std::span<const char>> read_chunk(size_t max_bytes = 64 * 1024);

    // Seek
    bool seek(int64_t offset);
    int64_t position() const;
    int64_t size() const;

    // Check status
    bool eof() const;
    bool has_error() const;
    std::string last_error() const;
};

} // namespace dwarfs
```

**New files**:
- `include/dwarfs/stream_reader.h`
- `src/reader/stream_reader.cpp`

### B.4 Error Handling Unification

Standardize error handling patterns:

```cpp
// Convenience API: std::optional<T>
std::optional<std::string> read(std::string_view path) const;

// Control API: std::error_code for detailed error info
std::error_code read(std::string_view path, std::string& out) const;

// Throw API: exceptions for unrecoverable errors
std::string read_or_throw(std::string_view path) const;
```

---

## Phase C: Code Quality (2-3 weeks)

### C.1 File Splitting (700-line rule)

**filesystem_v2.cpp (1712 → 3 files)**:
```
src/reader/filesystem_v2.cpp           (~300 lines - main entry)
src/reader/filesystem_v2_io.cpp        (~400 lines - read operations)
src/reader/filesystem_v2_metadata.cpp  (~400 lines - metadata access)
src/reader/filesystem_v2_cache.cpp     (~300 lines - cache management)
```

**segmenter.cpp (1998 → 3 files)**:
```
src/writer/segmenter.cpp               (~400 lines - main entry)
src/writer/segmenter_block.cpp         (~500 lines - block building)
src/writer/segmenter_analysis.cpp      (~500 lines - file analysis)
```

**domain_metadata_impl.cpp (1643 → 2 files)**:
```
src/reader/internal/domain_metadata_impl.cpp       (~400 lines - main)
src/reader/internal/domain_metadata_queries.cpp    (~500 lines - queries)
src/reader/internal/domain_metadata_walk.cpp       (~400 lines - traversal)
```

**fuse_driver.cpp (1263 → 2 files)**:
```
src/reader/fuse_driver.cpp             (~400 lines - main entry)
src/reader/fuse_driver_callbacks.cpp   (~500 lines - FUSE callbacks)
```

**frozen2_deserializer.cpp (1021 → 2 files)**:
```
src/metadata/legacy/frozen2_deserializer.cpp    (~400 lines - main)
src/metadata/legacy/frozen2_string_table.cpp    (~300 lines - string tables)
```

### C.2 API Documentation

Add Doxygen comments to all public headers:

```cpp
/**
 * @file simple_reader.h
 * @brief Simple convenience API for reading files from DwarFS images
 * @author Ribose Inc.
 *
 * @example{lineno}
 * auto content = dwarfs::simple_reader::read_file("image.dwarfs", "/index.html");
 * if (content) {
 *     std::cout << *content << std::endl;
 * }
 */

/**
 * @brief Read a file from the DwarFS image
 *
 * @param path Path within the image (e.g., "/index.html")
 * @return File content, or std::nullopt if not found or on error
 *
 * @note The file is read entirely into memory. For large files,
 *       consider using stream_reader for chunked reading.
 *
 * @see stream_reader
 * @see read_file() for one-shot reads
 */
std::optional<std::string> read(std::string_view path) const;
```

**Documentation generation**:
```bash
# Add Doxyfile configuration
doxygen Doxyfile  # Outputs to docs/api/
```

### C.3 Test Coverage

1. **Missing unit tests**:
   - `block_cache.cpp` - concurrent access tests
   - `segmenter.cpp` - algorithm edge cases
   - `simple_reader.cpp` - API tests
   - `stream_reader.cpp` - streaming tests

2. **Add performance regression tests**:
   ```cpp
   TEST(BlockCachePerformance, ConcurrentReads) {
       // Verify throughput doesn't regress
       auto throughput = measure_concurrent_read_throughput();
       EXPECT_GT(throughput, MINIMUM_THROUGHPUT_MBPS);
   }
   ```

3. **Enable coverage reporting**:
   ```cmake
   option(ENABLE_COVERAGE "Enable test coverage" OFF)
   if(ENABLE_COVERAGE)
       add_compile_options(--coverage -O0)
       add_link_options(--coverage)
   endif()
   ```

---

## Phase D: Build & Distribution (1 week)

### D.1 CMake Modernization

**Update `CMakeLists.txt`**:

```cmake
cmake_minimum_required(VERSION 3.20)
project(dwarfs VERSION 0.7.0 LANGUAGES CXX)

# Component options
option(DWARFS_BUILD_READER "Build reader library" ON)
option(DWARFS_BUILD_WRITER "Build writer library" ON)
option(DWARFS_BUILD_TOOLS "Build CLI tools (mkdwarfs, dwarfsck)" ON)
option(DWARFS_BUILD_FUSE "Build FUSE driver" ON)
option(DWARFS_BUILD_TESTS "Build tests" ON)
option(DWARFS_BUILD_DOCS "Build documentation" OFF)
option(DWARFS_BUILD_PYTHON "Build Python bindings" OFF)

# Minimal build option
option(DWARFS_MINIMAL_BUILD "Minimal build (zstd only, no FUSE)" OFF)

# Compression options
option(DWARFS_WITH_ZSTD "Enable zstd compression" ON)
option(DWARFS_WITH_LZ4 "Enable lz4 compression" ON)
option(DWARFS_WITH_LZMA "Enable lzma compression" ON)
option(DWARFS_WITH_BROTLI "Enable brotli compression" ON)
option(DWARFS_WITH_FLAC "Enable FLAC compression" OFF)

# Minimal build preset
if(DWARFS_MINIMAL_BUILD)
    set(DWARFS_WITH_ZSTD ON)
    set(DWARFS_WITH_LZ4 OFF)
    set(DWARFS_WITH_LZMA OFF)
    set(DWARFS_WITH_BROTLI OFF)
    set(DWARFS_WITH_FLAC OFF)
    set(DWARFS_BUILD_FUSE OFF)
    set(DWARFS_BUILD_TESTS OFF)
    set(DWARFS_BUILD_TOOLS OFF)
endif()

# Library targets
add_library(dwarfs-reader SHARED ${READER_SOURCES})
add_library(dwarfs-writer SHARED ${WRITER_SOURCES})

# Export targets
install(TARGETS dwarfs-reader dwarfs-writer
        EXPORT dwarfs-targets
        INCLUDES DESTINATION include)

# Package config
include(CMakePackageConfigHelpers)
install(EXPORT dwarfs-targets
        FILE dwarfs-targets.cmake
        NAMESPACE dwarfs::
        DESTINATION lib/cmake/dwarfs)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dwarfs-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dwarfs-config.cmake"
    INSTALL_DESTINATION lib/cmake/dwarfs)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dwarfs-config.cmake"
        DESTINATION lib/cmake/dwarfs)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/dwarfs-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
```

### D.2 User Integration

After installation, users can:

```cmake
# User's CMakeLists.txt
find_package(dwarfs REQUIRED COMPONENTS reader writer)

add_executable(my_app main.cpp)
target_link_libraries(my_app dwarfs::reader dwarfs::writer)
```

### D.3 vcpkg Integration

**Create `vcpkg.json`**:

```json
{
  "name": "dwarfs",
  "version": "0.7.0",
  "description": "Fast high compression read-only file system",
  "homepage": "https://github.com/tamatebako/dwarfs",
  "license": "GPL-3.0-only",
  "dependencies": [
    {
      "name": "fmt",
      "version>=": "9.0.0"
    },
    {
      "name": "boost-container-hash",
      "version>=": "1.80.0"
    },
    {
      "name": "boost-intrusive",
      "version>=": "1.80.0"
    },
    {
      "name": "zstd",
      "version>=": "1.5.0"
    },
    {
      "name": "parallelhashmap",
      "version>=": "1.3.0"
    }
  ],
  "features": {
    "writer": {
      "description": "Writer support with additional compressors",
      "dependencies": [
        "lz4",
        "liblzma",
        "brotli"
      ]
    },
    "fuse": {
      "description": "FUSE driver support",
      "dependencies": ["libfuse3"]
    },
    "flac": {
      "description": "FLAC audio compression support",
      "dependencies": ["libflac"]
    }
  }
}
```

---

## Phase E: Extensibility (2-3 weeks)

### E.1 Compression Codec Plugin Architecture

**Interface**: `include/dwarfs/plugin/codec_interface.h`

```cpp
namespace dwarfs {

class compression_codec {
public:
    virtual ~compression_codec() = default;

    // Identification
    virtual std::string_view name() const = 0;
    virtual std::vector<std::string> aliases() const { return {}; }
    virtual std::string_view description() const { return ""; }

    // Factory methods
    virtual std::unique_ptr<compressor> create_compressor(int level) = 0;
    virtual std::unique_ptr<decompressor> create_decompressor() = 0;

    // Capability queries
    virtual bool is_available() const { return true; }
    virtual std::vector<int> supported_levels() const { return {1, 3, 9, 19}; }
    virtual int default_level() const { return 3; }
};

// Singleton registry
class codec_registry {
public:
    static codec_registry& instance();

    void register_codec(std::unique_ptr<compression_codec> codec);
    std::vector<std::string> available_codecs() const;
    compression_codec* get_codec(std::string_view name) const;

    template<typename T>
    void register_codec() {
        register_codec(std::make_unique<T>());
    }
};

// Registrar helper for static registration
template<typename T>
struct codec_registrar {
    codec_registrar() {
        codec_registry::instance().register_codec<T>();
    }
};

} // namespace dwarfs

// Usage in codec implementation file:
// static dwarfs::codec_registrar<zstd_codec> const zstd_registrar;
```

**New files**:
- `include/dwarfs/plugin/codec_interface.h`
- `src/plugin/codec_registry.cpp`

### E.2 Metadata Format Plugin Architecture

**Interface**: `include/dwarfs/plugin/format_interface.h`

```cpp
namespace dwarfs {

class metadata_format {
public:
    virtual ~metadata_format() = default;

    // Identification
    virtual std::string_view name() const = 0;
    virtual std::string_view magic() const = 0;  // For format detection
    virtual std::string_view description() const { return ""; }

    // Factory methods
    virtual std::unique_ptr<metadata_serializer> create_serializer() = 0;
    virtual std::unique_ptr<metadata_deserializer> create_deserializer() = 0;

    // Capability queries
    virtual bool can_serialize() const { return true; }
    virtual bool can_deserialize() const { return true; }
    virtual bool can_detect(std::span<uint8_t const> data) const;
};

class metadata_format_registry {
public:
    static metadata_format_registry& instance();

    void register_format(std::unique_ptr<metadata_format> format);
    metadata_format* detect_format(std::span<uint8_t const> data) const;
    metadata_format* get_format(std::string_view name) const;
    std::vector<std::string> available_formats() const;
};

} // namespace dwarfs
```

**New files**:
- `include/dwarfs/plugin/format_interface.h`
- `src/plugin/format_registry.cpp`

### E.3 Categorizer Plugin Architecture

**Interface**: `include/dwarfs/plugin/categorizer_interface.h`

```cpp
namespace dwarfs {

class block_categorizer {
public:
    virtual ~block_categorizer() = default;

    // Identification
    virtual std::string_view name() const = 0;
    virtual std::string_view description() const { return ""; }

    // Categorization
    virtual std::optional<std::string> categorize(
        std::span<uint8_t const> data,
        std::string_view filename_hint) = 0;

    // Priority (higher = checked first)
    virtual int priority() const { return 0; }

    // Whether this categorizer can handle the given file
    virtual bool can_categorize(std::string_view filename_hint) const {
        return true;
    }
};

class categorizer_registry {
public:
    static categorizer_registry& instance();

    void register_categorizer(std::unique_ptr<block_categorizer> cat);
    std::optional<std::string> categorize(
        std::span<uint8_t const> data,
        std::string_view filename_hint) const;
    std::vector<std::string> available_categorizers() const;
};

} // namespace dwarfs
```

**New files**:
- `include/dwarfs/plugin/categorizer_interface.h`
- `src/plugin/categorizer_registry.cpp`

---

## Target File Structure

```
dwarfs/
├── include/dwarfs/
│   ├── simple_reader.h              # DONE
│   ├── simple_writer.h              # NEW
│   ├── stream_reader.h              # NEW
│   ├── c_api.h                      # DONE
│   ├── plugin/
│   │   ├── codec_interface.h        # NEW
│   │   ├── format_interface.h       # NEW
│   │   └── categorizer_interface.h  # NEW
│   └── ...
├── src/
│   ├── reader/
│   │   ├── simple_reader.cpp        # DONE
│   │   ├── stream_reader.cpp        # NEW
│   │   ├── c_api.cpp                # DONE
│   │   ├── filesystem_v2.cpp        # REDUCED
│   │   ├── filesystem_v2_io.cpp     # NEW
│   │   ├── filesystem_v2_metadata.cpp # NEW
│   │   ├── fuse_driver.cpp          # REDUCED
│   │   ├── fuse_driver_callbacks.cpp # NEW
│   │   └── internal/
│   │       ├── block_cache.cpp      # OPTIMIZED
│   │       ├── domain_metadata_impl.cpp    # REDUCED
│   │       ├── domain_metadata_queries.cpp # NEW
│   │       └── domain_metadata_walk.cpp    # NEW
│   ├── writer/
│   │   ├── simple_writer.cpp        # NEW
│   │   ├── segmenter.cpp            # REDUCED
│   │   ├── segmenter_block.cpp      # NEW
│   │   └── segmenter_analysis.cpp   # NEW
│   ├── metadata/
│   │   ├── legacy/
│   │   │   ├── frozen2_deserializer.cpp  # REDUCED
│   │   │   └── frozen2_string_table.cpp  # NEW
│   │   └── ...
│   └── plugin/
│       ├── codec_registry.cpp       # NEW
│       ├── format_registry.cpp      # NEW
│       └── categorizer_registry.cpp # NEW
├── benchmarks/
│   ├── CMakeLists.txt               # NEW
│   └── block_cache_benchmark.cpp    # NEW
├── cmake/
│   └── dwarfs-config.cmake.in       # NEW
├── vcpkg.json                       # NEW
└── TODO.performance-refactor.md     # This file
```

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Lock optimization breaks caching | Medium | High | Add concurrent read tests before changes |
| API changes break compatibility | Medium | High | Versioned namespaces, deprecation warnings |
| File splitting creates circular deps | Medium | Medium | Dependency analysis, forward declarations |
| Plugin ABI instability | Medium | High | Version interfaces, stability guarantees |
| Performance regression | Low | High | Benchmark suite runs on every PR |
| Memory leaks in object pools | Low | Medium | Valgrind/ASAN tests |

---

## Success Metrics

| Metric | Current | Target | Verification |
|--------|---------|--------|--------------|
| Concurrent read throughput | Baseline | 2x+ | `block_cache_benchmark` |
| Files over 700 lines | 11 | 0 | `wc -l src/**/*.cpp` |
| Test coverage | Unknown | 80%+ | `gcov` report |
| API documentation | None | 100% | Doxygen output |
| `find_package(dwarfs)` | No | Yes | CMake integration test |

---

## Timeline

| Phase | Duration | Start | End | Key Deliverables |
|-------|----------|-------|-----|------------------|
| A: Performance | 1-2 weeks | Week 1 | Week 2 | Lock optimization, benchmarks |
| B: API | 1-2 weeks | Week 2 | Week 4 | simple_writer, streaming |
| C: Quality | 2-3 weeks | Week 4 | Week 7 | File splitting, docs, tests |
| D: Build | 1 week | Week 7 | Week 8 | CMake exports, minimal build |
| E: Extensibility | 2-3 weeks | Week 8 | Week 11 | Plugin architecture |
| **Total** | **7-11 weeks** | | | |

---

## Next Steps

1. [ ] Create benchmark suite for baseline measurements
2. [ ] Implement block cache lock optimization
3. [ ] Implement LRU cache intrusive list
4. [ ] Run benchmarks to verify improvement
5. [ ] Create simple_writer API
6. [ ] Begin file splitting with largest files

---

## Notes

- Phase A and B can partially overlap (different files)
- Phase C should be done incrementally (one file at a time)
- Phase D can be done in parallel with Phase C
- Phase E should wait until API surface is stable
- All changes require tests to pass before merge
