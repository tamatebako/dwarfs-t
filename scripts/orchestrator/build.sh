#!/bin/bash
# DwarFS Build Orchestrator
#
# This script provides the core build logic for DwarFS.
# It should be sourced by other scripts, not executed directly.
#
# Usage:
#   source scripts/lib/build_env.sh
#   source scripts/lib/vcpkg_helper.sh
#   source scripts/orchestrator/build.sh
#   dwarfs_build_configuration "config-name" [options]

# Source guard
[[ -n "$_DWARFS_BUILD_ORCHESTRATOR_SOURCED" ]] && return 0
_DWARFS_BUILD_ORCHESTRATOR_SOURCED=1

# ============================================================================
# Build Configuration Model
# ============================================================================

# Array of build configurations: "name:thrift:description"
# production: FlatBuffers-only (stable, recommended for production use)
# experimental: FlatBuffers + Modern Thrift (experimental features)
DWARFS_BUILD_CONFIGS=(
    "production:OFF:Production (FlatBuffers-only, stable)"
    "experimental:ON:Experimental (FlatBuffers + Modern Thrift)"
)

# Get build configurations
dwarfs_list_configs() {
    for config in "${DWARFS_BUILD_CONFIGS[@]}"; do
        IFS=':' read -r name thrift description <<< "$config"
        echo "$name"
    done
}

# Get CMake options for a configuration
dwarfs_config_cmake_opts() {
    local config="$1"
    local thrift=""

    # Find config and extract thrift option
    for cfg in "${DWARFS_BUILD_CONFIGS[@]}"; do
        IFS=':' read -r name thrift_val description <<< "$cfg"
        if [[ "$name" == "$config" ]]; then
            thrift="$thrift_val"
            break
        fi
    done

    if [[ -z "$thrift" ]]; then
        error "Unknown configuration: $config"
        return 1
    fi

    # Output CMake options
    echo "-DDWARFS_WITH_FLATBUFFERS=ON"
    echo "-DDWARFS_WITH_THRIFT=$thrift"
    echo "-DWITH_TESTS=ON"
    echo "-DWITH_LIBDWARFS=ON"
    echo "-DWITH_TOOLS=ON"
    echo "-DWITH_FUSE_DRIVER=OFF"
}

# Get description for a configuration
dwarfs_config_description() {
    local config="$1"

    for cfg in "${DWARFS_BUILD_CONFIGS[@]}"; do
        IFS=':' read -r name thrift description <<< "$cfg"
        if [[ "$name" == "$config" ]]; then
            echo "$description"
            return 0
        fi
    done

    echo "Unknown configuration"
    return 1
}

# ============================================================================
# Build Functions
# ============================================================================

