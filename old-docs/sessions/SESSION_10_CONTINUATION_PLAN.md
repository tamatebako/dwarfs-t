# Session 10 Continuation Plan - Cross-Format Testing & Benchmarking

**Created**: 2025-12-16
**Priority**: Critical
**Estimated Time**: 6-8 hours

---

## Objective

Ensure all tests pass in FlatBuffers-only, Thrift-only, and dual-format builds. Enable performance benchmarking to compare formats across all filesystem operations.

---

## Prerequisites

- Session 9 complete: 18/18 tests passing with FlatBuffers + FSST
- Current status: Tests may fail in Thrift-only or dual-format builds
- Build configurations must be tested independently

---

## Phase 1: Build Configuration Matrix (2 hours)

### 1.1 Create Build Scripts
**File**: `scripts/test-all-configs.sh`

```bash
#!/bin/bash
# Test all build configurations

configs=(
  "flatbuffers-only"
  "thrift-only"
  "both-formats"
)

for config in "${configs[@]}"; do
  echo "Testing $config..."
  case $config in
    flatbuffers-only)
      cmake -B build-fb-only -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=OFF -DWITH_TESTS=ON
      ;;
    thrift-only)
      cmake -B build-thrift-only -DDWARFS_WITH_FLATBUFFERS=OFF -DDWARFS_WITH_THRIFT=ON -DWITH_TESTS=ON
      ;;
    both-formats)
      cmake -B build-both -DDWARFS_WITH_FLATBUFFERS=ON -DDWARFS_WITH_THRIFT=ON -DWITH_TESTS=ON
      ;;
  esac

  cmake --build build-$config --target dwarfs_filesystem_tests
  ./build-$config/dwarfs_filesystem_tests --gtest_color=yes
done
```

### 1.2 CI/CD Integration
**File**: `.github/workflows/cross-format-tests.yml`

Add job matrix testing all 3 configurations across platforms.

---

## Phase 2: Test Fixture Refactoring (2 hours)

### 2.1 Make FSST Conditional
**File**: `test/fixtures/dwarfs_test_fixture.cpp:32-37`

**Current** (breaks Thrift-only):
```cpp
options.metadata.plain_names_table = false;  // Always use FSST
```

**Fixed** (format-aware):
```cpp
#ifdef DWARFS_HAVE_FLATBUFFERS
  options.metadata.plain_names_table = false;  // Use FSST with FlatBuffers
  options.metadata.plain_symlinks_table = false;
#else
  options.metadata.plain_names_table = true;   // No FSST in Thrift-only
  options.metadata.plain_symlinks_table = true;
#endif
```

### 2.2 Add Format Detection Helper
**File**: `test/fixtures/dwarfs_test_fixture.h`

```cpp
protected:
  // Helper to detect which formats are available
  bool has_flatbuffers() const {
#ifdef DWARFS_HAVE_FLATBUFFERS
    return true;
#else
    return false;
#endif
  }

  bool has_thrift() const {
#ifdef DWARFS_HAVE_THRIFT
    return true;
#else
    return false;
#endif
  }
```

---

## Phase 3: Thrift Build Fixes (2 hours)

### 3.1 Verify Thrift String Table Handling
**Check**: Does Thrift support FSST string tables?

If NO:
- Update `src/writer/internal/flatbuffers_packing_processor.cpp` to only enable FSST with FlatBuffers
- Add `#ifdef DWARFS_HAVE_FLATBUFFERS` guards around FSST code

If YES:
- Ensure Thrift reader can unpack FSST tables
- Add integration tests

### 3.2 Test Thrift-Only Build
```bash
cmake -B build-thrift-only \
  -DDWARFS_WITH_FLATBUFFERS=OFF \
  -DDWARFS_WITH_THRIFT=ON \
  -DWITH_TESTS=ON
cmake --build build-thrift-only --target dwarfs_filesystem_tests
./build-thrift-only/dwarfs_filesystem_tests
```

Expected: 18/18 tests passing

---

## Phase 4: Benchmark Infrastructure (3 hours)

### 4.1 Benchmark Test Suite
**File**: `test/benchmark/format_benchmark_test.cpp`

