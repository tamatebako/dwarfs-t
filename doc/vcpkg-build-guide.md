# Building DwarFS with vcpkg

## Overview

vcpkg provides a way to build fully static DwarFS binaries with all dependencies included. This is especially useful for:

- Creating portable binaries
- Static linking for distribution
- Reproducible builds across platforms
- Avoiding system dependency conflicts

**Important**: First-time vcpkg builds are **very slow** (30-60 minutes) as all dependencies are built from source. Subsequent builds are much faster due to caching.

## Prerequisites

- Git
- CMake ≥3.28
- Ninja or Make
- C++ compiler (GCC ≥10, Clang ≥12, MSVC ≥19.29)
- 4-8 GB disk space for vcpkg cache
- 30-60 minutes for first build

## Quick Start

### 1. Install vcpkg

```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg

# Bootstrap vcpkg
~/vcpkg/bootstrap-vcpkg.sh  # Linux/macOS
~/vcpkg/bootstrap-vcpkg.bat # Windows

# Set environment variable
export VCPKG_ROOT="$HOME/vcpkg"
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc  # Make permanent
```

### 2. Build DwarFS

```bash
cd /path/to/dwarfs

# Build all configurations (FlatBuffers-only, both-formats, thrift-only)
./scripts/build-all-and-test.sh --vcpkg

# Or build single configuration manually
export VCPKG_ROOT="$HOME/vcpkg"
cmake -B build-static -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx-static \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -DWITH_TESTS=ON

cmake --build build-static --target mkdwarfs dwarfsck dwarfsextract -j8
```

## Platform-Specific Triplets

vcpkg uses "triplets" to specify target platform and linkage:

### Standard Triplets (20 Total)

DwarFS v0.17.0 supports all 20 standard vcpkg triplets:

#### Windows (8 triplets)
| Triplet | Architecture | Linkage | Use Case |
|---------|--------------|---------|----------|
| `x64-windows` | x64 | Dynamic (DLLs) | Default Windows build |
| `x64-windows-static` | x64 | Static | Standalone executable |
| `x64-mingw-dynamic` | x64 | Dynamic (DLLs) | MinGW toolchain |
| `x64-mingw-static` | x64 | Static | MinGW standalone |
| `arm64-windows` | ARM64 | Dynamic (DLLs) | Windows on ARM |
| `arm64-windows-static` | ARM64 | Static | ARM standalone |
| `arm64-mingw-dynamic` | ARM64 | Dynamic (DLLs) | ARM MinGW |
| `arm64-mingw-static` | ARM64 | Static | ARM MinGW standalone |

#### macOS (4 triplets)
| Triplet | Architecture | Linkage | Use Case |
|---------|--------------|---------|----------|
| `arm64-osx` | Apple Silicon | Static | **Default** macOS M1/M2/M3 |
| `arm64-osx-dynamic` | Apple Silicon | Dynamic | Shared libraries |
| `x64-osx` | Intel | Static | Intel Mac default |
| `x64-osx-dynamic` | Intel | Dynamic | Intel shared libraries |

#### Linux (4 triplets)
| Triplet | Architecture | Linkage | Use Case |
|---------|--------------|---------|----------|
| `x64-linux` | x64 | Static | **Default** Linux x64 |
| `x64-linux-dynamic` | x64 | Dynamic | Shared libraries |
| `arm64-linux` | ARM64 | Static | Default ARM64 |
| `arm64-linux-dynamic` | ARM64 | Dynamic | ARM64 shared libraries |

### Triplet Selection Guide

| Scenario | Recommended Triplet | Reason |
|----------|---------------------|--------|
| **Production deployment** | `*-static` | No runtime dependencies |
| **Library distribution** | `*-dynamic` | Users link against shared libs |
| **macOS M1/M2/M3** | `arm64-osx` | Native Apple Silicon |
| **macOS Intel** | `x64-osx` | Intel Mac |
| **Windows standalone** | `x64-windows-static` | Self-contained executable |
| **Linux containers** | `x64-linux` | Static by default |
| **Embedded systems** | `arm64-linux` | ARM Linux static |

The build script auto-detects your platform, or you can specify manually:

```bash
export VCPKG_DEFAULT_TRIPLET=arm64-osx
./scripts/build-all-and-test.sh --vcpkg

# Or specify in CMake directly:
cmake -B build -DVCPKG_TARGET_TRIPLET=x64-linux ...
```

## Build Time Expectations

**First Build** (cold cache):
- Dependencies download: 5-10 minutes
- Boost compilation: 15-25 minutes
- Other dependencies: 5-10 minutes
- DwarFS compilation: 5-10 minutes
- **Total**: 30-60 minutes

**Subsequent Builds** (warm cache):
- DwarFS compilation only: 5-10 minutes

## Disk Space Requirements

- vcpkg installation: ~500 MB
- vcpkg cache (first build): 2-4 GB
- Build artifacts: 1-2 GB per configuration
- **Total recommended**: 4-8 GB free space

## Verifying Static Linking

### macOS
```bash
otool -L build-static/mkdwarfs | grep -v "/usr/lib/system"
# Should show only system libraries or none
```

### Linux
```bash
ldd build-static/mkdwarfs
# Should show only libc, libm, libdl, libpthread, linux-vdso, ld-linux
```

### Windows
```powershell
dumpbin /DEPENDENTS build-static\mkdwarfs.exe
# Should show only kernel32.dll, user32.dll, etc.
```

## Troubleshooting

### Build hangs or is very slow

**Cause**: vcpkg builds all dependencies from source on first run.

**Solution**: Be patient. First build takes 30-60 minutes. Grab coffee ☕

### Out of disk space

**Cause**: vcpkg cache can grow large (2-4 GB).

