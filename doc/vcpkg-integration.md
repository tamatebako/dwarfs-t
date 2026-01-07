# DwarFS vcpkg Integration Guide

**Version**: v0.16.0+
**Last Updated**: 2025-12-27

---

## Overview

This guide explains how to use DwarFS libraries via vcpkg, Microsoft's C++ package manager. vcpkg provides:

- **Fully static builds** with all dependencies included
- **Cross-platform consistency** across Linux, macOS, Windows
- **Easy integration** into CMake projects
- **Reproducible builds** with version pinning

### Why Use vcpkg?

**For Application Developers**:
- Embed DwarFS filesystem support in your C++ application
- No manual dependency management
- Static linking for portable binaries

**For Tool Builders**:
- Build only the tools you need using pre-built libraries
- Faster iteration (libraries pre-compiled)
- Consistent across development machines

---

## Prerequisites

### Install vcpkg

```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg

# Bootstrap (platform-specific)
./bootstrap-vcpkg.sh  # Linux/macOS
./bootstrap-vcpkg.bat # Windows

# Set environment variable
export VCPKG_ROOT="$HOME/vcpkg"  # Add to ~/.bashrc or ~/.zshrc
```

### System Requirements

**All Platforms**:
- CMake ≥ 3.24
- C++20 compiler (GCC ≥10, Clang ≥12, MSVC ≥19.29)

**macOS**:
- Xcode Command Line Tools
- pkg-config (via Homebrew): `brew install pkg-config`

**Linux**:
- Build essentials: `sudo apt install build-essential ninja-build pkg-config`

**Windows**:
- Visual Studio 2019 or later
- Git for Windows

---

## Library Structure

DwarFS provides **7 libraries** via vcpkg:

### Core Libraries

1. **dwarfs_common** - Foundation layer
   - Compression algorithms (zstd, lzma, lz4, brotli, FLAC, Rice++)
   - I/O abstractions (memory-mapped, read-based)
   - Utilities (checksums, logging, performance monitoring)

2. **dwarfs_reader** - Read DwarFS images
   - Filesystem interface for mounting/reading
   - Metadata parsing (FlatBuffers/Thrift)
   - Block cache with prefetching

3. **dwarfs_writer** - Create DwarFS images
   - Scanner (multi-threaded directory traversal)
   - Segmenter (deduplication)
   - Categorizer framework (PCM audio, FITS images)

4. **dwarfs_extractor** - Extract DwarFS images
   - Multi-threaded extraction
   - Pattern-based selective extraction
   - Archive format conversion

5. **dwarfs_rewrite** - Recompress DwarFS images
   - Change compression algorithms
   - Rebuild metadata
   - Requires Thrift support (`DWARFS_HAVE_THRIFT`)

### Metadata Format Libraries

6. **dwarfs_metadata_legacy** - Legacy Thrift (Frozen2)
   - Always available (no external dependencies)
   - Hand-coded implementation
   - Homebrew v0.14.1 compatibility

7. **dwarfs_metadata_modern_thrift** - Modern Thrift Compact (OPTIONAL)
   - Requires Folly, fbthrift, jemalloc
   - CompactProtocol serialization
   - Smallest metadata format
   - Production-ready in v0.17.0+

### Tool Support Library

8. **dwarfs_tool_support** - CLI utilities (NEW in v0.16.0)
   - Command-line option parsing
   - Progress reporting
   - Safe main() wrappers
   - mkdwarfs/dwarfs/dwarfsck/dwarfsextract handlers

---

## Installing DwarFS Libraries

### Basic Installation

```bash
# Install all libraries (FlatBuffers-only, recommended)
vcpkg install dwarfs

# Check installed files
vcpkg list dwarfs
```

### With Custom Triplet

```bash
# Static linking (recommended)
vcpkg install dwarfs:x64-linux-static
vcpkg install dwarfs:arm64-osx-static
vcpkg install dwarfs:x64-windows-static

# Dynamic linking
vcpkg install dwarfs:x64-linux-dynamic
```

### Installation Output

After installation, you'll have:

