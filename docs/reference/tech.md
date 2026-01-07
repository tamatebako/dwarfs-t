# DwarFS Technical Stack

## Core Technologies

### Programming Language
- **C++20** (strictly enforced via `CMAKE_CXX_STANDARD`)
- Modern features: concepts, ranges, modules-ready
- Compiler requirements: GCC ≥10, Clang ≥12, MSVC ≥19.29

### Build System
- **CMake** ≥3.28.0 (≥3.24.0 for Tebako builds)
- **Ninja** (preferred) or Make
- Modular CMake structure in [`cmake/`](../cmake/)

### Serialization Technologies

**Metadata Serialization** (2 formats supported):
1. **FlatBuffers** (modern default):
   - Dependencies: FlatBuffers (header-only, FetchContent)
   - Format: Memory-mappable, zero-copy, self-describing
   - Defined in [`flatbuffers/metadata.fbs`](../flatbuffers/metadata.fbs)
   - Needs: 1 section (metadata)
   - Excellent portability, works on all platforms

2. **Apache Thrift Compact** (legacy):
   - Dependencies: Folly, fbthrift
   - Format: Frozen2 bit-packed structures
   - Defined in [`thrift/metadata.thrift`](../thrift/metadata.thrift)
   - Needs: 2 sections (schema + metadata)
   - Optional, for backward compatibility only

**History Serialization** (2 formats supported):
1. **FlatBuffers** (modern default):
   - Dependencies: FlatBuffers (header-only, FetchContent)
   - Format: Memory-mappable, zero-copy
   - Defined in [`flatbuffers/history.fbs`](../flatbuffers/history.fbs)
   - Excellent portability, works on all platforms

2. **Apache Thrift Compact** (legacy):
   - Dependencies: Folly, fbthrift
   - Format: Compact binary
   - Defined in [`thrift/history.thrift`](../thrift/history.thrift)
   - Optional, for backward compatibility only

### Compression Algorithms

**Always Available**:
- **zstd** (primary): Fast, excellent ratio
- **xxHash**: Ultra-fast checksums (XXH3-64, XXH3-128)

**Optional** (via pkg-config):
- **lz4/lz4hc**: Fastest compression
- **lzma/xz**: Best compression ratio
- **Brotli**: Balance between speed and ratio
- **FLAC**: Lossless audio (via categorizer)
- **Rice++**: Astronomical images (custom)

**Checksums**:
- **XXH3-64**: Fast integrity checks
- **SHA-512/256**: Thorough validation
- Via OpenSSL/LibreSSL libcrypto

### FUSE Implementations

**Linux**:
- **FUSE3** (preferred): Via libfuse3
- **FUSE2** (legacy): Via libfuse (if enabled with `WITH_LEGACY_FUSE`)

**macOS**:
- **FUSE-T** (preferred): Userspace, no kernel extension
- **macFUSE**: Requires kernel extension + security adjustment

**Windows**:
- **WinFsp**: Windows FUSE driver
- Requires `winfsp-x64.dll`

**FreeBSD**:
- FUSE2/FUSE3 via Linux compatibility layer

## Development Environment Setup

### Quick Start (Ubuntu 22.04/24.04)

```bash
# Install dependencies
sudo apt-get install -y ninja-build cmake \
  libboost-all-dev libssl-dev libevent-dev \
  libdouble-conversion-dev libfmt-dev lz4-dev \
  libzstd-dev libxxhash-dev libbz2-dev \
  libarchive-dev libgtest-dev liblzma-dev

# Clone with submodules
git clone --recurse-submodules https://github.com/tamatebako/dwarfs
cd dwarfs

# Build
mkdir build && cd build
cmake .. -GNinja -DWITH_TESTS=ON
ninja

# Test
ctest -j
```

### Platform-Specific Setup

**macOS** (via Homebrew):
```bash
brew install ninja boost openssl libevent \
  double-conversion fmt lz4 xz zstd xxhash \
  bzip2 libarchive googletest

# For FUSE-T:
brew tap macos-fuse-t/homebrew-cask
brew install fuse-t
```