```cpp
#include <benchmark/benchmark.h>
#include "../fixtures/filesystem_test_fixture.h"

namespace dwarfs::test::benchmark {

class FormatBenchmark : public ::benchmark::Fixture,
                        public FilesystemTestFixture {
protected:
  void SetUp(const ::benchmark::State& state) override {
    FilesystemTestFixture::SetUp();
  }

  void TearDown(const ::benchmark::State& state) override {
    FilesystemTestFixture::TearDown();
  }
};

// Benchmark: Filesystem creation with different formats
BENCHMARK_DEFINE_F(FormatBenchmark, CreateFilesystem)(benchmark::State& state) {
  for (auto _ : state) {
    input_ = create_test_instance();
    auto fsimage = build_filesystem();
    benchmark::DoNotOptimize(fsimage);
  }

  state.SetLabel(get_format_name());
}

BENCHMARK_REGISTER_F(FormatBenchmark, CreateFilesystem)
  ->Iterations(100)
  ->Unit(benchmark::kMillisecond);

// Benchmark: File lookup
BENCHMARK_DEFINE_F(FormatBenchmark, FileLookup)(benchmark::State& state) {
  input_ = create_test_instance();
  auto fsimage = build_filesystem();
  create_filesystem_from_image(fsimage);

  std::vector<std::string> paths;
  filesystem_->walk([&](auto e) { paths.push_back(e.unix_path()); });

  for (auto _ : state) {
    for (auto const& path : paths) {
      auto entry = filesystem_->find(path);
      benchmark::DoNotOptimize(entry);
    }
  }

  state.set
ItemsProcessed(state.iterations() * paths.size());
}

BENCHMARK_REGISTER_F(FormatBenchmark, FileLookup)
  ->Iterations(1000)
  ->Unit(benchmark::kMicrosecond);

// Add more benchmarks for:
// - Directory traversal
// - Metadata size
// - String table compression ratio
// - FSST pack/unpack speed

} // namespace dwarfs::test::benchmark
```

### 4.2 Benchmark Reporting
**File**: `scripts/run-benchmarks.sh`

```bash
#!/bin/bash
# Run benchmarks for all formats and generate comparison report

formats=("flatbuffers" "thrift" "both")

for format in "${formats[@]}"; do
  echo "Benchmarking $format..."
  ./build-$format/dwarfs_benchmark_tests \
    --benchmark_format=json \
    --benchmark_out=results-$format.json
done

# Generate comparison report
python3 scripts/compare-benchmarks.py \
  results-flatbuffers.json \
  results-thrift.json \
  results-both.json
```

### 4.3 Comparison Script
**File**: `scripts/compare-benchmarks.py`

Generates markdown table comparing:
- Filesystem creation time
- File lookup latency
- Metadata size
- String table size
- Compression ratio

---

## Phase 5: Documentation Updates (1 hour)

### 5.1 Update README.adoc
**Section**: Format Selection

Document when to use each format:
- FlatBuffers: Best for portability, header-only, modern default
- Thrift: Best for size, legacy compatibility, requires Folly+fbthrift
- Both: Maximum compatibility, larger binary size

### 5.2 Add Benchmark Results
**File**: `doc/PERFORMANCE_COMPARISON.md`

Include tables showing:
- Build size comparison
- Runtime performance
- Metadata size
- Compression effectiveness

---

## Success Criteria

✅ **Build Matrix**:
- FlatBuffers-only: 18/18 tests passing
- Thrift-only: 18/18 tests passing
- Both formats: 18/18 tests passing

✅ **Benchmarks**:
- All benchmarks run successfully
- Comparison report generated
- Performance differences < 10% between formats

✅ **Documentation**:
- README.adoc updated
- Benchmark results documented
- Format selection guide complete

---

## Deliverables

1. **Scripts**:
   - `scripts/test-all-configs.sh`
   - `scripts/run-benchmarks.sh`
   - `scripts/compare-benchmarks.py`

2. **Test Files**:
   - `test/benchmark/format_benchmark_test.cpp`
   - Updated `test/fixtures/dwarfs_test_fixture.cpp`

3. **Documentation**:
   - Updated `README.adoc`
   - New `doc/PERFORMANCE_COMPARISON.md`
   - Updated `.github/workflows/cross-format-tests.yml`

4. **Verification**:
   - All 3 build configurations tested
   - Benchmark results collected
   - CI/CD passing

---

## Risk Mitigation

**Risk**: Thrift-only build may not support FSST
**Mitigation**: Add conditional compilation guards, fallback to plain names

**Risk**: Benchmarks may show significant performance differences
**Mitigation**: Document trade-offs, provide guidance on format selection

**Risk**: Test failures in dual-format builds
**Mitigation**: Add format-specific test paths, ensure proper initialization

---

**Next Session Start**: Read this plan and implementation status tracker
**Estimated Completion**: Session 11-12 (6-8 hours total)