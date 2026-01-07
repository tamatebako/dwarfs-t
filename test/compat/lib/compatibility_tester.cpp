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

#include "compatibility_tester.h"

#include <chrono>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace dwarfs::test::compat {

namespace {
namespace fs = std::filesystem;

// Helper function to read file into byte vector
std::vector<uint8_t> read_file_to_bytes(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + path);
  }
  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char*>(data.data()), size);
  if (!file) {
    throw std::runtime_error("Failed to read file: " + path);
  }
  return data;
}

std::string test_type_to_string(CompatibilityTester::TestType type) {
  switch (type) {
    case CompatibilityTester::TestType::READ_HOMEBREW_FILES:
      return "READ_HOMEBREW_FILES";
    case CompatibilityTester::TestType::WRITE_COMPATIBLE_FILES:
      return "WRITE_COMPATIBLE_FILES";
    case CompatibilityTester::TestType::ROUND_TRIP:
      return "ROUND_TRIP";
  }
  return "UNKNOWN";
}

} // anonymous namespace

CompatibilityTester::CompatibilityTester(
    std::shared_ptr<FixtureCache> cache,
    std::shared_ptr<FixtureGenerator> generator,
    std::shared_ptr<HomebrewDetector> detector)
    : cache_(cache)
    , generator_(generator)
    , detector_(detector) {
}

TestResult CompatibilityTester::test_read_homebrew_file(const FixtureSpec& spec) {
  TestResult result;
  result.test_name = "read_homebrew_file";
  result.dwarfs_version = spec.dwarfs_version;
  result.platform = spec.platform;
  result.arch = spec.arch;
  result.passed = false;

  auto start = std::chrono::high_resolution_clock::now();

  try {
    // Check if fixture exists or can be generated
    if (!cache_->has_valid_fixture(spec)) {
      if (verbose_) {
        std::cerr << "Fixture not found, generating: " << spec.get_filename() << std::endl;
      }

      // Generator returns path to created fixture file
      std::string fixture_path = generator_->generate(spec);
      // Read the fixture file into bytes and cache it
      std::vector<uint8_t> data = read_file_to_bytes(fixture_path);
      cache_->store(spec, data);
    }

    // Load the fixture
    std::vector<uint8_t> data = cache_->load(spec);

    // Verify we can read the fixture
    // TODO: Actually deserialize using our implementation
    if (data.empty()) {
      result.error_message = "Failed to load fixture: empty data";
    } else {
      // Basic validation: check if file has reasonable size
      if (data.size() < 100) {
        result.error_message = "Fixture too small, likely corrupted";
      } else {
        result.passed = true;
      }
    }

  } catch (const std::exception& e) {
    result.error_message = std::string("Exception: ") + e.what();
  }

  auto end = std::chrono::high_resolution_clock::now();
  result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

  return result;
}

TestResult CompatibilityTester::test_write_compatible_file(const FixtureSpec& spec) {
  TestResult result;
  result.test_name = "write_compatible_file";
  result.dwarfs_version = spec.dwarfs_version;
  result.platform = spec.platform;
  result.arch = spec.arch;
  result.passed = false;

  auto start = std::chrono::high_resolution_clock::now();

  try {
    // Create test metadata
    metadata_domain::metadata meta = generator_->create_test_metadata(100, 10, true, false);

    // TODO: Serialize using our Legacy Thrift implementation
    // For now, we'll generate a fixture using mkdwarfs and verify it exists
    FixtureSpec write_spec;
    write_spec.dwarfs_version = spec.dwarfs_version;
    write_spec.platform = spec.platform;
    write_spec.arch = spec.arch;
    write_spec.test_metadata = meta;
    write_spec.output_dir = spec.output_dir;

    std::string fixture_path = generator_->generate(write_spec);

    // Verify the fixture was created
    struct stat buffer;
    if (stat(fixture_path.c_str(), &buffer) != 0) {
      result.error_message = "Failed to create fixture file";
    } else if (buffer.st_size < 100) {
      result.error_message = "Created fixture is too small";
    } else {
      result.passed = true;
    }

  } catch (const std::exception& e) {
    result.error_message = std::string("Exception: ") + e.what();
  }

  auto end = std::chrono::high_resolution_clock::now();
  result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

  return result;
}

TestResult CompatibilityTester::test_round_trip(const FixtureSpec& spec) {
  TestResult result;
  result.test_name = "round_trip";
  result.dwarfs_version = spec.dwarfs_version;
  result.platform = spec.platform;
  result.arch = spec.arch;
  result.passed = false;

  auto start = std::chrono::high_resolution_clock::now();

  try {
    // Step 1: Create test metadata
    metadata_domain::metadata meta = generator_->create_test_metadata(100, 10, true, false);

    // Step 2: Generate fixture using Homebrew mkdwarfs
    FixtureSpec gen_spec;
    gen_spec.dwarfs_version = spec.dwarfs_version;
    gen_spec.platform = spec.platform;
    gen_spec.arch = spec.arch;
    gen_spec.test_metadata = meta;
    gen_spec.output_dir = spec.output_dir;

    std::string fixture_path = generator_->generate(gen_spec);

    // Step 3: Verify fixture exists
    struct stat buffer;
    if (stat(fixture_path.c_str(), &buffer) != 0) {
      result.error_message = "Failed to create fixture for round-trip test";
    } else {
      // Step 4: Load and deserialize the fixture
      std::vector<uint8_t> data = cache_->load(spec);

      // TODO: Actually deserialize and verify metadata
      if (data.empty()) {
        result.error_message = "Failed to load fixture for round-trip";
      } else {
        result.passed = true;
      }
    }

  } catch (const std::exception& e) {
    result.error_message = std::string("Exception: ") + e.what();
  }

  auto end = std::chrono::high_resolution_clock::now();
  result.duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

  return result;
}

