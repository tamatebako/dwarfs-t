# Session 57: vcpkg Enforcement - Implementation Status

**Last Updated**: 2025-12-31 15:24 HKT
**Status**: 🔴 **NOT STARTED** - Ready for implementation
**Overall Progress**: 0% (0/5 phases complete)

---

## Phase Progress

### Phase 1: Remove Submodule References ⏸️ NOT STARTED
**Status**: Pending
**Estimated**: 1-2 hours
**Progress**: 0%

- [ ] 1.1: Update `cmake/folly.cmake`
  - [ ] Remove `add_subdirectory(folly)` (line 116)
  - [ ] Add `find_package(folly CONFIG REQUIRED)`
  - [ ] Add vcpkg enforcement guard
  - [ ] Remove `dwarfs_folly_lite` target (lines 169-274)

- [ ] 1.2: Update `cmake/thrift.cmake`
  - [ ] Remove `add_subdirectory(fbthrift)` (line 37)
  - [ ] Add `find_package(FBThrift CONFIG REQUIRED)`
  - [ ] Add vcpkg enforcement guard
  - [ ] Remove `dwarfs_thrift_lite` target (lines 47-76)

### Phase 2: Update Target Links ⏸️ NOT STARTED
**Status**: Pending
**Estimated**: 0.5-1 hour
**Progress**: 0%

- [ ] 2.1: Update `cmake/libdwarfs.cmake`
  - [ ] Replace `dwarfs_folly_lite` with `Folly::folly`
  - [ ] Replace `dwarfs_thrift_lite` with `FBThrift::thrift`

- [ ] 2.2: Update `cmake/tool_support.cmake`
  - [ ] Update any folly/thrift target references

### Phase 3: vcpkg Port Configuration ⏸️ NOT STARTED
**Status**: Pending
**Estimated**: 0.5 hour
**Progress**: 0%

- [ ] 3.1: Verify `vcpkg_ports/folly/portfile.cmake`
- [ ] 3.2: Verify `vcpkg_ports/fbthrift/portfile.cmake`
- [ ] 3.3: Verify `vcpkg_ports/jemalloc/portfile.cmake`

### Phase 4: Test Build ⏸️ NOT STARTED
**Status**: Pending
**Estimated**: 0.5 hour
**Progress**: 0%

- [ ] 4.1: Clean rebuild with vcpkg
- [ ] 4.2: Build dwarfs_unit_tests
- [ ] 4.3: Build mkdwarfs/dwarfsck

### Phase 5: Verify Converter Fix ⏸️ NOT STARTED
**Status**: Pending
**Estimated**: 0.5 hour
**Progress**: 0%

- [ ] 5.1: Run converter round-trip tests
- [ ] 5.2: Test Homebrew compatibility (our → Homebrew)
- [ ] 5.3: Test Homebrew compatibility (Homebrew → our)

---

## Completion Criteria

- [ ] All submodule references removed from CMake
- [ ] Build enforces vcpkg toolchain (fails without it)
- [ ] All tests pass
- [ ] Homebrew v0.14.1 can read our Thrift files
- [ ] We can read Homebrew v0.14.1 Thrift files

---

## Files Modified (To Be Updated)

**Completed in Session 56**:
1. ✅ `src/metadata/converters/cpp_thrift_converter.cpp` - Converter bug fix
2. ✅ `test/metadata/converter_roundtrip_test.cpp` - Round-trip tests
3. ✅ `vcpkg.json` - Added folly/fbthrift dependencies
4. ✅ `folly/` and `fbthrift/` - Submodules deleted

**To Be Modified in Session 57**:
5. ⏸️ `cmake/folly.cmake`
6. ⏸️ `cmake/thrift.cmake`
7. ⏸️ `cmake/libdwarfs.cmake`
8. ⏸️ `cmake/tool_support.cmake` (if needed)

---

## Blockers / Issues

**None currently** - Plan is clear, ready to execute

---

## Next Steps

1. Start with Phase 1.1 (`cmake/folly.cmake`)
2. Execute phases sequentially
3. Test after each phase
4. Update this status document as work progresses

**Ready to begin**: YES ✅