**Windows** (via vcpkg):
```powershell
# Install WinFsp first
choco install winfsp -y

# Install dependencies
vcpkg install --triplet=x64-windows-static `
  boost-system boost-filesystem boost-program-options `
  boost-iostreams boost-context boost-fiber `
  zlib lz4 zstd xxhash bzip2 liblzma `
  libarchive libevent double-conversion jemalloc gtest

# Build
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static `
  -DWITH_TESTS=ON
cmake --build build --config Release --parallel
```

**FreeBSD**:
```bash
# Install dependencies
pkg install git cmake ninja boost-all \
  openssl libevent double-conversion fmt \
  lz4 zstd xxhash bzip2 libarchive googletest

# Build (same as Linux)
```

### Docker Development

Pre-built Docker images for CI testing available:
- [Ubuntu 22.04](.docker/Dockerfile.ubuntu-2204)
- [Ubuntu 24.04](.docker/Dockerfile.ubuntu)
- [Fedora Rawhide](.docker/Dockerfile.fedora)
- [Arch Linux](.docker/Dockerfile.arch)
- [Alpine](.docker/Dockerfile.alpine)
- [Debian](.docker/Dockerfile.debian)
- [openSUSE Tumbleweed](.docker/Dockerfile.suse)

## Build Configuration Options

### Core Build Options
```cmake
# Library and tool selection
-DWITH_LIBDWARFS=ON          # Build libraries (default: ON)
-DWITH_TOOLS=ON              # Build mkdwarfs/dwarfsck/dwarfsextract (default: ON)
-DWITH_FUSE_DRIVER=ON        # Build FUSE driver (default: ON)

# Testing
-DWITH_TESTS=ON              # Enable test suite (default: OFF)
-DWITH_BENCHMARKS=ON         # Enable benchmarks (default: OFF)

# Metadata formats
-DDWARFS_WITH_FLATBUFFERS=ON # Enable FlatBuffers support (default: ON, required)
-DDWARFS_WITH_THRIFT=ON      # Enable Thrift support (default: ON, auto-OFF in Tebako)

# Tebako
-DTEBAKO_BUILD=ON            # Enable Tebako build mode
-DTEBAKO_BUILD_SCOPE=MKD     # Or ALL for full tools
```

### Compression Algorithm Options
```cmake
-DTRY_ENABLE_FLAC=ON         # FLAC audio compression (default: ON)
-DENABLE_RICEPP=ON           # Rice++ FITS compression (default: ON)
-DTRY_ENABLE_BROTLI=ON       # Brotli compression (default: ON)
-DTRY_ENABLE_LZ4=ON          # LZ4 compression (default: ON)
-DTRY_ENABLE_LZMA=ON         # LZMA compression (default: ON)
```

### Platform Options
```cmake
# macOS
-DWITH_UNIVERSAL_BINARY=ON   # Create universal binary (default: OFF)
-DUSE_HOMEBREW_LIBARCHIVE=ON # Use Homebrew libarchive (default: ON)

# Linux/Unix
-DWITH_LEGACY_FUSE=ON        # Build FUSE2 even if FUSE3 available (default: OFF)
-DUSE_JEMALLOC=ON            # Use jemalloc allocator (default: ON)
-DUSE_MIMALLOC=OFF           # Use mimalloc allocator (default: OFF)

