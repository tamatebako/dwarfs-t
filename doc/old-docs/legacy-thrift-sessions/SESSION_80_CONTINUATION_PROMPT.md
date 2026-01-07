# Session 80: Complete Frozen2 Implementation - Continuation Prompt

**Start Here**: Fix template errors, build, test, and complete Frozen2

---

## Quick Context

Session 79 created a **clean modular OOP architecture** for Frozen2 serialization:
- ✅ 9 files created (~3,000 lines)
- ✅ 4 separate components (all <800 lines)
- ❌ Compilation blocked by template errors

**Root Cause**: `std::function<>` cannot deduce from lambda types

---

## Step 1: Review Session 79 Output (5 min)

```bash
# Read Session 79 summary
cat doc/SESSION_79_COMPLETION_SUMMARY.md

# Read continuation plan
cat doc/SESSION_80_CONTINUATION_PLAN.md

# Read implementation status
cat doc/SESSION_80_IMPLEMENTATION_STATUS.md
```

---

## Step 2: Fix Template Signatures (1.5h)

### 2.1: Layout Builder Header (30 min)

**File**: `include/dwarfs/metadata/legacy/frozen2_layout_builder.h`

**Change ALL template functions** from `std::function<>` to generic callables:

```cpp
// BEFORE (lines 88-93):
template<typename T>
std::unique_ptr<Layout> build_optional(
    std::optional<T> const& opt,
    std::function<std::unique_ptr<Layout>(T const&)> builder);

// AFTER:
template<typename T, typename Builder>
std::unique_ptr<Layout> build_optional(
    std::optional<T> const& opt,
    Builder&& builder);
```

Apply to:
- `build_optional<T>` → `build_optional<T, Builder>`
- `build_vector<T>` → `build_vector<T, Builder>`
- `build_set<T>` → `build_set<T, Builder>`
- `build_map<K,V>` → `build_map<K, V, KBuilder, VBuilder>`

**Implementation remains in header** (templates must be in header).

### 2.2: Value Serializer Header (30 min)

**File**: `include/dwarfs/metadata/legacy/frozen2_value_serializer.h`

**Change ALL member function templates**:

```cpp
// BEFORE:
template<typename T>
void serialize_optional(
    std::optional<T> const& opt,
    std::function<void(Serializer&, T const&)> serializer);

// AFTER:
template<typename T, typename Func>
void serialize_optional(
    std::optional<T> const& opt,
    Func&& serializer);
```

Apply to:
- `Serializer::serialize_optional`
- `Serializer::serialize_vector`
- `Serializer::serialize_set`
- `Serializer::serialize_map`
- `StructSerializer::serialize_field`

**Use `std::forward<Func>(serializer)` when calling**.

### 2.3: Value Serializer Implementation (30 min)

**File**: `src/metadata/legacy/frozen2_value_serializer.cpp`

**Update template implementations** to use perfect forwarding:

```cpp
template<typename T, typename Func>
void Serializer::serialize_optional(
    std::optional<T> const& opt,
    Func&& serializer) {
  // ... existing code ...
  if (opt.has_value()) {
    st.serialize_field(*opt, std::forward<Func>(serializer));
  }
}
```

---

## Step 3: Fix DenseMap and Schema Access (30 min)

### 3.1: Fix DenseMap Construction

**File**: `src/metadata/legacy/frozen2_serializer.cpp` (line 67)

```cpp
// BEFORE:
schema.layouts = DenseMap<SchemaLayout>(std::move(layout_vec));

// AFTER:
std::vector<std::optional<SchemaLayout>> opt_layouts;
opt_layouts.reserve(layout_vec.size());
for (auto& layout : layout_vec) {
  opt_layouts.push_back(std::move(layout));
}
schema.layouts.raw_data() = std::move(opt_layouts);
```

### 3.2: Fix Root Schema Access

**File**: `src/metadata/legacy/frozen2_serializer.cpp` (lines 73-74)

```cpp
// BEFORE:
auto& root = schema.layouts.get(*root_id);
root.size = (root.bits + 7) / 8;

// AFTER:
auto root_opt = schema.layouts.get_mut(*root_id);
if (!root_opt) {
  throw std::runtime_error("root layout not found");
}
root_opt->size = (root_opt->bits + 7) / 8;
```

**Check DenseMap API** - may need `get_mut()` or access `raw_data()` directly.

---

## Step 4: Build and Verify (30 min)

```bash
# Clean build
rm -rf build-test

# Reconfigure
cmake -B build-test -GNinja \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=OFF \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF

# Build legacy metadata library
ninja -C build-test dwarfs_metadata_legacy

# Should output:
# [14/14] Linking CXX static library libdwarfs_metadata_legacy.a
# Build succeeded!
```

---

## Step 5: Port Tests (1.5h)

### 5.1: Create Test File (60 min)

**File**: `test/metadata/legacy/frozen2_serializer_test.cpp` (NEW)

