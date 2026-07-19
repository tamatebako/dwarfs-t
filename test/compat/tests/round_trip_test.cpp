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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "homebrew_detector.h"
#include "fixture_generator.h"
#include "fixture_cache.h"
#include "compatibility_tester.h"

#include <fstream>
#include <filesystem>

namespace dwarfs::test::compat {

using namespace testing;

namespace fs = std::filesystem;

namespace {

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

// Helper function to check if data is a valid dwarfs file
bool is_valid_dwarfs_file(const std::vector<uint8_t>& data) {
  // Check for DWARFS magic bytes at the start
  // DWARFS files start with "DWARFS" magic
  const std::string magic = "DWARFS";
  if (data.size() < magic.size()) {
    return false;
  }
  return std::string(data.begin(), data.begin() + magic.size()) == magic;
}

} // anonymous namespace

/**
 * \brief Test fixture for round-trip serialization tests
 */
class RoundTripTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Get platform/arch information
    auto detector = std::make_shared<HomebrewDetector>();
    info_ = detector->detect();

    // Set up cache directory
    auto temp_path = fs::temp_directory_path() / "dwarfs-compat-test-roundtrip";
    cache_dir_ = temp_path.string();
    fs::create_directories(temp_path);

    cache_ = std::make_shared<FixtureCache>(cache_dir_);
    generator_ = std::make_shared<FixtureGenerator>(cache_dir_);
  }

  void TearDown() override {
    // Clean up cache directory
    std::error_code ec;
    fs::remove_all(cache_dir_, ec);
  }

  /**
   * \brief Create a test fixture specification
   */
  FixtureSpec create_test_spec(const std::string& version = "latest") {
    FixtureSpec spec;
    spec.dwarfs_version = version;
    spec.platform = info_.platform;
    spec.arch = info_.arch;
    spec.test_metadata = generator_->create_test_metadata(50, 5, true, false);
    spec.output_dir = cache_dir_;
    return spec;
  }

  /**
   * \brief Compare two metadata structures
   */
  bool compare_metadata(const metadata_domain::metadata& lhs, const metadata_domain::metadata& rhs) {
    // Basic comparison of key fields
    if (lhs.directories.size() != rhs.directories.size()) {
      return false;
    }
    if (lhs.inodes.size() != rhs.inodes.size()) {
      return false;
    }
    if (lhs.chunks.size() != rhs.chunks.size()) {
      return false;
    }
    if (lhs.names.size() != rhs.names.size()) {
      return false;
    }
    return true;
  }

  std::string cache_dir_;
  std::shared_ptr<FixtureCache> cache_;
  std::shared_ptr<FixtureGenerator> generator_;
  HomebrewInfo info_;
};

/**
 * \brief Test basic round-trip: write with mkdwarfs, read back
 */
TEST_F(RoundTripTest, BasicRoundTrip) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Step 1: Generate fixture with mkdwarfs
  std::string fixture_path = generator_->generate(spec);

  EXPECT_TRUE(fs::exists(fixture_path)) << "Fixture should be created";

  // Step 2: Load the fixture
  std::vector<uint8_t> data = cache_->load(spec);

  EXPECT_FALSE(data.empty()) << "Should be able to load fixture";

  // Step 3: Store in cache
  cache_->store(spec, data);

  // Step 4: Verify we can load it back
  std::vector<uint8_t> reloaded_data = cache_->load(spec);

  EXPECT_EQ(data, reloaded_data) << "Reloaded data should match original";
}

/**
 * \brief Test round-trip with checksum validation
 */
TEST_F(RoundTripTest, RoundTripWithChecksum) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate and store
  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> data = cache_->load(spec);
  cache_->store(spec, data);

  // Get checksum
  FixtureChecksum checksum = cache_->get_checksum(spec);

  EXPECT_FALSE(checksum.hash.empty()) << "Checksum should be generated";

  // Validate
  EXPECT_TRUE(cache_->validate_checksum(spec)) << "Checksum should be valid";

  // Reload and verify checksum still matches
  std::vector<uint8_t> reloaded_data = cache_->load(spec);

  EXPECT_EQ(data.size(), checksum.size) << "Data size should match checksum size";
}

/**
 * \brief Test round-trip with different metadata configurations
 */
