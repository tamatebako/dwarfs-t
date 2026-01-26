#!/bin/bash
# install-macos-deps.sh
#
# Install build dependencies for macOS

set -e

WITH_FUSE="${WITH_FUSE:-false}"

echo "Installing macOS build dependencies..."

# Essential build tools
brew install ninja cmake pkg-config python3

# Autoconf tools (for building vcpkg dependencies from source)
brew install autoconf autoconf-archive automake libtool

# FUSE-T for FUSE support
if [[ "$WITH_FUSE" == "true" ]]; then
  echo "Installing FUSE-T for FUSE support..."
  brew install fuse-t
fi

echo "✓ macOS dependencies installed"