Port all 3 tests verbatim from `ser_frozen.rs:859-961`:

```cpp
#include <gtest/gtest.h>
#include "dwarfs/metadata/legacy/frozen2_serializer.h"

namespace dwarfs::metadata::legacy::test {

TEST(Frozen2SerializerTest, SmokeTest) {
  // Port ser_frozen.rs:864-886
  domain::metadata meta;
  auto opts = domain::fs_options{};
  opts.mtime_only = true;
  opts.time_resolution_sec = 42;
  meta.options = opts;

  auto [schema, out] = Frozen2Serializer::serialize(meta);

  EXPECT_EQ(schema.file_version, 1);
  EXPECT_TRUE(schema.relax_type_checks);

  std::vector<uint8_t> expected = {
    1,           // options.is_some = true
    1,           // options.inner.mtime_only = true
    1,           // options.inner.time_resolution.is_some = true
    42, 0, 0, 0, // options.inner.time_resolution.inner = 42
  };
  EXPECT_EQ(out, expected);
}

TEST(Frozen2SerializerTest, BytesTest) {
  // Port ser_frozen.rs:888-915
  // TODO: Complete
}

TEST(Frozen2SerializerTest, CollectionTest) {
  // Port ser_frozen.rs:917-960
  // TODO: Complete
}

} // namespace
```

### 5.2: Add to CMake (15 min)

**File**: `cmake/metadata_serialization.cmake` (after line 283)

```cmake
# Frozen2 serializer tests
add_executable(frozen2_serializer_tests
  test/metadata/legacy/frozen2_serializer_test.cpp
)

target_link_libraries(frozen2_serializer_tests
  PRIVATE
    dwarfs_metadata_legacy
    GTest::gtest_main
)

add_test(
  NAME frozen2_serializer_tests
  COMMAND frozen2_serializer_tests
)
```

### 5.3: Run Tests (15 min)

```bash
ninja -C build-test frozen2_serializer_tests
./build-test/frozen2_serializer_tests --gtest_filter='*'
```

---

## Step 6: Verify Integration (30 min)

If mkdwarfs builds in this configuration, test:

```bash
# Create test data
mkdir -p /tmp/frozen2-test
echo "test" > /tmp/frozen2-test/file.txt

# Create image (if tool available)
mkdwarfs -i /tmp/frozen2-test -o /tmp/test.dth --metadata-format=legacy-thrift

# Check with dwarfsck (if available)
dwarfsck -i /tmp/test.dth
```

---

## Step 7: Update Documentation (30 min)

### 7.1: Update Memory Bank

**File**: `.kilocode/rules/memory-bank/context.md`

Update Legacy Thrift status from "🟡 FROZEN2 NEEDED" to "✅ COMPLETE".

### 7.2: Create Completion Summary

**File**: `doc/SESSION_80_COMPLETION_SUMMARY.md`

Document:
- Modular architecture (9 files)
- Template fixes applied
- Test results
- Integration status
- Files created/modified
- Time spent

### 7.3: Move Old Docs

```bash
mkdir -p doc/old-sessions/session-77-79
mv doc/SESSION_77_*.md doc/old-sessions/session-77-79/
mv doc/SESSION_78_*.md doc/old-sessions/session-77-79/
mv doc/SESSION_79_*.md doc/old-sessions/session-77-79/
```

---

## Success Criteria

### Code Quality ✓
- [ ] All files compile
- [ ] No warnings
- [ ] Modular design (all <800 lines)
- [ ] Clean OOP architecture

### Tests ✓
- [ ] smoke test PASSES
- [ ] bytes test PASSES
- [ ] collection test PASSES
- [ ] Byte-for-byte match with dwarfs-rs

### Integration ✓
- [ ] Can create legacy Thrift images
- [ ] Homebrew v0.14.1 compatible (if testable)
- [ ] No regressions

### Documentation ✓
- [ ] Memory bank updated
- [ ] Completion summary created
- [ ] Old session docs archived

---

## Common Issues & Solutions

### Issue: Lambda type deduction

**Solution**: Use forwarding references with template template parameters:
```cpp
template<typename T, typename Builder>
auto build_optional(std::optional<T> const& opt, Builder&& builder)
    -> std::unique_ptr<Layout>;
```

### Issue: DenseMap construction

**Solution**: Use `raw_data()` method:
```cpp
std::vector<std::optional<SchemaLayout>> vec;
for (auto& layout : layouts) {
  vec.push_back(std::move(layout));
}
schema.layouts.raw_data() = std::move(vec);
```

### Issue: Optional member access

**Solution**: Check validity before accessing:
```cpp
auto opt = schema.layouts.get(*root_id);
if (opt) {
  opt->size = (opt->bits + 7) / 8;
}
```

---

**Created**: 2026-01-05 17:58 HKT
**Session**: 80
**Goal**: Complete Frozen2 + tests
**Time**: 4-5 hours
**Next**: Begin template fixes