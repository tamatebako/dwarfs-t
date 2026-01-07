<!-- vim:set ts=2 sw=2 sts=2 et: -->
# libdwarfs Integration Guide

**Created**: 2025-12-19  
**Version**: 0.16.0+  
**Purpose**: Complete guide to integrating libdwarfs C++ API in your applications

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [API Overview](#api-overview)
3. [Basic Usage](#basic-usage)
4. [Advanced Topics](#advanced-topics)
5. [Performance Tuning](#performance-tuning)
6. [Best Practices](#best-practices)
7. [Troubleshooting](#troubleshooting)

---

## Quick Start

### Minimal Example

```cpp
#include <iostream>
#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/reader/filesystem_loader.h>

int main() {
  // Setup logger and OS access
  auto lgr = dwarfs::stream_logger::create(std::cerr);
  auto os = dwarfs::os_access::create();

  // Load DwarFS image
  dwarfs::reader::filesystem_load_config config;
  config.image_path = "myfs.dff";
  config.cache_size = 512 << 20;  // 512 MiB

  auto fs = dwarfs::reader::filesystem_loader::load(*lgr, *os, config);

  // Find and read a file
  auto entry = fs.find("/path/to/file.txt");
  if (entry) {
    auto inode = fs.open(entry->inode());
    auto content = fs.read_string(inode);
    std::cout << content << std::endl;
  }

  return 0;
}
```

### CMake Integration

```cmake
find_package(dwarfs REQUIRED CONFIG)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE dwarfs::dwarfs_reader)
```

---

## API Overview

### Core Components

**[`filesystem_loader`](../include/dwarfs/reader/filesystem_loader.h)**: High-level loading API
- Simplified configuration
- Auto-detection of metadata format
- Portable across platforms

**[`filesystem_v2_lite`](../include/dwarfs/reader/filesystem_v2.h)**: Lightweight filesystem interface
- File/directory operations
- Memory-efficient
- Suitable for most use cases

**[`filesystem_v2`](../include/dwarfs/reader/filesystem_v2.h)**: Full filesystem interface
- Extends `filesystem_v2_lite`
- Integrity checking
- JSON export capabilities

### Key Types

**[`dir_entry_view`](../include/dwarfs/reader/metadata_types.h)**: Directory entry reference
- Lightweight, copyable
- Points to metadata

**[`inode_view`](../include/dwarfs/reader/metadata_types.h)**: Inode reference
- File attributes
- Metadata accessor

**[`file_stat`](../include/dwarfs/file_stat.h)**: File statistics
- Compatible with POSIX stat
- Cross-platform

---

## Basic Usage

### 1. Loading a Filesystem

```cpp
#include <dwarfs/logger.h>
#include <dwarfs/os_access.h>
#include <dwarfs/reader/filesystem_loader.h>

using namespace dwarfs;

// Create logger (stream, file, or null)
logger_options log_opts;
log_opts.threshold = logger::INFO;
auto lgr = stream_logger::create(std::cerr, log_opts);

// Create OS access layer
auto os = os_access::create();

// Configure filesystem loader
reader::filesystem_load_config config;
config.image_path = "/path/to/image.dff";
config.cache_size = 1024 << 20;  // 1 GiB cache
config.num_workers = 4;          // 4 decompression threads

// Load filesystem
auto fs = reader::filesystem_loader::load(*lgr, *os, config);
```

### 2. Finding Files

```cpp
// Find by absolute path
auto entry_opt = fs.find("/usr/bin/perl");
if (!entry_opt) {
  std::cerr << "File not found\n";
  return 1;
}

auto entry = *entry_opt;

// Get inode
auto inode = entry.inode();

// Get file attributes
auto stat = fs.getattr(inode);
std::cout << "Size: " << stat.size << " bytes\n";
std::cout << "Mode: " << std::oct << stat.mode << std::dec << "\n";
```

### 3. Reading Files

```cpp
std::error_code ec;

// Open file
auto inode_num = fs.open(inode, ec);
if (ec) {
  std::cerr << "Open failed: " << ec.message() << "\n";
  return 1;
}

// Read entire file as string
auto content = fs.read_string(inode_num, ec);
if (ec) {
  std::cerr << "Read failed: " << ec.message() << "\n";
  return 1;
}

// Or read into buffer
std::vector<char> buffer(stat.size);
size_t bytes_read = fs.read(inode_num, buffer.data(), buffer.size(), 0, ec);
```

### 4. Directory Traversal

```cpp
// Walk entire filesystem
fs.walk([](reader::dir_entry_view entry) {
  std::cout << entry.path() << "\n";
});

// Walk specific directory
auto dir_entry = fs.find("/usr/bin");
if (dir_entry) {
  auto dir = fs.opendir(dir_entry->inode());
  if (dir) {
    size_t offset = 0;
    while (auto child = fs.readdir(*dir, offset++)) {
      std::cout << child->name() << "\n";
    }
  }
}
```

### 5. Extracting to Disk

```cpp
#include <fstream>
#include <filesystem>

void extract_file(reader::filesystem_v2_lite const& fs,
                  std::string const& path,
                  std::filesystem::path const& output_dir) {
  auto entry_opt = fs.find(path);
  if (!entry_opt) return;

  auto inode = entry_opt->inode();
  auto stat = fs.getattr(inode);

  if (!stat.is_regular_file()) return;

  // Read file
  std::error_code ec;
  auto inode_num = fs.open(inode, ec);
  auto content = fs.read_string(inode_num, ec);

  // Write to disk
  auto output_path = output_dir / std::filesystem::path(path).filename();
  std::ofstream out(output_path, std::ios::binary);
  out.write(content.data(), content.size());
}
```

---

## Advanced Topics

### Random Access Reading

```cpp
// Read specific range
size_t offset = 1024;
size_t size = 4096;
std::vector<char> buffer(size);

size_t bytes_read = fs.read(inode_num, buffer.data(), size, offset, ec);
```

### Multi-Threaded Extraction

```cpp
#include <thread>
#include <atomic>
#include <vector>

void extract_parallel(reader::filesystem_v2_lite const& fs,
                      std::vector<std::string> const& files,
                      size_t num_threads) {
  std::atomic<size_t> next_index{0};

  auto worker = [&]() {
    while (true) {
      size_t index = next_index.fetch_add(1);
      if (index >= files.size()) break;
      
      extract_file(fs, files[index], "/output");
    }
  };

  std::vector<std::thread> threads;
  for (size_t i = 0; i < num_threads; ++i) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }
}
```

### Error Handling

```cpp
#include <system_error>

std::error_code ec;

// All operations have error code overloads
auto content = fs.read_string(inode_num, ec);
if (ec) {
  if (ec == std::errc::no_such_file_or_directory) {
    // Handle missing file
  } else if (ec == std::errc::permission_denied) {
    // Handle permission error
  } else {
    // Other error
    std::cerr << "Error: " << ec.message() << "\n";
  }
}
```

### Memory Management

```cpp
// filesystem_v2_lite is movable but not copyable
auto fs1 = reader::filesystem_loader::load(*lgr, *os, config);

// Move ownership
auto fs2 = std::move(fs1);  // OK
// auto fs3 = fs1;          // Error: deleted copy constructor

// Store in container
std::vector<reader::filesystem_v2_lite> filesystems;
filesystems.push_back(std::move(fs2));

// Use smart pointers for shared ownership
auto fs_ptr = std::make_unique<reader::filesystem_v2_lite>(
    reader::filesystem_loader::load(*lgr, *os, config)
);
```

---

## Performance Tuning

### Cache Configuration

```cpp
reader::filesystem_load_config config;

// Cache size affects random access performance
config.cache_size = 2048 << 20;  // 2 GiB for large working sets

// Number of decompression workers
config.num_workers = std::thread::hardware_concurrency();

// Block allocator (malloc/jemalloc/mimalloc)
config.block_allocator = reader::block_cache_allocation_mode::MALLOC;
```

### Sequential Access Optimization

```cpp
// Enable readahead for sequential access
config.readahead = 16;  // Prefetch 16 blocks ahead

// Sequential detector threshold
config.seq_detector_threshold = 4;  // Trigger after 4 sequential accesses
```

### Memory Locking (Linux)

```cpp
#include <dwarfs/reader/mlock_mode.h>

// Lock metadata in RAM (requires CAP_IPC_LOCK on Linux)
config.lock_mode = reader::mlock_mode::TRY;  // Try without failing
```

### Benchmark Your Access Pattern

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();

// Your operation here
for (auto const& file : files) {
  extract_file(fs, file, output_dir);
}

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration<double>(end - start).count();

std::cout << "Extracted " << files.size() << " files in "
          << duration << " seconds\n";
std::cout << "Throughput: " << (files.size() / duration) << " files/s\n";
```

---

## Best Practices

### 1. Reuse Filesystem Objects

**❌ Don't**: Load filesystem for each operation

```cpp
for (auto const& path : paths) {
  auto fs = reader::filesystem_loader::load(*lgr, *os, config);  // Expensive!
  extract_file(fs, path, output_dir);
}
```

**✅ Do**: Load once, reuse

```cpp
auto fs = reader::filesystem_loader::load(*lgr, *os, config);  // Once
for (auto const& path : paths) {
  extract_file(fs, path, output_dir);  // Fast
}
```

### 2. Check File Types Before Reading

**❌ Don't**: Assume all entries are files

```cpp
auto entry = fs.find(path);
auto inode = fs.open(entry->inode());  // May fail if directory/symlink
```

**✅ Do**: Check type first

```cpp
auto entry = fs.find(path);
if (!entry) return;

auto stat = fs.getattr(entry->inode());
if (!stat.is_regular_file()) return;  // Skip non-files

auto inode = fs.open(entry->inode());
```

### 3. Use Error Code Overloads in Production

**❌ Don't**: Let exceptions propagate

```cpp
auto content = fs.read_string(inode);  // May throw
```

**✅ Do**: Handle errors explicitly

```cpp
std::error_code ec;
auto content = fs.read_string(inode, ec);
if (ec) {
  // Handle error gracefully
  return;
}
```

### 4. Configure Logger Appropriately

**Development**:
```cpp
logger_options log_opts;
log_opts.threshold = logger::DEBUG;  // Verbose output
auto lgr = stream_logger::create(std::cerr, log_opts);
```

**Production**:
```cpp
logger_options log_opts;
log_opts.threshold = logger::WARN;  // Errors only
auto lgr = stream_logger::create(logfile, log_opts);
```

### 5. Profile Before Optimizing

```cpp
#include <dwarfs/performance_monitor.h>

auto perfmon = std::make_shared<performance_monitor>();

reader::filesystem_load_config config;
// ... configure ...

auto fs = reader::filesystem_loader::load(*lgr, *os, config, perfmon);

// After operations, check metrics
perfmon->dump(std::cout);
```

---

## Troubleshooting

### Build Errors

**Problem**: `dwarfs/reader/filesystem_loader.h: No such file or directory`

**Solution**: Ensure dwarfs is installed and `find_package(dwarfs)` succeeds:
```cmake
find_package(dwarfs REQUIRED CONFIG)
include_directories(${dwarfs_INCLUDE_DIRS})
```

**Problem**: Undefined reference to `dwarfs::reader::filesystem_loader::load`

**Solution**: Link against dwarfs_reader:
```cmake
target_link_libraries(myapp PRIVATE dwarfs::dwarfs_reader)
```

### Runtime Errors

**Problem**: `Failed to open image: No such file or directory`

**Solution**: Check image path and permissions:
```cpp
if (!std::filesystem::exists(config.image_path)) {
  std::cerr << "Image not found: " << config.image_path << "\n";
}
```

**Problem**: `Bad alloc` or out-of-memory errors

**Solution**: Reduce cache size:
```cpp
config.cache_size = 256 << 20;  // Start with 256 MiB
```

**Problem**: Slow performance

**Solution**: Increase workers and cache:
```cpp
config.num_workers = std::thread::hardware_concurrency();
config.cache_size = 1024 << 20;  // 1 GiB
config.readahead = 16;  // Enable prefetching
```

### Common Mistakes

**Mistake**: Not checking for null optionals

```cpp
auto entry = fs.find("/missing");
auto inode = entry->inode();  // CRASH if not found
```

**Fix**:
```cpp
auto entry_opt = fs.find("/missing");
if (!entry_opt) {
  // Handle not found
  return;
}
auto inode = entry_opt->inode();  // Safe
```

**Mistake**: Ignoring error codes

```cpp
std::error_code ec;
auto content = fs.read_string(inode, ec);
// Use content without checking ec
```

**Fix**:
```cpp
std::error_code ec;
auto content = fs.read_string(inode, ec);
if (ec) {
  std::cerr << "Read failed: " << ec.message() << "\n";
  return;
}
// Now safe to use content
```

---

## Additional Resources

- **API Reference**: [`include/dwarfs/reader/`](../include/dwarfs/reader/)
- **Examples**: [`benchmarks/libdwarfs/`](../benchmarks/libdwarfs/)
- **Performance Report**: [`doc/LIBDWARFS_API_PERFORMANCE.md`](LIBDWARFS_API_PERFORMANCE.md)
- **Architecture**: [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md)

---

**Last Updated**: 2025-12-19  
**Version**: 0.16.0+  
**Status**: Production-Ready