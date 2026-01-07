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
 * \brief Test fixture for writing DFT files compatible with Homebrew
 */
class WriteCompatibleFilesTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Get platform/arch information
    auto detector = std::make_shared<HomebrewDetector>();
    info_ = detector->detect();

    // Set up cache directory
    auto temp_path = fs::temp_directory_path() / "dwarfs-compat-test-write";
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
   * \brief Verify a fixture file exists and is valid
   */
  bool verify_fixture_exists(const std::string& filename) {
    std::string filepath = cache_dir_ + "/" + filename;
    struct stat buffer;
    return (stat(filepath.c_str(), &buffer) == 0) && S_ISREG(buffer.st_mode);
  }

  std::string cache_dir_;
  std::shared_ptr<FixtureCache> cache_;
  std::shared_ptr<FixtureGenerator> generator_;
  HomebrewInfo info_;
};

/**
 * \brief Test writing a basic compatible DFT file
 */
TEST_F(WriteCompatibleFilesTest, WriteBasicFixture) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate fixture using mkdwarfs
  std::string result_path = generator_->generate(spec);

  // Verify file was created
  EXPECT_TRUE(fs::exists(result_path)) << "Fixture file should be created";

  // Verify file has reasonable size
  std::error_code ec;
  auto file_size = fs::file_size(result_path, ec);
  EXPECT_FALSE(ec) << "Should be able to get file size";
  EXPECT_GT(file_size, 100) << "Fixture file should have reasonable size";
}

/**
 * \brief Test writing fixture with different metadata configurations
 */
TEST_F(WriteCompatibleFilesTest, WriteWithVariousMetadata) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  // Test with small metadata
  {
    FixtureSpec spec = create_test_spec();
    spec.test_metadata = generator_->create_test_metadata(10, 2, false, false);

    std::string result_path = generator_->generate(spec);

    EXPECT_TRUE(fs::exists(result_path)) << "Small fixture should be created";
  }

  // Test with larger metadata
  {
    FixtureSpec spec = create_test_spec();
    spec.test_metadata = generator_->create_test_metadata(100, 10, true, true);

    std::string result_path = generator_->generate(spec);

    EXPECT_TRUE(fs::exists(result_path)) << "Large fixture should be created";
  }
}

/**
 * \brief Test writing fixture creates valid checksum
 */
TEST_F(WriteCompatibleFilesTest, WriteCreatesValidChecksum) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Generate fixture
  std::string result_path = generator_->generate(spec);

  // Store in cache (which creates checksum)
  std::vector<uint8_t> data = cache_->load(spec);
  cache_->store(spec, data);

  // Verify checksum exists and is valid
  EXPECT_TRUE(cache_->validate_checksum(spec)) << "Checksum should be valid";

  FixtureChecksum checksum = cache_->get_checksum(spec);

  EXPECT_EQ(checksum.algorithm, "sha256") << "Algorithm should be sha256";
  EXPECT_FALSE(checksum.hash.empty()) << "Hash should not be empty";
  EXPECT_GT(checksum.size, 0) << "Size should be positive";
  EXPECT_EQ(checksum.platform_arch, spec.platform + "-" + spec.arch)
      << "Platform/arch should match";
}

/**
 * \brief Test writing fixture to custom output directory
 */
TEST_F(WriteCompatibleFilesTest, WriteToCustomDirectory) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  // Create custom output directory
  auto custom_dir = cache_dir_ + "/custom_output";
  fs::create_directories(custom_dir);

  FixtureSpec spec = create_test_spec();
  spec.output_dir = custom_dir;

  std::string result_path = generator_->generate(spec);

  EXPECT_TRUE(fs::exists(result_path)) << "Fixture should be created in custom directory";

  // Verify it's in the custom directory
  EXPECT_TRUE(result_path.find("custom_output") != std::string::npos)
      << "Fixture path should contain custom_output";
}

/**
 * \brief Test writing multiple fixtures
 */
TEST_F(WriteCompatibleFilesTest, WriteMultipleFixtures) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  std::vector<std::string> result_paths;

  // Generate multiple fixtures
  for (int i = 0; i < 3; ++i) {
    FixtureSpec spec = create_test_spec();
    spec.test_metadata = generator_->create_test_metadata(20 + i * 10, 3, true, false);

    std::string result_path = generator_->generate(spec);
    result_paths.push_back(result_path);

    EXPECT_TRUE(fs::exists(result_path)) << "Fixture " << i << " should be created";
  }

  // All fixtures should exist
  for (const auto& path : result_paths) {
    EXPECT_TRUE(fs::exists(path)) << "Fixture should exist: " << path;
  }
}

/**
 * \brief Test fixture naming convention
 */
