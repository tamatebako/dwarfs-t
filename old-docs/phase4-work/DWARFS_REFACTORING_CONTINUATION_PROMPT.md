# dwarfs_main.cpp Refactoring - Continuation Prompt

**Date**: 2025-11-26
**Context**: Refactoring dwarfs FUSE driver to use handler pattern and extract business logic to library classes
**Current Branch**: (to be created/determined)
**Related Documents**:
- [`doc/CLI_TOOLS_REFACTORING_PLAN.md`](CLI_TOOLS_REFACTORING_PLAN.md)
- [`doc/MKDWARFS_REFACTORING_STATUS.md`](MKDWARFS_REFACTORING_STATUS.md)

---

## Quick Start for Next Session

```
Continue the dwarfs_main.cpp refactoring. We've completed Phases 1-2:
- Phase 1: options_parser module (DONE)
- Phase 2: filesystem_loader library class (DONE)

Now proceed with Phase 3: Create fuse_driver library class in dwarfs_reader.
This is the most complex phase - extract ~900 lines of FUSE operations from
dwarfs_main.cpp into a reusable library component.
```

---

## Current Status (Phases 1-2 Complete)

### ✅ Phase 1: options_parser Module - COMPLETE

**Files Created**:
- `tools/include/dwarfs/tool/dwarfs/options_parser.h` (158 lines)
- `tools/src/dwarfs/options_parser.cpp` (370 lines)

**What Was Extracted**:
- FUSE option definitions (`dwarfs_opts` array, lines 316-355)
- Option validation and string parsing (lines 1895-1983)
- Auto-mountpoint handling (lines 1503-1559)
- Default value application

**Interface**:
```cpp
namespace dwarfs::tool::dwarfs_tool {
  struct parsed_options { /* all FUSE options */ };

  class options_parser {
    int parse(int argc, sys_char** argv, iolayer const& iol,
              parsed_options& opts, fuse_args& args,
              std::filesystem::path& progname);
  };
}
```

### ✅ Phase 2: filesystem_loader Library Class - COMPLETE

**Files Created** (NEW LIBRARY COMPONENTS):
- `include/dwarfs/reader/filesystem_loader.h` (145 lines)
- `src/reader/filesystem_loader.cpp` (93 lines)

**What Was Extracted**:
- Filesystem loading logic (load_filesystem<>, lines 1707-1797)
- Filesystem options construction
- Performance monitoring setup
- Image path canonicalization

**Interface**:
```cpp
namespace dwarfs::reader {
  struct filesystem_load_config { /* configuration */ };

  class filesystem_loader {
    static filesystem_v2_lite load(
        logger& lgr, os_access& os,
        filesystem_load_config const& config,
        std::shared_ptr<performance_monitor> perfmon = nullptr);

    static filesystem_options make_options(
        filesystem_load_config const& config);
  };
}
```

**Key Achievement**: This is a **reusable library class** that external projects can use to load DwarFS images without any CLI dependencies.

---

## Next Steps: Phase 3 - fuse_driver Library Class

### Overview

**Objective**: Extract ~900 lines of FUSE operations from `dwarfs_main.cpp` into a reusable library class in `dwarfs_reader`.

**Why This Matters**:
- Makes FUSE mounting functionality reusable by external projects
- Separates FUSE logic from CLI concerns
- Enables testing FUSE operations independently
- Follows the same library-first approach as `filesystem_loader`

### Files to Create

1. **`include/dwarfs/reader/fuse_driver.h`** (~200 lines)
   - Class definition for fuse_driver
   - Configuration struct
   - Public interface for FUSE operations

2. **`src/reader/fuse_driver.cpp`** (~700 lines)
   - All FUSE operation implementations
   - Helper functions
   - FUSE operations setup

### Code to Extract from dwarfs_main.cpp

**FUSE Operations** (lines 473-1373, ~900 lines):
- `op_init_common<>` / `op_init<>` (lines 473-516)
- `op_lookup<>` (lines 518-563)
- `op_getattr_common` / `op_getattr<>` (lines 565-621)
- `op_readlink_common` / `op_readlink<>` (lines 623-685)
- `op_open_common` / `op_open<>` (lines 687-785)
- `op_lseek_common` / `op_lseek<>` (lines 787-855, ifdef DWARFS_FUSE_HAS_LSEEK)
- `op_read<>` (lines 857-910)
- `op_readdir_common` / `op_readdir<>` (lines 912-1068)
- `op_statfs_common` / `op_statfs<>` (lines 1070-1121)
- `op_getxattr_common` / `op_getxattr<>` (lines 1123-1281)
- `op_listxattr_common` / `op_listxattr<>` (lines 1283-1373)

