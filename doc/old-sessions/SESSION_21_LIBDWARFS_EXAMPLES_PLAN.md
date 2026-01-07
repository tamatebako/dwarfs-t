# Session 21: Static Site Webserver Example

**Created**: 2025-12-20
**Priority**: HIGH (Practical developer example)
**Estimated Time**: 2-3 hours
**Type**: Single focused example

---

## Objective

Create **one complete, production-ready example** demonstrating libdwarfs C++ API integration: a static site webserver serving Aesop's Fables from a DwarFS archive.

---

## The Example: Static Site Webserver

### What We're Building

A C++ HTTP server that:
- Loads a DwarFS image containing a static website (Aesop's Fables from [`example/pg11339-h/`](../example/pg11339-h/))
- Serves HTML, images, and other assets via HTTP
- Uses libmicrohttpd for HTTP protocol
- Uses libdwarfs reader API for file access
- Starts immediately (no extraction needed)

### Why This Example

**Demonstrates**:
- Real-world libdwarfs API usage
- Integration with external C library (libmicrohttpd)
- MIME type handling, error handling, clean architecture
- CMake integration with multiple dependencies

**Use Cases**:
- Embedded documentation servers
- Offline help systems bundled with apps
- Read-only web content for embedded devices
- Single-file static site distribution

---

## Architecture

```
StaticSiteServer Application
│
├─ DwarFSLoader (filesystem wrapper)
│  └─ libdwarfs reader API
│     └─ aesop.dff (compressed HTML + images)
│
└─ HTTPServer (protocol handler)
   └─ libmicrohttpd
      └─ HTTP GET requests → DwarFSLoader → responses
```

### Key Classes

1. **`DwarFSLoader`**: Wraps libdwarfs API
   - `get_file(path)` → optional content
   - `file_exists(path)` → bool
   - MIME type detection

2. **`HTTPServer`**: Wraps libmicrohttpd
   - HTTP request handling
   - 404 error pages
   - MIME type headers

3. **`main.cpp`**: CLI with Boost.Program_options
   - Parse `--image`, `--port`, `--cache` args
   - Signal handling for clean shutdown

---

## Implementation Plan

### Phase 1: DwarFS Wrapper (45 min)

**Files**: `dwarfs_loader.{h,cpp}`

**Tasks**:
1. Design class API (constructor, get_file, file_exists)
2. Implement using [`filesystem_loader`](../include/dwarfs/reader/filesystem_loader.h)
3. Add MIME type detection by extension
4. Error handling (exceptions vs optional)

**Deliverable**: Working class that reads files from DwarFS image

### Phase 2: HTTP Server (45 min)

**Files**: `http_server.{h,cpp}`

**Tasks**:
1. Design HTTPServer class wrapping libmicrohttpd
2. Implement request callback (static → member function)
3. Handle `/` → `/index.html` redirect
4. Generate 404 responses
5. Set HTTP headers (Content-Type, Content-Length)

**Deliverable**: HTTP server that serves files from DwarFSLoader

### Phase 3: CLI & Integration (30 min)

**Files**: `main.cpp`, `CMakeLists.txt`

**Tasks**:
1. Implement main() with argument parsing
2. Setup signal handlers (SIGINT, SIGTERM)
3. Create CMakeLists.txt with both dependencies
4. Add install target

**Deliverable**: Complete executable with CLI

### Phase 4: Test Data & Scripts (30 min)

**Files**: `build.sh`, `test.sh`, `aesop.dff`

**Tasks**:
1. Create DwarFS image: `mkdwarfs -i example/pg11339-h -o example/static-site-server/aesop.dff`
2. Write build script (create image + compile)
3. Write test script (start server, curl tests, stop)
4. Verify everything works

**Deliverable**: Automated build and test

### Phase 5: Documentation (30 min)

**Files**: `README.md`, update main docs

**Tasks**:
1. Write comprehensive example README
2. Update main [`README.md`](../README.md) with example section
3. Update [`LIBDWARFS_INTEGRATION_GUIDE.md`](LIBDWARFS_INTEGRATION_GUIDE.md)

**Deliverable**: Complete documentation

---

## File Structure

```
example/static-site-server/
├── CMakeLists.txt              # Build config with both deps
├── README.md                   # Complete docs
├── main.cpp                    # CLI entry point
├── dwarfs_loader.h             # DwarFS API wrapper
├── dwarfs_loader.cpp
├── http_server.h               # libmicrohttpd wrapper
├── http_server.cpp
├── build.sh                    # Build automation
├── test.sh                     # Test automation
└── aesop.dff                   # Pre-built DwarFS image

Source data: example/pg11339-h/ (141 files, ~3.8 MB HTML + images)
```

---

## Dependencies

**Required**:
- libdwarfs (from main build)
- libmicrohttpd ≥0.9.70
  - Ubuntu: `sudo apt install libmicrohttpd-dev`
  - macOS: `brew install libmicrohttpd`
- Boost (already required by DwarFS)

---

## Success Criteria

### Functionality ✅
- [ ] Compiles without warnings
- [ ] Serves HTML correctly (view in browser)
- [ ] Serves images correctly
- [ ] Returns 404 for missing files
- [ ] Handles concurrent requests
- [ ] Clean shutdown on Ctrl+C

### Code Quality ✅
- [ ] Clean OOP (RAII, smart pointers, SRP)
- [ ] Comprehensive error handling
- [ ] Well-documented (Doxygen-style)
- [ ] No memory leaks (valgrind)

### Documentation ✅
- [ ] README explains architecture
- [ ] Build instructions work
- [ ] Usage examples accurate
- [ ] Main docs updated

---

## Expected CLI Usage

```bash
# Build everything
cd example/static-site-server
./build.sh

# Run server
./build/static-site-server --image aesop.dff

# Custom options
./build/static-site-server --image aesop.dff --port 9090 --cache 256 --verbose

# Test
./test.sh

# Browse
open http://localhost:8080/
```

---

## Performance Expectations

Based on [`LIBDWARFS_API_PERFORMANCE.md`](LIBDWARFS_API_PERFORMANCE.md):

- **Latency**: <1ms warm cache, 5-10ms cold cache
- **Throughput**: 20-30 MB/s
- **Memory**: ~10-20 MiB with 128 MiB cache
- **Concurrent**: Handles 10+ connections easily

---

## Next Steps

### Implementation Session

1. Read this plan
2. Read [`LIBDWARFS_INTEGRATION_GUIDE.md`](LIBDWARFS_INTEGRATION_GUIDE.md)
3. Review [`example/example.cpp`](../example/example.cpp)
4. Implement Phase 1 (DwarFS wrapper)
5. Implement Phase 2 (HTTP server)
6. Complete remaining phases
7. Test thoroughly
8. Update documentation

### Definition of Done

All checkboxes above checked ✅

---

**Status**: Plan Complete
**Next**: Begin implementation with Phase 1