# mkdwarfs Refactoring Continuation Plan

**Date**: 2025-11-23
**Status**: Architecture Design Phase
**Priority**: HIGH

## Executive Summary

The `mkdwarfs_main.cpp` file (1587 lines) violates single responsibility principle and requires proper architectural refactoring. The current linking issue with recompress functionality when building without Thrift support is a symptom of poor separation of concerns.

**Root Problem**: Recompress functionality is tightly coupled with filesystem creation logic, and both are embedded in a monolithic CLI handler.

**Correct Solution**: Extract into separate, testable modules with proper abstraction layers.

## Current State

### What Works
- ✅ Phase 2 (Multi-Format Metadata Serialization): 100% complete
- ✅ All metadata tests passing (58/59, 1 FLAC test issue unrelated)
- ✅ Core libraries build successfully

### What's Broken
- ❌ mkdwarfs cannot build without Thrift due to tight coupling with `rewrite_filesystem()`
- ❌ mkdwarfs_main.cpp is too large (1587 lines) for safe AI editing
- ❌ No separation between create and recompress logic

### What Was Attempted (WRONG Approach)
- ❌ Preprocessor guards (`#ifdef DWARFS_HAVE_THRIFT`) - this is a hack, not architecture
- ❌ The file was corrupted during edit attempts due to size

## Proposed Architecture

### Design Principles
1. **Single Responsibility**: Each class has one clear purpose
2. **Open/Closed**: Open for extension, closed for modification
3. **Dependency Inversion**: Depend on abstractions, not implementations
4. **Interface Segregation**: Clients shouldn't depend on interfaces they don't use
5. **Strategy Pattern**: Different algorithms (create vs recompress) as interchangeable strategies

### Module Structure

```
tools/
├── include/
│   └── mkdwarfs/
│       ├── filesystem_handler.h          (Abstract interface)
│       ├── create_handler.h              (Create strategy)
│       ├── recompress_handler.h          (Recompress strategy, Thrift-dependent)
│       ├── options_parser.h              (Shared option parsing)
│       └── handler_factory.h             (Factory for handlers)
├── src/
│   ├── mkdwarfs_main.cpp                 (~200 lines - CLI entry point only)
│   ├── filesystem_handler.cpp            (Base implementation if needed)
│   ├── create_handler.cpp                (~600 lines - filesystem creation)
│   ├── recompress_handler.cpp            (~400 lines - recompress logic)
│   ├── options_parser.cpp                (~300 lines - option parsing)
│   └── handler_factory.cpp               (~100 lines - handler instantiation)
```

### Class Hierarchy

```cpp
// Abstract interface (tools/include/mkdwarfs/filesystem_handler.h)
class IFilesystemHandler {
public:
    virtual ~IFilesystemHandler() = default;
    virtual int execute(parsed_options const& opts, iolayer const& iol) = 0;
};

// Create handler (tools/include/mkdwarfs/create_handler.h)
class CreateHandler : public IFilesystemHandler {
public:
    explicit CreateHandler(logger& lgr);
    int execute(parsed_options const& opts, iolayer const& iol) override;
private:
    // All current filesystem creation logic
};

// Recompress handler (tools/include/mkdwarfs/recompress_handler.h)
// Only compiled when DWARFS_HAVE_THRIFT=ON
class RecompressHandler : public IFilesystemHandler {
public:
    explicit RecompressHandler(logger& lgr);
    int execute(parsed_options const& opts, iolayer const& iol) override;
private:
    // All current recompress logic
};

// Factory (tools/include/mkdwarfs/handler_factory.h)
class HandlerFactory {
public:
    static std::unique_ptr<IFilesystemHandler> 
    create(parsed_options const& opts, logger& lgr);
};
```

### CMake Integration

```cmake
# In cmake/tools.cmake
add_library(mkdwarfs_lib OBJECT
    tools/src/create_handler.cpp
    tools/src/options_parser.cpp
    tools/src/handler_factory.cpp
)

if(DWARFS_HAVE_THRIFT)
    target_sources(mkdwarfs_lib PRIVATE
        tools/src/recompress_handler.cpp
    )
    target_link_libraries(mkdwarfs_lib PRIVATE dwarfs_rewrite)
endif()

add_executable(mkdwarfs 
    tools/src/mkdwarfs_main.cpp
)
target_link_libraries(mkdwarfs PRIVATE 
    mkdwarfs_lib
    dwarfs_writer
    dwarfs_reader
)
```

## Refactoring Steps

### Phase 1: Extract Options Parser (~1 hour)
1. Create `tools/include/mkdwarfs/options_parser.h`
2. Create `tools/src/options_parser.cpp`
3. Move all option parsing logic (lines 427-760 in current file)
4. **Test**: mkdwarfs still compiles and --help works

### Phase 2: Extract Create Handler (~2 hours)
1. Create `tools/include/mkdwarfs/create_handler.h`
2. Create `tools/src/create_handler.cpp`
3. Move filesystem creation logic (lines 1508-1524 in current file + supporting functions)
4. **Test**: mkdwarfs creates filesystems correctly

