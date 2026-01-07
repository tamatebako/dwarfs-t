#!/usr/bin/env bash
# Master script: Clean, Build, Test, and Benchmark all configurations
# Usage: ./scripts/run-all.sh [dataset_path]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "════════════════════════════════════════"
echo "DwarFS: Clean → Build → Test → Benchmark"
echo "════════════════════════════════════════"
echo

# Step 1: Clean
echo "🧹 Cleaning build artifacts..."
cd "$PROJECT_ROOT"
rm -rf build-* /tmp/test-*.dff /tmp/test-*.dft
echo "✓ Clean complete"
echo

# Step 2: Build and Test
echo "🔨 Building all configurations..."
"$SCRIPT_DIR/build-all-and-test.sh"
echo

# Step 3: Benchmark
echo "📊 Running benchmarks..."
"$SCRIPT_DIR/benchmark-all.sh" "$@"
echo

echo "════════════════════════════════════════"
echo "✨ All done!"
echo "════════════════════════════════════════"