# Frozen2 Modular Architecture

**Created**: 2026-01-05 (Session 79)
**Status**: Architecture complete, templates need fixes
**Purpose**: Homebrew v0.14.1 backward compatibility

---

## Design Overview

Frozen2 serialization is implemented as **4 separate modular components** orchestrated by a clean entry point. Each component has single responsibility and is independently testable.

```
┌──────────────────────────────────────────────────┐
│         frozen2_serializer.cpp (85 lines)        │
│              Entry Point Orchestration           │
└───────┬──────────────┬──────────────┬────────────┘
        │              │              │
        ▼              ▼              ▼
┌───────────────┐ ┌────────────┐ ┌─────────────────┐
│ Layout System │ │ Schema     │ │ Value           │
│  (404 lines)  │ │ Converter  │ │ Serializer      │
│               │ │ (161 lines)│ │  (856 lines)    │
│ Layout        │ │            │ │                 │
│ LayoutNone    │ │ cvt_layout │ │ Serializer      │
│LayoutPrimitive│ │            │ │StructSerializer │
│LayoutStruct   │ │            │ │                 │
│LayoutCollect  │ │            │ │                 │
└───────┬───────┘ └────────────┘ └─────────────────┘
        │
        ▼
┌─────────────────────────┐
│   Layout Builders       │
│     (446 lines)         │
│                         │
│ build_metadata()        │
│ build_chunk()           │
│ build_directory()       │
│ build_optional<T>()     │
│ build_vector<T>()       │
│ etc.                    │
└─────────────────────────┘
```

---

## Component 1: Layout System (404 lines)

**Purpose**: Define layout structure for all types

**Files**:
- [`include/dwarfs/metadata/legacy/frozen2_layout.h`](../include/dwarfs/metadata/legacy/frozen2_layout.h) (147 lines)
- [`src/metadata/legacy/frozen2_layout.cpp`](../src/metadata/legacy/frozen2_layout.cpp) (128 lines)

**Classes**:
- `Layout` - Abstract base class
- `LayoutNone` - Empty layout (no data)
- `LayoutPrimitive` - Fixed-size primitive
- `LayoutStruct` - Struct with fields
- `LayoutCollection` - Collection (converts to Struct after finish())

**Key Methods**:
- `finish()` - Optimize and validate layout, returns byte size
- `byte_size()` - Get size in bytes
- `is_none()` - Check if empty

**Reference**: dwarfs-rs `ser_frozen.rs:100-257`

---

## Component 2: Layout Builders (446 lines)

**Purpose**: Build Layout objects from domain types

**Files**:
- [`include/dwarfs/metadata/legacy/frozen2_layout_builder.h`](../include/dwarfs/metadata/legacy/frozen2_layout_builder.h) (181 lines)
- [`src/metadata/legacy/frozen2_layout_builder.cpp`](../src/metadata/legacy/frozen2_layout_builder.cpp) (265 lines)

**Functions**:

Primitive builders:
- `build_bool(bool)` → Layout
- `build_u32(uint32_t)` → Layout
- `build_u64(uint64_t)` → Layout
- `build_bytes(string)` → Layout

Generic builders (templates):
- `build_optional<T>(optional<T>, builder)` → Layout
- `build_vector<T>(vector<T>, builder)` → Layout
- `build_set<T>(set<T>, builder)` → Layout
- `build_map<K,V>(map<K,V>, kbuilder, vbuilder)` → Layout

Domain type builders:
- `build_chunk(chunk)` → Layout
- `build_directory(directory)` → Layout
- `build_inode_data(inode_data)` → Layout
- `build_dir_entry(dir_entry)` → Layout
- `build_fs_options(fs_options)` → Layout
- `build_string_table(string_table)` → Layout
- `build_inode_size_cache(inode_size_cache)` → Layout
- `build_history_entry(history_entry)` → Layout
- `build_metadata(metadata)` → Layout (top-level, 36 fields)

**Reference**: dwarfs-rs `ser_frozen.rs:195-256` + domain builders

---

## Component 3: Schema Converter (161 lines)

**Purpose**: Convert Layout tree to flat SchemaLayout vector

**Files**:
- [`include/dwarfs/metadata/legacy/frozen2_schema_converter.h`](../include/dwarfs/metadata/legacy/frozen2_schema_converter.h) (52 lines)
- [`src/metadata/legacy/frozen2_schema_converter.cpp`](../src/metadata/legacy/frozen2_schema_converter.cpp) (109 lines)

**Function**:
- `cvt_layout(Layout*, vector<SchemaLayout>&)` → optional<int16_t>
  - Recursive conversion
  - Layout deduplication
  - Field offset calculation (negative for bit offsets)
  - Returns layout ID in schema vector

