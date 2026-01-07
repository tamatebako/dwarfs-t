# Session 82: Fix Serialization Logic Bugs - Continuation Prompt

**Start Here**: Fix the 3 remaining serialization bugs to achieve 4/4 tests passing

---

## Quick Context

Session 81 achieved:
- ✅ Fixed fundamental architecture bug (Collection→Struct replacement)
- ✅ 1/4 tests passing (SimpleStruct)
- ❌ 3 tests failing with serialization logic bugs

**Your Mission**: Fix distance calculation, optional serialization, and bytes serialization bugs

---

## Step 1: Read Session 81 Output (5 min)

```bash
# Read completion summary
cat doc/SESSION_81_COMPLETION_SUMMARY.md

# Read continuation plan
cat doc/SESSION_82_CONTINUATION_PLAN.md
```

**Key Takeaway**: Architecture is CORRECT. Only serialization logic bugs remain.

---

## Step 2: Run Tests to See Current Failures (5 min)

```bash
cd /Users/mulgogi/src/external/dwarfs
ninja -C build-test frozen2_serializer_tests
./build-test/frozen2_serializer_tests
```

**Expected Output**: 1/4 passing (SimpleStruct ✓)

**Failures**:
- CollectionTest: Distance = 32 (expected 12)
- SmokeTest: Byte mismatch
- BytesTest: Byte mismatch

---

## Step 3: Debug CollectionTest Distance (30 min)

### 3.1: Add Hex Dump

Edit `test/metadata/legacy/frozen2_serializer_test.cpp` at line 131:

```cpp
EXPECT_EQ(out, expected);

// ADD THIS BLOCK:
if (out != expected) {
  std::cerr << "\n=== ACTUAL (size=" << out.size() << ") ===" << std::endl;
  for (size_t i = 0; i < out.size() && i < 32; ++i) {
    fprintf(stderr, "%02x ", out[i]);
    if ((i + 1) % 16 == 0) fprintf(stderr, "\n");
  }
  std::cerr << "\n=== EXPECTED (size=" << expected.size() << ") ===" << std::endl;
  for (size_t i = 0; i < expected.size(); ++i) {
    fprintf(stderr, "%02x ", expected[i]);
    if ((i + 1) % 16 == 0) fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
}
```

### 3.2: Add Distance Debug

Edit `include/dwarfs/metadata/legacy/frozen2_value_serializer.h` at line ~155:

```cpp
uint32_t distance = this->distance();
uint32_t len = static_cast<uint32_t>(vec.size());

// ADD THIS:
std::cerr << "serialize_vector: distance=" << distance 
          << " len=" << len 
          << " buf_size=" << buf_.size()
          << " base=" << base_
          << " inline_pos=" << inline_pos_ << std::endl;
```

### 3.3: Run CollectionTest

```bash
ninja -C build-test frozen2_serializer_tests
./build-test/frozen2_serializer_tests --gtest_filter=*CollectionTest 2>&1 | tail -50
```

**Analyze**:
- What is distance value?
- What is buf_size when distance is calculated?
- Compare with expected value (12)

### 3.4: Fix Distance Calculation

**Rust Reference** (ser_frozen.rs:650-656):
```rust
let distance = self.distance();  // Calculated BEFORE inline fields
let mut s = self.reborrow().serialize_struct("seq", 3)?;
let elem_layout = s.fields.get(2).unwrap_or(&Layout::None);
s.serialize_field("distance", &distance)?;  // Written to buffer
s.serialize_field("count", &len)?;          // Written to buffer
s.end()?;                                   // Inline fields done
```

**Key**: Distance is calculated BEFORE the struct's inline fields are written

**Your Fix**: Ensure distance calculation happens at the correct point in `serialize_vector()`

---

## Step 4: Fix Optional Serialization (SmokeTest) (20 min)

### 4.1: Add Debug to serialize_optional

Edit `include/dwarfs/metadata/legacy/frozen2_value_serializer.h` at line ~135:

