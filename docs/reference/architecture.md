# Tebako DwarFS Architecture Documentation

## Table of Contents

1. [Overview](#overview)
2. [For Developers](#for-developers)
   - [Project Architecture](#project-architecture)
   - [Build System](#build-system)
   - [Code Organization](#code-organization)
   - [Metadata Serialization](#metadata-serialization)
   - [Testing Framework](#testing-framework)
   - [Development Workflow](#development-workflow)
3. [For Release Managers](#for-release-managers)
   - [Release Process](#release-process)
   - [Platform Support Matrix](#platform-support-matrix)
   - [Compatibility Testing](#compatibility-testing)
   - [CI/CD Workflows](#cicd-workflows)
   - [Versioning Strategy](#versioning-strategy)
   - [Dependency Management](#dependency-management)
4. [For Users](#for-users)
   - [Installation](#installation)
   - [Supported Platforms](#supported-platforms)
   - [Basic Usage](#basic-usage)
   - [Homebrew Compatibility](#homebrew-compatibility)
   - [Performance Characteristics](#performance-characteristics)
   - [Known Limitations](#known-limitations)

---

## Overview

Tebako DwarFS is a read-only filesystem that stores data in high-performance compressed filesystem images. It is a fork of the original DwarFS project, specifically adapted for Tebako's use cases. This document provides comprehensive architectural information for three key audiences: developers contributing to the project, release managers managing releases, and users integrating DwarFS into their applications.

### Key Features

- **Read-only filesystem** with FUSE support
- **Multiple metadata serialization formats** (FlatBuffers, Legacy Thrift, Modern Thrift)
- **High-performance compression** with various algorithms
- **Cross-platform support** (Linux, macOS)
- **Multi-architecture support** (x86_64, arm64)
- **Homebrew compatibility** for macOS users

### Repository Structure

```
dwarfs/
├── cmake/                    # CMake build configuration
│   ├── folly.cmake          # Folly dependency setup
│   ├── libdwarfs.cmake      # libdwarfs library definition
│   └── ...
├── include/dwarfs/          # Public headers
│   ├── metadata/            # Metadata serialization headers
│   ├── reader/              # Reader implementation headers
│   └── ...
├── src/                     # Source implementation
│   ├── metadata/            # Metadata serialization code
│   ├── reader/              # Reader implementation
│   └── ...
├── test/                    # Test suites
│   ├── compat/              # Compatibility tests
│   └── ...
├── vcpkg_ports/             # Custom vcpkg ports
│   └── jemalloc/            # Tebako's jemalloc fork
├── scripts/                 # Build and utility scripts
│   └── clean-build.sh       # Clean build script
└── CLAUDE.md                # Development notes for Claude
```

---

## For Developers

### Project Architecture

#### High-Level Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         Application Layer                        │
│                  (mkdwarfs, dwarfs, dwarfsextract)              │
└─────────────────────────────┬───────────────────────────────────┘
                              │
┌─────────────────────────────┴───────────────────────────────────┐
│                       DwarFS Library (libdwarfs)                 │
├─────────────────────────────────────────────────────────────────┤
│  ┌───────────────┐  ┌───────────────┐  ┌─────────────────────┐ │
│  │  Metadata     │  │   Compression │  │   Filesystem        │ │
│  │  Serialization│  │   Layer       │  │   Layer             │ │
│  ├───────────────┤  ├───────────────┤  ├─────────────────────┤ │
│  │ • FlatBuffers │  │ • LZ4         │  │ • FUSE Interface    │ │
│  │ • Legacy      │  │ • ZSTD        │  │ • VFS Layer         │ │
│  │   Thrift      │  │ • FLAC        │  │ • Cache Management  │ │
│  │ • Modern      │  │ • PCMAUDIO    │  │ • MMAP Support      │ │
│  │   Thrift      │  │ • Rice        │  │                     │ │
│  └───────────────┘  └───────────────┘  └─────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                     Core Dependencies                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │   Folly     │  │  Fizz       │  │  jemalloc   │             │
│  │  (Facebook  │  │  (SSL/TLS   │  │  (Tebako's  │             │
│  │   Libraries)│  │   Library)  │  │   Fork)     │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
└─────────────────────────────────────────────────────────────────┘
```

#### Metadata Domain Model

The metadata domain (`dwarfs::metadata::domain`) represents the in-memory model of filesystem metadata:

```
namespace dwarfs::metadata::domain {

class metadata {
  std::vector<directory> directories;
  std::vector<inode_data> inodes;
  std::vector<chunk> chunks;
  std::vector<std::string> names;
  std::vector<uint32_t> modes;
  std::vector<uint64_t> uids;
  std::vector<uint64_t> gids;
  // ... other metadata fields
};

class directory {
  uint32_t parent_entry() const;  // Returns parent directory entry
  uint32_t first_entry() const;   // First entry in this directory
  uint32_t self_entry() const;    // This directory's own entry
};

class chunk {
  uint32_t block() const;   // Block index
  uint32_t offset() const;  // Offset within block
  uint32_t size() const;    // Size in bytes
};

class inode_data {
  uint32_t mode_index;
  uint32_t owner_index;
  uint32_t group_index;
  uint64_t atime_offset;    // Offset from timestamp_base
  uint64_t mtime_offset;
  uint64_t ctime_offset;
  // ... other inode fields
};

} // namespace dwarfs::metadata::domain
```

### Build System

#### Critical: Use the Clean Build Script

**NEVER manually run cmake or ninja to rebuild the DwarFS library!** Always use the provided clean-build script:

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

#### vcpkg Integration

The project uses vcpkg for dependency management with custom overlay ports:

```bash
# vcpkg configuration
VCPKG_ROOT=$HOME/src/vcpkg
VCPKG_OVERLAY_PORTS=$PROJECT_ROOT/vcpkg_ports
VCPKG_TARGET_TRIPLET=arm64-osx-static  # macOS arm64
# or x64-osx-static                     # macOS x86_64
# or x64-linux-static                   # Linux x86_64
```

#### CMake Build Configuration

```cmake
# Key CMake variables
set(JEMALLOC_REQUIRED_VERSION 5.5.0)  # Tebako's fork
set(JEMALLOC_GIT_REPO https://github.com/tamatebako/jemalloc.git)

# Metadata format selection
option(DWARFS_HAVE_FLATBUFFERS "Enable FlatBuffers" ON)
option(DWARFS_HAVE_THRIFT "Enable Modern Thrift" OFF)
```

#### After Running the Script

```bash
cd build
ninja              # Build the project
ctest -R metadata  # Run metadata tests
```

### Code Organization

#### Metadata Serialization (`src/metadata/`)

```
src/metadata/
├── legacy/                    # Legacy Thrift (Frozen2) implementation
│   ├── frozen2_deserializer.cpp
│   ├── frozen2_serializer.cpp
│   └── frozen_schema.cpp
├── serialization/             # Modern serialization formats
│   ├── flatbuffers_serializer.cpp
│   └── legacy_thrift_serializer.cpp
└── converters/                # Format converters
    ├── thrift_metadata_converter.cpp
    └── ...
```

#### Reader Implementation (`src/reader/`)

```
src/reader/
├── internal/
│   ├── metadata_factory.cpp   # Metadata format detection
│   ├── domain_metadata_views.cpp
│   └── ...
└── ...
```

#### Key Headers

- `include/dwarfs/metadata/domain/metadata.h` - Metadata domain model
- `include/dwarfs/metadata/legacy/frozen_schema.h` - DenseMap and schema definitions
- `include/dwarfs/reader/internal/metadata_factory.h` - Metadata loading factory

### Metadata Serialization

#### Serialization Format Comparison

| Feature | FlatBuffers | Legacy Thrift | Modern Thrift |
|---------|-------------|---------------|---------------|
| **Dependencies** | vcpkg flatbuffers | None (hand-coded) | vcpkg fbthrift |
| **Performance** | Fastest | Fast | Fast |
| **Size** | Compact | Compact | Compact |
| **Availability** | Default | Always available | Optional |
| **Use Case** | Production | Fallback/Legacy | Future |

#### DenseMap Data Structure

Located in `include/dwarfs/metadata/legacy/frozen_schema.h`:

```cpp
// Efficient map for i16 -> T using std::vector<std::optional<T>>
// Key Type: int16_t (1-based indexing: keys 1, 2, 3, ...)
// API: Returns T* pointer (NOT std::optional<T> by value!)

template <typename T>
class DenseMap {
  std::vector<std::optional<T>> data_;

public:
  T* get(int16_t key);  // Returns nullptr if not found
  // Note: Pointer is stable until vector reallocation
};
```

**Critical**: The `get()` method returns a pointer to the vector element, which is stable until vector reallocation. Never store the optional by value.

#### Metadata Factory Pattern

```cpp
// src/reader/internal/metadata_factory.cpp

class MetadataFactory {
  // Detects metadata format and creates appropriate metadata
  std::unique_ptr<metadata_v2> load_metadata(
      std::span<uint8_t const> data) const {
    // 1. Try FlatBuffers first (default)
    // 2. Fallback to Legacy Thrift
    // 3. Optionally try Modern Thrift
  }
};
```

### Testing Framework

#### Unit Tests (`test/`)

```
test/
├── metadata_test.cpp              # Metadata serialization tests
├── metadata_factory_test.cpp      # Metadata factory tests
├── metadata/serialization_test.cpp # Format-specific tests
└── ...
```

#### Compatibility Tests (`test/compat/`)

The compatibility testing framework validates compatibility with Homebrew's dwarfs:

```
test/compat/
├── lib/                           # Test library
│   ├── homebrew_detector.h/cpp    # Homebrew detection
│   ├── fixture_generator.h/cpp    # Fixture generation
│   ├── fixture_cache.h/cpp        # Fixture caching
│   └── compatibility_tester.h/cpp # Test runner
├── tests/                         # Test executables
│   ├── read_homebrew_files_test.cpp
│   ├── write_compatible_files_test.cpp
│   └── round_trip_test.cpp
└── CMakeLists.txt                 # Test build configuration
```

#### Running Tests

```bash
cd build
ninja                    # Build first
ctest -R metadata        # Run metadata tests
ctest -R compat          # Run compatibility tests
```

### Development Workflow

#### 1. Feature Development

```bash
# Create a feature branch
git checkout -b feature/my-feature

# Make your changes
vim src/metadata/...

# Build and test
BUILD_DIR=build-dev ./scripts/clean-build.sh -y
cd build-dev
ninja
ctest -R metadata
```

#### 2. Code Review Checklist

- [ ] Code follows MECE principles
- [ ] No hardcoded values (use constants or configuration)
- [ ] Separation of concerns maintained
- [ ] Object-oriented design principles applied
- [ ] Comprehensive tests included
- [ ] Documentation updated
- [ ] No memory leaks (use valgrind if needed)
- [ ] Thread-safe if applicable

#### 3. Common Patterns

##### RAII for Resource Management

```cpp
class FileHandle {
  int fd_;
public:
  explicit FileHandle(int fd) : fd_(fd) {}
  ~FileHandle() { if (fd_ >= 0) close(fd_); }
  // Delete copy, allow move
};
```

##### Error Handling

```cpp
try {
  // Operation that may throw
} catch (const std::exception& e) {
  LOG_ERROR << "Operation failed: " << e.what();
  throw;  // Re-throw for caller to handle
}
```

---

## For Release Managers

### Release Process

#### Pre-Release Checklist

1. **Compatibility Testing**
   ```bash
   cd test/compat
   ./scripts/run_all_compatibility_tests.sh
   ```

2. **Build All Targets**
   ```bash
   # Test all build configurations
   ./scripts/test-all-configs.sh
   ```

3. **Run Benchmarks**
   ```bash
   cd benchmarks
   ./run_comprehensive_benchmark.sh
   ```

4. **Verify Platform Support**
   ```bash
   # Test on all supported platforms
   # - macOS arm64 (default Homebrew)
   # - macOS x86_64 (Rosetta or native)
   # - Linux x86_64
   # - Linux arm64
   ```

#### Release Steps

1. **Update Version Numbers**
   - Update `CMakeLists.txt`
   - Update `vcpkg.json` if needed
   - Update documentation

2. **Create Release Branch**
   ```bash
   git checkout -b release/vX.Y.Z
   ```

3. **Run Full Test Suite**
   ```bash
   ./scripts/build-all-and-test.sh
   ```

4. **Tag Release**
   ```bash
   git tag -a vX.Y.Z -m "Release X.Y.Z"
   git push origin vX.Y.Z
   ```

5. **Build Release Artifacts**
   - Build for all supported platforms
   - Create distribution packages
   - Generate checksums

### Platform Support Matrix

| Platform | Architecture | Status | Tested By |
|----------|--------------|--------|-----------|
| macOS | arm64 (Apple Silicon) | ✅ Fully Supported | Homebrew CI |
| macOS | x86_64 (Intel) | ✅ Fully Supported | Homebrew CI |
| Linux | x86_64 | ✅ Fully Supported | GitHub Actions |
| Linux | arm64 | 🟡 Best Effort | Community |

**Note**: Tebako-specific testing focuses on macOS arm64 (primary platform for Tebako).

### Compatibility Testing

#### Homebrew Compatibility Test Framework

Located in `test/compat/`, this framework validates compatibility with Homebrew's dwarfs:

```
Compatibility Test Suite
├── Read Homebrew Files Test
│   └── Can we read DFT files created by Homebrew mkdwarfs?
├── Write Compatible Files Test
│   └── Can we write DFT files Homebrew dwarfs can read?
└── Round Trip Test
    └── Full round-trip serialization/deserialization
```

#### Fixture Naming Convention

```
dwarfs-v{version}-{platform}-{arch}.dft

Examples:
- dwarfs-v0.14.1-darwin-arm64.dft
- dwarfs-vlatest-linux-x86_64.dft
```

#### Running Compatibility Tests

```bash
cd test/compat

# Run all compatibility tests
ctest -R compat

# Run specific test
./read_homebrew_files_test
./write_compatible_files_test
./round_trip_test
```

### CI/CD Workflows

#### GitHub Actions Workflows

```
.github/workflows/
├── build-test.yml        # Main build and test workflow
├── compat-test.yml       # Compatibility testing
└── release.yml           # Release automation
```

#### Workflow Triggers

- **Pull Request**: Run full test suite on all platforms
- **Push to main**: Run compatibility tests and benchmarks
- **Tag**: Create release artifacts and publish

### Versioning Strategy

#### Semantic Versioning

- **Major version**: Breaking changes
- **Minor version**: New features, backward compatible
- **Patch version**: Bug fixes, backward compatible

#### Example: `0.14.1`

- `0`: Major version (still in active development)
- `14`: Minor version (feature release)
- `1`: Patch version (bug fix release)

### Dependency Management

#### Critical Dependencies

| Dependency | Version | Source | Notes |
|------------|---------|--------|-------|
| jemalloc | 5.5.0 | Tebako fork | **CRITICAL**: Must use Tebako's fork |
| folly | Latest | vcpkg | Facebook's C++ libraries |
| fizz | Latest | vcpkg | SSL/TLS library |
| fbthrift | 2025.12.29.00+ | vcpkg | Optional, for Modern Thrift |
| flatbuffers | Latest | vcpkg | Default metadata format |

#### Tebako jemalloc Fork

**CRITICAL**: This project REQUIRES Tebako's fork of jemalloc, NOT the upstream jemalloc!

- **Repository**: https://github.com/tamatebako/jemalloc.git
- **Version**: 5.5.0
- **Location**: `vcpkg_ports/jemalloc/` (overlay port)
- **CMake Variable**: `JEMALLOC_REQUIRED_VERSION` must be 5.5.0
- **Git Repo Variable**: `JEMALLOC_GIT_REPO` set to https://github.com/tamatebako/jemalloc.git

**Why Tebako jemalloc?**

The upstream jemalloc/jemalloc does NOT work with this project. We MUST use the Tebako fork which has specific patches for:
- Folly compatibility (without je_ prefix)
- Tebako-specific modifications
- Version 5.5.0 with custom build configuration

---

## For Users

### Installation

#### Prerequisites

- **macOS**: Xcode Command Line Tools
- **Linux**: GCC 9+ or Clang 10+
- **vcpkg**: https://github.com/microsoft/vcpkg
- **CMake**: 3.20+

#### Building from Source

```bash
# Clone the repository
git clone https://github.com/your-org/dwarfs.git
cd dwarfs

# Build with vcpkg
export VCPKG_ROOT=$HOME/src/vcpkg
export BUILD_DIR=$PWD/build
mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_OVERLAY_PORTS=$PWD/../vcpkg_ports \
      -DVCPKG_TARGET_TRIPLET=arm64-osx-static \
      -DDWARFS_HAVE_FLATBUFFERS=ON \
      -DDWARFS_HAVE_THRIFT=OFF \
      ..
cmake --build .
```

#### Installing via Homebrew (macOS)

```bash
# Install Homebrew dwarfs
brew install dwarfs

# Verify installation
mkdwarfs --version
dwarfs --version
```

### Supported Platforms

#### macOS

- **macOS 11+ (Big Sur) or later**
- **Apple Silicon (arm64)** - Primary platform, fully supported
- **Intel (x86_64)** - Fully supported (Rosetta 2 or native)

#### Linux

- **Ubuntu 20.04+ or equivalent**
- **x86_64** - Primary platform, fully supported
- **arm64** - Best effort support (community tested)

### Basic Usage

#### Creating a DwarFS Image

```bash
# Create a compressed filesystem image
mkdwarfs -o image.dwarfs /path/to/source/directory

# With compression level
mkdwarfs -o image.dwarfs -l 9 /path/to/source/directory

# With specific compression algorithm
mkdwarfs -o image.dwarfs -C zstd /path/to/source/directory
```

#### Mounting a DwarFS Image

```bash
# Mount the image
dwarfs image.dwarfs /mount/point

# With cache size limit
dwarfs --cache size 1G image.dwarfs /mount/point

# With decompression threads
dwarfs --workers 4 image.dwarfs /mount/point
```

#### Extracting from a DwarFS Image

```bash
# Extract entire image
dwarfsextract -i image.dwarfs -o /output/directory

# Extract specific files
dwarfsextract -i image.dwarfs -o /output/directory path/to/file
```

### Homebrew Compatibility

Tebako DwarFS maintains compatibility with Homebrew's dwarfs implementation.

#### Reading Homebrew-Created Images

```bash
# Homebrew mkdwarfs creates images that Tebako dwarfs can read
brew install dwarfs
mkdwarfs -o homebrew_image.dwarfs /path/to/source

# Mount with Tebako dwarfs
/path/to/tebako/dwarfs homebrew_image.dwarfs /mount/point
```

#### Writing Compatible Images

```bash
# Tebako dwarfs can create images that Homebrew dwarfs can read
/path/to/tebako/mkdwarfs -o tebako_image.dwarfs /path/to/source

# Mount with Homebrew dwarfs
dwarfs tebako_image.dwarfs /mount/point
```

#### Compatibility Verification

Run the compatibility test suite:

```bash
cd test/compat
./run_compatibility_tests.sh
```

### Performance Characteristics

#### Compression Algorithms

| Algorithm | Compression Ratio | Decompression Speed | Use Case |
|-----------|-------------------|---------------------|----------|
| **LZ4** | Low | Very Fast | Real-time access |
| **ZSTD** | High | Fast | General purpose |
| **FLAC** | Very High | Slow | Audio files |
| **PCMAUDIO** | Medium | Fast | Audio files |

#### Memory Usage

- **Cache**: Configurable size (default: 512MB)
- **Worker threads**: 4 threads recommended
- **Memory-mapped files**: Used for large files

#### Performance Tips

1. **Use LZ4** for frequently accessed files
2. **Use ZSTD** for archival storage
3. **Increase cache size** for better performance
4. **Use multiple worker threads** for decompression

### Known Limitations

#### Write Limitations

- DwarFS is **read-only** after creation
- Cannot modify existing images
- Must recreate image to update contents

#### Platform Limitations

- **Windows**: Not supported (no FUSE on Windows)
- **BSD**: Limited support (community tested)

#### Metadata Limitations

- Limited support for:
  - Extended attributes (xattrs)
  - Access Control Lists (ACLs)
  - Sparse files
  - Hard links (limited support)

#### File Size Limitations

- Maximum individual file size: **16 TB**
- Maximum total filesystem size: **1 PB**

---

## Appendix

### Useful Commands

```bash
# Check Homebrew dwarfs version
brew info dwarfs

# Check Tebako dwarfs version
mkdwarfs --version

# List DwarFS image metadata
dwarfsck --info image.dwarfs

# Verify DwarFS image integrity
dwarfsck image.dwarfs

# Benchmark compression
mkdwarfs -o image.dwarfs --categorize --benchmark /path/to/source
```

### Debug Output

Enable debug logging:

```bash
# Enable verbose output
export DWARFS_LOG_LEVEL=debug

# Enable FUSE debug output
dwarfs --debug image.dwarfs /mount/point
```

### Getting Help

- **Documentation**: See `CLAUDE.md` for development notes
- **Issues**: Report bugs on GitHub Issues
- **Discussions**: Use GitHub Discussions for questions
- **Email**: Contact the Tebako team for enterprise support

---

**Document Version**: 1.0
**Last Updated**: 2025-01-17
**Maintained By**: Tebako DwarFS Team
