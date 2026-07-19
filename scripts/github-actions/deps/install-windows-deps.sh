#!/bin/bash
# install-windows-deps.sh
#
# Install build dependencies for Windows

set -e

echo "Installing Windows build dependencies..."

# Install MinGW-w64 for MinGW/MSYS triplets
if [[ "${VCPKG_DEFAULT_TRIPLET:-}" == *"mingw"* ]]; then
  echo "Installing MinGW-w64 via MSYS2 (MinGW)..."
  # MSYS2 is pre-installed on GitHub Actions Windows runners
  # Update and install MinGW-w64 toolchain for MinGW triplet
  C:/msys64/usr/bin/bash -lc "pacman -Syuu --noconfirm"
  C:/msys64/usr/bin/bash -lc "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-libiconv"
  echo "✓ MinGW-w64 installed (with libiconv) for MinGW triplet"
elif [[ "${VCPKG_DEFAULT_TRIPLET:-}" == *"msys"* ]]; then
  echo "Installing dependencies via MSYS2 (MSYS)..."
  # For MSYS triplet, vcpkg uses mingw-w64 compiler
  # We need mingw-w64 libraries (NOT MSYS-native) for compatibility
  C:/msys64/usr/bin/bash -lc "pacman -Syuu --noconfirm"
  # Install mingw-w64 toolchain and libiconv
  C:/msys64/usr/bin/bash -lc "pacman -S --noconfirm --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-libiconv"
  echo "✓ MSYS dependencies installed (with MinGW-w64 libiconv for vcpkg compatibility)"
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
