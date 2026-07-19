#!/bin/bash
# One-Step Build Script for DwarFS
#
# This script builds all configurations of DwarFS using the unified build system.
#
# Usage:
#   ./scripts/one-step/build-all.sh [--vcpkg] [--system]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

# Source libraries and orchestrator
source scripts/lib/build_env.sh
source scripts/lib/vcpkg_helper.sh
source scripts/orchestrator/build.sh

# Parse arguments
USE_VCPKG=auto
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
TRIPLET=""

for arg in "$@"; do
  case $arg in
    --vcpkg)
      USE_VCPKG=vcpkg
      shift
      ;;
    --system)
      USE_VCPKG=system
      shift
      ;;
    --triplet=*)
      TRIPLET="${arg#*=}"
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [options]"
      echo ""
      echo "Build all DwarFS configurations"
      echo ""
      echo "Options:"
      echo "  --vcpkg           Force vcpkg mode"
      echo "  --system          Force system package mode"
      echo "  --triplet=TRIPLET Use specific vcpkg triplet"
      echo "  -h                Show this help"
      exit 0
      ;;
  esac
done

echo ""
echo -e "${COLOR_CYAN}╔═══════════════════════════════════════════════════════════════╗${COLOR_NC}"
echo -e "${COLOR_CYAN}║           DwarFS Build All Configurations                   ║${COLOR_NC}"
echo -e "${COLOR_CYAN}╚═══════════════════════════════════════════════════════════════╝${COLOR_NC}"
echo ""

# Detect environment
section "Environment Detection"

if [[ "$USE_VCPKG" == "auto" ]]; then
  USE_VCPKG=$(dwarfs_detect_build_mode false false "$VCPKG_ROOT")
fi

info "Build mode: $USE_VCPKG"

if [[ "$USE_VCPKG" == "vcpkg" ]]; then
  if [[ -z "$TRIPLET" ]]; then
    TRIPLET=$(dwarfs_auto_triplet)
  fi
  info "Triplet: $TRIPLET"

  # Verify Tebako jemalloc
  if ! dwarfs_verify_tebako_jemalloc "$PROJECT_ROOT"; then
    fatal "Tebako jemalloc verification failed"
  fi
fi

echo ""

# Build all configurations
if dwarfs_build_all "$USE_VCPKG" "$VCPKG_ROOT" "$TRIPLET"; then
  echo ""
  success "All configurations built successfully!"
  echo ""
  info "Build artifacts:"
  for config in $(dwarfs_list_configs); do
    if [[ -d "build-$config" ]]; then
      echo "  - build-$config/"
    fi
  done
  echo ""
else
  echo ""
  error "Some configurations failed to build"
  echo ""
  exit 1
fi
