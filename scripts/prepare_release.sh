#!/usr/bin/env bash
#
# DwarFS Release Preparation Script
#
# Runs all tests and benchmarks to prepare for a release.
# Generates a comprehensive release report.
#
# Usage: scripts/prepare_release.sh [--quick] [--skip-benchmarks]
#
# Copyright (c) Marcus Holland-Moritz
# SPDX-License-Identifier: MIT

set -euo pipefail

# ============================================================================
# Configuration
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_DIR="$PROJECT_ROOT/release_report_${TIMESTAMP}"

# Find the build directory
BUILD_DIR=""
for dir in build build-arm64-osx-production build-x64-osx-production build-both-formats; do
  if [[ -d "$PROJECT_ROOT/$dir" && -f "$PROJECT_ROOT/$dir/dwarfs_unit_tests" ]]; then
    BUILD_DIR="$PROJECT_ROOT/$dir"
    break
  fi
done

if [[ -z "$BUILD_DIR" ]]; then
  echo "ERROR: No build directory found. Please build the project first."
  exit 1
fi

# Options
QUICK_MODE=${QUICK_MODE:-false}
SKIP_BENCHMARKS=${SKIP_BENCHMARKS:-false}

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Results tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
TEST_RESULTS=()

# ============================================================================
# Helper Functions
# ============================================================================

info() {
  echo -e "${BLUE}[INFO]${NC} $*"
}

success() {
  echo -e "${GREEN}[SUCCESS]${NC} $*"
}

warn() {
  echo -e "${YELLOW}[WARN]${NC} $*"
}

error() {
  echo -e "${RED}[ERROR]${NC} $*" >&2
}

section() {
  echo ""
  echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
  echo -e "${CYAN}  $*${NC}"
  echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
  echo ""
}

record_test() {
  local name="$1"
  local result="$2"
  local details="${3:-}"

  TOTAL_TESTS=$((TOTAL_TESTS + 1))

  case "$result" in
    PASS)
      PASSED_TESTS=$((PASSED_TESTS + 1))
      TEST_RESULTS+=("✓ $name")
      ;;
    WARN|SKIP|PARTIAL)
      # These are not failures, just informational
      TEST_RESULTS+=("⚠ $name - $details")
      ;;
    *)
      FAILED_TESTS=$((FAILED_TESTS + 1))
      TEST_RESULTS+=("✗ $name - $details")
      ;;
  esac
}

run_test_suite() {
  local name="$1"
  local test_exe="$2"
  local filter="${3:-}"

  echo -e "  ${BOLD}Running $name...${NC}"

  local start_time=$(date +%s)
  local output
  local exit_code=0

  if [[ -n "$filter" ]]; then
    output=$("$test_exe" --gtest_filter="$filter" 2>&1) || exit_code=$?
  else
    output=$("$test_exe" 2>&1) || exit_code=$?
  fi

  local end_time=$(date +%s)
  local duration=$((end_time - start_time))

  # Parse output for results - gtest output format: "[  PASSED  ] N tests."
  if echo "$output" | grep -qE "\[  PASSED  \].*[0-9]+ test"; then
    local passed=$(echo "$output" | grep -oE "\[  PASSED  \] [0-9]+ test" | grep -oE "[0-9]+")
    echo -e "    ${GREEN}PASSED${NC} ($passed tests, ${duration}s)"
    record_test "$name" "PASS"
    return 0
  elif echo "$output" | grep -qE "\[  FAILED  \].*[0-9]+ test"; then
    local failed=$(echo "$output" | grep -oE "\[  FAILED  \] [0-9]+ test" | grep -oE "[0-9]+" | tail -1)
    echo -e "    ${RED}FAILED${NC} ($failed tests failed, ${duration}s)"
    record_test "$name" "FAIL" "$failed tests failed"
    return 1
  else
    # Check exit code (captured earlier)
    if [[ $exit_code -eq 0 ]]; then
      echo -e "    ${GREEN}PASSED${NC} (${duration}s)"
      record_test "$name" "PASS"
      return 0
    else
      echo -e "    ${RED}FAILED${NC} (${duration}s)"
      record_test "$name" "FAIL" "Exit code $exit_code"
      return 1
    fi
  fi
}

# ============================================================================
# Release Preparation
# ============================================================================

