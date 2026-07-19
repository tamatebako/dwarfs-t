# DwarFS vcpkg Ports

This directory contains vcpkg port definitions for DwarFS libraries and tools.

## Ports

### libdwarfs
DwarFS libraries for C++ applications.

**Install**:
```bash
vcpkg install libdwarfs
vcpkg install libdwarfs[flac,lz4,lzma,brotli]  # With optional compression
```

**Usage**:
```cmake
find_package(dwarfs CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE 
    dwarfs::dwarfs_common
    dwarfs::dwarfs_reader
    dwarfs::dwarfs_writer
)
```

### dwarfs
Command-line tools for creating and working with DwarFS images.

**Install**:
```bash
vcpkg install dwarfs        # Tools only
vcpkg install dwarfs[fuse]  # Tools + FUSE driver (Linux)
```

**Tools**: mkdwarfs, dwarfsck, dwarfsextract, dwarfs (with fuse feature)

## Local Testing

### Test from this repository

```bash
# Install vcpkg (if not already)
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
cd ~/vcpkg && ./bootstrap-vcpkg.sh

# Test overlay install
cd /Users/mulgogi/src/external/dwarfs
~/vcpkg/vcpkg install libdwarfs --overlay-ports=./ports
~/vcpkg/vcpkg install dwarfs --overlay-ports=./ports

# Verify
~/vcpkg/vcpkg list | grep dwarfs
```

### Test consumer project

```bash
mkdir -p /tmp/dwarfs_test && cd /tmp/dwarfs_test

cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.28)
project(dwarfs_consumer CXX)
find_package(dwarfs CONFIG REQUIRED)
add_executable(test_app main.cpp)
target_link_libraries(test_app PRIVATE dwarfs::dwarfs_reader)
EOF

cat > main.cpp << 'EOF'
#include <dwarfs/reader/filesystem_loader.h>
#include <iostream>
int main() {
    std::cout << "DwarFS library linked successfully!" << std::endl;
    return 0;
}
EOF

cmake -B build -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/test_app
```

## Known Issues

### SHA512 Placeholder
Both portfiles use placeholder SHA512. Before submission to vcpkg:

```bash
cd /tmp
git clone https://github.com/tamatebako/dwarfs.git
cd dwarfs
git checkout v0.16.0
git archive --format=tar.gz --prefix=dwarfs-0.16.0/ HEAD > ../dwarfs-0.16.0.tar.gz
shasum -a 512 ../dwarfs-0.16.0.tar.gz
```

Update SHA512 in both `portfile.cmake` files.

### Platform-Specific Notes

**macOS**:
- FUSE-T not in vcpkg (manual install required)
- Consider adding macFUSE as alternative

**Windows**:
- WinFsp not in vcpkg (manual install required)
- May need separate handling for FUSE support

**Linux**:
- FUSE3 available in vcpkg
- Full functionality supported

## Submission to vcpkg

Before submitting to official vcpkg repository:

1. **Update SHA512** with actual release archive hash
2. **Test on all platforms** (Linux, macOS, Windows)
3. **Verify dependencies** resolve correctly
4. **Test consumer projects** on each platform
5. **Follow vcpkg PR guidelines**: https://github.com/microsoft/vcpkg/blob/master/docs/contributing.md

## Maintainer Notes

- Both ports disable Thrift (FlatBuffers-only) for portability
- libdwarfs exports namespace `dwarfs::` (not `libdwarfs::`)
- Config files: `dwarfs-config.cmake`, `dwarfs-targets.cmake`
- Tools install to `${VCPKG_ROOT}/installed/${TRIPLET}/tools/dwarfs/`