```cpp
template<typename T, typename Func>
void Serializer::serialize_optional(
    std::optional<T> const& opt,
    Func&& serializer) {
  auto st_layout = as_struct(2);
  if (!st_layout) return;

  // ADD THIS:
  std::cerr << "serialize_optional: has_value=" << opt.has_value() << std::endl;

  StructSerializer st(*this, st_layout->fields());
  st.serialize_field(opt.has_value(), [](auto& s, bool v) { s.serialize_bool(v); });

  if (opt.has_value()) {
    st.serialize_field(*opt, std::forward<Func>(serializer));
  } else {
    st.skip_field();
  }
}
```

### 4.2: Run SmokeTest

```bash
./build-test/frozen2_serializer_tests --gtest_filter=*SmokeTest 2>&1 | tail -40
```

**Analyze**: Compare debug output with Rust implementation (ser_frozen.rs:610-628)

### 4.3: Fix Bug

Based on debug output, fix field ordering or skipping logic

---

## Step 5: Fix Bytes Serialization (BytesTest) (20 min)

### 5.1: Add Debug to serialize_bytes

Edit `src/metadata/legacy/frozen2_value_serializer.cpp` at line ~135:

```cpp
uint32_t distance = this->distance();
uint32_t len = static_cast<uint32_t>(v.size());

// ADD THIS:
std::cerr << "serialize_bytes: distance=" << distance
          << " len=" << len
          << " omit_elements=" << (st_layout->fields()[0]->is_none()) << std::endl;
```

### 5.2: Run BytesTest

```bash
./build-test/frozen2_serializer_tests --gtest_filter=*BytesTest 2>&1 | tail -40
```

### 5.3: Fix Bug

Ensure distance and length are calculated/written correctly

---

## Step 6: Verify All Tests Pass (10 min)

```bash
ninja -C build-test frozen2_serializer_tests
./build-test/frozen2_serializer_tests
```

**Success Criteria**: 
```
[  PASSED  ] 4 tests.
```

---

## Step 7: Clean Up Debug Output (10 min)

Remove all debug logging added in steps 3-5:
- Hex dump from frozen2_serializer_test.cpp
- Distance debug from serialize_vector()
- Optional debug from serialize_optional()
- Bytes debug from serialize_bytes()

Rebuild and verify tests still pass:
```bash
ninja -C build-test frozen2_serializer_tests
./build-test/frozen2_serializer_tests
```

---

## Step 8: Update Documentation (20 min)

### 8.1: Update Memory Bank

Edit `.kilocode/rules/memory-bank/context.md`:

Line 83:
```markdown
| **Legacy Thrift** | NONE | 50 | `.dth` | None | ✅ **READY** |
```

Line 113:
```markdown
**Status**: ✅ Production-ready for Homebrew v0.14.1 compatibility
```

### 8.2: Create Completion Summary

Create `doc/SESSION_82_COMPLETION_SUMMARY.md`:
- List all 3 bugs fixed
- Show final test results (4/4 passing)
- Document performance metrics
- Note Homebrew compatibility achieved

### 8.3: Archive Old Docs

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

✅ **All 4 tests passing**:
- SimpleStruct
- SmokeTest
- BytesTest
- CollectionTest

✅ **Byte-for-byte match** with dwarfs-rs expected output

✅ **No debug output** left in code

✅ **Documentation complete**:
- Memory bank updated
- Completion summary created
- Old docs archived

---

## Common Issues & Solutions

### Issue: Distance still wrong after fix

**Solution**: Check that distance is calculated BEFORE StructSerializer writes inline fields

### Issue: Optional serialization produces wrong bytes

**Solution**: Verify field order matches Rust (is_some first, then inner value)

### Issue: Bytes test fails on all-zero input

**Solution**: Check omit_elements logic - zero bytes should NOT be written to buffer

---

## Time Budget

- Debug CollectionTest: 30 min
- Fix Optional: 20 min
- Fix Bytes: 20 min
- Verification: 10 min
- Documentation: 20 min
- **Total**: 100 minutes (1.67 hours)

---

**Created**: 2026-01-05
**Session**: 82
**Goal**: Fix 3 serialization bugs, achieve 4/4 tests passing
**Next**: Begin Step 1 - Read context
