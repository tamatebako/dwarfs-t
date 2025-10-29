# Track B: Final Thrift Conversion Analysis

## Executive Summary

**Status:** 171 Thrift references found across 15 core files
**Complexity:** VERY HIGH - involves complete architectural refactoring
**Estimated Effort:** 4-6 hours of intensive work

## Critical Files Requiring Conversion

### Tier 1: Writer Infrastructure (Build metadata)
1. **src/writer/internal/metadata_builder.cpp** (~1300 lines)
   - Currently builds `thrift::metadata::metadata` objects
   - 94 Thrift references
   - Must convert to build `domain::Metadata` directly

2. **src/writer/internal/metadata_freezer.cpp** (~200 lines)
   - Uses Thrift frozen serialization
   - 4 Thrift references
   - Must convert to Cereal serialization

3. **include/dwarfs/writer/internal/metadata_builder.h**
   - Interface uses `thrift::metadata::metadata`
   - 9 Thrift references

4. **include/dwarfs/writer/internal/metadata_freezer.h**
   - Interface uses `thrift::metadata::metadata`
   - 4 Thrift references

5. **include/dwarfs/writer/internal/inode.h**
   - Uses `thrift::metadata::chunk`
   - 2 Thrift references

### Tier 2: Reader Infrastructure (Read metadata)
6. **src/reader/internal/metadata_v2.cpp** (~2400 lines) **KEYSTONE**
   - Uses frozen views extensively
   - 48 Thrift references
   - Loads metadata from serialized format
   - Most complex file to convert

7. **src/reader/internal/metadata_types.cpp** (~600 lines)
   - Type wrappers and views
   - 11 Thrift references

8. **src/reader/internal/metadata_analyzer.cpp** (~500 lines)
   - Analyzes frozen metadata
   - 9 Thrift references

9. **src/reader/internal/time_resolution_handler.cpp** (~100 lines)
   - Uses frozen views
   - 4 Thrift references

10. **src/reader/filesystem_v2.cpp**
    - Interface methods returning Thrift types
    - 9 Thrift references

### Tier 3: Headers/Interfaces
11. **include/dwarfs/reader/filesystem_v2.h**
    - Interface declarations
    - 8 Thrift references

12. **include/dwarfs/reader/internal/metadata_v2.h**
    - Interface
    - 6 Thrift references

13. **include/dwarfs/reader/internal/metadata_types.h**
    - Type definitions using frozen views
    - 14 Thrift references

14. **include/dwarfs/reader/internal/time_resolution_handler.h**
    - Interface
    - 3 Thrift references

15. **include/dwarfs/reader/internal/metadata_analyzer.h**
    - Interface
    - 6 Thrift references

## Files Already Converted (Keep As-Is)
- `src/metadata/serialization/thrift_converter.cpp` - Bridge between Thrift and domain
- `include/dwarfs/metadata/serialization/thrift_converter.h` - Same
- `include/dwarfs/metadata/serialization/thrift_compact_serializer.h` - Uses ThriftConverter

These files are INTENTIONALLY designed to work with Thrift for backward compatibility.

## Conversion Strategy

### Phase 1: Writer Side (Easier)
Convert files that BUILD metadata:
1. Update `metadata_builder.cpp` to build `domain::Metadata`
2. Update `metadata_freezer.cpp` to use Cereal
3. Update all writer interfaces

### Phase 2: Reader Side (Harder)
Convert files that READ metadata:
1. Convert `metadata_v2.cpp` - remove frozen views, use domain models directly
2. Convert `metadata_types.cpp` - remove frozen view wrappers
3. Convert `metadata_analyzer.cpp` - work with domain models
4. Convert `time_resolution_handler.cpp` - work with domain models
5. Update all reader interfaces

### Phase 3: Integration
1. Update `filesystem_v2.cpp` to remove Thrift return types
2. Clean rebuild
3. Fix compilation errors iteratively
4. Run tests

## Key Architectural Changes

### Before (Current):
```
Writer: Build thrift::metadata → Freeze → Serialize
Reader: Deserialize → Frozen Views → Access data
```

### After (Target):
```
Writer: Build domain::Metadata → Serialize with Cereal
Reader: Deserialize with Cereal → domain::Metadata → Access data
```

## Risk Assessment

**HIGH RISK AREAS:**
1. metadata_v2.cpp - 2400 lines, heavy frozen view usage
2. Potential data compatibility issues
3. Performance changes from removing frozen views
4. Test failures requiring debugging

**MITIGATION:**
1. Incremental conversion with frequent rebuilds
2. Keep ThriftConverter for backward compatibility
3. Extensive testing after each major change

## Success Metrics

✅ All 171 Thrift references removed from core files
✅ Clean compilation with DTEBAKO_BUILD=ON
✅ All tests passing
✅ Static library builds successfully
✅ Performance within acceptable range

## Estimated Timeline

- Phase 1 (Writer): 1-2 hours
- Phase 2 (Reader): 2-3 hours
- Phase 3 (Integration): 1 hour
- **Total: 4-6 hours**

## Next Steps

1. Start with metadata_builder.cpp (writer side)
2. Then metadata_freezer.cpp
3. Move to metadata_v2.cpp (keystone)
4. Convert remaining reader files
5. Update all interfaces
6. Build and test iteratively