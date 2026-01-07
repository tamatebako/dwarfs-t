# Next Session: Complete Phase 2.5-2.9 - Multi-Format Metadata Implementation

**For AI Session Starting After**: 2025-11-22 23:56 HKT
**Estimated Duration**: 11-15 hours (can be split across multiple sessions)

---

## Quick Context

You're continuing DwarFS multi-format metadata serialization work. **Phase 2.4 is complete** - abstract interfaces successfully created. Now you need to complete ALL remaining implementation work (Phases 2.5-2.9) to make dual-format builds functional.

**What's Done**:
- ✅ Phase 2.1-2.3: Code isolation (Thrift + FlatBuffers backends separated)
- ✅ Phase 2.4: Abstract interfaces created (4 interface headers)

**What Remains** (THIS IS YOUR COMPLETE TASK):
- ⬜ Phase 2.5: Implement interfaces in both backends
- ⬜ Phase 2.6: Factory pattern implementation
- ⬜ Phase 2.7: CMake build system integration
- ⬜ Phase 2.8: Comprehensive testing
- ⬜ Phase 2.9: Build validation across all configurations

---

## Your Complete Task: Phases 2.5-2.9

### Phase 2.5: Implement Interfaces in Both Backends (3-4 hours)

#### Goals
- Make FlatBuffers backend classes inherit from interfaces
- Make Thrift backend classes inherit from interfaces
- Update wrapper classes to use abstract interfaces
- Ensure polymorphism works correctly

#### Files to Modify

**FlatBuffers Backend** (4 files):
1. `include/dwarfs/reader/internal/metadata_types_flatbuffers.h`
   - Add inheritance: `class inode_view_impl : public inode_view_interface {`
   - Add inheritance: `class dir_entry_view_impl : public dir_entry_view_interface {`
   - Add inheritance: `class global_metadata : public global_metadata_interface {`
   - Add inheritance: `class chunk_view : public chunk_view_interface {`
   - Mark ALL methods with `override` keyword

2. `src/reader/internal/metadata_types_flatbuffers.cpp`
   - Verify all interface methods are implemented
   - Add any missing method implementations

**Thrift Backend** (4 files):
3. `include/dwarfs/reader/internal/metadata_types_thrift.h`
   - Add inheritance: `class inode_view_impl : public inode_view_interface {`
   - Add inheritance: `class dir_entry_view_impl : public dir_entry_view_interface {`
   - Add inheritance: `class global_metadata : public global_metadata_interface {`
   - Add inheritance: `class chunk_view : public chunk_view_interface {`
   - Mark ALL methods with `override` keyword

4. `src/reader/internal/metadata_types_thrift.cpp`
   - Verify all interface methods are implemented
   - Add any missing method implementations

**Wrapper Classes** (1 file):
5. `include/dwarfs/reader/metadata_types.h`
   - Change `std::shared_ptr<internal::inode_view_impl const>` to `std::unique_ptr<internal::inode_view_interface>`
   - Change `std::shared_ptr<internal::dir_entry_view_impl const>` to `std::unique_ptr<internal::dir_entry_view_interface>`
   - Update all usages accordingly

#### Implementation Notes
- Use `public interface_name` for inheritance
- Mark each overridden method with `override` keyword
- Methods that return backend objects must now return `std::unique_ptr<interface>`
- Example:
  ```cpp
  // Before
  std::shared_ptr<inode_view_impl> inode_shared() const;
  
  // After
  std::unique_ptr<inode_view_interface> inode() const override;
  ```

---

### Phase 2.6: Factory Pattern Implementation (2-3 hours)

#### Goals
- Create factory to instantiate correct backend at runtime
- Implement format detection logic
- Integrate with existing metadata loading code

#### Files to Create

1. **`include/dwarfs/reader/internal/metadata_factory.h`**
   ```cpp
   namespace dwarfs::reader::internal {
   
   enum class metadata_format {
     flatbuffers,
     thrift
   };
   
   class metadata_factory {
   public:
     static metadata_format detect_format(std::span<uint8_t const> data);
     
     static std::unique_ptr<global_metadata_interface>
     create_global_metadata(logger& lgr, std::span<uint8_t const> data);
     
     // Additional factory methods as needed
   };
   
   }
   ```

2. **`src/reader/internal/metadata_factory.cpp`**
   - Implement format detection via magic bytes
   - FlatBuffers: Check for FlatBuffers identifier
   - Thrift: Fallback if no FlatBuffers magic
   - Create appropriate backend instance

#### Files to Modify

3. **`src/reader/internal/metadata_v2_factory.cpp`**
   - Update `create_metadata_v2()` to use new factory
   - Remove direct backend instantiation
   - Use factory's format detection

4. **`src/reader/filesystem_v2.cpp`**
   - Update to use factory-created metadata
   - Remove any direct backend dependencies

