#!/bin/bash
# DwarFS Build Environment Detection Library
#
# This library provides functions to detect the build environment:
# - vcpkg vs system packages
# - Platform and architecture
# - vcpkg triplet detection
# - Dependency availability

# Source guard
[[ -n "$_DWARFS_BUILD_ENV_SOURCED" ]] && return 0
_DWARFS_BUILD_ENV_SOURCED=1

# ============================================================================
# Colors
# ============================================================================

export COLOR_RED='\033[0;31m'
export COLOR_GREEN='\033[0;32m'
export COLOR_YELLOW='\033[1;33m'
export COLOR_BLUE='\033[0;34m'
export COLOR_CYAN='\033[0;36m'
export COLOR_NC='\033[0m'

# ============================================================================
# Output Functions
# ============================================================================

info() {
    echo -e "${COLOR_BLUE}[INFO]${COLOR_NC} $*"
}

success() {
    echo -e "${COLOR_GREEN}[SUCCESS]${COLOR_NC} $*"
}

warn() {
    echo -e "${COLOR_YELLOW}[WARN]${COLOR_NC} $*"
}

error() {
    echo -e "${COLOR_RED}[ERROR]${COLOR_NC} $*" >&2
}

fatal() {
    error "$@"
    exit 1
}

section() {
    echo ""
    echo -e "${COLOR_CYAN}═══════════════════════════════════════════════════════════════${COLOR_NC}"
    echo -e "${COLOR_CYAN}  $*${COLOR_NC}"
    echo -e "${COLOR_CYAN}═══════════════════════════════════════════════════════════════${COLOR_NC}"
    echo ""
}

# ============================================================================
# Platform Detection
# ============================================================================

# Detect OS (returns: darwin, linux, windows)
dwarfs_detect_os() {
    uname -s | tr '[:upper:]' '[:lower:]'
}

# Detect architecture (returns: x64, arm64)
dwarfs_detect_arch() {
    local arch=$(uname -m)
    case "$arch" in
        x86_64) echo "x64" ;;
        aarch64|arm64) echo "arm64" ;;
        *) echo "$arch" ;;
    esac
}

# Get vcpkg triplet for current platform
dwarfs_get_triplet() {
    local os=$(dwarfs_detect_os)
    local arch=$(dwarfs_detect_arch)
    local link_type="${1:-static}"  # static or dynamic

    # Map OS to vcpkg naming
    case "$os" in
        darwin) os="osx" ;;
        linux) os="linux" ;;
        msysnt|mingw*) os="windows" ;;
    esac

    # Add -dynamic suffix if needed
    if [[ "$link_type" == "dynamic" && "$os" != "windows" ]]; then
        echo "${arch}-${os}-dynamic"
    else
        echo "${arch}-${os}"
    fi
}

# ============================================================================
# vcpkg Detection
# ============================================================================

# Check if vcpkg is available
dwarfs_has_vcpkg() {
    local vcpkg_root="${1:-${VCPKG_ROOT:-$HOME/vcpkg}}"

    [[ -d "$vcpkg_root" && -x "$vcpkg_root/vcpkg" ]]
}

# Detect vcpkg root directory
dwarfs_detect_vcpkg() {
    if [[ -n "${VCPKG_ROOT:-}" && -d "$VCPKG_ROOT" ]]; then
        echo "$VCPKG_ROOT"
        return 0
    fi

    # Check common locations
    local locations=(
        "$HOME/vcpkg"
        "$HOME/.vcpkg"
        "/opt/vcpkg"
        "/usr/local/vcpkg"
    )

    for loc in "${locations[@]}"; do
        if [[ -d "$loc" && -x "$loc/vcpkg" ]]; then
            echo "$loc"
            return 0
        fi
    done

    return 1
}

# Get vcpkg version
dwarfs_vcpkg_version() {
    local vcpkg_root="$1"

    if [[ ! -x "$vcpkg_root/vcpkg" ]]; then
        return 1
    fi

    "$vcpkg_root/vcpkg" version 2>/dev/null | head -1
}

# ============================================================================
# Dependency Detection
# ============================================================================

# Check for pkg-config
dwarfs_has_pkgconfig() {
    command -v pkg-config >/dev/null 2>&1
}

