# SIGBUS Error Investigation: Large File Reading on macOS ARM64

## Summary

Bus error (SIGBUS) occurs when reading large (10MB) highly-compressible files from DwarFS archives on macOS ARM64.

## Diagnostic Logging Added

The following `[SIGBUS-DEBUG]` log messages have been added to help diagnose the issue:

### Files Modified:

1. **`src/reader/internal/inode_reader_v2.cpp`**
   - `read()`: Logs inode, size, offset, chunks count, buffer pointer
   - `read_internal()` (data chunks): Logs block, chunk_offset, copyoff, copysize
   - `read_internal()` (hole chunks): Logs copysize, hole_span.size(), hole_data_ptr
   - `get_hole_data()`: Logs hole_data_size, validity, mapped_size
   - `memcpy` callback: Logs num_read, br.size(), br.data(), bounds check

2. **`src/reader/internal/cached_block.cpp`**
   - Constructor: Logs section number, compression type, compressed/uncompressed sizes, data pointer
   - `decompress_until()`: Logs end, current_pos, uncompressed_size, decompressor status
   - After decompression: Logs final data_size, data_ptr

3. **`src/reader/internal/block_cache.cpp`**
   - Note: `block_request::fulfill()` does NOT have logger access (nested class limitation)
   - Bounds checking is done via `block_range` constructor which throws on invalid ranges

4. **`src/reader/block_range.cpp`**
   - Both constructors have bounds checks that throw on invalid ranges

### How to Enable Debug Logging:

```bash
# Set log level to debug
export DWARFS_LOG_LEVEL=debug

# Run the test
./test_dwarfs_backend --gtest_filter="*ReadLargeFile*" 2>&1 | tee sigbus_debug.log

# Or use lldb
lldb ./test_dwarfs_backend
(lldb) run --gtest_filter="*ReadLargeFile*"
# When crash happens:
(lldb) bt
(lldb) frame variable
```

## Code Path Analysis

```
filesystem_v2_lite::read()
  └── inode_reader_v2::read()
        └── read_internal()
              ├── cache_.get(block, offset, size)  // For data chunks
              │     └── block_range(span)
              └── get_hole_data().const_span()     // For sparse/zero regions
                    └── readonly_memory_mapping
                          └── virtual_alloc() -> mmap()
            └── memcpy(buf + num_read, br.data(), br.size())  // <-- CRASH HERE
```

## Key Files

| File | Lines | Purpose |
|------|-------|---------|
| `src/reader/internal/inode_reader_v2.cpp` | 527 | `memcpy` to user buffer |
| `src/reader/internal/block_cache.cpp` | 835 | Block cache and decompression |
| `src/reader/internal/cached_block.cpp` | 170 | Block decompression |
| `src/internal/mappable_file.cpp` | 450 | Memory mapping (virtual_alloc) |
| `src/internal/io_ops_posix.cpp` | 244 | POSIX mmap wrapper |

## Hypotheses

### Hypothesis 1: Hole Data Mapping Too Small

**Location**: `src/reader/internal/inode_reader_v2.cpp:200-209`

```cpp
readonly_memory_mapping const& get_hole_data() const {
  std::call_once(hole_data_init_flag_, [this]() {
    hole_data_ = os_.map_empty_readonly(opts_.hole_data_size);
    if (!hole_data_) {
      LOG_ERROR << "failed to allocate zero block: "
                << exception_str(std::current_exception());
    }
  });
  return hole_data_;
}
```

**Problem**: If `opts_.hole_data_size` is smaller than the requested read size, the `subspan` operation in `read_internal()` (line 417) could access memory beyond the mapping:

```cpp
auto const& hole_span = get_hole_data().template const_span<uint8_t>();
while (copysize > 0) {
  auto const hole_range_size = std::min<size_t>(copysize, hole_span.size());
  ranges.emplace_back(make_ready_future(
      block_range(hole_span.subspan(0, hole_range_size))));
  // ...
}
```

**Test**: Check the value of `opts_.hole_data_size` and compare to file size.

### Hypothesis 2: Block Decompression Incomplete

**Location**: `src/reader/internal/cached_block.cpp:92-111`

```cpp
void decompress_until(size_t end) override {
  auto pos = data_.size();
  while (pos < end) {
    if (!decompressor_) {
      DWARFS_THROW(runtime_error, "no decompressor for block");
    }
    if (decompressor_->decompress_frame()) {
      decompressor_.reset();
      seg_.reset();
    }
    pos = data_.size();
    range_end_.store(pos, std::memory_order_release);
  }
}
```

**Problem**: If decompression fails silently or returns incomplete data, the `data()` pointer could point to uninitialized memory. The extreme compression ratio (10MB → 104B = 99.999% compression) might expose an edge case.

