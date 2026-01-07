# Session 82: Fix Serialization Logic Bugs - Continuation Plan

**Created**: 2026-01-05
**Prerequisite**: Session 81 (architecture fixed, 1/4 tests passing)
**Goal**: Fix remaining 3 serialization bugs and achieve 4/4 tests passing
**Estimated Time**: 1-2 hours

---

## Current State (Session 81 Output)

✅ **Architecture**: CORRECT - Collection→Struct replacement working
✅ **Build**: SUCCESS - All files compile
🟡 **Tests**: 1/4 passing (SimpleStruct ✓)

### Failing Tests

| Test | Error | Likely Cause |
|------|-------|--------------|
| CollectionTest | Distance = 32 (expected 12) | Distance calculated at wrong point |
| SmokeTest | Byte output mismatch | Optional/field serialization order |
| BytesTest | Byte output mismatch | Byte field serialization logic |

---

## Phase A: Debug Distance Calculation (30 min)

### Task A.1: Add Hex Dump to CollectionTest (10 min)

Modify test to show hex comparison:

```cpp
// test/metadata/legacy/frozen2_serializer_test.cpp
if (out != expected) {
  std::cerr << "\n=== ACTUAL BYTES ===" << std::endl;
  for (size_t i = 0; i < out.size(); ++i) {
    std::cerr << std::hex << std::setw(2) << std::setfill('0') 
              << (int)out[i] << " ";
    if ((i + 1) % 16 == 0) std::cerr << std::endl;
  }
  std::cerr << "\n=== EXPECTED BYTES ===" << std::endl;
  for (size_t i = 0; i < expected.size(); ++i) {
    std::cerr << std::hex << std::setw(2) << std::setfill('0')
              << (int)expected[i] << " ";
    if ((i + 1) % 16 == 0) std::cerr << std::endl;
  }
}
```

### Task A.2: Trace Distance Calculation (20 min)

Add debug to `serialize_vector()`:

```cpp
// include/dwarfs/metadata/legacy/frozen2_value_serializer.h:~155
uint32_t distance = this->distance();
std::cerr << "serialize_vector distance=" << distance 
          << " buf_size=" << buf_.size() 
          << " base=" << base_ << std::endl;
```

**Expected Finding**: Distance calculated before fields are serialized

**Fix**: Ensure correct timing - distance should be calculated AFTER inline fields

---

## Phase B: Fix Distance Bug (30 min)

### Task B.1: Review Rust Implementation

Check how Rust calculates distance:

```rust
// ser_frozen.rs:650-651
let distance = self.distance();
let mut s = self.reborrow().serialize_struct("seq", 3)?;
```

**Key**: Distance calculated BEFORE serialize_struct creates child serializer

### Task B.2: Fix C++ Implementation

Likely fix in `serialize_vector()`:

```cpp
// The distance must account for the inline struct fields (distance + count)
// Current (WRONG): distance = buf_.size() - base_
// Correct: distance should be relative to current inline_pos_

StructSerializer st(*this, st_layout->fields());
st.serialize_field(distance, [](auto& s, uint32_t v) { s.serialize_u32(v); });
st.serialize_field(len, [](auto& s, uint32_t v) { s.serialize_u32(v); });
// After these, inline_pos_ has advanced by 8 bytes
```

**Action**: Verify distance is calculated at the right point

### Task B.3: Test Fix

```bash
ninja -C build-test frozen2_serializer_tests
./build-test/frozen2_serializer_tests --gtest_filter=*CollectionTest
```

**Success Criteria**: Distance = 12, test passes

---

## Phase C: Fix Optional/Bytes Serialization (30 min)

### Task C.1: Debug SmokeTest

Add debug output to `serialize_optional()`:

```cpp
std::cerr << "serialize_optional has_value=" << opt.has_value() << std::endl;
```

**Compare with Rust**:
```rust
// ser_frozen.rs:610-628
fn serialize_none / serialize_some
```

### Task C.2: Debug BytesTest

Add debug to `serialize_bytes()`:

```cpp
std::cerr << "serialize_bytes distance=" << distance 
          << " len=" << len 
          << " omit=" << omit_elements << std::endl;
```

### Task C.3: Fix Bugs

Based on debug output, fix:
- Field ordering in struct serialization
- Skip logic for None fields
- Distance/length calculations

---

## Phase D: Verification (10 min)

### Task D.1: Run All Tests

```bash
ninja -C build-test frozen2_serializer_tests
./build-test/frozen2_serializer_tests
```

**Success Criteria**: All 4 tests pass

### Task D.2: Verify Byte-for-Byte Match

For each test, compare actual vs expected arrays element-by-element.

### Task D.3: Clean Up Debug Output

Remove all debug logging added in Phases A-C.

---

## Phase E: Documentation (20 min)

### Task E.1: Update Memory Bank

**File**: `.kilocode/rules/memory-bank/context.md`

Change line 83:
```markdown
| **Legacy Thrift** | NONE | 50 | `.dth` | None | ✅ **READY** |
```

Change line 113:
```markdown
**Status**: ✅ Production-ready for Homebrew v0.14.1 compatibility
```

### Task E.2: Create Completion Summary

**File**: `doc/SESSION_82_COMPLETION_SUMMARY.md`

Document:
- All 3 bugs fixed
- Byte-for-byte test verification
- Final test results (4/4 passing)
- Performance notes

### Task E.3: Move Old Docs

```bash
mkdir -p doc/old-sessions/session-77-81
mv doc/SESSION_77_*.md doc/old-sessions/session-77-81/
mv doc/SESSION_78_*.md doc/old-sessions/session-77-81/
mv doc/SESSION_79_*.md doc/old-sessions/session-77-81/
mv doc/SESSION_80_*.md doc/old-sessions/session-77-81/
mv doc/SESSION_81_*.md doc/old-sessions/session-77-81/
```

---

## Success Criteria

### Tests ✓
- [x] SimpleStruct PASSES
- [ ] SmokeTest PASSES
- [ ] BytesTest PASSES
- [ ] CollectionTest PASSES
- [ ] All byte output matches dwarfs-rs exactly

### Code Quality ✓
- [ ] No compiler warnings
- [ ] Clean architecture maintained
- [ ] No debug output left in code
- [ ] Proper separation of concerns

### Documentation ✓
- [ ] Memory bank updated
- [ ] Completion summary created
- [ ] Old docs archived

---

## Timeline Estimate

| Phase | Tasks | Time | Cumulative |
|-------|-------|------|------------|
| A | Debug distance | 0.5h | 0.5h |
| B | Fix distance | 0.5h | 1h |
| C | Fix optional/bytes | 0.5h | 1.5h |
| D | Verification | 0.17h | 1.67h |
| E | Documentation | 0.33h | 2h |

**Total**: 2 hours for complete Frozen2 implementation

---

**Created**: 2026-01-05
**Status**: Ready to start
**Next**: Begin Phase A - Debug distance calculation
