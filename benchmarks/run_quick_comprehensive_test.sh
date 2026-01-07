#!/usr/bin/env bash
#
# Quick Comprehensive Benchmark Validation
# Uses existing build/ directory (both-formats build)
# Tests path fix works end-to-end
#
# Runtime: ~5-10 min (vs 2-3 hours for full rebuild)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

BUILD_DIR="$PROJECT_ROOT/build"
IMAGES_DIR="$PROJECT_ROOT/test-images"
RESULTS_DIR="$PROJECT_ROOT/results/quick_test_${TIMESTAMP}"
PERL_SOURCE="$PROJECT_ROOT/benchmark-files/perl-5.43.3/perl-5.43.3"

ITERATIONS=2
CACHE_SIZE=512
NUM_WORKERS=4

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

info() { echo -e "${BLUE}[INFO]${NC} $*"; }
success() { echo -e "${GREEN}[✓]${NC} $*"; }
section() { echo -e "\n${CYAN}════ $* ════${NC}\n"; }

mkdir -p "$RESULTS_DIR" "$IMAGES_DIR"

section "Quick Comprehensive Benchmark Test"

# Check build exists
if [[ ! -f "$BUILD_DIR/mkdwarfs" ]]; then
  echo "ERROR: build/ directory not found or incomplete"
  echo "Run: cmake -B build -GNinja && ninja -C build"
  exit 1
fi

info "Using build: $BUILD_DIR"
info "Results: $RESULTS_DIR"
echo ""

# Create images if needed
section "Using Test Images"

# Check for existing images
if [[ -f "$IMAGES_DIR/perl-5.43.3.dff" ]]; then
  info "Using existing perl-5.43.3.dff"
  IMAGE_DFF="$IMAGES_DIR/perl-5.43.3.dff"
elif [[ -f "$IMAGES_DIR/perl-test.dff" ]]; then
  info "Using existing perl-test.dff"
  IMAGE_DFF="$IMAGES_DIR/perl-test.dff"
else
  echo "ERROR: No .dff test image found"
  echo "Create one with: build/mkdwarfs -i <source> -o test-images/perl-test.dff"
  exit 1
fi

if [[ -f "$IMAGES_DIR/perl-5.43.3.dft" ]]; then
  info "Using existing perl-5.43.3.dft"
  IMAGE_DFT="$IMAGES_DIR/perl-5.43.3.dft"
elif [[ -f "$IMAGES_DIR/perl-test.dft" ]]; then
  info "Using existing perl-test.dft"
  IMAGE_DFT="$IMAGES_DIR/perl-test.dft"
else
  info "No .dft image found, will test .dff only"
  IMAGE_DFT=""
fi

# Test dff format
section "Testing .dff Format"

image="$IMAGE_DFF"

# Get test file WITH "/" PREFIX (the fix!)
info "Getting test file path..."
test_file="/$("$BUILD_DIR/dwarfsck" "$image" -l 2>/dev/null | grep -E '\.(pm|pl|pod)$' | head -n 1)"

if [[ -z "$test_file" ]] || [[ "$test_file" == "/" ]]; then
  echo "ERROR: No test file found"
  exit 1
fi

info "Test file: $test_file"

# Run single file benchmark
info "Running single_file_bench..."
"$BUILD_DIR/benchmarks/libdwarfs/single_file_bench" "$image" "$test_file" \
  -n $ITERATIONS -c $CACHE_SIZE -w $NUM_WORKERS \
  --json "$RESULTS_DIR/api_single_dff.json" \
  > /dev/null 2>&1
success "Single file benchmark complete"

# Run full extraction benchmark
info "Running full_extract_bench..."
"$BUILD_DIR/benchmarks/libdwarfs/full_extract_bench" "$image" "/tmp/quick_extract_dff" \
  -n $ITERATIONS -c $CACHE_SIZE -w $NUM_WORKERS -t 1 \
  --json "$RESULTS_DIR/api_full_dff.json" \
  > /dev/null 2>&1
rm -rf "/tmp/quick_extract_dff"
success "Full extraction benchmark complete"

# Test dft format if available
if [[ -n "$IMAGE_DFT" ]]; then
  section "Testing .dft Format"

  image="$IMAGE_DFT"

  # Run single file benchmark
  info "Running single_file_bench..."
  "$BUILD_DIR/benchmarks/libdwarfs/single_file_bench" "$image" "$test_file" \
    -n $ITERATIONS -c $CACHE_SIZE -w $NUM_WORKERS \
    --json "$RESULTS_DIR/api_single_dft.json" \
    > /dev/null 2>&1
  success "Single file benchmark complete"

  # Run full extraction benchmark
  info "Running full_extract_bench..."
  "$BUILD_DIR/benchmarks/libdwarfs/full_extract_bench" "$image" "/tmp/quick_extract_dft" \
    -n $ITERATIONS -c $CACHE_SIZE -w $NUM_WORKERS -t 1 \
    --json "$RESULTS_DIR/api_full_dft.json" \
    > /dev/null 2>&1
  rm -rf "/tmp/quick_extract_dft"
  success "Full extraction benchmark complete"
fi

section "Results Summary"

info "Generated files:"
ls -lh "$RESULTS_DIR"/*.json | awk '{print "  " $9 ": " $5}'

echo ""
success "Quick validation complete!"
echo ""
echo "Results: $RESULTS_DIR"
echo ""
echo "View results:"
for json in "$RESULTS_DIR"/*.json; do
  echo "  cat $json | python3 -m json.tool"
done

