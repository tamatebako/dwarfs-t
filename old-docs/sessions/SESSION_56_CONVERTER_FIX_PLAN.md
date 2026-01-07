# Session 56: Thrift Converter Data Corruption Fix - CRITICAL

**Date**: 2025-12-30
**Severity**: CRITICAL - Blocks v0.14.1 compatibility
**Requirement**: MUST maintain dual-mode capability (FlatBuffers + Thrift)

---

## Quick Start for Next Session

Run these commands immediately to continue Phase 1 diagnostics:

```bash
cd /Users/mulgogi/src/external/dwarfs

# 1. Extract metadata JSON from both files
./build/dwarfsck --export-metadata=/tmp/homebrew-meta.json -i /tmp/homebrew-thrift.dwarfs
./build/dwarfsck --export-metadata=/tmp/local-meta.json -i /tmp/compat-test.dwarfs

# 2. Compare metadata
diff -u /tmp/homebrew-meta.json /tmp/local-meta.json | tee /tmp/metadata-diff.txt
cat /tmp/metadata-diff.txt

# 3. Analyze the differences
# Look for missing/corrupted fields in our output vs Homebrew
```

**Expected**: You'll see exactly which metadata fields are corrupted.

**Then**: Create round-trip tests in `test/metadata/converter_roundtrip_test.cpp`

---

## Problem Statement

**Root Cause**: Round-trip conversion Thrift → domain → Thrift corrupts metadata, making files unreadable by Homebrew dwarfs 0.14.1.

**Evidence**:
- Homebrew v0.14.1 metadata: 111 bytes
- Our build metadata: 85 bytes
- **26-byte corruption** = data loss in conversion

**Error**: `unexpected data size in names: 8 != 4306993742` (4.3 GB garbage)

---

## Architecture Requirement

**MANDATORY**: Dual-mode serialization must work:
```
┌─────────────────────────────────────────────┐
│  Domain Model (Format-Agnostic)             │
└──────────────┬──────────────────────────────┘
               │
     ┌─────────┴─────────┐
     ▼                   ▼
┌──────────┐      ┌──────────┐
│ Thrift   │      │FlatBuffers│
│ Writer   │      │ Writer    │
└──────────┘      └──────────┘
     │                   │
     ▼                   ▼
Both must produce VALID output readable by v0.14.1 (Thrift) / modern readers (FB)
```

---

## Phase 1: Diagnostic Deep Dive (2-3 hours)

### Step 1.1: Binary Comparison Analysis
- **Task**: Compare byte-for-byte Homebrew vs our Thrift output
- **Method**: Create minimal test case (1 file), dump metadata sections
- **Goal**: Identify EXACT byte offset where corruption occurs
- **Deliverable**: Corruption point map showing which field(s) are wrong

### Step 1.2: Converter Audit
- **Task**: Audit ALL converter functions in `cpp_thrift_converter.cpp`
- **Focus Areas**:
  - `to_thrift(string_table)` - Lines 89-100
  - `from_thrift(string_table)` - Inverse function
  - `to_thrift(metadata)` - Lines 243-321
  - `from_thrift(metadata)` - Lines 91-320+
- **Method**: For each converter, verify:
  1. All fields copied correctly
  2. Optional fields handled properly
  3. No endianness issues
  4. No size/offset calculation errors
- **Deliverable**: List of suspect converters

### Step 1.3: Round-Trip Test
- **Task**: Create isolated unit test for each converter
- **Method**: For each type (chunk, directory, string_table, etc.):
  ```cpp
  Thrift original = create_test_data();
  Domain intermediate = from_thrift(original);
  Thrift roundtrip = to_thrift(intermediate);
  ASSERT_EQ(original, roundtrip);  // Must be identical
  ```
- **Goal**: Identify which converter(s) corrupt data
- **Deliverable**: Failing test cases showing exact corruption

---

## Phase 2: Root Cause Isolation (1-2 hours)

### Step 2.1: Identify Corrupted Fields
- **Task**: Use Phase 1 diagnostics to pinpoint exact fields
- **Likely Suspects** (based on error message):
  1. `string_table.buffer` size field
  2. `string_table.index` vector
  3. `string_table.packed_index` (FSST compression)
  4. Optional field handling (`has_value()` vs actual data)

