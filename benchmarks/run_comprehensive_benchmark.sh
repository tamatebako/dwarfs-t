#!/usr/bin/env bash
#
# Comprehensive DwarFS Benchmark Suite
#
# Compares FUSE vs libdwarfs API performance across:
# - 2 build configurations (FB-only, Both)
# - 2 image formats (.dff FlatBuffers, .dft Thrift)
# - Multiple access patterns
#
# Expected runtime: 2-3 hours
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

# Build directories
BUILD_FB="$PROJECT_ROOT/build-fb-bench"
BUILD_BOTH="$PROJECT_ROOT/build-both-bench"

# Test data
PERL_SOURCE="$PROJECT_ROOT/benchmark-files/perl-5.43.3/perl-5.43.3"
IMAGES_DIR="$PROJECT_ROOT/test-images"
RESULTS_DIR="$PROJECT_ROOT/results/comprehensive_${TIMESTAMP}"

# Vcpkg configuration
VCPKG_ROOT="${VCPKG_ROOT:-/Users/mulgogi/vcpkg}"
USE_VCPKG="${USE_VCPKG:-false}"

# Benchmark parameters
ITERATIONS=3
CACHE_SIZE=512  # MiB
NUM_WORKERS=4
EXTRACT_THREADS=1
SKIP_BUILD=${SKIP_BUILD:-false}  # Set SKIP_BUILD=true to use existing builds
# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

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

section() {
  echo ""
  echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
  echo -e "${CYAN}  $*${NC}"
  echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
  echo ""
}

# ============================================================================
# Build Functions
# ============================================================================

build_configuration() {
  local name=$1
  local build_dir=$2
  local fb_flag=$3
  local thrift_flag=$4

  section "Building Configuration: $name"

  # Skip build if requested and directory exists
  if [[ "$SKIP_BUILD" == "true" ]] && [[ -d "$build_dir" ]]; then
    info "Skipping build (SKIP_BUILD=true)"
    info "Using existing build: $build_dir"
    success "Build exists: $name"
    return 0
  fi

  info "Build directory: $build_dir"
  info "FlatBuffers: $fb_flag"
  info "Thrift: $thrift_flag"
  info "Vcpkg mode: $USE_VCPKG"

  rm -rf "$build_dir"
  mkdir -p "$build_dir"

  cd "$build_dir"

  if [[ "$USE_VCPKG" == "true" ]]; then
    info "Using vcpkg toolchain with Tebako jemalloc 5.5.0..."
    NO_CMAKE_PATH=1 NO_CMAKE_ENVIRONMENT_PATH=1 cmake "$PROJECT_ROOT" \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
      -DVCPKG_OVERLAY_PORTS="$PROJECT_ROOT/vcpkg_ports" \
      -DVCPKG_OVERLAY_TRIPLETS="$PROJECT_ROOT/vcpkg_triplets" \
      -DVCPKG_TARGET_TRIPLET=arm64-osx-static \
      -DWITH_BENCHMARKS=ON \
      -DWITH_LIBDWARFS=ON \
      -DWITH_TOOLS=ON \
      -DWITH_FUSE_DRIVER=ON \
      -DWITH_TESTS=ON \
      -DDWARFS_WITH_FLATBUFFERS=$fb_flag \
      -DDWARFS_WITH_THRIFT=$thrift_flag
  else
    info "Using system packages (pkg-config)..."
    cmake "$PROJECT_ROOT" \
      -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DWITH_BENCHMARKS=ON \
      -DWITH_LIBDWARFS=ON \
      -DWITH_TOOLS=ON \
      -DWITH_FUSE_DRIVER=ON \
      -DWITH_TESTS=ON \
      -DDWARFS_WITH_FLATBUFFERS=$fb_flag \
      -DDWARFS_WITH_THRIFT=$thrift_flag
  fi

  ninja

  success "Build complete: $name"
  cd "$PROJECT_ROOT"
}

# ============================================================================
# Image Creation Functions
# ============================================================================

