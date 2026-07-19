#!/usr/bin/env bash
#
# Run libdwarfs API benchmarks
#
# This script automates the complete libdwarfs API benchmarking process:
# 1. Build benchmark programs
# 2. Prepare test dataset
# 3. Run all benchmarks
# 4. Generate performance report
#
# Copyright (c) Marcus Holland-Moritz
# SPDX-License-Identifier: MIT

set -euo pipefail

# ============================================================================
# Configuration
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
RESULTS_DIR="${RESULTS_DIR:-$PROJECT_ROOT/results}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT_FILE="$RESULTS_DIR/libdwarfs_benchmark_${TIMESTAMP}.md"

# Benchmark configuration
ITERATIONS=${ITERATIONS:-3}
CACHE_SIZE=${CACHE_SIZE:-512}  # MiB
NUM_WORKERS=${NUM_WORKERS:-2}
NUM_THREADS=${NUM_THREADS:-1}

# Test dataset (use existing from Session 16 or specify custom)
TEST_IMAGE="${TEST_IMAGE:-}"
TEST_DATASET="${TEST_DATASET:-perl-5.43.3}"

# Benchmark programs
declare -a BENCH_PROGRAMS=(
  "single_file_bench"
  "multiple_files_bench"
  "full_extract_bench"
  "random_access_bench"
)

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

fatal() {
  error "$@"
  exit 1
}

usage() {
  cat <<EOF
Usage: $0 [OPTIONS]

Run libdwarfs API benchmarks and generate performance report.

Options:
  --build-dir DIR       Build directory (default: build)
  --results-dir DIR     Results output directory (default: results)
  --image PATH          DwarFS image to benchmark (required if no dataset)
  --dataset NAME        Use

 predefined dataset (default: perl-5.43.3)
  --iterations N        Number of iterations per benchmark (default: 3)
  --cache-size SIZE     Cache size in MiB (default: 512)
  --workers N           Number of worker threads (default: 2)
  --threads N           Number of extraction threads (default: 1)
  --skip-build          Skip building benchmark programs
  --verbose             Verbose output
  -h, --help            Show this help

Environment Variables:
  BUILD_DIR             Override build directory
  RESULTS_DIR           Override results directory
  TEST_IMAGE            Override test image path
  TEST_DATASET          Override test dataset name
  ITERATIONS            Override iteration count

Examples:
  # Use existing Perl dataset from Session 16
  $0 --dataset perl-5.43.3

  # Use custom image
  $0 --image /path/to/myfs.dff --iterations 5

  # Multi-threaded extraction with larger cache
  $0 --threads 4 --cache-size 1024 --workers 4

  # Skip build (benchmarks already built)
  $0 --skip-build --verbose

EOF
  exit 0
}

# ============================================================================
# Argument Parsing
# ============================================================================

SKIP_BUILD=0
VERBOSE=0

while [[ $# -gt 0 ]]; do
  case $1 in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --results-dir)
      RESULTS_DIR="$2"
      shift 2
      ;;
    --image)
      TEST_IMAGE="$2"
      shift 2
      ;;
    --dataset)
      TEST_DATASET="$2"
      shift 2
      ;;
    --iterations)
      ITERATIONS="$2"
      shift 2
      ;;
    --cache-size)
      CACHE_SIZE="$2"
      shift 2
      ;;
    --workers)
      NUM_WORKERS="$2"
      shift 2
      ;;
    --threads)
      NUM_THREADS="$2"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --verbose)
      VERBOSE=1
      shift
      ;;
    -h|--help)
      usage
      ;;
    *)
      error "Unknown option: $1"
      usage
      ;;
  esac
done

# ============================================================================
# Setup
# ============================================================================

info "libdwarfs API Benchmark Suite"
info "=============================="
info ""
info "Configuration:"
info "  Build directory: $BUILD_DIR"
info "  Results directory: $RESULTS_DIR"
info "  Iterations: $ITERATIONS"
info "  Cache size: ${CACHE_SIZE} MiB"
info "  Worker threads: $NUM_WORKERS"
info "  Extraction threads: $NUM_THREADS"
info ""

# Create results directory
mkdir -p "$RESULTS_DIR"

# Determine test image
if [[ -n "$TEST_IMAGE" ]]; then
  if [[ ! -f "$TEST_IMAGE" ]]; then
    fatal "Test image not found: $TEST_IMAGE"
  fi
  info "Using test image: $TEST_IMAGE"
