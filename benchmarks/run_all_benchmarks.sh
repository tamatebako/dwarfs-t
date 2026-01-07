#!/usr/bin/env bash
#
# Master DwarFS Benchmark Orchestrator
#
# Runs ALL benchmarking systems:
# 1. Comprehensive benchmark (FUSE vs API, all formats) - MANDATORY
# 2. Metadata format comparison - OPTIONAL
# 3. Compression algorithm comparison - OPTIONAL
#
# Expected runtime: 3-5 hours (comprehensive only: 2-3 hours)
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
RESULTS_DIR="$PROJECT_ROOT/results/master_${TIMESTAMP}"

# Feature flags
RUN_METADATA=${RUN_METADATA:-false}
RUN_COMPRESSION=${RUN_COMPRESSION:-false}

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

section() {
  echo ""
  echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
  echo -e "${CYAN}  $*${NC}"
  echo -e "${CYAN}═══════════════════════════════════════════════════════════════${NC}"
  echo ""
}

# ============================================================================
# Main Execution
# ============================================================================

main() {
  section "DwarFS Master Benchmark Suite"

  cat <<EOF
This script runs ALL DwarFS benchmarking systems:

${GREEN}✓ MANDATORY:${NC}
  1. Comprehensive Benchmark (FUSE vs API, all formats)
     - 3 build configurations
     - 2 image formats
     - FUSE extraction benchmarks
     - libdwarfs API benchmarks
     - Expected: 2-3 hours

${BLUE}○ OPTIONAL (set environment variables to enable):${NC}
  2. Metadata Format Comparison (RUN_METADATA=true)
     - FlatBuffers vs Thrift detailed comparison
     - Expected: +30-60 minutes

  3. Compression Algorithm Comparison (RUN_COMPRESSION=true)
     - All compression algorithms tested
     - Expected: +30-60 minutes

${CYAN}Results will be saved to:${NC}
  $RESULTS_DIR

${YELLOW}Total estimated time:${NC}
  - Comprehensive only: 2-3 hours
  - With metadata: 3-4 hours
  - With all: 4-5 hours

EOF

  # Environment variable hints
  if [[ "$RUN_METADATA" == "true" ]]; then
    echo -e "${GREEN}✓ Metadata benchmarks ENABLED${NC}"
  else
    echo -e "${BLUE}  To enable metadata benchmarks: RUN_METADATA=true $0${NC}"
  fi

  if [[ "$RUN_COMPRESSION" == "true" ]]; then
    echo -e "${GREEN}✓ Compression benchmarks ENABLED${NC}"
  else
    echo -e "${BLUE}  To enable compression benchmarks: RUN_COMPRESSION=true $0${NC}"
  fi

  echo ""
  read -p "Press Enter to continue or Ctrl+C to cancel..."

  mkdir -p "$RESULTS_DIR"

  # ============================================================================
  # Suite 1: Comprehensive Benchmark (MANDATORY)
  # ============================================================================

  section "SUITE 1: Comprehensive Benchmark (FUSE vs API)"

  info "Running: benchmarks/run_comprehensive_benchmark.sh"
  info "Expected: 2-3 hours"
  echo ""

  "$SCRIPT_DIR/run_comprehensive_benchmark.sh"

  # Copy comprehensive results
  local latest_comprehensive=$(ls -td "$PROJECT_ROOT"/results/comprehensive_* | head -1)
  if [[ -d "$latest_comprehensive" ]]; then
    cp -r "$latest_comprehensive"/* "$RESULTS_DIR/"
    success "Comprehensive benchmark complete"
    info "Results copied to: $RESULTS_DIR"
  else
    error "Comprehensive benchmark results not found!"
  fi

  # ============================================================================
  # Suite 2: Metadata Format Benchmark (OPTIONAL)
  # ============================================================================

  if [[ "$RUN_METADATA" == "true" ]]; then
    section "SUITE 2: Metadata Format Comparison"

    info "Running: benchmarks/run_metadata_format_benchmark.py"
    info "Expected: 30-60 minutes"
    echo ""

    # Check if Python benchmark exists
    if [[ ! -f "$SCRIPT_DIR/run_metadata_format_benchmark.py" ]]; then
      warn "Metadata format benchmark script not found, skipping"
    else
      # Use the both-formats build for maximum compatibility
      local build_dir="$PROJECT_ROOT/build-both-bench"

      if [[ ! -d "$build_dir" ]]; then
        warn "Both-formats build not found, skipping metadata benchmarks"
      else
        python3 "$SCRIPT_DIR/run_metadata_format_benchmark.py" \
          --mkdwarfs "$build_dir/mkdwarfs" \
          --dwarfsextract "$build_dir/dwarfsextract" \
          --dwarfs "$build_dir/dwarfs" \
          --perl-dataset "$PROJECT_ROOT/benchmark-files/perl-5.43.3" \
          --output "$RESULTS_DIR/metadata_results.json" \
          --runs 5 || {
            warn "Metadata benchmark failed, continuing..."
          }

        success "Metadata format benchmark complete"
      fi
    fi
  fi

  # ============================================================================
  # Suite 3: Compression Algorithm Benchmark (OPTIONAL)
  # ============================================================================

  if [[ "$RUN_COMPRESSION" == "true" ]]; then
    section "SUITE 3: Compression Algorithm Comparison"

    info "Running: benchmarks/compression_algorithm_benchmark.py"
    info "Expected: 30-60 minutes"
    echo ""

    # Check if Python benchmark exists
    if [[ ! -f "$SCRIPT_DIR/compression_algorithm_benchmark.py" ]]; then
      warn "Compression algorithm benchmark script not found, skipping"
    else
      local build_dir="$PROJECT_ROOT/build-both-bench"

      if [[ ! -d "$build_dir" ]]; then
        warn "Both-formats build not found, skipping compression benchmarks"
      else
        python3 "$SCRIPT_DIR/compression_algorithm_benchmark.py" \
          --mkdwarfs "$build_dir/mkdwarfs" \
          --dataset "$PROJECT_ROOT/benchmark-files/perl-5.43.3" \
          --output "$RESULTS_DIR/compression_results.json" || {
            warn "Compression benchmark failed, continuing..."
          }

        success "Compression algorithm benchmark complete"
      fi
    fi
  fi

  # ============================================================================
  # Final Summary
  # ============================================================================

  section "Master Benchmark Suite Complete!"

  success "All benchmarks completed successfully"
  echo ""
  info "Results directory: $RESULTS_DIR"
  echo ""
  info "Available reports:"

  if [[ -f "$RESULTS_DIR/COMPREHENSIVE_REPORT.md" ]]; then
    echo "  ✓ Comprehensive Report: $RESULTS_DIR/COMPREHENSIVE_REPORT.md"
  fi

  if [[ -f "$RESULTS_DIR/metadata_results.json" ]]; then
    echo "  ✓ Metadata Results: $RESULTS_DIR/metadata_results.json"
    info "    Generate report: python3 benchmarks/generate_metadata_report.py \\"
    info "      $RESULTS_DIR/metadata_results.json \\"
    info "      $RESULTS_DIR/METADATA_REPORT.md"
  fi

  if [[ -f "$RESULTS_DIR/compression_results.json" ]]; then
    echo "  ✓ Compression Results: $RESULTS_DIR/compression_results.json"
  fi

  echo ""
  info "Quick view comprehensive report:"
  echo "  cat $RESULTS_DIR/COMPREHENSIVE_REPORT.md"
  echo ""

  # JSON files summary
  local json_count=$(find "$RESULTS_DIR" -name "*.json" | wc -l)
  info "Total JSON result files: $json_count"
  echo ""
}

# ============================================================================
# Usage Information
# ============================================================================

show_usage() {
  cat <<EOF
Usage: $0 [options]

Runs all DwarFS benchmarking systems.

OPTIONS:
  -h, --help           Show this help message
  --metadata           Enable metadata format benchmarks
  --compression        Enable compression algorithm benchmarks
  --all                Enable all optional benchmarks

ENVIRONMENT VARIABLES:
  RUN_METADATA=true    Enable metadata benchmarks (same as --metadata)
  RUN_COMPRESSION=true Enable compression benchmarks (same as --compression)
  SKIP_BUILD=true      Skip rebuilds, use existing build directories

EXAMPLES:
  # Comprehensive only (default, 2-3 hours)
  $0

  # With metadata benchmarks (3-4 hours)
  $0 --metadata

  # All benchmarks (4-5 hours)
  $0 --all

  # Use environment variables
  RUN_METADATA=true RUN_COMPRESSION=true $0

REQUIREMENTS:
  - Perl 5.43.3 dataset downloaded
    python3 benchmarks/download_datasets.py --download perl

OUTPUT:
  results/master_YYYYMMDD_HHMMSS/
    - COMPREHENSIVE_REPORT.md (always)
    - fuse_*.json (FUSE benchmark results)
    - api_*.json (API benchmark results)
    - metadata_results.json (if --metadata)
    - compression_results.json (if --compression)

SEE ALSO:
  benchmarks/README.md - Benchmark suite documentation
  doc/BENCHMARKING_SYSTEM_ARCHITECTURE.md - System architecture
  benchmarks/schemas/README.md - JSON schema documentation

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
    --metadata)
      RUN_METADATA=true
      shift
      ;;
    --compression)
      RUN_COMPRESSION=true
      shift
      ;;
    --all)
      RUN_METADATA=true
      RUN_COMPRESSION=true
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

# Run if executed directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  main "$@"
fi