```
vcpkg_installed/<triplet>/
├── lib/
│   ├── libdwarfs_common.a
│   ├── libdwarfs_reader.a
│   ├── libdwarfs_writer.a
│   ├── libdwarfs_extractor.a
│   ├── libdwarfs_rewrite.a (if Thrift enabled)
│   └── libdwarfs_tool_support.a
├── include/
│   └── dwarfs/
│       ├── *.h (public headers)
│       ├── reader/
│       ├── writer/
│       ├── utility/
│       ├── metadata/
│       └── tool/
└── share/dwarfs/
    ├── dwarfs-config.cmake
    └── dwarfs-targets.cmake
```

---

## Building Tools Separately

### Step 1: Install Libraries

```bash
cd dwarfs
vcpkg install dwarfs --overlay-ports=./vcpkg_ports
```

### Step 2: Build Tools

```bash
cd tools
cmake -B build \
  -DCMAKE_PREFIX_PATH=$VCPKG_ROOT/installed/arm64-osx-static \
  -DPKG_CONFIG_PATH=$VCPKG_ROOT/installed/arm64-osx-static/lib/pkgconfig

cmake --build build -j8
```

### Step 3: Use Tools

```bash
./build/mkdwarfs -i /usr/share/dict -o test.dff -l1
./build/dwarfsck test.dff
./build/dwarfsextract -i test.dff -o extracted/
```

### Available Tools

After building, you'll have:

- `mkdwarfs` - Create DwarFS filesystem images
- `dwarfsck` - Check/inspect filesystem images
- `dwarfsextract` - Extract filesystem images

**Note**: `dwarfs` (FUSE driver) requires additional FUSE libraries and is built separately.

---

## Building with Modern Thrift Format (v0.17.0+)

### Overview

Modern Thrift provides the smallest metadata footprint using Apache Thrift's CompactProtocol. Introduced in v0.17.0, it requires the full Facebook stack (Folly, fbthrift, jemalloc) via vcpkg overlay ports.

### When to Use Modern Thrift

**Use Modern Thrift when**:
- Absolute minimum size is critical (0.07-1.41% smaller than FlatBuffers)
- You have Folly + fbthrift dependencies available
- Building on platforms with good vcpkg support (Ubuntu, macOS, Windows x64)

**Use FlatBuffers when**:
- Portability is priority (header-only, no complex deps)
- Build simplicity matters
- Size overhead of <1.5% is acceptable

### Prerequisites

**vcpkg Overlay Ports**: Modern Thrift requires custom vcpkg ports for Folly, fbthrift, and jemalloc.

```bash
# Ensure you have DwarFS vcpkg overlay ports
ls vcpkg_ports/
# Should see: folly/ fbthrift/ wangle/ jemalloc/
```

### Build Command

```bash
cmake -B build \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=/path/to/dwarfs/vcpkg_ports \
  -DWITH_TESTS=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON

ninja -C build
```

### Dependencies (via vcpkg overlay ports)

Modern Thrift requires:
- **folly** (v2025.12.29.00) - Facebook core library
- **fbthrift** (v2025.12.29.00) - Apache Thrift compiler and runtime
- **jemalloc** (custom port) - Tebako's jemalloc fork
- Plus standard dependencies: glog, gflags, libevent, double-conversion, etc.

**Build Time**: First build takes 45-60 minutes (full Facebook stack compilation).

### Creating Modern Thrift Images

```bash
# Create using Modern Thrift format
./build/mkdwarfs -i /source -o image.dtc --metadata-format=MODERN_THRIFT

# Verify format
./build/dwarfsck image.dtc
# Should show: "metadata format: modern_thrift (CompactProtocol)"

# Check magic bytes
hexdump -C image.dtc | head -1
# Should show: 00000000  82 21 ...  (magic bytes)
```

### Format Specification

- **Magic Bytes**: `{0x82, 0x21}` (CompactProtocol indicator)
- **Priority**: 100 (between FlatBuffers 120 and Legacy 50)
- **File Extension**: `.dtc` (DwarFS Thrift Compact - recommended)
- **Wire Format**: `[2-byte magic][CompactProtocol data]`
- **Schema**: `thrift/metadata_modern.thrift`

### Troubleshooting Modern Thrift Builds

**Issue: "folly not found"**
```bash
# Solution: Ensure vcpkg overlay ports are configured
cmake ... -DVCPKG_OVERLAY_PORTS=/path/to/dwarfs/vcpkg_ports
```