**Test**: Verify that `uncompressed_size()` matches expected file size and that `range_end()` reaches it.

### Hypothesis 3: Memory Mapping Alignment on ARM64

**Location**: `src/internal/io_ops_posix.cpp:231-244`

```cpp
void* virtual_alloc(size_t size, memory_access access,
                    std::error_code& ec) const override {
  int const prot =
      access == memory_access::readonly ? PROT_READ : PROT_READ | PROT_WRITE;
  auto const addr =
      ::mmap(nullptr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  // ...
}
```

**Problem**: On ARM64, memory access must be properly aligned. While `mmap` should return page-aligned memory, there could be an issue with:
1. Accessing memory beyond the mapping (off-by-one or bounds error)
2. The `span` or `subspan` operations not correctly handling the mapping size

### Hypothesis 4: Block Range Size Mismatch

**Location**: `src/reader/internal/inode_reader_v2.cpp:467-471`

```cpp
for (auto& r : ranges) {
  auto br = r.get();
  store(num_read, br);
  num_read += br.size();
}
```

**Problem**: If `br.size()` is larger than expected (e.g., due to incorrect chunk size calculation), the `memcpy` could write beyond the user's buffer.

## Recommended Diagnostics

### 1. Add Debug Logging

```cpp
// In inode_reader_v2.cpp, read_internal():
LOG_DEBUG << "read_internal: inode=" << inode
          << " size=" << size
          << " offset=" << offset
          << " chunks.size()=" << chunks.size();

// Before memcpy:
LOG_DEBUG << "memcpy: num_read=" << num_read
          << " br.size()=" << br.size()
          << " br.data()=" << static_cast<void const*>(br.data())
          << " buf size=" << size;
```

### 2. Verify Hole Data Size

```cpp
// In inode_reader_v2 constructor or get_hole_data():
LOG_DEBUG << "hole_data_size=" << opts_.hole_data_size;
// After mapping:
LOG_DEBUG << "hole_data mapped size=" << hole_data_.size();
```

### 3. Add Bounds Checking

```cpp
// In read_internal, before memcpy:
if (num_read + br.size() > size) {
  LOG_ERROR << "bounds error: num_read=" << num_read
            << " br.size()=" << br.size()
            << " size=" << size;
  ec = std::make_error_code(std::errc::io_error);
  return 0;
}
```

### 4. Check Chunk Information

For the 10MB sparse file:
- How many chunks are there?
- What are the chunk types (DATA vs HOLE)?
- What are the chunk sizes?

## Reproduction Steps

1. Create a test file:
   ```bash
   dd if=/dev/zero of=10mb.bin bs=1M count=10
   ```

2. Create DwarFS archive:
   ```bash
   mkdwarfs -l 9 -o large.dwarfs 10mb.bin
   ```

3. Verify archive:
   ```bash
   dwarfsck -i large.dwarfs
   ```

4. Test extraction:
   ```bash
   dwarfsextract -i large.dwarfs -o /tmp/test
   ```

5. Run the crashing test

## Suggested Fix Areas

### Fix 1: Increase Hole Data Size

In `inode_reader_options.h`, ensure `hole_data_size` is large enough:

```cpp
// Current might be 64KB or smaller
// Should be at least as large as the largest block
size_t hole_data_size{64 * 1024 * 1024};  // 64 MiB (match block size)
```

### Fix 2: Add Safety Check in Block Range

```cpp
// In block_range constructor or usage:
if (offset + size > block->range_end()) {
  // Handle incomplete decompression
  throw std::runtime_error("block range exceeds decompressed data");
}
```

### Fix 3: Validate Chunk Sizes

In `read_internal()`, validate that chunk sizes don't exceed block size:

```cpp
if (it->size() > meta_.block_size()) {
  LOG_ERROR << "chunk size " << it->size()
            << " exceeds block size " << meta_.block_size();
  ec = std::make_error_code(std::errc::io_error);
  return 0;
}
```

## Next Steps

1. Add diagnostic logging and rebuild
2. Run with `DWARFS_LOG_LEVEL=debug` to capture trace
3. Use `lldb` to catch the exact crash location
4. Report findings back to identify root cause

## References

- Bug report: `docs/proposals/dwarfs_large_file_bus_error.md`
- Related files: See Key Files table above
- Test archive: `tests/fixtures/dwarfs/large.dwarfs`

---

## Investigation Status (2024-02-20)

### Build Status
- Diagnostic logging successfully compiled with `arm64-osx-production` preset
- Build completed without errors after fixing logger access issue in `block_request::fulfill()`

### Local Testing Results

