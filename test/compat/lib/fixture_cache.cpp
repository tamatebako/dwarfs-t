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

#include "fixture_cache.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <sys/stat.h>

namespace dwarfs::test::compat {

namespace {

namespace fs = std::filesystem;

// Calculate SHA256 checksum
std::string sha256_string(const std::string& str) {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, str.c_str(), str.length());
  SHA256_Final(hash, &ctx);

  std::ostringstream ss;
  for (unsigned char c : hash) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  }
  return ss.str();
}

// Read file into string
std::string read_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + path);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

// Write string to file
void write_file(const std::string& path, const std::string& content) {
  std::ofstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to write file: " + path);
  }
  file << content;
}

} // anonymous namespace

FixtureCache::FixtureCache(const std::string& cache_dir)
    : cache_dir_(cache_dir) {

  // Ensure cache directory exists
  fs::create_directories(cache_dir_);
}

bool FixtureCache::has_valid_fixture(const FixtureSpec& spec) const {
  std::string path = cache_dir_ + "/" + spec.get_filename();

  struct stat buffer;
  if (stat(path.c_str(), &buffer) != 0 || !S_ISREG(buffer.st_mode)) {
    return false;
  }

  // Check if checksum is valid (if we have one stored)
  auto it = checksums_.find(spec.get_filename());
  if (it != checksums_.end()) {
    // Verify checksum
    std::string current_hash = calculate_sha256_from_file(path);
    return current_hash == it->second.hash;
  }

  return true;
}

std::vector<uint8_t> FixtureCache::load(const FixtureSpec& spec) {
  std::string path = cache_dir_ + "/" + spec.get_filename();

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open fixture: " + path);
  }

  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> data(size);
  file.read(reinterpret_cast<char*>(data.data()), size);

  if (!file) {
    throw std::runtime_error("Failed to read fixture: " + path);
  }

  return data;
}

void FixtureCache::store(const FixtureSpec& spec, std::span<uint8_t const> data) {
  std::string path = cache_dir_ + "/" + spec.get_filename();

  // Write fixture data
  {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
      throw std::runtime_error("Failed to write fixture: " + path);
    }
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.flush();  // Ensure data is written before calculating checksum
  }

  // Calculate and store checksum from the written file
  FixtureChecksum checksum;
  checksum.algorithm = "sha256";
  checksum.hash = calculate_sha256_from_file(path);  // Calculate from file, not data
  checksum.size = data.size();
  checksum.platform_arch = spec.platform_arch();

  checksums_[spec.get_filename()] = checksum;
}

bool FixtureCache::validate_checksum(const FixtureSpec& spec) const {
  std::string path = cache_dir_ + "/" + spec.get_filename();

  struct stat buffer;
  if (stat(path.c_str(), &buffer) != 0) {
    return false;
  }

  std::string expected_hash;
  std::string checksum_source = "in-memory";

  // First, check if there's a checksum file on disk
  std::string checksum_path = get_checksum_path(spec);
  std::ifstream checksum_file(checksum_path);

  if (checksum_file.good()) {
    // Read checksum from disk file
    // Format: filename hash size platform_arch (platform_arch is optional)
    std::string filename, hash, size_str, platform_arch;
    if (checksum_file >> filename >> hash >> size_str) {
      expected_hash = hash;
      checksum_source = "disk file: " + checksum_path;
      // Try to read platform_arch (optional)
      checksum_file >> platform_arch;
    }
  }

  // If no disk file, use in-memory checksum
  if (expected_hash.empty()) {
    auto it = checksums_.find(spec.get_filename());
    if (it == checksums_.end()) {
      // No checksum stored, can't validate
      return true;
    }
    expected_hash = it->second.hash;
  }

  // Calculate current checksum
  std::string current_hash = calculate_sha256_from_file(path);

  return current_hash == expected_hash;
}

FixtureChecksum FixtureCache::get_checksum(const FixtureSpec& spec) const {
  std::string filename = spec.get_filename();

  // If we have it in memory, return it
  auto it = checksums_.find(filename);
  if (it != checksums_.end()) {
    return it->second;
  }

  // Calculate from file
  std::string path = cache_dir_ + "/" + filename;

  struct stat buffer;
  if (stat(path.c_str(), &buffer) != 0) {
    throw std::runtime_error("Fixture not found: " + path);
  }

  FixtureChecksum checksum;
  checksum.algorithm = "sha256";
  checksum.hash = calculate_sha256_from_file(path);
  checksum.size = buffer.st_size;
  checksum.platform_arch = spec.platform_arch();

  return checksum;
}