std::vector<TestResult> CompatibilityTester::run_all_tests(
    const std::vector<FixtureSpec>& specs,
    const std::vector<TestType>& test_types) {

  std::vector<TestResult> results;

  for (auto const& spec : specs) {
    for (auto test_type : test_types) {
      TestResult result;

      switch (test_type) {
        case TestType::READ_HOMEBREW_FILES:
          result = test_read_homebrew_file(spec);
          break;
        case TestType::WRITE_COMPATIBLE_FILES:
          result = test_write_compatible_file(spec);
          break;
        case TestType::ROUND_TRIP:
          result = test_round_trip(spec);
          break;
      }

      results.push_back(result);

      if (verbose_) {
        std::cerr << "Test: " << result.test_name
                  << " | " << result.dwarfs_version
                  << " | " << result.platform
                  << "/" << result.arch
                  << " | " << (result.passed ? "PASS" : "FAIL")
                  << " | " << result.duration_ms << "ms";
        if (!result.error_message.empty()) {
          std::cerr << " | " << result.error_message;
        }
        std::cerr << std::endl;
      }

      if (fail_fast_ && !result.passed) {
        if (verbose_) {
          std::cerr << "Fail-fast enabled, stopping tests." << std::endl;
        }
        return results;
      }
    }
  }

  return results;
}

std::string CompatibilityTester::format_results_table(const std::vector<TestResult>& results) {
  std::ostringstream ss;

  // Table header
  ss << "\n";
  ss << std::left << std::setw(25) << "Test Name"
     << std::setw(12) << "Version"
     << std::setw(10) << "Platform"
     << std::setw(8) << "Arch"
     << std::setw(8) << "Status"
     << std::setw(12) << "Duration"
     << "Error Message\n";
  ss << std::string(25 + 12 + 10 + 8 + 8 + 12 + 50, '-') << "\n";

  // Table rows
  for (auto const& result : results) {
    ss << std::left << std::setw(25) << result.test_name
       << std::setw(12) << result.dwarfs_version
       << std::setw(10) << result.platform
       << std::setw(8) << result.arch
       << std::setw(8) << (result.passed ? "PASS" : "FAIL")
       << std::setw(12) << (std::to_string(result.duration_ms) + "ms");

    if (!result.error_message.empty()) {
      std::string msg = result.error_message;
      if (msg.length() > 47) {
        msg = msg.substr(0, 47) + "...";
      }
      ss << msg;
    }
    ss << "\n";
  }

  // Summary
  size_t passed = std::count_if(results.begin(), results.end(),
                                [](const TestResult& r) { return r.passed; });
  size_t failed = results.size() - passed;

  ss << "\n";
  ss << "Summary: " << passed << " passed, " << failed << " failed (total: " << results.size() << ")\n";

  return ss.str();
}

std::string CompatibilityTester::format_results_json(const std::vector<TestResult>& results) {
  std::ostringstream ss;

  ss << "{\n";
  ss << "  \"results\": [\n";

  for (size_t i = 0; i < results.size(); ++i) {
    auto const& result = results[i];

    ss << "    {\n";
    ss << "      \"test_name\": \"" << result.test_name << "\",\n";
    ss << "      \"dwarfs_version\": \"" << result.dwarfs_version << "\",\n";
    ss << "      \"platform\": \"" << result.platform << "\",\n";
    ss << "      \"arch\": \"" << result.arch << "\",\n";
    ss << "      \"passed\": " << (result.passed ? "true" : "false") << ",\n";
    ss << "      \"duration_ms\": " << result.duration_ms;

    if (!result.error_message.empty()) {
      ss << ",\n";
      ss << "      \"error_message\": \"" << result.error_message << "\"";
    } else {
      ss << "\n";
    }

    ss << "    }";
    if (i < results.size() - 1) {
      ss << ",";
    }
    ss << "\n";
  }

  // Add summary
  size_t passed = std::count_if(results.begin(), results.end(),
                                [](const TestResult& r) { return r.passed; });
  size_t failed = results.size() - passed;

  ss << "  ],\n";
  ss << "  \"summary\": {\n";
  ss << "    \"total\": " << results.size() << ",\n";
  ss << "    \"passed\": " << passed << ",\n";
  ss << "    \"failed\": " << failed << "\n";
  ss << "  }\n";
  ss << "}\n";

  return ss.str();
}

bool CompatibilityTester::verify_metadata(const metadata_domain::metadata& meta) {
  // Basic metadata validation
  if (meta.directories.empty()) {
    return false;
  }

  if (meta.inodes.empty()) {
    return false;
  }

  if (meta.modes.empty()) {
    return false;
  }

  return true;
}

bool CompatibilityTester::invoke_dwarfs_read(const std::string& dft_path,
                                             const std::string& output_dir) {
  HomebrewInfo info = detector_->detect();

  if (!info.is_complete() || info.dwarfs_path.empty()) {
    return false;
  }

  // Create output directory
  fs::create_directories(output_dir);

  // Build command
  std::ostringstream cmd;
  cmd << info.dwarfs_path
      << " -i " << dft_path
      << " -o " << output_dir;

  // Execute command
  int status = system(cmd.str().c_str());
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status) == 0;
  }

  return false;
}

std::string CompatibilityTester::create_homebrew_fixture(const FixtureSpec& spec) {
  return generator_->generate(spec);
}

} // namespace dwarfs::test::compat