**Solution**:
```bash
# Clean vcpkg build trees (safe, won't affect installed packages)
$VCPKG_ROOT/vcpkg remove --outdated

# Clean downloads (will require re-download)
rm -rf $VCPKG_ROOT/downloads/*
```

### CMake can't find vcpkg

**Cause**: `VCPKG_ROOT` not set.

**Solution**:
```bash
export VCPKG_ROOT="$HOME/vcpkg"
# Or specify directly:
cmake -DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" ...
```

### Package conflicts or version errors

**Cause**: vcpkg baseline mismatch.

**Solution**: Our `vcpkg.json` and `vcpkg-configuration.json` are configured to use a specific baseline. If you see version conflicts, ensure you're using the latest DwarFS code.

### BZip2 not found (Fixed in v0.17.0)

**Problem** (pre-v0.17.0):
```
CMake Error: Could not find BZip2
-- boost-iostreams requires BZip2::BZip2 target
```

**Root Cause**:
- boost-iostreams requires BZip2::BZip2 target during configuration
- CMake was finding Boost before BZip2 target existed
- Dependency ordering issue in CMake configuration

**Solution** (v0.17.0+):
This is **automatically fixed** in v0.17.0. The fix ensures BZip2 is found **before** Boost in the CMake configuration sequence.

**Location**: [`cmake/vcpkg/bzip2.cmake`](../cmake/vcpkg/bzip2.cmake)

**Impact**:
- ✅ Works on all 20 vcpkg triplets
- ✅ No custom triplet files needed
- ✅ Platform-agnostic solution

**If you still see this error**:
1. Ensure you're using DwarFS v0.17.0 or later
2. Clean your build directory: `rm -rf build-*`
3. Reconfigure from scratch

### Modern Thrift compiler not found (v0.17.0+)

**Problem**:
```
ninja: error: 'bin/thrift1', needed by 'thrift/dwarfs/gen-cpp2/compression_clients.h', missing
```

**Root Cause**:
- Modern Thrift builds require the `thrift1` compiler
- vcpkg installs it to `tools/fbthrift/thrift1`
- CMake wasn't finding the compiler correctly

**Solution** (v0.17.0+):
This is **automatically fixed** in v0.17.0. The fix searches multiple locations:
1. vcpkg tools directory (highest priority)
2. Homebrew installation paths (macOS)
3. System PATH (fallback)

**Location**:
- Compiler detection: [`cmake/thrift.cmake`](../cmake/thrift.cmake)
- Code generation: [`cmake/thrift_library.cmake`](../cmake/thrift_library.cmake)

**Manual verification**:
```bash
# Check if thrift1 compiler is found
cmake -B build -DDWARFS_WITH_THRIFT=ON ... 2>&1 | grep "Found thrift1"
# Should output: -- Found thrift1 compiler: /path/to/thrift1
```

**If you still see this error**:
1. Ensure you're using DwarFS v0.17.0 or later
2. Verify fbthrift is installed via vcpkg: `vcpkg list | grep fbthrift`
3. Check compiler exists: `find $VCPKG_ROOT -name thrift1`
4. Clean and reconfigure: `rm -rf build-* && cmake ...`

## Advanced Usage

### Building Only Specific Tools

```bash
# Build only mkdwarfs
cmake --build build-static --target mkdwarfs

# Build only dwarfsck
cmake --build build-static --target dwarfsck
```

### Custom Triplet

Create a custom triplet in `vcpkg_triplets/`:

```cmake
# vcpkg_triplets/custom-triplet.cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
```

Then use it:
```bash
export VCPKG_DEFAULT_TRIPLET=custom-triplet
./scripts/build-all-and-test.sh --vcpkg
```

### Parallel Jobs

Control parallelism to reduce memory usage:

```bash
# Limit to 4 parallel jobs (default: 8)
export JOBS=4
./scripts/build-all-and-test.sh --vcpkg
```

## CI/CD Integration

For CI/CD pipelines, consider:

1. **Cache vcpkg** between builds:
   ```yaml
   # GitHub Actions example
   - uses: actions/cache@v3
     with:
       path: ~/vcpkg
       key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}
   ```

2. **Use binary caching** (advanced):
   ```bash
   export VCPKG_BINARY_SOURCES="clear;files,/path/to/cache,readwrite"
   ```

3. **Reduce parallel jobs** to avoid OOM:
   ```bash
   export JOBS=2
   ```

## Comparison with System Dependencies

| Aspect | vcpkg Static | System Dynamic |
|--------|-------------|----------------|
| **Binary Size** | Larger (10-25 MB) | Smaller (2-5 MB) |
| **Portability** | Excellent | Platform-specific |
| **Build Time** | Slow (first time) | Fast |
| **Dependencies** | Self-contained | System packages required |
| **Updates** | Controlled | System-managed |

## Files Created

This vcpkg integration adds:

- `vcpkg.json` - Dependency manifest
- `vcpkg-configuration.json` - Registry configuration
- `vcpkg_triplets/*.cmake` - Platform triplets (6 files)
- `vcpkg_ports/*/` - Custom overlay ports (jemalloc, folly, fbthrift)
- Updated `scripts/build-all-and-test.sh` - Adds `--vcpkg` flag

## Next Steps

After successful build:

1. Test the binaries:
   ```bash
   ./build-static/mkdwarfs --version
   ./build-static/dwarfsck --version
   ```

2. Run tests:
   ```bash
   cd build-static
   ctest --output-on-failure
   ```

3. Install (optional):
   ```bash
   sudo cmake --install build-static
   ```

## Support

For issues:
- Check [troubleshooting](#troubleshooting) section above
- Review vcpkg log: `build-*/vcpkg-manifest-install.log`
- Open issue on GitHub: https://github.com/tamatebako/dwarfs/issues