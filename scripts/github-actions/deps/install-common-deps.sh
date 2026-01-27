#!/bin/bash
# install-common-deps.sh
#
# Common dependencies for all platforms

set -e

echo "Installing common build dependencies..."

echo "Installing common Python packages from requirements.txt..."

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../../"
REQUIREMENTS_FILE="${SCRIPT_DIR}/requirements.txt"

if [[ ! -f "$REQUIREMENTS_FILE" ]]; then
  echo "::error::requirements.txt not found at $REQUIREMENTS_FILE"
  exit 1
fi

# Python packages for manpage generation
PIP_CMD=""
if command -v $PIP_CMD &>/dev/null; then
  PIP_CMD="pip3"
elif command -v pip &>/dev/null; then
  PIP_CMD="pip"
else
  echo "::error::pip3 or pip is not installed"
  exit 1
fi

$PIP_CMD install --break-system-packages -r "$REQUIREMENTS_FILE"

if [[ $? -ne 0 ]]; then
  echo "⚠️  Failed to install Python packages"
  exit 1
fi

echo "✓ Common Python packages installed"

echo "✓ Common dependencies installed"
