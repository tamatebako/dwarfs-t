# DwarFS Static Site Server

**Purpose**: Demonstration of libdwarfs C++ API integration with libmicrohttpd to serve static web content from a DwarFS archive.

**Use Cases**:
- Embedded documentation servers in applications
- Offline help systems bundled with executables
- Read-only web content for embedded devices
- Single-file static site distribution
- Development/testing servers with instant startup

---

## Architecture

```
┌─────────────────────────────────────┐
│   Static Site Server Application    │
├─────────────────────────────────────┤
│                                     │
│  ┌──────────────────────────────┐  │
│  │   HTTP Server                │  │
│  │   (libmicrohttpd wrapper)    │  │
│  │                              │  │
│  │  • GET request handling      │  │
│  │  • MIME type detection       │  │
│  │  • Error responses (404)     │  │
│  │  • /ls/ file listing route   │  │
│  └──────────┬───────────────────┘  │
│             │                       │
│             ▼                       │
│  ┌──────────────────────────────┐  │
│  │   DwarFS Loader              │  │
│  │   (libdwarfs wrapper)        │  │
│  │                              │  │
│  │  • Filesystem initialization │  │
│  │  • File reading              │  │
│  │  • MIME type detection       │  │
│  │  • File listing with sizes   │  │
│  └──────────┬───────────────────┘  │
│             │                       │
│             ▼                       │
│  ┌──────────────────────────────┐  │
│  │   candide.dff                │  │
│  │   (DwarFS image)             │  │
│  │                              │  │
│  │  • Compressed HTML + images  │  │
│  │  • ~1-2 MB (from ~3.8 MB)    │  │
│  └──────────────────────────────┘  │
└─────────────────────────────────────┘
```

---

## Features

- ✅ **Instant Startup**: No extraction needed, serves directly from archive
- ✅ **Fast Access**: Memory-mapped with intelligent caching
- ✅ **Small Footprint**: Compressed archive (~50% of original size)
- ✅ **File Listing**: `/ls/` route shows all files with sizes in styled HTML table
- ✅ **Clean Architecture**: Separate concerns (loader, server, CLI)
- ✅ **Error Handling**: Proper 404 pages and error responses
- ✅ **MIME Types**: Automatic detection for HTML, images, CSS, JS
- ✅ **Signal Handling**: Clean shutdown on Ctrl+C

---

## Dependencies

All dependencies are automatically managed by vcpkg:

- **DwarFS** (built from local source via vcpkg overlay port)
- **Boost** 1.90.0+ (program_options, filesystem, iostreams, chrono, context)
- **libmicrohttpd** 1.0.2+
- **jemalloc** 5.5.0 (Tebako fork from tamatebako/jemalloc)
- **FlatBuffers**, fmt, range-v3, parallel-hashmap, and compression libraries

---

## Build Instructions

### Prerequisites

- **vcpkg** installed with `VCPKG_ROOT` environment variable set
- **CMake** ≥3.24
- **C++20 compiler** (GCC 10+, Clang 12+, AppleClang 14+)
- **Ninja** build tool (or Make)

### Quick Start

```bash
# From example/static-site-server/ directory
./build.sh --help    # Show build options
./build.sh           # Build with vcpkg
./build.sh --clean   # Clean and rebuild
```

The build script uses vcpkg with custom overlay ports to:
1. Build DwarFS from local source (via `vcpkg_ports/dwarfs/`)
2. Fetch jemalloc 5.5.0 from tamatebako/jemalloc (via `vcpkg_ports/jemalloc/`)
3. Install all other dependencies from vcpkg registry
4. Build the static-site-server executable

### Build Options

```bash
./build.sh --help     # Show detailed help
./build.sh --clean    # Remove build/ and vcpkg_installed/ directories
./build.sh            # Normal build (incremental)
```

The `--clean` option removes:
- `build/` directory
- `vcpkg_installed/` directory
- CMake cache files

This ensures a completely fresh build from scratch.

---

## Usage

### Basic Usage

```bash
# Start server with default settings (port 8080)
./build/static-site-server --image candide.dff

# Output:
# Loaded: 6 files, 853.779 KB
# Files (showing 6 of 6):
#   /19942-cover.png (55.82 KB)
#   /images/001.jpg (310.29 KB)
#   ...
# Server started: http://127.0.0.1:8080
# Browse to: http://127.0.0.1:8080/
# Press Ctrl+C to stop
```

