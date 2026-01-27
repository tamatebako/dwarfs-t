#!/bin/bash
# install-macos-deps.sh
#
# Install build dependencies for macOS

set -e

echo "Installing macOS build dependencies..."

# Install all dependencies from Brewfile
brew bundle --file="$(cd "${BASH_SOURCE[0]}" && pwd)/Brewfile"

# Python packages for manpage generation
echo "Installing Python packages from requirements.txt..."
pip3 install -r "$(cd "${BASH_SOURCE[0]}" && pwd)/../../../requirements.txt"

echo "✓ macOS dependencies installed"
