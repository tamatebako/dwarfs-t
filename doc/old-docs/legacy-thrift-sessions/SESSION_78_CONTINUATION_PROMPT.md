# Session 78: Complete Frozen2 Serialization - Continuation Prompt

**Start Here**: Complete port of Frozen2 serialization from dwarfs-rs

---

## Mission

Complete the Frozen2 serialization implementation to enable Homebrew v0.14.1 compatibility by porting the remaining ~1,450 lines from dwarfs-rs.

**Status from Session 77**:
- ✅ Complete schema system (100% functional)
- ✅ Bit writer infrastructure (production-ready)
- ⏳ Serializer stub created (needs full implementation)

---

## Quick Start

### Step 1: Review Session 77 Output (15 min)

```bash
# Read completion summary
cat doc/SESSION_77_COMPLETION_SUMMARY.md

# Read implementation plan
cat doc/SESSION_77_CONTINUATION_PLAN_PHASE2.md

# Review status tracker
cat doc/SESSION_78_IMPLEMENTATION_STATUS.md
```

### Step 2: Study dwarfs-rs Reference (30 min)

**MUST READ** before coding:

```bash
cd /Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata

# Complete serialization implementation
cat ser_frozen.rs

# Entry point
grep -A 20 "pub fn to_schema_and_bytes" ../metadata.rs
```

### Step 3: Implement Layout System (2 hours)

**File**: `src/metadata/legacy/frozen2_serializer.cpp`

Port from `ser_frozen.rs:19-257`:

1. **Layout enum** (Rust) → **Layout class hierarchy** (C++)
   ```cpp
   class Layout {
   public:
     virtual ~Layout() = default;
     virtual uint16_t byte_size() const = 0;
     virtual std::optional<uint16_t> finish() = 0;
   };
   
   class LayoutNone : public Layout { ... };
   class LayoutPrimitive : public Layout { uint16_t byte_size_; };
   class LayoutStruct : public Layout { std::vector<std::unique_ptr<Layout>> fields_; };
   class LayoutCollection : public Layout { std::unique_ptr<Layout> element_; };
   ```

2. **Layout builder methods**
   - `plan_layout(domain::metadata)` - Build layout tree
   - `finish()` - Optimize layouts (ser_frozen.rs:143-193)
   - `put_primitive_opt()` - Register primitive
   - `put_struct()` - Register struct
   - `put_collection()` - Register collection

3. **Schema conversion**
   - `cvt_layout()` - Layout → SchemaLayout (ser_frozen.rs:57-98)

### Step 4: Implement Value Serialization (3 hours)

Port from `ser_frozen.rs:511-857`:

1. **ValueSerializer class**
   ```cpp
   class ValueSerializer {
     FrozenBitWriter& writer_;
     Layout const* layout_;
     uint32_t base_;
     uint32_t inline_pos_;
   };
   ```

2. **Primitive serializers**
   - `serialize_bool()`
   - `serialize_u32()`
   - `serialize_u64()`
   - `serialize_bytes()` (distance + count pattern)

3. **Collection serializers**
   - `serialize_vector<T>()`
   - `serialize_optional<T>()`
   - `serialize_set<T>()`
   - `serialize_map<K,V>()`

### Step 5: Implement Type Handlers (3 hours)

For ALL domain types in [`domain::metadata`](include/dwarfs/metadata/domain/metadata.h):

```cpp
void serialize_metadata(domain::metadata const&, SchemaLayout const&);
void serialize_chunk(domain::chunk const&, SchemaLayout const&);
void serialize_directory(domain::directory const&, SchemaLayout const&);
void serialize_inode_data(domain::inode_data const&, SchemaLayout const&);
void serialize_dir_entry(domain::dir_entry const&, SchemaLayout const&);
void serialize_fs_options(domain::fs_options const&, SchemaLayout const&);
void serialize_string_table(domain::string_table const&, SchemaLayout const&);
void serialize_inode_size_cache(domain::inode_size_cache const&, SchemaLayout const&);
void serialize_history_entry(domain::history_entry const&, SchemaLayout const&);
// ... all other types
```

---

## Success Criteria

### Code Complete
- [ ] Layout system fully ported (~400 lines)
- [ ] Value serialization fully ported (~600 lines)
- [ ] ALL type handlers implemented (~450 lines)
- [ ] Builds without errors
- [ ] No compiler warnings

### Functionality
- [ ] Can generate schema from domain::metadata
- [ ] Can serialize metadata to bit-packed format
- [ ] Schema validates correctly
- [ ] Round-trip: metadata → layout → schema (correct)

### Testing (Minimal)
- [ ] Simple metadata serializes without crash
- [ ] Generated schema is valid
- [ ] Byte output is non-empty

---

## Critical Reference Files

**dwarfs-rs source** (port from):
- `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata/ser_frozen.rs` (COMPLETE FILE)
- `/Users/mulgogi/src/external/dwarfs-rs/dwarfs/src/metadata.rs:454-457` (entry point)

**Our code** (implement in):
- `src/metadata/legacy/frozen2_serializer.cpp` (currently 107-line stub)

**Our infrastructure** (already working):
- `include/dwarfs/metadata/legacy/frozen_schema.h` - Schema types
- `src/metadata/legacy/frozen_schema.cpp` - Validation
- `include/dwarfs/metadata/legacy/frozen_bit_writer.h` - Bit ops
- `src/metadata/legacy/frozen_bit_writer.cpp` - Bit packing

---

## Architecture Principles

### 1. Port Faithfully
- Follow dwarfs-rs structure exactly
- Match function names where possible
- Preserve algorithm logic

### 2. Use C++ Idioms
- `std::unique_ptr` for ownership
- `std::optional` for Option<T>
- `std::vector` for Vec<T>
- RAII for resource management

### 3. Error Handling
- Use exceptions for errors (not Result<T>)
- Provide clear error messages
- Validate inputs

### 4. Performance
- Minimize allocations
- Reuse buffers where possible
- Avoid unnecessary copies

---

## Time Budget

| Task | Duration | Cumulative |
|------|----------|------------|
| Review Session 77 | 15 min | 0:15 |
| Study dwarfs-rs | 30 min | 0:45 |
| Layout system | 2 hours | 2:45 |
| Value serialization | 3 hours | 5:45 |
| Type handlers | 3 hours | 8:45 |
| Testing | 1 hour | 9:45 |

**Total**: ~10 hours for complete implementation

---

## After Completion

Once frozen2_serializer.cpp is complete:

1. **Update memory bank** with completion status
2. **Test build**: `ninja -C build-fb-only`
3. **Create test image**: Try serializing simple metadata
4. **Start Session 79**: Implement Frozen2 deserializer
5. **Then integrate**: Wire into metadata_freezer.cpp
6. **Finally test**: Homebrew v0.14.1 compatibility

---

**Created**: 2026-01-05 13:50 HKT
**Session**: 78
**Goal**: Complete Frozen2 serialization (~1,450 lines)
**Next**: Session 79 - Frozen2 deserialization + integration + Homebrew testing