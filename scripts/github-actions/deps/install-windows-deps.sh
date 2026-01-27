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
  C:/msys64/usr/bin/bash -lc "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja"
  echo "✓ MinGW-w64 installed"
fi

# Install WinFsp for FUSE support on Windows
choco install winfsp -y

# Install Python packages from requirements.txt
echo "Installing Python packages from requirements.txt..."
pip3 install -r "$(cd "${BASH_SOURCE[0]}" && pwd)/../../../requirements.txt"

echo "✓ Windows dependencies installed"