# Development
-DENABLE_ASAN=ON             # Address sanitizer (default: OFF)
-DENABLE_TSAN=ON             # Thread sanitizer (default: OFF)
-DENABLE_UBSAN=ON            # UB sanitizer (default: OFF)
-DENABLE_COVERAGE=ON         # Code coverage (default: OFF)
-DENABLE_STACKTRACE=ON       # Stack traces on errors (default: OFF)
```

## Technical Constraints

### Serialization Format Constraints

**FlatBuffers**:
- Header-only, fast compilation
- Memory-mappable, zero-copy access
- Slightly larger than Thrift (~5-10%)
- Excellent portability (all platforms)
- Self-describing format
- Forward/backward compatible

**Thrift/Folly Limitations**:
- Cannot build on MSys2/MinGW (type conflicts)
- Complex static linking (ABI stability issues)
- Requires C++17 or later
- Platform-specific: Limited on Windows ARM64
- Build time: Slow (full Folly + fbthrift)
- Memory-mappable, zero-copy access
- Smallest format (bit-packed)

### Memory Constraints

**32-bit Systems**:
- Limited to 2-4 GiB address space
- Memory-mapped files problematic for large images
- Solution: Segmented mapping strategy

**Block Cache**:
- Configurable size (default: depends on RAM)
- LRU eviction when full
- Multi-threaded decompression

### Compiler Constraints

**GCC**:
- Minimum: 10.0
- Issues: String overflow warnings, NRVO warnings (GCC 14+)
- Optimization: Always `-O2` (not `-O3` due to regressions)

**Clang**:
- Minimum: 12.0
- Better: ASAN, UBSAN, coverage support
- Faster builds than GCC

**MSVC**:
- Minimum: 19.29 (Visual Studio 2019 v16.11)
- Issues: `/bigobj` required for tests
- Full C++20 support

## Tool Usage Patterns

### Common Development Workflows

**1. Build for Development**:
```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_TESTS=ON \
  -DENABLE_ASAN=ON \
  -DENABLE_UBSAN=ON
ninja -C build
```

**2. Build for Release**:
```bash
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_TESTS=ON \
  -DWITH_BENCHMARKS=ON
ninja -C build
```

**3. Test Metadata Formats**:
```bash
# Both formats (FlatBuffers + Thrift)
cmake -B build -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
ninja -C build
ctest --test-dir build --tests-regex "metadata.*serial"

# FlatBuffers only (Thrift optional)
cmake -B build -GNinja \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON
ninja -C build
ctest --test-dir build
```

**4. Modular Build** (libraries → tools → FUSE):
```bash
# 1. Build libraries
cmake -B build -GNinja \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=OFF \
  -DWITH_TESTS=ON
ninja -C build && ctest --test-dir build
sudo ninja -C build install

# 2. Build tools
cmake -B build -GNinja \
  -DWITH_LIBDWARFS=OFF \
  -DWITH_TOOLS=ON \
  -DWITH_FUSE_DRIVER=OFF \
  -DWITH_TESTS=ON
ninja -C build && ctest --test-dir build
sudo ninja -C build install

# 3. Build FUSE driver
cmake -B build -GNinja \
  -DWITH_LIBDWARFS=OFF \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=ON \
  -DWITH_TESTS=ON
ninja -C build && ctest --test-dir build
sudo ninja -C build install
```

**5. Tebako Build**:
```bash
# MKD scope (mkdwarfs only)
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DTEBAKO_BUILD=ON \
  -DTEBAKO_BUILD_SCOPE=MKD \
  -DWITH_TESTS=ON
ninja -C build

# ALL scope (all tools)
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DTEBAKO_BUILD=ON \
  -DTEBAKO_BUILD_SCOPE=ALL \
  -DWITH_TESTS=ON \
  -DWITH_FUSE_DRIVER=ON
ninja -C build
```

## Code Quality Tools

### Static Analysis
```bash
# Format check
ninja -C build check-format

# Format all
ninja -C build format

# Clang-tidy
ninja -C build tidy

# Clang-tidy with fixes
ninja -C build tidy-fix
```

### Testing
```bash
# All tests
ctest --test-dir build -j

# Metadata tests only
ctest --test-dir build --tests-regex "metadata"

# With different I/O layers
DWARFS_IOLAYER_OPTS=open_mode=read \
  ctest --test-dir build --output-on-failure

# Expensive tests (compatibility, full workflows)
ctest --test-dir build -R expensive
```

### Coverage
```bash
# Build with coverage
cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_COVERAGE=ON \
  -DWITH_TESTS=ON
ninja -C build
ctest --test-dir build