---

### Phase 2.7: Update CMake Build System (30 minutes)

#### Goals
- Add interface headers to build
- Properly configure conditional compilation
- Ensure all configurations build

#### Files to Modify

1. **`cmake/libdwarfs.cmake`**
   
   **Add interface headers** (around line 30, after other headers):
   ```cmake
   # Backend abstract interfaces (always available)
   ${PROJECT_SOURCE_DIR}/include/dwarfs/reader/internal/inode_view_interface.h
   ${PROJECT_SOURCE_DIR}/include/dwarfs/reader/internal/dir_entry_view_interface.h
   ${PROJECT_SOURCE_DIR}/include/dwarfs/reader/internal/global_metadata_interface.h
   ${PROJECT_SOURCE_DIR}/include/dwarfs/reader/internal/chunk_view_interface.h
   ```
   
   **Add factory files** (after metadata_v2_factory.cpp):
   ```cmake
   ${PROJECT_SOURCE_DIR}/src/reader/internal/metadata_factory.cpp
   ```
   
   **Update conditional compilation** (around line 70-80):
   ```cmake
   if(DWARFS_HAVE_FLATBUFFERS)
     target_sources(dwarfs_reader PRIVATE
       ${PROJECT_SOURCE_DIR}/src/reader/internal/metadata_types_flatbuffers.cpp
       ${PROJECT_SOURCE_DIR}/src/reader/internal/metadata_v2_flatbuffers.cpp
     )
   endif()
   
   if(DWARFS_HAVE_THRIFT)
     target_sources(dwarfs_reader PRIVATE
       ${PROJECT_SOURCE_DIR}/src/reader/internal/metadata_types_thrift.cpp
       ${PROJECT_SOURCE_DIR}/src/reader/internal/metadata_v2_thrift.cpp
     )
   endif()
   ```

#### Build Test
```bash
# Test all configurations
cmake -B build-fb -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF
cmake -B build-both -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON
cmake --build build-fb
cmake --build build-both
```

---

### Phase 2.8: Write Comprehensive Tests (2-3 hours)

#### Goals
- Test interface compliance
- Test factory pattern
- Test backend compatibility
- Achieve high coverage

#### Files to Create

1. **`test/inode_view_interface_test.cpp`**
   - Test all inode_view_interface methods
   - Test with both backends (if available)
   - Verify polymorphism works

2. **`test/dir_entry_view_interface_test.cpp`**
   - Test all dir_entry_view_interface methods
   - Test path operations
   - Verify navigation works

3. **`test/global_metadata_interface_test.cpp`**
   - Test metadata navigation
   - Test string table access
   - Verify consistency checking

4. **`test/chunk_view_interface_test.cpp`**
   - Test chunk data/hole identification
   - Test block/offset/size accessors
   - Verify iterator behavior

5. **`test/metadata_factory_test.cpp`**
   - Test format detection
   - Test factory creation
   - Test error handling

6. **`test/backend_compatibility_test.cpp`**
   - Test reading FlatBuffers images
   - Test reading Thrift images (if available)
   - Test round-trip compatibility

#### Testing Guidelines
- Use GoogleTest framework
- Mock interfaces where appropriate
- Test error conditions
- Verify memory management (no leaks)

---

### Phase 2.9: Build Validation (1-2 hours)

#### Goals
- Verify all build configurations work
- Test with actual filesystem images
- Validate tools functionality

#### Build Configurations to Test

1. **FlatBuffers-only build** (should work)
   ```bash
   cmake -B build-fb-only \
     -DDWARFS_WITH_FLATBUFFERS=ON \
     -DDWARFS_WITH_THRIFT=OFF \
     -DWITH_TESTS=ON
   cmake --build build-fb-only
   ctest --test-dir build-fb-only
   ```

2. **Thrift-only build** (should fail - FlatBuffers required)
   ```bash
   cmake -B build-thrift-only \
     -DDWARFS_WITH_FLATBUFFERS=OFF \
     -DDWARFS_WITH_THRIFT=ON
   # Expected: CMake error about FlatBuffers being required
   ```

3. **Dual-format build** (should work)
   ```bash
   cmake -B build-dual \
     -DDWARFS_WITH_FLATBUFFERS=ON \
     -DDWARFS_WITH_THRIFT=ON \
     -DWITH_TESTS=ON
   cmake --build build-dual
   ctest --test-dir build-dual
   ```

4. **Tebako build** (should work, FlatBuffers-only)
   ```bash
   cmake -B build-tebako \
     -DTEBAKO_BUILD=ON \
     -DTEBAKO_BUILD_SCOPE=ALL \
     -DWITH_TESTS=ON
   cmake --build build-tebako
   ```

#### Functional Validation

