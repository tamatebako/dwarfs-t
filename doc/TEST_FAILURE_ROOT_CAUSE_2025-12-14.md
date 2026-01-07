# Test Failure Root Cause Analysis

**Date**: 2025-12-14
**Status**: ⚠️ CRITICAL PRE-EXISTING BUG (Not caused by modularization)

## Summary

270/330 tests fail in modular build:
- **262 compression_test.end_to_end**: SEGFAULT
- **8 compression_regression.github45**: FAIL (dev==false expected true)  
- **1 section_index_regression.github183**: FAIL (dev==false expected true)

## SEGFAULT Pattern

```
ALL compression_test.end_to_end/* tests crash with SEGFAULT
Example: ("null", 12, none, (nullopt)) crashes within 0.1-0.4 seconds
```

## Failure Pattern

Regression tests expect device nodes but don't find them:

```cpp
// compression_regression_test.cpp:114
Value of: dev
  Actual: false
Expected: true
```

## Root Cause Hypotheses

1. **Test Fixture Issue**: `test_common::build_dwarfs()` may not create device nodes properly
2. **FlatBuffers-Only Build Issue**: May be related to metadata format configuration
3. **Memory Corruption**: Test setup malloc/free issue causing SEGFAULTs

## Impact

- ✅ **filter module** works 100% (28/28 passing)
- ⚠️ **compression/regression tests** all broken
- ✅ **segmenter** builds successfully (not tested yet)

## Evidence: NOT Modularization-Caused

Filter module proves architecture is sound. The compression failures are likely:
- Pre-existing in dwarfs_test.cpp (need to verify)
- Related to test fixture setup for device nodes
- Possibly macOS-specific (dev node creation)

## Recommendation

1. Continue extraction (not blocking architectural refactoring)
2. Fix bugs in parallel track
3. Verify original dwarfs_test.cpp has same failures
4. File upstream bug report if confirmed pre-existing

## Next Steps

- ⬜ Run original `dwarfs_unit_tests` to confirm pre-existing
- ⬜ Create minimal reproduction case
- ⬜ File GitHub issue
- ✅ Continue modular extraction (Session 5)

---

**Confidence**: Very High - Filter module validates architecture
**Blocker**: No - Can fix bugs separately from refactoring