# Generate report
ninja -C build coverage
# Output in build/coverage/html/
```

## Environment Variables

### Runtime Configuration

**`DWARFS_IOLAYER_OPTS`** (v0.14.0+):
Controls I/O strategy for all tools:
- `open_mode=mmap` - Memory-map entire file (64-bit default)
- `open_mode=mmap_segments` - Map file in chunks (32-bit default)
- `open_mode=read` - Use read() calls (error-safe)
- `max_eager_map_size=SIZE` - Threshold for segment mapping

**`DWARFS_WORKER_GROUP_AFFINITY`**:
Set CPU affinity for worker thread groups:
```bash
export DWARFS_WORKER_GROUP_AFFINITY=blockify=3:compress=6,7
# blockify on CPU 3, compress on CPUs 6,7
```

**`DWARFS_LOCAL_REPO_PATH`**:
Use local git repos instead of fetching:
```bash
export DWARFS_LOCAL_REPO_PATH=/path/to/repos
# Expects: $PATH/fmt, $PATH/googletest, etc.
```

### Build Configuration

**`TEBAKO_BUILD`**: Enable Tebako mode
**`TEBAKO_BUILD_SCOPE`**: Set build scope (MKD/ALL)
**`GITHUB_WORKSPACE`**: Used for coverage path mapping
**`STRIP_TOOL`**: Custom strip tool for static builds
**`DWARFS_ARTIFACTS_DIR`**: Artifact output directory

## Platform-Specific Details

### Linux Specifics
- **Primary Platform**: Most features developed/tested here
- **Static Builds**: Alpine Linux + musl libc
- **ELF Branding**: Static binaries branded as Linux ABI (for FreeBSD)
- **Extended Attributes**: POSIX xattr with namespace mapping
- **FUSE**: Both FUSE2 and FUSE3 supported

### macOS Specifics
- **FUSE Detection**: Automatic (macFUSE or FUSE-T)
- **Full Disk Access**: Required for FUSE-T applications
- **Universal Binaries**: Optional via `-DWITH_UNIVERSAL_BINARY=ON`
- **Homebrew Integration**: Special handling for libarchive
- **Code Signing**: May be required for distribution

### Windows Specifics
- **WinFsp**: Mandatory for FUSE driver
- **vcpkg**: Package manager for dependencies
- **Static Linking**: Preferred (via vcpkg triplets)
- **Path Handling**: UTF-16 ↔ UTF-8 conversion throughout
- **Extended Attributes**: NTFS streams via NtQueryEaFile/NtSetEaFile
- **Symbolic Links**: Requires admin privileges to create

### FreeBSD Specifics
- **FUSE**: Via Linux compatibility layer
- **Static Binaries**: Linux static binaries work with emulation
- **ELF Branding**: Needs Linux ABI marking
- **Native Build**: Full support for native builds

## Dependency Version Matrix

### Required Dependencies (All Builds)

| Library | Minimum | Preferred | Purpose |
|---------|---------|-----------|---------|
| **Boost** | 1.67.0 | Latest | program_options, filesystem, chronos |
| **OpenSSL/LibreSSL** | 3.0.0 | Latest | Checksums (libcrypto) |
| **libarchive** | 3.6.0 | 3.7.0+ | Archive extraction |
| **xxHash** | 0.8.1 | Latest | Fast checksums |
| **zstd** | 1.4.8 | Latest | Primary compression |

### Optional Dependencies

| Library | Minimum | Purpose | Default |
|---------|---------|---------|---------|
| **FlatBuffers** | 23.5.26 | FlatBuffers format support (required) | FetchContent |
| **Folly + fbthrift** | - | Thrift format support (optional) | Submodule |
| **FLAC** | 1.4.2 | Audio compression | ON |
| **lz4** | 1.9.3 | Fast compression | ON |
| **liblzma** | 5.2.5 | LZMA compression | ON |
| **Brotli** | 1.0.9 | Brotli compression | ON |
| **jemalloc** | 5.2.1 | Memory allocator | ON (not Windows ARM64) |
| **mimalloc** | 2.0.0 | Memory allocator | OFF |

### Header-Only Dependencies (Auto-Fetched)

| Library | Minimum | Preferred | Purpose |
|---------|---------|-----------|---------|
| **fmt** | 10.0 | 12.0.0 | String formatting |
| **GoogleTest** | 1.13.0 | 1.17.0 | Testing |
| **range-v3** | 0.12.0 | 0.12.0 | Ranges |
| **parallel-hashmap** | 1.3.8 | 2.0.0 | Hash maps |

## Testing Infrastructure

### Test Categories

**Unit Tests** (`dwarfs_unit_tests`):
- 60+ test files in [`test/`](../test/)
- Algorithm, data structure, utility tests
- Metadata serialization round-trips
- Platform-specific tests (Windows stat, Linux xattr)

**Categorizer Tests** (`dwarfs_categorizer_tests`):
- PCM audio detection
- FITS image detection
- Incompressible data detection

**Compressor Tests** (`dwarfs_compressor_tests`):
- FLAC compression (if available)
- Rice++ compression (if enabled)

**Tool Tests** (`tool_main_test`):
- Command-line parsing
- Tool integration workflows
- Cross-tool compatibility

**Expensive Tests** (`dwarfs_expensive_tests`):
- Full filesystem workflows
- Compatibility with old formats
- Large-scale operations

### CI/CD Matrix

**11 Architectures Tested**:
- x86_64 (amd64)
- aarch64 (arm64)
- i386 (32-bit x86)
- arm (32-bit ARM)
- ppc64 (big-endian)
- ppc64le (little-endian)
- riscv64
- s390x (big-endian)
- loongarch64

**Multiple Distributions**:
- Ubuntu 22.04, 24.04
- Fedora Rawhide
- Debian Testing
- Arch Linux
- Alpine Linux
- openSUSE Tumbleweed

**Build Variants**:
- Release, Debug, RelWithDebInfo, MinSizeRel
- Shared libraries, static binaries
- With/without sanitizers
- With/without coverage
- GCC and Clang

**Metadata Format Matrix** (lines 956-1113 in [`build.yml`](../.github/workflows/build.yml)):
- Ubuntu x86_64: all-formats, minimal
- Ubuntu aarch64: all-formats
- macOS x86_64: all-formats
- macOS aarch64: minimal
- Windows x64: all-formats

**Tebako Matrix** (lines 1171-1343):
- Ubuntu 20.04: gcc-10, clang-12 (amd64, arm64)
- macOS: Xcode 15.0.1, 15.4
- Alpine 3.17: gcc, clang
- Both MKD and ALL scopes

## Performance Considerations

### Build Time
- **From Tarball**: ~10-15 minutes (no Thrift compiler)
- **From Git Repo**: ~20-30 minutes (includes Thrift compiler, manpage generation)
- **With Folly/Thrift**: +10-15 minutes
- **Parallel Build**: Scales well to 16+ cores

### Runtime Performance

**Memory Usage** (mkdwarfs):
- Scanning: ~1-2 GiB for large trees
- Segmentation: Depends on window size
- Compression: `--memory-limit` (default: auto)
- Total: Typically 2-8 GiB for normal use

**Memory Usage** (dwarfs FUSE):
- Metadata: Loaded in memory (varies by format)
- Block cache: Configurable (default: system-dependent)
- Total: Typically 100-500 MiB

**CPU Usage**:
- mkdwarfs: Scales well to available cores
- dwarfs: Scales to cache worker count
- Both: Can saturate CPU on fast storage

## Debugging Workflows

### Common Issues

**1. Link Errors (Missing Symbols)**:
- Check: Thrift/Folly built with same C++ standard
- Check: All required libraries linked
- Solution: Clean build + reconfigure

**2. Runtime Crashes (SIGBUS)**:
- Likely: Memory-mapped file I/O error
- Solution: Use `DWARFS_IOLAYER_OPTS=open_mode=read`

**3. Test Failures (Metadata)**:
- Check: FlatBuffers enabled (required)
- Check: Thrift enabled if testing both formats
- Check: Format detection in registry
- Solution: Verify CMake defines (`DWARFS_HAVE_FLATBUFFERS`, `DWARFS_HAVE_THRIFT`)

**4. Tebako Build Failures**:
- Check: `DWARFS_WITH_THRIFT=OFF` enforced (incompatible with static linking)
- Check: FlatBuffers available (required, always enabled)
- Check: Correct build scope (MKD/ALL)

### Debug Options
```bash
# Verbose logging
./mkdwarfs -i src -o test.dwarfs --log-level=verbose

