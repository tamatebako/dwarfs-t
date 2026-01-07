#!/usr/bin/env bash
# Wrapper script for comprehensive benchmarking
# Delegates to benchmarks/run_comprehensive_benchmark.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "========================================"
echo "DwarFS Comprehensive Benchmark Wrapper"
echo "========================================"
echo ""
echo "This script delegates to: benchmarks/run_comprehensive_benchmark.sh"
echo ""

# Check if benchmark infrastructure exists
if [[ ! -f "$PROJECT_ROOT/benchmarks/run_comprehensive_benchmark.sh" ]]; then
  echo "ERROR: Benchmark infrastructure not found!"
  echo "Expected: benchmarks/run_comprehensive_benchmark.sh"
  exit 1
fi

# Execute the comprehensive benchmark
exec "$PROJECT_ROOT/benchmarks/run_comprehensive_benchmark.sh" "$@"