# Next Session: Thrift Optional Refactoring - Investigation & Caching

**Session Start**: 2025-12-13+
**Current Phase**: Phase 1.3 - Investigate Failures + Implement Caching
**Status**: 20% Complete (10/50 hours)
**Priority**: CRITICAL

---

## ⚠️ CRITICAL CORRECTION

**DO NOT MODIFY** [`test/dwarfs_test.cpp`](../test/dwarfs_test.cpp) or `build_dwarfs()` function!

**Why**: The [`scanner_options::metadata_format`](../include/dwarfs/writer/scanner_options.h:58-65) field **already defaults to FLATBUFFERS** when available. The code is already correct.

**Real Issue**: Test failures must have a different root cause. We need to:
1. Run tests and capture ACTUAL error messages
2. Understand why tests fail despite correct defaults
3. Implement caching WITHOUT breaking existing test logic

---

## Session Context

### What Was Accomplished ✅

**Test Framework** (1 hour):
- ✅ Created [`test/format_test_base.h`](../test/format_test_base.h)

**Analysis** (2 hours):
- ✅ Analyzed 1,110 test failures
- ✅ Created [`doc/TEST_FAILURE_ANALYSIS.md`](TEST_FAILURE_ANALYSIS.md)
- ✅ Identified that scanner_options defaults are correct

**Planning** (1 hour):
- ✅ Created continuation plan
- ✅ Created implementation status tracker
- ✅ Created next session guide

### Current State

**Build**: ✅ Compiles successfully (334/334 targets)
**Tests**: ⚠️ 65% pass rate (2,022/3,132) - need to understand WHY
**scanner_options**: ✅ Already defaults to FLATBUFFERS

---

## Your Task: Investigate & Implement

### Step 1: Run Tests and Capture Real Errors (30 min) ⚠️ CRITICAL

**DO NOT assume** you know why tests fail. Capture actual error messages:

```bash
cd build

# Run specific failing test with full output
ctest -R "filesystem.uid_gid_32bit" --output-on-failure

# Capture full error details
ctest --output-on-failure 2>&1 | tee /tmp/test_errors_detail.log

# Look for actual error patterns
grep -A 10 "FAILED" /tmp/test_errors_detail.log | head -50
```

**Analyze**:
- What is the ACTUAL error message?
- Does it mention "format"? "schema"? "thrift"?
- Is it a file loading error? A data mismatch? An assertion failure?

**Document findings**:
```bash
cat > doc/TEST_FAILURE_ROOT_CAUSE.md << 'EOF'
# Actual Test Failure Root Cause

## Sample Failures

### filesystem.uid_gid_32bit
**Error**: [paste actual error]
**Analysis**: [why it's failing]
**Fix**: [what needs to change]

### [other failing tests]
...
EOF
```

### Step 2: Implement Test Fixture Caching (6 hours)

**IMPORTANT**: Implement caching WITHOUT modifying existing test logic!

**Architecture**:

