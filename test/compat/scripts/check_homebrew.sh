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
FORMAT="text"

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

#
# Print usage
#
usage() {
  cat << EOF
Usage: $0 [OPTIONS]

Check if Homebrew dwarfs is installed and accessible.

This script detects:
  - Homebrew installation (macOS Homebrew or Linuxbrew)
  - Platform (darwin/linux) and architecture (arm64/x86_64)
  - mkdwarfs binary location
  - dwarfs binary location
  - dwarfs version

OPTIONS:
  -h, --help          Show this help message
  -v, --verbose       Enable verbose output
  -f, --format FORMAT Output format: 'text' (default) or 'json'

EXAMPLES:
  $0                              # Check Homebrew dwarfs (text output)
  $0 --verbose                    # Check with verbose output
  $0 --format json                # Check and output JSON

EXIT CODES:
  0 - Homebrew dwarfs is fully installed and accessible
  1 - Homebrew dwarfs is not installed or incomplete
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
# Find dwarfs binary
#
find_dwarfs() {
  local prefix="$1"

  if [[ -x "$prefix/bin/dwarfs" ]]; then
    echo "$prefix/bin/dwarfs"
    return 0
  fi

  return 1
}

#
# Get dwarfs version from binary
#
get_dwarfs_version() {
  local mkdwarfs="$1"

  if [[ ! -x "$mkdwarfs" ]]; then
    return 1
  fi

  local version
  version=$("$mkdwarfs" --version 2>&1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)

  if [[ -n "$version" ]]; then
    echo "$version"
    return 0
  fi

  echo "unknown"
  return 1
}

#
# Output results in text format
#
output_text() {
  local installed="$1"
  local platform="$2"
  local arch="$3"
  local prefix="$4"
  local mkdwarfs="$5"
  local dwarfs="$6"
  local version="$7"

  echo "Homebrew dwarfs Detection Results"
  echo "=================================="
  echo

  if [[ "$installed" == "true" ]]; then
    echo -e "${GREEN}Status: Installed${NC}"
    echo "Platform: $platform"
    echo "Architecture: $arch"
    echo "Homebrew Prefix: $prefix"
    echo "mkdwarfs: $mkdwarfs"
    echo "dwarfs: $dwarfs"
    echo "Version: $version"
  else
    echo -e "${RED}Status: Not Installed or Incomplete${NC}"
    if [[ -n "$platform" ]]; then
      echo "Platform: $platform"
    fi
    if [[ -n "$arch" ]]; then
      echo "Architecture: $arch"
    fi
    if [[ -n "$prefix" ]]; then
      echo "Homebrew Prefix: $prefix (found)"
    else
      echo "Homebrew Prefix: (not found)"
    fi
    if [[ -z "$mkdwarfs" ]]; then
      echo -e "${RED}mkdwarfs: (not found)${NC}"
    fi
    if [[ -z "$dwarfs" ]]; then
      echo -e "${RED}dwarfs: (not found)${NC}"
    fi
  fi

  echo
}

#
# Output results in JSON format
#
output_json() {
  local installed="$1"
  local platform="$2"
  local arch="$3"
  local prefix="$4"
  local mkdwarfs="$5"
  local dwarfs="$6"
  local version="$7"

  cat << EOF
{
  "installed": $installed,
  "platform": "${platform}",
  "arch": "${arch}",
  "prefix": "${prefix}",
  "mkdwarfs_path": "${mkdwarfs}",
  "dwarfs_path": "${dwarfs}",
  "version": "${version}"
}
EOF
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
      -f|--format)
        FORMAT="$2"
        shift 2
        ;;
      *)
        echo "Error: Unknown option: $1" >&2
        usage
        exit 2
        ;;
    esac
  done

  log INFO "Starting Homebrew dwarfs detection..."

  # Detect platform and architecture
  local platform
  local arch
  platform=$(detect_platform)
  arch=$(detect_arch)

  log INFO "Detected platform: $platform"
  log INFO "Detected architecture: $arch"

  # Find Homebrew prefix
  local prefix=""
  if prefix=$(find_homebrew_prefix); then
    log INFO "Found Homebrew prefix: $prefix"
  else
    log WARN "Homebrew not found"
  fi

  # Find binaries
  local mkdwarfs=""
  local dwarfs=""

  if [[ -n "$prefix" ]]; then
    if mkdwarfs=$(find_mkdwarfs "$prefix"); then
      log INFO "Found mkdwarfs: $mkdwarfs"
    else
      log WARN "mkdwarfs not found in $prefix/bin"
    fi

    if dwarfs=$(find_dwarfs "$prefix"); then
      log INFO "Found dwarfs: $dwarfs"
    else
      log WARN "dwarfs not found in $prefix/bin"
    fi
  fi

  # Get version
  local version="unknown"
  if [[ -n "$mkdwarfs" ]]; then
    version=$(get_dwarfs_version "$mkdwarfs")
    log INFO "dwarfs version: $version"
  fi

  # Determine if fully installed
  local installed="false"
  if [[ -n "$prefix" && -n "$mkdwarfs" && -n "$dwarfs" && "$version" != "unknown" ]]; then
    installed="true"
    log SUCCESS "Homebrew dwarfs is fully installed and accessible!"
  else
    log ERROR "Homebrew dwarfs is not installed or incomplete"
  fi

  # Output results
  if [[ "$FORMAT" == "json" ]]; then
    output_json "$installed" "$platform" "$arch" "$prefix" "$mkdwarfs" "$dwarfs" "$version"
  else
    output_text "$installed" "$platform" "$arch" "$prefix" "$mkdwarfs" "$dwarfs" "$version"
  fi

  # Return appropriate exit code
  if [[ "$installed" == "true" ]]; then
    exit 0
  else
    exit 1
  fi
}

# Run main
main "$@"