Test with real images:
```bash
# Create test image with FlatBuffers
./build-dual/mkdwarfs -i /usr/include -o test-fb.dwarfs --metadata-format=flatbuffers

# Create test image with Thrift (if available)
./build-dual/mkdwarfs -i /usr/include -o test-thrift.dwarfs --metadata-format=thrift

# Mount and verify
mkdir -p mnt-fb mnt-thrift
./build-dual/dwarfs test-fb.dwarfs mnt-fb &
./build-dual/dwarfs test-thrift.dwarfs mnt-thrift &
ls -la mnt-fb/ mnt-thrift/

# Check integrity
./build-dual/dwarfsck test-fb.dwarfs
./build-dual/dwarfsck test-thrift.dwarfs

# Extract
./build-dual/dwarfsextract -i test-fb.dwarfs -o extract-fb/
./build-dual/dwarfsextract -i test-thrift.dwarfs -o extract-thrift/

# Cleanup
fusermount -u mnt-fb mnt-thrift
```

#### Validation Checklist
- [ ] All existing tests pass
- [ ] New interface tests pass
- [ ] Format detection works correctly
- [ ] Can mount FlatBuffers images (all builds)
- [ ] Can mount Thrift images (dual-format build only)
- [ ] mkdwarfs creates valid images
- [ ] dwarfsck verifies images
- [ ] dwarfsextract extracts correctly
- [ ] No memory leaks (run with valgrind)
- [ ] No compilation warnings

---

## Essential Reading (in order)

1. [`doc/IMPLEMENTATION_STATUS_PHASE_2_4_TO_2_9.md`](IMPLEMENTATION_STATUS_PHASE_2_4_TO_2_9.md) - Track your progress here
2. [`doc/PHASE_2_3_COMPLETION_ANALYSIS_2025-11-22.md`](PHASE_2_3_COMPLETION_ANALYSIS_2025-11-22.md) - Understand why interfaces were needed
3. [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md) - System architecture
4. [`doc/CONTINUATION_PLAN_PHASE_2_4_TO_2_9.md`](CONTINUATION_PLAN_PHASE_2_4_TO_2_9.md) - Detailed plan

---

## Success Criteria

You are done when ALL of the following are true:

✅ Phase 2.5: Both backends implement all interfaces with `override`
✅ Phase 2.5: Wrapper classes use abstract interfaces
✅ Phase 2.6: Factory pattern implemented and working
✅ Phase 2.7: CMake builds all configurations without errors
✅ Phase 2.8: All tests written and passing
✅ Phase 2.9: FlatBuffers-only build works
✅ Phase 2.9: Dual-format build works
✅ Phase 2.9: Tebako build works
✅ Phase 2.9: Can mount/read/extract both FlatBuffers and Thrift images
✅ Phase 2.9: All tools (mkdwarfs, dwarfs, dwarfsck, dwarfsextract) functional

---

## Troubleshooting Common Issues

### Compilation Errors

**"undefined reference to vtable"**
- Cause: Missing `override` implementations
- Fix: Ensure ALL interface methods are implemented in backend classes

**"cannot convert from X to Y"**
- Cause: Return type mismatch (shared_ptr vs unique_ptr)
- Fix: Update wrapper classes to use `std::unique_ptr<interface>`

**"pure virtual function called"**
- Cause: Calling interface method on uninitialized object
- Fix: Ensure factory properly instantiates concrete backend

### Build System Issues

**"target depends on itself"**
- Cause: Circular dependency in CMake
- Fix: Check target_link_libraries and target_sources ordering

**"file not found"**
- Cause: Missing include path or generated file
- Fix: Check CMake include_directories and generated files

### Test Failures

**"test crashes immediately"**
- Cause: Null pointer dereference
- Fix: Check factory returns valid object before use

**"test hangs"**
- Cause: Deadlock or infinite loop
- Fix: Check iterator implementations, especially end() conditions

---

## After Completion

1. Update [`doc/IMPLEMENTATION_STATUS_PHASE_2_4_TO_2_9.md`](IMPLEMENTATION_STATUS_PHASE_2_4_TO_2_9.md) with 100% complete
2. Create `doc/PHASE_2_COMPLETION_SUMMARY.md` with final results
3. Update [`.kilocode/rules/memory-bank/context.md`](../.kilocode/rules/memory-bank/context.md) to reflect completion
4. Archive phase docs to `old-docs/phase-work/`
5. Create PR with all changes

---

**Branch**: `feature/multi-format-serialization-fuse`
**Base Directory**: `/Users/mulgogi/src/external/dwarfs`
**Current Progress**: Phase 2.4 Complete (17%) → Phases 2.5-2.9 Remaining (83%)
**Estimated Time**: 11-15 hours total (can be split across sessions)

---

**IMPORTANT**: This is the COMPLETE task. Do not stop until ALL phases (2.5-2.9) are finished and validated. The goal is a fully working dual-format build with all tests passing.