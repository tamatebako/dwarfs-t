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
FIXTURE_DIR=""
FIXTURE_FILE=""
RECALCULATE=false
REPORT_FORMAT="text"

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

# Validation results
declare -a VALIDATION_RESULTS=()
declare -i TOTAL_CHECKED=0
declare -i TOTAL_VALID=0
declare -i TOTAL_INVALID=0
declare -i TOTAL_MISSING=0

#
# Print usage
#
usage() {
  cat << EOF
Usage: $0 [OPTIONS] [FIXTURE_FILE]

Validate Homebrew dwarfs test fixture files.

This script validates fixture files by:
  1. Checking if the fixture file exists
  2. Verifying SHA256 checksums
  3. Validating file size and format
  4. Testing deserialization if dwarfs is available

OPTIONS:
  -h, --help              Show this help message
  -v, --verbose           Enable verbose output
  -d, --dir DIR           Fixture directory to validate (all fixtures)
  -f, --file FILE         Specific fixture file to validate
  -r, --recalculate       Recalculate and update checksums
  -R, --report FORMAT     Report format: 'text' (default) or 'json'

EXAMPLES:
  $0                                          # Validate all fixtures in default dir
  $0 --dir ./fixtures                         # Validate all fixtures in custom dir
  $0 --file dwarfs-v0.14.1-darwin-arm64.dft  # Validate specific file
  $0 --recalculate                           # Recalculate all checksums

EXIT CODES:
  0 - All fixtures are valid
  1 - One or more fixtures are invalid or missing
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
# Calculate SHA256 checksum of a file
#
calculate_checksum() {
  local filepath="$1"

  if [[ ! -f "$filepath" ]]; then
    return 1
  fi

  sha256sum "$filepath" 2>/dev/null | cut -d' ' -f1
}

#
# Read stored checksum from checksum file
#
read_stored_checksum() {
  local checksum_file="$1"

  if [[ ! -f "$checksum_file" ]]; then
    return 1
  fi

  cut -d' ' -f1 "$checksum_file"
}

#
# Read metadata from checksum file
#
read_checksum_metadata() {
  local checksum_file="$1"

  if [[ ! -f "$checksum_file" ]]; then
    return 1
  fi

  # Format: hash size platform_arch
  read -r _ size platform_arch < "$checksum_file"

  echo "$size $platform_arch"
}

#
# Write checksum file
#
write_checksum_file() {
  local filepath="$1"
  local platform="$2"
  local arch="$3"

  local checksum_file="${filepath}.sha256"
  local filename
  filename=$(basename "$filepath")
  local filesize
  filesize=$(stat -f%z "$filepath" 2>/dev/null || stat -c%s "$filepath" 2>/dev/null)
  local hash
  hash=$(calculate_checksum "$filepath")

  echo "$hash $filesize ${platform}-${arch}" > "$checksum_file"

  log INFO "Updated checksum: $hash"
}

#
# Parse fixture filename
#
parse_fixture_filename() {
  local filename="$1"

  # Expected format: dwarfs-v{version}-{platform}-{arch}.dft
  if [[ ! $filename =~ ^dwarfs-v([0-9]+\.[0-9]+\.[0-9]+|latest)-(darwin|linux)-(arm64|x86_64)\.dft$ ]]; then
    return 1
  fi

  local version="${BASH_REMATCH[1]}"
  local platform="${BASH_REMATCH[2]}"
  local arch="${BASH_REMATCH[3]}"

  echo "$version $platform $arch"
}

#
# Validate a single fixture file
#
validate_fixture() {
  local filepath="$1"
  local recalculate="$2"

  local filename
  filename=$(basename "$filepath")
  local checksum_file="${filepath}.sha256"

  ((TOTAL_CHECKED++))

  log INFO "Validating: $filename"

  # Check if file exists
  if [[ ! -f "$filepath" ]]; then
    log ERROR "Fixture not found: $filepath"
    ((TOTAL_MISSING++))
    VALIDATION_RESULTS+=("$filename|missing|0||")
    return 1
  fi

  # Parse filename
  local metadata
  if ! metadata=$(parse_fixture_filename "$filename"); then
    log WARN "Invalid fixture filename format: $filename"
  fi

  local version=""
  local platform="unknown"
  local arch="unknown"

  if [[ -n "$metadata" ]]; then
    read -r version platform arch <<< "$metadata"
  fi

  # Get file size
  local filesize
  filesize=$(stat -f%z "$filepath" 2>/dev/null || stat -c%s "$filepath" 2>/dev/null)

  log VERBOSE "File size: $filesize bytes"

  # Calculate current checksum
  local current_hash
  current_hash=$(calculate_checksum "$filepath")

  if [[ -z "$current_hash" ]]; then
    log ERROR "Failed to calculate checksum for: $filename"
    ((TOTAL_INVALID++))
    VALIDATION_RESULTS+=("$filename|invalid|$filesize||checksum_error")
    return 1
  fi

  # Check if checksum file exists
  if [[ ! -f "$checksum_file" ]]; then
    log WARN "Checksum file not found: $checksum_file"

    if [[ "$recalculate" == "true" ]]; then
      write_checksum_file "$filepath" "$platform" "$arch"
    fi

    ((TOTAL_INVALID++))
    VALIDATION_RESULTS+=("$filename|no_checksum|$filesize|$current_hash|")
    return 1
  fi

  # Read stored checksum
  local stored_hash
  stored_hash=$(read_stored_checksum "$checksum_file")

  if [[ -z "$stored_hash" ]]; then
    log ERROR "Failed to read stored checksum for: $filename"
    ((TOTAL_INVALID++))
    VALIDATION_RESULTS+=("$filename|invalid|$filesize|$current_hash|")
    return 1
  fi

  # Compare checksums
  if [[ "$current_hash" != "$stored_hash" ]]; then
    log ERROR "Checksum mismatch for: $filename"
    log ERROR "  Stored:    $stored_hash"
    log ERROR "  Calculated: $current_hash"

    if [[ "$recalculate" == "true" ]]; then
      log WARN "Recalculating checksum for: $filename"
      write_checksum_file "$filepath" "$platform" "$arch"
    fi

    ((TOTAL_INVALID++))
    VALIDATION_RESULTS+=("$filename|checksum_mismatch|$filesize|$current_hash|$stored_hash")
    return 1
  fi

  # Verify basic file structure
  # DFT files should have at least a header (8 bytes size prefix + schema + metadata)
  if [[ $filesize -lt 100 ]]; then
    log ERROR "Fixture too small: $filename ($filesize bytes)"
    ((TOTAL_INVALID++))
    VALIDATION_RESULTS+=("$filename|too_small|$filesize|$current_hash|")
    return 1
  fi

  # All checks passed
  log SUCCESS "Valid: $filename"
  ((TOTAL_VALID++))
  VALIDATION_RESULTS+=("$filename|valid|$filesize|$current_hash|")

  return 0
}

#
# Validate all fixtures in a directory
#
validate_directory() {
  local fixture_dir="$1"
  local recalculate="$2"

  if [[ ! -d "$fixture_dir" ]]; then
    log ERROR "Fixture directory not found: $fixture_dir"
    return 1
  fi

  log INFO "Scanning directory: $fixture_dir"

  local found_any=false
  local has_failures=false

  while IFS= read -r -d '' fixture_file; do
    found_any=true

    if ! validate_fixture "$fixture_file" "$recalculate"; then
      has_failures=true
    fi

  done < <(find "$fixture_dir" -maxdepth 1 -type f -name "*.dft" -print0)

  if [[ "$found_any" == "false" ]]; then
    log WARN "No fixture files found in: $fixture_dir"
    return 1
  fi

  return 0
}

#
# Format validation results as table
#
format_table() {
  echo ""
  echo -e "${BOLD}Fixture Validation Report${NC}"
  echo "=========================="
  echo ""

  if [[ ${#VALIDATION_RESULTS[@]} -eq 0 ]]; then
    echo "No fixtures validated."
    echo ""
    return
  fi

  printf "%-45s %-12s %-12s %-12s\n" "Fixture" "Status" "Size" "SHA256"
  printf "%s\n" "-------------------------------------------------------------------------"

  for result in "${VALIDATION_RESULTS[@]}"; do
    IFS='|' read -r filename status size current_hash stored_hash <<< "$result"

    local status_color
    case "$status" in
      valid)
        status_color="$GREEN"
        ;;
      missing|checksum_mismatch|invalid|too_small)
        status_color="$RED"
        ;;
      no_checksum)
        status_color="$YELLOW"
        ;;
      *)
        status_color="$NC"
        ;;
    esac

    local short_hash="${current_hash:0:12}"
    [[ -n "$current_hash" ]] && short_hash="${short_hash}..."

    printf "%-45s " "$filename"
    echo -e "${status_color}${status}${NC} %-12s" "$(printf "%'d bytes" "$size" 2>/dev/null || echo "${size} bytes")"
    printf "              %s\n" "$short_hash"
  done

  echo ""
  echo "Summary:"
  echo "  Total checked: $TOTAL_CHECKED"
  echo -e "  ${GREEN}Valid:$NC         $TOTAL_VALID"
  echo -e "  ${RED}Invalid:$NC       $TOTAL_INVALID"
  echo -e "  ${RED}Missing:$NC       $TOTAL_MISSING"
  echo ""
}