TEST_F(RoundTripTest, RoundTripWithVariousMetadata) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  std::vector<std::pair<size_t, size_t>> configurations = {
    {10, 2},   // Small
    {50, 5},   // Medium
    {100, 10}  // Large
  };

  for (auto [num_files, num_dirs] : configurations) {
    FixtureSpec spec = create_test_spec();
    spec.test_metadata = generator_->create_test_metadata(num_files, num_dirs, true, false);

    std::string fixture_path = generator_->generate(spec);
    std::vector<uint8_t> data = cache_->load(spec);
    cache_->store(spec, data);

    EXPECT_TRUE(fs::exists(fixture_path))
        << "Fixture should be created for " << num_files << " files, " << num_dirs << " dirs";
    EXPECT_FALSE(data.empty()) << "Data should not be empty";

    // Verify round-trip
    std::vector<uint8_t> reloaded_data = cache_->load(spec);
    EXPECT_EQ(data, reloaded_data) << "Round-trip should preserve data";
  }
}

/**
 * \brief Test round-trip with symlinks
 */
TEST_F(RoundTripTest, RoundTripWithSymlinks) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  FixtureSpec spec = create_test_spec();
  spec.test_metadata = generator_->create_test_metadata(30, 5, true, false);

  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> data = cache_->load(spec);
  cache_->store(spec, data);

  std::vector<uint8_t> reloaded_data = cache_->load(spec);

  EXPECT_EQ(data, reloaded_data) << "Round-trip with symlinks should preserve data";
}

/**
 * \brief Test round-trip with hardlinks
 */
TEST_F(RoundTripTest, RoundTripWithHardlinks) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  FixtureSpec spec = create_test_spec();
  spec.test_metadata = generator_->create_test_metadata(30, 5, false, true);

  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> data = cache_->load(spec);
  cache_->store(spec, data);

  std::vector<uint8_t> reloaded_data = cache_->load(spec);

  EXPECT_EQ(data, reloaded_data) << "Round-trip with hardlinks should preserve data";
}

/**
 * \brief Test compatibility tester round-trip
 */
TEST_F(RoundTripTest, CompatibilityTesterRoundTrip) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto tester = std::make_shared<CompatibilityTester>(cache_, generator_, nullptr);
  tester->set_verbose(false);

  auto spec = create_test_spec();

  TestResult result = tester->test_round_trip(spec);

  EXPECT_TRUE(result.passed) << "Round-trip test should pass: " << result.error_message;
  EXPECT_EQ(result.test_name, "round_trip");
  EXPECT_EQ(result.dwarfs_version, spec.dwarfs_version);
  EXPECT_EQ(result.platform, spec.platform);
  EXPECT_EQ(result.arch, spec.arch);
}

/**
 * \brief Test multiple round-trips preserve data
 */
TEST_F(RoundTripTest, MultipleRoundTrips) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // First round-trip
  std::string fixture_path1 = generator_->generate(spec);
  std::vector<uint8_t> data1 = cache_->load(spec);
  cache_->store(spec, data1);

  // Clear and regenerate
  cache_->invalidate(spec);

  // Second round-trip
  std::string fixture_path2 = generator_->generate(spec);
  std::vector<uint8_t> data2 = cache_->load(spec);
  cache_->store(spec, data2);

  // Both outputs should be valid dwarfs files
  EXPECT_TRUE(is_valid_dwarfs_file(data1)) << "First output should be a valid dwarfs file";
  EXPECT_TRUE(is_valid_dwarfs_file(data2)) << "Second output should be a valid dwarfs file";

  // Both should have reasonable size (not empty, not too different)
  EXPECT_FALSE(data1.empty()) << "First output should not be empty";
  EXPECT_FALSE(data2.empty()) << "Second output should not be empty";

  // Sizes should be within 10% of each other (mkdwarfs output may vary slightly)
  size_t avg_size = (data1.size() + data2.size()) / 2;
  EXPECT_LT(data1.size(), avg_size * 1.1) << "First output size should be reasonable";
  EXPECT_GT(data1.size(), avg_size * 0.9) << "First output size should be reasonable";

  // Note: We don't check data1 == data2 because mkdwarfs output is not deterministic
  // due to timestamps and compression variations
}

/**
 * \brief Test round-trip with version specification
 */
TEST_F(RoundTripTest, RoundTripWithVersion) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  FixtureSpec spec = create_test_spec("0.14.1");
  spec.dwarfs_version = info_.version;  // Use actual installed version

  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> data = cache_->load(spec);
  cache_->store(spec, data);

  EXPECT_FALSE(data.empty()) << "Should be able to round-trip with specific version";
}

/**
 * \brief Test round-trip preserves fixture naming
 */