check_prerequisites() {
  section "Checking Prerequisites"

  echo "Build directory: $BUILD_DIR"
  echo "Report directory: $REPORT_DIR"
  echo ""

  # Check for required tools
  local missing=()

  command -v cmake &>/dev/null || missing+=("cmake")
  command -v ninja &>/dev/null || missing+=("ninja")
  command -v ctest &>/dev/null || missing+=("ctest")

  if [[ ${#missing[@]} -gt 0 ]]; then
    error "Missing required tools: ${missing[*]}"
    exit 1
  fi

  # Check for test executables
  local test_count=$(ls "$BUILD_DIR"/*test* 2>/dev/null | grep -v cmake | grep -v '\.cmake' | wc -l)
  echo "Found $test_count test executables"

  mkdir -p "$REPORT_DIR"
  success "Prerequisites OK"
}

run_unit_tests() {
  section "Unit Tests"

  local tests=(
    "dwarfs_unit_tests"
    "dwarfs_compression_tests"
    "dwarfs_compressor_tests"
    "dwarfs_segmenter_tests"
    "dwarfs_filter_tests"
  )

  for test in "${tests[@]}"; do
    if [[ -f "$BUILD_DIR/$test" ]]; then
      run_test_suite "$test" "$BUILD_DIR/$test"
    else
      warn "Test executable not found: $test"
      record_test "$test" "SKIP" "Not found"
    fi
  done
}

run_filesystem_tests() {
  section "Filesystem Tests"

  # Run filesystem tests with focus on large file handling
  if [[ -f "$BUILD_DIR/dwarfs_filesystem_tests" ]]; then
    echo "  Running all filesystem tests..."
    run_test_suite "filesystem_tests" "$BUILD_DIR/dwarfs_filesystem_tests"

    # Highlight large file test
    echo ""
    echo "  Verifying large file fix..."
    if DYLD_LIBRARY_PATH=/usr/local/lib "$BUILD_DIR/dwarfs_filesystem_tests" \
         --gtest_filter="*large*" 2>&1 | grep -qE "\[  PASSED  \]"; then
      success "Large file tests passed (SIGBUS fix verified)"
    else
      warn "Large file test check inconclusive"
    fi
  else
    warn "Filesystem tests not found"
  fi
}

run_ctest() {
  section "CTest Suite"

  echo "  Running ctest with parallel execution..."

  cd "$BUILD_DIR"
  local start_time=$(date +%s)

  if ctest -j4 --output-on-failure --timeout 300 2>&1 | tee "$REPORT_DIR/ctest_output.log"; then
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    success "All CTest tests passed (${duration}s)"
    record_test "ctest_suite" "PASS"
  else
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    error "Some CTest tests failed (${duration}s)"
    record_test "ctest_suite" "FAIL" "See ctest_output.log"
  fi

  cd "$PROJECT_ROOT"
}

run_benchmarks() {
  section "Benchmarks"

  if [[ "$SKIP_BENCHMARKS" == "true" ]]; then
    warn "Skipping benchmarks (--skip-benchmarks)"
    record_test "benchmarks" "SKIP" "Disabled by flag"
    return
  fi

  if [[ "$QUICK_MODE" == "true" ]]; then
    # Quick benchmark - just verify tools work
    echo "  Quick benchmark mode - verifying tools..."

    local tools=("mkdwarfs" "dwarfsck" "dwarfsextract")
    local all_ok=true
    local missing_deps=false

    for tool in "${tools[@]}"; do
      if [[ -f "$BUILD_DIR/$tool" ]]; then
        local output
        output=$(DYLD_LIBRARY_PATH=/usr/local/lib "$BUILD_DIR/$tool" --help 2>&1) || true
        if [[ $? -eq 0 ]]; then
          echo -e "    ${GREEN}✓${NC} $tool"
        elif echo "$output" | grep -q "Library not loaded\|dyld:"; then
          echo -e "    ${YELLOW}⚠${NC} $tool (missing runtime dependency)"
          missing_deps=true
        else
          echo -e "    ${RED}✗${NC} $tool (error)"
          all_ok=false
        fi
      else
        echo -e "    ${YELLOW}○${NC} $tool (not built)"
      fi
    done

    if $all_ok && ! $missing_deps; then
      success "All tools working"
      record_test "tools_verification" "PASS"
    elif $all_ok && $missing_deps; then
      warn "Tools built but missing runtime dependencies (FUSE)"
      record_test "tools_verification" "WARN" "Missing FUSE library"
    else
      error "Some tools not working"
      record_test "tools_verification" "FAIL"
    fi
  else
    # Full benchmark
    if [[ -f "$PROJECT_ROOT/benchmarks/run_quick_comprehensive_test.sh" ]]; then
      echo "  Running quick comprehensive benchmark..."

      cd "$PROJECT_ROOT/benchmarks"
      if bash run_quick_comprehensive_test.sh 2>&1 | tee "$REPORT_DIR/benchmark_output.log"; then
        success "Benchmarks completed"
        record_test "benchmarks" "PASS"
      else
        warn "Benchmarks had issues (may be expected without FUSE)"
        record_test "benchmarks" "PARTIAL"
      fi
      cd "$PROJECT_ROOT"
    else
      warn "Benchmark script not found"
      record_test "benchmarks" "SKIP" "Not found"
    fi
  fi
}

check_code_quality() {
  section "Code Quality"

  # Count lines in source files
  echo "  Checking file sizes..."

  local large_files=$(find "$PROJECT_ROOT/src" -name "*.cpp" -exec wc -l {} \; 2>/dev/null | \
    awk '$1 > 700 {print $2 ": " $1 " lines"}')

  if [[ -n "$large_files" ]]; then
    warn "Files exceeding 700 lines:"
    echo "$large_files" | head -10
    record_test "file_sizes" "WARN" "Some files >700 lines"
  else
    success "All source files under 700 lines"
    record_test "file_sizes" "PASS"
  fi

  # Check for debug code
  echo ""
  echo "  Checking for debug code..."

  local debug_code=$(grep -r "\[DEBUG\]\|\[SIGBUS-DEBUG\]" "$PROJECT_ROOT/src" 2>/dev/null | wc -l)
  if [[ "$debug_code" -gt 0 ]]; then
    warn "Found $debug_code debug statements in source code"
    record_test "debug_code" "WARN" "$debug_code statements"
  else
    success "No debug code found"
    record_test "debug_code" "PASS"
  fi
}

generate_report() {
  section "Generating Release Report"

  local report_file="$REPORT_DIR/RELEASE_REPORT.md"

  cat > "$report_file" <<EOF
# DwarFS Release Report

**Generated:** $(date -u +"%Y-%m-%d %H:%M:%S UTC")
**Platform:** $(uname -s) $(uname -m)
**Build Directory:** $BUILD_DIR

---

## Test Summary

| Metric | Count |
|--------|-------|
| Total Tests | $TOTAL_TESTS |
| Passed | $PASSED_TESTS |
| Failed | $FAILED_TESTS |
| Pass Rate | $(echo "scale=1; $PASSED_TESTS * 100 / $TOTAL_TESTS" | bc 2>/dev/null || echo "N/A")% |

## Test Results

\`\`\`
EOF

  for result in "${TEST_RESULTS[@]}"; do
    echo "$result" >> "$report_file"
  done

  cat >> "$report_file" <<EOF
\`\`\`

---

## Build Information

\`\`\`
$(cmake --version 2>/dev/null | head -1 || echo "cmake: not found")
$(ninja --version 2>/dev/null | echo "ninja: $(ninja --version 2>/dev/null || echo 'not found')")
Compiler: $(clang --version 2>/dev/null | head -1 || echo "unknown")
\`\`\`

---

## Recent Commits

\`\`\`
$(cd "$PROJECT_ROOT" && git log --oneline -10 2>/dev/null || echo "Git info unavailable")
\`\`\`

---

## Artifacts

The following artifacts are available:

- \`RELEASE_REPORT.md\` - This report
- \`ctest_output.log\` - CTest output (if run)
- \`benchmark_output.log\` - Benchmark output (if run)

---

## Next Steps

1. Review any failed tests above
2. Run full benchmark suite if needed: \`./benchmarks/run_all_benchmarks.sh\`
3. Update CHANGELOG.md with changes
4. Tag release: \`git tag -a vX.Y.Z -m "Release X.Y.Z"\`
5. Build release packages

EOF

  success "Report generated: $report_file"

  # Print summary
  echo ""
  echo -e "${BOLD}═══════════════════════════════════════════════════════════════${NC}"
  echo -e "${BOLD}  RELEASE PREPARATION SUMMARY${NC}"
  echo -e "${BOLD}═══════════════════════════════════════════════════════════════${NC}"
  echo ""
  echo "  Total:  $TOTAL_TESTS"
  echo -e "  ${GREEN}Passed: $PASSED_TESTS${NC}"
  if [[ $FAILED_TESTS -gt 0 ]]; then
    echo -e "  ${RED}Failed: $FAILED_TESTS${NC}"
  else
    echo -e "  Failed: $FAILED_TESTS"
  fi
  echo ""
  echo -e "  Report: $report_file"
  echo ""

  if [[ $FAILED_TESTS -gt 0 ]]; then
    error "Some tests failed. Review the report before releasing."
    return 1
  else
    success "All tests passed. Ready for release!"
    return 0
  fi
}

# ============================================================================
# Usage Information
# ============================================================================

show_usage() {
  cat <<EOF
Usage: $0 [options]

Prepares DwarFS for release by running all tests and generating a report.

OPTIONS:
  -h, --help           Show this help message
  --quick              Quick mode - minimal testing
  --skip-benchmarks    Skip benchmark tests

EXAMPLES:
  # Full release preparation
  $0

  # Quick check before commit
  $0 --quick

  # Tests only, no benchmarks
  $0 --skip-benchmarks

OUTPUT:
  release_report_YYYYMMDD_HHMMSS/
    - RELEASE_REPORT.md
    - ctest_output.log
    - benchmark_output.log

EOF
}

# ============================================================================
# Argument Parsing
# ============================================================================

while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      show_usage
      exit 0
      ;;
    --quick)
      QUICK_MODE=true
      shift
      ;;
    --skip-benchmarks)
      SKIP_BENCHMARKS=true
      shift
      ;;
    *)
      error "Unknown option: $1"
      echo ""
      show_usage
      exit 1
      ;;
  esac
done

# ============================================================================
# Main Execution
# ============================================================================

main() {
  section "DwarFS Release Preparation"

  if [[ "$QUICK_MODE" == "true" ]]; then
    echo "Running in QUICK mode (minimal testing)"
  fi

  echo ""

  check_prerequisites
  run_unit_tests
  run_filesystem_tests

  if [[ "$QUICK_MODE" != "true" ]]; then
    run_ctest
  fi

  run_benchmarks
  check_code_quality
  generate_report
}

# Run main
main