# Check for jemalloc via pkg-config
dwarfs_has_jemalloc() {
    local required_version="${1:-5.5.0}"

    if ! command -v pkg-config >/dev/null 2>&1; then
        return 1
    fi

    if ! pkg-config --exists jemalloc 2>/dev/null; then
        return 1
    fi

    local version=$(pkg-config --modversion jemalloc 2>/dev/null || echo "0")
    dwarfs_compare_versions "$version" ">=" "$required_version"
}

# Compare version strings (major.minor.patch)
dwarfs_compare_versions() {
    local current="$1"
    local operator="$2"
    local required="$3"

    # Split versions into arrays
    IFS='.' read -ra current_parts <<< "$current"
    IFS='.' read -ra required_parts <<< "$required"

    # Compare each part
    for i in {0..2}; do
        local c=${current_parts[$i]:-0}
        local r=${required_parts[$i]:-0}

        if [[ "$operator" == ">=" ]]; then
            if [[ $c -gt $r ]]; then
                return 0
            elif [[ $c -lt $r ]]; then
                return 1
            fi
        fi
    done

    return 0
}

# ============================================================================
# Build Mode Detection
# ============================================================================

# Detect best build mode (vcpkg or system)
dwarfs_detect_build_mode() {
    local force_vcpkg="${1:-false}"
    local force_system="${2:-false}"
    local vcpkg_root="${3:-${VCPKG_ROOT:-$HOME/vcpkg}}"

    local has_vcpkg=false
    local has_system_packages=false

    # Check for vcpkg
    if dwarfs_has_vcpkg "$vcpkg_root"; then
        has_vcpkg=true
    fi

    # Check for system packages
    if dwarfs_has_pkgconfig; then
        # Check if jemalloc 5.5.0+ is available
        if dwarfs_has_jemalloc 5.5.0; then
            has_system_packages=true
        else
            # System packages available but jemalloc is too old
            # vcpkg is required for Tebako's jemalloc
            has_system_packages=false
        fi
    fi

    # Determine build mode
    if [[ "$force_vcpkg" == "true" ]]; then
        if [[ "$has_vcpkg" == "false" ]]; then
            fatal "Forced vcpkg mode but vcpkg not found at: $vcpkg_root"
        fi
        echo "vcpkg"
    elif [[ "$force_system" == "true" ]]; then
        if [[ "$has_system_packages" == "false" ]]; then
            fatal "Forced system mode but suitable system packages not found"
        fi
        echo "system"
    elif [[ "$has_vcpkg" == "true" ]]; then
        echo "vcpkg"
    elif [[ "$has_system_packages" == "true" ]]; then
        echo "system"
    else
        fatal "No suitable build environment found. Please install vcpkg or system packages."
    fi
}

# ============================================================================
# Environment Summary
# ============================================================================

# Print build environment summary
dwarfs_print_env_summary() {
    local build_mode="$1"
    local vcpkg_root="$2"
    local triplet="$3"

    section "Build Environment"

    echo "Build Mode: $build_mode"

    if [[ "$build_mode" == "vcpkg" ]]; then
        echo "vcpkg Root: $vcpkg_root"
        echo "Triplet: $triplet"

        local vcpkg_version=$(dwarfs_vcpkg_version "$vcpkg_root")
        if [[ -n "$vcpkg_version" ]]; then
            echo "vcpkg Version: $vcpkg_version"
        fi
    else
        if dwarfs_has_jemalloc 5.5.0; then
            local jemalloc_version=$(pkg-config --modversion jemalloc 2>/dev/null)
            echo "System jemalloc: $jemalloc_version"
        else
            warn "System jemalloc: Not found or too old (need 5.5.0+)"
        fi
    fi

    echo ""
    echo "Platform: $(dwarfs_detect_os) $(dwarfs_detect_arch)"
    echo ""
}

# Export functions for use in other scripts
export -f info success warn error fatal section
export -f dwarfs_detect_os dwarfs_detect_arch dwarfs_get_triplet
export -f dwarfs_has_vcpkg dwarfs_detect_vcpkg dwarfs_vcpkg_version
export -f dwarfs_has_pkgconfig dwarfs_has_jemalloc dwarfs_compare_versions
export -f dwarfs_detect_build_mode dwarfs_print_env_summary
