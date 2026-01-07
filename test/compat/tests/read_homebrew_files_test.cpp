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

} // anonymous namespace

/**
 * \brief Test fixture for reading Homebrew-generated DFT files
 */
class ReadHomebrewFilesTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Get platform/arch information
    auto detector = std::make_shared<HomebrewDetector>();
    info_ = detector->detect();

    // Set up cache directory
    auto temp_path = fs::temp_directory_path() / "dwarfs-compat-test-read";
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

  std::string cache_dir_;
  std::shared_ptr<FixtureCache> cache_;
  std::shared_ptr<FixtureGenerator> generator_;
  HomebrewInfo info_;
};

/**
 * \brief Test reading a basic Homebrew-generated DFT file
 */
TEST_F(ReadHomebrewFilesTest, ReadBasicFixture) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate fixture if not cached
  if (!cache_->has_valid_fixture(spec)) {
    std::string fixture_path = generator_->generate(spec);
    std::vector<uint8_t> data = read_file_to_bytes(fixture_path);
    cache_->store(spec, data);
  }

  // Load the fixture
  std::vector<uint8_t> data = cache_->load(spec);

  // Basic validation
  EXPECT_FALSE(data.empty()) << "Fixture data should not be empty";
  EXPECT_GT(data.size(), 100) << "Fixture should have reasonable size";
}

/**
 * \brief Test reading fixture with SHA256 validation
 */
TEST_F(ReadHomebrewFilesTest, ReadWithChecksumValidation) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate and store fixture
  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> data = read_file_to_bytes(fixture_path);
  cache_->store(spec, data);

  // Verify checksum is stored
  FixtureChecksum checksum = cache_->get_checksum(spec);

  EXPECT_EQ(checksum.algorithm, "sha256") << "Checksum algorithm should be sha256";
  EXPECT_FALSE(checksum.hash.empty()) << "Checksum hash should not be empty";
  EXPECT_GT(checksum.size, 0) << "Checksum size should be positive";

  // Validate checksum
  EXPECT_TRUE(cache_->validate_checksum(spec)) << "Checksum should be valid";
}

/**
 * \brief Test reading multiple fixtures with different versions
 */
TEST_F(ReadHomebrewFilesTest, ReadMultipleVersions) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  // Test with current version
  auto spec_latest = create_test_spec("latest");
  std::string fixture_path_latest = generator_->generate(spec_latest);
  std::vector<uint8_t> data_latest = read_file_to_bytes(fixture_path_latest);
  cache_->store(spec_latest, data_latest);

  EXPECT_FALSE(data_latest.empty()) << "Latest version fixture should have data";

  // Note: Testing multiple versions would require having those versions installed
  // For now, we just verify the latest works
}

/**
 * \brief Test reading fixture from cache (without regeneration)
 */
TEST_F(ReadHomebrewFilesTest, ReadFromCache) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate and cache fixture
  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> original_data = read_file_to_bytes(fixture_path);
  cache_->store(spec, original_data);

  // Verify fixture is valid
  EXPECT_TRUE(cache_->has_valid_fixture(spec)) << "Fixture should be valid in cache";

  // Load from cache
  std::vector<uint8_t> cached_data = cache_->load(spec);

  EXPECT_EQ(cached_data.size(), original_data.size())
      << "Cached data should have same size as original";
  EXPECT_EQ(cached_data, original_data)
      << "Cached data should match original data";
}

/**
 * \brief Test that invalid fixture is detected
 */
TEST_F(ReadHomebrewFilesTest, DetectInvalidFixture) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate valid fixture
  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> data = read_file_to_bytes(fixture_path);
  cache_->store(spec, data);

  // Corrupt the checksum by changing the stored hash
  std::string checksum_path = cache_dir_ + "/" + spec.get_filename() + ".sha256";
  std::ofstream(checksum_path) << "invalid_checksum 100 darwin-arm64\n";

  // Validation should fail
  EXPECT_FALSE(cache_->validate_checksum(spec))
      << "Checksum validation should fail for corrupted checksum";
}

/**
 * \brief Test fixture listing
 */
TEST_F(ReadHomebrewFilesTest, ListFixtures) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  // Generate a couple of fixtures
  auto spec1 = create_test_spec("latest");
  std::string fixture_path1 = generator_->generate(spec1);
  std::vector<uint8_t> data1 = read_file_to_bytes(fixture_path1);
  cache_->store(spec1, data1);

  // List fixtures
  std::vector<FixtureSpec> fixtures = cache_->list_fixtures();

  EXPECT_GE(fixtures.size(), 1) << "Should have at least one fixture";

  // Check if our fixture is in the list
  bool found = false;
  for (const auto& fixture : fixtures) {
    if (fixture.get_filename() == spec1.get_filename()) {
      found = true;
      break;
    }
  }

  EXPECT_TRUE(found) << "Generated fixture should be in the list";
}

/**
 * \brief Test reading fixture fails when file doesn't exist
 */
TEST_F(ReadHomebrewFilesTest, ReadNonExistentFixture) {
  FixtureSpec spec;
  spec.dwarfs_version = "latest";
  spec.platform = "darwin";
  spec.arch = "arm64";
  spec.output_dir = cache_dir_;

  EXPECT_THROW(
      {
        cache_->load(spec);
      },
      std::runtime_error
  ) << "Loading non-existent fixture should throw exception";
}

/**
 * \brief Test compatibility tester read test
 */
TEST_F(ReadHomebrewFilesTest, CompatibilityTesterReadTest) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto tester = std::make_shared<CompatibilityTester>(cache_, generator_, nullptr);
  tester->set_verbose(false);

  auto spec = create_test_spec();

  TestResult result = tester->test_read_homebrew_file(spec);

  EXPECT_TRUE(result.passed) << "Read test should pass: " << result.error_message;
  EXPECT_EQ(result.test_name, "read_homebrew_file");
  EXPECT_EQ(result.dwarfs_version, spec.dwarfs_version);
  EXPECT_EQ(result.platform, spec.platform);
  EXPECT_EQ(result.arch, spec.arch);
}

/**
 * \brief Test with various platform/arch combinations
 */
TEST_F(ReadHomebrewFilesTest, PlatformArchCombinations) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  // Current platform/arch should work
  auto spec = create_test_spec();

  EXPECT_EQ(spec.platform, info_.platform) << "Platform should match detected platform";
  EXPECT_EQ(spec.arch, info_.arch) << "Architecture should match detected architecture";

  // Generate fixture for current platform
  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> data = read_file_to_bytes(fixture_path);
  cache_->store(spec, data);

  EXPECT_FALSE(data.empty()) << "Should be able to generate fixture for current platform";
}

/**
 * \brief Test fixture size statistics
 */
TEST_F(ReadHomebrewFilesTest, FixtureSizeStatistics) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate fixture
  std::string fixture_path = generator_->generate(spec);
  std::vector<uint8_t> data = read_file_to_bytes(fixture_path);
  cache_->store(spec, data);

  // Get checksum with size
  FixtureChecksum checksum = cache_->get_checksum(spec);

  EXPECT_EQ(checksum.size, data.size()) << "Checksum size should match data size";

  // Check cache size
  size_t cache_size = cache_->get_cache_size();

  EXPECT_GT(cache_size, 0) << "Cache should have non-zero size";
  EXPECT_GE(cache_size, data.size()) << "Cache size should be at least fixture size";
}

} // namespace dwarfs::test::compat