Then visit:
- <http://localhost:8080/> - Serves main HTML file
- <http://localhost:8080/ls/> - File listing with sizes (styled HTML table)

### Command-Line Options

```bash
./build/static-site-server [OPTIONS]

Options:
  -h, --help          Show help message
  -i, --image PATH    Path to DwarFS image (required)
  -p, --port NUM      Port to bind (default: 8080)
  -b, --bind ADDR     Bind address (default: all interfaces)
  -c, --cache SIZE    Cache size in MiB (default: 128)
  -w, --workers NUM   Worker threads (default: 4)
  -v, --verbose       Enable verbose logging
```

### Examples

```bash
# Custom port
./build/static-site-server --image candide.dff --port 9090

# Increase cache for better performance
./build/static-site-server --image candide.dff --cache 512 --workers 8

# Verbose logging (shows each request)
./build/static-site-server --image candide.dff --verbose

# Bind to specific interface
./build/static-site-server --image candide.dff --bind 192.168.1.100
```

### Testing

```bash
# Automated tests
./test.sh

# Manual tests
curl http://localhost:8080/         # Main page
curl http://localhost:8080/ls/      # File listing
curl http://localhost:8080/images/001.jpg  # Image file
```

---

## Technical Details

### Components

**[`dwarfs_loader`](dwarfs_loader.h)**: Wraps libdwarfs reader API
- Loads DwarFS image on construction
- `get_file(path)` returns optional content
- `list_files()` returns all files with sizes
- MIME type detection by file extension
- Thread-safe for concurrent reads

**[`http_server`](http_server.h)**: Wraps libmicrohttpd
- Accepts HTTP GET requests
- Routes `/` to index pages (auto-detect)
- Routes `/ls/` to file listing page (HTML table)
- Returns 404 for missing files
- Sets proper Content-Type headers

**[`main.cpp`](main.cpp)**: CLI entry point
- Boost.Program_options for argument parsing
- Signal handling for clean shutdown (SIGINT/SIGTERM)
- Displays first 10 files with sizes on startup
- Creates and manages server lifetime

### File Listing Features

The server provides two levels of file information:

1. **Console Output** (startup):
   - Shows first 10 files with formatted sizes
   - Example: `/19942-cover.png (55.82 KB)`

2. **`/ls/` HTTP Route**:
   - Complete file listing in styled HTML table
   - Clickable links to each file
   - Formatted sizes (B/KB/MB with 2 decimal places)
   - Professional styling with CSS

---

## Troubleshooting

### Build Errors

**Problem**: `dwarfs not found in baseline`

**Solution**: vcpkg configuration files are already set up. Ensure you're running from `example/static-site-server/` within the dwarfs repository so overlay ports are found.

**Problem**: `jemalloc version conflict`

**Solution**: The build uses Tebako's jemalloc 5.5.0 from tamatebako/jemalloc. Run `./build.sh --clean` to force a fresh build.

### Runtime Errors

**Problem**: `Failed to load DwarFS image`

**Solution**: Check image exists and is valid:
```bash
ls -lh candide.dff
# If missing, create it first (requires dwarfs tools in PATH)
```

**Problem**: `Failed to start HTTP server on port 8080`

**Solution**: Port already in use:
```bash
./build/static-site-server --image candide.dff --port 9090
```

---

## License

GPL-3.0 - See [`LICENSE.GPL-3.0`](../../LICENSE.GPL-3.0) for details.

---

## Related Documentation

- **vcpkg Integration**: [`vcpkg-configuration.json`](vcpkg-configuration.json) and [`vcpkg.json`](vcpkg.json)
- **API Guide**: [`LIBDWARFS_INTEGRATION_GUIDE.md`](../../doc/LIBDWARFS_INTEGRATION_GUIDE.md)
- **Performance**: [`LIBDWARFS_API_PERFORMANCE.md`](../../doc/LIBDWARFS_API_PERFORMANCE.md)
- **Architecture**: [`.kilocode/rules/memory-bank/architecture.md`](../../.kilocode/rules/memory-bank/architecture.md)

---

**Created**: 2025-12-20
**Updated**: 2025-12-27 (vcpkg integration + file listing features)
**Status**: Production-ready example