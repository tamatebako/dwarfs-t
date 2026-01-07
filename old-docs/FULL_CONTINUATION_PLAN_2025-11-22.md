# Complete Multi-Format Metadata Continuation Plan
**Created**: 2025-11-22 17:30 HKT | **For**: Next AI Session  
**Branch**: feature/multi-format-serialization-fuse | **Progress**: 18%

## Quick Start for Next Session

### What You Need to Know
1. **Phase 1 Status**: FlatBuffers backend COMPILED ✅, mkdwarfs blocked ⚠️
2. **Working Tools**: dwarfsck (2.0MB), dwarfsextract (2.2MB) 
3. **Blocker**: mkdwarfs needs `rewrite_filesystem` (likely Thrift-dependent)
4. **Strategy**: Proceed to Phase 2 (dual-format) to unblock mkdwarfs

### Essential Reading (in order)
1. `doc/PHASE_1_COMPLETION_STATUS_2025-11-22.md` - What was accomplished
2. `doc/IMPLEMENTATION_STATUS_TRACKER.md` - Current progress tracking
3. `.kilocode/rules/memory-bank/architecture.md` - System architecture
4. `.kilocode/rules/memory-bank/context.md` - Current work focus

### Immediate Task Decision
You have TWO options:

#### Option A: Debug mkdwarfs Now (1-2 hours)
```bash
# Investigate rewrite_filesystem dependency
grep -r "DWARFS_HAVE_THRIFT" src/utility/rewrite_filesystem.cpp
grep -r "thrift::metadata" src/utility/rewrite_filesystem.cpp
```
**Pros**: Completes Phase 1 fully  
**Cons**: May need Thrift anyway, delays Phase 2

#### Option B: Proceed to Phase 2 (RECOMMENDED)
Start Thrift backend isolation, which will:
- Enable dual-format builds
- Provide `rewrite_filesystem` via Thrift
- Unblock mkdwarfs naturally

**Pros**: More efficient, unblocks everything  
**Cons**: Defers Phase 1 validation

**Recommendation**: **Option B** - Proceed to Phase 2

---

## Phase 2: Thrift Backend Isolation (2-3 hours)

### Objective
Create `thrift_backend::` namespace with identical structure to `flatbuffers_backend::`, achieving complete format isolation.

### Step-by-Step Actions

#### Step 2.1: Create Thrift Backend Header (45 min)

**Task**: Create `include/dwarfs/reader/internal/metadata_types_thrift.h`

**Process**:
1. Copy structure from `metadata_types_flatbuffers.h`
2. Replace `flatbuffers_backend` → `thrift_backend`
3. Use Thrift Frozen2 types instead of FlatBuffers
4. Keep exact same public API

**Source code location**: Extract from existing `src/reader/internal/metadata_v2_thrift.cpp`

#### Step 2.2: Implement Thrift Backend (1 hour)

**Task**: Create `src/reader/internal/metadata_types_thrift.cpp`

**Implementation source**: Port from current Thrift-using code

#### Step 2.3: Isolate metadata_v2_thrift.cpp (45 min)

**Task**: Update `src/reader/internal/metadata_v2_thrift.cpp`

**Changes needed**:
- Add `namespace tb = thrift_backend;`
- Replace direct Frozen2 types with `tb::`
- Ensure no FlatBuffers code mixed in

#### Step 2.4: Update Build System (15 min)

**Task**: Modify `cmake/libdwarfs.cmake`

Add conditional Thrift backend compilation

#### Step 2.5: Write Tests (30 min)

**Task**: Create `test/metadata_types_thrift_test.cpp`

Match FlatBuffers test structure for API compatibility

#### Step 2.6: Build & Validate (15 min)

```bash
# Dual-format build
cmake -B build-dual \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
ninja -C build-dual
ctest --test-dir build-dual
```

---

## Phase 3: Factory Pattern Integration (2 hours)

### Step 3.1: Create Factory Class (1 hour)

**Files to create**:
- `include/dwarfs/reader/internal/metadata_v2_factory.h`
- `src/reader/internal/metadata_v2_factory.cpp`

**Factory responsibilities**:
1. Detect format from raw data
2. Create appropriate backend implementation
3. Handle error cases (unsupported format, missing backend)

#### Step 3.2: Update metadata_v2 (30 min)

**Task**: Create `src/reader/internal/metadata_v2.cpp`

Move constructor implementation to use factory

#### Step 3.3: Integration Tests (30 min)

**Files to create**:
- `test/metadata_v2_factory_test.cpp` - Factory tests
- `test/dual_format_integration_test.cpp` - End-to-end tests

---

## Phase 4: Code Cleanup (1 hour)

### Cleanup Tasks
1. Remove temporary conversion code (20 min)
2. Run clang-format on all files (10 min)
3. Run clang-tidy and fix issues (20 min)
4. Code review checklist (10 min)

---

## Phase 5: Testing & Validation (2 hours)

### Test Categories
1. Unit tests for both backends (30 min each)
2. Integration tests (30 min)
3. Performance benchmarks (30 min)
4. Memory leak testing (ASAN)
5. Thread safety testing (TSAN)

---

## Phase 6: Documentation (1.5 hours)

### Documentation Updates

