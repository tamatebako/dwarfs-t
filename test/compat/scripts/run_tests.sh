#!/usr/bin/env bash
# vim:set ts=2 sw=2 sts=2 et:
#
# \author     DwarFS Implementation
# \copyright  Copyright (c) Marcus Holland-Moritz
#
# This file is part of dwarfs.
#
# dwarfs is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# dwarfs is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.

set -euo pipefail

# Default values
VERBOSE=false
FAIL_FAST=false
VERSION="latest"
PLATFORM=""
ARCH=""
TEST_TYPES="all"
FORMAT="table"
OUTPUT_FILE=""
BUILD_DIR=""

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Test results tracking
declare -a TEST_RESULTS=()
declare -i TOTAL_TESTS=0
declare -i PASSED_TESTS=0
declare -i FAILED_TESTS=0

#
# Print usage
#
usage() {
  cat << EOF
Usage: $0 [OPTIONS]

Run Homebrew compatibility tests for dwarfs.

This script executes compatibility tests between the Legacy Thrift
implementation and Homebrew's dwarfs tool.

OPTIONS:
  -h, --help              Show this help message
  -v, --verbose           Enable verbose output
  -F, --fail-fast         Stop on first failure
  -V, --version VERSION   dwarfs version to test (default: 'latest')
  -p, --platform PLATFORM Platform: 'darwin' or 'linux' (default: auto-detect)
  -a, --arch ARCH         Architecture: 'arm64' or 'x86_64' (default: auto-detect)
  -t, --types TYPES       Test types: 'all', 'read', 'write', 'roundtrip' (default: 'all')
  -f, --format FORMAT     Output format: 'table' (default) or 'json'
  -o, --output FILE       Save results to file
  -b, --build DIR         Build directory containing test binaries

EXAMPLES:
  $0                                          # Run all tests
  $0 --types read,write                      # Run only read and write tests
  $0 --verbose --fail-fast                   # Run with verbose output and fail-fast
  $0 --format json --output results.json     # Run and save JSON results

TEST TYPES:
  read      - Test reading Homebrew-generated DFT files
  write     - Test writing DFT files Homebrew can read
  roundtrip - Test full round-trip serialization
  all       - Run all test types (default)

EXIT CODES:
  0 - All tests passed
  1 - One or more tests failed
  2 - Error in script execution

EOF
}

#
# Print message based on verbosity
#
log() {
  local level="$1"
  shift
  local msg="$*"

  if [[ "$level" == "ERROR" ]]; then
    echo -e "${RED}[ERROR]${NC} $msg" >&2
  elif [[ "$level" == "WARN" ]]; then
    echo -e "${YELLOW}[WARN]${NC} $msg" >&2
  elif [[ "$level" == "INFO" ]]; then
    echo -e "${BLUE}[INFO]${NC} $msg" >&2
  elif [[ "$VERBOSE" == "true" ]] || [[ "$level" == "SUCCESS" ]]; then
    if [[ "$level" == "SUCCESS" ]]; then
      echo -e "${GREEN}[SUCCESS]${NC} $msg"
    else
      echo -e "[DEBUG] $msg"
    fi
  fi
}

#
# Detect platform using uname
#
detect_platform() {
  local sysname
  sysname=$(uname -s)

  case "$sysname" in
    Darwin)
      echo "darwin"
      ;;
    Linux)
      echo "linux"
      ;;
    *)
      echo "unknown"
      ;;
  esac
}

#
# Detect architecture using uname
#
detect_arch() {
  local machine
  machine=$(uname -m)

  case "$machine" in
    x86_64|amd64)
      echo "x86_64"
      ;;
    arm64|aarch64)
      echo "arm64"
      ;;
    *)
      echo "$machine"
      ;;
  esac
}

#
# Find Homebrew prefix
#
find_homebrew_prefix() {
  local prefixes=(
    "/opt/homebrew"       # macOS arm64
    "/usr/local"          # macOS x86_64
    "/home/linuxbrew/.linuxbrew"  # Linuxbrew
    "/home/linuxbrew"     # Linuxbrew system-wide
    "$HOME/.linuxbrew"    # Linuxbrew user-local
  )

  for prefix in "${prefixes[@]}"; do
    if [[ -d "$prefix/bin" ]]; then
      echo "$prefix"
      return 0
    fi
  done

  return 1
}