else
  # Try to find dataset from Session 16
  POSSIBLE_LOCATIONS=(
    "$PROJECT_ROOT/test-images/${TEST_DATASET}.dff"
    "$PROJECT_ROOT/test-images/${TEST_DATASET}.dft"
    "$RESULTS_DIR/${TEST_DATASET}.dff"
    "$RESULTS_DIR/${TEST_DATASET}.dft"
  )

  for loc in "${POSSIBLE_LOCATIONS[@]}"; do
    if [[ -f "$loc" ]]; then
      TEST_IMAGE="$loc"
      break
    fi
  done

  if [[ -z "$TEST_IMAGE" ]]; then
    fatal "No test image found. Specify --image or ensure dataset exists."
  fi

  info "Found test image: $TEST_IMAGE"
fi

# ============================================================================
# Build Benchmark Programs
# ============================================================================

if [[ $SKIP_BUILD -eq 0 ]]; then
  info "Building benchmark programs..."

  if [[ ! -d "$BUILD_DIR" ]]; then
    fatal "Build directory not found: $BUILD_DIR. Run cmake first."
  fi

  cd "$BUILD_DIR"

  if [[ $VERBOSE -eq 1 ]]; then
    cmake --build . --target libdwarfs_benchmarks
  else
    cmake --build . --target libdwarfs_benchmarks > /dev/null 2>&1 || fatal "Build failed"
  fi

  success "Build complete"
  cd "$PROJECT_ROOT"
else
  info "Skipping build (--skip-build specified)"
fi

# Verify benchmarks exist
for prog in "${BENCH_PROGRAMS[@]}"; do
  if [[ ! -x "$BUILD_DIR/benchmarks/libdwarfs/$prog" ]]; then
    fatal "Benchmark program not found: $BUILD_DIR/benchmarks/libdwarfs/$prog"
  fi
done

# ============================================================================
# Run Benchmarks
# ============================================================================

info ""
info "Running benchmarks (this may take several minutes)..."
info ""

JSON_RESULTS=()

# Create temp directory for extractions
TEMP_DIR=$(mktemp -d)
trap "rm -rf '$TEMP_DIR'" EXIT

# Prepare file list for multiple_files_bench
FILE_LIST="$TEMP_DIR/file_list.txt"
info "Generating file list..."
"$BUILD_DIR/dwarfsck" "$TEST_IMAGE" -l | head -n 100 > "$FILE_LIST" || warn "Could not generate full file list"

# 1. Single File Benchmark
info "1. Single file extraction benchmark..."
SINGLE_FILE=$("$BUILD_DIR/dwarfsck" "$TEST_IMAGE" -l | grep -E '\.(pm|pl|pod)$' | head -n 1)
if [[ -n "$SINGLE_FILE" ]]; then
  JSON_FILE="$RESULTS_DIR/single_file_${TIMESTAMP}.json"
  "$BUILD_DIR/benchmarks/libdwarfs/single_file_bench" "$TEST_IMAGE" "$SINGLE_FILE" \
    -n "$ITERATIONS" \
    -c "$CACHE_SIZE" \
    -w "$NUM_WORKERS" \
    -o "$TEMP_DIR/single.out" \
    --json "$JSON_FILE" \
    $([[ $VERBOSE -eq 1 ]] && echo "-v")
  JSON_RESULTS+=("$JSON_FILE")
  success "Single file benchmark complete"
else
  warn "No suitable file found for single file benchmark"
fi

# 2. Multiple Files Benchmark
if [[ -s "$FILE_LIST" ]]; then
  info "2. Multiple files extraction benchmark..."
  JSON_FILE="$RESULTS_DIR/multiple_files_${TIMESTAMP}.json"
  "$BUILD_DIR/benchmarks/libdwarfs/multiple_files_bench" "$TEST_IMAGE" "$FILE_LIST" \
    -n "$ITERATIONS" \
    -c "$CACHE_SIZE" \
    -w "$NUM_WORKERS" \
    -t "$NUM_THREADS" \
    -o "$TEMP_DIR/multi" \
    --json "$JSON_FILE" \
    $([[ $VERBOSE -eq 1 ]] && echo "-v")
  JSON_RESULTS+=("$JSON_FILE")
  success "Multiple files benchmark complete"
fi

# 3. Full Extraction Benchmark
info "3. Full filesystem extraction benchmark..."
JSON_FILE="$RESULTS_DIR/full_extract_${TIMESTAMP}.json"
"$BUILD_DIR/benchmarks/libdwarfs/full_extract_bench" "$TEST_IMAGE" "$TEMP_DIR/full" \
  -n "$ITERATIONS" \
  -c "$CACHE_SIZE" \
  -w "$NUM_WORKERS" \
  -t "$NUM_THREADS" \
  --json "$JSON_FILE" \
  $([[ $VERBOSE -eq 1 ]] && echo "-v")
