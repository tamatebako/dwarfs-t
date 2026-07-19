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
#include "fixture_generator.h"
#include "fixture_cache.h"
#include "homebrew_detector.h"

namespace dwarfs::test::compat {

/**
 * \brief Test result information
 */
struct TestResult {
  std::string test_name;
  std::string dwarfs_version;
  std::string platform;
  std::string arch;
  bool passed;
  std::string error_message;
  double duration_ms;
};

/**
 * \brief Compatibility test suite
 *
 * This class executes compatibility tests between our Legacy Thrift
 * implementation and the Homebrew dwarfs tool.
 */
class CompatibilityTester {
public:
  /**
   * \brief Test type enumeration
   */
  enum class TestType {
    READ_HOMEBREW_FILES,     ///< Test reading Homebrew-generated DFT files
    WRITE_COMPATIBLE_FILES,  ///< Test writing DFT files Homebrew can read
    ROUND_TRIP               ///< Test full round-trip serialization
  };

  /**
   * \brief Constructor
   *
   * \param cache Fixture cache instance
   * \param generator Fixture generator instance
   * \param detector Homebrew detector instance
   */
  CompatibilityTester(
      std::shared_ptr<FixtureCache> cache,
      std::shared_ptr<FixtureGenerator> generator,
      std::shared_ptr<HomebrewDetector> detector);

  ~CompatibilityTester() = default;

  /**
   * \brief Test reading Homebrew-generated DFT files
   *
   * This test verifies that our deserializer can correctly read
   * DFT files created by the Homebrew mkdwarfs tool.
   *
   * \param spec Fixture specification
   * \return Test result
   */
  TestResult test_read_homebrew_file(const FixtureSpec& spec);

  /**
   * \brief Test writing DFT files Homebrew can read
   *
   * This test verifies that our serializer creates DFT files
   * that the Homebrew dwarfs tool can read.
   *
   * \param spec Fixture specification
   * \return Test result
   */
  TestResult test_write_compatible_file(const FixtureSpec& spec);

  /**
   * \brief Test full round-trip
   *
   * This test performs a full round-trip:
   * 1. Serialize metadata with our implementation
   * 2. Read with Homebrew dwarfs
   * 3. Create file with Homebrew mkdwarfs
   * 4. Deserialize with our implementation
   *
   * \param spec Fixture specification
   * \return Test result
   */
  TestResult test_round_trip(const FixtureSpec& spec);

  /**
   * \brief Run all configured tests
   *
   * \param specs Fixture specifications to test
   * \param test_types Which test types to run
   * \return Vector of test results
   */
  std::vector<TestResult> run_all_tests(
      const std::vector<FixtureSpec>& specs,
      const std::vector<TestType>& test_types = {
          TestType::READ_HOMEBREW_FILES,
          TestType::WRITE_COMPATIBLE_FILES,
          TestType::ROUND_TRIP
      });

  /**
   * \brief Set verbose mode
   *
   * \param verbose Enable verbose output
   */
  void set_verbose(bool verbose) {
    verbose_ = verbose;
  }

  /**
   * \brief Set fail-fast mode
   *
   * When true, testing will stop on first failure.
   *
   * \param fail_fast Enable fail-fast mode
   */
  void set_fail_fast(bool fail_fast) {
    fail_fast_ = fail_fast;
  }

  /**
   * \brief Format test results as a table
   *
   * \param results Test results to format
   * \return Formatted table string
   */
  static std::string format_results_table(const std::vector<TestResult>& results);

  /**
   * \brief Format test results as JSON
   *
   * \param results Test results to format
   * \return JSON string
   */
  static std::string format_results_json(const std::vector<TestResult>& results);

private:
  /**
   * \brief Verify metadata matches expected structure
   */
  bool verify_metadata(const metadata_domain::metadata& meta);

  /**
   * \brief Invoke Homebrew dwarfs to read a DFT file
   */
  bool invoke_dwarfs_read(const std::string& dft_path, const std::string& output_dir);

  /**
   * \brief Create a test fixture using Homebrew mkdwarfs
   */
  std::string create_homebrew_fixture(const FixtureSpec& spec);

  std::shared_ptr<FixtureCache> cache_;
  std::shared_ptr<FixtureGenerator> generator_;
  std::shared_ptr<HomebrewDetector> detector_;
  bool verbose_ = false;
  bool fail_fast_ = false;
};

} // namespace dwarfs::test::compat
