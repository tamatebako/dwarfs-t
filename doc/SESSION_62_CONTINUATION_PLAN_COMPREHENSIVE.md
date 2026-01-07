# Session 62-65: Legacy Thrift Implementation - Complete Plan

**Created**: 2026-01-01 14:28 HKT
**Status**: 📋 **COMPREHENSIVE ROADMAP**
**Total Duration**: 12 hours over 4 sessions (compressed from 16 hours)

---

## Executive Summary

**Goal**: Implement Legacy Thrift metadata format by porting dwarfs-rs's hand-coded Thrift Compact protocol to C++

**Why**: Unblock Homebrew v0.14.1 compatibility without fbthrift dependency version hell

**Strategy**: Three-format architecture
1. **FlatBuffers** (modern default) ✅ Working
2. **Legacy Thrift** (hand-coded) 🆕 This implementation
3. **Modern Thrift** (fbthrift) 🔮 Future (when vcpkg stable)

---

## Session-by-Session Breakdown

### Session 62: ✅ COMPLETE (1 hour)

**Achievements**:
- ✅ Phase 1: Thrift Compact primitives (31/31 tests)
- ✅ Phase 2 Start: Basic serialize() method
- ✅ Build system integration
- ✅ CMake updates for triple-format support

**Deliverables**: 9 files, 1,344 lines

---

### Session 63: Phase 2 + 3 Start (4 hours)

**Part 1: Complete Phase 2** (2.5h):
1. Implement deserialize() method (1.5h)
2. Write round-trip tests (0.5h)
3. Fix u64 handling (0.5h)

**Part 2: Start Phase 3** (1.5h):
4. Analyze Frozen2 (0.5h)
5. Basic Frozen2 schema (1h)

**Deliverables**:
- metadata_serialization_test.cpp (~200 lines)
- frozen_schema.h (~150 lines)
- frozen_schema.cpp (~250 lines)

---

### Session 64: Complete Phase 3 (4 hours)

**Goals**:
1. Frozen2 reader implementation (2h)
2. Frozen2 writer implementation (1.5h)
3. Optional fields support (0.5h)

---

### Session 65: Phase 4 Integration (3 hours)

**Goals**:
1. Facade integration (1h)
2. Full compatibility testing (1h)
3. Documentation (1h)

---

## Timeline Summary

| Session | Phase | Hours | Cumulative |
|---------|-------|-------|------------|
| **62** | Phase 1 + 2 start | 1h | 1h |
| **63** | Phase 2 + 3 start | 4h | 5h |
| **64** | Phase 3 complete | 4h | 9h |
| **65** | Phase 4 + docs | 3h | 12h |

**Total**: 12 hours (compressed from original 16-hour estimate)

---

**Created**: 2026-01-01 14:28 HKT
**Status**: 📋 **READY FOR SESSION 63**