### Phase 3: Extract Recompress Handler (~1.5 hours)
1. Create `tools/include/mkdwarfs/recompress_handler.h`
2. Create `tools/src/recompress_handler.cpp`
3. Move recompress logic (lines 1499-1507 + supporting code)
4. Add CMake conditional compilation
5. **Test**: mkdwarfs with Thrift can recompress

### Phase 4: Create Handler Factory (~30 minutes)
1. Create `tools/include/mkdwarfs/handler_factory.h`
2. Create `tools/src/handler_factory.cpp`
3. Implement factory logic to choose handler based on options
4. **Test**: Both create and recompress modes work

### Phase 5: Simplify Main (~30 minutes)
1. Reduce `mkdwarfs_main.cpp` to just:
   - Parse command line
   - Create handler via factory
   - Execute handler
   - Return result
2. **Test**: Full integration test

### Phase 6: Update Build System (~30 minutes)
1. Update `cmake/tools.cmake` with new structure
2. Ensure conditional compilation works correctly
3. **Test**: Build with and without Thrift

### Phase 7: Testing & Documentation (~1 hour)
1. Run full test suite
2. Update relevant documentation
3. Add comments explaining architecture
4. **Test**: All existing tests pass

**Total Estimated Time**: 7-8 hours

## Success Criteria

### Minimum
- ✅ mkdwarfs builds without Thrift (create mode only)
- ✅ mkdwarfs builds with Thrift (both modes)
- ✅ All existing functionality preserved
- ✅ All tests pass

### Ideal
- ✅ Each module < 600 lines
- ✅ Clear separation of concerns
- ✅ Easy to add new modes in future
- ✅ No preprocessor guards in business logic
- ✅ Unit tests for each module

## File Reference

### Current File
- **Location**: `tools/src/mkdwarfs_main.cpp`
- **Size**: 1587 lines
- **Status**: Needs refactoring

### New Files to Create
1. `tools/include/mkdwarfs/filesystem_handler.h` (interface)
2. `tools/include/mkdwarfs/create_handler.h`
3. `tools/include/mkdwarfs/recompress_handler.h`
4. `tools/include/mkdwarfs/options_parser.h`
5. `tools/include/mkdwarfs/handler_factory.h`
6. `tools/src/create_handler.cpp`
7. `tools/src/recompress_handler.cpp`
8. `tools/src/options_parser.cpp`
9. `tools/src/handler_factory.cpp`

### Files to Modify
1. `tools/src/mkdwarfs_main.cpp` (reduce to ~200 lines)
2. `cmake/tools.cmake` (add new library targets)

## Testing Strategy

### Unit Tests
```cpp
// Test create handler
TEST(CreateHandlerTest, CreatesFilesystem) {
    // Setup mock options
    // Execute handler
    // Verify filesystem created
}

// Test recompress handler (if Thrift available)
#ifdef DWARFS_HAVE_THRIFT
TEST(RecompressHandlerTest, RecompressesFilesystem) {
    // Setup mock options with input filesystem
    // Execute handler
    // Verify recompressed output
}
#endif

// Test factory
TEST(HandlerFactoryTest, CreatesCorrectHandler) {
    // Test with create options -> CreateHandler
    // Test with recompress options -> RecompressHandler (if Thrift)
}
```

### Integration Tests
- Create filesystem with various options
- Recompress existing filesystem (if Thrift)
- Verify output matches expected format

## Risk Mitigation

### Potential Issues
1. **Breaking existing functionality**: Mitigated by comprehensive tests
2. **Build system complexity**: Mitigated by clear CMake structure
3. **Performance regression**: Mitigated by keeping same algorithms
4. **Git history confusion**: Mitigated by clear commit messages

### Rollback Strategy
- Each phase is a separate commit
- Can revert to any previous working state
- Keep original file in git history

## Next Session Start

```bash
cd /Users/mulgogi/src/external/dwarfs

# 1. Verify current state
git status
git diff tools/src/mkdwarfs_main.cpp

# 2. Start Phase 1: Extract Options Parser
mkdir -p tools/include/mkdwarfs
touch tools/include/mkdwarfs/options_parser.h
touch tools/src/options_parser.cpp

# 3. Begin extraction following the plan above
```

## Notes

- **IMPORTANT**: No preprocessor guards in business logic
- **IMPORTANT**: Each module must be testable independently
- **IMPORTANT**: Follow SOLID principles throughout
- **IMPORTANT**: Preserve all existing functionality

## References

- Original file: `tools/src/mkdwarfs_main.cpp`
- Rewrite functionality: `src/utility/rewrite_filesystem.cpp`
- Build system: `cmake/tools.cmake`
- Memory bank: `.kilocode/rules/memory-bank/architecture.md`

---

**Last Updated**: 2025-11-23 22:51 HKT
**Next Update**: When Phase 1 begins
**Current Focus**: Architecture design complete, ready to implement