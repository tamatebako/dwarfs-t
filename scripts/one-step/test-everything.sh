#!/bin/bash
# One-Step Test Script for DwarFS
#
# This script runs all tests in the correct order:
# 1. Detects environment (vcpkg vs system packages)
# 2. Builds all configurations
# 3. Runs all tests
# 4. Optionally runs benchmarks
#
# Usage:
#   ./scripts/test-everything.sh              # Auto-detect mode
#   ./scripts/test-everything.sh --vcpkg      # Force vcpkg mode
#   ./scripts/test-everything.sh --system     # Force system mode
#   ./scripts/test-everything.sh --quick      # Quick test (no benchmarks)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

# ============================================================================
# Source Libraries
# ============================================================================

source scripts/lib/build_env.sh
source scripts/lib/vcpkg_helper.sh
source scripts/orchestrator/build.sh

# ============================================================================
# Configuration
# ============================================================================

MODE="${1:-}"
RUN_BENCHMARKS=true
QUICK_TEST=false

if [[ "$MODE" == "--quick" ]]; then
  QUICK_TEST=true
  RUN_BENCHMARKS=false
  MODE=""
fi

if [[ "$MODE" == "--vcpkg" ]]; then
  FORCE_VCPKG=true
elif [[ "$MODE" == "--system" ]]; then
  FORCE_SYSTEM=true
else
  FORCE_VCPKG=false
  FORCE_SYSTEM=false
fi

# ============================================================================
# Test Functions
# ============================================================================

test_configuration() {
  local name=$1
  local build_dir="build-$name"

  info "Testing: $name"

  if [[ ! -d "$build_dir" ]]; then
    error "Build directory not found: $build_dir"
    return 1
  fi

  # Run tests using ctest
  if ! ctest --test-dir "$build_dir" --output-on-failure -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4) \
    > "$build_dir-test.log" 2>&1; then
    error "Tests failed for $name"
    cat "$build_dir-test.log" | tail -100
    return 1
  fi

  # Count tests
  local test_count=$(grep -c "RUN " "$build_dir-test.log" || echo "?")
  success "Tests passed: $name ($test_count tests)"
  return 0
}

# ============================================================================
# Main Execution
# ============================================================================

