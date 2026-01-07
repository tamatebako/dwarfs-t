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

#include "homebrew_detector.h"
#include "homebrew_detector.h"

#include <cstdlib>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace dwarfs::test::compat {

namespace {

// Known Homebrew installation paths
constexpr const char* HOMEBREW_PATHS[] = {
  // macOS arm64 (default)
  "/opt/homebrew",
  // macOS x86_64 (default)
  "/usr/local",
  // Linuxbrew (user-local)
  "/home/linuxbrew/.linuxbrew",
  // Linuxbrew (system-wide)
  "/home/linuxbrew",
  "~/.linuxbrew",
};

// Binary names
constexpr const char* MKDWARFS = "mkdwarfs";
constexpr const char* DWARFS = "dwarfs";

} // anonymous namespace

std::string HomebrewDetector::detect_platform() {
  struct utsname uts;
  if (uname(&uts) != 0) {
    return "unknown";
  }

  std::string sysname(uts.sysname);
  // Normalize to lower case
  std::transform(sysname.begin(), sysname.end(), sysname.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (sysname == "darwin") {
    return "darwin";
  } else if (sysname == "linux") {
    return "linux";
  }

  return "unknown";
}

std::string HomebrewDetector::detect_arch() {
  struct utsname uts;
  if (uname(&uts) != 0) {
    return "unknown";
  }

  std::string machine(uts.machine);
  // Normalize architecture names
  if (machine == "x86_64" || machine == "amd64") {
    return "x86_64";
  } else if (machine == "arm64" || machine == "aarch64") {
    return "arm64";
  }

  return machine;
}

std::string HomebrewDetector::find_mkdwarfs_in_prefix(const std::string& prefix) {
  // First try the standard bin directory
  std::string path = prefix + "/bin/" + MKDWARFS;
  if (binary_exists(path)) {
    return path;
  }

  // For keg-only packages, search in Cellar
  std::string cellar_path = prefix + "/Cellar/dwarfs";
  struct stat buffer;
  if (stat(cellar_path.c_str(), &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
    // Find the versioned directory
    DIR* dir = opendir(cellar_path.c_str());
    if (dir) {
      struct dirent* entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.') {
          std::string versioned_path = cellar_path + "/" + entry->d_name + "/bin/" + MKDWARFS;
          if (binary_exists(versioned_path)) {
            closedir(dir);
            return versioned_path;
          }
        }
      }
      closedir(dir);
    }
  }

  return "";
}

std::string HomebrewDetector::find_dwarfs_in_prefix(const std::string& prefix) {
  // First try the standard bin directory
  std::string path = prefix + "/bin/" + DWARFS;
  if (binary_exists(path)) {
    return path;
  }

  // For keg-only packages, search in Cellar
  std::string cellar_path = prefix + "/Cellar/dwarfs";
  struct stat buffer;
  if (stat(cellar_path.c_str(), &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
    // Find the versioned directory
    DIR* dir = opendir(cellar_path.c_str());
    if (dir) {
      struct dirent* entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] != '.') {
          std::string versioned_path = cellar_path + "/" + entry->d_name + "/bin/" + DWARFS;
          if (binary_exists(versioned_path)) {
            closedir(dir);
            return versioned_path;
          }
        }
      }
      closedir(dir);
    }
  }

  return "";
}

std::string HomebrewDetector::get_dwarfs_version_from_binary(const std::string& mkdwarfs_path) {
  // Run mkdwarfs --version to get version
  std::string cmd = mkdwarfs_path + " --version 2>&1";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return "";
  }

  char buffer[128];
  std::string result;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  pclose(pipe);

  // Parse version from output
  // Expected format: "mkdwarfs 0.14.1" or similar
  std::istringstream iss(result);
  std::string token;
  while (iss >> token) {
    if (token[0] >= '0' && token[0] <= '9') {
      // Version number found
      return token;
    }
  }

  return "unknown";
}

bool HomebrewDetector::binary_exists(const std::string& path) {
  struct stat buffer;
  return (stat(path.c_str(), &buffer) == 0) &&
         (buffer.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
}

// Static methods

std::string HomebrewDetector::get_platform() {
  return detect_platform();
}

std::string HomebrewDetector::get_arch() {
  return detect_arch();
}

bool HomebrewDetector::is_homebrew_installed() {
  std::string prefix = get_homebrew_prefix();
  return !prefix.empty();
}

std::string HomebrewDetector::get_homebrew_prefix() {
  // Try known Homebrew paths
  for (const char* path : HOMEBREW_PATHS) {
    // Expand ~ if present
    std::string expanded_path = path;
    if (expanded_path[0] == '~') {
      const char* home = getenv("HOME");
      if (home) {
        expanded_path.replace(0, 1, home);
      }
    }

    // Check if bin directory exists
    std::string bin_path = expanded_path + "/bin";
    struct stat buffer;
    if (stat(bin_path.c_str(), &buffer) == 0 && S_ISDIR(buffer.st_mode)) {
      return expanded_path;
    }
  }
  return "";
}

// Instance methods

HomebrewInfo HomebrewDetector::detect() const {
  HomebrewInfo info;

  info.platform = get_platform();
  info.arch = get_arch();

  if (info.platform == "unknown" || info.arch == "unknown") {
    return info;
  }

  info.prefix = get_homebrew_prefix();
  if (info.prefix.empty()) {
    return info;
  }

  info.installed = true;
  info.mkdwarfs_path = find_mkdwarfs_in_prefix(info.prefix);
  info.dwarfs_path = find_dwarfs_in_prefix(info.prefix);

  if (!info.mkdwarfs_path.empty()) {
    info.version = get_dwarfs_version_from_binary(info.mkdwarfs_path);
  }

  return info;
}

std::string HomebrewDetector::find_dwarfs_version(const std::string& version) const {
  HomebrewInfo info = detect();

  if (!info.installed) {
    return "";
  }

  // If asking for "latest" or the installed version matches
  if (version == "latest" || version == info.version) {
    return info.mkdwarfs_path;
  }

  // For specific versions, we'd need to check if that version is installed
  // via brew (e.g., brew list --versions | grep dwarfs)
  // For now, return the current mkdwarfs path
  return info.mkdwarfs_path;
}

} // namespace dwarfs::test::compat