**Helper Classes** (lines 913-983):
- `readdir_lowlevel_policy` (FUSE lowlevel API)
- `readdir_policy` (FUSE high-level API)

**Helper Functions**:
- `checked_call<>` (lines 386-400)
- `checked_reply_err<>` (lines 402-410, lowlevel only)
- `get_caller_context` (lines 462-471)

**FUSE Operations Setup**:
- `init_fuse_ops<>` (lines 1561-1607)

### Proposed Architecture

```cpp
// include/dwarfs/reader/fuse_driver.h
namespace dwarfs::reader {

struct fuse_driver_config {
  // Worker configuration
  size_t num_workers{2};

  // Cache tidying
  cache_tidy_config tidy;

  // Preloading
  std::optional<std::string> preload_category;
  bool preload_all{false};

  // File caching
  bool cache_files{true};
  bool cache_sparse{false};

  // Analysis output
  std::optional<std::filesystem::path> analysis_file;

  // Performance monitoring
  std::shared_ptr<performance_monitor> perfmon;

  // Logging threshold (for operation selection)
  logger::level_type log_threshold{logger::WARN};
};

class fuse_driver {
 public:
  fuse_driver(filesystem_v2_lite& fs,
              fuse_driver_config const& config,
              logger& lgr,
              os_access const& os);

  ~fuse_driver();

  // Initialize FUSE operations structure
  void setup_operations(fuse_lowlevel_ops& ops);  // or fuse_operations

  // Get userdata pointer for FUSE
  void* get_userdata();

 private:
  // All FUSE operation implementations as private methods
  void op_init(void* data, fuse_conn_info* conn);
  void op_lookup(fuse_req_t req, fuse_ino_t parent, char const* name);
  void op_getattr(fuse_req_t req, fuse_ino_t ino, fuse_file_info*);
  // ... etc for all operations

  // Helper methods
  template <typename LogProxy, typename T>
  auto checked_call(LogProxy& log_, T&& f);

  template <typename LogProxy, typename T>
  void checked_reply_err(LogProxy& log_, fuse_req_t req, T&& f);

  std::string get_caller_context(fuse_req_t req);

  // Member data
  struct impl;
  std::unique_ptr<impl> impl_;
};

} // namespace dwarfs::reader
```

### Implementation Strategy

**Step 1**: Create header with class declaration and config struct

**Step 2**: Create implementation with PIMPL pattern
- Private `impl` struct contains:
  - Reference to `filesystem_v2_lite`
  - Logger reference
  - OS access reference
  - Configuration
  - Analysis helper (if enabled)
  - Performance monitor
  - FUSE-T detection flag

**Step 3**: Extract FUSE operations
- Convert free functions to member functions
- Use `impl_->` to access members
- Preserve template logger policies

**Step 4**: Extract helper classes
- Move readdir policies into impl or separate classes
- Keep them in reader namespace

**Step 5**: Implement setup_operations
- Populate fuse_lowlevel_ops or fuse_operations
- Use member function pointers or static trampolines

### Critical Considerations

1. **Template Logger Policies**: FUSE operations use `debug_logger_policy` vs `prod_logger_policy`. Keep this pattern but make it internal to the class.

2. **Userdata Access**: Current code uses `dUSERDATA` macro. In the class, this becomes `impl_->` or `this->`.

3. **Platform Differences**: Preserve `#ifdef DWARFS_FUSE_LOWLEVEL`, `#ifdef DWARFS_FUSE_HAS_LSEEK`, `#ifdef _WIN32` conditionals.

4. **Static vs Member Functions**: FUSE C API expects function pointers. Use static trampolines that call member functions, or use FUSE's userdata mechanism.

5. **Performance Monitor Macros**: Keep `PERFMON_*` macros functional, passing perfmon through config.

### Testing Approach

After creating the library class:

1. **Unit Test** (`test/fuse_driver_test.cpp`):
   - Mock filesystem_v2_lite
   - Test each operation individually
   - Verify error handling

2. **Integration Test** (`test/tool_dwarfs_integration_test.cpp`):
   - Mount real filesystem
   - Perform FUSE operations
   - Verify results match expected

---

## Subsequent Phases (4-9)

### Phase 4: mount_handler Module (~150 lines)

Create `tools/include/dwarfs/tool/dwarfs/mount_handler.h`:

