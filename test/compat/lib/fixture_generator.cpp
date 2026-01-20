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

#include "fixture_generator.h"
#include "homebrew_detector.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <random>
#include <chrono>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace dwarfs::test::compat {

namespace {

namespace fs = std::filesystem;

// Create a temporary directory
std::string create_temp_dir(const std::string& prefix) {
  std::string template_str = fs::temp_directory_path().string() + "/" + prefix + "-XXXXXX";

  // mkdtemp modifies the template in-place, so we need a mutable char array
  std::vector<char> template_buf(template_str.begin(), template_str.end());
  template_buf.push_back('\0');  // Null-terminate

  if (mkdtemp(template_buf.data()) == nullptr) {
    throw std::runtime_error("Failed to create temporary directory: " + template_str);
  }

  return std::string(template_buf.data());
}

// Create a test file with specific content
void create_test_file(const std::string& path, const std::string& content) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to create test file: " + path);
  }
  file << content;
}

// Create a test directory
void create_test_directory(const std::string& path) {
  std::error_code ec;
  if (!fs::create_directories(path, ec) && !fs::exists(path, ec)) {
    throw std::runtime_error("Failed to create directory: " + path + " (" + ec.message() + ")");
  }
}

// Create a symbolic link
void create_symlink(const std::string& target, const std::string& link_path) {
  if (symlink(target.c_str(), link_path.c_str()) != 0) {
    throw std::runtime_error("Failed to create symlink: " + link_path);
  }
}

// Create a hard link
void create_hardlink(const std::string& target, const std::string& link_path) {
  if (link(target.c_str(), link_path.c_str()) != 0) {
    throw std::runtime_error("Failed to create hardlink: " + link_path);
  }
}

// Generate random file content
std::string generate_random_content(size_t size) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 255);

  std::string content;
  content.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    content += static_cast<char>(dis(gen));
  }
  return content;
}

// Get file size for a file size category
size_t get_file_size(size_t category, size_t max_size) {
  // Small files: 0 - 1KB
  // Medium files: 1KB - 100KB
  // Large files: 100KB - max_size
  static std::random_device rd;
  static std::mt19937 gen(rd());
  switch (category) {
    case 0: return std::uniform_int_distribution<size_t>(0, 1024)(gen);
    case 1: return std::uniform_int_distribution<size_t>(1024, 102400)(gen);
    default: return std::uniform_int_distribution<size_t>(102400, max_size)(gen);
  }
}

} // anonymous namespace

metadata_domain::metadata FixtureGenerator::create_test_metadata(
    size_t num_files,
    size_t num_directories,
    bool include_symlinks,
    bool include_hardlinks) {

  metadata_domain::metadata meta;
  meta.timestamp_base = 1609459200; // 2021-01-01 00:00:00 UTC
  meta.block_size = 262144;
  meta.total_fs_size = 1024 * 1024 * 100; // 100 MB

  // Create root directory
  meta.directories.emplace_back(metadata_domain::directory(0, 0, 0));

  // Create subdirectories
  for (size_t i = 1; i < num_directories; ++i) {
    uint32_t parent = (i - 1) % meta.directories.size();
    meta.directories.emplace_back(metadata_domain::directory(i, parent, i));
  }

  // Create files and inodes
  for (size_t i = 0; i < num_files; ++i) {
    // File
    size_t size_category = i % 3;
    size_t file_size = get_file_size(size_category, 1048576); // Max 1MB

    metadata_domain::chunk chunk(i % meta.directories.size(), 0, file_size);
    meta.chunks.push_back(chunk);

    // Inode
    metadata_domain::inode_data inode;
    inode.mode_index = i % meta.modes.size();
    inode.owner_index = 0;
    inode.group_index = 0;

    // Add some variety to inode data
    if (i % 10 == 0) {
      inode.atime_offset = i * 100;
      inode.mtime_offset = i * 100;
    }

    meta.inodes.push_back(inode);

    // File name
    std::ostringstream name;
    name << "file_" << i << ".txt";
    meta.names.push_back(name.str());
  }

  // Add modes, uids, gids
  meta.modes.push_back(0644);  // Regular file
  meta.modes.push_back(0755);  // Directory
  meta.modes.push_back(0777);  // Executable
  meta.modes.push_back(0640);  // User-only

  meta.uids.push_back(0);       // root
  meta.uids.push_back(1000);    // user
  meta.gids.push_back(0);       // root
  meta.gids.push_back(1000);    // group

  return meta;
}