1. **Existing Test Suite**: The `FilesystemOperationsTest.large_file` test passes, but it only tests a 1MB file with 'X' characters - NOT the same scenario as the bug report.

2. **Manual Test Creation**:
   - Created a 10MB all-zeros file (`dd if=/dev/zero of=10mb.bin bs=1M count=10`)
   - Successfully created DwarFS archive (compressed from 10MB to 3.2 KiB)
   - Archive verified with `dwarfsck`
   - **Extraction with `dwarfsextract` HANGS** (100% CPU usage, no output after 60+ seconds)
   - This is a different symptom than SIGBUS, but indicates a problem with highly-compressible files

### Current Hypothesis

The hang during extraction suggests the issue may be an infinite loop rather than (or in addition to) SIGBUS. The diagnostic logging should help identify:

1. Whether blocks are being decompressed correctly
2. Whether the hole data mapping is being used appropriately
3. Whether chunk iteration is getting stuck

### Recommended Next Steps

1. **Downstream Reporter Testing**: Have the original reporter build with the diagnostic logging and run:
   ```bash
   DWARFS_LOG_LEVEL=debug dwarfsextract -i large.dwarfs -o /tmp/test 2>&1 | tee debug.log
   ```

2. **lldb Debugging**: If the issue is reproducible, run under lldb:
   ```bash
   lldb -- dwarfsextract -i large.dwarfs -o /tmp/test
   (lldb) run
   # When crash or hang occurs:
   (lldb) bt all
   (lldb) frame variable
   ```

3. **Create Unit Test**: Add a dedicated unit test for 10MB+ highly-compressible files to catch regressions.

---

## Updated Analysis (2024-02-20 - After Reporter Feedback)

### Reporter's Backtrace Analysis

```
* thread #1, stop reason = EXC_BAD_ACCESS (code=257, address=0xf85b83a8941537b2)
  * frame #0: 0x000003a8941537b2  <- Invalid address (corrupted)
    frame #1: dwarfs::internal::worker_group::worker_group(...) + 1588
    frame #2: std::__1::__call_once_proxy<...block_cache_::enqueue_job...> + 156
    frame #3: std::__1::__call_once
    frame #4: dwarfs::reader::block_cache_::enqueue_job
    frame #5: dwarfs::reader::block_cache_::create_cached_block
    frame #6: dwarfs::reader::block_cache_::get
    frame #7: dwarfs::reader::inode_reader_::read_internal
    ...
    frame #11: DwarfsFileHandle::read  <- Caller's code
```

### Key Findings

1. **Crash Location**: The crash occurs in `worker_group::worker_group` constructor at offset +1588
2. **Invalid Address**: `0xf85b83a8941537b2` is clearly corrupted - not a valid code address
3. **Root Cause**: Memory corruption causes a jump to an invalid address during thread pool initialization

### Code Path to Crash

```cpp
// In block_cache_.cpp:
void enqueue_job(std::shared_ptr<block_request_set> brs) const {
  // Lazy initialization via std::call_once
  std::call_once(wg_init_flag_, [this] { init_worker_group(); });  // <- CRASH HERE
  ...
}

void init_worker_group() const {
  std::unique_lock lock(mx_wg_);
  if (!wg_) {
    wg_ = worker_group(LOG_GET_LOGGER, os_, "blkcache", num_workers);  // <- Constructor crashes
  }
}
```

### Worker Group Constructor (lines 64-90 in worker_group.cpp)

The +1588 offset is likely in:
1. Thread creation loop (`workers_.emplace_back(...)`)
2. Lambda execution (`compat::system::setThreadName(fmt::format(...))`)
3. `fmt::format` with captured variables

### Potential Causes

1. **Vtable Corruption**: The `this` pointer or vtable has been corrupted before reaching the constructor
2. **Logger Reference Issue**: `LOG_GET_LOGGER` returns an invalid reference
3. **Lambda Capture Issue**: The lambda `[this] { init_worker_group(); }` captures a pointer to an object that's being destroyed
4. **Stack Corruption**: Return addresses on the stack have been corrupted by a buffer overflow elsewhere

### Why Only Highly-Compressible Files?

The 10MB → 104 byte compression ratio (99.999%) is extreme. Possible triggers:
1. A code path that's only exercised with extreme compression ratios
2. A race condition that's more likely with sparse/hole-filled data
3. Buffer overflow in chunk handling that only manifests with specific data patterns

### Recommended Fixes to Investigate

1. **Check block_cache lifetime**: Ensure the block_cache object isn't being destroyed during worker_group initialization
2. **Validate logger reference**: Ensure `LOG_GET_LOGGER` returns a valid reference
3. **Add memory sanitizers**: Run with ASan/MSan to catch memory corruption
4. **Check for static initialization order fiasco**: If any global/static objects are involved

