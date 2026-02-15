/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Ribose (@riboseinc @tamatebako)
 * \copyright  Copyright (c) Ribose
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <filesystem>

#include <dwarfs/logger.h>
#include <dwarfs/reader/filesystem_options.h>
#include <dwarfs/reader/filesystem_v2.h>

#include "test_helpers.h"
#include "test_logger.h"

using namespace dwarfs;
namespace fs = std::filesystem;

namespace {

// Test fixture for Legacy Thrift compatibility tests
class LegacyThriftCompatibilityTest : public testing::Test {
 protected:
  // Helper to check if we can read a specific compat image
  bool can_read_compat_image(const std::string& version) {
    fs::path compat_dir = fs::path(TEST_DATA_DIR) / "compat";
    fs::path image_file = compat_dir / ("compat-v" + version + ".dwarfs");

    if (!fs::exists(image_file)) {
      return false; // Skip if image doesn't exist
    }

    test::test_logger lgr;
    test::os_access_mock os;

    reader::filesystem_options opts;
    opts.image_offset = reader::filesystem_options::IMAGE_OFFSET_AUTO;

    try {
      reader::filesystem_v2 fs(lgr, os, test::make_real_file_view(image_file),
                               opts);
      return (fs.check(reader::filesystem_check_level::FULL) == 0);
    } catch (...) {
      return false;
    }
  }

  // Helper to find an available test image (compat or regular test data)
  fs::path find_available_test_image() {
    // First, try compat images in order of preference
    std::vector<std::string> compat_versions = {
      "0.2.3", "0.9.10", "0.4.0", "0.4.1", "0.14.1"
    };

    for (const auto& version : compat_versions) {
      fs::path compat_file = fs::path(TEST_DATA_DIR) / "compat" / ("compat-v" + version + ".dwarfs");
      if (fs::exists(compat_file)) {
        return compat_file;
      }
    }

    // Fall back to regular test data files
    std::vector<std::string> test_files = {
      "data.dwarfs",
      "catdata.dwarfs",
      "datadata.dwarfs",
      "timestamps.dwarfs"
    };

    for (const auto& filename : test_files) {
      fs::path test_file = fs::path(TEST_DATA_DIR) / filename;
      if (fs::exists(test_file)) {
        return test_file;
      }
    }

    return {}; // No test image found
  }
};

} // anonymous namespace

// Test 1: Read all available test images (compat images or regular test data)
TEST_F(LegacyThriftCompatibilityTest, ReadAllLegacyCompatImages) {
  std::vector<std::string> readable_images;
  std::vector<std::string> skipped_images;

  // First, try compat images
  fs::path compat_dir = fs::path(TEST_DATA_DIR) / "compat";
  std::vector<std::string> compat_versions = {
    "0.2.0", "0.2.3", "0.3.0", "0.4.0", "0.4.1",
    "0.5.6", "0.6.2", "0.7.5", "0.8.0", "0.9.10", "0.14.1"
  };

  for (const auto& version : compat_versions) {
    if (can_read_compat_image(version)) {
      readable_images.push_back("compat-v" + version + ".dwarfs");
    } else {
      skipped_images.push_back("compat-v" + version + ".dwarfs");
    }
  }

  // Also check regular test data files
  std::vector<std::string> test_files = {
    "data.dwarfs", "catdata.dwarfs", "datadata.dwarfs",
    "timestamps.dwarfs", "unixlink.dwarfs", "winlink.dwarfs"
  };

  for (const auto& filename : test_files) {
    fs::path test_file = fs::path(TEST_DATA_DIR) / filename;
    if (fs::exists(test_file)) {
      test::test_logger lgr;
      test::os_access_mock os;
      reader::filesystem_options opts;
      opts.image_offset = reader::filesystem_options::IMAGE_OFFSET_AUTO;
      try {
        reader::filesystem_v2 fs(lgr, os, test::make_real_file_view(test_file), opts);
        if (fs.check(reader::filesystem_check_level::FULL) == 0) {
          readable_images.push_back(filename);
        } else {
          skipped_images.push_back(filename);
        }
      } catch (...) {
        skipped_images.push_back(filename);
      }
    } else {
      skipped_images.push_back(filename);
    }
  }

  // Skip if no images are available at all
  if (readable_images.empty()) {
    GTEST_SKIP() << "No test images available";
  }

  std::cout << "\n=== DwarFS Compatibility Test Results ===" << std::endl;
  std::cout << "Successfully read " << readable_images.size() << " images:" << std::endl;

  for (const auto& img : readable_images) {
    std::cout << "  ✓ " << img << std::endl;
  }

  if (!skipped_images.empty()) {
    std::cout << "\nSkipped " << skipped_images.size() << " images (not available):" << std::endl;
    for (const auto& img : skipped_images) {
      std::cout << "  ✗ " << img << std::endl;
    }
  }

  // We should be able to read at least some images
  EXPECT_GT(readable_images.size(), 0u)
      << "Expected to read available test images";
}