**Issue: "FBThrift::thriftcpp2 target not found"**
```bash
# Solution: Rebuild vcpkg cache
rm -rf build/vcpkg_installed/
cmake -B build ...
```

**Issue: Link errors with jemalloc symbols**
```bash
# Solution: Ensure jemalloc from overlay port is used
vcpkg list jemalloc
# Should show: jemalloc:arm64-osx (from overlay port)
```

---

## Using Libraries in Your Project

### Example: Minimal CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.24)
project(my_dwarfs_app)

set(CMAKE_CXX_STANDARD 20)

# Find DwarFS
find_package(dwarfs REQUIRED CONFIG)

# Your application
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE
  dwarfs::dwarfs_reader
  dwarfs::dwarfs_extractor
)
```

### Example: Load and Extract

```cpp
#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/reader/filesystem_loader.h>

int main() {
  auto lgr = dwarfs::stream_logger::create(std::cerr);
  auto os = dwarfs::os_access::create();

  // Load filesystem
  dwarfs::reader::filesystem_load_config config;
  config.image_path = "myfs.dff";
  config.cache_size = 512 << 20;  // 512 MiB
  config.num_workers = 4;

  auto fs = dwarfs::reader::filesystem_loader::load(*lgr, *os, config);

  // Read file
  auto entry = fs.find("/path/to/file.txt");
  auto inode = fs.open(entry->inode());
  auto content = fs.read_string(inode);

  return 0;
}
```

### Building Your Project

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

cmake --build build
```

---

## Dependencies Explained

### Required by All Builds

| Library | Purpose | Provided By |
|---------|---------|-------------|
| **Boost** | program_options, filesystem | vcpkg |
| **OpenSSL** | Checksums (libcrypto) | vcpkg |
| **libarchive** | Archive extraction | vcpkg |
| **xxHash** | Fast integrity checks | vcpkg |
| **zstd** | Primary compression | vcpkg |

### Compression Libraries (MANDATORY in vcpkg)

vcpkg builds include all compression algorithms:

| Library | Algorithm | Auto-Enabled |
|---------|-----------|--------------|
| **lz4** | Fast compression | ✅ Yes |
| **liblzma** | LZMA compression | ✅ Yes |
| **Brotli** | Web compression | ✅ Yes |
| **FLAC** | Audio compression | ✅ Yes |

### Memory Allocator (MANDATORY)

| Library | Purpose | Platform Notes |
|---------|---------|----------------|
| **jemalloc** | Memory allocator | ✅ All (including ARM64) |

**Why Mandatory**: This is the Tebako fork - we ALWAYS use Tebako's jemalloc fork for performance and Tebako compatibility.

### Header-Only Libraries (Auto-Fetched)

| Library | Purpose | vcpkg Handling |
|---------|---------|----------------|
| **FlatBuffers** | Metadata serialization | Auto-fetched |
| **fmt** | String formatting | vcpkg (if available) or FetchContent |
| **range-v3** | Range utilities | vcpkg (if available) or FetchContent |
| **parallel-hashmap** | Efficient hash maps | vcpkg (if available) or FetchContent |

---

## Troubleshooting

### Issue: "Could not find dwarfs"

**Symptom**:
```
CMake Error: find_package could not find dwarfs
```

**Solution**:
```bash
# Verify installation
vcpkg list dwarfs

# Set CMAKE_PREFIX_PATH
cmake -B build -DCMAKE_PREFIX_PATH=$VCPKG_ROOT/installed/<triplet>

# Or use toolchain file
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

### Issue: "jemalloc::jemalloc_static not found"

**Symptom**:
```
The link interface contains jemalloc::jemalloc_static but the target was not found
```

**Solution**:
```bash
# Ensure pkg-config can find jemalloc
export PKG_CONFIG_PATH=$VCPKG_ROOT/installed/<triplet>/lib/pkgconfig