TEST_F(WriteCompatibleFilesTest, FixtureNamingConvention) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();
  spec.dwarfs_version = "0.14.1";

  std::string result_path = generator_->generate(spec);
  std::string filename = fs::path(result_path).filename();

  // Expected format: dwarfs-v{version}-{platform}-{arch}.dft
  std::string expected = "dwarfs-v0.14.1-" + spec.platform + "-" + spec.arch + ".dft";

  EXPECT_EQ(filename, expected) << "Fixture filename should follow naming convention";
}

/**
 * \brief Test writing fixture with symlinks
 */
TEST_F(WriteCompatibleFilesTest, WriteFixtureWithSymlinks) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  FixtureSpec spec = create_test_spec();
  spec.test_metadata = generator_->create_test_metadata(20, 3, true, false);

  std::string result_path = generator_->generate(spec);

  EXPECT_TRUE(fs::exists(result_path)) << "Fixture with symlinks should be created";
}

/**
 * \brief Test writing fixture with hardlinks
 */
TEST_F(WriteCompatibleFilesTest, WriteFixtureWithHardlinks) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  FixtureSpec spec = create_test_spec();
  spec.test_metadata = generator_->create_test_metadata(20, 3, false, true);

  std::string result_path = generator_->generate(spec);

  EXPECT_TRUE(fs::exists(result_path)) << "Fixture with hardlinks should be created";
}

/**
 * \brief Test compatibility tester write test
 */
TEST_F(WriteCompatibleFilesTest, CompatibilityTesterWriteTest) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto tester = std::make_shared<CompatibilityTester>(cache_, generator_, nullptr);
  tester->set_verbose(false);

  auto spec = create_test_spec();

  TestResult result = tester->test_write_compatible_file(spec);

  EXPECT_TRUE(result.passed) << "Write test should pass: " << result.error_message;
  EXPECT_EQ(result.test_name, "write_compatible_file");
  EXPECT_EQ(result.dwarfs_version, spec.dwarfs_version);
  EXPECT_EQ(result.platform, spec.platform);
  EXPECT_EQ(result.arch, spec.arch);
}

/**
 * \brief Test that fixture can be read back
 */
TEST_F(WriteCompatibleFilesTest, WriteThenReadFixture) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Write fixture
  std::string result_path = generator_->generate(spec);

  // Load it back
  std::vector<uint8_t> data = cache_->load(spec);

  EXPECT_FALSE(data.empty()) << "Should be able to read back fixture";
  EXPECT_GT(data.size(), 100) << "Fixture should have reasonable size";
}

/**
 * \brief Test fixture exists check
 */
TEST_F(WriteCompatibleFilesTest, FixtureExistsCheck) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Initially should not exist
  EXPECT_FALSE(cache_->has_valid_fixture(spec)) << "Fixture should not exist initially";

  // Generate fixture
  std::string result_path = generator_->generate(spec);
  std::vector<uint8_t> data = cache_->load(spec);
  cache_->store(spec, data);

  // Now should exist
  EXPECT_TRUE(cache_->has_valid_fixture(spec)) << "Fixture should exist after generation";
}

/**
 * \brief Test writing with different file size distributions
 */
TEST_F(WriteCompatibleFilesTest, WriteWithDifferentFileSizes) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  // Test with mostly small files
  {
    FixtureSpec spec = create_test_spec();
    spec.test_metadata = generator_->create_test_metadata(30, 2, false, false);

    std::string result_path = generator_->generate(spec);
    EXPECT_TRUE(fs::exists(result_path)) << "Small file fixture should be created";
  }

  // Test with mixed sizes
  {
    FixtureSpec spec = create_test_spec();
    spec.test_metadata = generator_->create_test_metadata(100, 10, true, false);

    std::string result_path = generator_->generate(spec);
    EXPECT_TRUE(fs::exists(result_path)) << "Mixed size fixture should be created";
  }
}

/**
 * \brief Test cache statistics after writing
 */
TEST_F(WriteCompatibleFilesTest, CacheStatisticsAfterWrite) {
  if (!info_.is_complete()) {
    GTEST_SKIP() << "Homebrew dwarfs not available";
  }

  auto spec = create_test_spec();

  // Write and cache fixture
  std::string result_path = generator_->generate(spec);
  std::vector<uint8_t> data = cache_->load(spec);
  cache_->store(spec, data);

  // Check cache statistics
  size_t fixture_count = cache_->get_fixture_count();
  size_t cache_size = cache_->get_cache_size();

  EXPECT_GT(fixture_count, 0) << "Should have at least one fixture in cache";
  EXPECT_GT(cache_size, 0) << "Cache should have non-zero size";
  EXPECT_GE(cache_size, data.size()) << "Cache size should be at least fixture size";
}

} // namespace dwarfs::test::compat
