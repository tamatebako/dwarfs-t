# Session 21: Static Site Webserver - START HERE

**Created**: 2025-12-20
**Type**: Implementation
**Time**: 2-3 hours

---

## What We're Building

C++ HTTP server serving Aesop's Fables from DwarFS archive.

**Tech Stack**: libmicrohttpd + libdwarfs + Boost.Program_options

---

## MUST READ

1. This prompt (you're here!)
2. [`SESSION_21_LIBDWARFS_EXAMPLES_PLAN.md`](SESSION_21_LIBDWARFS_EXAMPLES_PLAN.md) - Complete plan
3. [`SESSION_21_IMPLEMENTATION_STATUS.md`](SESSION_21_IMPLEMENTATION_STATUS.md) - Task tracker
4. [`LIBDWARFS_INTEGRATION_GUIDE.md`](LIBDWARFS_INTEGRATION_GUIDE.md) - API guide

**Skim**: [`example/example.cpp`](../example/example.cpp), [`benchmarks/libdwarfs/single_file_bench.cpp`](../benchmarks/libdwarfs/single_file_bench.cpp)

---

## Quick Start Checklist

### Phase 1: DwarFS Wrapper (45 min)
- [ ] Create `example/static-site-server/dwarfs_loader.h`
- [ ] Create `example/static-site-server/dwarfs_loader.cpp`
- [ ] Test standalone compilation

### Phase 2: HTTP Server (45 min)
- [ ] Create `example/static-site-server/http_server.h`
- [ ] Create `example/static-site-server/http_server.cpp`
- [ ] Test with curl

### Phase 3: CLI & Build (30 min)
- [ ] Create `example/static-site-server/main.cpp`
- [ ] Create `example/static-site-server/CMakeLists.txt`
- [ ] Test end-to-end

### Phase 4: Test Data (30 min)
- [ ] Build `aesop.dff` from [`pg11339-h/`](../example/pg11339-h/)
- [ ] Create `build.sh` script
- [ ] Create `test.sh` script

### Phase 5: Documentation (30 min)
- [ ] Create example `README.md`
- [ ] Update main [`README.md`](../README.md)
- [ ] Update [`LIBDWARFS_INTEGRATION_GUIDE.md`](LIBDWARFS_INTEGRATION_GUIDE.md)

---

## Key Implementation Patterns

### Load Filesystem
```cpp
auto lgr = dwarfs::stream_logger::create(std::cerr);
auto os = dwarfs::os_access::create();

dwarfs::reader::filesystem_load_config config;
config.image_path = "aesop.dff";
config.cache_size = 128 << 20;

auto fs = dwarfs::reader::filesystem_loader::load(*lgr, *os, config);
```

### Read File
```cpp
auto entry = fs->find("/index.html");
if (!entry) return std::nullopt;

std::error_code ec;
auto inode = fs->open(entry->inode(), ec);
auto content = fs->read_string(inode, ec);
```

### HTTP Response
```cpp
auto response = MHD_create_response_from_buffer(
  content.size(),
  const_cast<char*>(content.data()),
  MHD_RESPMEM_MUST_COPY
);

MHD_add_response_header(response, "Content-Type", "text/html");
```

---

## Expected Output

```bash
$ ./build/static-site-server --image aesop.dff
Loading DwarFS image: aesop.dff (141 files, 1.2 MB)
Server started: http://127.0.0.1:8080
Press Ctrl+C to stop

GET / -> 200 (redirect to /index.html)
GET /index.html -> 200 (48 KB)
GET /images/01hare.jpg -> 200 (23 KB)
```

---

## Success Criteria

- [ ] Compiles without warnings
- [ ] Serves site in web browser
- [ ] Images load correctly
- [ ] 404 pages work
- [ ] Clean Ctrl+C shutdown
- [ ] No memory leaks
- [ ] Comprehensive README

---

## Start Implementation

1. Read [`SESSION_21_LIBDWARFS_EXAMPLES_PLAN.md`](SESSION_21_LIBDWARFS_EXAMPLES_PLAN.md)
2. Begin with Phase 1 (DwarFS wrapper)
3. Test each phase before proceeding
4. Update status tracker as you go

**Focus**: Quality over speed - one excellent example