main() {
  echo ""
  echo -e "${COLOR_CYAN}╔═══════════════════════════════════════════════════════════════╗${COLOR_NC}"
  echo -e "${COLOR_CYAN}║           DwarFS One-Step Test Suite                         ║${COLOR_NC}"
  echo -e "${COLOR_CYAN}╚═══════════════════════════════════════════════════════════════╝${COLOR_NC}"
  echo ""

  # Detect environment
  section "Detecting Build Environment"

  VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
  HAS_VCPKG=false

  # Check for vcpkg
  if dwarfs_has_vcpkg "$VCPKG_ROOT"; then
    HAS_VCPKG=true
    info "Found vcpkg at: $VCPKG_ROOT"
  else
    warn "vcpkg not found at: $VCPKG_ROOT"
  fi

  # Determine build mode
  USE_VCPKG=false
  if [[ "$FORCE_VCPKG" == "true" ]]; then
    if [[ "$HAS_VCPKG" == "false" ]]; then
      fatal "Forced vcpkg mode but vcpkg not found at: $VCPKG_ROOT"
    fi
    USE_VCPKG=true
    info "Forced vcpkg mode"
  elif [[ "$FORCE_SYSTEM" == "true" ]]; then
    USE_VCPKG=false
    info "Forced system package mode"
  elif [[ "$HAS_VCPKG" == "true" ]]; then
    USE_VCPKG=true
    info "Auto-detected vcpkg mode (recommended)"
  else
    # Check if system packages are suitable
    if dwarfs_has_jemalloc 5.5.0; then
      USE_VCPKG=false
      info "Using system package mode (jemalloc 5.5.0+ found)"
    else
      fatal "No suitable build environment found. Please install vcpkg or suitable system packages (jemalloc 5.5.0+)."
    fi
  fi

  # Detect triplet if using vcpkg
  TRIPLET=""
  if [[ "$USE_VCPKG" == "true" ]]; then
    TRIPLET=$(dwarfs_auto_triplet)
    info "Auto-detected triplet: $TRIPLET"

    # Verify Tebako jemalloc
    if ! dwarfs_verify_tebako_jemalloc "$PROJECT_ROOT"; then
      fatal "Tebako jemalloc verification failed"
    fi
  fi

  # Print summary
  dwarfs_print_env_summary "$([ "$USE_VCPKG" == "true" ] && echo "vcpkg" || echo "system")" \
    "$([ "$USE_VCPKG" == "true" ] && echo "$VCPKG_ROOT" || echo "")" \
    "$TRIPLET"

  # Track results
  PASSED=()
  FAILED=()

  # Build and test each configuration
  for config in $(dwarfs_list_configs); do
    section "Testing Configuration: $config"

    local description=$(dwarfs_config_description "$config")
    info "$description"

    # Build
    if dwarfs_build_configuration "$config" "build-$config" "RelWithDebInfo" \
        "$([ "$USE_VCPKG" == "true" ] && echo "vcpkg" || echo "system")" \
        "$VCPKG_ROOT" "$TRIPLET"; then
      # Build succeeded, run tests
      if test_configuration "$config"; then
        PASSED+=("$config")
      else
        FAILED+=("$config (tests)")
      fi
    else
      FAILED+=("$config (build)")
    fi
  done

  # ============================================================================
  # Summary
  # ============================================================================

  section "Test Summary"

  if [[ ${#PASSED[@]} -gt 0 ]]; then
    echo "Passed configurations:"
    for config in "${PASSED[@]}"; do
      local description=$(dwarfs_config_description "$config")
      echo -e "  ${COLOR_GREEN}✓${COLOR_NC} $description"
    done
    echo ""
  fi

  if [[ ${#FAILED[@]} -gt 0 ]]; then
    echo "Failed configurations:"
    for config in "${FAILED[@]}"; do
      echo -e "  ${COLOR_RED}✗${COLOR_NC} $config"
    done
    echo ""

    error "Some tests failed. See logs above for details."
    exit 1
  fi

  echo -e "${COLOR_GREEN}╔═══════════════════════════════════════════════════════════════╗${COLOR_NC}"
  echo -e "${COLOR_GREEN}║                   ALL TESTS PASSED!                           ║${COLOR_NC}"
  echo -e "${COLOR_GREEN}╚═══════════════════════════════════════════════════════════════╝${COLOR_NC}"
  echo ""

  # Quick test mode - skip benchmarks
  if [[ "$QUICK_TEST" == "true" ]]; then
    info "Quick test mode - skipping benchmarks"
    exit 0
  fi

  # Optional: Run benchmarks
  if [[ "$RUN_BENCHMARKS" == "true" ]]; then
    section "Running Benchmarks (Optional)"
    warn "Benchmarks can take 30+ minutes. Press Ctrl+C to skip."
    read -p "Run benchmarks? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
      info "Running comprehensive benchmarks..."
      if [[ -f "scripts/benchmark-all.sh" ]]; then
        ./scripts/benchmark-all.sh
      else
        warn "Benchmark script not found. Skipping."
      fi
    else
      info "Skipping benchmarks."
    fi
  fi

  section "Developer Quick Reference"
  echo "Common commands:"
  echo "  ./scripts/test-everything.sh --quick       # Quick validation"
  echo "  ./scripts/build-all-and-test.sh --vcpkg    # Full CI test"
  echo "  BUILD_DIR=build ./scripts/clean-build.sh -y # Clean rebuild"
  echo ""
  echo "For more information, see:"
  echo "  - TESTING.md                  - Testing documentation"
  echo "  - BUILD_SYSTEM_ARCHITECTURE.md - Build system documentation"
  echo "  - README.md                   - Project documentation"
  echo ""
}

# Run main function
main "$@"
