/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "test_helpers.h"
#include "test_tool_main_tester.h"

namespace fs = std::filesystem;

using namespace dwarfs;
using namespace dwarfs::test;

namespace {

// Helper to create test directory structure
void create_test_directory(const fs::path& dir) {
  fs::create_directories(dir);

  // Create various files with different content
  std::ofstream(dir / "file1.txt") << "Content of file 1\n";
  std::ofstream(dir / "file2.txt") << "This is file 2 with more text content\n";
  std::ofstream(dir / "file3.dat") << "Binary-like data: \x01\x02\x03\x04\n";

  // Create subdirectory with files
  fs::create_directories(dir / "subdir");
  std::ofstream(dir / "subdir/nested1.txt") << "Nested file 1\n";
  std::ofstream(dir / "subdir/nested2.txt") << "Nested file 2 content\n";

  // Create deeper nesting
  fs::create_directories(dir / "subdir/deep");
  std::ofstream(dir / "subdir/deep/deep_file.txt") << "Deeply nested content\n";
}

// Compare directory trees recursively
bool compare_directories(const fs::path& dir1, const fs::path& dir2) {
  std::vector<fs::path> files1, files2;

  // Collect all files from both directories
  for (auto& entry : fs::recursive_directory_iterator(dir1)) {
    if (entry.is_regular_file()) {
      files1.push_back(fs::relative(entry.path(), dir1));
    }
  }

  for (auto& entry : fs::recursive_directory_iterator(dir2)) {
    if (entry.is_regular_file()) {
      files2.push_back(fs::relative(entry.path(), dir2));
    }
  }

  // Sort for comparison
  std::sort(files1.begin(), files1.end());
  std::sort(files2.begin(), files2.end());

  // Check same number of files
  if (files1.size() != files2.size()) {
    std::cerr << "Different number of files: " << files1.size()
              << " vs " << files2.size() << std::endl;
    return false;
  }

  // Compare each file
  for (size_t i = 0; i < files1.size(); ++i) {
    if (files1[i] != files2[i]) {
      std::cerr << "Different file names: " << files1[i]
                << " vs " << files2[i] << std::endl;
      return false;
    }

    // Compare file contents
    std::ifstream f1(dir1 / files1[i], std::ios::binary);
    std::ifstream f2(dir2 / files2[i], std::ios::binary);

    std::string content1((std::istreambuf_iterator<char>(f1)), {});
    std::string content2((std::istreambuf_iterator<char>(f2)), {});

    if (content1 != content2) {
      std::cerr << "Content mismatch: " << files1[i] << std::endl;
      std::cerr << "  Expected: " << content1.size() << " bytes" << std::endl;
      std::cerr << "  Got:      " << content2.size() << " bytes" << std::endl;
      return false;
    }
  }

  return true;
}

} // anonymous namespace

class ImageConversionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    DWARFS_SLOW_TEST();

    // Create temp directories for tests
    test_input_dir_ = fs::temp_directory_path() / "dwarfs_conversion_test_input";
    test_extract_dir_ = fs::temp_directory_path() / "dwarfs_conversion_test_extract";

    // Clean up any previous test runs
    fs::remove_all(test_input_dir_);
    fs::remove_all(test_extract_dir_);

    // Create fresh test data
    create_test_directory(test_input_dir_);
  }

  void TearDown() override {
    // Clean up test directories
    fs::remove_all(test_input_dir_);
    fs::remove_all(test_extract_dir_);

    // Clean up any images created
    for (const auto& img : created_images_) {
      fs::remove(img);
    }
  }

  fs::path create_image(const std::string& format_name,
                       const std::string& suffix = "") {
    fs::path image_path = fs::temp_directory_path() /
                         ("test_" + format_name + suffix + ".dwarfs");

    tool_main_tester tester;
    std::vector<std::string> args = {
      "mkdwarfs",
      "-i", test_input_dir_.string(),
      "-o", image_path.string(),
      "--metadata-format", format_name
    };

    int exitcode = tester.run("mkdwarfs", args);
    EXPECT_EQ(0, exitcode) << "Failed to create image with format: " << format_name;

    created_images_.push_back(image_path);
    return image_path;
  }

  void extract_image(const fs::path& image_path, const fs::path& output_dir) {
    fs::create_directories(output_dir);

    tool_main_tester tester;
    std::vector<std::string> args = {
      "dwarfsextract",
      "-i", image_path.string(),
      "-o", output_dir.string()
    };

    int exitcode = tester.run("dwarfsextract", args);
    EXPECT_EQ(0, exitcode) << "Failed to extract image: " << image_path;
  }

  fs::path test_input_dir_;
  fs::path test_extract_dir_;
  std::vector<fs::path> created_images_;
};

// NOTE: Format conversion tests are limited with current architecture:
// - FlatBuffers: Modern default format (can write)
// - Thrift: Legacy format (read-only for backward compatibility)
//
// Since Thrift is read-only, we cannot convert TO Thrift format.
// The main conversion workflow is:
//   Thrift (old image) → FlatBuffers (new image)
// This is tested implicitly through recompression workflows.

// Placeholder test to ensure the test fixture compiles
TEST_F(ImageConversionTest, PlaceholderTest) {
  // This test exists to maintain the test fixture structure
  // Actual format conversion tests should be added when testing
  // specific recompression workflows or when additional write-capable
  // formats are available
  EXPECT_TRUE(true);
}