#
# Check if Homebrew dwarfs is available
#
check_homebrew() {
  log INFO "Checking Homebrew dwarfs availability..."

  local prefix
  if ! prefix=$(find_homebrew_prefix); then
    log ERROR "Homebrew not found"
    return 1
  fi

  if [[ ! -x "$prefix/bin/mkdwarfs" ]]; then
    log ERROR "mkdwarfs not found in $prefix/bin"
    return 1
  fi

  if [[ ! -x "$prefix/bin/dwarfs" ]]; then
    log ERROR "dwarfs not found in $prefix/bin"
    return 1
  fi

  log INFO "Homebrew dwarfs is available"
  return 0
}

#
# Run a single test
#
run_test() {
  local test_name="$1"
  local version="$2"
  local platform="$3"
  local arch="$4"
  local test_binary="$5"

  log INFO "Running test: $test_name"

  local start_time
  start_time=$(date +%s.%N)

  # Run the test binary
  local output
  local exit_code=0

  if [[ "$VERBOSE" == "true" ]]; then
    if output=$("$test_binary" "$test_name" "$version" "$platform" "$arch" 2>&1); then
      exit_code=$?
    else
      exit_code=$?
    fi
  else
    if output=$("$test_binary" "$test_name" "$version" "$platform" "$arch" 2>&1); then
      exit_code=$?
    else
      exit_code=$?
    fi
  fi

  local end_time
  end_time=$(date +%s.%N)
  local duration
  duration=$(echo "$end_time - $start_time" | bc)

  local passed="false"
  local error_message=""

  if [[ $exit_code -eq 0 ]]; then
    passed="true"
    ((PASSED_TESTS++))
    log SUCCESS "Test passed: $test_name"
  else
    ((FAILED_TESTS++))
    error_message="$output"
    log ERROR "Test failed: $test_name - $output"
  fi

  ((TOTAL_TESTS++))

  # Store result
  TEST_RESULTS+=("$test_name|$version|$platform|$arch|$passed|$duration|$error_message")

  return $exit_code
}

#
# Format results as table
#
format_table() {
  echo ""
  echo -e "${BOLD}Homebrew Compatibility Test Results${NC}"
  echo "======================================"
  echo ""
  printf "%-30s %-12s %-10s %-8s %-8s %-12s\n" "Test Name" "Version" "Platform" "Arch" "Status" "Duration"
  printf "%s\n" "--------------------------------------------------------------------------------"

  for result in "${TEST_RESULTS[@]}"; do
    IFS='|' read -r test_name version platform arch passed duration error <<< "$result"

    local status
    local status_color

    if [[ "$passed" == "true" ]]; then
      status="PASS"
      status_color="$GREEN"
    else
      status="FAIL"
      status_color="$RED"
    fi

    printf "%-30s %-12s %-10s %-8s " "$test_name" "$version" "$platform" "$arch"
    echo -e "${status_color}${status}${NC} %-12s" "${duration}s"

    if [[ -n "$error" ]]; then
      printf "    Error: %s\n" "$error"
    fi
  done

  echo ""
  echo "Summary: $PASSED_TESTS passed, $FAILED_TESTS failed (total: $TOTAL_TESTS)"
  echo ""
}

#
# Format results as JSON
#
format_json() {
  echo "{"
  echo "  \"results\": ["

  local first=true
  for result in "${TEST_RESULTS[@]}"; do
    if [[ "$first" == "true" ]]; then
      first=false
    else
      echo ","
    fi

    IFS='|' read -r test_name version platform arch passed duration error <<< "$result"

    printf '    {\n'
    printf '      "test_name": "%s",\n' "$test_name"
    printf '      "dwarfs_version": "%s",\n' "$version"
    printf '      "platform": "%s",\n' "$platform"
    printf '      "arch": "%s",\n' "$arch"
    printf '      "passed": %s,\n' "$passed"
    printf '      "duration_seconds": %s' "$duration"

    if [[ -n "$error" ]]; then
      printf ',\n      "error_message": "%s"\n' "$error"
    else
      printf '\n'
    fi

    printf -v '    }'
  done

  echo ""
  echo "  ],"
  echo "  \"summary\": {"
  echo "    \"total\": $TOTAL_TESTS,"
  echo "    \"passed\": $PASSED_TESTS,"
  echo "    \"failed\": $FAILED_TESTS"
  echo "  }"
  echo "}"
}

#
# Save results to file
#
save_results() {
  local output_file="$1"

  mkdir -p "$(dirname "$output_file")"

  if [[ "$FORMAT" == "json" ]]; then
    format_json > "$output_file"
  else
    format_table > "$output_file"
  fi

  log INFO "Results saved to: $output_file"
}

