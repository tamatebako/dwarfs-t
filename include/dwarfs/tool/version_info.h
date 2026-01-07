//
// Copyright (c) Marcus Holland-Moritz
//
// This file is part of dwarfs.
//
// dwarfs is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// dwarfs is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
// A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// dwarfs.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace dwarfs::tool {

/**
 * @brief Version information for DwarFS tools
 *
 * Provides comprehensive version, build, and dependency information
 * for display via --version option.
 */
struct version_info {
  // Version information
  std::string version;      ///< Full version string (e.g., "0.16.0-dev-a1b2c3d")
  std::string version_short; ///< Short version (e.g., "0.16.0")
  std::string commit_hash;  ///< Git commit hash (short)
  std::string commit_date;  ///< Git commit date
  std::string git_branch;   ///< Git branch name
  bool is_release;          ///< True if built from release tag

  // Build information
  std::string build_id;     ///< Build identifier (arch + OS + compiler)
  std::string compiler;     ///< Compiler name and version
  std::string build_date;   ///< Build timestamp
  std::string build_type;   ///< Debug, Release, etc.

  // Platform information
  std::string platform;     ///< Platform string (e.g., "Linux x86_64")
  std::string architecture; ///< CPU architecture

  // Feature flags
  std::vector<std::string> features; ///< Enabled features (FlatBuffers, FLAC, etc.)

  // Library versions
  std::map<std::string, std::string> libraries; ///< Dependency name -> version

  /**
   * @brief Get current version information
   * @return Populated version_info structure
   */
  static version_info get();

  /**
   * @brief Format version info as string for display
   * @param tool_name Name of the tool (e.g., "mkdwarfs")
   * @param include_features Include feature list in output
   * @param include_libraries Include library versions in output
   * @return Formatted version string
   */
  std::string to_string(std::string_view tool_name,
                       bool include_features = true,
                       bool include_libraries = true) const;

  /**
   * @brief Get short version string (just version number)
   * @return Version number only (e.g., "0.16.0")
   */
  std::string short_string() const;

private:
  static std::string get_compiler_version();
  static std::string get_platform_string();
  static std::vector<std::string> get_feature_list();
  static std::map<std::string, std::string> get_library_versions();
};

} // namespace dwarfs::tool