# FlatBuffers vs Thrift Benchmark Status
**Date**: 2025-11-17
**Branch**: feature/multi-format-serialization-fuse
**Goal**: Run comprehensive performance comparison between FlatBuffers and Thrift

## Execution Attempt Summary

### Prerequisites Verified ✅
- ✅ Benchmark suite updated (`run_metadata_format_benchmark.py`)
- ✅ Report generator updated (`report_generator.py`)
- ✅ FlatBuffers format implemented and integrated
- ✅ CMake configuration supports both formats
- ✅ Python scripts syntax validated

### Blockers Encountered ❌

#### 1. Thrift Support Not Built (CRITICAL)
**Status**: fbthrift/Folly submodules not built
**Impact**: Cannot compare FlatBuffers vs Thrift
**Build Time**: 30-60 minutes on first build
**CMake Output**:
```
-- Thrift serialization: DISABLED (fbthrift not found)
-- ════════════════════════════════════════════════════
-- Metadata serialization summary:
--   - FlatBuffers:    ON (REQUIRED, modern default)
--   - Thrift Compact: OFF (legacy, optional)
```

**Root Cause**:
- Submodules exist: `git submodule status` shows folly and fbthrift
- But not **initialized/built**: `git submodule update --init` completed
- Still requires full Folly+fbthrift CMake build (~30-60 min)

#### 2. Local macOS ARM64 Build Failure (KNOWN ISSUE)
**Status**: Compilation error in `src/writer/internal/similarity_ordering.cpp:687`
**Compiler**: AppleClang 17.0.6 on macOS ARM64
**Error Type**: Lambda capture of move-only types (std::promise)

**Error Message**:
```
error: call to implicitly-deleted copy constructor of '(lambda at .../filesystem_writer.cpp:393:16)'
  393 |     wg.add_job([this, prom = std::move(prom)]() mutable {
```

**Context**: This is a **documented known issue** in memory bank:
- `.kilocode/rules/memory-bank/context.md` line 164: "AppleClang ≥17.0: Full support required (current macOS compiler)"
- Issue exists in `similarity_ordering.cpp` with lambda captures
- "Works on CI only" was explicitly rejected as unacceptable

#### 3. CI/CD Artifacts Not Available
**Status**: All recent CI runs are QUEUED, none completed
**Impact**: Cannot download pre-built binaries with both formats

**CI Status** (checked 2025-11-17 02:05 UTC):
```json
[
  {"conclusion":"","status":"queued","createdAt":"2025-11-17T00:51:56Z"},
  {"conclusion":"","status":"queued","createdAt":"2025-11-16T09:06:27Z"},
  {"conclusion":"","status":"queued","createdAt":"2025-11-16T09:04:58Z"}
]
```

## Analysis

### Why FlatBuffers vs Thrift Comparison is Important
Per project memory bank (`.kilocode/rules/memory-bank/architecture.md`):

| Aspect | Thrift Compact | FlatBuffers |
|--------|---------------|-------------|
| **Size** | Smallest (bit-packed) | Medium (+5-10%) |
| **Speed (deserialize)** | Fastest (zero-copy) | Fast (zero-copy) |
| **Platform Support** | Limited | Excellent |
| **Static Linking** | Difficult | Easy |
| **Default** | No (legacy) | **Yes** |

The comparison would validate these theoretical differences with real performance data.

### Why This Matters Less Than Expected
Per memory bank context (`.kilocode/rules/memory-bank/context.md`):
- FlatBuffers is the **modern default** (required, always enabled)
- Thrift is **legacy/optional** (for backward compatibility only)
- Project already transitioned: "Cereal/Bitsery removed, FlatBuffers is default"

**Key Insight**: FlatBuffers standalone benchmarks would still be valuable to:
- Validate FlatBuffers implementation performance
- Establish baseline metrics for future comparisons
- Ensure no regressions from previous formats

## Recommended Next Steps

### Option 1: Complete Thrift Build (Recommended for Full Comparison)
**Time**: 30-60 minutes
**Steps**:
```bash
# Already completed:
git submodule update --init --recursive folly fbthrift

# Still needed:
cd build
cmake .. -GNinja \
  -DDWARFS_WITH_THRIFT=ON \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DWITH_TESTS=OFF
ninja  # Will take 30-60 min on first build

# Then run benchmarks:
python3 benchmarks/run_metadata_format_benchmark.py \
  --mkdwarfs ./build/mkdwarfs \
  --dwarfsextract ./build/dwarfsextract \
  --dwarfs ./build/dwarfs \
  --perl-dataset benchmark-files/perl-5.43.3 \
  --output benchmarks/results/flatbuffers-vs-thrift-$(date +%Y%m%d).json \
  --runs 5
```

### Option 2: FlatBuffers-Only Baseline (Alternative)
**Time**: 10-15 minutes (after build succeeds)
**Value**: Validates FlatBuffers, establishes baseline
**Limitation**: No format comparison

Requires modifying benchmark script to:
```python
FORMATS = ['flatbuffers']  # Single format mode
```

### Option 3: Wait for CI/CD
**Time**: Unknown (currently queued)
**Steps**: Wait for CI/CD to complete, download artifacts

### Option 4: Use Different Platform
**Platforms with working builds** (per CI/CD):
- Ubuntu 22.04/24.04 (x86_64, aarch64)
- macOS with different Xcode versions (15.0.1, 15.4)
- Windows Server 2022/2025

Any platform with GCC ≥10 or Clang ≥12 should work.

## Current Decision

**User Choice**: Run FlatBuffers-only baseline benchmarks
**Status**: BLOCKED by local build failure (Option 2 requires working binaries)

## Next Actions Required

1. **Immediate**: Document this status in memory bank
2. **Short-term**: Fix AppleClang 17 lambda capture issue ([`similarity_ordering.cpp:687`](../../src/writer/internal/similarity_ordering.cpp))
3. **Medium-term**: Build Folly+fbthrift OR use CI/CD artifacts
4. **Long-term**: Run comprehensive FlatBuffers vs Thrift comparison

## References

- **Benchmark Suite**: [`benchmarks/run_metadata_format_benchmark.py`](../../benchmarks/run_metadata_format_benchmark.py)
- **Report Generator**: [`benchmarks/lib/report_generator.py`](../../benchmarks/lib/report_generator.py)
- **Execution Plan**: [`doc/PHASE_5_DETAILED_EXECUTION_PLAN.md`](PHASE_5_DETAILED_EXECUTION_PLAN.md)
- **Memory Bank**: `.kilocode/rules/memory-bank/`

---

**Conclusion**: Full benchmarks require either:
1. Fixing local build issue + building Thrift (60-90 min total), OR
2. Using CI/CD artifacts when available, OR
3. Running on different platform (Linux/Windows)

FlatBuffers-only baseline would be possible with Option 1 or 2 (binaries working first).