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

# Python packages for manpage generation
echo "Installing Python packages from requirements.txt..."
pip3 install -r "$(cd "${BASH_SOURCE[0]}" && pwd)/../../../requirements.txt"

# FUSE-T - REQUIRED for macOS builds
echo "Installing FUSE-T (REQUIRED for FUSE support)..."
brew install fuse-t || {
  echo "::error::FUSE-T installation failed! FUSE-T is REQUIRED for macOS builds."
  exit 1
}

# Verify FUSE-T installation
echo "Verifying FUSE-T installation..."
if [[ -d "/Library/Application Support/fuse-t/include/fuse" ]]; then
  echo "✓ FUSE-T headers found at: /Library/Application Support/fuse-t/include/fuse"
  ls -la "/Library/Application Support/fuse-t/include/fuse/" | head -10
else
  echo "::error::FUSE-T headers NOT found at /Library/Application Support/fuse-t/include/fuse after installation!"
  echo "Listing /Library/Application Support/fuse-t/:"
  ls -la "/Library/Application Support/fuse-t/" || echo "Directory does not exist"
  exit 1
fi

echo "✓ FUSE-T installed successfully"
echo "✓ macOS dependencies installed"
