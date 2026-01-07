#!/bin/bash
# Quick Benchmark Validation Script
#
# This is a lightweight benchmark script for quick validation.
# For comprehensive benchmarks, use benchmarks/run_comprehensive_benchmark.sh
#
# Usage: ./scripts/one-step/benchmark-quick.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

# Source libraries
source scripts/lib/build_env.sh

section "Quick Benchmark Validation"

# Check if builds exist
BUILD_EXISTS=false
for dir in build-*; do
  if [[ -d "$dir" && -x "$dir/mkdwarfs" ]]; then
    BUILD_EXISTS=true
    info "Found build: $dir"
  fi
done

if [[ "$BUILD_EXISTS" == "false" ]]; then
  warn "No valid builds found. Run tests first:"
  echo "  ./scripts/one-step/test-everything.sh --quick"
  exit 1
fi

# Find a working build
BUILD_DIR=""
for dir in build-flatbuffers-only build-both-formats build-fb-only; do
  if [[ -d "$dir" && -x "$dir/mkdwarfs" ]]; then
    BUILD_DIR="$dir"
    break
  fi
done

if [[ -z "$BUILD_DIR" ]]; then
  error "No working build directory found"
  exit 1
fi

info "Using build: $BUILD_DIR"

# Create test data
TEST_DATA="/tmp/dwarfs-quick-benchmark-$$"
mkdir -p "$TEST_DATA/input"

info "Creating test data..."
# Create some test files
echo "Test file 1" > "$TEST_DATA/input/file1.txt"
echo "Test file 2" > "$TEST_DATA/input/file2.txt"
mkdir -p "$TEST_DATA/input/subdir"
echo "Test file 3" > "$TEST_DATA/input/subdir/file3.txt"

# Create test image
TEST_IMAGE="/tmp/dwarfs-quick-test-$$.dff"

info "Creating DwarFS image..."
if ! "$BUILD_DIR/mkdwarfs" -i "$TEST_DATA/input" -o "$TEST_IMAGE" 2>&1; then
  error "Failed to create test image"
  rm -rf "$TEST_DATA" "$TEST_IMAGE"
  exit 1
fi

success "Test image created: $TEST_IMAGE"

# Get image size
IMAGE_SIZE=$(stat -f%z "$TEST_IMAGE" 2>/dev/null || stat -c%s "$TEST_IMAGE" 2>/dev/null || echo "?")
info "Image size: $IMAGE_SIZE bytes"

# Clean up
rm -rf "$TEST_DATA" "$TEST_IMAGE"

success "Quick benchmark validation passed!"
echo ""
info "For comprehensive benchmarks:"
echo "  ./scripts/benchmark-all.sh"
echo "  ./benchmarks/run_comprehensive_benchmark.sh"
