# Modern Thrift Metadata Format Guide

**Version**: v0.17.0+
**Format ID**: `MODERN_THRIFT`
**Status**: ✅ Production-ready (validated 2026-01-06)

---

## Overview

Modern Thrift is a high-performance metadata serialization format using Apache Thrift's CompactProtocol. Introduced in v0.17.0, it provides the smallest metadata footprint of all DwarFS formats while maintaining full compatibility with DwarFS features.

**Key Characteristics**:
- **Smallest size**: 0.07-1.41% smaller than FlatBuffers (baseline 100%)
- **Fast serialization**: CompactProtocol optimized bit-packing
- **Zero-copy reads**: Memory-mappable like FlatBuffers
- **Requires dependencies**: Full Facebook stack (Folly + fbthrift + jemalloc)

---

## When to Use Modern Thrift

### ✅ Use Modern Thrift When

1. **Absolute minimum size is critical**
   - Storage cost is high
   - Network bandwidth is limited
   - Size difference of 0.07-1.41% matters

2. **You already have Facebook stack**
   - Existing Folly deployment
   - fbthrift infrastructure in place
   - Comfortable with complex build toolchain

3. **Platform support is sufficient**
   - Building on Ubuntu, macOS x64/ARM64, Windows x64
   - vcpkg overlay ports available
   - CI/CD can handle 45-60 min initial builds

### ❌ Use FlatBuffers Instead When

1. **Portability is priority**
   - Header-only library
   - Simple build process
   - Works on all platforms

2. **Build simplicity matters**
   - No complex dependencies
   - Fast compilation (5-10 min)
   - Easy cross-compilation

3. **Size overhead is acceptable**
   - 1.41% overhead at level 3 is negligible
   - 385 KB difference on 27 MB metadata

---

## Creating Modern Thrift Images

### Basic Creation

```bash
# Create with Modern Thrift metadata
mkdwarfs -i /source -o image.dtc --metadata-format=MODERN_THRIFT

# With compression level
mkdwarfs -i /source -o image.dtc --metadata-format=MODERN_THRIFT -l 3

# With progress
mkdwarfs -i /source -o image.dtc --metadata-format=MODERN_THRIFT --progress
```

### Recommended File Extension

**Always use `.dtc`** for Modern Thrift images:
- `.dtc` = **D**warFS **T**hrift **C**ompact
- Makes format clear to users
- Prevents confusion with Legacy Thrift (`.dth`)

### Verification

```bash
# Check image was created with Modern Thrift
dwarfsck image.dtc

# Output should show:
# metadata format: modern_thrift (CompactProtocol)

# Verify magic bytes
hexdump -C image.dtc | head -1
# Should show: 00000000  82 21 ...  (0x82 0x21 = CompactProtocol magic)
```

---

## Reading Modern Thrift Images

All DwarFS tools automatically detect format via magic bytes:

### Mounting

```bash
# Mount Modern Thrift image
dwarfs image.dtc /mnt/point

# With cache configuration
dwarfs image.dtc /mnt/point -o cache_size=1g -o workers=4
```

### Extraction

```bash
# Extract entire image
dwarfsextract -i image.dtc -o /destination

# Extract specific path
dwarfsextract -i image.dtc -o /destination --input-file=/path/in/image

# Multi-threaded extraction
dwarfsextract -i image.dtc -o /destination --num-workers=8
```

### Inspection

```bash
# Check integrity
dwarfsck image.dtc

# Detailed JSON output
dwarfsck image.dtc --print-metadata --json > metadata.json

# List all files
dwarfsck image.dtc --list-files

# Check specific compression
dwarfsck image.dtc --check-integrity
```

---

## Performance Characteristics

Based on comprehensive benchmarking (Session 19 - Perl 5.43.3 dataset):

### Compression Speed (vs FlatBuffers)

| Level | Modern Thrift Time | FlatBuffers Time | Performance |
|-------|-------------------|------------------|-------------|
| Level 1 (fast) | 2.095s | 1.489s | 28.9% slower |
| Level 3 (default) | 3.617s | 2.999s | 17.1% slower |
| Level 9 (max) | 26.606s | 27.043s | 1.6% faster (≈equal) |

**Analysis**:
- FlatBuffers is faster at typical compression levels (1-3)
- Performance converges at maximum compression (level 9)
- Difference is in serialization overhead, not compression itself

### Extraction Speed

