# DwarFS vcpkg Triplets

This directory contains custom vcpkg triplets for building DwarFS across multiple platforms.

## Supported Triplets

### macOS (OSX)
- **arm64-osx**: Apple Silicon (M1/M2/M3) static build (recommended)
- **x64-osx**: Intel Mac static build (recommended)
- **arm64-osx-dynamic**: Apple Silicon dynamic build
- **x64-osx-dynamic**: Intel Mac dynamic build

### Linux
- **arm64-linux**: ARM64 Linux static build (recommended)
- **x64-linux**: x64 Linux static build (recommended)
- **arm64-linux-dynamic**: ARM64 Linux dynamic build
- **x64-linux-dynamic**: x64 Linux dynamic build

### Windows
- **arm64-windows-static**: ARM64 Windows static build with MSVC (recommended)
- **x64-windows-static**: x64 Windows static build with MSVC (recommended)
- **arm64-windows-dynamic**: ARM64 Windows dynamic build with MSVC
- **x64-windows-dynamic**: x64 Windows dynamic build with MSVC

## Usage

### Building with Custom Triplets

```bash
# Set VCPKG_OVERLAY_TRIPLETS to include our custom triplets
export VCPKG_OVERLAY_TRIPLETS="/path/to/dwarfs/vcpkg_triplets"

# Build with a specific triplet
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON
```

### Using the Build Scripts

The build scripts will automatically detect and use the appropriate triplet:

```bash
# Auto-detects triplet based on current platform
./scripts/test-everything.sh

# Force specific triplet
VCPKG_TARGET_TRIPLET=arm64-osx-dynamic ./scripts/test-everything.sh --vcpkg
```

## Triplet Naming Convention

```
{architecture}-{os}-{linkage}
```

- **architecture**: `arm64`, `x64`
- **os**: `osx`, `linux`, `windows`
- **linkage**: (empty for static), `dynamic` for shared libraries

## Platform-Specific Notes

### macOS
- Minimum deployment target: macOS 11.0 (x64), macOS 13.0 (arm64)
- Uses Apple Clang compiler
- Can use system libraries via Homebrew

### Linux
- Uses GCC or Clang depending on distribution
- Position-independent code enabled by default
- System libraries detected via pkg-config

### Windows
- Uses MSVC compiler
- vcpkg required for all dependencies
- Static linking recommended for easier distribution

## Adding New Triplets

To add a new triplet:

1. Create a new `.cmake` file in this directory
2. Follow the naming convention above
3. Set required vcpkg variables:
   ```cmake
   set(VCPKG_TARGET_ARCHITECTURE arm64)
   set(VCPKG_CRT_LINKAGE static)
   set(VCPKG_LIBRARY_LINKAGE static)
   set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
   ```
4. Add to `scripts/lib/vcpkg_helper.sh:dwarfs_list_triplets()`
5. Update this README

## Tebako jemalloc Requirement

**CRITICAL**: All DwarFS builds require Tebako's fork of jemalloc 5.5.0, NOT the upstream jemalloc!

The overlay port at `../vcpkg_ports/jemalloc/` provides this dependency automatically when using vcpkg.

See `../BUILD_SYSTEM_ARCHITECTURE.md` for more details.
