# Session 81: Debug and Fix Runtime Errors - Continuation Plan

**Created**: 2026-01-05
**Prerequisite**: Session 80 (templates fixed, builds successfully, tests fail at runtime)
**Goal**: Fix structural bugs in layout/serialization interaction and achieve Homebrew compatibility
**Estimated Time**: 2-3 hours (compressed from original 5-hour plan)

---

## Mission

Fix the runtime errors in Frozen2 serialization by addressing the architectural mismatch between:
1. How layouts are built (layout_builder)
2. How `finish()` modifies layouts (layout)
3. How serialization accesses layouts (value_serializer)

**Core Principle**: Prioritize architectural correctness over test passage. If tests fail because they have wrong expectations, we update the tests, not compromise the architecture.

---

## Current State (Session 80 Output)

### ✅ What Works
- **Build**: All 14 files compile, library links successfully
- **Templates**: All ~40 template deduction errors fixed
- **Architecture**: Clean modular OOP design (9 files, all <800 lines)

### ❌ What Fails
- **All tests**: 0/4 tests pass at runtime
- **Errors**:
  - "struct field count mismatch" (most common)
  - "LayoutCollection::byte_size() called before finish()"
  - "unexpected layout type"

### 🔍 Root Cause Hypothesis
After `finish()` is called, `LayoutCollection` objects are converted to `LayoutStruct` internally (via `converted_struct_`). However, the serializer may be accessing the original `LayoutCollection` pointer instead of the converted struct.

---

## Phase A: Root Cause Analysis via Comparison (1 hour)

### Task A.1: Compare with dwarfs-rs Implementation (30 min)

**Goal**: Understand how Rust implementation handles layout conversion

**Files to Review**:
1. `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs:28-55` - Main serialization flow
2. `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs:156-192` - Collection finish()
3. `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs:511-857` - Serialization

**Key Questions**:
- How does Rust handle Collection→Struct conversion after finish()?
- Does serialization use the original layout or the converted one?
- How are layout pointers tracked during serialization?

**Action**: Document differences in architecture/behavior

### Task A.2: Add Comprehensive Debug Logging (30 min)

**File**: `src/metadata/legacy/frozen2_value_serializer.cpp`

Add debug output in key methods:

```cpp
LayoutStruct const* Serializer::as_struct(size_t field_count) const {
  std::cerr << "=== as_struct() called ===" << std::endl;
  std::cerr << "Expected field count: " << field_count << std::endl;
  std::cerr << "Layout type: " << typeid(*layout_).name() << std::endl;

  if (layout_->is_none()) {
    std::cerr << "Layout is None, returning nullptr" << std::endl;
    return nullptr;
  }

  Layout const* actual_layout = layout_;

  auto const* coll = dynamic_cast<LayoutCollection const*>(actual_layout);
  if (coll) {
    std::cerr << "Layout is Collection, converting via to_struct()" << std::endl;
    actual_layout = coll->to_struct();
    std::cerr << "Converted layout type: " << typeid(*actual_layout).name() << std::endl;
  }

  auto const* st = dynamic_cast<LayoutStruct const*>(actual_layout);
  if (!st) {
    std::cerr << "ERROR: Not a struct layout!" << std::endl;
    throw std::logic_error("expected struct layout");
  }

  std::cerr << "Actual field count: " << st->fields().size() << std::endl;

  if (st->fields().size() != field_count) {
    std::cerr << "ERROR: Field count mismatch!" << std::endl;
    throw std::logic_error("struct field count mismatch");
  }

  return st;
}
```

**Run**: `./build-test/frozen2_serializer_tests --gtest_filter=Frozen2SerializerTest.SimpleStruct 2>&1 | tee debug.log`

**Analyze**: Review debug.log to understand what's happening

---

## Phase B: Architectural Fix (1 hour)

### Task B.1: Identify Core Architectural Issue (15 min)

Based on debug output and comparison with Rust, identify the exact architectural mismatch:
- Is the issue in how layouts store their state?
- Is the issue in how finish() modifies layouts?
- Is the issue in how serialization accesses layouts?

