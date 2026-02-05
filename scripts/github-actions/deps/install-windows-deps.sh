#!/bin/bash
# install-windows-deps.sh
#
# Install build dependencies for Windows

set -e

echo "Installing Windows build dependencies..."

# Install MinGW-w64 for MinGW/MSYS triplets
if [[ "${VCPKG_DEFAULT_TRIPLET:-}" == *"mingw"* ]] || [[ "${VCPKG_DEFAULT_TRIPLET:-}" == *"msys"* ]]; then
  echo "Installing MinGW-w64 via MSYS2..."
  # MSYS2 is pre-installed on GitHub Actions Windows runners
  # Update and install MinGW-w64 toolchain
  C:/msys64/usr/bin/bash -lc "pacman -Syuu --noconfirm"
  C:/msys64/usr/bin/bash -lc "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-libiconv"
  echo "✓ MinGW-w64 installed (with libiconv)"
fi

# Check if Chocolatey is installed
if ! command -v choco &> /dev/null; then
  echo "::error::Chocolatey is not installed"
  exit 1
fi

# Install WinFsp for FUSE support on Windows
echo "Installing WinFsp through Chocolatey..."
choco install winfsp -y
if [[ $? -ne 0 ]]; then
  echo "::error::Failed to install WinFsp via Chocolatey"
  exit 1
fi
echo "✓ WinFsp installed"

echo "✓ Windows dependencies installed"