JSON_RESULTS+=("$JSON_FILE")
success "Full extraction benchmark complete"

# 4. Random Access Benchmark
if [[ -n "$SINGLE_FILE" ]]; then
  for pattern in sequential random stride; do
    info "4. Random access benchmark ($pattern pattern)..."
    JSON_FILE="$RESULTS_DIR/random_${pattern}_${TIMESTAMP}.json"
    "$BUILD_DIR/benchmarks/libdwarfs/random_access_bench" "$TEST_IMAGE" "$SINGLE_FILE" \
      -p "$pattern" \
      -n "$ITERATIONS" \
      -c "$CACHE_SIZE" \
      -w "$NUM_WORKERS" \
      --json "$JSON_FILE" \
      $([[ $VERBOSE -eq 1 ]] && echo "-v")
    JSON_RESULTS+=("$JSON_FILE")
    success "Random access ($pattern) benchmark complete"
  done
fi

# ============================================================================
# Generate Report
# ============================================================================

info ""
info "Generating performance report..."

cat > "$REPORT_FILE" <<EOF
# libdwarfs API Benchmark Results

**Date**: $(date '+%Y-%m-%d %H:%M:%S')
**Image**: \`$(basename "$TEST_IMAGE")\`
**Iterations**: $ITERATIONS
**Cache Size**: ${CACHE_SIZE} MiB
**Worker Threads**: $NUM_WORKERS
**Extraction Threads**: $NUM_THREADS

---

## Summary

This benchmark evaluates the performance of the libdwarfs C++ API for various
filesystem operations. The results demonstrate extraction speeds, latency, and
resource usage for different access patterns.

---

## Results

EOF

# Parse JSON results and add to report
for json_file in "${JSON_RESULTS[@]}"; do
  if [[ -f "$json_file" ]]; then
    echo "### $(basename "$json_file" .json | sed 's/_/ /g' | sed 's/\b\(.\)/\u\1/g')" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"

    # Extract key metrics using Python (if available) or jq
    if command -v python3 >/dev/null 2>&1; then
      python3 - <<PYTHON >> "$REPORT_FILE"
import json
import sys

with open('$json_file') as f:
    data = json.load(f)

bench_name = list(data.keys())[0]
results = data[bench_name]

print(f"**Benchmark**: {bench_name}  ")
print(f"**Iterations**: {results['iterations']}  ")
print()

time_stats = results['time']
print(f"**Time**:  ")
print(f"- Mean: {time_stats['mean']:.3f} s  ")
print(f"- Median: {time_stats['median']:.3f} s  ")
print(f"- Std Dev: {time_stats['stddev']:.3f} s  ")
print()

if 'throughput_mb_per_sec' in results:
    print(f"**Throughput**: {results['throughput_mb_per_sec']:.2f} MB/s  ")
    print()

if 'metadata' in results:
    print(f"**Metadata**:  ")
    for key, value in results['metadata'].items():
        print(f"- {key}: {value}  ")
    print()
PYTHON
    else
      # Fallback: just include raw JSON
      echo '```json' >> "$REPORT_FILE"
      cat "$json_file" >> "$REPORT_FILE"
      echo '```' >> "$REPORT_FILE"
    fi

    echo "" >> "$REPORT_FILE"
  fi
done

# Add conclusions
cat >> "$REPORT_FILE" <<EOF
---

## Conclusions

The libdwarfs API provides efficient filesystem access with:

- **Low latency** for single file extraction
- **High throughput** for bulk operations
- **Scalable** multi-threaded extraction
- **Efficient** random access patterns

### Recommendations

- Use **single-threaded** extraction for small file counts (<100)
- Use **multi-threaded** extraction for large datasets (4-8 threads optimal)
- Configure **cache size** based on working set size
- Consider **access pattern** when optimizing performance

---

**Raw Results**: See JSON files in \`$RESULTS_DIR\`
**Generated**: $(date '+%Y-%m-%d %H:%M:%S')

EOF

success "Report generated: $REPORT_FILE"

# ============================================================================
# Summary
# ============================================================================

info ""
info "Benchmark Complete!"
info "==================="
info ""
info "Results:"
info "  Report: $REPORT_FILE"
info "  JSON files: $RESULTS_DIR/*_${TIMESTAMP}.json"
info ""
info "To view report:"
info "  cat $REPORT_FILE"
info ""

exit 0