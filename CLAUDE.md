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

## CRITICAL: Build Configuration Isolation

**MANDATORY**: Windows MSVC, Linux, and macOS builds MUST NOT be affected when modifying MinGW/MSYS build configurations.

### Build Preset Isolation

The project uses separate CMake presets for different build environments:

**Windows MSVC builds** (use separate presets, MUST NOT be affected):
- `x64-windows-base`, `x64-windows-static-base`, `x64-windows-static-md-base`
- `arm64-windows-static-base`

**Linux builds** (use separate presets, MUST NOT be affected):
- `x64-linux-base`, `x64-linux-dynamic-base`, `x64-linux-musl-base`

**macOS builds** (use separate presets, MUST NOT be affected):
- `x64-osx-base`, `arm64-osx-base`

**Windows GCC builds** (MinGW/MSYS2 - completely different environment):
- `x64-windows-mingw-base`, `x64-windows-msys-base`
- These are the ONLY presets that should be modified for Windows GCC Matrix fixes

### When Fixing Windows GCC (MinGW/MSYS) Issues

**ONLY modify these files/presets:**
- CMakePresets.json: `x64-windows-mingw-*` and `x64-windows-msys-*` presets ONLY
- `.github/workflows/windows-gcc-matrix.yml` ONLY

**NEVER modify:**
- `common-base` preset (affects ALL builds)
- Windows MSVC presets (`x64-windows-*` except mingw/msys variants)
- Linux presets (`x64-linux-*`)
- macOS presets (`*osx-*`)
- CMakeLists.txt global settings (affects ALL builds)

### Why This Is Critical

MinGW/MSYS2 use GCC compiler on Windows, which is fundamentally different from:
- MSVC builds (use Microsoft Visual C++ compiler)
- Linux builds (use native GCC on Linux)
- macOS builds (use Clang on macOS)

Changes to MinGW/MSYS builds MUST NOT propagate to other environments.

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

## Iterating on Windows CI Errors

### Overview
Windows CI builds often fail due to conflicts between Windows macros and C++ code. The most common issues are:

1. **ERROR macro conflict** - Windows.h defines `ERROR` as a macro (typically 0)
2. **WARN macro conflict** - Windows.h may define `WARN` as a macro in some contexts
3. **min/max macro conflicts** - Windows defines `min` and `max` as macros
4. **C4996 deprecation warnings** - POSIX function names like `::read`, `::write`, `getenv`

### Debugging Workflow

1. **Check CI status:**
   ```bash
   gh run list --limit 5 --json databaseId,status,conclusion,displayTitle,headSha --jq '.[] | select(.displayTitle | contains("Windows Matrix")) | {displayTitle, status, conclusion}'
   ```

2. **Fetch logs automatically:**
   ```bash
   ruby scripts/fetch_windows_ci_logs_octokit.rb
   ```

   This downloads and organizes logs into `tmp/windows_ci_logs/run_<id>_<sha>/` with:
   - One subdirectory per job
   - Logs organized by step/group
   - Index file showing all sections

3. **Check CMake build errors:**
   ```bash
   grep -E "error C" tmp/windows_ci_logs/run_*/__x64___Static___windows-latest___x64-windows-static___production/group_39_Build_with_CMake.log
   ```

4. **Fix errors ONE BY ONE** - NO SED!

### Common Windows Macro Conflicts

#### ERROR Macro
**Problem:** Windows.h defines `ERROR` as a macro (value 0), conflicting with enum value.

**Error pattern:**
```
error C2079: 'log_level_map' uses undefined class 'dwarfs::logger::level_type'
error C2059: syntax error: 'constant'
```

**Solution:** Add `#undef ERROR` before enum definition or usage:
```cpp
// In header files (before enum):
#ifdef _WIN32
#undef ERROR
#undef WARN
#endif

// In source files (after includes but before usage):
#ifdef _WIN32
#undef ERROR
#endif
```

**Files affected:**
- `include/dwarfs/logger.h` - has enum with ERROR value
- `src/logger.cpp` - uses `logger::ERROR` and bare `ERROR` in switch

#### NOMINMAX
**Problem:** Windows defines `min`/`max` as macros, breaking template syntax.

**Error pattern:**
```
error C3878: syntax error: unexpected token ')' following 'simple-type-specifier'
```

**Solution:** `NOMINMAX` is defined globally in `cmake/compile.cmake` for all MSVC builds:
```cmake
add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601 NOMINMAX)
```

#### C4996 Deprecation Warnings
**Problem:** POSIX function names are deprecated on MSVC (`::read`, `::write`, `getenv`).

**Error pattern:**
```
warning C4996: 'read': The POSIX name for this item is deprecated
error C2220: the following warning is treated as an error
```

**Solution:** Use pragma warning suppression in source:
```cpp
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
    ssize_t n = ::read(fd, ptr, remaining);
#pragma warning(pop)
#else
    ssize_t n = ::read(fd, ptr, remaining);
#endif
```

**Location:** `include/dwarfs/internal/folly_compat.h` - readFull() and writeFull() functions

### Scripts for Windows CI Debugging

1. **fetch_windows_ci_logs_octokit.rb** - Main log fetcher using Octokit gem
   - Downloads latest failed Windows Matrix run logs
   - Organizes by job and step/group
   - Saves to `tmp/windows_ci_logs/run_<id>_<sha>/`

2. **check_windows_ci.rb** - Quick CI status checker
   - Checks latest Windows Matrix run status
   - Automatically fetches logs if failed

3. **fetch_windows_ci_logs_grouped.rb** - Alternative grouped log fetcher

### Example Debugging Session

```bash
# 1. Check CI status
ruby scripts/check_windows_ci.rb

# 2. Fetch logs (automatically done by check_windows_ci.rb)
ruby scripts/fetch_windows_ci_logs_octokit.rb

# 3. Examine specific error
grep -E "error C" tmp/windows_ci_logs/run_*/__x64___Static__*/group_39_Build_with_CMake.log | head -30

# 4. Fix the issue (edit file, commit, push)

# 5. Repeat until all jobs pass
```

### Windows Build Jobs

The Windows Matrix tests these configurations:
- `x64-windows-static` (MSVC static) - Main target
- `x64-windows-dynamic` (MSVC dynamic)
- `x64-windows-msys` (MSYS2 with GCC)
- `x64-windows-mingw` (MinGW with GCC)
- `arm64-windows-static` (ARM64 MSVC static)

All MSVC builds should have the same errors if macro-related. MSYS/MinGW use GCC and may have different issues.