FixtureGenerator::FixtureGenerator(const std::string& default_output_dir)
    : default_output_dir_(default_output_dir)
    , homebrew_detector_(std::make_shared<HomebrewDetector>()) {

  // Ensure output directory exists
  fs::create_directories(default_output_dir_);
}

std::string FixtureGenerator::create_temp_directory(const metadata_domain::metadata& meta) {
  std::string temp_dir = create_temp_dir("dwarfs-compat");

  // Create directory structure from metadata
  for (size_t i = 0; i < meta.directories.size(); ++i) {
    auto const& dir = meta.directories[i];

    // Build directory path
    std::ostringstream path;
    path << temp_dir;

    // Trace back to root to build full path
    std::vector<uint32_t> path_components;
    uint32_t current = i;
    while (current != dir.parent_entry()) {
      path_components.push_back(current);
      current = meta.directories[current].parent_entry();
    }

    for (auto it = path_components.rbegin(); it != path_components.rend(); ++it) {
      path << "/dir_" << *it;
    }

    // Create directory
    create_test_directory(path.str());

    // Add some files to this directory
    size_t files_in_dir = 0;
    for (size_t j = 0; j < meta.chunks.size(); ++j) {
      if (meta.chunks[j].block() == i && files_in_dir < 10) {
        std::ostringstream file_path;
        file_path << path.str() << "/file_" << j << ".dat";

        // Generate content with appropriate size
        metadata_domain::chunk const& chunk = meta.chunks[j];
        std::string content = generate_random_content(chunk.size());

        create_test_file(file_path.str(), content);
        files_in_dir++;
      }
    }
  }

  // Create some symlinks if requested
  if (meta.names.size() > 2) {
    std::string link_path = temp_dir + "/symlink_test";
    std::string target = "file_0.txt";
    create_symlink(target, link_path);
  }

  return temp_dir;
}

std::string FixtureGenerator::invoke_mkdwarfs(
    const std::string& temp_dir,
    const std::string& output_path,
    const HomebrewInfo& info) {

  std::string mkdwarfs = info.mkdwarfs_path;
  if (mkdwarfs.empty()) {
    throw std::runtime_error("mkdwarfs not found");
  }

  // Build command
  std::ostringstream cmd;
  cmd << mkdwarfs
      << " -i " << temp_dir
      << " -o " << output_path
      << " -l 5"        // Compression level
      << " --force";    // Overwrite existing files

  // Execute command
  int status = system(cmd.str().c_str());
  if (WIFEXITED(status)) {
    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0) {
      throw std::runtime_error("mkdwarfs failed with exit code: " +
                               std::to_string(exit_code));
    }
  } else {
    throw std::runtime_error("mkdwarfs terminated abnormally");
  }

  return output_path;
}

void FixtureGenerator::cleanup_temp_directory(const std::string& temp_dir) {
  std::error_code ec;
  fs::remove_all(temp_dir, ec);
  // Ignore errors during cleanup
}

std::string FixtureGenerator::generate(const FixtureSpec& spec) {
  // Get Homebrew info
  HomebrewInfo info;
  if (homebrew_detector_) {
    info = homebrew_detector_->detect();
  }

  if (!info.is_complete()) {
    throw std::runtime_error("Homebrew dwarfs not available or incomplete");
  }

  // Create temporary directory with test files
  std::string temp_dir = create_temp_directory(spec.test_metadata);

  // Generate output path
  std::string output_path = spec.get_fixture_path();

  // Ensure output directory exists
  fs::create_directories(spec.output_dir);

  try {
    // Invoke mkdwarfs
    std::string result = invoke_mkdwarfs(temp_dir, output_path, info);

    // Verify output file exists
    struct stat buffer;
    if (stat(result.c_str(), &buffer) != 0) {
      throw std::runtime_error("Fixture file not created: " + result);
    }

    return result;
  } catch (...) {
    // Cleanup on failure
    cleanup_temp_directory(temp_dir);
    throw;
  }

  // Cleanup temp directory
  cleanup_temp_directory(temp_dir);
}

bool FixtureGenerator::fixture_exists(const FixtureSpec& spec) const {
  std::string path = get_fixture_path(spec);
  struct stat buffer;
  return (stat(path.c_str(), &buffer) == 0) && S_ISREG(buffer.st_mode);
}

std::string FixtureGenerator::get_fixture_path(const FixtureSpec& spec) const {
  return default_output_dir_ + "/" + spec.get_filename();
}

} // namespace dwarfs::test::compat
