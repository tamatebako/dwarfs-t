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
echo "✓ Homebrew dependencies installed"

echo "✓ macOS dependencies installed"
