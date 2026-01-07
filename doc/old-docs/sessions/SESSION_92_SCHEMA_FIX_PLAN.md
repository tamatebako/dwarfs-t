# Session 92: Modern Thrift Schema Fix - Detailed Plan

**Created**: 2026-01-06
**Estimated Duration**: 45-60 minutes
**Prerequisite**: Session 91 complete (namespace + config.h fixes ✅)
**Goal**: Align Thrift schema with domain model, complete Modern Thrift build

---

## Problem Statement

The Modern Thrift schema (`thrift/metadata_modern.thrift`) was created from documentation rather than the actual domain model source code, resulting in 17 type mismatches. The compiler cannot convert between incompatible types even though field names match.

**Root Cause**: Violation of **Single Source of Truth** principle - domain model is authoritative, schema must mirror it exactly.

---

## Approach: Systematic Schema Audit & Fix

### Phase 1: Domain Model Analysis (10 min)

**Objective**: Extract authoritative type definitions from domain model

**Actions**:
1. Read [`include/dwarfs/metadata/domain/metadata.h`](../include/dwarfs/metadata/domain/metadata.h)
2. Document ALL field types for `metadata` struct
3. Focus on v2.5+ fields (where errors occur):
   - `category_names`
   - `block_categories`
   - `category_metadata_json`
   - `block_category_metadata`
   - `large_hole_size`
   - `features`
   - `reg_file_size_cache`

**Output**: Complete type mapping table (domain model → Thrift types)

---

### Phase 2: Schema Comparison (10 min)

**Objective**: Identify ALL discrepancies between domain and schema

**Actions**:
1. Read [`thrift/metadata_modern.thrift`](../thrift/metadata_modern.thrift)
2. Compare each field against domain model
3. Document every type mismatch (not just the 17 compiler found)

**Output**: Full mismatch report with line numbers

---

### Phase 3: Schema Correction (15 min)

**Objective**: Fix ALL type mismatches in schema

**Critical Type Mappings** (C++ → Thrift):

| C++ Type | Thrift Type | Notes |
|----------|-------------|-------|
| `std::string` | `string` | Direct mapping |
| `std::vector<std::string>` | `list<string>` | Collection |
| `std::vector<uint32_t>` | `list<i32>` | Unsigned → signed (Thrift limitation) |
| `std::vector<uint64_t>` | `list<i64>` | Unsigned → signed |
| `std::map<uint32_t, uint32_t>` | `map<i32, i32>` | Unsigned → signed |
| `std::optional<T>` | `optional T` | Direct mapping |

**Known Fixes Needed** (from compiler errors):

```thrift
# WRONG (current schema)
12: optional list<i32> categoryNames
13: optional list<i16> blockCategories
32: optional string categoryMetadataJson
33: optional string blockCategoryMetadata
47: optional i64 largeHoleSize

# CORRECT (should be)
12: optional list<string> categoryNames
13: optional list<i32> blockCategories
32: optional list<string> categoryMetadataJson
33: optional map<i32, i32> blockCategoryMetadata
47: optional list<i64> largeHoleSize
```

**Actions**:
1. Edit `thrift/metadata_modern.thrift` with fixes
2. Fix ALL 17+ type mismatches
3. Ensure optional/required modifiers match domain model

---

### Phase 4: Regenerate Thrift Code (5 min)

**Objective**: Generate correct C++ types from fixed schema

**Actions**:
```bash
# Clean old generated code
rm -rf build-modern/thrift/modern/gen-cpp2/*

# Regenerate via CMake
ninja -C build-modern dwarfs_metadata_modern_thrift_generate

# Verify generated types
ls -lh build-modern/thrift/modern/gen-cpp2/
```

**Success Criteria**:
- 21 files generated
- `metadata_modern_types.h` contains corrected types
- No generation errors

---

### Phase 5: Converter Adjustment (10 min)

**Objective**: Ensure converters handle fixed types correctly

**Review Areas**:
1. Type conversions may be direct now (no more casting needed)
2. Check for any remaining manual type conversions
3. Verify optional field handling

**Files to Review**:
- [`src/metadata/modern/domain_to_thrift.cpp`](../src/metadata/modern/domain_to_thrift.cpp)
- [`src/metadata/modern/thrift_to_domain.cpp`](../src/metadata/modern/thrift_to_domain.cpp)

**Expected**: Most conversions should now be simple assignments (types match)

---

### Phase 6: Build & Verify (5-10 min)

**Objective**: Complete Modern Thrift library build

**Actions**:
```bash
# Build Modern Thrift library
ninja -C build-modern dwarfs_metadata_modern_thrift

# Verify library created
ls -lh build-modern/libdwarfs_metadata_modern_thrift.a

# Build test executables
ninja -C build-modern modern_thrift_converter_tests
ninja -C build-modern modern_thrift_serialization_tests
```

**Success Criteria**:
- ✅ Library compiles without errors
- ✅ Library size: ~500 KB - 1 MB
- ✅ Both test executables compile
- ✅ No type mismatch errors

---

## Verification Checklist

### Schema Correctness
- [ ] All fields match domain model types exactly
- [ ] Optional/required modifiers correct
- [ ] No unsigned → signed precision loss
- [ ] Collection types match (vector → list, map → map)
- [ ] String vs string collections correct

### Build Success
- [ ] Thrift code generates without errors
- [ ] 21 files generated in gen-cpp2/
- [ ] Converters compile without warnings
- [ ] Modern Thrift library created
- [ ] Test executables compile

### Code Quality
- [ ] No type casting hacks in converters
- [ ] All conversions are direct assignments
- [ ] Namespace aliases working correctly
- [ ] Include paths correct

---

## Risk Mitigation

### Risk 1: More Type Mismatches Discovered
**Mitigation**: Systematic audit catches all issues upfront

### Risk 2: Thrift Doesn't Support Unsigned Types Well
**Mitigation**: Document that i32/i64 in Thrift represent uint32/uint64 from domain model

### Risk 3: Converters Break After Schema Fix
**Mitigation**: Type safety ensures compiler catches any issues

---

## Time Budget

| Phase | Estimated | Notes |
|-------|-----------|-------|
| 1. Domain Model Analysis | 10 min | Read source code |
| 2. Schema Comparison | 10 min | Line-by-line audit |
| 3. Schema Correction | 15 min | Edit .thrift file |
| 4. Regenerate Code | 5 min | CMake + ninja |
| 5. Converter Adjustment | 10 min | Review + minor fixes |
| 6. Build & Verify | 5-10 min | Full compilation |
| **Total** | **45-60 min** | |

---

## Output Artifacts

1. **Fixed Schema**: `thrift/metadata_modern.thrift` (corrected types)
2. **Generated Code**: 21 files in `build-modern/thrift/modern/gen-cpp2/`
3. **Library**: `build-modern/libdwarfs_metadata_modern_thrift.a`
4. **Tests**: `modern_thrift_converter_tests`, `modern_thrift_serialization_tests`
5. **Documentation**: Type mapping reference

---

## Success Definition

**Session 92 Complete** when:
1. All 17 type mismatches fixed in schema
2. Thrift code regenerates successfully
3. Modern Thrift library compiles without errors
4. Ready for Session 89 testing (all 3 formats)

---

**Next Session After 92**: Session 89 (Resume Testing with vcpkg)
**See**: [`SESSION_89_CONTINUATION_PROMPT.md`](SESSION_89_CONTINUATION_PROMPT.md)

---

**Created**: 2026-01-06 16:50 HKT
**Status**: Ready to execute
**Blocking**: Must complete before Session 89 testing