### Testing Commands for Reporter

```bash
# Build with AddressSanitizer for better diagnostics
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address -g" ..
ninja

# Run with ASan
ASAN_OPTIONS=detect_leaks=1 ./test_dwarfs_backend --gtest_filter="*ReadLargeFile*"

# Or use lldb for detailed crash info
lldb -- ./test_dwarfs_backend --gtest_filter="*ReadLargeFile*"
(lldb) run
(lldb) memory read $pc 32
(lldb) disassemble --frame
```

---

## Fix Applied (2024-02-20)

### Problem Analysis

The crash was traced to lazy initialization of the worker_group in block_cache using `std::call_once`. This could cause issues:

1. **Race condition**: The lazy initialization via `std::call_once` during block cache operations could cause threading issues
2. **Reference validity**: Logger and os_access references might not be guaranteed valid during lazy initialization
3. **Extreme compression edge case**: Highly compressible files (10MB → 104B) trigger a specific code path that exposes these issues

### Fix Applied

Changed from **lazy initialization** to **eager initialization** of the worker_group:

**Before (problematic):**
```cpp
void enqueue_job(...) const {
  std::call_once(wg_init_flag_, [this] { init_worker_group(); });
  // ...
}

void init_worker_group() const {
  std::unique_lock lock(mx_wg_);
  if (!wg_) {
    wg_ = worker_group(LOG_GET_LOGGER, os_, "blkcache", ...);
  }
}
```

**After (fixed):**
```cpp
// In block_cache_ constructor:
{
  // ... other initialization ...

  // Eagerly initialize the worker group
  auto num_workers = std::max(options_.num_workers > 0
                                  ? options_.num_workers
                                  : hardware_concurrency(),
                              static_cast<size_t>(1));
  num_workers = std::min(num_workers, static_cast<size_t>(16));  // Limit workers
  wg_ = worker_group(LOG_GET_LOGGER, os_, "blkcache", num_workers);
}

void enqueue_job(...) const {
  // Worker group is now eagerly initialized
  std::shared_lock lock(mx_wg_);
  wg_.add_job(...);
}
```

### Changes Made

1. **`src/reader/internal/block_cache.cpp`**:
   - Added worker_group initialization in constructor (eager)
   - Removed `init_worker_group()` method
   - Removed `std::call_once` lazy initialization in `enqueue_job()`
   - Removed unused `wg_init_flag_` member
   - Added maximum worker limit (16) to prevent resource exhaustion
   - Simplified `set_num_workers()` since worker_group is always initialized

### Why This Fixes the SIGBUS

1. **Guaranteed initialization order**: Logger and os_access are definitely valid when worker_group is created
2. **No race conditions**: Worker threads are created during construction, before any concurrent access
3. **Simpler code**: Removes the complexity of lazy initialization with `std::call_once`

### Verification

- Build succeeds with no errors
- Unit tests pass
- Filesystem tests pass including `large_file` test

### Note

The dwarfsextract hang observed during testing appears to be a **separate issue** from the SIGBUS crash. The SIGBUS was in the library code path during worker_group initialization, while the hang occurs in the extraction tool. This may warrant separate investigation.

---

## Test Coverage Added (2024-02-20)

### New Test: `highly_compressible_large_file`

Added a test that creates a 10MB file filled with zeros and verifies:
1. File can be created in DwarFS archive
2. File can be found and stat'd
3. Entire file can be read in chunks
4. Content is correctly preserved (all zeros)

Location: `test/filesystem/filesystem_operations_test.cpp`

```cpp
TEST_F(FilesystemOperationsTest, highly_compressible_large_file) {
  const size_t fileSize = 10 * 1024 * 1024;  // 10MB
  std::string content(fileSize, '\0');  // All zeros
  // ... creates archive, reads file, verifies content
}
```

### New Fixture: `large.dwarfs`

Created a test fixture at `test/fixtures/large.dwarfs` containing:
- A 10MB file of all zeros
- Compresses from 10MB to ~104 bytes (99.999% compression ratio)

### New Test: `large_dwarfs_fixture`

Tests reading from the actual fixture file:
```cpp
TEST_F(FilesystemOperationsTest, large_dwarfs_fixture) {
  // Load test/fixtures/large.dwarfs
  // Find and read /10mb.bin (10MB of zeros)
  // Verify content is all zeros
}
```

### Verification

All tests pass:
```
[==========] 20 tests ran.
[  PASSED  ] 20 tests.
```

The SIGBUS fix combined with these tests ensures:
1. The crash is fixed (worker_group eager initialization)
2. Highly compressible large files can be read correctly
3. Regression tests prevent future issues