void FixtureCache::invalidate(const FixtureSpec& spec) {
  std::string path = cache_dir_ + "/" + spec.get_filename();

  std::error_code ec;
  fs::remove(path, ec);

  // Remove from checksum map
  checksums_.erase(spec.get_filename());
}

void FixtureCache::clear() {
  std::error_code ec;

  // Remove all fixture files
  for (auto const& entry : fs::directory_iterator(cache_dir_)) {
    if (entry.is_regular_file() && entry.path().extension() == ".dft") {
      fs::remove(entry.path(), ec);
    }
  }

  // Clear checksums
  checksums_.clear();
}

size_t FixtureCache::get_cache_size() const {
  size_t total = 0;
  std::error_code ec;

  for (auto const& entry : fs::directory_iterator(cache_dir_)) {
    if (entry.is_regular_file()) {
      total += entry.file_size();
    }
  }

  return total;
}

size_t FixtureCache::get_fixture_count() const {
  size_t count = 0;
  std::error_code ec;

  for (auto const& entry : fs::directory_iterator(cache_dir_)) {
    if (entry.is_regular_file() && entry.path().extension() == ".dft") {
      count++;
    }
  }

  return count;
}

std::vector<FixtureSpec> FixtureCache::list_fixtures() const {
  std::vector<FixtureSpec> specs;
  std::error_code ec;

  for (auto const& entry : fs::directory_iterator(cache_dir_)) {
    if (entry.is_regular_file() && entry.path().extension() == ".dft") {
      std::string filename = entry.path().filename().string();

      // Parse filename: dwarfs-v{version}-{platform}-{arch}.dft
      if (filename.substr(0, 8) == "dwarfs-v" && filename.substr(filename.size() - 4) == ".dft") {
        std::string base = filename.substr(8, filename.size() - 12);  // "dwarfs-v" is 8 chars, ".dft" is 4 chars
        size_t dash1 = base.find('-');
        size_t dash2 = base.rfind('-');

        if (dash1 != std::string::npos && dash2 != std::string::npos && dash1 < dash2) {
          FixtureSpec spec;
          spec.dwarfs_version = base.substr(0, dash1);
          spec.platform = base.substr(dash1 + 1, dash2 - dash1 - 1);
          spec.arch = base.substr(dash2 + 1);
          spec.output_dir = cache_dir_;

          specs.push_back(spec);
        }
      }
    }
  }

  return specs;
}

bool FixtureCache::load_checksums(const std::string& checksums_path) {
  std::ifstream file(checksums_path);
  if (!file) {
    return false;
  }

  checksums_.clear();

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Format: filename sha256 size platform_arch
    std::istringstream iss(line);
    std::string filename, hash, size_str, platform_arch;

    if (iss >> filename >> hash >> size_str >> platform_arch) {
      FixtureChecksum checksum;
      checksum.algorithm = "sha256";
      checksum.hash = hash;
      checksum.size = std::stoull(size_str);
      checksum.platform_arch = platform_arch;

      checksums_[filename] = checksum;
    }
  }

  return true;
}

bool FixtureCache::save_checksums(const std::string& checksums_path) {
  std::ofstream file(checksums_path);
  if (!file) {
    return false;
  }

  file << "# Homebrew Compatibility Test Fixture Checksums\n";
  file << "# Format: filename sha256 size platform_arch\n";
  file << "#\n";

  for (auto const& [filename, checksum] : checksums_) {
    file << filename << " "
          << checksum.hash << " "
          << checksum.size << " "
          << checksum.platform_arch << "\n";
  }

  return true;
}

std::string FixtureCache::calculate_sha256(std::span<uint8_t const> data) const {
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, data.data(), data.size());
  SHA256_Final(hash, &ctx);

  std::ostringstream ss;
  for (unsigned char c : hash) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  }
  return ss.str();
}

std::string FixtureCache::calculate_sha256_from_file(const std::string& path) const {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open file: " + path);
  }

  SHA256_CTX ctx;
  SHA256_Init(&ctx);

  char buffer[8192];
  while (file.read(buffer, sizeof(buffer))) {
    SHA256_Update(&ctx, buffer, file.gcount());
  }

  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_Final(hash, &ctx);

  std::ostringstream ss;
  for (unsigned char c : hash) {
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
  }
  return ss.str();
}

std::string FixtureCache::get_checksum_path(const FixtureSpec& spec) const {
  return cache_dir_ + "/" + spec.get_filename() + ".sha256";
}

} // namespace dwarfs::test::compat
