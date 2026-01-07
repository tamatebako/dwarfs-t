# Session 88: CompactProtocol Serialization - Continuation Prompt

**Start Here**: Implement `ModernThriftSerializer` class using `apache::thrift::CompactSerializer`

---

## Quick Context

Session 87 achieved:
- ✅ Thrift schema created (`thrift/metadata_modern.thrift`)
- ✅ Converters implemented (domain ↔ thrift)
- ✅ Unit tests passing (6 test cases)
- ✅ CMake configured for fbthrift
- ⏳ Serialization pending

**Your Mission**: Implement the ModernThriftSerializer class that uses CompactProtocol

---

## Implementation Steps

### Step 1: Implement Serializer (90 min)

**File**: `src/metadata/serialization/modern_thrift_serializer.cpp`

**Key Methods**:
1. `serialize()`: domain → thrift → CompactProtocol bytes + magic {0x82, 0x21}
2. `deserialize()`: verify magic → CompactProtocol bytes → thrift → domain
3. Interface methods: `get_format_name()`, `get_priority()`, etc.

### Step 2: Register with Registry (15 min)

Update `src/metadata/serialization/serializer_registry.cpp`

### Step 3: Write Unit Tests (40 min)

**File**: `test/metadata/serialization/modern_thrift_serializer_test.cpp`

Test cases:
- Simple metadata serialization
- Complex metadata
- Magic byte verification
- Registry integration

### Step 4: Build & Test (20 min)

```bash
cmake -B build -GNinja \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON

ninja -C build modern_thrift_converter_tests
./build/modern_thrift_converter_tests
```

---

## Time Budget

- Implementation: 90 min
- Registry: 15 min
- Tests: 40 min
- Build: 20 min
- **Total**: ~3 hours

---

## Success Criteria

✅ Serializer implements all interface methods
✅ Magic bytes {0x82, 0x21} handled correctly
✅ Registry integration working (priority 100)
✅ All tests passing

---

## Next Session

Session 89: Testing & Validation

Read: `doc/SESSION_89_CONTINUATION_PROMPT.md` (will be created)

---

**Created**: 2026-01-06
**Session**: 88
**Goal**: Implement CompactProtocol serialization
