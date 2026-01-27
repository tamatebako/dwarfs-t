#!/bin/bash
# install-macos-deps.sh
#
# Install build dependencies for macOS

set -e

echo "Installing macOS build dependencies..."

BREW_CMD=""
if command -v brew &>/dev/null; then
  BREW_CMD="brew"
else
  echo "::error::Homebrew is not installed"
  exit 1
fi

BREWFILE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../../"
BREWFILE_PATH="$BREWFILE_DIR/Brewfile"
if [[ ! -f "$BREWFILE_PATH" ]]; then
  echo "::error::Brewfile not found at $BREWFILE_PATH"
  exit 1
fi

# Install all dependencies from Brewfile
echo "Installing Homebrew dependencies from Brewfile..."
$BREW_CMD bundle --file="$BREWFILE_PATH"
if [[ $? -ne 0 ]]; then
  echo "::error::Failed to install Homebrew dependencies"
  exit 1
fi
echo "✓ macOS dependencies installed"

# Verify FUSE-T installation
echo "Verifying FUSE-T installation..."
FUSE_T_INCLUDE_PATH="/Library/Application Support/fuse-t/include/fuse/fuse.h"
if [[ -f "$FUSE_T_INCLUDE_PATH" ]]; then
  echo "✓ FUSE-T headers found at: $FUSE_T_INCLUDE_PATH"
else
  echo "::warning::FUSE-T headers NOT found at $FUSE_T_INCLUDE_PATH"
  echo "::warning::Attempting to locate fuse.h..."
  FUSE_H=$(find /Library -name "fuse.h" 2>/dev/null | head -1)
  if [[ -n "$FUSE_H" ]]; then
    echo "✓ Found fuse.h at: $FUSE_H"
  else
    echo "::error::fuse.h not found anywhere in /Library"
  fi
fi