```cpp
// test/test_fixtures.h
namespace dwarfs::test {

/**
 * Cached Test Fixtures Manager
 *
 * Singleton class that manages cached filesystem images for tests.
 * Uses Strategy Pattern for different fixture types.
 *
 * Architecture:
 * - Single Responsibility: Only manages caching
 * - Open/Closed: Easy to add new fixtures via registration
 * - Strategy Pattern: Different generators for different fixtures
 * - Thread-safe: Mutex-protected cache access
 */
class CachedTestFixtures {
 public:
  static CachedTestFixtures& instance();

  /**
   * Get cached image or create if doesn't exist
   *
   * @param name Fixture name (e.g., "basic_test_data")
   * @param format Metadata format (FLATBUFFERS or THRIFT_COMPACT)
   * @return Path to cached filesystem image
   */
  std::filesystem::path get_image(
      std::string const& name,
      metadata::serialization::SerializationFormat format);

  /**
   * Force regeneration of all cached images
   * Use when test data changes or cache is corrupted
   */
  void regenerate_all();

  /**
   * Force regeneration of specific image
   */
  void regenerate(std::string const& name,
                  metadata::serialization::SerializationFormat format);

  /**
   * Register custom fixture generator
   *
   * @param name Fixture name
   * @param generator Function that creates the filesystem image
   */
  void register_generator(
      std::string name,
      std::function<void(std::filesystem::path const&,
                        metadata::serialization::SerializationFormat)> generator);

 private:
  CachedTestFixtures();
  ~CachedTestFixtures() = default;

  // Non-copyable
  CachedTestFixtures(CachedTestFixtures const&) = delete;
  CachedTestFixtures& operator=(CachedTestFixtures const&) = delete;

  std::filesystem::path cache_dir_;
  std::mutex mutex_;

  using ImageGenerator = std::function<void(
      std::filesystem::path const& output_path,
      metadata::serialization::SerializationFormat format)>;

  std::unordered_map<std::string, ImageGenerator> generators_;

  void register_builtin_generators();
  void generate_image(std::string const& name,
                     metadata::serialization::SerializationFormat format,
                     std::filesystem::path const& output_path);
  bool is_valid_cache(std::string const& name,
                     metadata::serialization::SerializationFormat format) const;
  std::filesystem::path get_cache_path(
      std::string const& name,
      metadata::serialization::SerializationFormat format) const;
};

} // namespace dwarfs::test
```

**Key Design Points**:
1. **DO NOT modify existing tests** - add caching as opt-in
2. **Preserve test logic** - caching is transparent to tests
3. **Thread-safe** - tests may run in parallel
4. **Format-aware** - separate cache for each format
5. **Invalidation** - via `TEST_REGENERATE_FIXTURES=1` env var

### Step 3: Fix Actual Test Failures (varies based on investigation)

**Based on Step 1 findings**, implement appropriate fixes:

**If tests fail due to format detection**:
- Check if images need format parameter
- Verify serializer registry is working

**If tests fail due to missing features**:
- Make features conditional on format availability

**If tests fail due to data mismatches**:
- Check if test expectations need updating for FlatBuffers format

---

## Success Criteria

### After Step 1 (Investigation)
- [ ] Understand ACTUAL root cause of test failures
- [ ] Document specific error patterns
- [ ] Identify correct fix strategy

### After Step 2 (Caching)
- [ ] `CachedTestFixtures` class implemented
- [ ] Cache directory managed by CMake
- [ ] Unit tests for caching pass
- [ ] Sample tests converted

### After Step 3 (Fixes)
- [ ] **FlatBuffers-only: 100% pass rate**
- [ ] **Both-formats: 100% pass rate**
- [ ] Test execution time <90s

---

## Key Principles (REINFORCED)

### 1. Correctness Over Speed
- **NEVER delete test logic** to make tests pass
- **NEVER lower thresholds** to hide failures
- **ALWAYS preserve test coverage** - understand and fix root cause

### 2. Minimal Changes
- **DO NOT modify** working test code unnecessarily
- **ADD features** (caching) without breaking existing
- **ISOLATE changes** - caching is separate concern

### 3. Architecture First
- **Use Strategy Pattern** for fixture generators
- **Use Singleton** for cache manager
- **Separate Concerns**: Caching vs testing vs image creation

---

## Timeline (COMPRESSED)

**This Session** (16 hours):
- Investigation (30min)
- Caching implementation (6h)
- Test conversion (8h)
- Validation (1.5h)

**Next Session** (10 hours):
- Documentation updates

**Final Session** (2 hours):
- Release preparation

**Total**: 28 hours remaining
**Target**: 2025-12-20 (1 week)

---

**Created**: 2025-12-13 20:32 HKT
**Status**: Ready for next session
**Priority**: 🔴 **CRITICAL** - Investigate actual errors first, do NOT modify tests blindly