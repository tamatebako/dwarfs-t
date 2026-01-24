#!/bin/bash
# install-build-deps.sh
#
# Install minimal build dependencies for DwarFS CI
# Since we use vcpkg for ALL dependencies, this only installs:
# 1. Essential build tools (cmake, ninja, git, pkg-config for vcpkg's use)
# 2. FUSE libraries (for tests, when WITH_FUSE=true)
# 3. Autoconf tools (for building vcpkg dependencies from source if needed)
#
# Usage:
#   scripts/github-actions/install-build-deps.sh [--with-fuse]
#
# Environment variables:
#   WITH_FUSE - Set to 'true' to install FUSE libraries (default: false)

set -e

WITH_FUSE="${WITH_FUSE:-false}"

echo "Installing build dependencies for $RUNNER_OS..."

case "$RUNNER_OS" in
  Windows)
    echo "Installing Windows build dependencies..."

    if [[ "$WITH_FUSE" == "true" ]]; then
      choco install winfsp -y
    fi

    echo "✓ Windows dependencies installed"
    ;;

  macOS)
    echo "Installing macOS build dependencies..."

    # Essential build tools
    brew install ninja cmake pkg-config

    # Autoconf tools (for building vcpkg dependencies from source)
    brew install autoconf autoconf-archive automake libtool

    # FUSE support for tests
    if [[ "$WITH_FUSE" == "true" ]]; then
      if ! brew list macfuse 2>/dev/null && ! brew list fuse-t 2>/dev/null; then
        echo "::warning::No FUSE package found. Install macFUSE or FUSE-T manually."
      fi
    fi

    echo "✓ macOS dependencies installed"
    ;;

  Linux)
    echo "Installing Linux build dependencies..."

    # Detect package manager
    if command -v apt-get &> /dev/null; then
      PKG_MANAGER="apt"
    elif command -v yum &> /dev/null; then
      PKG_MANAGER="yum"
    elif command -v apk &> /dev/null; then
      PKG_MANAGER="apk"
    else
      echo "::warning::Unable to detect package manager"
      exit 0
    fi

    case "$PKG_MANAGER" in
      apt)
        sudo apt-get update
        sudo apt-get install -y \
          build-essential \
          git \
          ninja-build \
          pkg-config \
          autoconf \
          autoconf-archive \
          automake \
          libtool

        if [[ "$WITH_FUSE" == "true" ]]; then
          sudo apt-get install -y libfuse3-dev fuse3
        fi
        ;;

      yum)
        sudo yum install -y \
          gcc-c++ \
          git \
          ninja-build \
          pkgconfig \
          autoconf \
          autoconf-archive \
          automake \
          libtool

        if [[ "$WITH_FUSE" == "true" ]]; then
          sudo yum install -y fuse3-devel
        fi
        ;;

      apk)
        apk add --no-cache \
          build-base \
          git \
          ninja \
          pkgconf \
          autoconf \
          autoconf-archive \
          automake \
          libtool

        if [[ "$WITH_FUSE" == "true" ]]; then
          apk add --no-cache fuse3-dev fuse3
        fi
        ;;
    esac

    echo "✓ Linux dependencies installed"
    ;;
esac

# Create FUSE mount directory if requested
if [[ "$WITH_FUSE" == "true" ]]; then
  sudo mkdir -p /tmp/dwarfs-test-mount
  sudo chmod 777 /tmp/dwarfs-test-mount
  echo "✓ Created FUSE mount directory: /tmp/dwarfs-test-mount"
  echo "DWARFS_TEST_MOUNT_DIR=/tmp/dwarfs-test-mount" >> $GITHUB_ENV
fi

echo "✓ Build dependencies installation complete"
