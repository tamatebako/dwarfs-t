#!/bin/bash
# Clean Rebuild and Comprehensive Benchmark Script
# Deletes all benchmark builds, rebuilds from scratch, and runs benchmarks

set -e  # Exit on error

WORKSPACE="/Users/mulgogi/src/external/dwarfs"
cd "$WORKSPACE"

echo "========================================================================"
echo "CLEAN REBUILD AND COMPREHENSIVE BENCHMARK"
echo "========================================================================"
echo ""

# Step 1: Clean all benchmark builds
echo "Step 1: Cleaning benchmark builds..."
echo "------------------------------------------------------------------------"
rm -rf build-fb-bench build-thrift-bench build-both-bench
echo "✓ Removed build-fb-bench"
echo "✓ Removed build-thrift-bench"
echo "✓ Removed build-both-bench"
echo ""

# Step 2: Clean benchmark data
echo "Step 2: Cleaning benchmark data..."
echo "------------------------------------------------------------------------"
rm -rf benchmark-data/images/*
echo "✓ Removed all test images"
echo ""

# Step 3: Rebuild all configurations
echo "Step 3: Rebuilding all configurations..."
echo "------------------------------------------------------------------------"
python3 benchmarks/lib/build_manager.py --workspace "$WORKSPACE" --build-all
echo ""

# Step 4: Run comprehensive benchmarks
echo "Step 4: Running comprehensive benchmarks..."
echo "------------------------------------------------------------------------"
python3 benchmarks/comprehensive_benchmark.py \
  --builds fb-only,both \
  --datasets dwarfs-source \
  --operations create,extract_full \
  --formats flatbuffers,thrift \
  --runs 3 \
  --output-dir benchmark-results/comprehensive/$(date +%Y%m%d-%H%M%S)

echo ""
echo "========================================================================"
echo "BENCHMARK COMPLETE"
echo "========================================================================"