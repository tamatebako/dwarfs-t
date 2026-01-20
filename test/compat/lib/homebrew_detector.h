/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     DwarFS Implementation
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>
#include <optional>
#include <cstdint>

namespace dwarfs::test::compat {

/**
 * \brief Information about a Homebrew installation
 *
 * This struct contains detected information about the Homebrew
 * installation and the dwarfs tool.
 */
struct HomebrewInfo {
  /// Whether Homebrew is installed
  bool installed = false;

  /// Path to mkdwarfs binary (empty if not found)
  std::string mkdwarfs_path;

  /// Path to dwarfs binary (empty if not found)
  std::string dwarfs_path;

  /// Detected dwarfs version (e.g., "0.14.1")
  std::string version;

  /// Platform: "darwin" for macOS, "linux" for Linux
  std::string platform;

  /// Architecture: "arm64" or "x86_64"
  std::string arch;

  /// Homebrew installation prefix
  std::string prefix;

  /// Check if all required binaries are available
  /// Note: dwarfs binary (FUSE reader) is Linux-only and not required for macOS
  bool is_complete() const {
    return installed && !mkdwarfs_path.empty();
  }

  /// Get platform-arch string for fixture naming
  std::string platform_arch() const {
    return platform + "-" + arch;
  }
};

/**
 * \brief Detects Homebrew installation and dwarfs tool
 *
 * This class provides methods to detect the Homebrew installation
 * and the dwarfs tool on the current system.
 *
 * Supported platforms:
 * - macOS (arm64 and x86_64)
 * - Linux (Linuxbrew, arm64 and x86_64)
 */
class HomebrewDetector {
public:
  HomebrewDetector() = default;
  ~HomebrewDetector() = default;

  /**
   * \brief Detect Homebrew installation
   *
   * \return HomebrewInfo with detected information
   *
   * This method detects:
   * - Platform (macOS/Linux)
   * - Architecture (arm64/x86_64)
   * - Homebrew installation path
   * - mkdwarfs and dwarfs binary paths
   * - dwarfs version
   */
  HomebrewInfo detect() const;

  /**
   * \brief Find a specific dwarfs version
   *
   * \param version Version to find (e.g., "0.14.1" or "latest")
   * \return Path to mkdwarfs for the requested version, or empty if not found
   */
  std::string find_dwarfs_version(const std::string& version) const;

  /**
   * \brief Get current platform
   *
   * \return "darwin" for macOS, "linux" for Linux
   */
  static std::string get_platform();

  /**
   * \brief Get current architecture
   *
   * \return "arm64" or "x86_64"
   */
  static std::string get_arch();

  /**
   * \brief Check if Homebrew is installed
   *
   * \return true if Homebrew is found
   */
  static bool is_homebrew_installed();

  /**
   * \brief Get Homebrew installation prefix
   *
   * \return Homebrew prefix path, or empty if not found
   */
  static std::string get_homebrew_prefix();

private:
  /**
   * \brief Detect platform using uname
   */
  static std::string detect_platform();

  /**
   * \brief Detect architecture using uname
   */
  static std::string detect_arch();

  /**
   * \brief Find mkdwarfs in a Homebrew prefix
   */
  static std::string find_mkdwarfs_in_prefix(const std::string& prefix);

  /**
   * \brief Find dwarfs in a Homebrew prefix
   */
  static std::string find_dwarfs_in_prefix(const std::string& prefix);

  /**
   * \brief Get dwarfs version by running mkdwarfs --version
   */
  static std::string get_dwarfs_version_from_binary(const std::string& mkdwarfs_path);

  /**
   * \brief Check if a binary exists and is executable
   */
  static bool binary_exists(const std::string& path);
};

} // namespace dwarfs::test::compat