| Metric | Modern Thrift | FlatBuffers | Delta |
|--------|---------------|-------------|-------|
| Extraction Time (level 3) | 1.998s | 2.069s | -3.4% (slightly faster) |
| **Content Verification** | ✅ Identical | ✅ Identical | **Byte-for-byte match** |

**Analysis**:
- Extraction speeds are virtually identical (3.4% within noise)
- Both formats produce byte-for-byte identical files (verified via SHA256)
- Both use zero-copy memory-mapped access

### Image Size (vs FlatBuffers)

| Level | Modern Thrift Size | FlatBuffers Size | Overhead | Absolute |
|-------|-------------------|------------------|----------|----------|
| Level 1 | 36,433,300 bytes | 36,773,465 bytes | -0.93% | -340 KB |
| Level 3 | 27,286,666 bytes | 27,672,472 bytes | -1.41% | -385 KB |
| Level 9 | 13,993,517 bytes | 14,003,477 bytes | -0.07% | -9.9 KB |

**Analysis**:
- Modern Thrift is consistently smaller
- Absolute savings range from 9.9 KB to 385 KB
- At high compression (level 9), difference is negligible

---

## Format Specification

### Wire Format

```
[2-byte magic][CompactProtocol serialized data]
```

**Magic Bytes**: `{0x82, 0x21}`
- `0x82` = CompactProtocol start byte
- `0x21` = First field ID in metadata structure

### Thrift Schema

**Location**: `thrift/metadata_modern.thrift`

**Key Structures**:
- `metadata_modern` - Root metadata container
- `inode_data` - File inode information
- `directory_entry_data` - Directory entries
- `chunk_data` - Data chunk descriptors
- `symlink_data` - Symbolic link targets
- `device_data` - Device file information

### Serialization Details

**Serializer**: `apache::thrift::CompactSerializer<apache::thrift::BinaryProtocolWriter>`

**Features**:
- Variable-length integer encoding
- Bit-packed field headers
- Omits default values
- Efficient for sparse data structures

### Priority in Format Detection

Modern Thrift has **priority 100** in the serializer registry:

```
Priority Order:
120 - FlatBuffers (highest, default for creation)
100 - Modern Thrift (medium-high)
 50 - Legacy Thrift (fallback, no magic bytes)
```

**Detection Logic**:
1. Check for FlatBuffers magic (`DFBF`) → FlatBuffers
2. Check for Modern Thrift magic (`0x82 0x21`) → Modern Thrift
3. No magic bytes → Legacy Thrift (fallback)

---

## Building with Modern Thrift

### Prerequisites

**vcpkg**: Required for Folly, fbthrift, jemalloc

```bash
# Install vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"
```

**DwarFS vcpkg Overlay Ports**: Custom ports for Facebook stack

```bash
# Verify overlay ports exist
ls vcpkg_ports/
# Should see: folly/ fbthrift/ wangle/ jemalloc/
```

### Build Command

```bash
cd dwarfs

cmake -B build-modern \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=$(pwd)/vcpkg_ports \
  -DWITH_TESTS=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON

ninja -C build-modern
```

**Build Time**: 45-60 minutes (first build, includes full Facebook stack).

### Dependencies Installed (via vcpkg)

Modern Thrift pulls in:
- **folly** (v2025.12.29.00) - Facebook core library
- **fbthrift** (v2025.12.29.00) - Thrift compiler + runtime
- **jemalloc** (Tebako fork) - Memory allocator
- **glog** (0.6.0) - Logging library
- **gflags** - Command-line flags
- **libevent** - Event notification
- **double-conversion** - String-number conversion
- **fizz** - TLS 1.3 library
- **mvfst** - QUIC transport
- **wangle** - Network framework

**Total dependency size**: ~500-800 MB installed.

### Validation

```bash
# Run Modern Thrift tests
cd build-modern
./modern_thrift_converter_tests    # 5/5 tests
./modern_thrift_serialization_tests # 10/10 tests

# Run full test suite
ctest -j
```

---

## Migration from Legacy Thrift

Legacy Thrift images (`.dth`) can be recompressed to Modern Thrift:

```bash
# Recompress to Modern Thrift
mkdwarfs --recompress -i legacy.dth -o modern.dtc --metadata-format=MODERN_THRIFT

# Verify conversion
dwarfsck modern.dtc

# Compare sizes
ls -lh legacy.dth modern.dtc
```

