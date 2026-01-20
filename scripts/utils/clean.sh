#!/bin/bash
# DwarFS Clean Utility
#
# This script cleans build artifacts and temporary files.
# Part of the unified build system - uses build_env.sh library.
#
# Usage:
#   ./scripts/utils/clean.sh           # Interactive clean
#   ./scripts/utils/clean.sh --all     # Clean everything including benchmarks
#   ./scripts/utils/clean.sh --yes     # Non-interactive clean

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

# Source libraries
source scripts/lib/build_env.sh

# Parse arguments
CLEAN_ALL=false
AUTO_CONFIRM=false
for arg in "$@"; do
  case $arg in
    --all)
      CLEAN_ALL=true
      shift
      ;;
    --yes|-y)
      AUTO_CONFIRM=true
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [--all] [--yes]"
      echo ""
      echo "Clean build artifacts and temporary files"
      echo ""
      echo "Options:"
      echo "  --all    Also remove benchmark results and datasets"
      echo "  --yes    Non-interactive mode (skip confirmation)"
      echo "  -h       Show this help"
      exit 0
      ;;
  esac
done

section "DwarFS Cleanup"

# Count items to clean
count=0

# Build directories
info "Build directories:"
for dir in build build-* .build*; do
  if [[ -d "$dir" ]]; then
    echo "  - $dir"
    ((count++))
  fi
done

# Temporary test files
info "Temporary files:"
for pattern in "/tmp/test-*.dff" "/tmp/test-*.dft" "/tmp/dwarfs-*"; do
  if ls $pattern 2>/dev/null | head -5; then
    ((count++))
  fi
done

# CMake cache files
if [[ -f "CMakeCache.txt" ]]; then
  echo "  - CMakeCache.txt"
  ((count++))
fi

if [[ -d "CMakeFiles" ]]; then
  echo "  - CMakeFiles/"
  ((count++))
fi

# Generated files
if [[ -d "include/dwarfs/gen-cpp2" ]]; then
  echo "  - include/dwarfs/gen-cpp2/ (Thrift)"
  ((count++))
fi

if [[ -d "include/dwarfs/gen-flatbuffers" ]]; then
  echo "  - include/dwarfs/gen-flatbuffers/"
  ((count++))
fi

if [[ "$CLEAN_ALL" == "true" ]]; then
  info "Benchmark results (--all):"
  if [[ -d "results" ]]; then
    echo "  - results/"
    ((count++))
  fi
  if [[ -d "benchmark-results" ]]; then
    echo "  - benchmark-results/"
    ((count++))
  fi
  if [[ -d "vcpkg_installed" ]]; then
    echo "  - vcpkg_installed/"
    ((count++))
  fi
fi

echo ""

if [[ $count -eq 0 ]]; then
  success "Nothing to clean!"
  exit 0
fi

# Confirm
if [[ "$AUTO_CONFIRM" == "false" ]]; then
  read -p "Remove $count items? (y/N) " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    info "Cancelled."
    exit 0
  fi
fi

# Clean build directories
info "Cleaning build directories..."
rm -rf build build-* .build*

# Clean temporary files
info "Cleaning temporary files..."
rm -f /tmp/test-*.dff /tmp/test-*.dft 2>/dev/null || true
rm -rf /tmp/dwarfs-* 2>/dev/null || true

# Clean CMake cache
info "Cleaning CMake cache..."
rm -f CMakeCache.txt
rm -rf CMakeFiles

# Clean generated files
info "Cleaning generated files..."
rm -rf include/dwarfs/gen-cpp2
rm -rf include/dwarfs/gen-flatbuffers

if [[ "$CLEAN_ALL" == "true" ]]; then
  info "Cleaning benchmark results..."
  rm -rf results
  # Keep benchmark-results/images but remove other files
  if [[ -d "benchmark-results" ]]; then
    find benchmark-results -type f ! -path "*/images/*" -delete 2>/dev/null || true
  fi

  info "Cleaning vcpkg installed..."
  rm -rf vcpkg_installed
fi

echo ""
success "Cleanup complete!"
echo ""
info "To rebuild:"
echo "  ./scripts/one-step/test-everything.sh"
echo "  ./scripts/one-step/build-all-and-test.sh --vcpkg"
