# Next Session: Phase 2.4 - Create Abstract Interface Layer
**For AI Session Starting After**: 2025-11-22
**Estimated Duration**: 2-3 hours

---

## Quick Context

You're continuing DwarFS multi-format metadata serialization work. **Phase 2.3 is complete** - Thrift backend successfully isolated in `thrift_backend::` namespace. However, compilation revealed an architectural issue: missing polymorphic abstraction layer.

**Current Problem**: Wrapper classes are tightly coupled to concrete `internal::` types, preventing dual-format builds from working.

**Solution**: Implement Strategy Pattern with abstract interfaces (Phase 2.4-2.9).

---

## Your Task: Phase 2.4

Create 4 abstract interface headers defining pure virtual base classes for all backend types.

### Files to Create

1. **`include/dwarfs/reader/internal/inode_view_interface.h`**
   - Pure virtual methods for all inode accessors (mode, uid, gid, timestamps, etc.)
   - See existing `metadata_types_flatbuffers.h` for method signatures

2. **`include/dwarfs/reader/internal/dir_entry_view_interface.h`**
   - Pure virtual methods for directory entry access (name, inode_num, indexes)

3. **`include/dwarfs/reader/internal/global_metadata_interface.h`**
   - Pure virtual methods for metadata navigation
   - Must return `std::unique_ptr<interface>` for polymorphism

4. **`include/dwarfs/reader/internal/chunk_view_interface.h`**
   - Pure virtual methods for chunk access and iteration

### Implementation Guidelines

✅ **DO**:
- Make all methods `= 0` (pure virtual)
- Use `override` keyword in implementations
- Return interface pointers (`std::unique_ptr<interface>`)
- Keep interfaces platform-neutral

❌ **DON'T**:
- Include backend-specific types in interfaces
- Use concrete types in return values
- Mix FlatBuffers and Thrift code

---

## Essential Reading (in order)

1. [`doc/PHASE_2_3_COMPLETION_ANALYSIS_2025-11-22.md`](PHASE_2_3_COMPLETION_ANALYSIS_2025-11-22.md) - Why interfaces are needed
2. [`.kilocode/rules/memory-bank/architecture.md`](../.kilocode/rules/memory-bank/architecture.md) - System architecture
3. [`doc/IMPLEMENTATION_STATUS_PHASE_2_4_TO_2_9.md`](IMPLEMENTATION_STATUS_PHASE_2_4_TO_2_9.md) - Track progress here

---

## After Phase 2.4 Complete

Update status tracker and proceed to **Phase 2.5**: Implement interfaces in both backends.

---

**Branch**: `feature/multi-format-serialization-fuse`
**Base Directory**: `/Users/mulgogi/src/external/dwarfs`
**Progress**: Phase 2.3 Complete (50%) → Phase 2.4 Next
// ... existing code ...