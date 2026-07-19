#!/bin/bash

# DwarFS Benchmark Files Download Script
# Downloads benchmark and test files from GitHub releases

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BENCHMARK_DIR="$PROJECT_ROOT/benchmarks/benchmark-files"
TEST_IMAGES_DIR="$PROJECT_ROOT/test-images"

# GitHub release configuration
# These can be overridden via environment variables
GITHUB_REPO="${GITHUB_REPO:-mhogomchungu/dwarfs}"
RELEASE_TAG="${RELEASE_TAG:-benchmark-files-v1}"

# Download URLs
GITHUB_RELEASE_URL="https://github.com/${GITHUB_REPO}/releases/download/${RELEASE_TAG}"

# Files to download
declare -A BENCHMARK_FILES=(
    ["perl-5.43.3.tar.xz"]="$BENCHMARK_DIR/perl-5.43.3.tar.xz"
)

declare -A TEST_IMAGE_FILES=(
    ["perl-test.dff"]="$TEST_IMAGES_DIR/perl-test.dff"
    ["perl-test.dft"]="$TEST_IMAGES_DIR/perl-test.dft"
)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

usage() {
    cat << EOF
Usage: $0 [OPTIONS] [TARGET]

Download DwarFS benchmark and test files from GitHub releases.

TARGETS:
    all           Download all files (default)
    benchmark     Download benchmark files only (perl source)
    test-images   Download test images only (.dff, .dft files)

OPTIONS:
    -f, --force   Force re-download even if files exist
    -l, --list    List available files and their sizes
    -h, --help    Show this help message

ENVIRONMENT VARIABLES:
    GITHUB_REPO   GitHub repository (default: mhogomchungu/dwarfs)
    RELEASE_TAG   Release tag to download from (default: benchmark-files-v1)

EXAMPLES:
    $0                    # Download all files
    $0 benchmark          # Download benchmark files only
    $0 test-images        # Download test images only
    $0 --force all        # Force re-download all files
    GITHUB_REPO=user/repo $0  # Use custom repository
EOF
}

list_files() {
    echo "Benchmark Files:"
    echo "  perl-5.43.3.tar.xz (~23 MB) - Perl 5.43.3 source tarball"
    echo ""
    echo "Test Images:"
    echo "  perl-test.dff (~21 MB) - DwarFS FlatBuffers format test image"
    echo "  perl-test.dft (~21 MB) - DwarFS Thrift format test image"
    echo ""
    echo "Total download size: ~65 MB"
}

download_file() {
    local filename="$1"
    local dest_path="$2"
    local url="${GITHUB_RELEASE_URL}/${filename}"

    local dest_dir
    dest_dir=$(dirname "$dest_path")

    # Create destination directory
    mkdir -p "$dest_dir"

    # Check if file exists
    if [ -f "$dest_path" ]; then
        if [ "$FORCE_DOWNLOAD" = true ]; then
            log_warn "File exists, forcing re-download: $dest_path"
            rm -f "$dest_path"
        else
            log_info "File already exists, skipping: $dest_path"
            return 0
        fi
    fi

    log_info "Downloading: $filename"
    log_info "  From: $url"
    log_info "  To: $dest_path"

    # Download with curl
    if command -v curl &> /dev/null; then
        curl --fail --progress-bar -L -o "$dest_path" "$url"
    elif command -v wget &> /dev/null; then
        wget -q --show-progress -O "$dest_path" "$url"
    else
        log_error "Neither curl nor wget is available. Please install one of them."
        return 1
    fi

    if [ $? -eq 0 ]; then
        log_info "Successfully downloaded: $filename"
    else
        log_error "Failed to download: $filename"
        rm -f "$dest_path"
        return 1
    fi
}

extract_benchmark() {
    local archive="$BENCHMARK_DIR/perl-5.43.3.tar.xz"
    local dest_dir="$BENCHMARK_DIR"

    if [ ! -f "$archive" ]; then
        log_warn "Benchmark archive not found: $archive"
        return 1
    fi

    log_info "Extracting benchmark files..."

    # Check if already extracted
    if [ -d "$dest_dir/perl-5.43.3" ]; then
        if [ "$FORCE_DOWNLOAD" = true ]; then
            log_warn "Removing existing extracted directory"
            rm -rf "$dest_dir/perl-5.43.3"
        else
            log_info "Benchmark files already extracted"
            return 0
        fi
    fi

    # Extract
    tar -xf "$archive" -C "$dest_dir"
    log_info "Successfully extracted benchmark files"
}

download_benchmark_files() {
    log_info "Downloading benchmark files..."

    for filename in "${!BENCHMARK_FILES[@]}"; do
        local dest_path="${BENCHMARK_FILES[$filename]}"
        download_file "$filename" "$dest_path" || return 1
    done

    extract_benchmark || return 1
}

download_test_images() {
    log_info "Downloading test images..."

    for filename in "${!TEST_IMAGE_FILES[@]}"; do
        local dest_path="${TEST_IMAGE_FILES[$filename]}"
        download_file "$filename" "$dest_path" || return 1
    done
}

# Parse command-line options
FORCE_DOWNLOAD=false
TARGET="all"

while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--force)
            FORCE_DOWNLOAD=true
            shift
            ;;
        -l|--list)
            list_files
            exit 0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        all|benchmark|test-images)
            TARGET="$1"
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

echo "=== DwarFS Benchmark Files Download ==="
echo "Repository: $GITHUB_REPO"
echo "Release: $RELEASE_TAG"
echo "Target: $TARGET"
echo "Force: $FORCE_DOWNLOAD"
echo

case "$TARGET" in
    all)
        download_benchmark_files || exit 1
        download_test_images || exit 1
        ;;
    benchmark)
        download_benchmark_files || exit 1
        ;;
    test-images)
        download_test_images || exit 1
        ;;
esac

log_info "Download complete!"