#
# Find or build test binary
#
get_test_binary() {
  local test_name="$1"

  # Check if build directory is specified
  if [[ -n "$BUILD_DIR" ]]; then
    local binary="$BUILD_DIR/test/compat/$test_name"
    if [[ -x "$binary" ]]; then
      echo "$binary"
      return 0
    fi
  fi

  # Check common build directories
  local build_dirs=(
    "${PROJECT_ROOT}/build"
    "${PROJECT_ROOT}/build-test"
    "${PROJECT_ROOT}/cmake-build-release"
    "${PROJECT_ROOT}/cmake-build-debug"
  )

  for build_dir in "${build_dirs[@]}"; do
    local binary="$build_dir/test/compat/$test_name"
    if [[ -x "$binary" ]]; then
      echo "$binary"
      return 0
    fi
  done

  log WARN "Test binary not found: $test_name"
  echo ""
  return 1
}

#
# Parse test types
#
parse_test_types() {
  local types="$1"
  local -a result=()

  if [[ "$types" == "all" ]]; then
    result=("read_homebrew_files" "write_compatible_files" "round_trip")
  else
    IFS=',' read -ra TYPE_ARRAY <<< "$types"
    for type in "${TYPE_ARRAY[@]}"; do
      case "$type" in
        read)
          result+=("read_homebrew_files")
          ;;
        write)
          result+=("write_compatible_files")
          ;;
        roundtrip|round_trip)
          result+=("round_trip")
          ;;
        *)
          log WARN "Unknown test type: $type"
          ;;
      esac
    done
  fi

  printf '%s\n' "${result[@]}"
}

#
# Main function
#
main() {
  # Parse command line arguments
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -h|--help)
        usage
        exit 0
        ;;
      -v|--verbose)
        VERBOSE=true
        shift
        ;;
      -F|--fail-fast)
        FAIL_FAST=true
        shift
        ;;
      -V|--version)
        VERSION="$2"
        shift 2
        ;;
      -p|--platform)
        PLATFORM="$2"
        shift 2
        ;;
      -a|--arch)
        ARCH="$2"
        shift 2
        ;;
      -t|--types)
        TEST_TYPES="$2"
        shift 2
        ;;
      -f|--format)
        FORMAT="$2"
        shift 2
        ;;
      -o|--output)
        OUTPUT_FILE="$2"
        shift 2
        ;;
      -b|--build)
        BUILD_DIR="$2"
        shift 2
        ;;
      *)
        echo "Error: Unknown option: $1" >&2
        usage
        exit 2
        ;;
    esac
  done

  log INFO "Starting Homebrew compatibility tests..."

  # Auto-detect platform/arch if not specified
  if [[ -z "$PLATFORM" ]]; then
    PLATFORM=$(detect_platform)
    log INFO "Auto-detected platform: $PLATFORM"
  fi

  if [[ -z "$ARCH" ]]; then
    ARCH=$(detect_arch)
    log INFO "Auto-detected architecture: $ARCH"
  fi

  # Check Homebrew availability
  if ! check_homebrew; then
    log ERROR "Homebrew dwarfs not available, cannot run tests"
    exit 1
  fi

  # Get test types to run
  local -a tests
  mapfile -t tests < <(parse_test_types "$TEST_TYPES")

  if [[ ${#tests[@]} -eq 0 ]]; then
    log ERROR "No valid test types specified"
    exit 1
  fi

  log INFO "Test types to run: ${tests[*]}"

  # Run tests
  local has_failures=false

  for test_name in "${tests[@]}"; do
    local test_binary
    test_binary=$(get_test_binary "$test_name")

    if [[ -z "$test_binary" ]]; then
      log ERROR "Test binary not found: $test_name"
      log ERROR "Build the tests first or specify --build DIR"
      ((FAILED_TESTS++))
      ((TOTAL_TESTS++))
      has_failures=true

      if [[ "$FAIL_FAST" == "true" ]]; then
        break
      fi
      continue
    fi

    if ! run_test "$test_name" "$VERSION" "$PLATFORM" "$ARCH" "$test_binary"; then
      has_failures=true

      if [[ "$FAIL_FAST" == "true" ]]; then
        log INFO "Fail-fast enabled, stopping tests"
        break
      fi
    fi
  done

  # Output results
  if [[ "$FORMAT" == "json" ]]; then
    format_json
  else
    format_table
  fi

  # Save to file if specified
  if [[ -n "$OUTPUT_FILE" ]]; then
    save_results "$OUTPUT_FILE"
  fi

  # Return appropriate exit code
  if [[ "$has_failures" == "true" ]]; then
    exit 1
  else
    exit 0
  fi
}

# Run main
main "$@"
