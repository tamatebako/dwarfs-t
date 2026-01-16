# DwarFS Development Notes for Claude

## Build System

### IMPORTANT: Use the clean-build script!

**NEVER manually run cmake or ninja to rebuild the DwarFS library!** Always use the provided clean-build script.

#### Location
`/Users/mulgogi/src/external/dwarfs/scripts/clean-build.sh`

#### Usage

```bash
# From the dwarfs source root
BUILD_DIR=build ./scripts/clean-build.sh -y
```

**Options:**
- `-y`: Skip confirmation prompt (automatically confirms the rebuild)
- `BUILD_DIR=build`: Specify the build directory (default is `build-test`)

**Environment Variables:**
- `BUILD_DIR`: The build directory to use (default: `$PROJECT_ROOT/build-test`)
- `BUILD_TYPE`: CMake build type (default: `Release`)
- `WITH_TESTS`: Enable tests (default: `ON`)
- `WITH_TOOLS`: Enable tools (default: `ON`)
- `WITH_LIBDWARFS`: Enable libdwarfs (default: `ON`)
- `WITH_FLATBUFFERS`: Enable FlatBuffers (default: `ON`)
- `WITH_THRIFT`: Enable Modern Thrift (default: `OFF`)

#### What the script does

1. **Validates** the build directory exists
2. **Prompts for confirmation** (unless `-y` flag is used)
3. **Removes** the old build directory
4. **Creates** a fresh build directory
5. **Runs cmake** with proper vcpkg configuration
6. **Outputs** next steps (run ninja, run tests)

#### After running the script

```bash
cd build
ninja
```

#### Related Scripts

- `scripts/clean.sh` - Clean build artifacts
- `scripts/build-all-and-test.sh` - Build and run all tests
- `scripts/test-all-configs.sh` - Test multiple configurations

## CRITICAL: Tebako jemalloc Dependency

**MANDATORY**: This project REQUIRES Tebako's fork of jemalloc, NOT the upstream jemalloc!

### Tebako jemalloc Details

- **Repository**: https://github.com/tamatebako/jemalloc.git
- **Version**: 5.5.0
- **Location**: `vcpkg_ports/jemalloc/` (overlay port)
- **CMake Variable**: `JEMALLOC_REQUIRED_VERSION` must be 5.5.0
- **Git Repo Variable**: `JEMALLOC_GIT_REPO` set to https://github.com/tamatebako/jemalloc.git

### Why Tebako jemalloc?

The upstream jemalloc/jemalloc does NOT work with this project. We MUST use the Tebako fork which has specific patches for:
- Folly compatibility (without je_ prefix)
- Tebako-specific modifications
- Version 5.5.0 with custom build configuration

### vcpkg Overlay Port

The `vcpkg_ports/jemalloc/` directory contains the overlay port that:
1. Downloads from `tamatebako/jemalloc` NOT `jemalloc/jemalloc`
2. Builds with `--with-jemalloc-prefix=` (no je_ prefix)
3. Provides CMake config files (`jemallocConfig.cmake`)
4. Version is 5.5.0, NOT upstream versions

### CI/CD Requirements

When updating CI workflows or build configurations:
- **NEVER** use system jemalloc packages
- **NEVER** use upstream jemalloc from vcpkg
- **ALWAYS** use the overlay port in `vcpkg_ports/jemalloc/`
- **MUST** install autoconf-archive for jemalloc build (autotools dependency)

### Version Verification

Always verify:
```cmake
# CMakeLists.txt MUST have:
set(JEMALLOC_REQUIRED_VERSION 5.5.0)  # NOT 5.3.0 or any other version!

# vcpkg_ports/jemalloc/vcpkg.json MUST have:
"version": "5.5.0"  # NOT 5.3.0!

# vcpkg_ports/jemalloc/portfile.cmake MUST have:
REPO tamatebako/jemalloc  # NOT jemalloc/jemalloc!
REF 5.5.0  # Use tag, not commit hash
```

## Project Structure

### Metadata Serialization

The project supports multiple metadata serialization formats:

1. **Legacy Thrift (Frozen2)**: Hand-coded, no external deps, always available
   - Located in: `src/metadata/legacy/`
   - Files: `frozen2_deserializer.cpp`, `frozen_schema.cpp`

2. **FlatBuffers**: Modern default, uses vcpkg flatbuffers
   - Located in: `src/metadata/serialization/flatbuffers_serializer.cpp`

3. **Modern Thrift (fbthrift)**: Optional, uses fbthrift v2025.12.29.00+
   - Located in: `src/metadata/serialization/legacy_thrift_serializer.cpp`

### Key Files

- `include/dwarfs/metadata/legacy/frozen_schema.h`: DenseMap and schema definitions
- `src/metadata/legacy/frozen2_deserializer.cpp`: Legacy Thrift deserializer
- `src/metadata/legacy/frozen_schema_serializer.cpp`: Legacy Thrift serializer
- `src/reader/internal/metadata_factory.cpp`: Metadata factory that selects format

### DenseMap Data Structure

Located in `include/dwarfs/metadata/legacy/frozen_schema.h`

- **Purpose**: Efficient map for i16 -> T using `std::vector<std::optional<T>>`
- **Key Type**: `int16_t` (1-based indexing: keys 1, 2, 3, ...)
- **API**: Returns `T*` pointer (NOT `std::optional<T>` by value!)
- **Critical**: The `get()` method returns a pointer to the vector element, which is stable until vector reallocation

```cpp
T* get(int16_t key);  // Returns nullptr if not found, pointer to value if found
```

## Example Programs

### static-site-server

Location: `example/static-site-server/`

**Build:**
```bash
cd build-test
cmake -DDWARFS_BUILD_DIR=/Users/mulgogi/src/external/dwarfs/build \
      /Users/mulgogi/src/external/dwarfs/example/static-site-server
cmake --build .
```

**Run:**
```bash
./static-site-server --image <path-to-.dwarfs-file> --port 8080
```

## Testing

### Metadata Tests
```bash
cd build
ninja  # build first
ctest -R metadata  # run metadata tests
```

### Manual Testing with Compat Image
```bash
./static-site-server --image /Users/mulgogi/src/external/dwarfs/test/compat/compat-v0.2.3.dwarfs --port 8080
curl http://localhost:8080/
```

## Debug Output

Some files have `[DEBUG]` output using `std::cerr`. These are for development and should be removed before production use.

- `src/reader/internal/metadata_factory.cpp`: Has debug output for metadata loading
- `src/metadata/legacy/thrift_compact_reader.cpp`: Has extensive debug output for Thrift Compact protocol parsing

## Common Issues

### Linker Errors with Threads
If you see `$<LINK_ONLY:Threads::Threads>` errors, rebuild using the clean-build script.

### Vector Corruption/Segfaults
If DenseMap::get() returns `std::optional<T>` by value instead of `T*`, the pointer will become invalid when the optional is destroyed. Always use the pointer API.

### 1-based vs 0-based Indexing
DenseMap uses 1-based keys (1, 2, 3, ...), NOT 0-based (0, 1, 2, ...). When accessing field N, use `fields.get(N)`, not `fields.get(N-1)`.
