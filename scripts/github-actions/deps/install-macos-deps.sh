#!/bin/bash
# install-macos-deps.sh
#
# Install build dependencies for macOS

set -e

echo "Installing macOS build dependencies..."

# Essential build tools
brew install ninja cmake pkg-config python3

# Autoconf tools (for building vcpkg dependencies from source)
brew install autoconf autoconf-archive automake libtool

# FUSE-T - REQUIRED for macOS builds
echo "Installing FUSE-T (REQUIRED for FUSE support)..."
brew install fuse-t || {
  echo "::error::FUSE-T installation failed! FUSE-T is REQUIRED for macOS builds."
  exit 1
}
echo "✓ FUSE-T installed successfully"

echo "✓ macOS dependencies installed"