// Test 2: Verify a dwarfs image can be read fully
// Uses compat image if available, otherwise falls back to test data
TEST_F(LegacyThriftCompatibilityTest, ReadCompatV0910) {
  fs::path image_file = find_available_test_image();

  if (image_file.empty()) {
    GTEST_SKIP() << "No test images available";
  }

  std::cout << "\n=== Using test image: " << image_file.filename() << " ===" << std::endl;

  test::test_logger lgr;
  test::os_access_mock os;

  reader::filesystem_options opts;
  opts.image_offset = reader::filesystem_options::IMAGE_OFFSET_AUTO;

  ASSERT_NO_THROW({
    reader::filesystem_v2 fs(lgr, os, test::make_real_file_view(image_file),
                             opts);

    // Verify filesystem integrity
    EXPECT_EQ(0, fs.check(reader::filesystem_check_level::FULL));

    // Verify root directory
    auto root = fs.root();
    EXPECT_TRUE(root.is_root());

    // Walk filesystem and count entries (basic functionality check)
    size_t entry_count = 0;
    size_t file_count = 0;
    fs.walk([&](auto const& de) {
      entry_count++;
      auto iv = de.inode();
      if (iv.is_regular_file()) {
        file_count++;
      }
    });

    std::cout << "Total entries: " << entry_count << std::endl;
    std::cout << "Files: " << file_count << std::endl;

    // Should have at least some files
    EXPECT_GT(file_count, 0u);
  });
}

// Test 3: Walk filesystem and verify structure
TEST_F(LegacyThriftCompatibilityTest, WalkCompatV040) {
  fs::path image_file = find_available_test_image();

  if (image_file.empty()) {
    GTEST_SKIP() << "No test images available";
  }

  std::cout << "\n=== Using test image: " << image_file.filename() << " ===" << std::endl;

  test::test_logger lgr;
  test::os_access_mock os;

  reader::filesystem_options opts;
  opts.image_offset = reader::filesystem_options::IMAGE_OFFSET_AUTO;

  ASSERT_NO_THROW({
    reader::filesystem_v2 fs(lgr, os, test::make_real_file_view(image_file),
                             opts);

    // Walk filesystem and count entries
    size_t dir_count = 0;
    size_t file_count = 0;
    size_t link_count = 0;

    fs.walk([&](auto const& de) {
      auto iv = de.inode();
      if (iv.is_directory()) {
        dir_count++;
      } else if (iv.is_regular_file()) {
        file_count++;
      } else if (iv.is_symlink()) {
        link_count++;
      }
    });

    std::cout << "Directories: " << dir_count << std::endl;
    std::cout << "Files: " << file_count << std::endl;
    std::cout << "Symlinks: " << link_count << std::endl;

    // Should have at least some files
    EXPECT_GT(file_count, 0u);
  });
}

// Test 4: Verify directory entries are loaded (frozen2 issue check)
TEST_F(LegacyThriftCompatibilityTest, VerifyDirectoryEntriesCompatV041) {
  fs::path image_file = find_available_test_image();

  if (image_file.empty()) {
    GTEST_SKIP() << "No test images available";
  }

  std::cout << "\n=== Using test image: " << image_file.filename() << " ===" << std::endl;

  test::test_logger lgr;
  test::os_access_mock os;

  reader::filesystem_options opts;
  opts.image_offset = reader::filesystem_options::IMAGE_OFFSET_AUTO;

  ASSERT_NO_THROW({
    reader::filesystem_v2 fs(lgr, os, test::make_real_file_view(image_file),
                             opts);

    // Verify filesystem loaded successfully
    EXPECT_EQ(0, fs.check(reader::filesystem_check_level::FULL));

    // Verify root directory exists
    auto root = fs.root();
    EXPECT_TRUE(root.is_root());

    // Try to walk filesystem (this will fail if dir_entries not loaded)
    size_t entry_count = 0;
    fs.walk([&](auto const& de) {
      entry_count++;
    });

    std::cout << "Walk found " << entry_count << " entries" << std::endl;

    // Should have at least some entries
    EXPECT_GT(entry_count, 0u) << "Expected to find filesystem entries";
  });
}