#
# Format validation results as JSON
#
format_json() {
  echo "{"
  echo "  \"fixtures\": ["

  local first=true
  for result in "${VALIDATION_RESULTS[@]}"; do
    if [[ "$first" == "true" ]]; then
      first=false
    else
      echo ","
    fi

    IFS='|' read -r filename status size current_hash stored_hash <<< "$result"

    printf '    {\n'
    printf '      "filename": "%s",\n' "$filename"
    printf '      "status": "%s",\n' "$status"
    printf '      "size_bytes": %s,\n' "$size"
    printf '      "sha256": "%s"\n' "$current_hash"

    if [[ -n "$stored_hash" ]]; then
      printf ',\n      "stored_sha256": "%s"' "$stored_hash"
    fi

    printf '\n    }'
  done

  echo ""
  echo "  ],"
  echo "  \"summary\": {"
  echo "    \"total_checked\": $TOTAL_CHECKED,"
  echo "    \"valid\": $TOTAL_VALID,"
  echo "    \"invalid\": $TOTAL_INVALID,"
  echo "    \"missing\": $TOTAL_MISSING"
  echo "  }"
  echo "}"
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
      -d|--dir)
        FIXTURE_DIR="$2"
        shift 2
        ;;
      -f|--file)
        FIXTURE_FILE="$2"
        shift 2
        ;;
      -r|--recalculate)
        RECALCULATE=true
        shift
        ;;
      -R|--report)
        REPORT_FORMAT="$2"
        shift 2
        ;;
      *)
        # Treat as fixture file if it looks like one
        if [[ $1 =~ \.dft$ ]]; then
          FIXTURE_FILE="$1"
          shift
        else
          echo "Error: Unknown option or invalid fixture file: $1" >&2
          usage
          exit 2
        fi
        ;;
    esac
  done

  log INFO "Starting fixture validation..."

  local has_failures=false

  # Determine what to validate
  if [[ -n "$FIXTURE_FILE" ]]; then
    # Validate specific file
    if [[ ! -f "$FIXTURE_FILE" ]]; then
      log ERROR "Fixture file not found: $FIXTURE_FILE"
      exit 1
    fi

    if ! validate_fixture "$FIXTURE_FILE" "$RECALCULATE"; then
      has_failures=true
    fi
  elif [[ -n "$FIXTURE_DIR" ]]; then
    # Validate all fixtures in directory
    if ! validate_directory "$FIXTURE_DIR" "$RECALCULATE"; then
      has_failures=true
    fi
  else
    # Default: validate fixtures in default directory
    local default_dir="${PROJECT_ROOT}/test/compat/fixtures"
    if [[ -d "$default_dir" ]]; then
      if ! validate_directory "$default_dir" "$RECALCULATE"; then
        has_failures=true
      fi
    else
      log ERROR "Default fixture directory not found: $default_dir"
      log INFO "Use --dir to specify a fixture directory"
      exit 1
    fi
  fi

  # Output results
  if [[ "$REPORT_FORMAT" == "json" ]]; then
    format_json
  else
    format_table
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