# Add to CMakeLists.txt:
find_package(PkgConfig REQUIRED)
pkg_check_modules(jemalloc REQUIRED IMPORTED_TARGET jemalloc>=5.2.1)
add_library(jemalloc::jemalloc_static ALIAS PkgConfig::jemalloc)
```

### Issue: "parallel_hashmap/phmap_config.h not found"

**Symptom**:
```
fatal error: 'parallel_hashmap/phmap_config.h' file not found
```

**Solution**: Fixed in Session 45 - ensure you have latest `cmake/vcpkg/phmap.cmake`:
```cmake
find_path(PARALLEL_HASHMAP_INCLUDE_DIR
  NAMES parallel_hashmap/phmap_config.h  # Correct header
  REQUIRED
)
```

### Issue: "Manpage functions undefined"

**Symptom**:
```
Undefined symbols: get_mkdwarfs_manpage()
```

**Solution**: Add `tools/src/manpage_stubs.cpp` to your tool executable:
```cmake
add_executable(mkdwarfs
  src/mkdwarfs_main.cpp
  src/mkdwarfs.cpp
  src/manpage_stubs.cpp  # Provides stub implementations
)
```

### Issue: Tools built but crash with "Illegal instruction: 4"

**Symptom**:
```
Illegal instruction: 4
```

**Cause**: Architecture/optimization mismatch in vcpkg-installed libraries

**Solution**: Use full project build instead:
```bash
cmake -B build -DWITH_LIBDWARFS=ON -DWITH_TOOLS=ON -DDWARFS_WITH_THRIFT=OFF
cmake --build build
```

---

## Complete Example: Static Site Server

See [`example/static-site-server/`](../example/static-site-server/) for a complete working example.

**Key Files**:
- `CMakeLists.txt` - CMake configuration using `find_package(dwarfs)`
- `vcpkg.json` - Manifest file for dependencies
- `build.sh` - Automated build script
- `dwarfs_loader.{h,cpp}` - DwarFS integration
- `http_server.cpp` - HTTP server implementation
- `main.cpp` - Application entry point

**Build**:
```bash
cd example/static-site-server
./build.sh
```

**Run**:
```bash
./build/static-site-server --image candide.dff --port 8080
```

---

## Advanced Topics

### Custom vcpkg Overlay

If building from DwarFS source with local modifications:

```bash
vcpkg install dwarfs --overlay-ports=./vcpkg_ports
```

This uses the port definition in `vcpkg_ports/dwarfs/` instead of the vcpkg registry.

### Selective Library Installation

Install only what you need:

```bash
# Reader libraries only
vcpkg install dwarfs[core]

# With FUSE driver (Linux only)
vcpkg install dwarfs[fuse]

# Custom combination
vcpkg install dwarfs[flac,lz4,lzma]
```

### Static vs Dynamic Linking

```bash
# Static (recommended for vcpkg)
vcpkg install dwarfs:arm64-osx-static

# Dynamic
vcpkg install dwarfs:arm64-osx-dynamic
```

vcpkg defaults to static linking which is ideal for portable binaries.

---

## Platform-Specific Notes

### macOS

**Triplets**:
- `arm64-osx-static` - Apple Silicon (M1/M2/M3)
- `x64-osx-static` - Intel Macs

**Dependencies**:
```bash
# Install pkg-config (required)
brew install pkg-config
```

**Common Issue**: Ensure `PKG_CONFIG_PATH` includes vcpkg's pkgconfig directory.

### Linux

**Triplets**:
- `x64-linux-static` - x86_64
- `arm64-linux-static` - ARM64

**FUSE Support**:
- vcpkg doesn't include FUSE libraries
- Install system FUSE: `sudo apt install libfuse3-dev`

### Windows

**Triplets**:
- `x64-windows-static` - x86_64
- `arm64-windows-static` - ARM64

**WinFsp**:
- Required for FUSE driver
- Install separately: https://winfsp.dev/

---

## Summary

**Recommended Workflow**:
1. Install vcpkg once per development machine
2. Add DwarFS overlay port: `vcpkg install dwarfs --overlay-ports=./vcpkg_ports`
3. Use `find_package(dwarfs CONFIG)` in your CMakeLists.txt
4. Link against `dwarfs::dwarfs_*` targets
5. Build with vcpkg toolchain file

**For Questions**:
- vcpkg: https://github.com/microsoft/vcpkg
- DwarFS: https://github.com/tamatebako/dwarfs
- Example: `example/static-site-server/`

---

**See Also**:
- [LIBDWARFS_INTEGRATION_GUIDE.md](LIBDWARFS_INTEGRATION_GUIDE.md) - Using DwarFS reader API
- [README.md](../README.md) - Main documentation
- [example/static-site-server/](../example/static-site-server/) - Complete working example