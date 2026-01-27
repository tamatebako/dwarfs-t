# Brewfile for DwarFS macOS CI dependencies
# Install with: brew bundle --file=Brewfile

# Essential build tools
brew "ninja"
brew "cmake"
brew "pkg-config"
brew "python3", break_system_packages: true

# Autoconf tools (for building vcpkg dependencies from source)
brew "autoconf"
brew "autoconf-archive"
brew "automake"
brew "libtool"

# FUSE-T - REQUIRED for macOS builds and tests
tap "macos-fuse-t/homebrew-cask"
cask "macos-fuse-t/homebrew-cask/fuse-t"
cask "macos-fuse-t/homebrew-cask/fuse-t-sshfs"
