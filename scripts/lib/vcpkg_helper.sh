#!/bin/bash
# DwarFS vcpkg Helper Library
#
# This library provides functions for working with vcpkg:
# - Triplet detection and validation
# - Overlay port management
# - Cache management
# - vcpkg command execution

# Source guard
[[ -n "$_DWARFS_VCPKG_HELPER_SOURCED" ]] && return 0
_DWARFS_VCPKG_HELPER_SOURCED=1

# ============================================================================
# Triplet Management
# ============================================================================

# List all supported triplets for DwarFS
dwarfs_list_triplets() {
    cat <<'EOF'
# macOS triplets
arm64-osx
x64-osx
arm64-osx-dynamic
x64-osx-dynamic

# Linux triplets
arm64-linux
x64-linux
arm64-linux-dynamic
x64-linux-dynamic

# Windows triplets (MSVC)
arm64-windows-static
x64-windows-static
arm64-windows-dynamic
x64-windows-dynamic

# Windows triplets (MinGW)
arm64-mingw-static
x64-mingw-static
EOF
}

# Validate triplet
dwarfs_validate_triplet() {
    local triplet="$1"

    # List valid triplets
    local valid_triplets=(
        "arm64-osx" "x64-osx" "arm64-osx-dynamic" "x64-osx-dynamic"
        "arm64-linux" "x64-linux" "arm64-linux-dynamic" "x64-linux-dynamic"
        "arm64-windows-static" "x64-windows-static"
        "arm64-windows-dynamic" "x64-windows-dynamic"
        "arm64-mingw-static" "x64-mingw-static"
    )

    for valid in "${valid_triplets[@]}"; do
        if [[ "$triplet" == "$valid" ]]; then
            return 0
        fi
    done

    return 1
}

# Get triplet for current platform
dwarfs_auto_triplet() {
    local os=$(uname -s | tr '[:upper:]' '[:lower:]')
    local arch=$(uname -m)

    # Map architecture
    case "$arch" in
        x86_64) arch="x64" ;;
        aarch64|arm64) arch="arm64" ;;
    esac

    # Map OS
    case "$os" in
        darwin) os="osx" ;;
        linux) os="linux" ;;
    esac

    echo "${arch}-${os}"
}

# Get all triplets for a platform
dwarfs_triplets_for_platform() {
    local platform="$1"  # osx, linux, windows

    case "$platform" in
        osx)
            echo "arm64-osx x64-osx arm64-osx-dynamic x64-osx-dynamic"
            ;;
        linux)
            echo "arm64-linux x64-linux arm64-linux-dynamic x64-linux-dynamic"
            ;;
        windows)
            echo "arm64-windows-static x64-windows-static arm64-windows-dynamic x64-windows-dynamic arm64-mingw-static x64-mingw-static"
            ;;
        *)
            echo ""
            ;;
    esac
}

# ============================================================================
# vcpkg Command Execution
# ============================================================================

# Run vcpkg command with proper error handling
dwarfs_vcpkg() {
    local vcpkg_root="$1"
    shift

    if [[ ! -x "$vcpkg_root/vcpkg" ]]; then
        error "vcpkg not found or not executable: $vcpkg_root/vcpkg"
        return 1
    fi

    "$vcpkg_root/vcpkg" "$@"
    return $?
}

# Install vcpkg package
dwarfs_vcpkg_install() {
    local vcpkg_root="$1"
    local package="$2"
    local triplet="${3:-}"

    local cmd=(install "$package")

    if [[ -n "$triplet" ]]; then
        cmd+=(--triplet="$triplet")
    fi

    dwarfs_vcpkg "$vcpkg_root" "${cmd[@]}"
}

# ============================================================================
# Overlay Ports
# ============================================================================

# Setup overlay ports for DwarFS
dwarfs_setup_overlays() {
    local vcpkg_root="$1"
    local project_root="$2"

    local overlay_dir="$project_root/vcpkg_ports"

    if [[ ! -d "$overlay_dir" ]]; then
        warn "No overlay ports directory found: $overlay_dir"
        return 0
    fi

    # Export for vcpkg to use
    export VCPKG_OVERLAY_PORTS="$overlay_dir${VCPKG_OVERLAY_PORTS:+:$VCPKG_OVERLAY_PORTS}"

    info "Using overlay ports: $overlay_dir"
}

