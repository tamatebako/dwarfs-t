# Session 14 Continuation Prompt

Start Session 14 by reading this prompt and the implementation plan.

## Context

**Previous Sessions**:
- Session 12: Fixed Folly allocator (all 3 configs work on macOS ARM64) ✅
- Session 13: Identified FlatBuffers performance issue (53x slower than Thrift) ✅

**Current State**:
- All builds working without jemalloc
- FlatBuffers extraction: 2.11s (TOO SLOW)
- Thrift extraction: 0.04s (baseline)
- Benchmarks only test one format per build

## Your Mission

Fix three critical issues:

### 1. Fix Benchmarking (1 hour)
**Problem**: Both-formats build doesn't test BOTH image formats
**Goal**: Benchmark both FlatBuffers AND Thrift images properly

**Action**:
1. Find the correct option to specify image format at creation time
2. Update benchmark scripts to create both formats
3. Re-run benchmarks showing both formats

### 2. Remove Cereal/Bitsery (2 hours) 
**Problem**: Dead code from removed serialization formats
**Goal**: Clean codebase with only FlatBuffers + Thrift

**Action**:
```bash
# Find all references
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec grep -l "cereal\|bitsery" {} \;

# Remove files and update CMakeLists.txt
# Update metadata serialization registry
# Rebuild and test
```

### 3. Optimize FlatBuffers (1-2 hours)
**Problem**: FlatBuffers extraction is 53x slower than Thrift
**Goal**: FlatBuffers ≤ 0.10s (within 2.5x of Thrift)

**Root Cause Hypotheses**:
- H1: Metadata compressed when should be uncompressed
- H2: FSST string decompression slow
- H3: Not using memory-mapped access
- H4: Missing optimizations

**Action**:
1. Profile extraction to find bottleneck
2. Compare `metadata_v2_flatbuffers.cpp` vs `metadata_v2_thrift.cpp`
3. Apply fixes and benchmark each change
4. Target: ≤ 0.10s extraction time

## Files to Read First

1. `doc/SESSION_14_FLATBUFFERS_OPTIMIZATION_PLAN.md` - Full implementation plan
2. `doc/SESSION_13_IMPLEMENTATION_STATUS.md` - Previous session results (if exists)
3. `benchmarks/results/build_comparison.json` - Current performance data

## Success Criteria

- [ ] Both FlatBuffers and Thrift formats benchmarked
- [ ] Zero Cereal/Bitsery references in code
- [ ] FlatBuffers extraction ≤ 0.10s
- [ ] All tests passing
- [ ] Documentation updated

## Start Here

Begin with Phase 1 of the plan: Fix benchmarking infrastructure.

**First Command**:
```bash
# Search for format option in code
grep -r "metadata.*format\|format.*metadata" tools/src/mkdwarfs/ | grep -i option
```

Good luck!
