#!/bin/bash
# install-linux-deps.sh
#
# Install build dependencies for Linux
# Supports: apt (Ubuntu/Debian), yum (RHEL/CentOS), apk (Alpine)

set -e

# Common packages across all package managers
COMMON_PACKAGES="
  git
  ninja-build
  pkg-config
  autoconf
  autoconf-archive
  automake
  libtool
"

FUSE_PACKAGES="fuse3"

echo "Installing Linux build dependencies..."

# Detect package manager
if command -v apt-get &> /dev/null; then
  PKG_MANAGER="apt"
elif command -v yum &> /dev/null; then
  PKG_MANAGER="yum"
elif command -v apk &> /dev/null; then
  PKG_MANAGER="apk"
else
  echo "::warning::Unable to detect package manager"
  exit 0
fi

case "$PKG_MANAGER" in
  apt)
    sudo apt-get update
    sudo apt-get install -y build-essential $COMMON_PACKAGES python3-pip
    if [[ -n "$FUSE_PACKAGES" ]]; then
      sudo apt-get install -y $FUSE_PACKAGES
    fi
    ;;

  yum)
    sudo yum install -y gcc-c++ $COMMON_PACKAGES python3-pip
    if [[ -n "$FUSE_PACKAGES" ]]; then
      sudo yum install -y ${FUSE_PACKAGES%-devel}-devel
    fi
    ;;

  apk)
    apk add --no-cache build-base $COMMON_PACKAGES py3-pip
    if [[ -n "$FUSE_PACKAGES" ]]; then
      apk add --no-cache ${FUSE_PACKAGES%-dev}-dev
    fi
    ;;
esac

# Install Python packages from requirements.txt
echo "Installing Python packages from requirements.txt..."
pip3 install -r "$(cd "${BASH_SOURCE[0]}" && pwd)/../../../requirements.txt"

echo "✓ Linux dependencies installed"