# List overlay ports
dwarfs_list_overlay_ports() {
    local project_root="$1"
    local overlay_dir="$project_root/vcpkg_ports"

    if [[ ! -d "$overlay_dir" ]]; then
        return 0
    fi

    find "$overlay_dir" -maxdepth 1 -type d ! -name "$overlay_dir" -exec basename {} \;
}

# ============================================================================
# Cache Management
# ============================================================================

# Get vcpkg cache directory
dwarfs_vcpkg_cache_dir() {
    local vcpkg_root="$1"

    echo "$vcpkg_root/archives"
}

# Clear vcpkg cache
dwarfs_vcpkg_clear_cache() {
    local vcpkg_root="$1"

    local cache_dir=$(dwarfs_vcpkg_cache_dir "$vcpkg_root")

    if [[ -d "$cache_dir" ]]; then
        info "Clearing vcpkg cache: $cache_dir"
        rm -rf "$cache_dir"/*
        success "vcpkg cache cleared"
    else
        warn "vcpkg cache directory not found: $cache_dir"
    fi
}

# Get vcpkg cache size
dwarfs_vcpkg_cache_size() {
    local vcpkg_root="$1"

    local cache_dir=$(dwarfs_vcpkg_cache_dir "$vcpkg_root")

    if [[ -d "$cache_dir" ]]; then
        du -sh "$cache_dir" 2>/dev/null | cut -f1
    else
        echo "N/A"
    fi
}

# ============================================================================
# Binary Cache
# ============================================================================

# Setup vcpkg binary cache (for CI)
dwarfs_setup_binary_cache() {
    local cache_dir="$1"

    export VCPKG_BINARY_SOURCES="default,readwrite"
    export X_VCPKG_ASSET_SOURCES="$cache_dir"

    info "Using vcpkg binary cache: $cache_dir"
}

# ============================================================================
# Tebako jemalloc Detection
# ============================================================================

# Check if Tebako's jemalloc overlay port is available
dwarfs_has_tebako_jemalloc() {
    local project_root="$1"
    local overlay_port="$project_root/vcpkg_ports/jemalloc"

    [[ -f "$overlay_port/vcpkg.json" && -f "$overlay_port/portfile.cmake" ]]
}

# Get Tebako jemalloc version from overlay port
dwarfs_tebako_jemalloc_version() {
    local project_root="$1"
    local vcpkg_json="$project_root/vcpkg_ports/jemalloc/vcpkg.json"

    if [[ ! -f "$vcpkg_json" ]]; then
        return 1
    fi

    # Extract version from vcpkg.json
    grep -o '"version"[[:space:]]*:[[:space:]]*"[^"]*"' "$vcpkg_json" | \
        cut -d'"' -f4 | \
        head -1
}

# Verify Tebako jemalloc is configured correctly
dwarfs_verify_tebako_jemalloc() {
    local project_root="$1"

    if ! dwarfs_has_tebako_jemalloc "$project_root"; then
        error "Tebako jemalloc overlay port not found!"
        error "Expected: $project_root/vcpkg_ports/jemalloc/"
        return 1
    fi

    local version=$(dwarfs_tebako_jemalloc_version "$project_root")

    if [[ "$version" != "5.5.0" ]]; then
        error "Tebako jemalloc version mismatch: $version (expected 5.5.0)"
        return 1
    fi

    # Check that portfile uses Tebako's fork
    local portfile="$project_root/vcpkg_ports/jemalloc/portfile.cmake"

    if ! grep -q "tamatebako/jemalloc" "$portfile"; then
        error "portfile.cmake doesn't use Tebako's jemalloc fork!"
        return 1
    fi

    success "Tebako jemalloc 5.5.0 overlay port verified"
    return 0
}

# ============================================================================
# Export Functions
# ============================================================================

export -f dwarfs_list_triplets dwarfs_validate_triplet dwarfs_auto_triplet
export -f dwarfs_triplets_for_platform dwarfs_vcpkg dwarfs_vcpkg_install
export -f dwarfs_setup_overlays dwarfs_list_overlay_ports
export -f dwarfs_vcpkg_cache_dir dwarfs_vcpkg_clear_cache dwarfs_vcpkg_cache_size
export -f dwarfs_setup_binary_cache
export -f dwarfs_has_tebako_jemalloc dwarfs_tebako_jemalloc_version dwarfs_verify_tebako_jemalloc
