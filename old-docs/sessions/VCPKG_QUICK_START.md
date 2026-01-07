# vcpkg Quick Start Guide

## Installation

### For End Users (via vcpkg)

Once published to vcpkg registry:

```bash
# Install libraries
vcpkg install libdwarfs

# Install tools
vcpkg install dwarfs

# With all features
vcpkg install libdwarfs[flac,lz4,lzma,brotli]
vcpkg install dwarfs[fuse]  # Linux only
```

### For Developers (local testing)

```bash
# 1. Install vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh

# 2. Install from this repository
cd /Users/mulgogi/src/external/dwarfs
~/vcpkg/vcpkg install libdwarfs --overlay-ports=./ports
~/vcpkg/vcpkg install dwarfs --overlay-ports=./ports
```

## Usage

### In CMake Projects

```cmake
cmake_minimum_required(VERSION 3.28)
project(myapp)

# Find DwarFS
find_package(dwarfs CONFIG REQUIRED)

# Link libraries
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE dwarfs::dwarfs_reader)
```

### Example Application

```cpp
#include <dwarfs/reader/filesystem_loader.h>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <image.dwarfs>" << std::endl;
        return 1;
    }

    try {
        dwarfs::reader::filesystem_load_config config;
        config.image_path = argv[1];
        config.cache_size = 1024 * 1024 * 1024;  // 1 GiB cache

        auto lgr = dwarfs::stream_logger::create(std::cerr);
        auto os = dwarfs::os_access::create();
        auto fs = dwarfs::reader::filesystem_loader::load(*lgr, *os, config);

        std::cout << "Successfully loaded DwarFS image!" << std::endl;
        return 0;
    } catch (std::exception const& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
```

Build and run:
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/myapp test.dwarfs
```

## Command-Line Tools

After installing `dwarfs` port:

```bash
# Create DwarFS image
mkdwarfs -i /path/to/source -o image.dwarfs

# Check image
dwarfsck image.dwarfs

# Extract files
dwarfsextract -i image.dwarfs -o /path/to/dest

# Mount (Linux with fuse feature)
dwarfs image.dwarfs /mnt/point
```

## Features

### libdwarfs Features

| Feature | Description | Enable With |
|---------|-------------|-------------|
| flac | FLAC audio compression | `libdwarfs[flac]` |
| lz4 | LZ4 fast compression | `libdwarfs[lz4]` |
| lzma | LZMA/XZ compression | `libdwarfs[lzma]` |
| brotli | Brotli compression | `libdwarfs[brotli]` |

### dwarfs Features

| Feature | Description | Platform | Enable With |
|---------|-------------|----------|-------------|
| fuse | FUSE driver | Linux | `dwarfs[fuse]` |

## Troubleshooting

### libdwarfs not found

```bash
# Verify installation
~/vcpkg/vcpkg list | grep dwarfs

# Check CMake can find it
cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Missing compression support

Install with features:
```bash
~/vcpkg/vcpkg install libdwarfs[flac,lz4]
```

### FUSE not available

**Linux**: Install with fuse feature:
```bash
~/vcpkg/vcpkg install dwarfs[fuse]
```

**macOS**: Manual FUSE-T installation required:
```bash
brew tap macos-fuse-t/homebrew-cask
brew install fuse-t
```

**Windows**: Manual WinFsp installation required:
```powershell
choco install winfsp
```

## Platform Support

| Platform | Libraries | Tools | FUSE |
|----------|-----------|-------|------|
| Linux (x64, arm64) | ✅ | ✅ | ✅ |
| macOS (x64, arm64) | ✅ | ✅ | ⚠️ Manual |
| Windows (x64) | ✅ | ✅ | ⚠️ Manual |

⚠️ = Requires manual installation (not in vcpkg)

## Links

- **Project**: https://github.com/tamatebako/dwarfs
- **Original**: https://github.com/mhx/dwarfs
- **Documentation**: See `/doc` directory in repository
- **vcpkg**: https://github.com/microsoft/vcpkg