```cpp
namespace dwarfs::tool::dwarfs_tool {

class mount_handler : public handler_interface {
 public:
  mount_handler(parsed_options const& opts, iolayer const& iol);
  int run() override;

 private:
  parsed_options opts_;
  iolayer const& iol_;
};

} // namespace
```

**Responsibilities**:
- Load filesystem using `filesystem_loader::load()`
- Create `fuse_driver` instance
- Setup FUSE operations
- Run FUSE loop
- Clean up on exit

### Phase 5: Update dwarfs_main.cpp (~100 lines)

Transform main() to thin wrapper:

```cpp
int dwarfs_main(int argc, sys_char** argv, iolayer const& iol) {
  // Parse options
  fuse_args args = /* initialize */;
  parsed_options opts;
  std::filesystem::path progname;
  options_parser parser;

  if (parser.parse(argc, argv, iol, opts, args, progname)) {
    return 1;  // error or help shown
  }

  // Create handler
  auto handler = std::make_unique<mount_handler>(opts, iol);

  // Run
  return handler->run();
}
```

**Lines Removed**: ~1900 lines (93% reduction!)

### Phase 6: Update CMake Build System

**Add to `cmake/libdwarfs.cmake`**:
```cmake
# dwarfs_reader library sources
target_sources(dwarfs_reader PRIVATE
  src/reader/filesystem_loader.cpp
  src/reader/fuse_driver.cpp
)
```

**Add to `cmake/tools.cmake`**:
```cmake
# dwarfs tool sources
set(DWARFS_SOURCES
  tools/src/dwarfs_main.cpp
  tools/src/dwarfs/options_parser.cpp
  tools/src/dwarfs/mount_handler.cpp
)
```

### Phase 7: Write Unit Tests

- `test/filesystem_loader_test.cpp`
- `test/fuse_driver_test.cpp`
- `test/dwarfs_options_parser_test.cpp`
- `test/dwarfs_mount_handler_test.cpp`

### Phase 8: Write Integration Tests

- `test/tool_dwarfs_integration_test.cpp`
  - Test full mount/unmount cycle
  - Test file operations
  - Test cache behavior
  - Test error handling

### Phase 9: Update Documentation

**Update**:
- `doc/CHANGES.md` - Add v0.16.0 entry
- `doc/dwarfs.md` - Update if library APIs exposed
- `.kilocode/rules/memory-bank/architecture.md` - Document new architecture
- `.kilocode/rules/memory-bank/context.md` - Mark complete

---

## Expected Final State

### Metrics

| Component | Before | After | Reduction |
|-----------|--------|-------|-----------|
| dwarfs_main.cpp | 2041 lines | <500 lines | **75%** |
| New library code | 0 | ~800 lines | Reusable |
| Tool modules | 0 | ~200 lines | Clean |

### Architecture

```
dwarfs_main.cpp (<500 lines)
├── options_parser.parse() → opts + args
├── mount_handler.create(opts, iol) → handler
└── handler.run() → exit code
         │
         ├── filesystem_loader::load() → filesystem
         ├── fuse_driver(filesystem, config) → driver
         ├── driver.setup_operations() → fuse_ops
         └── run_fuse_loop() → result
```

### Library Reusability

External projects can now:

```cpp
// Load filesystem
reader::filesystem_load_config config;
config.image_path = "app.dwarfs";
auto fs = reader::filesystem_loader::load(lgr, os, config);

// Mount with FUSE
reader::fuse_driver_config fuse_config;
fuse_config.num_workers = 4;
reader::fuse_driver driver(fs, fuse_config, lgr, os);

fuse_lowlevel_ops ops;
driver.setup_operations(ops);
// ... use with FUSE
```

---

## Checklist for Next Session

### Before Starting
- [ ] Read this continuation prompt
- [ ] Review completed files from Phases 1-2
- [ ] Read dwarfs_main.cpp lines 473-1607 (FUSE operations)

### Phase 3 Execution
- [ ] Create `include/dwarfs/reader/fuse_driver.h`
- [ ] Create `src/reader/fuse_driver.cpp`
- [ ] Extract all FUSE operations
- [ ] Extract helper functions
- [ ] Test compilation
- [ ] Verify no regressions

### Validation
- [ ] All FUSE operations preserved
- [ ] Platform-specific code intact (#ifdef blocks)
- [ ] Performance monitoring preserved
- [ ] Logger policies functional

---

**Last Updated**: 2025-11-26
**Status**: Phases 1-2 complete, ready for Phase 3
**Next Action**: Create fuse_driver library class