**Expected Results**:
- Modern Thrift will be 3-6% smaller than Legacy Thrift
- Both preserve identical file content (verified via checksums)
- Modern Thrift uses proper magic bytes vs Legacy Thrift's fallback detection

---

## Troubleshooting

### Build Fails: "folly not found"

**Symptom**:
```
CMake Error: Could not find a package configuration file provided by "folly"
```

**Solution**:
```bash
# Ensure vcpkg overlay ports are configured
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_PORTS=/path/to/dwarfs/vcpkg_ports
```

### Build Fails: "FBThrift::thriftcpp2 target not found"

**Symptom**:
```
CMake Error: FBThrift::thriftcpp2 target not found
```

**Solution**:
```bash
# Clean vcpkg cache and rebuild
rm -rf build/vcpkg_installed/
cmake -B build ...
ninja -C build
```

### Link Errors: jemalloc symbols

**Symptom**:
```
undefined reference to `malloc_stats_print'
```

**Solution**: Ensure jemalloc from overlay port is used (not system jemalloc):
```bash
# Verify jemalloc source
vcpkg list jemalloc
# Should show: jemalloc:arm64-osx (from overlay port)

# If system jemalloc is found instead:
export PKG_CONFIG_PATH=$VCPKG_ROOT/installed/<triplet>/lib/pkgconfig
cmake -B build ...
```

### Format Detection Fails

**Symptom**:
```
dwarfsck image.dtc
# Shows: "metadata format: legacy_thrift" (wrong!)
```

**Solution**: Verify magic bytes are present:
```bash
hexdump -C image.dtc | head -1
# Should show: 00000000  82 21 ...

# If no magic bytes (00000000  XX XX where XX != 82 21):
# Image was NOT created with Modern Thrift
# Recreate with --metadata-format=MODERN_THRIFT
```

### CMake Generator Expression Bug (CMake 4.x + vcpkg)

**Symptom**:
```
undefined reference to `$<LINK_ONLY:Threads::Threads>'
```

**Solution**: Fixed in v0.17.0 - ensure you have latest CMakeLists.txt:
```cmake
# CMakeLists.txt should contain (after line 309):
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0" AND VCPKG_BUILD)
  # Workaround for CMake 4.x generator expression bug
  ...
endif()
```

---

## Advanced Topics

### Combining with Category-Aware Compression

```bash
# Modern Thrift + FLAC for audio
mkdwarfs -i audio_library/ -o audio.dtc \
  --metadata-format=MODERN_THRIFT \
  --categorize \
  --compression=flac

# Modern Thrift + Rice++ for FITS
mkdwarfs -i astronomy_data/ -o astronomy.dtc \
  --metadata-format=MODERN_THRIFT \
  --categorize \
  --compression=ricepp
```

### Memory Limits

```bash
# Control memory during creation
mkdwarfs -i large_dataset/ -o large.dtc \
  --metadata-format=MODERN_THRIFT \
  --memory-limit=4g \
  --num-workers=4
```

### Performance Tuning

```bash
# Optimize for compression speed
mkdwarfs -i data/ -o data.dtc \
  --metadata-format=MODERN_THRIFT \
  -l 1 \
  --num-workers=8

# Optimize for compression ratio
mkdwarfs -i data/ -o data.dtc \
  --metadata-format=MODERN_THRIFT \
  -l 9 \
  --num-workers=16
```

---

## Integration Examples

### C++ API - Loading Modern Thrift Image

```cpp
#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/reader/filesystem_loader.h>

int main() {
  auto lgr = dwarfs::stream_logger::create(std::cerr);
  auto os = dwarfs::os_access::create();

  // Load Modern Thrift image
  dwarfs::reader::filesystem_load_config config;
  config.image_path = "myfs.dtc";  // Modern Thrift image
  config.cache_size = 512 << 20;   // 512 MiB
  config.num_workers = 4;

  auto fs = dwarfs::reader::filesystem_loader::load(*lgr, *os, config);

  // Format is automatically detected via magic bytes
  // No need to specify format

  return 0;
}
```

### Python - Using via CLI

```python
import subprocess

# Create Modern Thrift image
subprocess.run([
    "mkdwarfs",
    "-i", "/data",
    "-o", "data.dtc",
    "--metadata-format=MODERN_THRIFT",
    "-l", "3"
])

# Mount and use
subprocess.run(["dwarfs", "data.dtc", "/mnt/data"])
# Use files in /mnt/data
subprocess.run(["umount", "/mnt/data"])
```

