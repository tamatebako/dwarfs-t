# Session 36 - Phase 1 Implementation Prompt

**Date**: 2025-12-24  
**Mode**: Code  
**Estimated Time**: 1 hour  
**Goal**: Fix test registration, add production/debug modes, fix macOS compatibility

---

## Context

The build/test/benchmark system has three critical issues:

1. **Tests show as "NOT_BUILT"** even with `-DWITH_TESTS=ON` because test targets aren't being built
2. **No production vs debug mode** - scripts always try to run tests even for production builds
3. **macOS incompatibility** - benchmark script uses GNU-specific commands that don't exist on macOS

See full analysis in [`SESSION_36_BUILD_SYSTEM_CLEANUP_PLAN.md`](SESSION_36_BUILD_SYSTEM_CLEANUP_PLAN.md)

---

## Task 1: Fix Test Registration

### Problem
```bash
# Current behavior:
$ ./scripts/build-all-and-test.sh
...
Building...
✓ Build successful
Testing...
Could not find executable dwarfs_filesystem_tests_NOT_BUILT
```

**Root Cause**: [`scripts/build-all-and-test.sh:72`](../scripts/build-all-and-test.sh#L72) only builds tool targets, not test targets:

```bash
cmake --build "$build_dir" --target mkdwarfs dwarfsck dwarfsextract -j"$JOBS"
```

### Solution

Update [`scripts/build-all-and-test.sh`](../scripts/build-all-and-test.sh):

```bash
# Around line 70-79, replace the build section:

  # Build
  echo -e "${YELLOW}Building...${NC}"
  
  # Build tools (always)
  if cmake --build "$build_dir" --target mkdwarfs dwarfsck dwarfsextract -j"$JOBS"; then
    echo -e "${GREEN}✓ Tools built successfully${NC}"
  else
    echo -e "${RED}✗ Tool build failed${NC}"
    BUILD_RESULTS[$name]="FAIL"
    return 1
  fi
  
  # Build tests if enabled
  if [[ "${BUILD_TESTS:-ON}" == "ON" ]]; then
    echo -e "${YELLOW}Building tests...${NC}"
    if cmake --build "$build_dir" --target dwarfs_filesystem_tests dwarfs_unit_tests -j"$JOBS"; then
      echo -e "${GREEN}✓ Tests built successfully${NC}"
      BUILD_RESULTS[$name]="PASS"
    else
      echo -e "${RED}✗ Test build failed${NC}"
      BUILD_RESULTS[$name]="FAIL"
      return 1
    fi
  else
    echo -e "${YELLOW}⊘ Tests skipped (production mode)${NC}"
    BUILD_RESULTS[$name]="PASS"
  fi
```

---

## Task 2: Add Production/Debug Modes

### Problem
Scripts always configure with `-DWITH_TESTS=ON` and run `ctest`, even for production builds where tests aren't needed.

### Solution

#### 2.1 Update [`scripts/run-all.sh`](../scripts/run-all.sh)

Add mode detection at the top:

```bash
#!/usr/bin/env bash
# Master script: Clean, Build, Test, and Benchmark all configurations
# Usage: 
#   ./scripts/run-all.sh [dataset_path]         # Production mode (no tests)
#   ./scripts/run-all.sh --debug [dataset_path] # Debug mode (with tests)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Parse arguments
DEBUG_MODE=false
DATASET_PATH=""

for arg in "$@"; do
  case $arg in
    --debug)
      DEBUG_MODE=true
      shift
      ;;
    *)
      DATASET_PATH="$arg"
      ;;
  esac
done

# Export mode for child scripts
export BUILD_TESTS="OFF"
export RUN_TESTS="false"
if [[ "$DEBUG_MODE" == "true" ]]; then
  BUILD_TESTS="ON"
  RUN_TESTS="true"
  echo "🐛 DEBUG MODE: Tests enabled"
else
  echo "🚀 PRODUCTION MODE: Tests disabled"
fi

echo "════════════════════════════════════════"
echo "DwarFS: Clean → Build → Test → Benchmark"
echo "════════════════════════════════════════"
echo

# ... rest of script unchanged ...

# Step 3: Benchmark (pass dataset path if provided)
echo "📊 Running benchmarks..."
if [[ -n "$DATASET_PATH" ]]; then
  "$SCRIPT_DIR/benchmark-all.sh" "$DATASET_PATH"
else
  "$SCRIPT_DIR/benchmark-all.sh"
fi
```

#### 2.2 Update [`scripts/build-all-and-test.sh`](../scripts/build-all-and-test.sh)

Read the environment variables:

```bash
# Around line 24-26, after configuration section:

# Configuration
JOBS=${JOBS:-8}
CMAKE_GENERATOR=${CMAKE_GENERATOR:-Ninja}
BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_TESTS=${BUILD_TESTS:-OFF}  # NEW: from parent script
RUN_TESTS=${RUN_TESTS:-false}    # NEW: from parent script

echo "Build configuration:"
echo "  - Tests: ${BUILD_TESTS} (run: ${RUN_TESTS})"
echo "  - Jobs: ${JOBS}"
echo "  - Type: ${BUILD_TYPE}"
echo
```

Update configure step (around line 57-62):

```bash
  # Configure
  echo -e "${YELLOW}Configuring...${NC}"
  if cmake -B "$build_dir" -G"$CMAKE_GENERATOR" \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DDWARFS_WITH_FLATBUFFERS="$fb" \
      -DDWARFS_WITH_THRIFT="$thrift" \
      -DWITH_TESTS="$BUILD_TESTS" \
      -DWITH_BENCHMARKS="$BUILD_TESTS"; then  # Benchmarks follow tests
    echo -e "${GREEN}✓ Configuration successful${NC}"
  else
    echo -e "${RED}✗ Configuration failed${NC}"
    BUILD_RESULTS[$name]="FAIL"
    return 1
  fi
```

Update test step (around line 81-90):

```bash
  # Test (only if enabled)
  if [[ "$RUN_TESTS" == "true" ]]; then
    echo -e "${YELLOW}Testing...${NC}"
    if ctest --test-dir "$build_dir" --output-on-failure -j"$JOBS"; then
      echo -e "${GREEN}✓ Tests passed${NC}"
      TEST_RESULTS[$name]="PASS"
    else
      echo -e "${RED}✗ Tests failed${NC}"
      TEST_RESULTS[$name]="FAIL"
      return 1
    fi
  else
    echo -e "${YELLOW}⊘ Tests skipped (production mode)${NC}"
    TEST_RESULTS[$name]="SKIP"
  fi
```

---

## Task 3: Fix macOS Compatibility

### Problem
[`scripts/benchmark-all.sh`](../scripts/benchmark-all.sh) uses GNU-specific commands:

1. `stat -c%s` (line 80) - GNU stat syntax
2. `du -sb` (line 98, 185) - `-b` flag doesn't exist on macOS
3. `numfmt` (line 113, 151) - Command doesn't exist on macOS

### Solution

Add compatibility functions at the top of [`scripts/benchmark-all.sh`](../scripts/benchmark-all.sh):

```bash
#!/usr/bin/env bash
# Benchmark all three metadata serialization configurations
# Usage: ./scripts/benchmark-all.sh [dataset_path]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# ============================================================================
# Cross-platform Compatibility Functions
# ============================================================================

# Get file size in bytes (cross-platform)
get_file_size() {
  local file=$1
  if [[ "$OSTYPE" == "darwin"* ]]; then
    stat -f%z "$file"
  else
    stat -c%s "$file"
  fi
}

# Get directory size in bytes (cross-platform)
get_dir_size() {
  local dir=$1
  if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS: use find + stat + awk
    find "$dir" -type f -exec stat -f%z {} + 2>/dev/null | awk '{sum+=$1} END {print sum}'
  else
    # Linux: use du -sb
    du -sb "$dir" 2>/dev/null | cut -f1
  fi
}

# Format bytes to human-readable (cross-platform)
format_bytes() {
  local bytes=$1
  if command -v numfmt >/dev/null 2>&1; then
    # GNU numfmt available
    numfmt --to=iec-i --suffix=B "$bytes"
  else
    # Fallback: awk-based formatter
    awk -v bytes="$bytes" 'BEGIN {
      split("B KiB MiB GiB TiB", units, " ")
      i = 1
      val = bytes
      while (val >= 1024 && i < 5) {
        val /= 1024
        i++
      }
      printf "%.1f%s", val, units[i]
    }'
  fi
}

# ============================================================================
# Rest of script...
# ============================================================================
```

Replace usages throughout the file:

**Line 80** (in `benchmark_config` function):
```bash
# OLD:
local image_size=$(stat -f%z "$image" 2>/dev/null || stat -c%s "$image" 2>/dev/null)

# NEW:
local image_size=$(get_file_size "$image")
```

**Line 98** (in `benchmark_config` function):
```bash
# OLD:
local dataset_size=$(du -sb "$DATASET" | cut -f1)

# NEW:
local dataset_size=$(get_dir_size "$DATASET")
```

**Line 113** (in `benchmark_config` function):
```bash
# OLD:
echo "  - Image size: $(numfmt --to=iec-i --suffix=B $image_size)"

# NEW:
echo "  - Image size: $(format_bytes $image_size)"
```

**Line 151** (in report generation):
```bash
# OLD:
image_size_h=$(numfmt --to=iec-i --suffix=B $image_size)

# NEW:
image_size_h=$(format_bytes $image_size)
```

**Line 185-186** (in report generation):
```bash
# OLD:
dataset_size=$(du -sb "$DATASET" | cut -f1)
dataset_size_h=$(numfmt --to=iec-i --suffix=B $dataset_size)

# NEW:
dataset_size=$(get_dir_size "$DATASET")
dataset_size_h=$(format_bytes $dataset_size)
```

---

## Testing Checklist

After implementing all changes, verify:

### Test 1: Production Mode
```bash
./scripts/run-all.sh example/pg11339-h
```

**Expected**:
- ✅ Shows "🚀 PRODUCTION MODE: Tests disabled"
- ✅ No test compilation
- ✅ No ctest execution
- ✅ Benchmark completes with "✨ All done!"
- ✅ Creates `benchmarks/results/*/benchmark-report.md`

### Test 2: Debug Mode
```bash
./scripts/run-all.sh --debug example/pg11339-h
```

**Expected**:
- ✅ Shows "🐛 DEBUG MODE: Tests enabled"
- ✅ Tests compile successfully
- ✅ ctest runs and passes
- ✅ Benchmark completes with "✨ All done!"

### Test 3: macOS Compatibility
```bash
# On macOS:
./scripts/benchmark-all.sh example/pg11339-h
cat benchmarks/results/*/benchmark-report.md
```

**Expected**:
- ✅ No "command not found" errors
- ✅ File sizes display correctly (e.g., "4.5MiB")
- ✅ Benchmark report contains valid data
- ✅ No errors about `du -b` or `numfmt`

---

## Success Criteria

✅ **Phase 1 Complete** when:
1. Production mode runs WITHOUT building/running tests
2. Debug mode builds AND runs tests successfully  
3. Benchmark script completes on macOS with all functionality working
4. No "NOT_BUILT" or "command not found" errors
5. All three build configs (fb-only, both, thrift-only) work in both modes

---

## Files to Modify

1. ✅ [`scripts/run-all.sh`](../scripts/run-all.sh) - Add `--debug` flag parsing
2. ✅ [`scripts/build-all-and-test.sh`](../scripts/build-all-and-test.sh) - Conditional test building
3. ✅ [`scripts/benchmark-all.sh`](../scripts/benchmark-all.sh) - Add compatibility functions

**Total Changes**: ~150 lines across 3 files

---

## Next Session

After Phase 1 completes successfully, we can optionally proceed with:
- **Phase 2**: vcpkg integration (2-3 hours)
- **Phase 3**: Build optimization (0.5 hour)

See [`SESSION_36_BUILD_SYSTEM_CLEANUP_PLAN.md`](SESSION_36_BUILD_SYSTEM_CLEANUP_PLAN.md) for details.

---

**Ready to implement**: Switch to Code mode and start with Task 1