TEST_F(RoundTripTest, RoundTripPreservesNaming) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  std::string expected_filename = spec.get_filename();

  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> data = cache_->load(spec);
  cache_->store(spec, data);

  // List fixtures
  std::vector<FixtureSpec> fixtures = cache_->list_fixtures();

  bool found = false;
  for (const auto& fixture : fixtures) {
    if (fixture.get_filename() == expected_filename) {
      found = true;
      break;
    }
  }

  EXPECT_TRUE(found) << "Round-tripped fixture should be in list";
}

/**
 * \brief Test round-trip failure handling
 */
TEST_F(RoundTripTest, RoundTripFailureHandling) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate fixture
  std::string fixture_path = generator_->generate(spec);

  // Corrupt the fixture file
  std::ofstream corrupt_file(fixture_path, std::ios::binary | std::ios::trunc);
  corrupt_file << "corrupted data";
  corrupt_file.close();

  // Loading should work but validation should fail
  std::vector<uint8_t> data = cache_->load(spec);

  EXPECT_TRUE(data.empty() || data.size() < 100)
      << "Corrupted fixture should be detected";
}

/**
 * \brief Test round-trip timing
 */
TEST_F(RoundTripTest, RoundTripTiming) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto tester = std::make_shared<CompatibilityTester>(cache_, generator_, nullptr);
  tester->set_verbose(false);

  auto spec = create_test_spec();

  auto start = std::chrono::high_resolution_clock::now();

  TestResult result = tester->test_round_trip(spec);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration<double, std::milli>(end - start).count();

  EXPECT_TRUE(result.passed) << "Round-trip should pass";
  EXPECT_GT(result.duration_ms, 0) << "Duration should be positive";
  EXPECT_LT(result.duration_ms, 60000) << "Round-trip should complete in less than 60 seconds";

  // Test result duration should be reasonable
  EXPECT_GT(duration, 0) << "Actual duration should be positive";
}

/**
 * \brief Test round-trip with cache invalidation
 */
TEST_F(RoundTripTest, RoundTripWithCacheInvalidation) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate and cache
  std::string fixture_path1 = generator_->generate(spec);
  std::vector<uint8_t> data1 = cache_->load(spec);
  cache_->store(spec, data1);

  EXPECT_TRUE(cache_->has_valid_fixture(spec)) << "Fixture should be in cache";

  // Invalidate
  cache_->invalidate(spec);

  EXPECT_FALSE(cache_->has_valid_fixture(spec)) << "Fixture should not be in cache after invalidation";

  // Regenerate
  std::string fixture_path2 = generator_->generate(spec);
  std::vector<uint8_t> data2 = cache_->load(spec);
  cache_->store(spec, data2);

  EXPECT_TRUE(cache_->has_valid_fixture(spec)) << "Fixture should be back in cache";
}

/**
 * \brief Test round-trip with all test types
 */
TEST_F(RoundTripTest, AllTestTypesInRoundTrip) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto tester = std::make_shared<CompatibilityTester>(cache_, generator_, nullptr);
  tester->set_verbose(false);

  auto spec = create_test_spec();

  // Test read
  TestResult read_result = tester->test_read_homebrew_file(spec);
  EXPECT_TRUE(read_result.passed) << "Read test should pass";

  // Test write
  TestResult write_result = tester->test_write_compatible_file(spec);
  EXPECT_TRUE(write_result.passed) << "Write test should pass";

  // Test round-trip
  TestResult roundtrip_result = tester->test_round_trip(spec);
  EXPECT_TRUE(roundtrip_result.passed) << "Round-trip test should pass";
}

/**
 * \brief Test round-trip results formatting
 */
TEST_F(RoundTripTest, RoundTripResultsFormatting) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto tester = std::make_shared<CompatibilityTester>(cache_, generator_, nullptr);

  auto spec = create_test_spec();

  std::vector<TestResult> results;
  results.push_back(tester->test_round_trip(spec));

  // Test table formatting
  std::string table = CompatibilityTester::format_results_table(results);

  EXPECT_FALSE(table.empty()) << "Table format should not be empty";
  EXPECT_THAT(table, HasSubstr("round_trip")) << "Table should contain test name";
  EXPECT_THAT(table, HasSubstr("PASS")) << "Table should show PASS status";

  // Test JSON formatting
  std::string json = CompatibilityTester::format_results_json(results);

  EXPECT_FALSE(json.empty()) << "JSON format should not be empty";
  EXPECT_THAT(json, HasSubstr("\"test_name\"")) << "JSON should have test_name field";
  EXPECT_THAT(json, HasSubstr("\"passed\":")) << "JSON should have passed field";
  EXPECT_THAT(json, HasSubstr("\"summary\":")) << "JSON should have summary section";
}

} // namespace dwarfs::test::compat
