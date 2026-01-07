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
FORCE=false
VERSION="latest"
PLATFORM=""
ARCH=""
OUTPUT_DIR=""
CACHE_DIR=""
CONFIG_FILE=""

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Fixture file extension
FIXTURE_EXT=".dft"

#
# Print usage
#
usage() {
  cat << EOF
Usage: $0 [OPTIONS]

Generate Homebrew dwarfs test fixture files (.dft).

This script creates test fixtures by:
  1. Creating a temporary directory with test files
  2. Invoking mkdwarfs to create a DFT file
  3. Caching the fixture with SHA256 checksum validation

OPTIONS:
  -h, --help              Show this help message
  -v, --verbose           Enable verbose output
  -f, --force             Force regeneration even if fixture exists
  -V, --version VERSION   dwarfs version (default: 'latest')
  -p, --platform PLATFORM Platform: 'darwin' or 'linux' (default: auto-detect)
  -a, --arch ARCH         Architecture: 'arm64' or 'x86_64' (default: auto-detect)
  -o, --output DIR        Output directory for fixtures
  -c, --cache DIR         Cache directory for fixtures
  -C, --config FILE       Config file path (default: test/compat/config.yaml)

EXAMPLES:
  $0                                          # Generate for current platform
  $0 --version 0.14.1                         # Generate specific version
  $0 --platform darwin --arch arm64           # Generate for macOS arm64
  $0 --force --verbose                        # Force regeneration with verbose output

EXIT CODES:
  0 - Fixture generated successfully or already exists
  1 - Error generating fixture
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
# Find mkdwarfs binary
#
find_mkdwarfs() {
  local prefix="$1"

  if [[ -x "$prefix/bin/mkdwarfs" ]]; then
    echo "$prefix/bin/mkdwarfs"
    return 0
  fi

  return 1
}

#
# Generate fixture filename
#
get_fixture_filename() {
  local version="$1"
  local platform="$2"
  local arch="$3"

  echo "dwarfs-v${version}-${platform}-${arch}${FIXTURE_EXT}"
}

#
# Check if fixture exists and is valid
#
fixture_exists() {
  local filepath="$1"
  local checksum_file="${filepath}.sha256"

  if [[ ! -f "$filepath" ]]; then
    return 1
  fi

  # If checksum file exists, validate it
  if [[ -f "$checksum_file" ]]; then
    local stored_hash
    stored_hash=$(cut -d' ' -f1 "$checksum_file")

    local current_hash
    current_hash=$(sha256sum "$filepath" | cut -d' ' -f1)

    if [[ "$stored_hash" != "$current_hash" ]]; then
      log WARN "Fixture checksum mismatch"
      return 1
    fi
  fi

  return 0
}

#
# Create temporary test directory
#
create_temp_test_dir() {
  local temp_dir_template="${TMPDIR:-/tmp}/dwarfs-compat-XXXXXX"
  mktemp -d "$temp_dir_template"
}

#
# Create test files in directory
#
create_test_files() {
  local test_dir="$1"

  log INFO "Creating test files in $test_dir"

  # Create some directories
  mkdir -p "$test_dir/subdir1"
  mkdir -p "$test_dir/subdir2/nested"

  # Create test files with various sizes
  echo "Small file content" > "$test_dir/small.txt"

  # Create a larger file
  dd if=/dev/urandom of="$test_dir/medium.bin" bs=1024 count=100 2>/dev/null

  # Create files in subdirectories
  echo "Subdirectory file 1" > "$test_dir/subdir1/file1.txt"
  echo "Subdirectory file 2" > "$test_dir/subdir1/file2.txt"
  echo "Nested file" > "$test_dir/subdir2/nested/file.txt"

  # Create a symlink if supported
  if [[ "$OSTYPE" != "msys" ]] && [[ "$OSTYPE" != "win32" ]]; then
    ln -s "small.txt" "$test_dir/link_to_small.txt"
  fi

  log VERBOSE "Created test file structure"
}

#
# Invoke mkdwarfs to create fixture
#
invoke_mkdwarfs() {
  local mkdwarfs="$1"
  local source_dir="$2"
  local output_file="$3"

  log INFO "Invoking mkdwarfs..."

  local cmd=(
    "$mkdwarfs"
    -i "$source_dir"
    -o "$output_file"
    -l 5
  )

  if [[ "$VERBOSE" == "true" ]]; then
    log INFO "Running: ${cmd[*]}"
  fi

  if "${cmd[@]}" 2>&1; then
    log SUCCESS "mkdwarfs completed successfully"
    return 0
  else
    local exit_code=$?
    log ERROR "mkdwarfs failed with exit code: $exit_code"
    return 1
  fi
}

#
# Calculate and save checksum
#
save_checksum() {
  local filepath="$1"
  local platform="$2"
  local arch="$3"

  local checksum_file="${filepath}.sha256"
  local filename
  filename=$(basename "$filepath")
  local filesize
  filesize=$(stat -f%z "$filepath" 2>/dev/null || stat -c%s "$filepath" 2>/dev/null)
  local hash
  hash=$(sha256sum "$filepath" | cut -d' ' -f1)

  echo "$hash $filesize ${platform}-${arch}" > "$checksum_file"

  log INFO "Saved checksum: $hash"
}

#
# Generate fixture
#
generate_fixture() {
  local version="$1"
  local platform="$2"
  local arch="$3"
  local output_dir="$4"

  local filename
  filename=$(get_fixture_filename "$version" "$platform" "$arch")
  local output_file="$output_dir/$filename"

  # Check if fixture already exists
  if [[ "$FORCE" != "true" ]] && fixture_exists "$output_file"; then
    log INFO "Fixture already exists: $output_file"
    log INFO "Use --force to regenerate"
    return 0
  fi

  log INFO "Generating fixture: $filename"

  # Find Homebrew and mkdwarfs
  local prefix
  if ! prefix=$(find_homebrew_prefix); then
    log ERROR "Homebrew not found"
    return 1
  fi

  local mkdwarfs
  if ! mkdwarfs=$(find_mkdwarfs "$prefix"); then
    log ERROR "mkdwarfs not found in $prefix/bin"
    return 1
  fi

  log INFO "Using mkdwarfs: $mkdwarfs"

  # Create output directory
  mkdir -p "$output_dir"

  # Create temporary test directory
  local temp_dir
  temp_dir=$(create_temp_test_dir)
  trap "rm -rf '$temp_dir'" EXIT

  log VERBOSE "Created temp directory: $temp_dir"

  # Create test files
  create_test_files "$temp_dir"

  # Invoke mkdwarfs
  if ! invoke_mkdwarfs "$mkdwarfs" "$temp_dir" "$output_file"; then
    return 1
  fi

  # Verify output file exists
  if [[ ! -f "$output_file" ]]; then
    log ERROR "Output file not created: $output_file"
    return 1
  fi

  # Calculate and save checksum
  save_checksum "$output_file" "$platform" "$arch"

  local filesize
  filesize=$(stat -f%z "$output_file" 2>/dev/null || stat -c%s "$output_file" 2>/dev/null)

  log SUCCESS "Fixture generated: $output_file ($filesize bytes)"

  return 0
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
      -f|--force)
        FORCE=true
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
      -o|--output)
        OUTPUT_DIR="$2"
        shift 2
        ;;
      -c|--cache)
        CACHE_DIR="$2"
        shift 2
        ;;
      -C|--config)
        CONFIG_FILE="$2"
        shift 2
        ;;
      *)
        echo "Error: Unknown option: $1" >&2
        usage
        exit 2
        ;;
    esac
  done

  log INFO "Starting fixture generation..."

  # Auto-detect platform/arch if not specified
  if [[ -z "$PLATFORM" ]]; then
    PLATFORM=$(detect_platform)
    log INFO "Auto-detected platform: $PLATFORM"
  fi

  if [[ -z "$ARCH" ]]; then
    ARCH=$(detect_arch)
    log INFO "Auto-detected architecture: $ARCH"
  fi

  # Validate platform/arch
  if [[ "$PLATFORM" != "darwin" && "$PLATFORM" != "linux" ]]; then
    log ERROR "Invalid platform: $PLATFORM (must be 'darwin' or 'linux')"
    exit 1
  fi

  if [[ "$ARCH" != "arm64" && "$ARCH" != "x86_64" ]]; then
    log ERROR "Invalid architecture: $ARCH (must be 'arm64' or 'x86_64')"
    exit 1
  fi

  # Set output directory
  if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="${PROJECT_ROOT}/test/compat/fixtures"
  fi

  log INFO "Output directory: $OUTPUT_DIR"

  # Generate fixture
  if generate_fixture "$VERSION" "$PLATFORM" "$ARCH" "$OUTPUT_DIR"; then
    exit 0
  else
    exit 1
  fi
}

# Run main
main "$@"
