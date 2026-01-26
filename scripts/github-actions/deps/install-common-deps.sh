#!/bin/bash
# install-common-deps.sh
#
# Common dependencies for all platforms

set -e

echo "Installing common build dependencies..."

# Python packages for manpage generation
if command -v pip3 &>/dev/null; then
  pip3 install -r "${SCRIPT_DIR}/requirements.txt" 2>/dev/null || pip3 install mistletoe || echo "⚠️  Failed to install Python packages"
elif command -v pip &>/dev/null; then
  pip install -r "${SCRIPT_DIR}/requirements.txt" 2>/dev/null || pip install mistletoe || echo "⚠️  Failed to install Python packages"
fi

echo "✓ Common dependencies installed"