---

## Comparison with Other Formats

### vs FlatBuffers

| Aspect | Modern Thrift | FlatBuffers | Winner |
|--------|---------------|-------------|--------|
| **Size** | 100% | 101.41% | Modern Thrift |
| **Compression Speed** | Slower (17% at L3) | Faster | FlatBuffers |
| **Extraction Speed** | 1.998s | 2.069s | ≈ Equal |
| **Dependencies** | Folly + fbthrift | Header-only | FlatBuffers |
| **Build Time** | 45-60 min | 5-10 min | FlatBuffers |
| **Portability** | Limited | Excellent | FlatBuffers |
| **Memory** | Zero-copy | Zero-copy | ≈ Equal |

**Recommendation**: Use FlatBuffers unless size difference of 1.41% justifies 45-60 min build time.

### vs Legacy Thrift

| Aspect | Modern Thrift | Legacy Thrift | Winner |
|--------|---------------|---------------|--------|
| **Size** | 100% | 103-106% | Modern Thrift |
| **Dependencies** | Folly + fbthrift | None (hand-coded) | Legacy Thrift |
| **Build Time** | 45-60 min | <1 min | Legacy Thrift |
| **Format** | CompactProtocol | Frozen2 | Modern Thrift |
| **Magic Bytes** | Yes (`0x82 0x21`) | No (fallback) | Modern Thrift |
| **Compatibility** | v0.17.0+ | v0.14.1+ | Legacy Thrift |

**Recommendation**: Use Modern Thrift if you need Facebook stack; use Legacy Thrift for backward compatibility without dependencies.

---

## Frequently Asked Questions

### Q: Can I read Modern Thrift images without building with Thrift support?

**A**: No. Modern Thrift images require `DWARFS_WITH_THRIFT=ON` to read. However:
- FlatBuffers-only builds can still use `dwarfsck --list-files` on Modern Thrift images
- You can convert Modern Thrift → FlatBuffers using a both-formats build

### Q: What's the difference between Modern Thrift and Legacy Thrift?

**A**: Two completely different implementations:

**Modern Thrift**:
- Uses apache::thrift::CompactSerializer from fbthrift v2025.12.29.00
- Requires Folly + fbthrift dependencies
- Has magic bytes `{0x82, 0x21}` for detection
- Priority 100

**Legacy Thrift**:
- Hand-coded Frozen2 bit-packing implementation
- No external dependencies (2,500 lines of custom code)
- No magic bytes (fallback detection)
- Priority 50

Both produce Thrift-compatible output but use different serialization methods.

### Q: Should I use Modern Thrift or FlatBuffers for new projects?

**A**: Use **FlatBuffers** unless:
- You already have Folly/fbthrift infrastructure
- You need absolute minimum size (1.41% savings matters)
- You're comfortable with complex build dependencies

FlatBuffers provides 98.6% of Modern Thrift's efficiency with 10× simpler build process.

### Q: Can Modern Thrift images be read by upstream dwarfs?

**A**: Not yet. Modern Thrift is specific to this Tebako fork (v0.17.0+). Upstream dwarfs only supports:
- FlatBuffers (if built with `--with-flatbuffers`)
- Legacy fbthrift (original format)

### Q: What happens if I use `.dff` extension with Modern Thrift?

**A**: Extension is just a hint - format is detected via magic bytes:
```bash
# This works fine (format detected correctly)
mkdwarfs -i data/ -o data.dff --metadata-format=MODERN_THRIFT

# But you'll get a warning:
# Warning: Extension .dff doesn't match Modern Thrift format
# Recommendation: use .dtc extension
```

---

## See Also

- **vcpkg Integration**: [`vcpkg-integration.md`](vcpkg-integration.md) - Building with vcpkg
- **Performance Report**: [`DWARFS_METADATA_FORMAT_PERFORMANCE.md`](DWARFS_METADATA_FORMAT_PERFORMANCE.md) - Detailed benchmarks
- **Architecture**: [`MODERN_THRIFT_ARCHITECTURE.md`](MODERN_THRIFT_ARCHITECTURE.md) - Technical design (680+ lines)
- **Session History**: [`old-docs/sessions/`](../old-docs/sessions/) - Development history (Sessions 86-94)

---

**Created**: 2026-01-06
**Status**: Production-ready
**Validated**: Session 94 (15/15 tests PASSED)