# Build a single configuration
dwarfs_build_configuration() {
    local config="$1"
    local build_dir="${2:-build-$config}"
    local build_type="${3:-RelWithDebInfo}"
    local use_vcpkg="${4:-auto}"
    local vcpkg_root="${5:-${VCPKG_ROOT:-$HOME/vcpkg}}"
    local triplet="${6:-}"

    section "Building: $config"

    local description=$(dwarfs_config_description "$config")
    info "Configuration: $description"
    info "Build directory: $build_dir"

    # Clean build directory
    rm -rf "$build_dir"

    # Setup CMake args
    local cmake_args=(
        -B "$build_dir"
        -GNinja
        -DCMAKE_BUILD_TYPE="$build_type"
    )

    # Add configuration options
    while IFS= read -r opt; do
        cmake_args+=("$opt")
    done < <(dwarfs_config_cmake_opts "$config")

    # Determine if we should use vcpkg
    if [[ "$use_vcpkg" == "auto" ]]; then
        use_vcpkg=$(dwarfs_detect_build_mode false false "$vcpkg_root")
    fi

    # Add vcpkg toolchain if needed
    if [[ "$use_vcpkg" == "vcpkg" ]]; then
        if ! dwarfs_has_vcpkg "$vcpkg_root"; then
            fatal "vcpkg not found at: $vcpkg_root"
        fi

        # Auto-detect triplet if not specified
        if [[ -z "$triplet" ]]; then
            triplet=$(dwarfs_auto_triplet)
        fi

        # Validate triplet
        if ! dwarfs_validate_triplet "$triplet"; then
            fatal "Invalid triplet: $triplet"
        fi

        info "Using vcpkg: $vcpkg_root"
        info "Triplet: $triplet"

        # Setup overlays
        dwarfs_setup_overlays "$vcpkg_root" "$PROJECT_ROOT"

        # Verify Tebako jemalloc
        if ! dwarfs_verify_tebako_jemalloc "$PROJECT_ROOT"; then
            fatal "Tebako jemalloc verification failed"
        fi

        cmake_args+=(
            -DCMAKE_TOOLCHAIN_FILE="$vcpkg_root/scripts/buildsystems/vcpkg.cmake"
            -DVCPKG_TARGET_TRIPLET="$triplet"
        )
    else
        info "Using system packages"
    fi

    # Configure
    info "Configuring with CMake..."
    if ! cmake "${cmake_args[@]}" > "$build_dir-cmake.log" 2>&1; then
        error "CMake configuration failed"
        cat "$build_dir-cmake.log" | tail -50
        return 1
    fi

    success "CMake configuration successful"

    # Build
    info "Building..."
    local jobs=${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}

    if ! cmake --build "$build_dir" --parallel "$jobs" > "$build_dir-build.log" 2>&1; then
        error "Build failed"
        cat "$build_dir-build.log" | tail -50
        return 1
    fi

    success "Build successful"
    return 0
}

# Build all configurations
dwarfs_build_all() {
    local use_vcpkg="${1:-auto}"
    local vcpkg_root="${2:-${VCPKG_ROOT:-$HOME/vcpkg}}"
    local triplet="${3:-}"

    declare -A BUILD_RESULTS

    for config in $(dwarfs_list_configs); do
        if dwarfs_build_configuration "$config" "build-$config" "RelWithDebInfo" "$use_vcpkg" "$vcpkg_root" "$triplet"; then
            BUILD_RESULTS[$config]="PASS"
            success "$config: BUILD PASS"
        else
            BUILD_RESULTS[$config]="FAIL"
            error "$config: BUILD FAIL"
        fi
    done

    # Print summary
    section "Build Summary"

    for config in $(dwarfs_list_configs); do
        local status="${BUILD_RESULTS[$config]:-SKIP}"
        local description=$(dwarfs_config_description "$config")

        if [[ "$status" == "PASS" ]]; then
            echo -e "${COLOR_GREEN}✓${COLOR_NC} $description: $status"
        else
            echo -e "${COLOR_RED}✗${COLOR_NC} $description: $status"
        fi
    done

    # Return failure if any build failed
    for status in "${BUILD_RESULTS[@]}"; do
        if [[ "$status" == "FAIL" ]]; then
            return 1
        fi
    done

    return 0
}

# ============================================================================
# Clean Functions
# ============================================================================

# Clean build directories
dwarfs_clean_builds() {
    local pattern="${1:-build-*}"

    info "Cleaning build directories matching: $pattern"

    for dir in $pattern; do
        if [[ -d "$dir" ]]; then
            info "Removing: $dir"
            rm -rf "$dir"
        fi
    done

    success "Build directories cleaned"
}

# Clean all build artifacts
dwarfs_clean_all() {
    info "Cleaning all build artifacts..."

    # Clean build directories
    dwarfs_clean_builds "build-*"

    # Clean log files
    rm -f *-cmake.log *-build.log *-test.log

    # Clean vcpkg build cache (optional)
    # dwarfs_vcpkg_clear_cache "$VCPKG_ROOT"

    success "All build artifacts cleaned"
}

# ============================================================================
# Export Functions
# ============================================================================

export -f dwarfs_list_configs dwarfs_config_cmake_opts dwarfs_config_description
export -f dwarfs_build_configuration dwarfs_build_all
export -f dwarfs_clean_builds dwarfs_clean_all
