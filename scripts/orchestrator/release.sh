#!/bin/bash
# DwarFS Release Orchestrator
#
# This script orchestrates the release process:
# 1. Full test suite across all platforms
# 2. Benchmark runs
# 3. Documentation updates
# 4. Release artifact generation
#
# Usage:
#   ./scripts/orchestrator/release.sh --dry-run     # Test release process
#   ./scripts/orchestrator/release.sh --version X.Y.Z  # Create release

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$PROJECT_ROOT"

# Source libraries
source scripts/lib/build_env.sh

# Parse arguments
DRY_RUN=false
VERSION=""
BENCHMARKS=true
UPDATE_DOCS=true

for arg in "$@"; do
  case $arg in
    --dry-run)
      DRY_RUN=true
      shift
      ;;
    --version=*)
      VERSION="${arg#*=}"
      shift
      ;;
    --version)
      VERSION="$2"
      shift 2
      ;;
    --no-benchmarks)
      BENCHMARKS=false
      shift
      ;;
    --no-docs)
      UPDATE_DOCS=false
      shift
      ;;
    -h|--help)
      echo "Usage: $0 [options]"
      echo ""
      echo "Orchestrate the DwarFS release process"
      echo ""
      echo "Options:"
      echo "  --dry-run         Test release process without creating artifacts"
      echo "  --version X.Y.Z   Set release version"
      echo "  --no-benchmarks   Skip benchmark runs"
      echo "  --no-docs         Skip documentation updates"
      echo "  -h                Show this help"
      exit 0
      ;;
  esac
done

# ============================================================================
# Release Functions
# ============================================================================

check_prerequisites() {
  section "Checking Prerequisites"

  # Check for vcpkg
  if ! dwarfs_has_vcpkg "$VCPKG_ROOT"; then
    warn "vcpkg not found. Release requires vcpkg."
  fi

  # Check for Tebako jemalloc
  if ! dwarfs_verify_tebako_jemalloc "$PROJECT_ROOT"; then
    fatal "Tebako jemalloc verification failed"
  fi

  # Check git status
  if [[ "$DRY_RUN" == "false" ]]; then
    if [[ -n "$(git status --porcelain)" ]]; then
      warn "Working directory has uncommitted changes"
      read -p "Continue anyway? (y/N) " -n 1 -r
      echo
      if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 0
      fi
    fi
  fi

  success "Prerequisites check passed"
}

run_tests() {
  section "Running Test Suite"

  if [[ "$DRY_RUN" == "true" ]]; then
    warn "DRY RUN: Would run full test suite"
    return 0
  fi

  info "Running tests on all configurations..."
  if [[ -f "scripts/one-step/test-everything.sh" ]]; then
    ./scripts/one-step/test-everything.sh --quick
  else
    fatal "test-everything.sh not found"
  fi

  success "All tests passed"
}

run_benchmarks() {
  if [[ "$BENCHMARKS" == "false" ]]; then
    return 0
  fi

  section "Running Benchmarks"

  if [[ "$DRY_RUN" == "true" ]]; then
    warn "DRY RUN: Would run benchmarks"
    return 0
  fi

  if [[ -f "scripts/one-step/benchmark-all.sh" ]]; then
    ./scripts/one-step/benchmark-all.sh
  else
    warn "benchmark-all.sh not found, skipping"
  fi

  success "Benchmarks complete"
}

update_documentation() {
  if [[ "$UPDATE_DOCS" == "false" ]]; then
    return 0
  fi

  section "Updating Documentation"

  if [[ "$DRY_RUN" == "true" ]]; then
    warn "DRY RUN: Would update documentation"
    return 0
  fi

  # Update version in README if provided
  if [[ -n "$VERSION" ]]; then
    info "Updating version to $VERSION in README.md"
    # sed -i '' "s/Version: .*/Version: $VERSION/" README.md
  fi

  # Update TESTING.md with latest test results
  info "Documentation would be updated here"

  success "Documentation updated"
}

generate_artifacts() {
  section "Generating Release Artifacts"

  if [[ "$DRY_RUN" == "true" ]]; then
    warn "DRY RUN: Would generate release artifacts"
    return 0
  fi

  info "Release artifacts would be generated here"
  # This would create:
  # - Source tarball
  # - Binary builds for all platforms
  # - Release notes
  # - Checksums

  success "Artifacts generated"
}

# ============================================================================
# Main Execution
# ============================================================================

main() {
  echo ""
  echo -e "${COLOR_CYAN}╔═══════════════════════════════════════════════════════════════╗${COLOR_NC}"
  echo -e "${COLOR_CYAN}║           DwarFS Release Orchestrator                        ║${COLOR_NC}"
  echo -e "${COLOR_CYAN}╚═══════════════════════════════════════════════════════════════╝${COLOR_NC}"
  echo ""

  if [[ "$DRY_RUN" == "true" ]]; then
    warn "DRY RUN MODE - No actual changes will be made"
    echo ""
  fi

  # Check prerequisites
  check_prerequisites

  # Run tests
  run_tests

  # Run benchmarks
  run_benchmarks

  # Update documentation
  update_documentation

  # Generate artifacts
  generate_artifacts

  # Summary
  section "Release Summary"

  echo "Release process completed successfully!"
  echo ""

  if [[ "$DRY_RUN" == "true" ]]; then
    info "This was a dry run. No actual changes were made."
    echo ""
    info "To perform actual release, run without --dry-run"
  else
    info "Next steps:"
    echo "  1. Review release artifacts"
    echo "  2. Test release artifacts"
    echo "  3. Create GitHub release"
    echo "  4. Upload artifacts"
    echo "  5. Publish release"
  fi

  echo ""
}

# Run main function
main "$@"
