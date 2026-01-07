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
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>
#include "dwarfs/metadata/domain/metadata.h"
#include "homebrew_detector.h"

namespace dwarfs::test::compat {

// Namespace alias for convenience
namespace metadata_domain = dwarfs::metadata::domain;

/**
 * \brief Specification for generating a test fixture
 *
 * This struct describes all the parameters needed to generate
 * a test fixture DFT file.
 */
struct FixtureSpec {
  /// dwarfs version (e.g., "0.14.1", "latest")
  std::string dwarfs_version;

  /// Platform ("darwin" or "linux")
  std::string platform;

  /// Architecture ("arm64" or "x86_64")
  std::string arch;

  /// Test metadata to serialize
metadata_domain::metadata test_metadata;

  /// Output directory for fixtures
  std::string output_dir;

  /// Get the fixture filename for this spec
  std::string get_filename() const {
    return "dwarfs-v" + dwarfs_version + "-" + platform + "-" + arch + ".dft";
  }

  /// Get the full fixture path for this spec
  std::string get_fixture_path() const {
    return output_dir + "/" + get_filename();
  }

  /// Get the platform-arch string
  std::string platform_arch() const {
    return platform + "-" + arch;
  }
};

/**
 * \brief Generates test fixture DFT files using Homebrew mkdwarfs
 *
 * This class handles the generation of test fixture files using the
 * Homebrew mkdwarfs tool. It creates test metadata, invokes mkdwarfs,
 * and stores the resulting DFT file with the correct naming convention.
 */
class FixtureGenerator {
public:
  /**
   * \brief Constructor
   *
   * \param default_output_dir Default directory for generated fixtures
   */
  explicit FixtureGenerator(const std::string& default_output_dir = "test/compat/fixtures");

  ~FixtureGenerator() = default;

  /**
   * \brief Generate a test fixture DFT file
   *
   * This method:
   * 1. Creates a temporary directory with test files
   * 2. Invokes mkdwarfs to create a DFT file
   * 3. Moves the DFT file to the fixtures directory with correct naming
   * 4. Cleans up temporary files
   *
   * \param spec Fixture specification
   * \return Path to the generated fixture file
   * \throws std::runtime_error if generation fails
   */
  std::string generate(const FixtureSpec& spec);

  /**
   * \brief Generate test metadata
   *
   * Creates a populated domain::metadata object with typical
   * filesystem contents (files, directories, symlinks, etc.)
   *
   * \param num_files Number of files to include
   * \param num_directories Number of directories to include
   * \param include_symlinks Whether to include symbolic links
   * \return Populated metadata object
   */
  static metadata_domain::metadata create_test_metadata(
      size_t num_files = 100,
      size_t num_directories = 10,
      bool include_symlinks = true,
      bool include_hardlinks = true);

  /**
   * \brief Check if a fixture already exists
   *
   * \param spec Fixture specification
   * \return true if fixture exists and is valid
   */
  bool fixture_exists(const FixtureSpec& spec) const;

  /**
   * \brief Get fixture path for a spec
   *
   * \param spec Fixture specification
   * \return Full path to the fixture file
   */
  std::string get_fixture_path(const FixtureSpec& spec) const;

  /**
   * \brief Set the Homebrew detector to use
   *
   * \param detector Homebrew detector instance
   */
  void set_homebrew_detector(std::shared_ptr<HomebrewDetector> detector) {
    homebrew_detector_ = std::move(detector);
  }

private:
  /**
   * \brief Create a temporary directory with test files
   */
  std::string create_temp_directory(const metadata_domain::metadata& meta);

  /**
   * \brief Invoke mkdwarfs to create a DFT file
   */
  std::string invoke_mkdwarfs(
      const std::string& temp_dir,
      const std::string& output_path,
      const HomebrewInfo& info);

  /**
   * \brief Clean up temporary files
   */
  void cleanup_temp_directory(const std::string& temp_dir);

  std::string default_output_dir_;
  std::shared_ptr<HomebrewDetector> homebrew_detector_;
};

} // namespace dwarfs::test::compat
