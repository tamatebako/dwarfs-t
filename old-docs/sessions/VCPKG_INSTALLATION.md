# DwarFS vcpkg Installation Guide

This guide explains how to install DwarFS libraries and tools using the vcpkg package manager.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installing vcpkg](#installing-vcpkg)
- [Installing DwarFS](#installing-dwarfs)
  - [Installing Libraries](#installing-libraries)
  - [Installing Tools](#installing-tools)
- [Using DwarFS in CMake Projects](#using-dwarfs-in-cmake-projects)
- [Available Features](#available-features)
- [Platform Support](#platform-support)
- [Troubleshooting](#troubleshooting)
- [Examples](#examples)

---

## Prerequisites

### System Requirements

- **Operating System**: Linux, macOS, Windows, FreeBSD
- **CMake**: ≥3.20
- **Compiler**: 
  - GCC ≥10
  - Clang ≥12
  - MSVC ≥19.29 (Visual Studio 2019 v16.11)
- **Git**: For cloning vcpkg repository

### Build Tools

**Linux (Ubuntu/Debian)**:
```bash
sudo apt-get install -y build-essential cmake ninja-build pkg-config git
```

**macOS**:
```bash
brew install cmake ninja pkg-config git
```

**Windows**:
- Install Visual Studio 2019 or later with C++ desktop development workload
- Install Git for Windows
- Install CMake (via Visual Studio installer or standalone)

---

## Installing vcpkg

If you don't already have vcpkg installed:

```bash
# Clone vcpkg repository
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg

# Bootstrap vcpkg
cd ~/vcpkg
./bootstrap-vcpkg.sh  # Linux/macOS
# OR
.\bootstrap-vcpkg.bat  # Windows
```

Add vcpkg to your PATH (optional but recommended):

```bash
# Linux/macOS
echo 'export PATH="$HOME/vcpkg:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Windows (PowerShell)
$env:PATH += ";$HOME\vcpkg"
```

---

## Installing DwarFS

### Installing Libraries

The `libdwarfs` port provides all DwarFS libraries for reading, writing, and extracting DwarFS filesystem images.

**Basic Installation**:
```bash
vcpkg install libdwarfs
```

**With All Optional Features**:
```bash
vcpkg install libdwarfs[flac,lz4,lzma,brotli]
```

**Installed Libraries**:
- `dwarfs::dwarfs_common` - Core utilities, compression, serialization
- `dwarfs::dwarfs_reader` - Read DwarFS filesystem images
- `dwarfs::dwarfs_writer` - Create DwarFS filesystem images
- `dwarfs::dwarfs_extractor` - Extract files from DwarFS images
- `dwarfs::dwarfs_rewrite` - Recompress existing DwarFS images (if Thrift enabled)

### Installing Tools

The `dwarfs` port provides command-line tools for working with DwarFS images.

**Basic Installation**:
```bash
vcpkg install dwarfs
```

**Installed Tools**:
- `mkdwarfs` - Create DwarFS filesystem images
- `dwarfsck` - Check and inspect DwarFS images
- `dwarfsextract` - Extract files from DwarFS images

**With FUSE Driver (Linux only)**:
```bash
vcpkg install dwarfs[fuse]
```

This additionally installs:
- `dwarfs` - Mount DwarFS images via FUSE

---

## Using DwarFS in CMake Projects

### Basic Usage

After installing via vcpkg, use `find_package()` in your CMakeLists.txt:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_project)

# Find DwarFS package
find_package(dwarfs CONFIG REQUIRED)

# Link against DwarFS libraries
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE 
    dwarfs::dwarfs_common
    dwarfs::dwarfs_reader
)
```

### CMake Configuration

Always specify the vcpkg toolchain file when configuring:

```bash
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build
```

### Available Library Targets

| CMake Target | Description |
|--------------|-------------|
| `dwarfs::dwarfs_common` | Core utilities, compression algorithms, metadata serialization |
| `dwarfs::dwarfs_reader` | Read DwarFS images, FUSE driver integration |
| `dwarfs::dwarfs_writer` | Create DwarFS images, scanner, categorizer, segmenter |
| `dwarfs::dwarfs_extractor` | Extract files to disk or archive formats |
| `dwarfs::dwarfs_rewrite` | Recompress existing images (requires Thrift) |

---

## Available Features

### libdwarfs Features

| Feature | Description | Default | Dependencies |
|---------|-------------|---------|--------------|
| `flac` | FLAC audio compression support | OFF | `flac` |
| `lz4` | LZ4 fast compression support | OFF | `lz4` |
| `lzma` | LZMA/XZ compression support | OFF | `liblzma` |
| `brotli` | Brotli compression support | OFF | `brotli` |

**Example**: Install with all compression algorithms:
```bash
vcpkg install libdwarfs[flac,lz4,lzma,brotli]
```

### dwarfs Features

| Feature | Description | Platform | Default |
|---------|-------------|----------|---------|
| `fuse` | FUSE driver for mounting DwarFS images | Linux only | OFF |

**Example**: Install with FUSE driver:
```bash
vcpkg install dwarfs[fuse]
```

---

## Platform Support

### Linux

**Fully Supported**:
- All libraries and features
- FUSE2/FUSE3 driver support
- Extended attributes
- All compression algorithms

**Installation**:
```bash
vcpkg install libdwarfs[flac,lz4,lzma,brotli]
vcpkg install dwarfs[fuse]
```

### macOS

**Fully Supported**:
- All libraries and features
- FUSE via FUSE-T (userspace, no kernel extension)
- Extended attributes
- All compression algorithms

**Installation**:
```bash
vcpkg install libdwarfs[flac,lz4,lzma,brotli]
vcpkg install dwarfs
```

**Note**: For FUSE support, install FUSE-T separately:
```bash
brew tap macos-fuse-t/homebrew-cask
brew install fuse-t
```

### Windows

**Fully Supported**:
- All libraries and features
- FUSE via WinFsp
- NTFS extended attributes
- All compression algorithms

**Installation**:
```bash
vcpkg install libdwarfs[flac,lz4,lzma,brotli] --triplet=x64-windows
vcpkg install dwarfs --triplet=x64-windows
```

**Note**: For FUSE support, install WinFsp separately:
```powershell
choco install winfsp
```

### FreeBSD

**Supported via Linux Emulation**:
- All libraries and features
- FUSE via Linux compatibility layer
- Static binaries from Linux builds work with emulation

---

## Troubleshooting

### Issue: "Package not found"

**Problem**: vcpkg cannot find the dwarfs ports.

**Solution**: Use `--overlay-ports` to specify the port location:
```bash
vcpkg install libdwarfs --overlay-ports=/path/to/dwarfs/ports
```

### Issue: "find_package(dwarfs) not found"

**Problem**: CMake cannot locate the DwarFS package config.

**Solution**: Ensure you're using the vcpkg toolchain file:
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Issue: Build errors on Windows

**Problem**: Compilation fails with missing dependencies.

**Solution**: Use static linking triplet:
```bash
vcpkg install libdwarfs --triplet=x64-windows-static
```

### Issue: FUSE feature not available

**Problem**: FUSE feature fails to install on non-Linux platforms.

**Solution**: FUSE driver is Linux-only in vcpkg. On macOS/Windows:
- macOS: Install FUSE-T separately via Homebrew
- Windows: Install WinFsp separately via Chocolatey

### Issue: Slow installation

**Problem**: vcpkg takes a long time to build from source.

**Solution**: vcpkg uses binary caching by default. For first-time installations:
```bash
# Enable binary caching
vcpkg install libdwarfs --binarysource=clear

# Or use manifest mode for automatic caching
```

### Issue: Version conflicts

**Problem**: Dependency version conflicts with existing packages.

**Solution**: Use manifest mode with version constraints:
```json
{
  "dependencies": [
    {
      "name": "libdwarfs",
      "version>=": "0.16.0"
    }
  ]
}
```

---

## Examples

### Example 1: Simple DwarFS Reader

```cpp
#include <dwarfs/reader/filesystem_loader.h>
#include <dwarfs/logger.h>
#include <iostream>

int main() {
    // Create logger
    auto lgr = dwarfs::stream_logger::create(std::cerr);
    
    // Create OS access
    auto os = dwarfs::os_access::create();
    
    // Configure filesystem loading
    dwarfs::reader::filesystem_load_config config;
    config.image_path = "example.dwarfs";
    config.cache_size = 1024 * 1024 * 128; // 128 MiB cache
    config.num_workers = 4;
    
    // Load filesystem
    auto fs = dwarfs::reader::filesystem_loader::load(*lgr, *os, config);
    
    // Access filesystem
    std::cout << "Filesystem loaded: " << config.image_path << std::endl;
    
    return 0;
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(dwarfs_reader_example)

find_package(dwarfs CONFIG REQUIRED)

add_executable(reader_example main.cpp)
target_link_libraries(reader_example PRIVATE dwarfs::dwarfs_reader)
```

**Build**:
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/reader_example
```

### Example 2: Using Compression

```cpp
#include <dwarfs/block_compressor.h>
#include <dwarfs/compression_registry.h>
#include <iostream>

int main() {
    // Get compression registry
    auto& registry = dwarfs::compression_registry::instance();
    
    // Create zstd compressor
    auto compressor = registry.make_compressor("zstd:level=9");
    
    // Compress data
    std::string data = "Hello, DwarFS!";
    std::vector<uint8_t> input(data.begin(), data.end());
    std::vector<uint8_t> output;
    
    compressor->compress(input, output);
    
    std::cout << "Original size: " << input.size() << std::endl;
    std::cout << "Compressed size: " << output.size() << std::endl;
    
    return 0;
}
```

### Example 3: Manifest Mode

**vcpkg.json**:
```json
{
  "name": "my-dwarfs-app",
  "version": "1.0.0",
  "dependencies": [
    {
      "name": "libdwarfs",
      "features": ["flac", "lz4", "lzma", "brotli"]
    }
  ]
}
```

**Usage**:
```bash
# vcpkg will automatically install dependencies
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

---

## Additional Resources

- **DwarFS Documentation**: [README.md](../README.md)
- **GitHub Repository**: https://github.com/tamatebako/dwarfs
- **vcpkg Documentation**: https://vcpkg.io/
- **Issue Tracker**: https://github.com/tamatebako/dwarfs/issues

---

## License

DwarFS is licensed under GPL-3.0. See [LICENSE.GPL-3.0](../LICENSE.GPL-3.0) for details.

---

**Last Updated**: 2025-12-01  
**Version**: 0.16.0