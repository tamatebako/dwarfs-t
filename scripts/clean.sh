#!/usr/bin/env bash
# Clean all build artifacts and temporary files
# Usage: ./scripts/clean.sh [--all]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Parse arguments
CLEAN_ALL=false
for arg in "$@"; do
  case $arg in
    --all)
      CLEAN_ALL=true
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [--all]"
      echo ""
      echo "Clean build artifacts and temporary files"
      echo ""
      echo "Options:"
      echo "  --all    Also remove benchmark results and datasets"
      echo "  -h       Show this help"
      exit 0
      ;;
  esac
done

echo "========================================"
echo "DwarFS Cleanup Script"
echo "========================================"
echo ""

# Count items to clean
count=0

# Build directories
echo -e "${BLUE}Build directories:${NC}"
for dir in build build-* .build*; do
  if [[ -d "$dir" ]]; then
    echo "  - $dir"
    ((count++))
  fi
done

# Temporary test files
echo -e "${BLUE}Temporary files:${NC}"
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

if [[ "$CLEAN_ALL" == "true" ]]; then
  echo -e "${BLUE}Benchmark results (--all):${NC}"
  if [[ -d "results" ]]; then
    echo "  - results/"
    ((count++))
  fi
  if [[ -d "benchmark-results" ]]; then
    echo "  - benchmark-results/"
    ((count++))
  fi
fi

echo ""

if [[ $count -eq 0 ]]; then
  echo -e "${GREEN}Nothing to clean!${NC}"
  exit 0
fi

# Confirm
read -p "Remove $count items? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
  echo "Cancelled."
  exit 0
fi

# Clean build directories
echo -e "${YELLOW}Cleaning build directories...${NC}"
rm -rf build build-* .build*

# Clean temporary files
echo -e "${YELLOW}Cleaning temporary files...${NC}"
rm -f /tmp/test-*.dff /tmp/test-*.dft 2>/dev/null || true
rm -rf /tmp/dwarfs-* 2>/dev/null || true

# Clean CMake cache
echo -e "${YELLOW}Cleaning CMake cache...${NC}"
rm -f CMakeCache.txt
rm -rf CMakeFiles

if [[ "$CLEAN_ALL" == "true" ]]; then
  echo -e "${YELLOW}Cleaning benchmark results...${NC}"
  rm -rf results
  # Keep benchmark-results/images but remove other files
  if [[ -d "benchmark-results" ]]; then
    find benchmark-results -type f ! -path "*/images/*" -delete 2>/dev/null || true
  fi
fi

echo ""
echo -e "${GREEN}✓ Cleanup complete!${NC}"
echo ""
echo "To rebuild:"
echo "  ./scripts/build-all-and-test.sh"