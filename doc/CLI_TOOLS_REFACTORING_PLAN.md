# CLI Tools Refactoring Plan

**Date**: 2025-11-26
**Objective**: Make all CLI tools thin layers on top of library functionality
**Approach**: Apply the same handler pattern used successfully in mkdwarfs

---

## Executive Summary

All CLI tools should be **thin wrappers** that:
1. Parse command-line options
2. Call library functions
3. Report errors
4. Return exit codes

**No business logic should remain in CLI tool files.**

---

## Current State Analysis

| Tool | Lines | Status | Priority | Target Size | Reduction |
|------|-------|--------|----------|-------------|-----------|
| **mkdwarfs** | ~~1578~~ → 689 | ✅ **DONE** | - | <700 | 56.3% |
| **dwarfs** | 2041 | 🔴 URGENT | **HIGHEST** | <500 | 75% |
| **dwarfsck** | 391 | 🟡 NEEDED | MEDIUM | <150 | 62% |
| **dwarfsextract** | 280 | 🟢 GOOD | LOW | <100 | 64% |

---

## Tool 1: dwarfs_main.cpp (2041 lines)

### Current Architecture Problems

**Massive monolith with embedded FUSE operations**:
- FUSE operation implementations (lines 473-1373): **900 lines**
- Option parsing and validation (lines 1457-1983): **526 lines**
- Filesystem loading (lines 1707-1797): **90 lines**
- Helper functions mixed throughout

**Key Issues**:
- FUSE callbacks defined inline (not reusable)
- Option handling scattered across multiple functions
- Filesystem initialization buried in main()
- No separation between FUSE logic and CLI logic

### Target Architecture

```
dwarfs_main.cpp (< 500 lines)
├── options_parser.parse() → parsed_options
├── handler_factory.create(opts) → unique_ptr<handler_interface>
└── handler->run() → exit code
         │
         ▼
    mount_handler
    (always available)
         │
         ├── Create filesystem_loader
         ├── Setup FUSE operations (via fuse_driver library class)
         └── Run FUSE loop
```

### Required Library Classes (NEW)

**1. `lib/dwarfs/reader/fuse_driver.h`** (300-400 lines)
```cpp
namespace dwarfs::reader {

class fuse_driver {
public:
  fuse_driver(filesystem_v2_lite& fs, 
              fuse_driver_options const& opts,
              logger& lgr);
  
  // Initialize FUSE operations
  void setup_operations(fuse_lowlevel_ops& ops);
  
  // Run the FUSE loop
  int run(fuse_args& args, std::string const& mountpoint);
  
private:
  // All FUSE callbacks as member functions
  void op_init(...);
  void op_lookup(...);
  void op_getattr(...);
  // ... etc
};

} // namespace dwarfs::reader
```

**2. `lib/dwarfs/reader/filesystem_loader.h`** (100-150 lines)
```cpp
namespace dwarfs::reader {

class filesystem_loader {
public:
  static filesystem_v2_lite load(
      logger& lgr,
      os_access& os,
      std::filesystem::path const& image,
      filesystem_options const& opts,
      std::shared_ptr<performance_monitor> perfmon = nullptr);
};

} // namespace dwarfs::reader
```

### Refactoring Phases for dwarfs_main.cpp

**Phase 1**: Extract options_parser (~250 lines)
- All FUSE option definitions
- Option validation logic
- Default value application

**Phase 2**: Create fuse_driver library class (~400 lines)
- Move ALL FUSE operation implementations
- Keep as methods of fuse_driver class
- Make it a proper library component in dwarfs_reader

**Phase 3**: Create filesystem_loader library class (~100 lines)
- Extract filesystem loading logic
- Make it reusable across tools

**Phase 4**: Create mount_handler (~150 lines)
- Orchestrates: options → loader → fuse_driver → run
- Thin glue code

**Phase 5**: Update main() to use handler pattern (~100 lines)
- Parse options
- Create handler
- Run handler
- Report errors

### Expected Result

```
Before: 2041 lines (monolith)
After:  <500 lines (thin CLI layer)

New library classes:
- lib/dwarfs/reader/fuse_driver.cpp: ~400 lines
- lib/dwarfs/reader/filesystem_loader.cpp: ~100 lines

Total code reduction: 75% in CLI, 0% duplication (moved to library)
```

---

## Tool 2: dwarfsck_main.cpp (391 lines)

### Current Architecture Problems

- Three operation modes mixed in one function:
  1. Print header (lines 309-327)
  2. Check/list/checksum (lines 328-376)
  3. Export metadata (lines 330-344)
- Helper functions inline (lines 71-162)
- Option parsing embedded in main

### Target Architecture

```
dwarfsck_main.cpp (< 150 lines)
├── options_parser.parse() → parsed_options
├── handler_factory.create(opts) → unique_ptr<handler_interface>
└── handler->run() → exit code
         │
    ┌────┴────────┐
    ▼             ▼
header_handler  check_handler
                     │
                ├────┼────┐
                │    │    │
             list checksum integrity
```

### Refactoring Phases

**Phase 1**: Extract options_parser (~100 lines)
- All boost::program_options setup
- Validation logic

**Phase 2**: Create operation_handlers (~150 lines)
- `header_handler`: Print filesystem header
- `check_handler`: Check/list/checksum operations
  - Uses existing `do_list_files()` and `do_checksum()`

**Phase 3**: Create handler_factory (~50 lines)
- Select appropriate handler based on options

**Phase 4**: Simplify main() (~100 lines)
- Use handler pattern

### Expected Result

```
Before: 391 lines
After:  <150 lines (62% reduction)
```

---