#### Official Docs to Update
1. **README.adoc** (30 min)
   - Add metadata formats section
   - Migration guide
   - Build options

2. **doc/metadata-formats.md** (30 min - NEW)
   - Technical specifications
   - Performance comparison
   - Troubleshooting guide

3. **Tool Manuals** (30 min)
   - mkdwarfs.md: --format option
   - dwarfsck.md: format detection
   - dwarfs-format.md: format versioning

#### Documentation Cleanup

**Move to** `old-docs/phase-work/`:
- All `PHASE_*_2025-11-22.md` files
- All `OPTION_C_*.md` files
- All continuation prompts
- Temporary implementation docs

**Keep in** `doc/`:
- README.adoc (updated)
- metadata-formats.md (NEW)
- Tool manuals (updated)
- dwarfs-format.md (updated)
- Permanent architecture docs

---

## Architectural Principles

### 1. Pure Object-Oriented
- No procedural code
- Single responsibility per class
- Inheritance and polymorphism

### 2. MECE (Mutually Exclusive, Collectively Exhaustive)
- Backend namespaces don't overlap
- All format cases handled
- No gaps, no overlaps

### 3. Separation of Concerns
- `flatbuffers_backend::` - FlatBuffers only
- `thrift_backend::` - Thrift only
- Factory - Selection logic only
- Public API - Abstracts backend choice

### 4. Open/Closed Principle
- Open for extension (add new format = new backend)
- Closed for modification (existing code unchanged)

### 5. Minimal Code Guards
- Architecture > #ifdef
- Polymorphism > conditional compilation
- Guards only where necessary (headers, CMake, factory)

### 6. No Compromises on Testing
- Behavior must be correct
- No threshold lowering
- No corner cutting
- Comprehensive coverage

---

## Testing Philosophy

### Unit Tests
- Test ONE thing per test
- Test behavior, not implementation
- Cover all edge cases
- No mocking (use real objects)

### Integration Tests
- End-to-end workflows
- Real filesystem operations
- Cross-format compatibility
- Performance validation

### Test Coverage
- Minimum: 85% for new code
- Target: 95%+ for critical paths
- 100% for format detection logic

---

## Performance Requirements

### Acceptable Performance
- Read latency: Within 5% of baseline
- Memory usage: Within 10% of baseline
- CPU usage: Within 10% of baseline
- Format detection: <1ms overhead

### Benchmark Process
1. Create identical test images (both formats)
2. Run on same hardware
3. Multiple iterations (statistical significance)
4. Document trade-offs

---

## Risk Management

### Known Risks

| Risk | Mitigation | Status |
|------|-----------|--------|
| mkdwarfs linker issue | Proceed to Phase 2 | ACTIVE |
| Thrift isolation complexity | Incremental approach | MONITORING |
| Format incompatibility | Integration tests | PLANNED |
| Performance regression | Benchmarking | PLANNED |

### Contingency Plans

**If Thrift isolation fails**:
- Document blocker
- Investigate alternative architecture
- Consider format migration only (dropping Thrift read support)

**If performance unacceptable**:
- Profile hot paths
- Optimize backend accessors
- Consider lazy initialization

**If tests fail**:
- Debug systematically
- Fix root causes (no workarounds)
- May need architecture changes

---

## Timeline Estimates

### Optimistic: 6.5 hours ($52)
- Phase 2: 2h
- Phase 3: 1.5h
- Phase 4: 0.5h
- Phase 5: 1.5h
- Phase 6: 1h

### Realistic: 9.5 hours ($74)
- Phase 2: 3h
- Phase 3: 2h
- Phase 4: 1h
- Phase 5: 2h
- Phase 6: 1.5h

### Pessimistic: 13.5 hours ($106)
- Phase 2: 4h
- Phase 3: 3h
- Phase 4: 1.5h
- Phase 5: 3h
- Phase 6: 2h

**Recommended buffer**: +$20 for unknowns  
**Total projection**: $75-125 USD

---

## Lessons Learned (Phase 1)

### What Worked
- sed/awk for large file modifications
- Incremental compilation testing
- Clear namespace separation
- Format detection framework

### What Didn't Work
- edit_file on 2377-line files (multiple corruptions)
- Assuming backup files are clean
- Not validating edits immediately

### Improvements for Next Phases
1. **Use sed/awk for files >1000 lines**
2. **Verify file integrity after each edit**
3. **Keep multiple backup points**
4. **Test compilation more frequently**

---

## Next Session Checklist

### Before Starting
- [ ] Read all essential docs (listed above)
- [ ] Review Phase 1 completion status
- [ ] Understand current blocker (mkdwarfs)
- [ ] Decide: Option A (debug) vs Option B (Phase 2)

### During Work
- [ ] Follow architecture principles
- [ ] Test frequently
- [ ] Keep backups
- [ ] Update progress tracker

### Before Completing
- [ ] All tests passing
- [ ] Documentation updated
- [ ] Old docs moved
- [ ] Create continuation prompt

---

**Status**: Phase 1 core COMPLETE (95%), ready for Phase 2  
**Recommendation**: Proceed to Phase 2 for dual-format support  
**ETA**: 8-10 hours across Phases 2-6  
**Working directory**: `/Users/mulgogi/src/external/dwarfs`
