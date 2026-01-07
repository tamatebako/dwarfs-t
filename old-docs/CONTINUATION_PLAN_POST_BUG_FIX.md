# Continuation Plan - Post Bug Fix

**Date**: 2025-11-27
**Previous Work**: Fixed 11 unsafe optional access bugs in mkdwarfs
**Status**: Ready to proceed with cleanup, testing, and benchmarking

## Immediate Actions (Session 1)

### 1. Code Cleanup
- Remove debug logging from scanner.cpp (partial cleanup done)
- Verify no debug statements remain in production code
- Run clang-format on modified files

### 2. Commit Bug Fixes
```bash
git add tools/src/mkdwarfs_main.cpp \
        tools/src/mkdwarfs/options_parser.cpp \
        src/writer/internal/flatbuffers_metadata_builder.cpp \
        src/writer/internal/metadata_builder.cpp \
        src/writer/segmenter.cpp \
        src/writer/scanner.cpp \
        src/writer/internal/entry.cpp

git commit -m "fix(mkdwarfs): resolve 11 unsafe optional access bugs

- Fix uninitialized categorized_option defaults in segmenter config
- Add safety checks for optional access in metadata builders
- Fix scanner and entry visitor inode_num access
- Initialize uid/gid variables properly
- Ensure compression and metadata format defaults applied

Fixes crashes during filesystem creation caused by bad_optional_access"
```

### 3. Full Test Suite
```bash
cd build
ctest -j --output-on-failure
ctest -R expensive
```

## Short-term Work (Sessions 2-3)

### 4. Benchmarking Framework Validation
**Status**: Framework discovered at benchmarks/ (3000+ lines)
**Priority**: HIGH
**Tasks**:
- Validate existing benchmark infrastructure
- Create test filesystem images
- Run metadata format comparisons
- Document performance characteristics

### 5. Documentation Updates
- Update README.md with bug fix notes
- Document safe optional access patterns
- Add categorized_option initialization requirements

## Medium-term Work (Sessions 4-6)

### 6. Architecture Improvements
Based on lessons learned from bug fixes:

#### 6.1 Refactor categorized_option
**Problem**: get() calls value() without safety
**Solution**: Add safe methods
```cpp
// Add to contextual_option class
value_type get_or(context_argument_type const& arg, 
                  value_type const& fallback) const {
  auto opt = get_optional(arg);
  return opt.value_or(fallback);
}
```

#### 6.2 Optional Safety Linter
**Goal**: Catch unsafe .value() calls at compile time
**Options**:
- clang-tidy custom check
- Static analyzer rule
- CI/CD lint stage

### 7. Test Coverage Expansion
- Add tests for categorized_option initialization
- Add tests for segmenter config edge cases
- Expand metadata builder tests

## Long-term Work (Future)

### 8. Code Quality Improvements
- Audit entire codebase for unsafe optional access
- Create coding guidelines document
- Add pre-commit hooks for safety checks

### 9. Performance Benchmarking
- Complete metadata format comparison
- Document performance characteristics
- Optimize hot paths if needed

### 10. Feature Work
Resume normal development after stability confirmed

## Success Criteria

### Phase 1 (Immediate)
- ✅ All bugs fixed (DONE)
- [ ] Debug logging removed
- [ ] Changes committed
- [ ] All tests pass

### Phase 2 (Short-term)
- [ ] Benchmarking framework validated
- [ ] Performance baseline established
- [ ] Documentation updated

### Phase 3 (Medium-term)
- [ ] categorized_option refactored
- [ ] Safety tooling in place
- [ ] Test coverage improved

## Risk Mitigation

### Regression Risk
**Mitigation**: Full test suite must pass before merge

### Performance Risk
**Mitigation**: Benchmark before/after to confirm no degradation

### Compatibility Risk
**Mitigation**: All changes are backward compatible

## Timeline Estimate

- Immediate (1 session): 2-3 hours
- Short-term (2-3 sessions): 6-8 hours
- Medium-term (4-6 sessions): 12-16 hours

Total: ~24 hours of focused work

## Notes

All work follows principles:
- Object-oriented design
- MECE (Mutually Exclusive, Collectively Exhaustive)
- Separation of concerns
- Architectural solutions over guards
- Correctness over convenience