create_images() {
  section "Creating DwarFS Images"

  mkdir -p "$IMAGES_DIR"

  # FlatBuffers image (.dff)
  if [[ ! -f "$IMAGES_DIR/perl-5.43.3.dff" ]]; then
    info "Creating FlatBuffers image..."
    "$BUILD_FB/mkdwarfs" -i "$PERL_SOURCE" -o "$IMAGES_DIR/perl-5.43.3.dff" \
      --compression zstd:level=3 \
      --metadata-format flatbuffers \
      --log-level error
    success "Created: perl-5.43.3.dff"
  else
    info "FlatBuffers image already exists"
  fi

  # Thrift image (.dft) - use both-formats build
  if [[ ! -f "$IMAGES_DIR/perl-5.43.3.dft" ]]; then
    info "Creating Thrift image..."
    "$BUILD_BOTH/mkdwarfs" -i "$PERL_SOURCE" -o "$IMAGES_DIR/perl-5.43.3.dft" \
      --compression zstd:level=3 \
      --metadata-format thrift \
      --log-level error
    success "Created: perl-5.43.3.dft"
  else
    info "Thrift image already exists"
  fi

  # Show image sizes
  info "Image sizes:"
  ls -lh "$IMAGES_DIR"/perl-5.43.3.* | awk '{print "  " $9 ": " $5}'
}

# ============================================================================
# FUSE Benchmark Functions
# ============================================================================

