# Bug Fix Status - 2025-11-27

## Summary

Investigation revealed multiple initialization bugs in mkdwarfs refactored code causing `bad_optional_access` errors.

## Bugs Found & Fixed

### Bug #1: Assertion Failure (Line 636) ✅ FIXED
**File**: `tools/src/mkdwarfs_main.cpp:636`  
**Error**: `Assertion failed: compressor registered more than once for section type schema`  
**Root Cause**: Typo using `METADATA_V2_SCHEMA` instead of `METADATA_V2`  
**Fix**: Changed to correct enum value  
**Status**: ✅ Committed

### Bug #2: Uninitialized uid/gid Variables ✅ FIXED  
**File**: `tools/src/mkdwarfs_main.cpp:460`  
**Issue**: `uint16_t uid, gid;` declared but not initialized  
**Fix**: `uint16_t uid = 0, gid = 0;`  
**Status**: ✅ Safety fix applied

### Bug #3: Missing Default Compression Algorithm ⚠️ PARTIALLY FIXED
**File**: `tools/src/mkdwarfs/options_parser.cpp:551-568`  
**Issue**: Level defaults applied for block_size, schema_compression, metadata_compression, but NOT for data compression  
**Impact**: When no `-C` option provided, `opts.compression` vector empty → no default compressor added → `default_bc_` in filesystem_writer uninitialized  
**Fix**: Added lines 569-572 to apply `defaults.data_compression` when compression not explicitly set  
**Status**: ⚠️ Applied but ERROR PERSISTS

### Bug #4: metadata_format Not Propagated ⚠️ PARTIALLY FIXED
**File**: `tools/src/mkdwarfs/options_parser.cpp:532-538`  
**Issue**: `opts.metadata_format_enum` parsed but never copied to `opts.scanner_opts.metadata_format`  
**Fix**: Added line 536 to set `opts.scanner_opts.metadata_format = opts.metadata_format_enum`  
**Status**: ⚠️ Applied but ERROR PERSISTS

## Current Status: 🔴 BLOCKED

Despite all fixes above, the error persists:
```
F 11:44:20.006133 exception thrown in worker thread: bad_optional_access
```

### Error Characteristics
- Occurs in worker thread (not main thread)
- Happens during metadata writing phase (scanner.cpp:970 in worker started at line 754)
- Occurs with test data (24 files, 0.00 B scanned before crash)
- Error message provides no stack trace or line number

### Next Steps Required

1. **Enable Debug Symbols**: Build with `-O0 -g` to get full stack trace
2. **Add Logging**: Instrument worker thread code to identify exact optional access
3. **Check Additional Optionals**: Search for all `.value()` calls in writer/metadata code paths
4. **Verify Default Values**: Ensure all scanner_options fields have default initializers

### Files Modified
- `tools/src/mkdwarfs_main.cpp` (2 changes)
- `tools/src/mkdwarfs/options_parser.cpp` (2 changes)

### Test Command
```bash
build-benchmark/mkdwarfs -i testdata -o /tmp/test.dwarfs -l1
```

### Recommendation

**This requires debugging with symbols to identify the exact location of the bad_optional_access in the worker thread.** The fixes applied address known issues but there appears to be an additional uninitialized optional somewhere in the metadata building or compression workflow.

---
**Date**: 2025-11-27  
**Reporter**: Investigation via code analysis  
**Priority**: HIGH - Blocks all mkdwarfs functionality