### Step 2.2: Converter Logic Review
- **Task**: For each corrupted field, review converter logic
- **Check**:
  - Are we using deprecated Thrift APIs? (e.g., `has_value()`)
  - Are we copying pointers vs values?
  - Are we handling empty vectors correctly?
  - Are we preserving Thrift field IDs?

---

## Phase 3: Fix Implementation (2-4 hours)

### Step 3.1: Fix Converters
- **Task**: Fix identified bugs in converter functions
- **Approach**:
  1. Replace deprecated `has_value()` with `is_non_optional_field_set_manually_or_by_serializer()`
  2. Fix any pointer/reference issues
  3. Ensure all vector sizes preserved
  4. Handle optional fields correctly

### Step 3.2: Add Round-Trip Tests
- **Task**: Add comprehensive round-trip tests for ALL types
- **Location**: `test/metadata/converter_roundtrip_test.cpp` (new file)
- **Coverage**:
  - `chunk` type
  - `directory` type
  - `string_table` type (with/without FSST)
  - `metadata` type (full metadata object)
  - `inode_data` type
  - `dir_entry` type

### Step 3.3: Integration Testing
- **Task**: Test full mkdwarfs → dwarfsck → extract workflow
- **Test Cases**:
  1. Create with `--format=thrift`, read with Homebrew 0.14.1
  2. Create with Homebrew 0.14.1, read with our build
  3. Create with `--format=flatbuffers`, read with our build
  4. Recompress Thrift → Thrift (domain round-trip)
  5. Recompress FlatBuffers → Thrift (cross-format)

---

## Phase 4: Validation & Regression Prevention (1 hour)

### Step 4.1: Compatibility Test Suite
- **Task**: Create automated compatibility tests
- **Location**: `test/compatibility/thrift_v0141_test.cpp`
- **Tests**:
  - Write Thrift, verify readable by v0.14.1 binary (if available)
  - Round-trip preservation for all data types
  - Cross-format conversion accuracy

### Step 4.2: CI Integration
- **Task**: Add to GitHub Actions
- **Check**: Thrift round-trip tests run on every PR

---

## Success Criteria

✅ **Phase 1 Complete** when:
- Exact corruption point identified
- Failing unit tests for corrupted converters

✅ **Phase 2 Complete** when:
- Root cause of each corruption documented
- Fix strategy defined

✅ **Phase 3 Complete** when:
- All converter round-trip tests PASS
- Integration tests PASS

✅ **ISSUE RESOLVED** when:
1. Homebrew 0.14.1 can read Thrift files created by our build
2. Our build can read Thrift files created by Homebrew 0.14.1
3. FlatBuffers mode still works perfectly
4. Automated tests prevent future regressions

---

## Timeline Estimate

| Phase | Duration | Priority |
|-------|----------|----------|
| Phase 1: Diagnostic | 2-3 hours | CRITICAL |
| Phase 2: Root Cause | 1-2 hours | CRITICAL |
| Phase 3: Fix + Test | 2-4 hours | CRITICAL |
| Phase 4: Validation | 1 hour | HIGH |
| **TOTAL** | **6-10 hours** | **CRITICAL** |

---

## Key Principles

1. **NO SHORTCUTS**: Fix root cause, not symptoms
2. **DUAL-MODE MANDATORY**: Both Thrift and FlatBuffers must work
3. **BACKWARD COMPATIBLE**: v0.14.1 must read our Thrift files
4. **FORWARD COMPATIBLE**: We must read v0.14.1 Thrift files
5. **REGRESSION PREVENTION**: Automated tests for all conversions

---

## Files to Modify

**Converters** (fix bugs):
- `src/metadata/converters/cpp_thrift_converter.cpp` - PRIMARY FIX LOCATION

**Tests** (add coverage):
- `test/metadata/converter_roundtrip_test.cpp` - NEW FILE
- `test/compatibility/thrift_v0141_test.cpp` - NEW FILE

**Documentation** (update after fix):
- `doc/HOMEBREW_COMPATIBILITY_ISSUE.md` - Resolution summary
- Memory bank context - Dual-mode requirement

---

**Next Session**: Run the Quick Start commands above, then proceed to Phase 1.2