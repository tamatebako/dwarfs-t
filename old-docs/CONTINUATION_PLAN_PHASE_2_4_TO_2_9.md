{
# Continuation Plan: Phase 2.4 - 2.9
## Multi-Format Metadata Serialization - Strategy Pattern Implementation

**Created**: 2025-11-22 23:22 HKT
**Estimated Duration**: 13-18 hours (2-3 workdays)
**Current Status**: Phase 2.3 Complete | Architecture Redesign Required

---

## Executive Summary

Phase 2.3 successfully isolated Thrift backend code into `thrift_backend::` namespace, but revealed a critical architectural issue: **lack of polymorphic abstraction**. The current tight coupling to concrete types violates the Dependency Inversion Principle and prevents dual-format builds from working.

**Solution**: Implement Strategy Pattern with abstract interfaces.

---

## Phase 2.4: Create Abstract Interface Layer
**Duration**: 2-3 hours | **Priority**: CRITICAL

### Objective
Define pure virtual base classes enabling polymorphism.

### Files to Create (4 interface headers)
1. `include/dwarfs/reader/internal/inode_view_interface.h` - Inode accessor interface
2. `include/dwarfs/reader/internal/dir_entry_view_interface.h` - Directory entry interface
3. `include/dwarfs/reader/internal/global_metadata_interface.h` - Metadata accessor interface
4. `include/dwarfs/reader/internal/chunk_view_interface.h` - Chunk accessor interface

---

## Phase 2.5: Implement Interfaces in Both Backends
**Duration**: 3-4 hours | **Priority**: CRITICAL

Update both `flatbuffers_backend::` and `thrift_backend::` to implement interfaces.

---

## Phase 2.6: Factory Pattern Implementation
**Duration**: 2-3 hours | **Priority**: HIGH

Create factory for format detection and backend instantiation.

---

## Phase 2.7: Update CMake Build System
**Duration**: 30 minutes

Configure conditional compilation for backends.

---

## Phase 2.8: Write Comprehensive Tests
**Duration**: 2-3 hours

Test interface compliance, factory pattern, backend compatibility.

---

## Phase 2.9: Build Validation
**Duration**: 1-2 hours

Test all build configurations (FlatBuffers-only, dual-format, Tebako).

---

## Documentation Updates

### Create/Update
- `doc/metadata-formats.md` (NEW)
- Update README.adoc with metadata serialization section
- Update tool manuals (`mkdwarfs.md`, `dwarfs.md`)

### Archive to `old-docs/phase-work/`
- `doc/PHASE_2_CONTINUATION_PROMPT_2025-11-22.md`
- `doc/FULL_CONTINUATION_PLAN_2025-11-22.md`
- `doc/PHASE_2_IMPLEMENTATION_STATUS_2025-11-22.md`

---

## Success Criteria

### Technical
- ✅ All backends implement interfaces correctly
- ✅ Factory pattern works for both formats
- ✅ All build configurations succeed
- ✅ 100% test coverage for interfaces
- ✅ Zero compilation warnings

### Architectural
- ✅ Dependency Inversion Principle upheld
- ✅ Open/Closed Principle satisfied
- ✅ Single Responsibility maintained
- ✅ MECE properties verified
- ✅ Proper separation of concerns

---

**Next Session**: See [`doc/NEXT_SESSION_PROMPT_PHASE_2_4.md`](NEXT_SESSION_PROMPT_PHASE_2_4.md)
**Status Tracker**: See [`doc/IMPLEMENTATION_STATUS_PHASE_2_4_TO_2_9.md`](IMPLEMENTATION_STATUS_PHASE_2_4_TO_2_9.md)