# Debug logging
./mkdwarfs -i src -o test.dwarfs --log-level=debug

# Performance monitoring
./dwarfs test.dwarfs mnt -o perfmon=fuse,filesystem_v2,inode_reader_v2

# I/O layer diagnostics
DWARFS_IOLAYER_OPTS=log_operations=true \
  ./dwarfs test.dwarfs mnt
```

## Key Filenames & Paths

### Configuration
- [`CMakeLists.txt`](../CMakeLists.txt) - Main build configuration
- [`cmake/metadata_serialization.cmake`](../cmake/metadata_serialization.cmake) - Format configuration
- [`cmake/tebako.cmake`](../cmake/tebako.cmake) - Tebako integration
- [`cmake/version.cmake`](../cmake/version.cmake) - Version detection

### Build Artifacts
- `build/mkdwarfs` - Filesystem creator
- `build/dwarfs` or `build/dwarfs-bin` - FUSE driver
- `build/dwarfsck` - Checker/inspector
- `build/dwarfsextract` - Extractor
- `build/dwarfsuniversal` - Universal binary (optional)

### Test Artifacts
- `build/dwarfs_unit_tests` - Unit test suite
- `build/tool_main_test` - Tool integration tests
- `build/dwarfs_expensive_tests` - Expensive test suite
- `build/Testing/` - CTest results

### Documentation
- [`doc/mkdwarfs.md`](../doc/mkdwarfs.md) - mkdwarfs manual
- [`doc/dwarfs.md`](../doc/dwarfs.md) - dwarfs manual
- [`doc/dwarfsck.md`](../doc/dwarfsck.md) - dwarfsck manual
- [`doc/dwarfsextract.md`](../doc/dwarfsextract.md) - dwarfsextract manual
- [`doc/dwarfs-format.md`](../doc/dwarfs-format.md) - File format specification
- [`doc/dwarfs-env.md`](../doc/dwarfs-env.md) - Environment variables

## Git Workflow

### Branching Strategy
- `main` - Stable releases
- `tebako-*` - Tebako-specific work
- Feature branches for development

### Submodules
- `folly/` - Facebook Folly library (optional, for Thrift)
- `fbthrift/` - Facebook Thrift compiler (optional, for Thrift)
- `fsst/` - Fast Static Symbol Table compression
- `ricepp/` - Rice++ compression for FITS
- `fast_float/` - Fast float parsing

### CI Triggers
- **Push to main**: Full CI suite
- **Pull requests**: Full CI suite
- **Tag `v*`**: Release build + artifacts
- **Manual**: `workflow_dispatch`
- **Skip paths**: Documentation, scripts, benchmarks (see [`build.yml:13-30`](../.github/workflows/build.yml))

## Known Platform Quirks

### Folly Build Issues
- **MSys2/MinGW**: Type conflicts (pid_t, mode_t)
- **Alpine ARM64**: Occasionally flaky clang builds
- **Windows ARM64**: boost-fiber not supported

### Test Flakiness
- **macOS unmount**: Occasional delays in cleanup
- **Terminal tests**: Screen width assumptions
- **Thread timing**: TSAN may catch false positives

### Toolchain Bugs Encountered
- **Clang** (s390x): Misaligned symbols with LTO
- **Binutils** (ppc64): Mold linker compatibility
- **UPX** (various): Architecture support gaps
- **GCC 14**: NRVO warnings (disabled via flag)