## Tool 3: dwarfsextract_main.cpp (280 lines)

### Current Architecture Problems

- Relatively clean, but still has mixed concerns
- Option parsing in main (lines 84-173)
- Extraction logic in main (lines 184-267)
- Format selection logic inline

### Target Architecture

```
dwarfsextract_main.cpp (< 100 lines)
├── options_parser.parse() → parsed_options
├── handler_factory.create(opts) → unique_ptr<handler_interface>
└── handler->run() → exit code
         │
         ▼
    extract_handler
         │
         ├── Load filesystem
         ├── Setup extractor (disk/archive/stream)
         └── Extract with options
```

### Refactoring Phases

**Phase 1**: Extract options_parser (~80 lines)

**Phase 2**: Create extract_handler (~100 lines)
- Filesystem loading
- Extractor setup
- Extraction execution

**Phase 3**: Simplify main() (~100 lines)

### Expected Result

```
Before: 280 lines
After:  <100 lines (64% reduction)
```

---

## Common Pattern: Handler Architecture

All tools follow this structure:

```cpp
// tools/include/dwarfs/tool/{tool}/handler_interface.h
class handler_interface {
public:
  virtual ~handler_interface() = default;
  virtual int run() = 0;
};

// tools/include/dwarfs/tool/{tool}/options_parser.h
class options_parser {
public:
  parsed_options parse(int argc, char** argv, iolayer const& iol);
};

// tools/include/dwarfs/tool/{tool}/handler_factory.h
class handler_factory {
public:
  static std::unique_ptr<handler_interface> 
    create(parsed_options const& opts, iolayer const& iol);
};

// tools/src/{tool}_main.cpp
int {tool}_main(int argc, sys_char** argv, iolayer const& iol) {
  options_parser parser;
  auto opts = parser.parse(argc, argv, iol);
  
  auto handler = handler_factory::create(opts, iol);
  if (!handler) {
    return 1; // error already reported
  }
  
  return handler->run();
}
```

---

## Implementation Priority

### Immediate (This Session - if time permits)
1. ✅ **mkdwarfs** - COMPLETED

### Next Session
2. 🔴 **dwarfs** (HIGHEST PRIORITY)
   - Largest file (2041 lines)
   - Most complex (FUSE operations)
   - Biggest impact (75% reduction)
   - **Needs new library classes**: fuse_driver, filesystem_loader

### Following Sessions
3. 🟡 **dwarfsck** (MEDIUM PRIORITY)
   - Medium complexity
   - Three operation modes

4. 🟢 **dwarfsextract** (LOW PRIORITY)
   - Smallest file
   - Already relatively clean

---

## Success Criteria

### Quantitative
- dwarfs_main.cpp: <500 lines (75% reduction)
- dwarfsck_main.cpp: <150 lines (62% reduction)  
- dwarfsextract_main.cpp: <100 lines (64% reduction)
- Average CLI tool size: ~250 lines

### Qualitative
- ✅ All tools use handler pattern
- ✅ No business logic in CLI tools
- ✅ Reusable library classes
- ✅ Consistent architecture
- ✅ Testable components
- ✅ Extensible design
- ✅ Zero user-visible changes

---

## File Organization

```
tools/
├── include/dwarfs/tool/
│   ├── dwarfs/
│   │   ├── options_parser.h
│   │   ├── handler_interface.h
│   │   ├── handler_factory.h
│   │   └── mount_handler.h
│   ├── dwarfsck/
│   │   ├── options_parser.h
│   │   ├── handler_interface.h
│   │   ├── handler_factory.h
│   │   ├── header_handler.h
│   │   └── check_handler.h
│   └── dwarfsextract/
│       ├── options_parser.h
│       ├── handler_interface.h
│       ├── handler_factory.h
│       └── extract_handler.h
├── src/
│   ├── dwarfs/
│   │   ├── options_parser.cpp
│   │   ├── handler_factory.cpp
│   │   └── mount_handler.cpp
│   ├── dwarfsck/
│   │   ├── options_parser.cpp
│   │   ├── handler_factory.cpp
│   │   ├── header_handler.cpp
│   │   └── check_handler.cpp
│   └── dwarfsextract/
│       ├── options_parser.cpp
│       ├── handler_factory.cpp
│       └── extract_handler.cpp
└── {tool}_main.cpp (thin wrapper, <150 lines each)

include/dwarfs/reader/ (NEW library classes for dwarfs)
├── fuse_driver.h
└── filesystem_loader.h

src/reader/ (NEW library implementations)
├── fuse_driver.cpp
└── filesystem_loader.cpp
```

---

## Testing Strategy

For each tool:

1. **Unit Tests** (`test/{tool}_handler_*_test.cpp`)
   - Test each handler independently
   - Mock iolayer
   - Verify option parsing

2. **Integration Tests** (`test/tool_{tool}_integration_test.cpp`)
   - Full command-line scenarios
   - Verify output matches expected
   - Cross-tool compatibility

3. **Regression Tests**
   - Ensure user-visible behavior unchanged
   - Compare before/after outputs

---

## Next Steps

1. ✅ **Complete mkdwarfs refactoring** (DONE)
2. ⏳ **Merge mkdwarfs to main**
3. 🎯 **Begin dwarfs_main.cpp refactoring**:
   - Start with Phase 1: Extract options_parser
   - Then Phase 2: Create fuse_driver library class
   - This is the most complex but highest impact refactoring

---

**Last Updated**: 2025-11-26
**Status**: mkdwarfs ✅ COMPLETE, ready to proceed with dwarfs
**Next Action**: Begin dwarfs_main.cpp Phase 1 - options_parser extraction