**Expected Finding**: The serializer receives pre-finish() layout pointers but needs post-finish() structures.

### Task B.2: Implement Architectural Solution (45 min)

**Option 1**: Make finish() return the final layout
```cpp
// frozen2_layout.h
class Layout {
  // Instead of modifying in-place, return the final layout
  virtual Layout const* finalize(std::optional<uint16_t>& byte_size) = 0;
};
```

**Option 2**: Always use to_struct() when accessing layouts
```cpp
// Helper function
Layout const* get_actual_layout(Layout const* layout) {
  if (auto coll = dynamic_cast<LayoutCollection const*>(layout)) {
    return coll->to_struct();
  }
  return layout;
}
```

**Option 3**: Store both original and converted in LayoutCollection
```cpp
// Already done, just need to ensure proper access
```

**Decision**: Choose Option 2 (minimal invasive, preserves architecture)

**Implementation**:
1. Create helper function `get_actual_layout()` in value_serializer.cpp
2. Use it in all methods that access layout structure
3. Ensure byte_size() calls use actual layout

---

## Phase C: Verification (30 min)

### Task C.1: Rebuild and Test (15 min)

```bash
ninja -C build-test frozen2_serializer_tests
./build-test/frozen2_serializer_tests
```

**Success Criteria**:
- All 4 tests pass
- No exceptions thrown
- Byte output matches expected values

### Task C.2: Validate Against dwarfs-rs (15 min)

For each test:
1. Compare byte output with expected from dwarfs-rs
2. Verify schema structure matches
3. Confirm field count calculations

**If tests fail**: Return to Phase B.1 with new insights

---

## Phase D: Integration and Documentation (30 min)

### Task D.1: Remove Debug Logging (5 min)

Clean up all debug output added in Phase A.2

### Task D.2: Update Documentation (25 min)

1. Update `.kilocode/rules/memory-bank/context.md`:
   - Change Legacy Thrift status to ✅ COMPLETE
   - Document final architecture

2. Create `doc/SESSION_81_COMPLETION_SUMMARY.md`:
   - What was fixed
   - Architectural solution applied
   - Test results
   - Performance notes

3. Move old session docs:
```bash
mkdir -p doc/old-sessions/session-77-80
mv doc/SESSION_77_*.md doc/old-sessions/session-77-80/
mv doc/SESSION_78_*.md doc/old-sessions/session-77-80/
mv doc/SESSION_79_*.md doc/old-sessions/session-77-80/
mv doc/SESSION_80_*.md doc/old-sessions/session-77-80/
```

---

## Success Criteria

### Code Quality ✓
- [ ] All files compile without warnings
- [ ] No architectural compromises made
- [ ] Clean separation of concerns maintained
- [ ] MECE structure preserved

### Tests ✓
- [ ] SimpleStruct test PASSES
- [ ] SmokeTest PASSES
- [ ] BytesTest PASSES
- [ ] CollectionTest PASSES
- [ ] All byte output matches dwarfs-rs exactly

### Architecture ✓
- [ ] Layout building logic correct
- [ ] finish() behavior correct
- [ ] Serialization access patterns correct
- [ ] No code guards used, pure architectural solution

### Documentation ✓
- [ ] Memory bank updated
- [ ] Completion summary created
- [ ] Old docs moved to old-sessions/

---

## Compressed Timeline

| Phase | Tasks | Time | Cumulative |
|-------|-------|------|------------|
| A | Root cause analysis | 1h | 1h |
| B | Architectural fix | 1h | 2h |
| C | Verification | 0.5h | 2.5h |
| D | Documentation | 0.5h | 3h |

**Total**: 3 hours for complete Frozen2 implementation

---

## Key Principles

1. **Architecture First**: Fix the root cause, not symptoms
2. **No Compromises**: If tests are wrong, fix tests, not architecture
3. **MECE**: Each component has single responsibility
4. **OOP**: Proper inheritance, polymorphism, encapsulation
5. **Extensibility**: Open/closed principle maintained

---

**Created**: 2026-01-05
**Status**: Ready to start
**Next**: Begin Phase A - Root cause analysis