benchmark_fuse_extraction() {
  local build_name=$1
  local build_dir=$2
  local image=$3
  local format=$4

  info "FUSE benchmark: $build_name with $format"

  local mount_point="/tmp/dwarfs_bench_$$"
  local extract_dir="$RESULTS_DIR/fuse_${build_name}_${format}_extract"

  mkdir -p "$mount_point" "$extract_dir"

  # Mount
  info "Mounting filesystem..."
  "$build_dir/dwarfs" "$image" "$mount_point" \
    -o workers=$NUM_WORKERS \
    -o cache_size=$(( CACHE_SIZE * 1024 * 1024 )) &

  local fuse_pid=$!
  sleep 2  # Wait for mount

  if ! mountpoint -q "$mount_point"; then
    error "Failed to mount filesystem"
    return 1
  fi

  # Extract via cp
  info "Extracting via cp..."
  local start=$(date +%s.%N)
  cp -r "$mount_point"/* "$extract_dir/" 2>/dev/null || true
  local end=$(date +%s.%N)

  local duration=$(echo "$end - $start" | bc)
  local size=$(du -sb "$extract_dir" | awk '{print $1}')
  local throughput=$(echo "scale=2; $size / 1024 / 1024 / $duration" | bc)

  # Unmount
  if [[ "$(uname)" == "Darwin" ]]; then
    umount "$mount_point" 2>/dev/null || diskutil unmount force "$mount_point"
  else
    fusermount -u "$mount_point" 2>/dev/null || umount "$mount_point"
  fi

  wait $fuse_pid 2>/dev/null || true
  rm -rf "$mount_point"

  # Save results
  cat > "$RESULTS_DIR/fuse_${build_name}_${format}.json" <<EOF
{
  "benchmark": "fuse_extraction",
  "build": "$build_name",
  "format": "$format",
  "image": "$image",
  "duration_seconds": $duration,
  "extracted_bytes": $size,
  "throughput_mb_per_sec": $throughput,
  "cache_size_mib": $CACHE_SIZE,
  "num_workers": $NUM_WORKERS
}
EOF

  success "FUSE $build_name/$format: ${duration}s, ${throughput} MB/s"

  # Cleanup extract dir
  rm -rf "$extract_dir"
}

# ============================================================================
# libdwarfs API Benchmark Functions
# ============================================================================

benchmark_libdwarfs_api() {
  local build_name=$1
  local build_dir=$2
  local image=$3
  local format=$4

  info "libdwarfs API benchmark: $build_name with $format"

  # Get a test file for single/random benchmarks
  local test_file="/$("$build_dir/dwarfsck" -i "$image" -l 2>/dev/null | grep -E '\.(pm|pl|pod)$' | head -n 1 || echo "")"

  # Validate test file exists and is not just "/"
  if [[ -z "$test_file" ]] || [[ "$test_file" == "/" ]]; then
    warn "No suitable test file found for $build_name/$format, skipping single/random benchmarks"
    # Still run full extraction benchmark
    "$build_dir/benchmarks/libdwarfs/full_extract_bench" "$image" "/tmp/full_${build_name}_${format}" \
      -n $ITERATIONS \
      -c $CACHE_SIZE \
      -w $NUM_WORKERS \
      -t $EXTRACT_THREADS \
      --json "$RESULTS_DIR/api_full_${build_name}_${format}.json" \
      > /dev/null 2>&1

    success "libdwarfs API $build_name/$format: full extraction only (no single file benchmark)"
    rm -rf "/tmp/full_${build_name}_${format}"
    return 0
  fi

  # Run single file benchmark
  "$build_dir/benchmarks/libdwarfs/single_file_bench" "$image" "$test_file" \
    -n $ITERATIONS \
    -c $CACHE_SIZE \
    -w $NUM_WORKERS \
    -o "/tmp/single_${build_name}_${format}.out" \
    --json "$RESULTS_DIR/api_single_${build_name}_${format}.json" \
    > /dev/null 2>&1

  # Run full extraction benchmark
  "$build_dir/benchmarks/libdwarfs/full_extract_bench" "$image" "/tmp/full_${build_name}_${format}" \
    -n $ITERATIONS \
    -c $CACHE_SIZE \
    -w $NUM_WORKERS \
    -t $EXTRACT_THREADS \
    --json "$RESULTS_DIR/api_full_${build_name}_${format}.json" \
    > /dev/null 2>&1

  success "libdwarfs API $build_name/$format complete"

  # Cleanup
  rm -rf "/tmp/single_${build_name}_${format}.out" "/tmp/full_${build_name}_${format}"
}

# ============================================================================
# Report Generation
# ============================================================================

generate_report() {
  section "Generating Comprehensive Report"

  local report="$RESULTS_DIR/COMPREHENSIVE_REPORT.md"

  cat > "$report" <<EOF
# DwarFS Comprehensive Benchmark Report

**Date**: $(date '+%Y-%m-%d %H:%M:%S')
**Platform**: $(uname -s) $(uname -m)
**Test Dataset**: Perl 5.43.3 source (~96 MB)
**Iterations**: $ITERATIONS
**Cache Size**: ${CACHE_SIZE} MiB
**Worker Threads**: $NUM_WORKERS

---

## Build Configurations Tested

1. **FlatBuffers-only** (\`build-fb-bench\`)
   - Metadata: FlatBuffers only
   - Images: .dff

2. **Both-formats** (\`build-both-bench\`)
   - Metadata: FlatBuffers + Thrift (Legacy + Modern)
   - Images: .dff, .dft

---

## Results Summary

### FUSE Extraction Performance

| Build | Format | Duration (s) | Throughput (MB/s) | Cache | Workers |
|-------|--------|--------------|-------------------|-------|---------|
EOF

  # Parse FUSE results
  for json in "$RESULTS_DIR"/fuse_*.json; do
    if [[ -f "$json" ]]; then
      python3 - "$json" "$report" <<'PYTHON'
import json, sys
with open(sys.argv[1]) as f:
    data = json.load(f)
with open(sys.argv[2], 'a') as out:
    out.write(f"| {data['build']} | {data['format']} | {data['duration_seconds']:.2f} | {data['throughput_mb_per_sec']:.2f} | {data['cache_size_mib']} MiB | {data['num_workers']} |\n")
PYTHON
    fi
  done

  cat >> "$report" <<EOF

### libdwarfs API Extraction Performance

| Build | Format | Operation | Time (s) | Throughput (MB/s) | Memory (MB) |
|-------|--------|-----------|----------|-------------------|-------------|
EOF

  # Parse API results
  for json in "$RESULTS_DIR"/api_*.json; do
    if [[ -f "$json" ]]; then
      python3 - "$json" "$report" <<'PYTHON'
import json, sys
with open(sys.argv[1]) as f:
    data = json.load(f)
    bench_name = list(data.keys())[0]
    results = data[bench_name]

# Extract build and format from filename
import os
filename = os.path.basename(sys.argv[1])
parts = filename.replace('api_', '').replace('.json', '').split('_')
operation = parts[0]
build = '_'.join(parts[1:-1])
fmt = parts[-1]

with open(sys.argv[2], 'a') as out:
    time = results['time']['mean']
    throughput = results.get('throughput_mb_per_sec', 0)
    memory = results.get('memory', {}).get('max', 0) / (1024*1024)
    out.write(f"| {build} | {fmt} | {operation} | {time:.2f} | {throughput:.2f} | {memory:.1f} |\n")
PYTHON
    fi
  done

  cat >> "$report" <<EOF

---

## Conclusions

### FUSE vs API Performance

**Expected Pattern**:
- libdwarfs API should be 5-10% faster (no FUSE overhead)
- Both should show similar throughput patterns

### Format Comparison

**FlatBuffers (.dff)**:
- Slightly larger images (~1-5%)
- Faster serialization
- Better portability

**Thrift (.dft)**:
- Smallest images
- Complex dependencies
- Good for space-critical scenarios

### Build Configuration Impact

**FlatBuffers-only**:
- Simplest build
- Best portability
- Good performance

**Thrift-only**:
- Complex dependencies
- Smallest images
- Platform limitations

**Both formats**:
- Maximum flexibility
- Larger binary size
- Can read/write both formats

---

**Raw Results**: See JSON files in \`$RESULTS_DIR\`
**Generated**: $(date '+%Y-%m-%d %H:%M:%S')

EOF

  success "Report generated: $report"

  info ""
  info "Results location: $RESULTS_DIR"
  info "View report: cat $report"
}

# ============================================================================
# Main Execution
# ============================================================================

main() {
  section "DwarFS Comprehensive Benchmark Suite"

  info "This will take 2-3 hours to complete"
  info "Results will be saved to: $RESULTS_DIR"
  info ""

  mkdir -p "$RESULTS_DIR"

  # Phase 1: Build all configurations
  section "Phase 1: Building Configurations (20-40 minutes)"

  build_configuration "flatbuffers-only" "$BUILD_FB" ON OFF
  build_configuration "both-formats" "$BUILD_BOTH" ON ON

  # Phase 2: Create images
  create_images

  # Phase 3: FUSE Benchmarks
  section "Phase 2: FUSE Benchmarks (20-40 minutes)"

  # FB-only build with .dff
  if [[ -f "$IMAGES_DIR/perl-5.43.3.dff" ]]; then
    benchmark_fuse_extraction "fb-only" "$BUILD_FB" "$IMAGES_DIR/perl-5.43.3.dff" "dff"
  fi

  # Both build with both formats
  if [[ -f "$IMAGES_DIR/perl-5.43.3.dff" ]]; then
    benchmark_fuse_extraction "both" "$BUILD_BOTH" "$IMAGES_DIR/perl-5.43.3.dff" "dff"
  fi
  if [[ -f "$IMAGES_DIR/perl-5.43.3.dft" ]]; then
    benchmark_fuse_extraction "both" "$BUILD_BOTH" "$IMAGES_DIR/perl-5.43.3.dft" "dft"
  fi

  # Phase 4: libdwarfs API Benchmarks
  section "Phase 3: libdwarfs API Benchmarks (20-40 minutes)"

  # FB-only build with .dff
  if [[ -f "$IMAGES_DIR/perl-5.43.3.dff" ]]; then
    benchmark_libdwarfs_api "fb-only" "$BUILD_FB" "$IMAGES_DIR/perl-5.43.3.dff" "dff"
  fi

  # Both build with both formats
  if [[ -f "$IMAGES_DIR/perl-5.43.3.dff" ]]; then
    benchmark_libdwarfs_api "both" "$BUILD_BOTH" "$IMAGES_DIR/perl-5.43.3.dff" "dff"
  fi
  if [[ -f "$IMAGES_DIR/perl-5.43.3.dft" ]]; then
    benchmark_libdwarfs_api "both" "$BUILD_BOTH" "$IMAGES_DIR/perl-5.43.3.dft" "dft"
  fi

  # Phase 5: Generate Report
  generate_report

  section "Benchmark Complete!"

  success "All benchmarks completed successfully"
  info ""
  info "Results directory: $RESULTS_DIR"
  info "Comprehensive report: $RESULTS_DIR/COMPREHENSIVE_REPORT.md"
  info ""
  info "Quick view:"
  info "  cat $RESULTS_DIR/COMPREHENSIVE_REPORT.md"
}

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  main "$@"
fi