**Reference**: dwarfs-rs `ser_frozen.rs:57-98`

---

## Component 4: Value Serializer (856 lines)

**Purpose**: Serialize domain values to bit-packed bytes

**Files**:
- [`include/dwarfs/metadata/legacy/frozen2_value_serializer.h`](../include/dwarfs/metadata/legacy/frozen2_value_serializer.h) (224 lines)
- [`src/metadata/legacy/frozen2_value_serializer.cpp`](../src/metadata/legacy/frozen2_value_serializer.cpp) (632 lines)

**Classes**:

`Serializer`:
- Manages byte buffer and positions
- Primitive serializers: `serialize_bool/u32/u64/bytes()`
- Generic serializers: `serialize_optional/vector/set/map<T>()`
- Domain serializers: `serialize_chunk/directory/...()` (9 types)
- Top-level: `serialize_metadata()`

`StructSerializer`:
- Helper for field iteration
- `serialize_field(value, serializer)` - serialize one field
- `skip_field()` - skip empty field
- Tracks inline_pos automatically

**Reference**: dwarfs-rs `ser_frozen.rs:511-857`

---

## Entry Point (85 lines)

**File**: [`src/metadata/legacy/frozen2_serializer.cpp`](../src/metadata/legacy/frozen2_serializer.cpp)

**Function**: `Frozen2Serializer::serialize(metadata)` → (Schema, bytes)

**Pipeline**:
1. Build layout tree (`build_metadata()`)
2. Finalize layout (`layout->finish()`)
3. Convert to schema (`cvt_layout()`)
4. Serialize values (`Serializer::serialize_metadata()`)
5. Return (Schema, bytes)

**Reference**: dwarfs-rs `ser_frozen.rs:28-55`

---

## Data Flow

```
domain::metadata
      ↓
build_metadata()  ──→ Layout tree
      ↓
finish()          ──→ Optimized layout + size
      ↓
cvt_layout()      ──→ Schema (DenseMap<SchemaLayout>)
      ↓
serialize_metadata() ──→ Frozen bytes (bit-packed)
      ↓
(Schema, bytes)
```

---

## File Size Summary

| Component | Files | Header | Impl | Total |
|-----------|-------|--------|------|-------|
| Layout System | 2 | 147 | 128 | 275 |
| Layout Builders | 2 | 181 | 265 | 446 |
| Schema Converter | 2 | 52 | 109 | 161 |
| Value Serializer | 2 | 224 | 632 | 856 |
| Entry Point | 1 | - | 85 | 85 |
| **Total** | **9** | **604** | **1,219** | **1,823** |

**All files under 700 lines** ✅

---

## Dependencies

**Internal** (within legacy metadata):
- `frozen_schema.h` - Schema data structures
- `frozen_schema_serializer.h` - Thrift CompactProtocol
- `frozen_bit_writer.h` - Bit-level operations

**Domain Types**:
- `domain/metadata.h` - Top-level metadata
- `domain/chunk.h`, `domain/directory.h`, etc. - Individual types

**Standard Library**:
- `<memory>` - smart pointers
- `<vector>`, `<map>`, `<set>`, `<optional>` - containers
- `<functional>` - std::function (to be removed)

---

## Known Issues (Session 79)

### Template Deduction Errors

**Problem**: `std::function<>` cannot deduce from lambda types

**Affected Files**:
- `frozen2_layout_builder.h` - All template functions
- `frozen2_value_serializer.h` - All template methods

**Solution** (Session 80):
Change signatures from:
```cpp
template<typename T>
void foo(T const& val, std::function<void(T)> func);
```

To:
```cpp
template<typename T, typename Func>
void foo(T const& val, Func&& func);
```

---

## Testing Strategy

### Unit Tests (Planned)

**File**: `test/metadata/legacy/frozen2_serializer_test.cpp`

Port from dwarfs-rs `ser_frozen.rs:859-961`:
1. **smoke test**: Minimal metadata with options
2. **bytes test**: String serialization with distance relaxation
3. **collection test**: Vector serialization with outlined storage

**Success Criteria**: Byte-for-byte match with dwarfs-rs output

### Integration Tests (Planned)

1. Create .dth image with mkdwarfs
2. Mount with Homebrew v0.14.1 dwarfs
3. Verify file extraction
4. Compare checksums

---

## Future Extensions

### Easy to Add

- New domain types: Add builder + serializer functions
- New primitive types: Add build_* and serialize_* functions
- Optimizations: Modify finish() logic

### Maintains

- Separation of concerns
- Modularity
- Testability
- File size limits

---

**Created**: 2026-01-05
**Session**: 79
**Status**: Architecture complete, templates need fixes
**Next**: Session 80 - Fix templates and test