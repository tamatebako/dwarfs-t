# Session 96: v0.17.0 Release Preparation - Continuation Prompt

**Quick Start**: Read this file to begin Session 96

---

## Context

Session 95 completed **all build integration and documentation** for Modern Thrift.

**What's Done** ✅:
- CMake generator expression bug fixed permanently
- Modern Thrift integrated into main build
- 800+ lines of documentation created
- All official docs updated

**What's Next** ⏳:
- Validate Homebrew v0.14.1 compatibility (CRITICAL)
- Test all 3 formats work correctly
- Fix benchmark scripts
- Prepare v0.17.0 release

---

## CRITICAL REQUIREMENTS

### 1. Three Metadata Formats in Two Builds

**Build 1: Default** (FlatBuffers + Legacy Thrift)
```bash
cmake -B build -GNinja -DCMAKE_BUILD_TYPE=Release
# FlatBuffers: ON by default (no need to specify)
# Legacy Thrift: Always available
# Modern Thrift: OFF by default
```

**Build 2: Thrift-Enabled** (All Three)
```bash
cmake -B build-modern -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DDWARFS_WITH_THRIFT=ON
# FlatBuffers: ON
# Legacy Thrift: Always available
# Modern Thrift: ON (requires fbthrift)
```

### 2. Homebrew Compatibility (TOP PRIORITY)

**Location**: `/opt/homebrew/Cellar/dwarfs/0.14.1_3`

**Must Work**:
- ✅ Read Homebrew v0.14.1 `.dft` images (Legacy Thrift)
- ✅ Write `.dth` images that Homebrew can read
- ✅ Format detection (Legacy Thrift = priority 50, no magic bytes)
- ✅ Byte-for-byte content preservation

**Test Plan**: Create test suite in `test/metadata/legacy/homebrew_compatibility_test.cpp`

### 3. Benchmark Scripts Must Work

**Script 1**: `scripts/benchmark-all.sh`
```bash
cd scripts
./benchmark-all.sh
# Must benchmark all 3 formats
```

**Script 2**: `benchmarks/build_and_test_all.py`
```bash
python3 benchmarks/build_and_test_all.py --build-all
```

### 4. Example Application Must Work

```bash
cd example/static-site-server
./build.sh  # Must succeed
./test.sh   # Must pass
```

### 5. Build Script Must Work

```bash
./scripts/build-all-and-test.sh --vcpkg
# Must complete with exit code 0
```

---

## Your Mission (4 hours)

### Phase 1: Legacy Thrift Homebrew Compatibility (90 min)

**Task 1.1**: Create Homebrew compatibility test suite (40 min)
- Write `test/metadata/legacy/homebrew_compatibility_test.cpp`
- 4 tests: ReadHomebrew, HomebrewReadsOurs, FormatDetection, RoundTrip
- Integrate into cmake/metadata_serialization.cmake

**Task 1.2**: Test all 3 formats end-to-end (30 min)
- Create images with FlatBuffers, Modern Thrift, Legacy Thrift
- Verify format detection (magic bytes vs fallback)
- Extract and diff content

**Task 1.3**: Verify default build behavior (20 min)
- Confirm DWARFS_WITH_FLATBUFFERS=ON by default (no need to specify)
- Verify Legacy Thrift always available
- Check Modern Thrift OFF by default

### Phase 2: Benchmark System Validation (60 min)

**Task 2.1**: Fix and test benchmark-all.sh (30 min)
- Ensure works with all 3 formats
- Produces comparison table
- No errors

**Task 2.2**: MECE cleanup of benchmarks/ and scripts/ (30 min)
- Remove duplicates
- Document each script/directory
- Create README.md for both

### Phase 3: Integration Testing (45 min)

**Task 3.1**: Example app (20 min)
- `cd example/static-site-server && ./build.sh && ./test.sh`

**Task 3.2**: Build script (25 min)
- `./scripts/build-all-and-test.sh --vcpkg`

### Phase 4: Release Preparation (45 min)

**Task 4.1**: Move docs (10 min)
**Task 4.2**: Create RELEASE_NOTES_v0.17.0.md (20 min)
**Task 4.3**: Tag v0.17.0-rc1 (15 min)

---

## Essential Reading

**Required Order**:
1. [`SESSION_96_CONTINUATION_PLAN.md`](SESSION_96_CONTINUATION_PLAN.md) - Full plan (this prompt's companion)
2. [`SESSION_95_COMPLETION_SUMMARY.md`](SESSION_95_COMPLETION_SUMMARY.md) - What just completed
3. [`SESSION_96_IMPLEMENTATION_STATUS.md`](SESSION_96_IMPLEMENTATION_STATUS.md) - Task tracker
4. [`.kilocode/rules/memory-bank/context.md`](../.kilocode/rules/memory-bank/context.md) - Project context

---

## Start Command

```bash
# Begin with Homebrew compatibility testing
cd /Users/mulgogi/src/external/dwarfs

# Verify Homebrew dwarfs available
/opt/homebrew/bin/mkdwarfs --version  # Should show 0.14.1

# Start Task 1.1: Create Homebrew compatibility test
cat doc/SESSION_96_CONTINUATION_PLAN.md  # Read full plan
```

---

## Format Specifications (Reference)

| Format | Magic Bytes | Priority | Extension | Always Available? |
|--------|-------------|----------|-----------|-------------------|
| **FlatBuffers** | `"DFBF"` | 120 | `.dff` | When WITH_FLATBUFFERS=ON (default) |
| **Modern Thrift** | `{0x82, 0x21}` | 100 | `.dtc` | When WITH_THRIFT=ON |
| **Legacy Thrift** | None (fallback) | 50 | `.dth` | **Always** (hand-coded) |

---

## Critical Success Path

1. ✅ **Homebrew compatibility** - Without this, v0.14.1 users can't upgrade
2. ✅ **All formats functional** - Core feature validation
3. ✅ **Scripts work** - Release automation depends on this
4. ✅ **RC1 tagged** - Triggers final validation

---

**Time Budget**: ~4 hours
**Priority**: CRITICAL - v0.17.0 blocker
**Start**: Task 1.1 (Homebrew compatibility tests)